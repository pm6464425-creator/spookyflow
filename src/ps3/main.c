/* main.c - spookyflow PS3 entry point (PSL1GHT + Tiny3D).
 *
 * Frame: read pads -> app_update (portable core) -> ui_draw (portable,
 * through render_tiny3d.c) -> tiny3d_Flip (waits for vblank, also runs
 * sysUtilCheckCallback).
 *
 * Exit paths (all end in a normal return from main = back to XMB):
 *   - hold ◯ 1.5 s on the cover flow  -> fade -> loop ends
 *   - Flow settings > Quit to XMB      -> fade -> loop ends
 *   - XMB "Quit game" (SYSUTIL_EXIT_GAME): our slot-0 callback saves settings
 *     and logs first; Tiny3D's own slot-3 callback then calls exit(0)
 *     (see tiny3d.c), which runs our atexit handler. */
#include <math.h>
#include <stdio.h>
#include <sys/process.h>
#include <sys/memory.h> /* read-only memory snapshot for Arcade diagnostics */
#include <sys/file.h>    /* sysFSStat/sysFsStat for SNES-only launch checkpoints */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/systime.h>
#include <sys/thread.h>
#include <sysutil/sysutil.h>
#include <tiny3d.h>

#include "app.h"
#include "draw.h"
#include "log.h"
#include "cover_diag.h"
#include "sound.h"
#include "ps3_platform.h"
#include "snes_name_match.h"
#include "sf_snes_bundle.h"
#include "arcade_engine.h"
#include "arcade_native_select.h"
#include "arcade_diag.h"
#include "sf_snes_diag.h"
#include "sf_snes_screen.h"
#include "sf_video_diag.h"
#include "sf_ps1_video_diag.h"
#include "sf_cobra_mount.h"
#include "sf_ps2_mount.h"
#include "sf_ps1_mount.h"
#include "sf_ps1_engine.h"
#include "sf_ps1_join.h"
#include "sf_ps2_hen.h"
#include "sf_bdiso_ntfs.h"
#include "sf_bdiso_art.h"
#include "sf_usb_diag.h"
#include "util.h"
#include "ui.h"

#define VERTEX_BUFFER_BYTES (4u * 1024u * 1024u)
#define TEXTURE_POOL_BYTES  (32u * 1024u * 1024u)   /* UI atlases, case images, backgrounds (1280x720 each) */
/* V15.45 temporary diagnostic A/B: the PS3 and BD games are silent only
 * after SpookyFlow launches them, while XMB-started titles have audio.
 * Remove our mixer/port from the equation entirely for one test build;
 * no audioInit()/audioPortOpen() or background decoder threads run here.
 * Menu sound effects intentionally OFF. Game audio remains untouched. */
#define SF_V15_45_AUDIO_ISOLATION_TEST 1
/* V15.48: stop *startup* Cobra probes from recreating retired TXT files.
 * Native Cobra backends and their error handling stay byte-for-byte intact. */
#ifdef SF_VIDEO_ONLY_DIAG
#define sf_cobra_diag_startup(stage) (0)
#define sf_cobra_diag_report_previous() ((void)0)
#endif

static Library g_lib;
static App g_app;
static unsigned char g_scratch[UI_SCRATCH_BYTES];
static volatile int g_sys_exit;
static int g_saved_on_exit;
/* V15.44: audio is handed back exactly once after an accepted disc mount.
 * Previously we waited for atexit while the game's autoplay was already
 * queued; SNES did not even use the normal exit path (direct exitspawn). */
static int g_audio_released_for_launch;


/* Poll presence without touching native NTFS mount handles or files.
 * Port 0..7: native storage probe for NTFS; 0..10: LV2 sysFs FAT drives. */
/* This is done ONCE when library loading prepares its USB roots, not
 * every frame. libntfs can see disks that lv2 sysFs cannot enumerate. */
static uint32_t native_bdiso_ready_mask(void)
{
    uint32_t mask=0;
    int p,mounted=sf_bdntfs_start();
    for(p=0;p<8;p++)
        if(sf_bdntfs_has_port(p))mask|=(uint32_t)1u<<p;
    log_printf("bdiso: native roots probe mounted=%d ready_mask=0x%02x",
               mounted,(unsigned)mask);
    return mask;
}

static uint32_t usb_media_mask(void)
{
    uint32_t mask=sf_bdntfs_usb_present_mask();
    int port;
    for(port=0;port<SCAN_USB_PORTS;port++){
        char path[24];
        snprintf(path,sizeof(path),"/dev_usb%03d",port);
        if(ps3_fs_exists(path))mask |= (uint32_t)1u<<port;
    }
    return mask;
}

static double now_seconds(void)
{
    u64 sec = 0, nsec = 0;
    sysGetCurrentTime(&sec, &nsec);
    return (double)sec + (double)nsec * 1e-9;
}

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; (void)usrdata;
    switch (status) {
    case SYSUTIL_EXIT_GAME:
        sf_ps2_hen_cancel(); /* abort any in-progress encrypted temporary file */
        sf_ps1_join_cancel(); /* abort only our PS1 .sfpart on XMB exit */
        g_sys_exit = 1;
        log_printf("sysutil: exit requested from XMB");
        /* Tiny3D's callback (slot 3) calls exit(0) right after this one */
        if (!g_saved_on_exit) { app_save(&g_app); g_saved_on_exit = 1; }
        break;
    case SYSUTIL_MENU_OPEN:
        log_printf("sysutil: XMB menu opened");
        break;
    case SYSUTIL_MENU_CLOSE:
        log_printf("sysutil: XMB menu closed");
        break;
    default:
        break;
    }
}

/* Background thread for the library scan (sys/thread.h, verified against
 * PSL1GHT: sysThreadCreate(id, entry, arg, prio, stack, flags, name),
 * sysThreadExit(u64)). Priority 1500 is below the main thread's usual 1000
 * so drawing always wins. */
