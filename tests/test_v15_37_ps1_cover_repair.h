/* V15.37 PS1 cover repair: a missing PS1 box is recovered automatically,
 * while already-downloaded PS1 boxart is never fetched or overwritten.
 * This reuses the real downloader and its existing region/serial sources.
 */
#ifndef SF_TEST_V15_37_PS1_COVER_REPAIR_H
#define SF_TEST_V15_37_PS1_COVER_REPAIR_H
static void test_v15_37_ps1_missing_cover_auto_repair(void)
{
    CoverIO io;
    CoverDl d;
    CoverDlItem item[2];
    static const char already[]="IMG 80 112 220 65 25 255";
    MemFile *existing;
    const char *original=COVER_DL_DIR "/Resident Evil 2 (USA).jpg";
    printf("V15.37: background PS1 retry fills missing art, preserves existing art\n");
    memfs_reset();
    memfs_dir(COVER_DL_DIR);
    mem_io(&io,"/cache");
    CHECK(memfs_put(original,already,(uint32_t)strlen(already))!=NULL);
    /* Genuine failed lookup marker from a previous installed version. */
    CHECK(memfs_put(COVER_DL_DIR "/SLUS00594.missing","404",3)!=NULL);
    memset(item,0,sizeof(item));
    item[0].plat=PLAT_PS1;
    sf_strlcpy(item[0].path,
        "/dev_hdd0/PSXISO/Metal Gear Solid [SLUS-00594].chd",
        sizeof(item[0].path));
    item[1].plat=PLAT_PS1;
    sf_strlcpy(item[1].path,
        "/dev_hdd0/PSXISO/Resident Evil 2 (USA).cue",
        sizeof(item[1].path));
    memset(&d,0,sizeof(d));
    d.item=item;d.n=2;d.io=&io;
    d.http=test_ps1_http;
    d.auto_mode=1; /* background one-shot, not a manual button press */
    g_ps1_http_calls=0;g_ps1_http_bad_url=0;
    cover_dl_run(&d);
    CHECK(d.finished && d.n_got==1 && d.n_have==1 && d.n_error==0);
    CHECK(g_ps1_http_calls==1 && !g_ps1_http_bad_url);
    CHECK(memfs_find(COVER_DL_DIR "/SLUS00594.jpg")!=NULL);
    CHECK(memfs_find(COVER_DL_DIR "/SLUS00594.missing")==NULL);
    existing=memfs_find(original);
    CHECK(existing && existing->len==strlen(already));
    CHECK(memcmp(existing->data,already,strlen(already))==0);
    /* A second boot/scan must not re-request either existing cover. */
    memset(&d,0,sizeof(d));
    d.item=item;d.n=2;d.io=&io;
    d.http=test_ps1_http;
    d.auto_mode=1;
    cover_dl_run(&d);
    CHECK(d.finished && d.n_have==2 && d.n_got==0 && d.n_error==0);
    CHECK(g_ps1_http_calls==1 && !g_ps1_http_bad_url);
    existing=memfs_find(original);
    CHECK(existing && existing->len==strlen(already));
    CHECK(memcmp(existing->data,already,strlen(already))==0);
    memfs_reset();
}
#endif
