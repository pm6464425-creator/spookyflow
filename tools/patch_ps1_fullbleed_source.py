#!/usr/bin/env python3
"""Exact-code modifications to pinned PSXonPS3 PS3 source (GPLv2)."""
import sys
from pathlib import Path
def once(path,old,new):
    s=path.read_text()
    if s.count(old)!=1:raise RuntimeError(f"{path}: expected exactly one anchor {old[:75]!r}, got {s.count(old)}")
    path.write_text(s.replace(old,new))
def main(root):
    com=root/"ps3/src/common.h"
    m=root/"ps3/src/main.c"
    once(com,'#define APP_DIR   "/dev_hdd0/game/" APP_ID "/USRDIR"',
             '#define APP_DIR   "/dev_hdd0/game/SPKF00001/USRDIR/PS1ENGINE"')
    once(com,'#define APP_TITLE "PCSX-ReARMed for PS3"',
             '#define APP_TITLE "SpookyFlow PS1 Graphics Engine"')
    once(m,'static int app_main(void)\n{',
           'static char sf_ps1_rom_arg[1100];\nstatic int app_main(void)\n{')
    once(m,'    config_game_begin(game_name);       /* its own settings (configs/<game>.ini), if it has them */',
          '''    config_game_begin(game_name);       /* its own settings (configs/<game>.ini), if it has them */
    if (sf_ps1_rom_arg[0]) {
        /* Set after any per-game config. 16:9 full-bleed in the real RSX game quad.
           Uniform 10% overscan also crops borders baked into source pixels. */
        cfg.aspect=ASPECT_FULL;
        cfg.screen_w=110; cfg.screen_h=110;
        cfg.show_fps=0;
        logf_("SF_PS1_FULLBLEED: aspect=%d screen_percent=%dx%d\\n",
              cfg.aspect,cfg.screen_w,cfg.screen_h);
    }''')
    once(m,
        '''    hooks_init();

    while (!app_exit_requested) {
        const char *game = menu_run();''',
        '''    hooks_init();

    if (sf_ps1_rom_arg[0]) {
        struct stat st;
        if (stat(sf_ps1_rom_arg,&st)==0 && S_ISREG(st.st_mode)) {
            logf_("SF_PS1_DIRECT: rom=%s\\n",sf_ps1_rom_arg);
            run_game(sf_ps1_rom_arg);
        } else logf_("SF_PS1_DIRECT: ROM_NOT_FOUND %s\\n",sf_ps1_rom_arg);
        app_exit_requested=1; /* go back to XMB, no second ROM selection */
    }
    while (!app_exit_requested) {
        const char *game = menu_run();''')
    once(m,
         '    (void)argc; (void)argv;\n    if (sysThreadCreate(&t, app_thread, NULL, 1000, APP_STACK, THREAD_JOINABLE, (char *)"pcsx main") != 0',
         '''    if(argc>1 && argv && argv[1] && argv[1][0]=='/' &&
        strlen(argv[1])<sizeof(sf_ps1_rom_arg)) {
        strcpy(sf_ps1_rom_arg,argv[1]); /* verified path from SpookyFlow */
    }
    if (sysThreadCreate(&t, app_thread, NULL, 1000, APP_STACK, THREAD_JOINABLE, (char *)"pcsx main") != 0''')
    s=m.read_text()
    assert 'sf_ps1_rom_arg' in s and 'cfg.aspect=ASPECT_FULL' in s
    print("PASS pinned GPL PSXonPS3: one-arg direct-game launch, real RSX fullbleed 16:9, no emulator-core/audio/pad patches")
if __name__=="__main__":
    if len(sys.argv)!=2:raise SystemExit("usage: patch.py source")
    main(Path(sys.argv[1]))