typedef struct { void (*fn)(void *); void *arg; } AsyncJob;
static AsyncJob g_job;

static void async_entry(void *p)
{
    AsyncJob *j = (AsyncJob *)p;
    j->fn(j->arg);
    sysThreadExit(0);
}

static int run_async(void (*fn)(void *), void *arg)
{
    sys_ppu_thread_t tid;
    g_job.fn = fn;
    g_job.arg = arg;
    if (sysThreadCreate(&tid, async_entry, &g_job, 1500, 256 * 1024, 0, (char *)"sf_scan") != 0) return -1;
    return 0;
}

/* Clean power off: webMAN MOD's /shutdown.ps3 shuts down through the VSH,
 * exactly like the XMB's "Turn Off System". (lv2 syscall 379 with 0x1100,
 * used in fix 12, cuts power without the VSH saving its state, so the next
 * boot reported "The system was not turned off properly".) */
static int power_off_via_webman(void)
{
    return ps3_webman_get("/shutdown.ps3");
}

/* Launch: webMAN mounts and starts the game once spookyflow is back at the XMB */
static int launch_via_webman(const char *request)
{
    int rc=ps3_webman_send(request);
#ifdef SF_PS1_VIDEO_DIAG
    /* The host.launch callback is the existing PS1 fallback transport.
     * No extra request, mount command, plugin, or system patch is issued. */
    sf_ps1_video_diag_webman(request,rc);
#endif
    return rc;
}
#ifdef SF_PS1_VIDEO_DIAG
static int launch_ps1_forensic(const char *path,int emulator)
{
    int rc;
    sf_ps1_video_diag_before_launch(path,emulator,"PSX_FULLBLEED_PPU_OR_SONY");
    /* New user-mode graphics engine: full-screen 16:9 is calculated from
     * the actual PS1 frame into the PS3 RSX. It has its own memory cards.
     * The old Sony engine stays intact; explicit emulator preference 1
     * retains the Sony native mounting behavior as a compatibility fallback.
     * NTFS pointer indexes continue through the original webMAN backend. */
    if(emulator!=1 && path && path[0]=='/' &&
       ps3_fs_is_file(NULL,path) && !strstr(path,".ntfs[")) {
        rc=sf_ps1_engine_ready();
        if(rc==0) {
            static char cue_alias[1200];
            const char *launch_path=path;
            const char *args[2];
            int alias_rc=sf_ps1_fullbleed_cue_alias(path,cue_alias,sizeof cue_alias);
            if(alias_rc==0) launch_path=cue_alias;
            if(ps3_fs_is_file(NULL,launch_path)) {
                log_printf("PS1_DIAG STAGE=NEW_NATIVE_RENDERER TARGET=PSXONPS3_FULLSCREEN "
                           "ROM=%s original=%s alias_rc=%d width=110 height=110 aspect=16:9",
                           launch_path,path,alias_rc);
                args[0]=launch_path; /* PS3 supplies executable argv[0] */
                args[1]=NULL;
                ps3_audio_stop();
                g_audio_released_for_launch=1;
                log_printf("PS1_DIAG STAGE=PSX_SELF_SPAWN APP_TYPE=4 ROM=%s",launch_path);
                /* On success the SpookyFlow process is replaced. */
                sysProcessExitSpawn2(sf_ps1_engine_path(),args,NULL,NULL,0,1001,
                                     SYS_PROCESS_SPAWN_STACK_SIZE_1M);
                log_printf("PS1_DIAG STAGE=PSX_SELF_SPAWN RESULT=REJECTED "
                           "FALLBACK=ORIGINAL_SONY_EMULATOR");
                g_audio_released_for_launch=0;
            }
        } else
            log_printf("PS1_DIAG STAGE=NEW_NATIVE_RENDERER RESULT=UNAVAILABLE rc=%d "
                       "FALLBACK=SONY",rc);
    }
    rc=sf_ps1_mount(path,emulator);
    sf_ps1_video_diag_launch_result("COBRA_NATIVE_FALLBACK",rc);
    return rc;
}
static int launch_ps1_staged_forensic(void)
{
    int rc;
    sf_ps1_video_diag_before_launch(NULL,-1,"MULTIBIN_CUE_CACHE");
    rc=sf_ps1_join_mount();
    sf_ps1_video_diag_launch_result("MULTIBIN_CUE_CACHE",rc);
    return rc;
}
#endif

/* BDISO: native Cobra mounting for sysFs drives; webMAN-assisted mounting
 * for NTFS .ntfs[BDISO] index files (which aren't a real .iso filepath).
 * Never send these pointer files to the game ISO mount syscall. */
