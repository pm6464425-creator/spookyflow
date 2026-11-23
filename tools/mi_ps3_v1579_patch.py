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
print("PASS V15.79 changed only App, UI, safe policy, and isolated delete backend")
