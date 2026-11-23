#!/usr/bin/env python3
"""V15.79: harden the existing V15.78 delete-only integration in a repeatable way."""
from pathlib import Path

def source(p):
    return Path(p).read_text()

def save(p,s):
    Path(p).write_text(s)

def replace_once(s,a,b,name):
    n=s.count(a)
    assert n==1,(name,"expected one anchor",n,a[:120])
    return s.replace(a,b)

# Multi-disc: a deliberate per-disc choice, not an unsafe whole-group operation.
p="src/core/app.h";s=source(p)
s=replace_once(s,"    int delete_confirm;       /* second deliberate X, Circle cancels; modal input lock */",
"""    int delete_confirm;       /* second deliberate X, Circle cancels; modal input lock */
    int delete_select_disc;   /* explicit disc choice, never bulk delete */
    int delete_disc_index;    /* selected physical disc of a grouped game */""",p)
save(p,s)
p="src/core/app.c";s=source(p)
s=replace_once(s,'#include "discid.h"','#include "discid.h"\n#include "sf_delete_policy.h"',p)
s=replace_once(s,"    a->panel = panel;\n    a->panel_open = 1;",
"""    a->panel = panel;
    a->panel_open = 1;
    a->delete_select_disc = 0;
    a->delete_disc_index = -1;""",p)
s=s.replace("    a->delete_confirm = 0;","    a->delete_confirm = 0;\n    a->delete_select_disc = 0;\n    a->delete_disc_index = -1;")
assert s.count("a->delete_select_disc = 0;")==3, "reset in startup, open panel and close panel"
s=replace_once(s,
'''        } else if(g->ndisc>1) {
            app_show_toast(a,"Multidisco: usa un gestor de archivos");
        } else {
            a->delete_confirm=1;''',
'''        } else if(g->ndisc>1) {
            /* Select exactly one disc; never silently erase an entire set. */
            app_open_panel(a,PANEL_DISC);
            a->delete_select_disc=1;
            a->delete_disc_index=-1;
            app_show_toast(a,"Selecciona el disco que deseas eliminar");
        } else {
            a->delete_confirm=1;''',p)
start=s.index("    if(a->delete_confirm){",s.index("static void panel_input"))
end=s.index("    if (a->about)",start)
s=s[:start]+"""    if(a->delete_confirm){
        if(in->pressed&SFB_CIRCLE){
            a->delete_confirm=0;
            sound_play(SND_CLOSE);
        }else if(in->pressed&SFB_CROSS){
            const Game *g=NULL;
            const Disc *d=NULL;
            int one_disc=a->panel==PANEL_DISC && a->delete_select_disc;
            if((a->panel==PANEL_GAME || one_disc) &&
               a->panel_plat>=0 && a->panel_plat<PLAT_COUNT &&
               a->panel_game>=0 && a->panel_game<a->lib->tab[a->panel_plat].count)
                g=&a->lib->tab[a->panel_plat].games[a->panel_game];
            a->delete_confirm=0;
            if(g && !(g->flags&GAME_DEMO) && a->host.delete_game &&
               ((!one_disc && g->ndisc<=1) ||
                (one_disc && g->ndisc>1 &&
                 (d=lib_game_disc(a->lib,g,a->delete_disc_index))!=NULL))){
                char path[SCAN_PATH_MAX];
                int rc;
                sf_strlcpy(path,one_disc ? lib_str(a->lib,d->path) :
                           lib_path(a->lib,g),sizeof(path));
                rc=a->host.delete_game(a->panel_plat,path,
                                       (g->flags&GAME_DIR)!=0);
                if(rc==0){
                    log_printf("delete: success platform=%d path=%s",
                               a->panel_plat,path);
                    sound_play(SND_CONFIRM);
                    a->usb_auto_rescan=1;
                    app_close_panel(a);
                    start_loading(a,1);
                }else if(rc==SF_DEL_PARTIAL){
                    /* A subset might already have been erased. Never leave
                     * a stale cover which appears playable after failure. */
                    log_printf("delete: PARTIAL failure; force rescan path=%s",path);
                    sound_play(SND_CLOSE);
                    app_close_panel(a);
                    a->usb_auto_rescan=1;
                    start_loading(a,1);
                    app_show_toast(a,"Borrado parcial: revisa el USB y los archivos");
                }else{
                    log_printf("delete: rejected or failed rc=%d",rc);
                    sound_play(SND_CLOSE);
                    app_show_toast(a,rc==-2?
                        "No se puede borrar: elemento protegido":
                        "No se pudo eliminar: revisa el archivo o el USB");
                }
            }
        }
        return;
    }
"""+s[end:]
s=replace_once(s,
'''    if (a->panel == PANEL_DISC) {
        if (in->pressed & SFB_CROSS) { launch_disc(a, a->row); app_close_panel(a); }
    } else {''',
'''    if (a->panel == PANEL_DISC) {
        if (in->pressed & SFB_CROSS) {
            if(a->delete_select_disc){
                a->delete_disc_index=a->row;
                a->delete_confirm=1;
                sound_play(SND_OPEN);
            }else {
                launch_disc(a,a->row);
                app_close_panel(a);
            }
        }
    } else {''',p)