static int launch_bdiso(const char *path)
{
    if(!path) return -1;
    if(!strncmp(path,NTFS_INDEX_DIR "/",sizeof(NTFS_INDEX_DIR))) {
        char enc[LAUNCH_CMD_MAX],request[LAUNCH_CMD_MAX];
        if(!sf_ends_with_i(path,".ntfs[BDISO]")) return -2;
        if(launch_url_encode(path,enc,sizeof(enc))!=0) return -3;
        /* webMAN MOD documents /mount.ps3/<path> for BDISO; no game syscall.
           webMAN itself controls the raw-ISO plugin without unloading slot 0. */
        if(snprintf(request,sizeof(request),"/mount.ps3%s",enc)
             >= (int)sizeof(request)) return -4;
        return ps3_webman_send(request);
    }
    if(!strncmp(path,"/dev_usb00",10) &&
       path[10]>='0' && path[10]<='7' && sf_bdntfs_has_port(path[10]-'0')) {
        /* Real NTFS paths cannot be passed to Cobra mount_bd_disc_image:
           they need the sector index and raw_iso.sprx. Reuse the index
           prepared by webMAN/prepISO so game plugins remain untouched. */
        char index[LAUNCH_CMD_MAX],name[256],enc[LAUNCH_CMD_MAX],req[LAUNCH_CMD_MAX];
        const char *base=strrchr(path,'/');
        size_t n;
        base=base?base+1:path;
        snprintf(name,sizeof(name),"%s",base);
        n=strlen(name);
        if(n>6 && sf_ends_with_i(name,".iso.0"))name[n-6]=0;
        else if(n>4 && sf_ends_with_i(name,".iso"))name[n-4]=0;
        else return -5;
        if(snprintf(index,sizeof(index),"%s/%s.ntfs[BDISO]",NTFS_INDEX_DIR,name)
           >=(int)sizeof(index)) return -6;
        if(!ps3_fs_exists(index)) {
            char resp[280];
            (void)ps3_webman_get_wait("/refresh.ps3?prepntfs",180,resp,sizeof(resp));
        }
        if(!ps3_fs_exists(index)) {
            int raw=sf_bdntfs_mount_raw_iso(path);
            log_printf("bdiso: webMAN NTFS pointer absent; independent raw ISO result=%d",raw);
            return raw;
        }
        if(launch_url_encode(index,enc,sizeof(enc))!=0) return -8;
        if(snprintf(req,sizeof(req),"/mount.ps3%s",enc)>=(int)sizeof(req))
            return -9;
        return ps3_webman_send(req);
    }
    return sf_cobra_mount_bd_iso(path);
}

/* webMAN re-indexes every NTFS drive it has mounted; its answer reads
 * "NTFS VOLUMES: <n> ... <games> ..." (include/cmd/refresh.h) */
static int ntfs_refresh(void)
{
    char resp[600];
    const char *p;
    int games = -1;
    sf_bdiso_reset_attempts();
    if (ps3_webman_get_wait("/refresh.ps3?prepntfs", 180, resp, sizeof(resp)) != 0) return -1;
    p = strstr(resp, "NTFS VOLUMES:");
    log_printf("ntfs: webMAN: %.120s", p ? p : resp);
    if (p) {
        const char *q = strstr(p, "\">");                    /* " - <a href=...>N</a> games" */
        if (q) games = atoi(q + 2);
        else games = 0;
    }
    return games < 0 ? 0 : games;
}

static void read_wm_config(WmConfig *cfg)
{
    unsigned char buf[1024];
    size_t n = 0;
    FILE *f = fopen(WM_CONFIG_PATH, "rb");
    if (f) { n = fread(buf, 1, sizeof(buf), f); fclose(f); }
    if (!launch_parse_wm_config(buf, n, cfg) && f)
        log_printf("launch: %s not understood (%u bytes, starts %02x %02x %02x %02x)", WM_CONFIG_PATH, (unsigned)n,
                   n > 0 ? buf[0] : 0, n > 1 ? buf[1] : 0, n > 2 ? buf[2] : 0, n > 3 ? buf[3] : 0);
}

static void on_exit_handler(void)
{
    static int done;
    if (done) return;           /* called directly before power-off, then again by atexit */
    done = 1;
    if (!g_saved_on_exit) { app_save(&g_app); g_saved_on_exit = 1; }
    ps3_covers_stop();
    ps3_audio_stop();
    cover_diag("CIERRE","La aplicacion cerro correctamente");
    cover_diag_stop();
    log_printf("shutdown");
    log_close();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    ps3_pad_end();
}

/* Wait until the flip queued by the last tiny3d_Flip has happened. The RSX
 * runs the command stream in order, so once that flip is done every draw of
 * the previous frame is finished and the vertex buffer is free to reuse.
 * Same flip-status poll Tiny3D's own waitFlip() uses (rsxutil.c), but WITHOUT
 * gcmResetFlipStatus(): Tiny3D's next waitFlip() then sees "done" at once and
 * does the reset itself. gcmGetFlipStatus() comes from <rsx/gcm_sys.h>,
 * which tiny3d.h already includes (declared there as u32; Tiny3D casts it to
 * a signed int). 0 = flip done, >0 = still pending, <0 = error. */
static void wait_gpu_frame_done(void)
{
    int n = 500;                      /* 500 x 200 us = 100 ms safety cap */
    while (n-- > 0 && (s32)gcmGetFlipStatus() > 0) usleep(200);
}

/* Frame-time log: a summary every 10 s plus any frame slower than 50 ms
 * (rate-limited), so a pasted log shows whether the app keeps 60 fps. */
static void frame_stats(float dt, double draw_s, double wait_s)
{
    static float acc, worst;
    static double draw_acc, wait_acc, draw_worst;
    static int frames, slow_logged;
    acc += dt; frames++;
    draw_acc += draw_s; wait_acc += wait_s;
    if (dt > worst) worst = dt;
    if (draw_s > draw_worst) draw_worst = draw_s;
    if (dt >= 0.05f && slow_logged < 40) {
        log_printf("perf: slow frame %.0f ms (draw %.1f ms, gpu wait %.1f ms)", dt * 1000.0f, draw_s * 1000.0, wait_s * 1000.0);
        slow_logged++;
    }
    if (acc >= 10.0f) {
        log_printf("perf: %.1f fps average, worst frame %.0f ms; draw avg %.1f ms (worst %.1f), gpu wait avg %.1f ms",
                   (float)frames / acc, worst * 1000.0f, draw_acc * 1000.0 / frames, draw_worst * 1000.0,
                   wait_acc * 1000.0 / frames);
        acc = 0.0f; worst = 0.0f; frames = 0;
        draw_acc = wait_acc = draw_worst = 0.0;
    }
}

