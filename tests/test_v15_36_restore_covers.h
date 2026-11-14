/* V15.36 regression: recover black PS1, PS2, PS3 from already-saved
 * JPG/PNG despite stale .none; selective SNES rescan and 5-tab GPU budget.
 * Uses the existing deterministic in-memory PS3 filesystem and loader. */
#ifndef SF_TEST_V15_36_RESTORE_COVERS_H
#define SF_TEST_V15_36_RESTORE_COVERS_H

static void test_v15_36_restore_existing_covers(void)
{
    static CoverLoader L;
    CoverIO io;
    CoverCache cc;
    const char *const paths[3]={
        "/dev_hdd0/PSXISO/Crash.cue",
        "/dev_hdd0/PS2ISO/Okami.iso",
        "/dev_hdd0/PS3ISO/Tekken.iso"
    };
    const char *const pngs[3]={
        COVER_DL_DIR "/Crash.png",
        COVER_DL_DIR "/Okami.png",
        COVER_DL_DIR "/Tekken.png"
    };
    char stems[4][COVER_PATH_MAX], f[COVER_PATH_MAX+8];
    unsigned char negative[3]={0,0,0};
    const char *const good="IMG 80 112 220 65 25 255";
    int p;
    printf("V15.36: PS1, PS2 and PS3 JPG/PNG outrank stale .none\n");
    memfs_reset();
    memfs_dir(COVER_DL_DIR);
    mem_io(&io,"/cache");
    cover_loader_init(&L,&io);
    cc_init(&cc,8,NULL,NULL);
    for(p=0;p<3;p++){
        CHECK(memfs_put(pngs[p],good,(uint32_t)strlen(good))!=NULL);
        cover_cache_stem("/cache",p,paths[p],1234,stems[p],sizeof(stems[p]));
        snprintf(f,sizeof(f),"%s.none",stems[p]);
        negative[0]=(unsigned char)(p==PLAT_PS1?7:6);
        CHECK(memfs_put(f,negative,3)!=NULL);
        cc.slot[p].state=CC_LOADING;
        cover_request(&L,p,p,p,p,paths[p],0,1234,"");
    }
    /* The absence of a source image should still respect a real .none. */
    cover_cache_stem("/cache",PLAT_PS3,"/dev_hdd0/PS3ISO/Absent.iso",
                     1234,stems[3],sizeof(stems[3]));
    snprintf(f,sizeof(f),"%s.none",stems[3]);
    negative[0]=6;
    CHECK(memfs_put(f,negative,3)!=NULL);
    cc.slot[3].state=CC_LOADING;
    cover_request(&L,3,PLAT_PS3,PLAT_PS3,3,
                  "/dev_hdd0/PS3ISO/Absent.iso",0,1234,"");
    for(p=0;p<4;p++)CHECK(cover_worker_step(&L)==1);
    cover_poll(&L,&cc);
    for(p=0;p<3;p++){
        CHECK(cc.slot[p].state==CC_READY);
        snprintf(f,sizeof(f),"%s.none",stems[p]);
        CHECK(memfs_find(f)==NULL);
        snprintf(f,sizeof(f),"%s.cvr",stems[p]);
        CHECK(memfs_find(f)!=NULL);     /* disk cache recreated offline */
        CHECK(memfs_find(pngs[p])!=NULL); /* original downloads untouched */
    }
    CHECK(cc.slot[3].state==CC_FAILED);
    snprintf(f,sizeof(f),"%s.none",stems[3]);
    CHECK(memfs_find(f)!=NULL);
    cover_loader_stop(&L);
    memfs_reset();
}