save(p,s)
p="src/core/ui.c";s=source(p)
s=replace_once(s,
'''    if(!a->delete_confirm || a->panel!=PANEL_GAME ||
       a->panel_plat<0 || a->panel_plat>=PLAT_COUNT ||''',
'''    if(!a->delete_confirm ||
       (a->panel!=PANEL_GAME &&
        !(a->panel==PANEL_DISC && a->delete_select_disc)) ||
       a->panel_plat<0 || a->panel_plat>=PLAT_COUNT ||''',p)
s=replace_once(s,
'''    g=&a->lib->tab[a->panel_plat].games[a->panel_game];
    w=W()*0.62f;h=H()*0.32f;''',
'''    g=&a->lib->tab[a->panel_plat].games[a->panel_game];
    w=W()*0.62f;h=H()*0.32f;''',p)
s=replace_once(s,
'''               TH_TEXT,"¿Eliminar?",ALIGN_CENTER,w*0.88f);''',
'''               TH_TEXT,a->delete_select_disc?"¿Eliminar este disco?":"¿Eliminar?",
               ALIGN_CENTER,w*0.88f);''',p)
s=replace_once(s,
'''               TH_TEXT_DIM,"Esta accion es irreversible",ALIGN_CENTER,w*0.88f);''',
'''               TH_TEXT_DIM,a->delete_select_disc?
               "Solo el disco seleccionado; accion irreversible":
               "Esta accion es irreversible",ALIGN_CENTER,w*0.88f);''',p)
# When deleting a group member show which disc the user selected.
anchor='''    d_text_fit(&g_font_main,x+w*0.5f,y+h*0.42f,0.030f*H(),
               TH_TEXT,lib_title(a->lib,g),ALIGN_CENTER,w*0.88f);'''
assert s.count(anchor)==1
s=s.replace(anchor,'''    {
        char disc_name[64],label[256];
        const char *shown=lib_title(a->lib,g);
        if(a->delete_select_disc){
            const Disc *d=lib_game_disc(a->lib,g,a->delete_disc_index);
            if(d){
                lib_disc_name(a->lib,d,a->delete_disc_index,disc_name,sizeof(disc_name));
                snprintf(label,sizeof(label),"%s - %s",shown,disc_name);
                shown=label;
            }
        }
        d_text_fit(&g_font_main,x+w*0.5f,y+h*0.42f,0.030f*H(),
                   TH_TEXT,shown,ALIGN_CENTER,w*0.88f);
    }''')
save(p,s)

p="src/core/sf_delete_policy.h";s=source(p)
s=replace_once(s,"    SF_DEL_UNSUPPORTED=-2","    SF_DEL_UNSUPPORTED=-2,\n    SF_DEL_PARTIAL=-3",p);save(p,s)

p="src/ps3/sf_delete_game.c";s=source(p)
s=replace_once(s,'#include "sf_delete_policy.h"','#include "sf_delete_policy.h"\n#include "sf_delete_arcade_guard.h"',p)
s=replace_once(s,'''           sysFsUnlink(track)!=0)return -1;''',
'''           sysFsUnlink(track)!=0)return i>0?SF_DEL_PARTIAL:-1;''',p)
s=replace_once(s,'''    return sysFsUnlink(cue)==0?0:-1;''',
'''    return sysFsUnlink(cue)==0?0:SF_DEL_PARTIAL;''',p)
s=replace_once(s,'''           sysFsUnlink(part)!=0)return -1;''',
'''           sysFsUnlink(part)!=0)return i>0?SF_DEL_PARTIAL:-1;''',p)
s=replace_once(s,"static int delete_tree(const char *dir,int depth,unsigned *count)",
"static int delete_tree(const char *dir,int depth,unsigned *count,unsigned *removed)",p)
s=replace_once(s,"            rc=delete_tree(child,depth+1,count);",
"            rc=delete_tree(child,depth+1,count,removed);",p)
s=replace_once(s,"        if(rc!=0)break;\n    }\n    sysFsClosedir(fd);",
"        if(rc!=0)break;\n        (*removed)++;\n    }\n    sysFsClosedir(fd);",p)
s=replace_once(s,"    unsigned n=0;\n    int rc=sf_delete_path_allowed(platform,path,is_dir);",
"    unsigned n=0,removed=0;\n    int rc=sf_delete_path_allowed(platform,path,is_dir);",p)
s=replace_once(s,"    if(sysFsStat(path,&st)!=0)return -1;",
'''    if(platform==PLAT_ARCADE && !sf_delete_arcade_allowed(path))return -2;
    if(sysFsStat(path,&st)!=0)return -1;''',p)