static void setup_screen(void)
{
    Screen s;
    float vw = (float)Video_Resolution.width, vh = (float)Video_Resolution.height;
    float display_aspect;
    /* HD modes have square pixels. For SD (480/576) use the console's TV
     * aspect setting. UNVERIFIED: Tiny3D's Video_aspect == 1 means 4:3
     * (VIDEO_ASPECT_4_3) as in PSL1GHT's video.h. */
    if (vh >= 700.0f) display_aspect = vw / vh;
    else display_aspect = (Video_aspect == 1) ? (4.0f / 3.0f) : (16.0f / 9.0f);
    s.W = 848.0f;            /* Tiny3D 2D virtual space */
    s.H = 512.0f;
    s.pa = display_aspect * 512.0f / 848.0f;
    s.px = vh / 512.0f;
    d_set_screen(&s);
    /* Map the 848x512 virtual space onto the WHOLE screen (Tiny3D's default
     * 720p viewport leaves an overscan border). UNVERIFIED semantics:
     * scale = output pixels per virtual unit, inferred from tiny3d.c's own
     * defaults (e.g. 848/611 at 720p). */
    tiny3d_UserViewport(1, 0.0f, 0.0f, vw / 848.0f, vh / 512.0f, vw / 1920.0f, vh / 1080.0f);
    log_printf("video: %dx%d, display aspect %.3f, pixel aspect %.4f", (int)vw, (int)vh, display_aspect, s.pa);
}

/* V15.41: correct PS3 native process replacement.
 * The original pSNES EBOOT.BIN is a type-8 NPDRM application and cannot
 * launch via sysProcessExitSpawn2 (the V15.39/40 XMB regression).
 * Use a build-verified type-4 APP SELF bundled with SpookyFlow itself.
 * It is the exact same official pSNES 7.2 executable, re-signed as APP.
 * pSNES retains its own /dev_hdd0/game/PSNE00001/USRDIR resources, prepared
 * safely by sf_psnes_prepare; no webMAN, RetroArch, or external PKG needed.
 */
/* V15.65 ARCADE ONLY: native FBNeo/MAME 0.125 engines from verified
 * FBNeo Retro Loader Plus v2.30 release, bundled in this SpookyFlow PKG.
 * Uses exactly the argv format of upstream c_fbaRL::LaunchFBACore().
 * No separate XMB app, no webMAN MOD, no menu/selector handoff.
 * The cores must be signed APP SELF (type 4) and have SpookyFlow-specific
 * paths in their ELF, with all companion resources in our USRDIR.
 */

/* A read-only syscall snapshot BEFORE SpookyFlow exits. This is NOT memory
 * available inside the later MAME process; the TXT states that limitation. */
typedef struct {u32 total_user_memory;u32 available_user_memory;} SfArcadeUserMem;
LV2_SYSCALL sf_arcade_memory_snapshot(SfArcadeUserMem *m)
{
    lv2syscall1(352,(u64)m);
    return_to_user_prog(s32);
}