static void test_v15_36_scoped_rescan_preserves_psx_ps2_ps3(void)
{
    static CoverLoader L;
    CoverIO io;
    char stems[PLAT_COUNT][COVER_PATH_MAX];
    char cachefile[COVER_PATH_MAX+8];
    const char *keep=COVER_DL_DIR "/Original_PS1.jpg";
    int p;
    printf("V15.36: rescanning SNES cannot erase PS1 PS2 PS3 caches\n");
    memfs_reset();
    mem_io(&io,"/cache");
    cover_loader_init(&L,&io);
    CHECK(memfs_put(keep,"IMG 80 112 2 4 8 255",19)!=NULL);
    for(p=0;p<PLAT_COUNT;p++){
        char path[96];
        snprintf(path,sizeof(path),"/dev_hdd0/TEST/%d/Game.iso",p);
        cover_cache_stem("/cache",p,path,100,stems[p],sizeof(stems[p]));
        snprintf(cachefile,sizeof(cachefile),"%s.cvr",stems[p]);
        CHECK(memfs_put(cachefile,"TEST",4)!=NULL);
        snprintf(cachefile,sizeof(cachefile),"%s.none",stems[p]);
        CHECK(memfs_put(cachefile,"NEG",3)!=NULL);
    }
    cover_wipe_cache_platform(&L,PLAT_SNES);
    CHECK(cover_worker_step(&L)==1);
    for(p=0;p<PLAT_COUNT;p++){
        snprintf(cachefile,sizeof(cachefile),"%s.cvr",stems[p]);
        CHECK((memfs_find(cachefile)!=NULL)==(p!=PLAT_SNES));
        snprintf(cachefile,sizeof(cachefile),"%s.none",stems[p]);
        CHECK((memfs_find(cachefile)!=NULL)==(p!=PLAT_SNES));
    }
    CHECK(memfs_find(keep)!=NULL);
    cover_wipe_cache_platform(&L,PLAT_PS2);
    CHECK(cover_worker_step(&L)==1);
    for(p=0;p<PLAT_COUNT;p++){
        snprintf(cachefile,sizeof(cachefile),"%s.cvr",stems[p]);
        CHECK((memfs_find(cachefile)!=NULL)==(p!=PLAT_SNES && p!=PLAT_PS2));
    }
    CHECK(memfs_find(keep)!=NULL);
    cover_loader_stop(&L);
    memfs_reset();
}

static void test_v15_36_five_tab_gpu_budget(void)
{
    CoverCache cc;
    int main_tab=PLAT_PS3,t,i,rounds,total;
    printf("V15.36: five console flow windows fit 64 GPU cover slots\n");
    cc_init(&cc,CC_DEFAULT_BUDGET,NULL,NULL);
    for(t=0;t<CC_TABS;t++)cc_set_side(&cc,t,120,500);
    for(rounds=0;rounds<140;rounds++)cc_update(&cc,main_tab,120,500);
    total=0;
    for(t=0;t<CC_TABS;t++)if(cc.lo[t]<=cc.hi[t])
        total+=cc.hi[t]-cc.lo[t]+1;
    CHECK(total<=CC_DEFAULT_BUDGET);
    CHECK(total==CC_DEFAULT_BUDGET);
    CHECK(cc.hi[main_tab]-cc.lo[main_tab]+1==CC_MAIN_WINDOW);
    CHECK(cc_resident_count(&cc)==CC_DEFAULT_BUDGET);
    for(t=0;t<CC_TABS;t++){
        CHECK(cc_lookup(&cc,t,120)>=0);
        for(i=cc.lo[t];i<=cc.hi[t];i++)
            CHECK(cc_lookup(&cc,t,i)>=0);
    }
    main_tab=PLAT_PS2;
    for(rounds=0;rounds<140;rounds++)cc_update(&cc,main_tab,120,500);
    total=0;
    for(t=0;t<CC_TABS;t++)if(cc.lo[t]<=cc.hi[t])
        total+=cc.hi[t]-cc.lo[t]+1;
    CHECK(total<=CC_DEFAULT_BUDGET);
    CHECK(cc.hi[main_tab]-cc.lo[main_tab]+1==CC_MAIN_WINDOW);
    for(t=0;t<CC_TABS;t++)CHECK(cc_lookup(&cc,t,120)>=0);
}

#endif