s=replace_once(s,"        rc=delete_tree(path,0,&n);",
"        rc=delete_tree(path,0,&n,&removed);\n        if(rc!=0 && removed)rc=SF_DEL_PARTIAL;",p)
save(p,s)
# Do not delete chunk zero of a broken .iso.0/.iso.2 layout.
p="src/ps3/sf_delete_game.c";s=source(p)
start=s.index("static int delete_split_iso(")
end=s.index("/* Only selected PS3 game folders",start)
new_impl=r'''static int delete_split_iso(const char *path)
{
    char root[SF_DEL_PATH],part[SF_DEL_PATH],dir[SF_DEL_PATH];
    const char *slash=strrchr(path,'/');
    size_t n=strlen(path),stem;
    int i,parts=0,fd,rc=0;
    u64 got=0;
    sysFSDirent ent;
    if(!slash || slash==path || n<6 || n>=sizeof(root) ||
       !ends_ci(path,".iso.0"))return -2;
    if((size_t)(slash-path)>=sizeof(dir))return -2;
    memcpy(dir,path,(size_t)(slash-path));dir[slash-path]=0;
    memcpy(root,path,n-1);root[n-1]=0; /* '.iso.0' -> '.iso.' */
    stem=strlen(slash+1)-1;
    for(i=0;i<64;i++){
        sysFSStat st;
        if(snprintf(part,sizeof(part),"%s%d",root,i)>=(int)sizeof(part))
            return -2;
        if(sysFsStat(part,&st)!=0)break;
        if((st.st_mode&SF_DEL_IFMT)==SF_DEL_IFDIR)return -2;
        parts++;
    }
    /* Require at least .0 and .1; a lone head may be a damaged set. */
    if(parts<2 || parts==64)return -2;
    if(sysFsOpendir(dir,&fd)!=0)return -2;
    while((rc=sysFsReaddir(fd,&ent,&got))==0 && got){
        const char *other=ent.d_name,*digits;
        int number=0;
        if(strncmp(other,slash+1,stem)!=0)continue;
        digits=other+stem;
        if(!*digits)continue;
        while(*digits>='0' && *digits<='9'){
            if(number>100000)break;
            number=number*10+(*digits++-'0');
        }
        if(!*digits && number>=parts){rc=-2;break;}
    }
    if(sysFsClosedir(fd)!=0 || rc!=0)return -2;
    for(i=0;i<parts;i++){
        if(snprintf(part,sizeof(part),"%s%d",root,i)>=(int)sizeof(part) ||
           sysFsUnlink(part)!=0)
            return i>0?SF_DEL_PARTIAL:-1;
    }
    return 0;
}
'''
s=s[:start]+new_impl+s[end:]
save(p,s)

# Real NTFS runtime uses the separately host-tested safety module.
p="src/ps3/main.c";s=source(p)
s=replace_once(s,'#include "sf_delete_policy.h"',
'#include "sf_delete_policy.h"\n#include "sf_delete_ntfs.h"',p)
start=s.index("          /* Only a BDISO movie") if "          /* Only a BDISO movie" in s else s.index("/* Only a BDISO movie")
end=s.index("int main(int argc, const char *argv[])",start)
new_wrapper=r'''/* Only a real user movie qualifies for NTFS; the pure guarded
 * transaction below is also executed in host tests with fake media. */
static int sf_delete_game_with_ntfs(int platform,const char *path,int is_dir)
{
    sysFSStat st;
    int rc;
    if(platform!=PLAT_BDISO || is_dir || !path || sysFsStat(path,&st)==0)
        return sf_delete_game(platform,path,is_dir);
    rc=sf_delete_bdiso_ntfs_safe(path,sf_bdntfs_is_file,
                                 sf_bdntfs_path,ps3ntfs_unlink);
    log_printf("delete: native BDISO NTFS rc=%d path=%s",rc,path);
    return rc;
}

'''
s=s[:start]+new_wrapper+s[end:]
save(p,s)