static int launch_arcade_native(const char *rom_path)
{
    static const char fbneo[]=
        "/dev_hdd0/game/SPKF00001/USRDIR/cores/fb_neo.SELF";
    static const char mame[]=
        "/dev_hdd0/game/SPKF00001/USRDIR/MAME125/mame125.SELF";
    static const char fbneo_legacy[]=
        "/dev_hdd0/game/SPKF00001/USRDIR/cores/fb_neo_legacy.SELF";
    static const char reload[]=
        "/dev_hdd0/game/SPKF00001/USRDIR/RELOAD.SELF";
    static const char config[]=
        "/dev_hdd0/game/SPKF00001/USRDIR/retroarch.cfg";
    static const char saves[]=
        "/dev_hdd0/game/SPKF00001/USRDIR/cores/savefiles";
    static const char states[]=
        "/dev_hdd0/game/SPKF00001/USRDIR/cores/savestates";
    const char *args[12];
    const char *exe;
    int engine;
    char compat_reason[160];
    sysFSStat stat;
    /* A single report is rewritten on EVERY X press, even when the ROM,
     * core, BIOS or configuration cannot be opened. The TXT is flushed
     * before exitspawn so it survives the FBNeo "Wrong romset" screen. */
    /* Detect the exact MAME0125-era NeoGeo BIOS and prove by original
     * MAME0.125 chip CRCs whether the selected ZIP belongs to that engine.
     * Keep FBNeo for modern BIOS and all unrelated arcade games. */
    engine=arcade_native_engine_select(rom_path,
            arcade_engine_for_rom(rom_path),compat_reason,sizeof compat_reason);
    exe=engine==ARCADE_ENGINE_MAME125 ? mame :
        engine==ARCADE_ENGINE_FBNEO_LEGACY ? fbneo_legacy :
        engine==ARCADE_ENGINE_FBNEO ? fbneo : NULL;
    if(arcade_diag_begin(rom_path,engine,exe,reload,config)!=0){
        log_printf("arcade: cannot open unique diagnostico_mame.txt");
        return -95;
    }
    arcade_diag_event("RUTA_ARCADE_BIOS_Y_ROMSET_CRC",engine,compat_reason);

    {
        SfArcadeUserMem mem={0,0};
        char mem_text[170];
        int mem_rc=sf_arcade_memory_snapshot(&mem);
        if(mem_rc==0)
            snprintf(mem_text,sizeof mem_text,
                    "TOTAL_BYTES=%u DISPONIBLES_BYTES=%u MEDIDO_EN_SPOOKYFLOW_NO_EN_MAME",
                    (unsigned)mem.total_user_memory,(unsigned)mem.available_user_memory);
        else
            snprintf(mem_text,sizeof mem_text,
                     "SYSCALL_352_ERROR=%d NO_SE_PUEDE_ESTIMAR_RAM_MAME",mem_rc);
        arcade_diag_event("MEMORIA_PS3_ANTES_DEL_CAMBIO_DE_PROCESO",mem_rc,mem_text);
    }

    log_printf("arcade: automatic engine=%s reason=%s rom=%s",
               engine==ARCADE_ENGINE_MAME125?"MAME0125":
               engine==ARCADE_ENGINE_FBNEO?"FBNeo":
               engine==ARCADE_ENGINE_FBNEO_LEGACY?"FBNeoLegacy":"NONE",
               compat_reason,rom_path?rom_path:"(null)");
    if(!rom_path || !ps3_fs_is_file(NULL,rom_path)){
        arcade_diag_event("ROM_ZIP_NO_ENCONTRADO",-93,rom_path);
        log_printf("arcade: direct launch aborted, ZIP missing path=%s",
                   rom_path?rom_path:"NULL");
        return -93;
    }
    if(!engine){
        /* Never call everything "incompatible": missing PS3 driver, 7z
         * archives and incomplete CRC sets are different actionable causes. */
        int failure=-90;
        if(strstr(compat_reason,"7Z_NO_ADMITIDO"))failure=-96;
        else if(strstr(compat_reason,"ROM_NO_INCLUIDA_EN_BASE_MOTORES_PS3"))
            failure=-97;
        else if(strstr(compat_reason,"CRC_MISMATCH"))failure=-98;
        arcade_diag_event("ARCADE_MOTOR_NO_SELECCIONABLE",failure,compat_reason);
        log_printf("arcade: no engine rc=%d reason=%s rom=%s",
                   failure,compat_reason,rom_path);
        return failure;
    }
    if(sysFsStat(exe,&stat)!=0 || stat.st_size<1000000u ||
       !ps3_fs_is_file(NULL,reload)){
        arcade_diag_event("MOTOR_O_RELOAD_NO_DISPONIBLE",-91,exe);
        log_printf("arcade: verified APP SELF/RELOAD absent engine=%d path=%s",
                   engine,exe);
        return -91;
    }
    if(!ps3_fs_is_file(NULL,config)){
        arcade_diag_event("CONFIG_16_9_NO_ENCONTRADA",-92,config);
        log_printf("arcade: fullscreen RetroArch config missing: %s",config);
        return -92;
    }
    args[0]=rom_path;
    args[1]=reload;
    args[2]="-s";args[3]=saves;
    args[4]="-c";args[5]=config;
    args[6]="-S";args[7]=states;
    /* Legacy FBNeo MUST load the same patched core via RetroArch -L.
     * MAME continues to use original FBNeo RL Plus argument convention. */
    args[8]="-L";
    args[9]=engine==ARCADE_ENGINE_FBNEO_LEGACY ? fbneo_legacy : fbneo;
    args[10]="-v";args[11]=NULL;
    log_printf("arcade: SELECT engine=%s source=FBNeoRLPlus_v2.30_DB",
               engine==ARCADE_ENGINE_MAME125?"MAME125":
               engine==ARCADE_ENGINE_FBNEO_LEGACY?"FBNeo_LEGACY":"FBNeo");
    log_printf("arcade: SPAWN_FULL_SCREEN=true aspect=16:9 rom=%s exe=%s",
               rom_path,exe);
    /* The emulator's runtime screen cannot be intercepted by the prior
     * process. The durable TXT records this precise handoff boundary. */
    arcade_diag_event("INTENTANDO_INICIAR_MOTOR_PS3",0,exe);
    /* Correct audio handoff is required before replacing the process. */
    ps3_audio_stop();
    g_audio_released_for_launch=1;
    sysProcessExitSpawn2(exe,args,NULL,NULL,0,1001,
                         SYS_PROCESS_SPAWN_STACK_SIZE_1M);
    /* Reaching this point means LV2 rejected the process replacement. */
    arcade_diag_event("PS3_RECHAZO_SPAWN",-94,exe);
    log_printf("arcade: APP_SELF_SPAWN_RETURNED error; restoring launcher audio");
#if !SF_V15_45_AUDIO_ISOLATION_TEST
    ps3_audio_start();
#endif
    g_audio_released_for_launch=0;
    return -94;
}