# Adapt the existing 18 scenario host tests to parent-protecting semantics.
p="tests/test_mi_ps3_delete_backend.c";s=source(p)
s=replace_once(s,'#include "sf_delete_game.h"',
'#include "sf_delete_game.h"\n#include "sf_delete_policy.h"',p)
s=replace_once(s,'static char root[PATH_MAX];',
'''static char root[PATH_MAX];
static char arcade_map[PATH_MAX];
static int fail_unlink_after=-1;
const char *sf_delete_arcade_test_map(void) {return arcade_map;}''',p)
s=replace_once(s,
'''s32 sysFsUnlink(const char *p)
{
    char dst[PATH_MAX];real_path(p,dst,sizeof(dst));
    return unlink(dst)==0?0:-1;
}''',
'''s32 sysFsUnlink(const char *p)
{
    char dst[PATH_MAX];real_path(p,dst,sizeof(dst));
    if(fail_unlink_after==0)return -1;
    if(fail_unlink_after>0)fail_unlink_after--;
    return unlink(dst)==0?0:-1;
}''',p)
s=replace_once(s,'''    char *p=mkdtemp(templ);assert(p);strcpy(root,p);''',
'''    char *p=mkdtemp(templ);assert(p);strcpy(root,p);
    assert(snprintf(arcade_map,sizeof(arcade_map),
        "%s/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/arcade_parents.tsv",
        root)<(int)sizeof(arcade_map));
    make_file("/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/arcade_parents.tsv",
              "kof2002b\\tkof2002\\n");
    {
        FILE *map=fopen(arcade_map,"ab");
        int i;assert(map);
        for(i=0;i<20;i++)fprintf(map,"fake_child%d\\tfake_parent%d\\n",i,i);
        fclose(map);
    }''',p)
s=replace_once(s,
'''    DEL(PLAT_ARCADE,"/dev_hdd0/ARCADE/kof2002.zip",0);
    assert(!exists("/dev_hdd0/ARCADE/kof2002.zip"));
    assert(exists("/dev_hdd0/ARCADE/kof2002b.zip"));''',
'''    FAIL(PLAT_ARCADE,"/dev_hdd0/ARCADE/kof2002.zip",0);
    assert(exists("/dev_hdd0/ARCADE/kof2002.zip"));
    DEL(PLAT_ARCADE,"/dev_hdd0/ARCADE/kof2002b.zip",0);
    assert(!exists("/dev_hdd0/ARCADE/kof2002b.zip"));
    DEL(PLAT_ARCADE,"/dev_hdd0/ARCADE/kof2002.zip",0);
    assert(!exists("/dev_hdd0/ARCADE/kof2002.zip"));''',p)
s=replace_once(s,
'''    printf("PASS 18 MI PS3 end-to-end deletion and safety scenarios\\n");''',
'''    /* 19. BIOS is shared and cannot be removed from the ROM browser. */
    make_file("/dev_hdd0/ARCADE/neogeo.zip","BIOS");
    FAIL(PLAT_ARCADE,"/dev_hdd0/ARCADE/neogeo.zip",0);
    assert(exists("/dev_hdd0/ARCADE/neogeo.zip"));
    /* 20. Detect file removal interrupted after one CUE track. */
    make_file("/dev_hdd0/PSXISO/PartialTests/Partial 1.bin","TRACK1");
    make_file("/dev_hdd0/PSXISO/PartialTests/Partial 2.bin","TRACK2");
    make_file("/dev_hdd0/PSXISO/PartialTests/Partial.cue",
              "FILE \\"Partial 1.bin\\" BINARY\\nFILE \\"Partial 2.bin\\" BINARY\\n");
    fail_unlink_after=1;
    assert(sf_delete_game(PLAT_PS1,"/dev_hdd0/PSXISO/PartialTests/Partial.cue",0)==SF_DEL_PARTIAL);
    fail_unlink_after=-1;
    assert(exists("/dev_hdd0/PSXISO/PartialTests/Partial.cue"));
    assert(exists("/dev_hdd0/PSXISO/PartialTests/Partial 2.bin"));
    /* 21. Missing split chunk cannot silently delete a healthy first piece. */
    make_file("/dev_hdd0/PS3ISO/Gap.iso.0","PART0");
    make_file("/dev_hdd0/PS3ISO/Gap.iso.2","PART2");
    FAIL(PLAT_PS3,"/dev_hdd0/PS3ISO/Gap.iso.0",0);
    assert(exists("/dev_hdd0/PS3ISO/Gap.iso.0"));
    assert(exists("/dev_hdd0/PS3ISO/Gap.iso.2"));
    /* 22. A protected malformed mapping fails closed. */
    {
        FILE *m=fopen(arcade_map,"wb");assert(m);
        fputs("broken",m);fclose(m);
    }
    make_file("/dev_hdd0/ARCADE/1942.zip","ROM");
    FAIL(PLAT_ARCADE,"/dev_hdd0/ARCADE/1942.zip",0);
    assert(exists("/dev_hdd0/ARCADE/1942.zip"));
    printf("PASS 22 MI PS3 end-to-end deletion and safety scenarios\\n");''',p)
save(p,s)
print("PASS V15.79 hardened multidisc selection, protected parent BIOS, sparse ISO and partial failures")