static int launch_snes_psnes(const char *rom_path)
{
    static const char exe[] =
        "/dev_hdd0/game/" SF_APPID "/USRDIR/SNESENGINE/PSNES_ENGINE.SELF";
    const char *args[2];
    sysFSStat rom_stat, exe_stat;
    int rc;

    log_printf("snes: DIAG_LAUNCH_SELECTED rom=%s",
               rom_path ? rom_path : "(null)");
    if (!rom_path || !rom_path[0] || !ps3_fs_is_file(NULL, rom_path)) {
        log_printf("snes: DIAG_ROM_CHECK=FAIL missing_or_unreadable");
        return -64;
    }
    if (sysFsStat(rom_path, &rom_stat) == 0)
        log_printf("snes: DIAG_ROM_CHECK=OK bytes=%llu",
                   (unsigned long long)rom_stat.st_size);
    else
        log_printf("snes: DIAG_ROM_STAT=FAILED (is_file reported success)");

    /* Setup original pSNES data, only if it was never installed before;
     * never overwrite an existing PSNE00001 title. */
    log_printf("snes: DIAG_ENGINE_PREPARE_BEGIN data_home=/dev_hdd0/game/PSNE00001");
    rc = sf_psnes_prepare();
    log_printf("snes: DIAG_ENGINE_PREPARE_RESULT=%d", rc);
    if (rc != 0) {
        log_printf("snes: embedded pSNES 7.2 preflight rc=%d", rc);
        return rc;
    }

    /* V15.42: pSNES defaults to FIT (small 4:3 image). Before launching,
     * set only PSNES.EMULATION.SCALING="FULL" and the selected ROM's own
     * PSNES_GAME.EMULATION.SCALING if its override exists.
     * Fail open to keep games playable if filesystem/config is unavailable;
     * report exact reason in the existing diagnostic.
     * TV output resolution remains selected by PS3 / Tiny3D (1080p if
     * available), independent from the native SNES ROM resolution. */
    rc = sf_snes_force_widescreen(rom_path);
#ifdef SF_VIDEO_ONLY_DIAG
    sf_video_diag_before_snes(rom_path, rc);
#endif
    if (rc == 0) {
        log_printf("snes: DIAG_VIDEO_SCALING=FULL_16_9 cfg=GLOBAL_PLUS_ROM_OVERRIDE");
    } else {
        log_printf("snes: DIAG_VIDEO_SCALING=CONFIG_UPDATE_FAILED rc=%d; preserving playable SNES launch", rc);
    }

    /* New type-4 SELF must be present and SHA256-pinned. */
    log_printf("snes: DIAG_APP_SELF_PRECHECK path=%s", exe);
    rc = sf_psnes_plain_ready();
    if (rc != 0) {
        log_printf("snes: DIAG_APP_SELF_PRECHECK_FAILED rc=%d", rc);
        return rc;
    }
#ifdef SF_VIDEO_ONLY_DIAG
    sf_video_diag_verified_self();
#endif
    if (sysFsStat(exe, &exe_stat) != 0 || !exe_stat.st_size) {
        log_printf("snes: DIAG_APP_SELF_STAT=FAIL");
        return -80;
    }
    log_printf("snes: DIAG_APP_SELF_STAT=OK bytes=%llu",
               (unsigned long long)exe_stat.st_size);

    /* PS3 process spawning supplies the target's argv[0] itself. Feed one
     * user argument (ROM path), exactly as native RetroArch PS3 does.
     * pSNES 7.2 main(argc,argv) then reads argv[1] as the selected ROM.
     * Do NOT supply exe as args[0] (old V15.39/40 regression). */
    args[0] = rom_path;
    args[1] = NULL;

    rc = sf_snes_diag_before_spawn(rom_path);
    if (rc != 0) {
        log_printf("snes: DIAG_SPAWN_CANCELLED cause=persistent_trace_not_saved rc=%d", rc);
        return rc;
    }
    log_printf("snes: DIAG_SPAWN_KIND=SIGNED_APP_SELF no_npdrm=YES direct=YES");
    log_printf("snes: DIAG_SPAWN_CALL exe=%s supplied_argc=1 supplied_arg0_ROM=%s",
               exe, args[0]);
    log_printf("snes: handoff pSNES 7.2 APP SELF ROM=%s (no webMAN)", rom_path);

    /* On success SpookyFlow is replaced; on failure capture in one
     * diagnostic file and display an error on screen. */
    /* V15.44: exitspawn replaces the process directly and must not rely on
     * normal atexit() cleanup. PSL1GHT's ps3load does explicit cleanup
     * before sysProcessExitSpawn2; doing the same for the streaming audio
     * port prevents handing pSNES a still-open audio session. */
    log_printf("audio: DIAG_SNES_PHASE=PRE_SPAWN ui_port_state=%d", ps3_audio_diag_state());
    log_printf("audio: DIAG_SNES_HANDOFF=READY runtime_video_unchanged=YES");
    log_printf("audio: HANDOFF_SNES pre-release before direct spawn");
    ps3_audio_stop();
    g_audio_released_for_launch = 1;
    log_printf("audio: DIAG_SNES_PHASE=AFTER_STOP ui_port_state=%d", ps3_audio_diag_state());
    log_printf("audio: DIAG_SNES_OWN_AUDIO=UNOBSERVABLE_AFTER_PROCESS_REPLACEMENT");
    log_printf("audio: HANDOFF_SNES audioPortStop/Close/Quit completed, executing pSNES");
    sysProcessExitSpawn2(exe, args, NULL, NULL, 0, 1001,
                         SYS_PROCESS_SPAWN_STACK_SIZE_1M);
    sf_snes_diag_spawn_returned();
    log_printf("snes: DIAG_RESULT=APP_SELF_SPAWN_API_RETURNED");
    /* If the operating system refused to replace us, keep the UI usable. */
#if SF_V15_45_AUDIO_ISOLATION_TEST
    log_printf("audio: DIAG_SNES_SPAWN_REJECTED port_never_initialized; UI_AUDIO_STILL_DISABLED");
#else
    log_printf("audio: HANDOFF_SNES spawn rejected; restarting UI audio");
    ps3_audio_start();
#endif
    g_audio_released_for_launch = 0;
    return -65;
}

int main(int argc, const char *argv[])
{
    AppHost host;
    double prev;
    int rc;
    (void)argc; (void)argv;

    /* Create a persistent trace BEFORE any graphics, pad, audio or UI work.
     * This separates an early startup freeze from a Cobra mount freeze. */
    ps3_fs_mkdirs();
#if defined(SF_ARCADE_DIAG_ONLY)
    /* Delete only retired diagnostic files, never ROMs or covers. 
     * Preserve the last MAME TXT until the next X press overwrites it. */
    (void)ps3_fs_cleanup_legacy_diagnostics();
#elif defined(SF_PS1_VIDEO_DIAG)
    {
        int removed=ps3_fs_cleanup_legacy_diagnostics();
        int diag_rc=log_open(SF_DIAG_SINGLE_PATH);
        if(diag_rc==0)
            log_printf("ps1_video_diag: REPORT=" SF_DIAG_SINGLE_PATH
                       " OLD_DIAGNOSTIC_TXTS_REMOVED=%d",removed);
    }
#elif defined(SF_VIDEO_ONLY_DIAG)
    {
        int removed=ps3_fs_cleanup_legacy_diagnostics();
        int diag_rc=log_open(SF_DIAG_SINGLE_PATH);
        if(diag_rc==0) {
            log_printf("video_diag: SINGLE_REPORT=" SF_DIAG_SINGLE_PATH " OLD_TXTS_REMOVED=%d",removed);
            log_printf("video_diag: SCOPE=SCREEN_SNES; audio_and_other_platforms_UNMODIFIED");
            sf_snes_diag_on_startup(); /* import actual previous pSNES boot trace */
        }
    }
#elif defined(SF_SINGLE_DIAG_FILE)
    {int removed=ps3_fs_cleanup_legacy_diagnostics();
     int diag_rc=log_open(SF_DIAG_SINGLE_PATH);
     if(diag_rc==0) {
         log_printf("snes: DIAG_STARTUP V15.40 legacy_reports_removed=%d", removed);
         log_printf("snes: DIAG_REPORT_PATH=" SF_DIAG_SINGLE_PATH " MODE=APPEND_UNTIL_80KB");
         /* Import the previous pSNES process trace BEFORE a new launch. */
         sf_snes_diag_on_startup();
         log_printf("snes: DIAG_READY select a SNES cover and press X");
     }
    }
#endif
    rc = sf_usb_diag_begin();
    log_printf("usb_diag: report open status=%d at " SF_USB_DIAG_PATH,rc);
    rc = cover_diag_start(SF_DATA_DIR "/diagnostico_portadas.txt");
    log_printf("bdiso diag: report open status=%d at " SF_DATA_DIR "/diagnostico_portadas.txt",rc);
    cover_diag("INICIO","El programa arranco. Solo portadas BDISO. No altera ISO ni COVERS");
    rc = sf_cobra_diag_startup("BEFORE_TINY3D_INIT");
#ifndef SF_SINGLE_DIAG_FILE
    if (log_open(SF_LOG_PATH) == 0) {
#endif
        log_printf("==== %s %s starting ====", APP_NAME, APP_VERSION);
        log_printf("cobra early-probe build EARLY_PROBE_20261109_R1, startup result=%d", rc);
#ifndef SF_SINGLE_DIAG_FILE
    }
#endif
    /* V15.24 PS1 report: do not dump PS2 Classics forensic state at startup.
     * PS2 encryption and its staging/validation behavior are unchanged. */
    sf_cobra_diag_report_previous();
    sf_usb_diag_snapshot("PRE_NATIVE_NTFS",NULL,NULL,usb_media_mask());

    tiny3d_Init(VERTEX_BUFFER_BYTES);
    if (sf_cobra_diag_startup("AFTER_TINY3D_INIT") != 0)
        log_printf("cobra: failed to update startup probe after Tiny3D");
    ps3_pad_init();
    (void)sf_cobra_diag_startup("AFTER_PAD_INIT");
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_cb, NULL);
    (void)sf_cobra_diag_startup("AFTER_SYSUTIL_CALLBACK");
    atexit(on_exit_handler);   /* registered after tiny3d_Init: runs before tiny3d_Exit */

    sound_init();
#if SF_V15_45_AUDIO_ISOLATION_TEST
    log_printf("audio: DIAG_SYSTEM=V15.46 AUDIO_ONLY_RPT path=/dev_hdd0/tmp/spookyflow/diagnostico_audio.txt");
    log_printf("audio: DIAG_UI_AUDIO_MODE=ISOLATED no_audioInit no_audioPortOpen no_decoder");
    log_printf("audio: DIAG_UI_PORT_INITIAL=%d expected_zero=YES", ps3_audio_diag_state());
    log_printf("audio: DIAG_NEXT_GAME_AUDIO=CANNOT_BE_MEASURED_FROM_LAUNCHER");
    log_printf("audio: DIAG_REQUEST=COMPARE_SAME_TITLE_DIRECT_XMB_VS_SPOOKYFLOW_AND_REPORT_HEARD");
    log_printf("audio: DIAG_ISOLATION_TEST=ACTIVE V15.45 UI_AUDIO_DISABLED=YES GAME_AUDIO_NOT_MODIFIED");
    log_printf("audio: DIAG_NO_AUDIO_INIT=YES no_LV2_audio_ports_no_mixer_threads_no_wav_decoder");
#else
    ps3_load_sounds();
    ps3_audio_start();
#endif
    if (ps3_render_init(TEXTURE_POOL_BYTES) != 0) log_printf("render: texture pool allocation failed");
    if (ps3_render_init_covers() != 0) log_printf("render: cover texture allocation failed");
    rc = ui_upload_textures(g_scratch, sizeof(g_scratch));
    if (rc != 0) log_printf("render: texture upload failed (%d)", rc);
    setup_screen();
#if defined(SF_ARCADE_DIAG_ONLY)
    /* Native Arcade reports are generated only on X. */
#elif defined(SF_PS1_VIDEO_DIAG)
    sf_ps1_video_diag_startup();
#elif defined(SF_VIDEO_ONLY_DIAG)
    sf_video_diag_startup();
#endif

    memset(&host, 0, sizeof(host));
    host.fs_list = ps3_fs_list;
    host.arcade_fs_page = ps3_fs_list_arcade_page;
    host.settings_path = SF_SETTINGS_PATH;
    host.prepare = ps3_load_tab_images;
    host.run_async = run_async;
    host.own_sfo_path = "/dev_hdd0/game/" SF_APPID "/PARAM.SFO";
    /* Preserve original optional services; PS3 mounting never calls webMAN. */
    host.power_off = power_off_via_webman;
    host.launch = launch_via_webman; /* Legacy PS1 only. PS2 uses Cobra. */
    host.ps2_preflight = sf_ps2_preflight;
    host.launch_ps2 = sf_ps2_mount;
#ifdef SF_PS1_VIDEO_DIAG
    host.launch_ps1 = launch_ps1_forensic;
#else
    host.launch_ps1 = sf_ps1_mount;
#endif
    host.ps1_join_start = sf_ps1_join_start;
    host.ps1_join_poll = sf_ps1_join_poll;
#ifdef SF_PS1_VIDEO_DIAG
    host.ps1_join_mount = launch_ps1_staged_forensic;
#else
    host.ps1_join_mount = sf_ps1_join_mount;
#endif
    host.ps2_hen_start = sf_ps2_hen_start;
    host.ps2_hen_poll = sf_ps2_hen_poll;
    host.launch_ps3 = sf_cobra_mount_ps3;
    host.launch_bdiso = launch_bdiso;
    host.launch_snes = launch_snes_psnes;
    host.launch_arcade = launch_arcade_native;
    host.diag_ps3_select = sf_cobra_diag_selection;
    host.read_wm_config = read_wm_config;
    host.read_at = ps3_read_at;
    host.ntfs_refresh = ntfs_refresh;
    host.usb_media_mask = usb_media_mask;
    host.native_bdiso_ready_mask = native_bdiso_ready_mask;
    host.usb_diag = sf_usb_diag_snapshot;
    host.ntfs_bdiso_online = sf_bdntfs_index_online;
    host.music_dir = SF_MUSIC_DIR;
    host.bg_dir = SF_BG_DIR;
    host.load_bg_image = ps3_load_bg_image;
    host.is_file = ps3_fs_is_file;
    host.write_file = ps3_fs_write_file;
    host.remove_file = ps3_fs_remove_file;
    host.covers = ps3_covers_start();
    host.http_get = ps3_http_get;          /* 0.9.2: Download Missing Covers */
    {   /* 1.1: the real clock, different on every start (the shuffle) */
        u64 sec = 0, nsec = 0;
        sysGetCurrentTime(&sec, &nsec);
        host.seed = (uint32_t)(sec * 2654435761u) ^ (uint32_t)nsec ^ (uint32_t)((u64)sec >> 32);
        if (!host.seed) host.seed = 1;
    }
#ifdef SF_HAVE_TLS
    log_printf("network: HTTPS available (mbedTLS)");
#else
    log_printf("network: plain HTTP only (this build has no mbedTLS)");
#endif
    /* Movie-only native NTFS discovery. Other platforms remain untouched. */
    rc=sf_bdntfs_start();
    sf_usb_diag_snapshot("POST_NATIVE_NTFS",NULL,NULL,usb_media_mask());
    log_printf("bdiso: NTFS volumes discovered=%d",rc);
    cover_diag("NTFS","volumenes_detectados=%d; si es 0, la app no puede leer JPG NTFS",rc);
    (void)sf_cobra_diag_startup("BEFORE_APP_INIT");
    app_init(&g_app, &g_lib, &host);
    (void)sf_cobra_diag_startup("READY_FOR_GAME_SELECTION");

    prev = now_seconds();
    while (!g_app.quit) {
        double t = now_seconds(), tw0, tw1, td1;
        float dt = (float)(t - prev);
        uint32_t held;
        prev = t;
        if (dt < 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;          /* slow frame: keep time moving, avoid jumps */
        {   /* 0.8.7: whole TV refreshes. The loop's clock wobbles by a few ms
             * while every frame is shown for exactly one refresh, so moving
             * by the measured time made fast scrolling step unevenly (doubled
             * cases on the TV). */
            const float T = 1.0f / 59.94f;
            int n = (int)(dt / T + 0.5f);
            if (n < 1) n = 1;
            dt = (float)n * T;
        }

        held = ps3_pad_read();
        if (tiny3d_MenuActive()) held = 0;          /* XMB overlay open: ignore pads */
        if (g_sys_exit) app_request_exit(&g_app, "sysutil");
        app_update(&g_app, held, dt);

        /* V15.44 shared audio handoff: PS1/PS2/PS3/BDISO still perform their
         * original mounts and webMAN requests unchanged. If mounting fails,
         * g_app.launched stays false and sound remains available in the UI.
         * Once an accepted mount queues a return to XMB / autoplay, release
         * our audio port before the fade and before XMB can run the title.
         * Keep normal Quit and power-off flows unchanged. */
        if (g_app.launched && !g_audio_released_for_launch) {
            log_printf("audio: DIAG_DISC_HANDOFF_SELECTED platform=%d audio_test=NO_PORT_INIT", (int)g_app.tab);
            log_printf("audio: DIAG_DISC_PHASE=PRE_RELEASE ui_port_state=%d", ps3_audio_diag_state());
            log_printf("audio: HANDOFF_DISC begin platform=%d", (int)g_app.tab);
            ps3_audio_stop();
            g_audio_released_for_launch = 1;
            log_printf("audio: DIAG_DISC_PHASE=AFTER_RELEASE ui_port_state=%d", ps3_audio_diag_state());
            log_printf("audio: DIAG_DISC_NEXT_STEP=RETURN_TO_XMB_GAME_AUDIO_NOT_ACCESSIBLE");
            log_printf("audio: HANDOFF_DISC audio released before XMB/game transition");
        }

        /* Tiny3D reuses ONE vertex buffer every frame and its Flip does not
         * wait for the GPU (tiny3d_WaitRSX is only a comment in tiny3d.c and
         * is not in the library). Without a wait we overwrite vertices the
         * RSX is still drawing: flashes, torn/stretched triangles, ghosts. */
        tw0 = now_seconds();
        wait_gpu_frame_done();
        tw1 = now_seconds();
        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
        /* discard only fully transparent pixels; blending does the rest */
        tiny3d_AlphaTest(1, 0x00, TINY3D_ALPHA_FUNC_GREATER);
        tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
                         TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
                         TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
        tiny3d_Project2D();
        ps3_render_frame_begin();
        ui_draw(&g_app);
        td1 = now_seconds();
        tiny3d_Flip();
        frame_stats(dt, td1 - tw1, tw1 - tw0);
    }

    if (g_app.poweroff) log_printf("main loop finished, webMAN is powering the system off");
    if (g_app.launched) {
        log_printf("audio: DIAG_MAIN_EXIT=AFTER_GAME_HANDOFF_REQUEST ui_port_state=%d", ps3_audio_diag_state());
        log_printf("main loop finished, Cobra mounted \"%s\" for XMB", g_app.last_launch);
    } else {
        log_printf("audio: DIAG_MAIN_EXIT=NORMAL_UI_EXIT ui_port_state=%d", ps3_audio_diag_state());
    }
    log_printf("main loop finished, returning to XMB");
    return 0;   /* atexit handler saves, closes the log and releases pads */
}
