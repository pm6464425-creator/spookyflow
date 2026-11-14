/* test_covers.h - milestone 2 tests (title IDs, cover lookup, processing,
 * cache, worker). Included by test_core.c (shares CHECK and g_app). */
#include "coverart.h"
#include "ps1_name_match.h"
#include "discid.h"
#include "render.h"

extern int g_null_cover_defined[R_COVER_SLOTS];

/* ---- a tiny in-memory file system ---- */
#define MEMFS_MAX 64
typedef struct { char path[COVER_PATH_MAX]; unsigned char *data; uint32_t len; int used; } MemFile;
static MemFile g_mf[MEMFS_MAX];
static const char *g_md[32];   /* directories that exist */
static int g_nmd, g_decodes, g_reads;

static void memfs_reset(void)
{
    int i;
    for (i = 0; i < MEMFS_MAX; i++) { free(g_mf[i].data); memset(&g_mf[i], 0, sizeof(g_mf[i])); }
    g_nmd = 0; g_decodes = 0; g_reads = 0;
}

static MemFile *memfs_find(const char *path)
{
    int i;
    for (i = 0; i < MEMFS_MAX; i++) if (g_mf[i].used && strcmp(g_mf[i].path, path) == 0) return &g_mf[i];
    return NULL;
}

static MemFile *memfs_put(const char *path, const void *data, uint32_t len)
{
    MemFile *f = memfs_find(path);
    int i;
    for (i = 0; !f && i < MEMFS_MAX; i++) if (!g_mf[i].used) f = &g_mf[i];
    if (!f) return NULL;
    free(f->data);
    f->used = 1;
    sf_strlcpy(f->path, path, sizeof(f->path));
    f->data = (unsigned char *)malloc(len ? len : 1);
    if (len) memcpy(f->data, data, len);
    f->len = len;
    return f;
}

static void memfs_dir(const char *d) { if (g_nmd < 32) g_md[g_nmd++] = d; }

static int mem_read_at(void *ctx, const char *path, uint64_t off, void *buf, uint32_t len)
{
    MemFile *f = memfs_find(path);
    (void)ctx;
    g_reads++;
    if (!f) return -1;
    if (off >= f->len) return 0;
    if (len > f->len - off) len = (uint32_t)(f->len - off);
    memcpy(buf, f->data + off, len);
    return (int)len;
}

static int mem_is_file(void *ctx, const char *path) { (void)ctx; return memfs_find(path) != NULL; }

static int mem_is_dir(void *ctx, const char *path)
{
    int i;
    (void)ctx;
    for (i = 0; i < g_nmd; i++) if (strcmp(g_md[i], path) == 0) return 1;
    return 0;
}

/* "IMG w h r g b a" -> solid image; "IMG2 w h" -> left half red, right half blue */
static int mem_decode(void *ctx, const char *path, int *w, int *h, uint8_t **rgba)
{
    MemFile *f = memfs_find(path);
    char txt[64];
    int r = 0, g = 0, b = 0, a = 255, i, split = 0;
    (void)ctx;
    g_decodes++;
    if (!f || f->len >= sizeof(txt)) return -1;
    memcpy(txt, f->data, f->len);
    txt[f->len] = '\0';
    if (sscanf(txt, "IMG %d %d %d %d %d %d", w, h, &r, &g, &b, &a) == 6) split = 0;
    else if (sscanf(txt, "IMG2 %d %d", w, h) == 2) split = 1;
    else return -1;
    *rgba = (uint8_t *)malloc((size_t)*w * (size_t)*h * 4u);
    if (!*rgba) return -1;
    for (i = 0; i < *w * *h; i++) {
        uint8_t *p = *rgba + (size_t)i * 4u;
        if (split) { int left = (i % *w) < *w / 2; p[0] = left ? 255 : 0; p[1] = 0; p[2] = left ? 0 : 255; p[3] = 255; }
        else { p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b; p[3] = (uint8_t)a; }
    }
    return 0;
}

static int mem_write(void *ctx, const char *path, const void *a, uint32_t alen, const void *b, uint32_t blen)
{
    unsigned char *tmp = (unsigned char *)malloc(alen + blen + 1);
    MemFile *f;
    (void)ctx;
    if (!tmp) return -1;
    memcpy(tmp, a, alen);
    memcpy(tmp + alen, b, blen);
    f = memfs_put(path, tmp, alen + blen);
    free(tmp);
    return f ? 0 : -1;
}

static int mem_remove(void *ctx, const char *path)
{
    MemFile *f = memfs_find(path);
    (void)ctx;
    if (!f) return -1;
    free(f->data);
    memset(f, 0, sizeof(*f));
    return 0;
}

static int mem_mkdir(void *ctx, const char *path) { (void)ctx; (void)path; return 0; }

static int mem_list(void *ctx, const char *dir, FsEntry *out, int max)
{
    int i, n = 0;
    size_t dl = strlen(dir);
    (void)ctx;
    for (i = 0; i < MEMFS_MAX && n < max; i++) {
        const char *p = g_mf[i].path;
        if (!g_mf[i].used || strncmp(p, dir, dl) != 0 || p[dl] != '/' || strchr(p + dl + 1, '/')) continue;
        sf_strlcpy(out[n].name, p + dl + 1, FS_NAME_MAX);
        out[n].is_dir = 0;
        out[n].size = g_mf[i].len;
        n++;
    }
    return n;
}

static void mem_sleep(void *ctx, int ms) { (void)ctx; (void)ms; }

static void mem_io(CoverIO *io, const char *cache)
{
    memset(io, 0, sizeof(*io));
    io->read_at = mem_read_at;
    io->is_file = mem_is_file;
    io->is_dir = mem_is_dir;
    io->decode = mem_decode;
    io->write_file = mem_write;
    io->remove = mem_remove;
    io->mkdir = mem_mkdir;
    io->list = mem_list;
    io->sleep_ms = mem_sleep;
    io->cache_dir = cache;
    io->pixels = r_cover_pixels;
}

/* ---- disc image builders ---- */

static void put_le32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }

static int dir_record(unsigned char *p, const char *name, uint32_t lba, uint32_t len, int is_dir)
{
    int nl = name[0] == '\0' || name[0] == '\1' ? 1 : (int)strlen(name);   /* "." and ".." are one byte */
    int rl = 33 + nl + ((33 + nl) & 1);                                        /* records have even length */
    memset(p, 0, (size_t)rl);
    p[0] = (unsigned char)rl;
    put_le32(p + 2, lba);
    put_le32(p + 10, len);
    p[25] = is_dir ? 2 : 0;
    p[32] = (unsigned char)nl;
    memcpy(p + 33, name, (size_t)nl);
    return rl;
}

/* 20 sectors of 2048: PVD at 16, root at 18, SYSTEM.CNF at 19. raw = 2352-byte
 * mode-2 sectors (user data at +24). Returns malloc'd image, *len set. */
static unsigned char *make_disc(const char *cnf, const char *exe, int raw, uint32_t *len)
{
    uint32_t pitch = raw ? 2352u : 2048u, off = raw ? 24u : 0u, s;
    unsigned char sec[20][2048];
    unsigned char *img;
    int o = 0;
    memset(sec, 0, sizeof(sec));
    sec[16][0] = 1; memcpy(sec[16] + 1, "CD001", 5);
    dir_record(sec[16] + 156, "\0", 18, 2048, 1);
    o += dir_record(sec[18] + o, "\0", 18, 2048, 1);
    o += dir_record(sec[18] + o, "\1", 18, 2048, 1);
    if (exe) o += dir_record(sec[18] + o, exe, 0, 1000, 0);
    if (cnf) {
        o += dir_record(sec[18] + o, "SYSTEM.CNF;1", 19, (uint32_t)strlen(cnf), 0);
        memcpy(sec[19], cnf, strlen(cnf));
    }
    *len = 20u * pitch;
    img = (unsigned char *)calloc(1, *len);
    for (s = 0; s < 20; s++) memcpy(img + s * pitch + off, sec[s], 2048);
    return img;
}

static size_t make_sfo_str(unsigned char *b, const char *key, const char *val)
{
    size_t kl = strlen(key) + 1, vl = strlen(val) + 1, kts = 20 + 16, dts = kts + ((kl + 3) & ~(size_t)3);
    memset(b, 0, 256);
    b[1] = 'P'; b[2] = 'S'; b[3] = 'F'; b[4] = 1; b[5] = 1;
    put_le32(b + 8, (uint32_t)kts); put_le32(b + 12, (uint32_t)dts); put_le32(b + 16, 1);
    b[20] = 0; b[22] = 4; b[23] = 2;               /* utf8 */
    put_le32(b + 24, (uint32_t)vl); put_le32(b + 28, 16); put_le32(b + 32, 0);
    memcpy(b + kts, key, kl);
    memcpy(b + dts, val, vl);
    return dts + 16;
}

static void test_discid(void)
{
    char id[16];
    unsigned char *img, sfo[256];
    uint32_t len;
    MemFile *f;
    printf("title IDs from disc images\n");
    CHECK(discid_parse_system_cnf("BOOT2 = cdrom0:\\SLUS_209.14;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n", strlen("BOOT2 = cdrom0:\\SLUS_209.14;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n"), id, sizeof(id)));
    CHECK_STR(id, "SLUS20914");
    CHECK(discid_parse_system_cnf("BOOT = cdrom:\\SCUS_944.26;1\nTCB = 4\n", strlen("BOOT = cdrom:\\SCUS_944.26;1\nTCB = 4\n"), id, sizeof(id)));
    CHECK_STR(id, "SCUS94426");
    CHECK(discid_parse_system_cnf("BOOT=cdrom:SLES_123.45;1", strlen("BOOT=cdrom:SLES_123.45;1"), id, sizeof(id)));
    CHECK_STR(id, "SLES12345");
    CHECK(!discid_parse_system_cnf("BOOT = cdrom:\\MAIN.EXE;1\n", strlen("BOOT = cdrom:\\MAIN.EXE;1\n"), id, sizeof(id)) && id[0] == '\0');
    {   /* BOOT2 wins over a BOOT line (PS2 discs that also carry one) */
        const char *t = "BOOT = cdrom:\\SLPS_000.01;1\nBOOT2 = cdrom0:\\SLPM_650.02;1\n";
        CHECK(discid_parse_system_cnf(t, strlen(t), id, sizeof(id)));
        CHECK_STR(id, "SLPM65002");
    }
    {
        char out[256];
        const char *cue = "REM test\r\nFILE \"Crash Bandicoot (Track 1).bin\" BINARY\r\n  TRACK 01 MODE2/2352\r\n";
        CHECK(discid_cue_first_file("/dev_hdd0/PSXISO/Crash.cue", cue, strlen(cue), out, sizeof(out)));
        CHECK_STR(out, "/dev_hdd0/PSXISO/Crash Bandicoot (Track 1).bin");
        CHECK(discid_cue_first_file("/x/G.cue", "FILE G.bin BINARY\n", strlen("FILE G.bin BINARY\n"), out, sizeof(out)));
        CHECK_STR(out, "/x/G.bin");
        CHECK(!discid_cue_first_file("/x/G.cue", "TRACK 01\n", strlen("TRACK 01\n"), out, sizeof(out)));
    }
    memfs_reset();
    /* PS2 .iso, 2048-byte sectors */
    img = make_disc("BOOT2 = cdrom0:\\SLUS_209.14;1\r\nVER = 1.00\r\n", "SLUS_209.14;1", 0, &len);
    memfs_put("/dev_hdd0/PS2ISO/Game.iso", img, len); free(img);
    CHECK(discid_read(PLAT_PS2, "/dev_hdd0/PS2ISO/Game.iso", 0, mem_read_at, NULL, id, sizeof(id)));
    CHECK_STR(id, "SLUS20914");
    /* PS1 .cue -> raw 2352-byte .bin */
    img = make_disc("BOOT = cdrom:\\SCUS_944.26;1\r\n", NULL, 1, &len);
    memfs_put("/dev_hdd0/PSXISO/Crash.bin", img, len); free(img);
    memfs_put("/dev_hdd0/PSXISO/Crash.cue", "FILE \"Crash.bin\" BINARY\n", (uint32_t)strlen("FILE \"Crash.bin\" BINARY\n"));
    CHECK(discid_read(PLAT_PS1, "/dev_hdd0/PSXISO/Crash.cue", 0, mem_read_at, NULL, id, sizeof(id)));
    CHECK_STR(id, "SCUS94426");
    /* no SYSTEM.CNF: the executable's name is used */
    img = make_disc(NULL, "SLES_015.03;1", 1, &len);
    memfs_put("/dev_hdd0/PSXISO/Other.bin", img, len); free(img);
    CHECK(discid_read(PLAT_PS1, "/dev_hdd0/PSXISO/Other.bin", 0, mem_read_at, NULL, id, sizeof(id)));
    CHECK_STR(id, "SLES01503");
    /* not an ISO 9660 image */
    memfs_put("/dev_hdd0/PSXISO/Junk.bin", "hello", 5);
    CHECK(!discid_read(PLAT_PS1, "/dev_hdd0/PSXISO/Junk.bin", 0, mem_read_at, NULL, id, sizeof(id)) && id[0] == '\0');
    CHECK(!discid_read(PLAT_PS2, "/dev_hdd0/PS2ISO/Missing.iso", 0, mem_read_at, NULL, id, sizeof(id)));
    CHECK(!discid_read(PLAT_PS2, "/dev_hdd0/PS2ISO/Game.BIN.ENC", 0, mem_read_at, NULL, id, sizeof(id)));
    /* PS3 ISO: disc info sector */
    img = (unsigned char *)calloc(1, 0x1000);
    memcpy(img + 0x800, "PlayStation3\0\0\0\0", 16);
    memcpy(img + 0x810, "BLES-80608", 10);
    memfs_put("/dev_hdd0/PS3ISO/mm.iso", img, 0x1000); free(img);
    CHECK(discid_read(PLAT_PS3, "/dev_hdd0/PS3ISO/mm.iso", 0, mem_read_at, NULL, id, sizeof(id)));
    CHECK_STR(id, "BLES80608");
    /* PS3 folder: PARAM.SFO TITLE_ID */
    len = (uint32_t)make_sfo_str(sfo, "TITLE_ID", "BLUS30443");
    memfs_put("/dev_hdd0/GAMES/Demon/PS3_GAME/PARAM.SFO", sfo, len);
    CHECK(discid_read(PLAT_PS3, "/dev_hdd0/GAMES/Demon", 1, mem_read_at, NULL, id, sizeof(id)));
    CHECK_STR(id, "BLUS30443");
    {
        char s[32];
        CHECK(sfo_get_str(sfo, len, "TITLE_ID", s, sizeof(s)) == 0);
        CHECK_STR(s, "BLUS30443");
        CHECK(sfo_get_str(sfo, len, "NOPE", s, sizeof(s)) == -2);
        CHECK(sfo_get_str(sfo, len, "TITLE_ID", s, 4) == 0);        /* truncated, still terminated */
        CHECK_STR(s, "BLU");
    }
    f = memfs_find("/dev_hdd0/GAMES/Demon/PS3_GAME/PARAM.SFO");
    if (f) f->data[1] = 'X';
    CHECK(!discid_read(PLAT_PS3, "/dev_hdd0/GAMES/Demon", 1, mem_read_at, NULL, id, sizeof(id)));
    memfs_reset();
}

/* PS1-only regression: game serial fallback and the verified xlenore
 * cover collection. PS2/PS3 URLs and the finished BDISO path stay untouched. */
static void test_ps1_covers_serial_and_sources(void)
{
    char id[16],urls[COVER_DL_URLS][COVER_URL_MAX];
    unsigned char *disc;
    uint32_t len;
    int n;
    const char *multi_cue=
        "REM COMMENT Multi-file game\n"
        "FILE \"Audio Track.bin\" BINARY\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n"
        "FILE \"Data Track.bin\" BINARY\n"
        "  TRACK 02 MODE2/2352\n"
        "    INDEX 01 00:00:00\n";
    printf("PS1 covers: serial fallback, multi-file CUE and xlenore URLs\\n");
    memfs_reset();

    /* Explicit serials on formats not currently parsed as ISO9660. */
    CHECK(discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/Metal Gear Solid (USA) [SLUS-00594].chd",
                      0,mem_read_at,NULL,id,sizeof(id)));
    CHECK_STR(id,"SLUS00594");
    CHECK(discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/SCUS-94900 Crash Bandicoot.pbp",
                      0,mem_read_at,NULL,id,sizeof(id)));
    CHECK_STR(id,"SCUS94900");
    CHECK(discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/SLUS_005.94 Metal Gear Solid.cue",
                      0,mem_read_at,NULL,id,sizeof(id)));
    CHECK_STR(id,"SLUS00594");
    CHECK(discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/Game [SLPS-12345].bin",
                      0,mem_read_at,NULL,id,sizeof(id)));
    CHECK_STR(id,"SLPS12345");
    CHECK(!discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/SLUS-005940 Invalid.bin",
                       0,mem_read_at,NULL,id,sizeof(id)));
    CHECK(!discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/NotSCUS-94900.bin",
                       0,mem_read_at,NULL,id,sizeof(id)));
    CHECK(!discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/Unknown Game.bin",
                       0,mem_read_at,NULL,id,sizeof(id)));
    CHECK(!discid_read(PLAT_PS2,"/dev_hdd0/PS2ISO/SLUS-20914.chd",
                       0,mem_read_at,NULL,id,sizeof(id)));

    /* Older implementation incorrectly opened Audio Track.bin and could
     * never recover the real data-track game's serial. */
    disc=make_disc("BOOT = cdrom:\\SLUS_005.94;1\r\n",NULL,1,&len);
    memfs_put("/dev_hdd0/PSXISO/Audio Track.bin","audio",5);
    memfs_put("/dev_hdd0/PSXISO/Data Track.bin",disc,len);
    memfs_put("/dev_hdd0/PSXISO/Multitrack.cue",multi_cue,
              (uint32_t)strlen(multi_cue));
    free(disc);
    CHECK(discid_read(PLAT_PS1,"/dev_hdd0/PSXISO/Multitrack.cue",
                      0,mem_read_at,NULL,id,sizeof(id)));
    CHECK_STR(id,"SLUS00594");

    /* Source documented at github.com/xlenore/psx-covers.
     * Use the exact real serial, not fuzzy title or modified game's ID. */
    n=cover_dl_urls(PLAT_PS1,"SLUS00594",urls,COVER_DL_URLS);
    CHECK(n==4);
    CHECK_STR(urls[0],
        "https://raw.githubusercontent.com/xlenore/psx-covers/main/covers/default/SLUS-00594.jpg");
    CHECK_STR(urls[1],
        "https://rpdbs3.gerbilsoft.com/ps1/cover/SLUS/SLUS-00594.jpg");
    CHECK_STR(urls[2],
        "http://rpdb.gerbilsoft.com/ps1/cover/SLUS/SLUS-00594.jpg");
    CHECK_STR(urls[3],
        "http://gamecovers.free.fr/download.php?file=SLUS_005.94.jpg");
    CHECK(cover_dl_urls(PLAT_PS1,"NOT-A-SERIAL",urls,COVER_DL_URLS)==0);
    n=cover_dl_urls(PLAT_PS2,"SLUS20914",urls,COVER_DL_URLS);
    CHECK(n>0);
    CHECK_STR(urls[0],
        "http://gamecovers.free.fr/download.php?file=SLUS_209.14.jpg");
    n=cover_dl_urls(PLAT_PS3,"BLUS30443",urls,COVER_DL_URLS);
    CHECK(n>0);
    CHECK_STR(urls[0],
        "http://gamecovers.free.fr/download.php?file=BLUS30443.jpg");
    CHECK(cover_dl_urls(PLAT_BDISO,"SLUS00594",urls,COVER_DL_URLS)==0);
    memfs_reset();
}

/* PS1 real worker path: prior .missing 404 markers MUST NOT block a
 * newly available verified xlenore cover on startup, and we must not
 * fabricate a cover for a file with no PlayStation serial. */
static int g_ps1_http_calls,g_ps1_http_bad_url;
static int test_ps1_http(void *ctx,const char *url,uint8_t *buf,uint32_t cap,
                         uint32_t *len,char *loc,size_t loc_cap)
{
    static const char *expected=
        "https://raw.githubusercontent.com/xlenore/psx-covers/main/covers/default/SLUS-00594.jpg";
    (void)ctx;
    if(strcmp(url,expected)) { g_ps1_http_bad_url=1; return 404; }
    g_ps1_http_calls++;
    if(cap<96)return COVER_HTTP_BIG;
    memset(buf,0,96);
    buf[0]=0xff;buf[1]=0xd8;buf[2]=0xff; /* valid JPEG signature for downloader */
    *len=96;
    if(loc_cap)loc[0]=0;
    return 200;
}
static void test_ps1_cover_download_retry(void)
{
    CoverIO io;
    CoverDl d;
    CoverDlItem item;
    printf("PS1 auto cover: old 404 marker bypass + no-ID safety\\n");
    memfs_reset();
    mem_io(&io,"/dev_hdd0/tmp/spookyflow/covers");
    memfs_put(COVER_DL_DIR "/SLUS00594.missing","404",3);
    memset(&item,0,sizeof(item));
    item.plat=PLAT_PS1;
    sf_strlcpy(item.path,"/dev_hdd0/PSXISO/Metal Gear Solid [SLUS-00594].chd",
               sizeof(item.path));
    memset(&d,0,sizeof(d));
    d.item=&item;
    d.n=1;
    d.io=&io;
    d.http=test_ps1_http;
    d.auto_mode=1;
    g_ps1_http_calls=0;g_ps1_http_bad_url=0;
    cover_dl_run(&d);
    CHECK(d.finished);
    CHECK(!g_ps1_http_bad_url && g_ps1_http_calls==1);
    CHECK(d.n_got==1 && d.n_have==0 && d.n_noid==0);
    CHECK_STR(item.id,"SLUS00594");
    CHECK(memfs_find(COVER_DL_DIR "/SLUS00594.jpg")!=NULL);
    CHECK(memfs_find(COVER_DL_DIR "/SLUS00594.missing")==NULL);
    memset(&item,0,sizeof(item));
    item.plat=PLAT_PS1;
    sf_strlcpy(item.path,"/dev_hdd0/PSXISO/Unknown Game.bin",sizeof(item.path));
    memset(&d,0,sizeof(d));
    d.item=&item;
    d.n=1;
    d.io=&io;
    d.http=test_ps1_http;
    d.auto_mode=1;
    cover_dl_run(&d);
    /* No serial: V15.9 tries only the exact game filename on Libretro.
     * A 404 never fabricates a cover and does not write a blank-ID marker. */
    CHECK(d.finished && d.n_missing==1 && d.n_got==0);
    CHECK(g_ps1_http_calls==1); /* no guessed serial; only an exact title request */
    CHECK(memfs_find(COVER_DL_DIR "/Unknown Game.png")==NULL);
    CHECK(memfs_find(COVER_DL_DIR "/.missing")==NULL);
    memfs_reset();
}

/* PS1 Redump CUE/BIN sets must appear as ONE cover per game.
 * Real dumps have FILE names different from CUE names, often 20+ tracks.
 * Earlier versions treated differently named track BINs as independent games,
 * all with no serial/cover. Reading CUE FILE directives is authoritative. */
static void test_ps1_cue_bin_track_grouping(void)
{
    Scanner sc;
    ScanRoot root[1]={{PLAT_PS1,"/dev_hdd0/PSXISO",SRC_HDD_ISO_DIR}};
    const char *cue=
        "REM GENRE GAME\r\n"
        "FILE \"RE2 Track 01.bin\" BINARY\r\n"
        "  TRACK 01 MODE2/2352\r\n"
        "FILE \"RE2 Track 02.bin\" BINARY\r\n"
        "  TRACK 02 AUDIO\r\n"
        "FILE \"RE2 Track 03.bin\" BINARY\r\n"
        "  TRACK 03 AUDIO\r\n";
    const char *cue2=
        "FILE \"GT Dump Track 01.bin\" BINARY\n"
        "  TRACK 01 MODE2/2352\n";
    char names[PS1_CUE_REF_MAX][PS1_CUE_REF_NAME_MAX];
    int i,c1=0,c2=0,solo=0,n;
    printf("PS1 CUE BIN: authoritative multi-track grouping and no duplicate covers\\n");
    n=discid_cue_files(cue,strlen(cue),names,PS1_CUE_REF_MAX);
    CHECK(n==3);
    CHECK_STR(names[0],"RE2 Track 01.bin");
    CHECK_STR(names[1],"RE2 Track 02.bin");
    CHECK_STR(names[2],"RE2 Track 03.bin");
    CHECK(discid_cue_files("FILE \"DIR\\\\Game (Track 01).bin\" BINARY\n",
                           strlen("FILE \"DIR\\\\Game (Track 01).bin\" BINARY\n"),
                           names,PS1_CUE_REF_MAX)==1);
    CHECK_STR(names[0],"Game (Track 01).bin");
    memfs_reset();
    memfs_put("/dev_hdd0/PSXISO/Resident Evil 2 (USA).cue",cue,(uint32_t)strlen(cue));
    memfs_put("/dev_hdd0/PSXISO/RE2 Track 01.bin","one",3);
    memfs_put("/dev_hdd0/PSXISO/RE2 Track 02.bin","audio",5);
    memfs_put("/dev_hdd0/PSXISO/RE2 Track 03.bin","more",4);
    memfs_put("/dev_hdd0/PSXISO/Gran Turismo (USA).cue",cue2,(uint32_t)strlen(cue2));
    memfs_put("/dev_hdd0/PSXISO/GT Dump Track 01.bin","original",8);
    memfs_put("/dev_hdd0/PSXISO/Unrelated.bin","standalone",10);
    lib_clear(&g_lib);
    scanner_begin(&sc,root,1);
    scanner_set_reader(&sc,mem_read_at,NULL);
    while(!scanner_step(&sc,&g_lib,mem_list,NULL)){}
    CHECK(sc.done);
    CHECK(g_lib.tab[PLAT_PS1].count==3);
    CHECK(sc.found[PLAT_PS1]==3);
    for(i=0;i<g_lib.tab[PLAT_PS1].count;i++){
        const Game *g=&g_lib.tab[PLAT_PS1].games[i];
        const char *name=lib_title(&g_lib,g);
        if(strcmp(name,"Resident Evil 2")==0){
            c1++;
            CHECK(g->size==(uint64_t)strlen(cue)+3+5+4);
            CHECK(strstr(lib_path(&g_lib,g),".cue")!=NULL);
        }else if(strcmp(name,"Gran Turismo")==0){
            c2++;
            CHECK(g->size==(uint64_t)strlen(cue2)+8);
        }else if(strcmp(name,"Unrelated")==0){
            solo++;
        }
        CHECK(strstr(name,"Track")==NULL);
    }
    CHECK(c1==1 && c2==1 && solo==1);
    memfs_reset();
}

/* V15.11 regressions for the actual two remaining user games.
 * Golden Leon is a separate Russian ROM hack with its own LaunchBox
 * packaging. Do NOT use the retail Tekken 3 cover just because its
 * internal boot title ID is the same. The correct selected cover must
 * save under the exact user's CUE filename for lookup/rendering. */
static int g_ps1_special_http_hits,g_ps1_special_http_wrong;
static int ps1_remaining_covers_mock_http(void *ctx,const char *url,
                                           uint8_t *buf,uint32_t cap,
                                           uint32_t *len,char *loc,size_t loc_cap)
{
    static const char *const golden=
        "https://images.launchbox-app.com/817f6c51-f659-4f8f-b0f2-40a486b39b60.png";
    static const char *const ace=
        "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/Ace%20Combat%203%20-%20Electrosphere%20(USA).png";
    (void)ctx;
    if(loc_cap)loc[0]=0;
    *len=0;
    if(strcmp(url,golden)!=0&&strcmp(url,ace)!=0){
        g_ps1_special_http_wrong++;
        return 404;
    }
    if(cap<120)return COVER_HTTP_BIG;
    memset(buf,0,120);
    memcpy(buf,"\x89PNG\r\n\x1a\n",8);
    *len=120;
    g_ps1_special_http_hits++;
    return 200;
}
static void test_ps1_ace_and_golden_leon_cover_download(void)
{
    CoverIO io;
    CoverDl d;
    CoverDlItem item;
    const char *index=
        "Ace Combat 3 - Electrosphere (USA).png\n"
        "Ace Combat 3 - Electrosphere (Europe) (En,Fr,De,Es,It).png\n"
        "Tekken 3 (USA).png\n";
    printf("PS1 Ace Combat 3 and Golden Leon: real official/hack cover lookup\n");
    memfs_reset();
    mem_io(&io,"/cache");
    memfs_put(PS1_BOXART_INDEX,index,(uint32_t)strlen(index));
    /* Retail Tekken 3 art preexists; it is NOT Golden Leon's art. */
    memfs_put(COVER_DL_DIR "/SLUS00402.jpg","IMG 96 96 0 0 0 255",18);
    memset(&item,0,sizeof(item));
    item.plat=PLAT_PS1;
    sf_strlcpy(item.path,"/dev_hdd0/PSXISO/Tekken 3 OT Golden Leon (MOD).cue",
               sizeof(item.path));
    sf_strlcpy(item.id,"SLUS00402",sizeof(item.id));
    memset(&d,0,sizeof(d));
    d.item=&item;d.n=1;d.io=&io;
    d.http=ps1_remaining_covers_mock_http;
    d.auto_mode=1;
    g_ps1_special_http_hits=g_ps1_special_http_wrong=0;
    cover_dl_run(&d);
    CHECK(d.finished && d.n_got==1 && d.n_have==0);
    CHECK(g_ps1_special_http_hits==1 && g_ps1_special_http_wrong==0);
    CHECK(memfs_find(COVER_DL_DIR "/Tekken 3 OT Golden Leon (MOD).png")!=NULL);
    CHECK(memfs_find(COVER_DL_DIR "/SLUS00402.jpg")!=NULL); /* unchanged */
    CHECK(cover_has_art(&io,PLAT_PS1,item.path,0,0,""));
    /* Do not replace the special edition with unrelated standard artwork. */
    memset(&item,0,sizeof(item));
    item.plat=PLAT_PS1;
    sf_strlcpy(item.path,"/dev_hdd0/PSXISO/Ace Combat 3.cue",
               sizeof(item.path));
    memset(&d,0,sizeof(d));
    d.item=&item;d.n=1;d.io=&io;
    d.http=ps1_remaining_covers_mock_http;
    d.auto_mode=1;
    cover_dl_run(&d);
    CHECK(d.finished && d.n_got==1 && d.n_noid==0);
    CHECK(g_ps1_special_http_hits==2 && g_ps1_special_http_wrong==0);
    CHECK(memfs_find(COVER_DL_DIR "/Ace Combat 3.png")!=NULL);
    CHECK(cover_has_art(&io,PLAT_PS1,item.path,0,0,""));
    /* A bogus similarly named fighter must not get Golden Leon cover. */
    memset(&item,0,sizeof(item));
    item.plat=PLAT_PS1;
    sf_strlcpy(item.path,"/dev_hdd0/PSXISO/Tekken 3 Fan Tournament.cue",
               sizeof(item.path));
    memset(&d,0,sizeof(d));d.item=&item;d.n=1;d.io=&io;
    d.http=ps1_remaining_covers_mock_http;d.auto_mode=1;
    cover_dl_run(&d);
    CHECK(d.finished && d.n_got==0);
    CHECK(g_ps1_special_http_hits==2);
    memfs_reset();
}

/* PS1 V15.10: user reports "Disney Tarzan Latino.cue" with
 * "Disney Tarzan Latino.bin", while real Libretro archive contains
 * "Disney Tarzan (Spain).png". This must download and SAVE the cover
 * under the user's *original* .cue name without touching source media.
 */
static int g_tarzan_covers_requested,g_tarzan_unexpected;
static int tarzan_mock_http(void *ctx,const char *url,uint8_t *buf,uint32_t cap,
                            uint32_t *len,char *loc,size_t loc_cap)
{
    const char *real=
       "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/Disney%20Tarzan%20(Spain).png";
    (void)ctx;
    if(loc_cap)loc[0]=0;
    *len=0;
    if(strcmp(url,real)!=0){g_tarzan_unexpected++;return 404;}
    if(cap<100)return COVER_HTTP_BIG;
    memset(buf,0,100);
    memcpy(buf,"\x89PNG\r\n\x1a\n",8);
    *len=100;
    g_tarzan_covers_requested++;
    return 200;
}
static void test_ps1_tarzan_latino_catalog_download(void)
{
    CoverIO io;
    CoverDl d;
    CoverDlItem it;
    const char *index=
       "Disney's Tarzan (USA).png\n"
       "Disney Tarzan (Spain).png\n"
       "Disney's Tarzan (Europe).png\n"
       "Gran Turismo (USA).png\n";
    printf("PS1 catalog download: Disney Tarzan Latino -> Disney Tarzan Spain\n");
    memfs_reset();
    mem_io(&io,"/cache");
    memfs_put(PS1_BOXART_INDEX,index,(uint32_t)strlen(index));
    memset(&it,0,sizeof(it));
    it.plat=PLAT_PS1;
    sf_strlcpy(it.path,"/dev_hdd0/PSXISO/Disney Tarzan Latino.cue",
               sizeof(it.path));
    memset(&d,0,sizeof(d));d.item=&it;d.n=1;d.io=&io;
    d.http=tarzan_mock_http;d.auto_mode=1;
    g_tarzan_covers_requested=g_tarzan_unexpected=0;
    {
        char matched[256]="";
        char url[512]="";
        int yes=ps1_name_catalog_match(it.path,index,strlen(index),
                                       matched,sizeof(matched));
        if(yes)ps1_name_catalog_url(matched,url,sizeof(url));
        printf("PS1 Tarzan catalog direct match=%d selected=%s url=%s\n",
               yes,matched,url);
    }
    cover_dl_run(&d);
    printf("PS1 Tarzan worker downloaded=%d missing=%d noid=%d error=%d "
           "HTTP matches=%d other=%d id=%s\n",
           d.n_got,d.n_missing,d.n_noid,d.n_error,
           g_tarzan_covers_requested,g_tarzan_unexpected,it.id);
    CHECK(d.finished && d.n_got==1);
    CHECK(g_tarzan_covers_requested==1 && g_tarzan_unexpected==0);
    CHECK(memfs_find(COVER_DL_DIR "/Disney Tarzan Latino.png")!=NULL);
    CHECK(cover_has_art(&io,PLAT_PS1,it.path,0,0,""));
    CHECK(memfs_find("/dev_hdd0/PSXISO/Disney Tarzan Latino.bin")==NULL);
    memfs_reset();
}

/* A PS1 CUE without a SYSTEM.CNF serial can still use the exact filename
 * and region. Libretro's official PlayStation Named_Boxarts contains e.g.
 * "Crash Bandicoot (USA).png". This must NOT use fuzzy guesses or alter
 * the original .cue/.bin pair.
 */
static int g_ps1_libretro_hits,g_ps1_libretro_serial_lookups;
static int test_ps1_libretro_http(void *ctx,const char *url,uint8_t *buf,
                                  uint32_t cap,uint32_t *len,
                                  char *loc,size_t loc_cap)
{
    static const char *expected=
        "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/Crash%20Bandicoot%20(USA).png";
    (void)ctx;
    if(loc_cap)loc[0]=0;
    *len=0;
    if(strcmp(url,expected)!=0){g_ps1_libretro_serial_lookups++;return 404;}
    if(cap<96)return COVER_HTTP_BIG;
    memset(buf,0,96);
    memcpy(buf,"\x89PNG\r\n\x1a\n",8);
    *len=96;
    g_ps1_libretro_hits++;
    return 200;
}
static void test_ps1_libretro_filename_download(void)
{
    CoverIO io;
    CoverDl d;
    CoverDlItem item;
    char url[COVER_URL_MAX];
    int n;
    printf("PS1 CUE no serial: exact Libretro filename/regional cover recovery\\n");
    CHECK(cover_dl_ps1_title_url("/dev_hdd0/PSXISO/Crash Bandicoot (USA).cue",
                                 url,sizeof(url)));
    CHECK_STR(url,
      "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/Crash%20Bandicoot%20(USA).png");
    CHECK(cover_dl_ps1_title_url("/dev_hdd0/PSXISO/Resident Evil 2 (USA) (Disc 1).cue",
                                 url,sizeof(url)));
    CHECK(strstr(url,"Resident%20Evil%202%20(USA)%20(Disc%201).png")!=NULL);
    CHECK(!cover_dl_ps1_title_url("/dev_hdd0/PSXISO/Football [MOD].cue",
                                  url,sizeof(url)));
    CHECK(!cover_dl_ps1_title_url("/dev_hdd0/PSXISO/x.cue",
                                  url,sizeof(url)));

    memfs_reset();
    mem_io(&io,"/cache");
    memset(&item,0,sizeof(item));
    item.plat=PLAT_PS1;
    sf_strlcpy(item.path,"/dev_hdd0/PSXISO/Crash Bandicoot (USA).cue",
               sizeof(item.path));
    memset(&d,0,sizeof(d));
    d.item=&item;d.n=1;d.io=&io;
    d.http=test_ps1_libretro_http;
    d.auto_mode=1;
    g_ps1_libretro_hits=g_ps1_libretro_serial_lookups=0;
    cover_dl_run(&d);
    CHECK(d.finished);
    CHECK(d.n_got==1 && d.n_noid==0);
    CHECK(g_ps1_libretro_hits==1 && g_ps1_libretro_serial_lookups==0);
    CHECK(memfs_find(COVER_DL_DIR "/Crash Bandicoot (USA).png")!=NULL);
    CHECK(cover_has_art(&io,PLAT_PS1,item.path,0,0,""));

    /* CUE has a known SLUS serial, but the serial-indexed sources 404.
     * Exact title source must still rescue the user-visible cover. */
    n=0;
    if(mem_remove(NULL,COVER_DL_DIR "/Crash Bandicoot (USA).png")==0)n++;
    CHECK(n==1);
    memset(&item,0,sizeof(item));
    item.plat=PLAT_PS1;
    sf_strlcpy(item.path,"/dev_hdd0/PSXISO/Crash Bandicoot (USA).cue",
               sizeof(item.path));
    sf_strlcpy(item.id,"SCUS94900",sizeof(item.id));
    memset(&d,0,sizeof(d));d.item=&item;d.n=1;d.io=&io;
    d.http=test_ps1_libretro_http;
    d.auto_mode=1;
    cover_dl_run(&d);
    CHECK(d.finished && d.n_got==1 && g_ps1_libretro_hits==2);
    CHECK(g_ps1_libretro_serial_lookups==4);
    CHECK(memfs_find(COVER_DL_DIR "/Crash Bandicoot (USA).png")!=NULL);
    CHECK(memfs_find(COVER_DL_DIR "/.missing")==NULL);
    memfs_reset();
}

static int cand_has(const CoverCand *c, int n, const char *base)
{
    int i;
    for (i = 0; i < n; i++) if (strcmp(c[i].base, base) == 0) return i;
    return -1;
}

static void test_cover_lookup(void)
{
    CoverCand c[COVER_MAX_CANDS];
    unsigned char ok[16];
    int n;
    char stem[COVER_PATH_MAX], stem2[COVER_PATH_MAX];
    printf("cover lookup: exclusive managed directory\n");
    CHECK(cover_folder_count() == 1);
    CHECK_STR(cover_folder(0), COVER_DL_DIR);

    n = cover_candidates(PLAT_PS3, "/dev_hdd0/PS3ISO/Gran Turismo6.ISO", 0, "BCES01893", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 2);
    CHECK_STR(c[0].base, COVER_DL_DIR "/Gran Turismo6");
    CHECK_STR(c[1].base, COVER_DL_DIR "/BCES01893");
    CHECK(cand_has(c, n, "/dev_hdd0/PS3ISO/Gran Turismo6") < 0); /* ignore multiMAN sidecar */
    CHECK(cand_has(c, n, "/dev_hdd0/game/BLES80608/USRDIR/covers/BCES01893") < 0);

    n = cover_candidates(PLAT_PS3, "/dev_hdd0/PS3ISO/TEKKEN TAG TOURNAMENT 2.ISO", 0, "BLES01702", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 2 && strcmp(c[0].base, COVER_DL_DIR "/TEKKEN TAG TOURNAMENT 2") == 0);
    CHECK_STR(c[1].base, COVER_DL_DIR "/BLES01702");

    n = cover_candidates(PLAT_PS3, "/dev_hdd0/PS3ISO/Assassins Creed.ISO", 0, "BLUS30089", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 2 && strcmp(c[0].base, COVER_DL_DIR "/Assassins Creed") == 0);
    CHECK_STR(c[1].base, COVER_DL_DIR "/BLUS30089");

    n = cover_candidates(PLAT_PS3, "/dev_hdd0/PS3ISO/EA Sports FC 27.iso", 0, "BLES01676", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 1);  /* mod cannot reuse the base FIFA's official cover */
    CHECK_STR(c[0].base, COVER_DL_DIR "/EA Sports FC 27");

    n = cover_candidates(PLAT_PS3, "/dev_hdd0/GAMES/Demon", 1, "BLUS30443", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 2);
    CHECK_STR(c[0].base, COVER_DL_DIR "/Demon");
    CHECK_STR(c[1].base, COVER_DL_DIR "/BLUS30443");

    n = cover_candidates(PLAT_PS2, "/dev_hdd0/PS2ISO/Game.dec.iso", 0, "SLUS20914", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 2);
    CHECK_STR(c[0].base, COVER_DL_DIR "/Game");
    CHECK_STR(c[1].base, COVER_DL_DIR "/SLUS20914");

    n = cover_candidates(PLAT_PS1, "/dev_hdd0/PSXISO/Crash.cue", 0, "SCUS94426", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 2 && strcmp(c[1].base, COVER_DL_DIR "/SCUS94426") == 0);
    memset(ok, 0, sizeof(ok));
    n = cover_candidates(PLAT_PS3, "/dev_hdd0/PS3ISO/Big.iso.0", 0, "BLES80608", ok, c, COVER_MAX_CANDS);
    CHECK(n == 2 && strcmp(c[0].base, COVER_DL_DIR "/Big") == 0);
    n = cover_candidates(PLAT_PS3, "/dev_hdd0/PS3ISO/Game.iso", 0, "", NULL, c, COVER_MAX_CANDS);
    CHECK(n == 1 && strcmp(c[0].base, COVER_DL_DIR "/Game") == 0);
    CHECK(cover_candidates(PLAT_PS1, "", 0, "X", NULL, c, COVER_MAX_CANDS) == 0);
    CHECK(cover_candidates(PLAT_PS2, "/a/b.iso", 0, "SLUS20914", NULL, c, 1) == 1);

    cover_cache_stem("/c", PLAT_PS2, "/dev_hdd0/PS2ISO/A.iso", 100, stem, sizeof(stem));
    cover_cache_stem("/c", PLAT_PS2, "/dev_hdd0/PS2ISO/A.iso", 100, stem2, sizeof(stem2));
    CHECK_STR(stem, stem2);
    CHECK(strncmp(stem, "/c/ps2/", 7) == 0 && strlen(stem) == 7 + 16);
    cover_cache_stem("/c", PLAT_PS2, "/dev_hdd0/PS2ISO/A.iso", 101, stem2, sizeof(stem2));
    CHECK(strcmp(stem, stem2) != 0);
    cover_cache_stem("/c", PLAT_PS2, "/dev_hdd0/PS2ISO/B.iso", 100, stem2, sizeof(stem2));
    CHECK(strcmp(stem, stem2) != 0);
}

static void channel_mean(const uint16_t *px, int n, double m[3])
{
    int i;
    m[0] = m[1] = m[2] = 0.0;
    for (i = 0; i < n; i++) {
        m[0] += (px[i] >> 10) & 31; m[1] += (px[i] >> 5) & 31; m[2] += px[i] & 31;
    }
    m[0] /= n; m[1] /= n; m[2] /= n;
}

static void test_cover_process(void)
{
    static uint16_t px[R_COVER_SLOT_BYTES / 2];
    uint8_t *img;
    int i, tw, th, all_opaque = 1;
    double m[3];
    printf("cover processing\n");
    cover_tex_size(PLAT_PS1, &tw, &th);
    CHECK(tw == 384 && th == 384);
    cover_tex_size(PLAT_PS2, &tw, &th);
    CHECK(tw == 320 && th == 448);
    for (i = 0; i < PLAT_COUNT; i++) {
        cover_tex_size(i, &tw, &th);
        CHECK(tw <= R_COVER_MAX_W && th <= R_COVER_MAX_H && (tw * 2) % 64 == 0);   /* RSX pitch */
    }
    CHECK_NEAR(cover_art_aspect(PLAT_PS1), 1.0, 0.001);
    CHECK_NEAR(cover_art_aspect(PLAT_PS2), 0.70, 0.002);
    CHECK_NEAR(cover_art_aspect(PLAT_PS3), 0.86, 0.002);

    /* solid colors survive exactly (white -> 31, black -> 0) */
    img = (uint8_t *)malloc(500 * 700 * 4);
    for (i = 0; i < 500 * 700; i++) { img[i * 4] = 255; img[i * 4 + 1] = 0; img[i * 4 + 2] = 255; img[i * 4 + 3] = 255; }
    cover_process(img, 500, 700, cover_art_aspect(PLAT_PS2), px, tw, th);
    for (i = 0; i < tw * th; i++) if (px[i] != (0x8000 | (31 << 10) | 31)) { all_opaque = 0; break; }
    CHECK(all_opaque);
    /* mid gray: dithered, but averages to the true value */
    for (i = 0; i < 500 * 700; i++) { img[i * 4] = img[i * 4 + 1] = img[i * 4 + 2] = 128; }
    cover_process(img, 500, 700, cover_art_aspect(PLAT_PS2), px, tw, th);
    channel_mean(px, tw * th, m);
    CHECK_NEAR(m[0], 128.0 * 31.0 / 255.0, 0.08);
    CHECK_NEAR(m[1], 128.0 * 31.0 / 255.0, 0.08);
    for (i = 1; i < tw * th && px[i] == px[0]; i++) { }
    CHECK(i < tw * th);                                    /* really dithered */
    /* transparent pixels show the dark backdrop, never garbage */
    for (i = 0; i < 500 * 700; i++) img[i * 4 + 3] = 0;
    cover_process(img, 500, 700, cover_art_aspect(PLAT_PS2), px, tw, th);
    channel_mean(px, tw * th, m);
    CHECK(m[0] < 4.0 && m[2] < 4.0 && (px[0] & 0x8000));
    free(img);

    /* nothing is ever cut off: a slightly different shape is stretched */
    img = (uint8_t *)malloc(100 * 100 * 4);
    for (i = 0; i < 100 * 100; i++) {
        int x = i % 100;
        img[i * 4] = (uint8_t)(x < 10 || x >= 90 ? 255 : 0); img[i * 4 + 1] = 0; img[i * 4 + 2] = (uint8_t)(x < 10 || x >= 90 ? 0 : 255); img[i * 4 + 3] = 255;
    }
    cover_process(img, 100, 100, 0.84f, px, 384, 448);     /* 1.0 -> 0.84: stretched */
    CHECK(((px[0] >> 10) & 31) == 31 && (px[0] & 31) == 0);             /* left border kept */
    CHECK(((px[383] >> 10) & 31) == 31 && (px[383] & 31) == 0);         /* right border kept */
    cover_process(img, 100, 100, 0.78f, px, 320, 448);     /* 1.0 -> 0.78: too different, letterboxed */
    CHECK(((px[0] >> 10) & 31) < 5);                                    /* backdrop above */
    CHECK(((px[224 * 320 + 2] >> 10) & 31) == 31);                      /* border still there */
    free(img);
    /* a very wide banner is shown whole: backdrop above and below */
    img = (uint8_t *)malloc(400 * 100 * 4);
    for (i = 0; i < 400 * 100; i++) { img[i * 4] = 255; img[i * 4 + 1] = 255; img[i * 4 + 2] = 255; img[i * 4 + 3] = 255; }
    cover_process(img, 400, 100, 0.70f, px, 320, 448);
    CHECK(((px[0] >> 10) & 31) < 5);                       /* top row: backdrop */
    CHECK(((px[224 * 320 + 160] >> 10) & 31) == 31);       /* middle: the image */
    free(img);
    /* tiny source enlarges without crashing */
    img = (uint8_t *)malloc(2 * 3 * 4);
    for (i = 0; i < 6; i++) { img[i * 4] = img[i * 4 + 1] = img[i * 4 + 2] = 200; img[i * 4 + 3] = 255; }
    cover_process(img, 2, 3, 0.70f, px, 320, 448);
    CHECK((px[0] & 0x8000) && ((px[0] >> 10) & 31) >= 23);
    free(img);
}

static void test_cover_loader(void)
{
    static CoverLoader L;
    CoverIO io;
    CoverCache cc;
    uint32_t len;
    unsigned char *img;
    int i, k, slot, tw, th, decodes, steps;
    char stem[COVER_PATH_MAX], path[COVER_PATH_MAX + 8];
    printf("cover loader\n");
    memfs_reset();
    memfs_dir(COVER_DL_DIR);
    img = make_disc("BOOT2 = cdrom0:\\SLUS_209.14;1\r\n", NULL, 0, &len);
    memfs_put("/dev_hdd0/PS2ISO/Game.iso", img, len); free(img);
    memfs_put(COVER_DL_DIR "/SLUS20914.JPG", "IMG 50 70 255 0 0 255", 21);
    memfs_put("/dev_hdd0/PS2ISO/Other.iso", "junk", 4);
    memfs_put("/dev_hdd0/PS2ISO/Side.iso", "junk", 4);
    memfs_put("/dev_hdd0/PS2ISO/Side.png", "IMG 70 100 255 0 0 255", 22); /* bad external sidecar, must be ignored */
    memfs_put(COVER_DL_DIR "/Side.png", "IMG 70 100 0 0 255 255", 22);  /* managed cover wins */
    mem_io(&io, "/cache");
    cover_loader_init(&L, &io);
    cc_init(&cc, 8, NULL, NULL);
    cover_tex_size(PLAT_PS2, &tw, &th);

    /* found through the title ID read from the image */
    cc.slot[0].state = CC_LOADING;
    cover_request(&L, 0, PLAT_PS2, PLAT_PS2, 0, "/dev_hdd0/PS2ISO/Game.iso", 0, len, "");
    CHECK(cover_worker_step(&L) == 1);
    CHECK(L.probed && L.n_art == 1);
    memset(g_null_cover_defined, 0, sizeof(g_null_cover_defined));
    cover_poll(&L, &cc);
    CHECK(cc.slot[0].state == CC_READY && cc.slot[0].tex == TEX_COVER0);
    CHECK(g_null_cover_defined[0] == 1);
    CHECK(r_cover_pixels(0)[0] == (0x8000 | (31 << 10)));     /* red */
    cover_cache_stem("/cache", PLAT_PS2, "/dev_hdd0/PS2ISO/Game.iso", len, stem, sizeof(stem));
    snprintf(path, sizeof(path), "%s.cvr", stem);
    CHECK(memfs_find(path) && memfs_find(path)->len == 16u + (uint32_t)(tw * th * 2));
    CHECK(cover_worker_step(&L) == 0);                         /* nothing left */

    /* same-name image beside the game, and a missing cover */
    cc.slot[1].state = CC_LOADING;
    cc.slot[2].state = CC_LOADING;
    cover_request(&L, 1, PLAT_PS2, PLAT_PS2, 1, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    cover_request(&L, 2, PLAT_PS2, PLAT_PS2, 2, "/dev_hdd0/PS2ISO/Other.iso", 0, 4, "");
    while (cover_worker_step(&L)) { }
    cover_poll(&L, &cc);
    CHECK(cc.slot[1].state == CC_READY && (r_cover_pixels(1)[0] & 31) == 31);   /* blue */
    CHECK(cc.slot[2].state == CC_FAILED);
    cover_cache_stem("/cache", PLAT_PS2, "/dev_hdd0/PS2ISO/Other.iso", 4, stem, sizeof(stem));
    snprintf(path, sizeof(path), "%s.none", stem);
    CHECK(memfs_find(path) != NULL);                           /* searched only once */
    decodes = g_decodes;
    cc.slot[2].state = CC_LOADING;
    cover_request(&L, 2, PLAT_PS2, PLAT_PS2, 2, "/dev_hdd0/PS2ISO/Other.iso", 0, 4, "");
    g_reads = 0;
    cover_worker_step(&L);
    cover_poll(&L, &cc);
    CHECK(cc.slot[2].state == CC_FAILED && g_reads <= 2 && g_decodes == decodes);   /* cache header + marker */

    /* second run: straight from the cache, no decoding, even if the art is gone */
    mem_remove(NULL, COVER_DL_DIR "/SLUS20914.JPG");
    cover_loader_stop(&L);
    cover_loader_init(&L, &io);
    cc_init(&cc, 8, NULL, NULL);
    cc.slot[3].state = CC_LOADING;
    cover_request(&L, 3, PLAT_PS2, PLAT_PS2, 0, "/dev_hdd0/PS2ISO/Game.iso", 0, len, "");
    decodes = g_decodes;
    cover_worker_step(&L);
    cover_poll(&L, &cc);
    CHECK(cc.slot[3].state == CC_READY && L.n_cache == 1 && g_decodes == decodes);
    CHECK(r_cover_pixels(3)[0] == (0x8000 | (31 << 10)));
    CHECK(!L.probed);                                          /* cache hits never touch the cover folders */

    /* a slot re-requested before the worker got to it only delivers the newer game */
    cc.slot[4].state = CC_LOADING;
    cover_request(&L, 4, PLAT_PS2, PLAT_PS2, 5, "/dev_hdd0/PS2ISO/Other.iso", 0, 4, "");
    cover_request(&L, 4, PLAT_PS2, PLAT_PS2, 1, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    steps = 0;
    while (cover_worker_step(&L)) steps++;
    CHECK(steps == 1);
    cover_poll(&L, &cc);
    CHECK(cc.slot[4].state == CC_READY && (r_cover_pixels(4)[0] & 31) == 31);
    /* cancel: nothing is processed or marked */
    cc.slot[5].state = CC_LOADING;
    cover_request(&L, 5, PLAT_PS2, PLAT_PS2, 1, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    cover_cancel_all(&L);
    CHECK(cover_worker_step(&L) == 0);
    /* nearest to the selection first */
    for (i = 0; i < 3; i++) cc.slot[i].state = CC_LOADING;
    cover_request(&L, 0, PLAT_PS2, PLAT_PS2, 40, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    cover_request(&L, 1, PLAT_PS1, PLAT_PS1, 11, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    cover_request(&L, 2, PLAT_PS2, PLAT_PS2, 9, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    cover_set_focus(&L, PLAT_PS2, 10);
    cover_worker_step(&L);
    CHECK(L.job[2].done_gen == L.job[2].req_gen && L.job[0].done_gen != L.job[0].req_gen);
    cover_worker_step(&L);
    CHECK(L.job[0].done_gen == L.job[0].req_gen && L.job[1].done_gen != L.job[1].req_gen);   /* other tab last */
    cover_worker_step(&L);

    /* Explicit Rescan Library now drops both .none and .cvr so replaced
     * user JPG/PNG artwork appears; underlying originals are preserved. */
    for (i = 0, k = 0; i < MEMFS_MAX; i++) if (g_mf[i].used && strncmp(g_mf[i].path, "/cache/", 7) == 0 && strstr(g_mf[i].path, ".cvr")) k++;
    cover_wipe_cache(&L);
    CHECK(cover_worker_step(&L) == 1);
    snprintf(path, sizeof(path), "%s.none", stem);
    CHECK(memfs_find(path) == NULL);
    for (i = 0, slot = 0; i < MEMFS_MAX; i++) if (g_mf[i].used && strncmp(g_mf[i].path, "/cache/", 7) == 0) slot++;
    CHECK(k > 0 && slot == 0);                                  /* all caches invalidated */
    CHECK(memfs_find("/dev_hdd0/PS2ISO/Side.png") != NULL);    /* user files untouched */

    /* no disk cache configured: still works */
    mem_io(&io, NULL);
    cover_loader_stop(&L);
    cover_loader_init(&L, &io);
    cc.slot[6].state = CC_LOADING;
    cover_request(&L, 6, PLAT_PS2, PLAT_PS2, 1, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    cover_worker_step(&L);
    cover_poll(&L, &cc);
    CHECK(cc.slot[6].state == CC_READY);
    /* a decoder failure is a missing cover, not a crash */
    memfs_put(COVER_DL_DIR "/Side.png", "garbage", 7);
    cc.slot[7].state = CC_LOADING;
    cover_request(&L, 7, PLAT_PS2, PLAT_PS2, 1, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
    cover_worker_step(&L);
    cover_poll(&L, &cc);
    CHECK(cc.slot[7].state == CC_FAILED);
    /* an oversized PNG is refused from its header, never decoded */
    {
        unsigned char png[24] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R',
                                  0, 0, 0x0B, 0xB8, 0, 0, 0x0B, 0xB8 };   /* 3000 x 3000 */
        memfs_put(COVER_DL_DIR "/Side.png", png, 24);
        decodes = g_decodes;
        cc.slot[7].state = CC_LOADING;
        cover_request(&L, 7, PLAT_PS2, PLAT_PS2, 1, "/dev_hdd0/PS2ISO/Side.iso", 0, 4, "");
        cover_worker_step(&L);
        cover_poll(&L, &cc);
        CHECK(cc.slot[7].state == CC_FAILED && g_decodes == decodes);
    }
    cover_loader_stop(&L);
    memfs_reset();
}

static void test_cover_cache_gpu(void)
{
    CoverCache cc;
    int s, i, reused = 0;
    printf("cover cache gpu guard\n");
    cc_init(&cc, 4, NULL, NULL);
    cc_update(&cc, 0, 0, 100);                 /* loads 0..3 (4 per frame) */
    CHECK(cc_resident_count(&cc) == 4);
    s = cc_lookup(&cc, 0, 0);                  /* slot drawn this frame */
    CHECK(s >= 0);
    cc_update(&cc, 0, 50, 100);                /* jump away: everything is out of the window */
    CHECK(cc.slot[s].index == 0);              /* ...but the drawn slot is still in use by the GPU */
    for (i = 0; i < CC_GPU_FRAMES + 1; i++) cc_update(&cc, 0, 50, 100);
    CHECK(cc.slot[s].index != 0);              /* reused once the GPU is done with it */
    /* cc_mark: async completion and fade age */
    cc_init(&cc, 4, NULL, NULL);
    cc.slot[1].state = CC_LOADING;
    cc_mark(&cc, 1, TEX_COVER0 + 1);
    CHECK(cc.slot[1].state == CC_READY && cc.slot[1].tex == TEX_COVER0 + 1 && cc_ready_age(&cc, 1) == 0);
    cc_update(&cc, 0, 0, 1);
    cc_update(&cc, 0, 0, 1);
    CHECK(cc_ready_age(&cc, 1) == 2);
    cc.slot[2].state = CC_LOADING;
    cc_mark(&cc, 2, -1);
    CHECK(cc.slot[2].state == CC_FAILED && cc_ready_age(&cc, 2) == 0);
    cc_mark(&cc, 3, 5);                        /* not loading: ignored */
    CHECK(cc.slot[3].state != CC_READY || cc.slot[3].tex != 5);
    (void)reused;
}

/* The app hands real games to the loader, keeps placeholders for demo games
 * and drops pending work when the library changes. */
static void test_app_covers(void)
{
    static CoverLoader L;
    CoverIO io;
    AppHost host;
    int i, loading = 0, ready = 0;
    printf("app covers\n");
    memfs_reset();
    mem_io(&io, NULL);
    cover_loader_init(&L, &io);
    memset(&host, 0, sizeof(host));
    host.fs_list = fake_list;
    host.covers = &L;
    app_init(&g_app, &g_lib, &host);
    run_frames(0, LOAD_MIN_TIME + 1.0f);
    CHECK(g_app.mode == MODE_FLOW);
    for (i = 0; i < g_app.covers.budget; i++) if (g_app.covers.slot[i].state == CC_LOADING) loading++;
    CHECK(loading == g_lib.tab[0].count + g_lib.tab[1].count + g_lib.tab[2].count);   /* every tab's games queued */
    while (cover_worker_step(&L)) { }
    run_frames(0, 0.1f);
    for (i = 0, loading = 0; i < g_app.covers.budget; i++) {
        if (g_app.covers.slot[i].state == CC_LOADING) loading++;
        if (g_app.covers.slot[i].state == CC_READY) ready++;
    }
    CHECK(loading == 0);                                       /* no art in the fake fs: all FAILED */
    CHECK(ready == 0);
    /* demo games never go to the worker */
    host.fs_list = fake_none;
    cover_loader_stop(&L);
    cover_loader_init(&L, &io);
    app_init(&g_app, &g_lib, &host);
    run_frames(0, LOAD_MIN_TIME + 1.0f);
    CHECK(g_lib.tab[g_app.tab].demo);
    CHECK(cover_worker_step(&L) == 0);
    for (i = 0, ready = 0; i < g_app.covers.budget; i++) if (g_app.covers.slot[i].state == CC_READY) ready++;
    CHECK(ready > 0);
    ui_draw(&g_app);                                           /* draws with and without art */
    cover_loader_stop(&L);
    memfs_reset();
}

/* fix 1: sleep after 5 idle minutes, any button wakes without acting;
 * Show Game Info persists */
static void test_sleep_and_info(void)
{
    AppHost host;
    int sel;
    Settings s1, s2;
    printf("sleep mode, game info toggle\n");
    memset(&host, 0, sizeof(host));
    host.fs_list = fake_none;
    app_init(&g_app, &g_lib, &host);
    run_frames(0, LOAD_MIN_TIME + 1.5f);
    CHECK(g_app.mode == MODE_FLOW && !g_app.asleep);
    run_frames(0, SLEEP_TIME - 10.0f);
    CHECK(!g_app.asleep);
    tap(SFB_RIGHT);                                   /* activity resets the timer */
    run_frames(0, SLEEP_TIME - 10.0f);
    CHECK(!g_app.asleep);
    run_frames(0, 12.0f + SLEEP_FADE_TIME);
    CHECK(g_app.asleep && g_app.sleep_t >= 0.999f);
    ui_draw(&g_app);                                  /* asleep: dimmed, still drawn */
    sel = g_app.view[g_app.tab].sel;
    run_frames(SFB_RIGHT, 0.6f);                      /* the waking press doesn't move... */
    run_frames(0, 0.1f);
    CHECK(!g_app.asleep && g_app.view[g_app.tab].sel == sel);
    run_frames(0, WAKE_FADE_TIME + 0.05f);
    CHECK(g_app.sleep_t == 0.0f);
    tap(SFB_RIGHT);                                   /* ...the next one does */
    CHECK(g_app.view[g_app.tab].sel == sel + 1);
    run_frames(0, SLEEP_TIME + 2.0f);
    CHECK(g_app.asleep);
    run_frames(SFB_CIRCLE, HOLD_EXIT_TIME + 1.0f);    /* holding ◯ to wake never exits */
    CHECK(!g_app.asleep && !g_app.exiting);
    run_frames(0, 0.1f);

    /* Sleep Mode setting: Off never sleeps; other times are honored */
    g_app.settings.sleep_min = 0;
    run_frames(0, 0.5f);
    run_frames(0, 400.0f);
    CHECK(!g_app.asleep);
    g_app.settings.sleep_min = 1;
    run_frames(0, 1.0f);
    CHECK(g_app.asleep);
    tap(SFB_CROSS); run_frames(0, 0.5f);
    CHECK(!g_app.asleep && g_app.mode == MODE_FLOW);
    {
        Settings ss;
        GameCfg gg;
        char t[32];
        settings_defaults(&ss);
        memset(&gg, 0, sizeof(gg));
        menu_value_text(ROW_F_SLEEP, 0, &ss, &gg, t, sizeof(t));
        CHECK_STR(t, "Tras 5 min");
        CHECK(menu_change(ROW_F_SLEEP, 0, 1, &ss, &gg) == MENU_FX_CHANGED && ss.sleep_min == 10);
        CHECK(menu_change(ROW_F_SLEEP, 0, -1, &ss, &gg) == MENU_FX_CHANGED && ss.sleep_min == 5);
        menu_change(ROW_F_SLEEP, 0, -1, &ss, &gg); menu_change(ROW_F_SLEEP, 0, -1, &ss, &gg);
        menu_value_text(ROW_F_SLEEP, 0, &ss, &gg, t, sizeof(t));
        CHECK(ss.sleep_min == 0);
        CHECK_STR(t, "No");
        CHECK(menu_change(ROW_F_CASES, 0, 0, &ss, &gg) == MENU_FX_CHANGED && ss.cases == 0);
        CHECK_STR(menu_label(ROW_F_CASES), "Estuches de juegos");
        CHECK_STR(menu_label(ROW_F_SLEEP), "Modo de reposo");
    }
    settings_defaults(&s1);
    CHECK(s1.show_info == 1);
    s1.show_info = 0; s1.cases = 0; s1.sleep_min = 15;
    CHECK(settings_save(&s1, "build-host/test_info.ini") == 0);
    settings_defaults(&s2);
    CHECK(settings_load(&s2, "build-host/test_info.ini") == 0 && s2.show_info == 0 && s2.cases == 0 && s2.sleep_min == 15);
    {
        GameCfg g;
        char txt[32];
        memset(&g, 0, sizeof(g));
        CHECK(menu_change(ROW_F_INFO, 0, 0, &s1, &g) == MENU_FX_CHANGED && s1.show_info == 1);
        menu_value_text(ROW_F_INFO, 0, &s1, &g, txt, sizeof(txt));
        CHECK_STR(txt, "Sí");
        CHECK_STR(menu_label(ROW_F_INFO), "Mostrar info. del juego");
    }
    g_app.settings.show_info = 0;
    ui_draw(&g_app);
}

/* fix 2: the other tabs' visible covers stay loaded, so switching tabs
 * shows covers at once */
static void test_cover_cache_tabs(void)
{
    CoverCache cc;
    int f, i, ok = 1;
    printf("cover cache keeps other tabs\n");
    cc_init(&cc, CC_DEFAULT_BUDGET, NULL, NULL);
    cc_set_side(&cc, 0, 40, 300);
    cc_set_side(&cc, 2, 0, 5);
    for (f = 0; f < 60; f++) cc_update(&cc, 1, 100, 500);
    for (i = 100 - 7; i <= 100 + 7; i++) if (cc_lookup(&cc, 1, i) < 0) ok = 0;
    for (i = 40 - 7; i <= 40 + 7; i++) if (cc_lookup(&cc, 0, i) < 0) ok = 0;
    for (i = 0; i < 5; i++) if (cc_lookup(&cc, 2, i) < 0) ok = 0;
    CHECK(ok);
    CHECK(cc_resident_count(&cc) <= CC_DEFAULT_BUDGET);
    /* switching to tab 0 (the app always reports every tab): nothing new needs loading */
    cc_set_side(&cc, 1, 100, 500);
    for (f = 0; f < 4; f++) cc_update(&cc, 0, 40, 300);
    for (i = 40 - 7, ok = 1; i <= 40 + 7; i++) if (cc_lookup(&cc, 0, i) < 0) ok = 0;
    CHECK(ok);
    /* scrolling far on one tab never steals the others' visible covers */
    for (f = 0; f < 200; f++) cc_update(&cc, 0, 40 + f, 300);
    for (i = 100 - 7, ok = 1; i <= 100 + 7; i++) if (cc_lookup(&cc, 1, i) < 0) ok = 0;
    CHECK(ok);
}

/* fix 3: case images (ManaGunZ's layout) set the case shape and cover window */
static void test_case_images(void)
{
    CaseWindow w;
    printf("case images\n");
    layout_set_case_image(PLAT_PS3, 270.0f, 340.0f, 0.0f, 30.0f, 260.0f, 300.0f);
    CHECK_NEAR(layout_case_aspect(PLAT_PS3), 270.0 / 340.0, 1e-5);
    layout_case_window(PLAT_PS3, &w);
    CHECK_NEAR(w.x0, 0.0, 1e-6); CHECK_NEAR(w.x1, 260.0 / 270.0, 1e-5);
    CHECK_NEAR(w.y0, 30.0 / 340.0, 1e-5); CHECK_NEAR(w.y1, 330.0 / 340.0, 1e-5);
    CHECK_NEAR(layout_window_aspect(PLAT_PS3), 260.0 / 300.0, 1e-4);    /* = the cover's shape */
    ui_set_case_image(PLAT_PS3, TEX_CASE_PS3);
    g_app.tab = PLAT_PS3;
    ui_draw(&g_app);
    layout_use_case_images(0);                                           /* Custom Cases off: bare cover */
    CHECK_NEAR(layout_case_aspect(PLAT_PS3), 0.86, 0.002);
    layout_use_case_images(1);
    CHECK_NEAR(layout_case_aspect(PLAT_PS3), 270.0 / 340.0, 1e-5);
    ui_clear_case_images();
    CHECK_NEAR(layout_window_aspect(PLAT_PS3), 0.86, 0.002);             /* no image: bare cover */
    {
        CaseWindow bare;
        layout_case_window(PLAT_PS2, &bare);
        CHECK(bare.x0 == 0.0f && bare.y0 == 0.0f && bare.x1 == 1.0f && bare.y1 == 1.0f);
    }
    layout_set_case_image(PLAT_PS1, 0.0f, 250.0f, 30.0f, 5.0f, 240.0f, 240.0f);   /* bad size: ignored */
    CHECK_NEAR(layout_window_aspect(PLAT_PS1), 1.0, 0.002);
}

/* fix 5: multi-disc games show once; ✖ asks which disc */
static int fake_discs(void *ctx, const char *dir, FsEntry *out, int max)
{
    static const struct { const char *name; uint64_t size; } k[] = {
        { "Final Fantasy VII (Disc 1).cue", 100 }, { "Final Fantasy VII (Disc 1).bin", 600 },
        { "Final Fantasy VII (Disc 3).cue", 100 }, { "Final Fantasy VII (Disc 3).bin", 650 },
        { "Final Fantasy VII (Disc 2).cue", 100 }, { "Final Fantasy VII (Disc 2).bin", 640 },
        { "Crash Bandicoot.cue", 50 }, { "Crash Bandicoot.bin", 500 },
    };
    int i, n = 0;
    (void)ctx;
    if (strcmp(dir, "/dev_hdd0/PSXISO") != 0) return -1;
    for (i = 0; i < (int)(sizeof(k) / sizeof(k[0])) && n < max; i++, n++) {
        sf_strlcpy(out[n].name, k[i].name, FS_NAME_MAX);
        out[n].is_dir = 0;
        out[n].size = k[i].size;
    }
    return n;
}


/* milestone 3: launching through webMAN */
static char g_launch_req[LAUNCH_CMD_MAX];
static int g_launch_calls, g_launch_fail;
static int fake_launch(const char *request)
{
    sf_strlcpy(g_launch_req, request, sizeof(g_launch_req));
    g_launch_calls++;
    return g_launch_fail ? -1 : 0;
}
static WmConfig g_fake_wm;
static void fake_wm_config(WmConfig *cfg) { *cfg = g_fake_wm; }
/* a test that launched carries on: undo the exit */
static void relaunch_reset(void)
{
    g_app.exiting = 0; g_app.exit_fade = 0.0f; g_app.quit = 0; g_app.launched = 0;
}

static void test_multi_disc(void)
{
    char base[TITLE_MAX];
    int d, i, ff;
    AppHost host;
    const Game *g;
    printf("multi-disc games\n");
    CHECK(title_disc_split("Final Fantasy VII (Disc 2)", base, sizeof(base), &d) && d == 2);
    CHECK_STR(base, "Final Fantasy VII");
    CHECK(title_disc_split("Metal Gear Solid (Disk 1 of 2)", base, sizeof(base), &d) && d == 1);
    CHECK_STR(base, "Metal Gear Solid");
    CHECK(title_disc_split("Grandia (CD1)", base, sizeof(base), &d) && d == 1);
    CHECK_STR(base, "Grandia");
    CHECK(!title_disc_split("Gran Turismo 4", base, sizeof(base), &d) && d == 0);
    CHECK_STR(base, "Gran Turismo 4");
    CHECK(!title_disc_split("(Disc 1)", base, sizeof(base), &d));        /* nothing left: keep as is */

    /* library: three discs become one game, a lone "(Disc 1)" stays */
    lib_clear(&g_lib);
    lib_add(&g_lib, PLAT_PS1, "/p/FF (Disc 2).cue", "Final Fantasy VII (Disc 2).cue", 20, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS1, "/p/Crash.cue", "Crash Bandicoot.cue", 5, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS1, "/p/FF (Disc 1).cue", "Final Fantasy VII (Disc 1).cue", 10, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS1, "/p/FF (Disc 3).cue", "Final Fantasy VII (Disc 3).cue", 30, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS1, "/p/Solo (Disc 1).cue", "Solo Game (Disc 1).cue", 7, NULL, 0, 0, 0);
    CHECK(lib_group_discs(&g_lib, PLAT_PS1) == 2);
    CHECK(g_lib.tab[PLAT_PS1].count == 3);
    for (i = 0, ff = -1; i < g_lib.tab[PLAT_PS1].count; i++)
        if (strcmp(lib_title(&g_lib, &g_lib.tab[PLAT_PS1].games[i]), "Final Fantasy VII") == 0) ff = i;
    CHECK(ff >= 0);
    if (ff >= 0) {
        g = &g_lib.tab[PLAT_PS1].games[ff];
        CHECK(g->ndisc == 3 && g->size == 60);
        CHECK_STR(lib_path(&g_lib, g), "/p/FF (Disc 1).cue");                 /* covers & settings from disc 1 */
        CHECK(lib_game_disc(&g_lib, g, 0)->num == 1 && lib_game_disc(&g_lib, g, 2)->num == 3);
        CHECK_STR(lib_str(&g_lib, lib_game_disc(&g_lib, g, 1)->path), "/p/FF (Disc 2).cue");
        CHECK(lib_game_disc(&g_lib, g, 3) == NULL);
    }
    CHECK(idx_of(PLAT_PS1, "Solo Game (Disc 1)") >= 0);
    CHECK(idx_of(PLAT_PS1, "Crash Bandicoot") >= 0);

    /* app: ✖ on a multi-disc game opens Choose Disc; ✖ there launches that disc */
    memset(&host, 0, sizeof(host));
    host.fs_list = fake_discs;
    host.launch = fake_launch;
    g_launch_calls = 0;
    app_init(&g_app, &g_lib, &host);
    g_app.settings.last_tab = PLAT_PS1;
    run_frames(0, LOAD_MIN_TIME + 1.5f);
    CHECK(g_lib.tab[PLAT_PS1].count == 2);
    g_app.tab = PLAT_PS1;
    app_select(&g_app, PLAT_PS1, idx_of(PLAT_PS1, "Crash Bandicoot"), 0);
    tap(SFB_CROSS);
    CHECK(g_app.panel == PANEL_NONE && strstr(g_app.last_launch, "Crash Bandicoot") != NULL);   /* single disc: launches */
    CHECK(g_launch_calls == 1 && strstr(g_launch_req, "/mount_ps3/dev_hdd0/PSXISO/Crash%20Bandicoot.cue?random=0&emu=ps1_emu.self") != NULL);
    CHECK(g_app.exiting && g_app.launched);                                   /* ...and exits to the XMB */
    relaunch_reset();
    run_frames(0, 0.3f);
    app_select(&g_app, PLAT_PS1, idx_of(PLAT_PS1, "Final Fantasy VII"), 0);
    tap(SFB_CROSS);
    CHECK(g_app.panel == PANEL_DISC && g_app.nrows == 3);
    ui_draw(&g_app);
    tap(SFB_DOWN);
    tap(SFB_CROSS);
    CHECK(strstr(g_app.last_launch, "Final Fantasy VII (Disc 2)") != NULL);
    CHECK(g_launch_calls == 2 &&
          strstr(g_launch_req, "/mount_ps3/dev_hdd0/PSXISO/Final%20Fantasy%20VII%20(Disc%202).cue?random=0") != NULL);   /* the chosen disc */
    relaunch_reset();
    run_frames(0, 0.5f);
    CHECK(g_app.panel == PANEL_NONE);
    tap(SFB_CROSS); tap(SFB_CIRCLE); run_frames(0, 0.5f);   /* ◯ backs out without launching */
    CHECK(g_app.panel == PANEL_NONE && !g_app.exiting);
}

/* milestone 4: NTFS drives through webMAN's index (/dev_hdd0/tmp/wmtmp) */
static uint32_t make_ntfs_ptr(unsigned char *b, const uint32_t *start, const uint32_t *count, int n)
{
    int i;
    uint32_t o = 20;
    memset(b, 0, 20);
    b[7] = 0x02;                                           /* device (low byte) */
    b[11] = 0x01;                                          /* emu mode */
    b[15] = (unsigned char)n;                              /* sections */
    b[19] = 0x01;                                          /* tracks */
    for (i = 0; i < n; i++, o += 4) { b[o] = (unsigned char)(start[i] >> 24); b[o + 1] = (unsigned char)(start[i] >> 16); b[o + 2] = (unsigned char)(start[i] >> 8); b[o + 3] = (unsigned char)start[i]; }
    for (i = 0; i < n; i++, o += 4) { b[o] = (unsigned char)(count[i] >> 24); b[o + 1] = (unsigned char)(count[i] >> 16); b[o + 2] = (unsigned char)(count[i] >> 8); b[o + 3] = (unsigned char)count[i]; }
    return o + 8;                                          /* + a track descriptor */
}

static int g_ntfs_refreshes;
static int fake_ntfs_refresh(void) { g_ntfs_refreshes++; return 4; }

static void test_ntfs(void)
{
    static Scanner sc;
    static const ScanRoot roots[] = { { PLAT_PS3, NTFS_INDEX_DIR, SRC_NTFS_INDEX } };
    static const uint32_t st1[2] = { 100, 900000 }, ct1[2] = { 2048000, 1000 };
    static const uint32_t st2[1] = { 77 }, ct2[1] = { 1300000 };
    unsigned char ptr[128], sfo[256];
    char stem[FS_NAME_MAX];
    uint32_t len;
    int i, ps3 = 0, ps1 = 0;
    printf("NTFS drives (webMAN index)\n");
    /* names and kinds */
    CHECK(scanner_ntfs_kind("Demon's Souls (USA).ntfs[PS3ISO]", stem, sizeof(stem)) == PLAT_PS3);
    CHECK_STR(stem, "Demon's Souls (USA)");
    CHECK(scanner_ntfs_kind("Crash.ntfs[PSXISO]", stem, sizeof(stem)) == PLAT_PS1 && strcmp(stem, "Crash") == 0);
    CHECK(scanner_ntfs_kind("Movie.ntfs[BDISO]", stem, sizeof(stem)) == PLAT_BDISO); /* index movies separately */
    CHECK(scanner_ntfs_kind("Movie.ntfs[DVDISO]", stem, sizeof(stem)) == -1);
    CHECK(scanner_ntfs_kind("Demon's Souls (USA).PNG", stem, sizeof(stem)) == -1);
    CHECK(scanner_ntfs_kind(".ntfs[PS3ISO]", stem, sizeof(stem)) == -1);
    CHECK(scanner_ntfs_kind("XY.ntfs[PS3ISO]", stem, 2) == -1);                   /* too small for the stem */
    /* size from the pointer file: sections x 512 bytes */
    len = make_ntfs_ptr(ptr, st1, ct1, 2);
    CHECK(scanner_ntfs_size(ptr, len) == (uint64_t)(2048000 + 1000) * 512u);
    CHECK(scanner_ntfs_size(ptr, 27) == 0);                                       /* cut short */
    CHECK(scanner_ntfs_size(NULL, 0) == 0);
    ptr[15] = 0; CHECK(scanner_ntfs_size(ptr, len) == 0);                         /* no sections */
    /* two drives' games side by side in webMAN's index */
    memfs_reset();
    len = make_ntfs_ptr(ptr, st1, ct1, 2);
    memfs_put(NTFS_INDEX_DIR "/Demon's Souls (USA).ntfs[PS3ISO]", ptr, len);
    len = (uint32_t)make_sfo_str(sfo, "TITLE_ID", "BLUS30443");
    memfs_put(NTFS_INDEX_DIR "/Demon's Souls (USA).SFO", sfo, len);
    memfs_put(NTFS_INDEX_DIR "/Demon's Souls (USA).PNG", "IMG 70 100 0 0 255 255", 22);
    len = make_ntfs_ptr(ptr, st2, ct2, 1);
    memfs_put(NTFS_INDEX_DIR "/Gran Turismo 5.iso.ntfs[PS3ISO]", ptr, len);      /* webMAN kept ".iso"; no .SFO */
    memfs_put(NTFS_INDEX_DIR "/[RPG] Final Fantasy VII (Disc 1).ntfs[PSXISO]", ptr, len);   /* subfolder tag */
    memfs_put(NTFS_INDEX_DIR "/[RPG] Final Fantasy VII (Disc 2).ntfs[PSXISO]", ptr, len);
    memfs_put(NTFS_INDEX_DIR "/Some Movie.ntfs[BDISO]", ptr, len);
    memfs_put(NTFS_INDEX_DIR "/Okami (USA).PNG", "IMG 70 100 0 0 255 255", 22);   /* webMAN's cover cache */
    lib_clear(&g_lib);
    scanner_begin(&sc, roots, 1);
    scanner_set_reader(&sc, mem_read_at, NULL);
    while (!scanner_step(&sc, &g_lib, mem_list, NULL)) { }
    CHECK(sc.found[PLAT_PS3] == 2 && sc.found[PLAT_PS1] == 2 && sc.found[PLAT_PS2] == 0 && sc.found[PLAT_BDISO] == 1 && sc.found_ntfs == 5);
    for (i = 0; i < g_lib.tab[PLAT_PS3].count; i++) {
        const Game *g = &g_lib.tab[PLAT_PS3].games[i];
        if (strcmp(lib_title(&g_lib, g), "Demon's Souls") == 0) {
            ps3 |= 1;
            CHECK_STR(lib_id(&g_lib, g), "BLUS30443");                           /* from webMAN's .SFO */
            CHECK(g->size == (uint64_t)(2048000 + 1000) * 512u);
            CHECK_STR(lib_path(&g_lib, g), NTFS_INDEX_DIR "/Demon's Souls (USA).ntfs[PS3ISO]");
        }
        if (strcmp(lib_title(&g_lib, g), "Gran Turismo 5") == 0) {
            ps3 |= 2;
            CHECK(lib_id(&g_lib, g)[0] == '\0' && g->size == 1300000ull * 512u);
        }
    }
    CHECK(ps3 == 3);
    for (i = 0; i < g_lib.tab[PLAT_PS1].count; i++)
        if (strncmp(lib_title(&g_lib, &g_lib.tab[PLAT_PS1].games[i]), "Final Fantasy VII (Disc ", 24) == 0) ps1++;
    CHECK(ps1 == 2);                                                               /* "[RPG]" tag dropped */
    CHECK(lib_group_discs(&g_lib, PLAT_PS1) == 1 && g_lib.tab[PLAT_PS1].count == 1);   /* discs from 2 drives group too */
    /* without file access: still listed, no size or ID */
    lib_clear(&g_lib);
    scanner_begin(&sc, roots, 1);
    while (!scanner_step(&sc, &g_lib, mem_list, NULL)) { }
    CHECK(sc.found_ntfs == 5 && g_lib.tab[PLAT_PS3].games[0].size == 0);
    /* NTFS images also resolve only within the managed artwork directory. */
    {
        CoverCand cand[COVER_MAX_CANDS];
        char id[16];
        int nc = cover_candidates(PLAT_PS3, NTFS_INDEX_DIR "/Demon's Souls (USA).ntfs[PS3ISO]", 0, "BLUS30443", NULL, cand, COVER_MAX_CANDS);
        CHECK(nc == 2);
        CHECK_STR(cand[0].base, COVER_DL_DIR "/Demon's Souls (USA)");
        CHECK_STR(cand[1].base, COVER_DL_DIR "/BLUS30443");
        nc = cover_candidates(PLAT_PS3, NTFS_INDEX_DIR "/Gran Turismo 5.iso.ntfs[PS3ISO]", 0, "", NULL, cand, COVER_MAX_CANDS);
        CHECK(nc == 1 && strcmp(cand[0].base, COVER_DL_DIR "/Gran Turismo 5") == 0);
        CHECK(!discid_read(PLAT_PS3, NTFS_INDEX_DIR "/Demon's Souls (USA).ntfs[PS3ISO]", 0, mem_read_at, NULL, id, sizeof(id)));
        CHECK(!discid_read(PLAT_PS1, NTFS_INDEX_DIR "/Crash.ntfs[PSXISO]", 0, mem_read_at, NULL, id, sizeof(id)));
    }
    /* launching mounts the pointer file through webMAN */
    {
        LaunchReq r;
        char out[LAUNCH_CMD_MAX];
        memset(&r, 0, sizeof(r));
        r.plat = PLAT_PS3; r.path = NTFS_INDEX_DIR "/Demon's Souls (USA).ntfs[PS3ISO]"; r.autoplay = 1;
        CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
        CHECK_STR(out, "/wait.ps3?xmb;/mount_ps3/dev_hdd0/tmp/wmtmp/Demon's%20Souls%20(USA).ntfs%5BPS3ISO%5D?random=0;/wait.ps3?2;/play.ps3");
    }
    memfs_reset();
    lib_clear(&g_lib);
}

/* milestone 4: "Rescan Library" asks webMAN to re-index the NTFS drives first */
static void test_ntfs_rescan(void)
{
    AppHost host;
    int i;
    printf("NTFS: rescan\n");
    memfs_reset();
    lib_clear(&g_lib);
    memset(&host, 0, sizeof(host));
    host.fs_list = mem_list;
    host.read_at = mem_read_at;
    host.ntfs_refresh = fake_ntfs_refresh;
    g_ntfs_refreshes = 0;
    app_init(&g_app, &g_lib, &host);
    run_frames(0, LOAD_MIN_TIME + 1.5f);
    CHECK(g_app.mode == MODE_FLOW && g_ntfs_refreshes == 0);                      /* a normal start reads the index as it is */
    CHECK(flow_goto(ROW_F_RESCAN));
    (void)i;
    tap(SFB_CROSS);
    run_frames(0, RESCAN_MIN_TIME + 1.5f);
    CHECK(g_ntfs_refreshes == 1 && g_app.mode == MODE_FLOW);
}

/* 1.1: FAT32 USB drives (/dev_usb000...): the ports with a drive in them,
 * their PS3ISO / PS2ISO / PSXISO... folders, launched from there */
static void test_usb_drives(void)
{
    AppHost host;
    int nb, n, nd, t, found3 = 0, found1 = 0, i;
    const ScanRoot *base = scanner_default_roots(&nb), *r;
    printf("USB drives\n");
    memfs_reset();
    lib_clear(&g_lib);
    r = scanner_roots_with_usb(base, nb, mem_list, NULL, &n, &nd);
    CHECK(nd == 0 && n == nb);                                   /* nothing plugged in */
    memfs_put("/dev_usb001/readme.txt", "x", 1);
    memfs_put("/dev_usb001/PS3ISO/Demon's Souls [BLUS30443].iso", "iso", 3);
    memfs_put("/dev_usb001/PSXISO/Spyro (USA).iso", "iso", 3);
    memfs_put("/dev_usb006/note.txt", "x", 1);
    r = scanner_roots_with_usb(base, nb, mem_list, NULL, &n, &nd);
    CHECK(nd == 2 && n == nb + 16);
    CHECK_STR(r[nb].path, "/dev_usb001/PS3ISO");
    CHECK(r[nb].plat == PLAT_PS3 && r[nb + 4].plat == PLAT_PS1 && r[nb + 3].plat == PLAT_PS2);
    CHECK_STR(r[nb + 6].path, "/dev_usb001/BDISO");
    CHECK_STR(r[nb + 7].path, "/dev_usb001/SNES");
    CHECK(r[nb + 7].plat == PLAT_SNES); /* New root, original USB root order preserved. */
    CHECK_STR(r[nb + 8].path, "/dev_usb006/PS3ISO");
    memset(&host, 0, sizeof(host));
    host.fs_list = mem_list;
    host.read_at = mem_read_at;
    app_init(&g_app, &g_lib, &host);
    run_frames(0, LOAD_MIN_TIME + 1.5f);
    CHECK(g_app.mode == MODE_FLOW);
    for (t = 0; t < PLAT_COUNT; t++)
        for (i = 0; i < g_lib.tab[t].count; i++) {
            const char *p = lib_path(&g_lib, &g_lib.tab[t].games[i]);
            if (t == PLAT_PS3 && strcmp(p, "/dev_usb001/PS3ISO/Demon's Souls [BLUS30443].iso") == 0) found3 = 1;
            if (t == PLAT_PS1 && strcmp(p, "/dev_usb001/PSXISO/Spyro (USA).iso") == 0) found1 = 1;
        }
    CHECK(found3 && found1);
    {
        LaunchReq q;
        char out[LAUNCH_CMD_MAX];
        memset(&q, 0, sizeof(q));
        q.plat = PLAT_PS3; q.path = "/dev_usb001/PS3ISO/Game.iso"; q.autoplay = 1;
        CHECK(launch_build(&q, NULL, out, sizeof(out)) == 0);
        CHECK_STR(out, "/wait.ps3?xmb;/mount_ps3/dev_usb001/PS3ISO/Game.iso?random=0;/wait.ps3?2;/play.ps3");
    }
    memfs_reset();
    lib_clear(&g_lib);
}

/* milestone 3: the webMAN launch request */
static void test_launch(void)
{
    char out[LAUNCH_CMD_MAX], small[40];
    LaunchReq r;
    WmConfig wm;
    unsigned char cfg[1024];
    printf("launching through webMAN\n");
    /* URL encoding: spaces, + & ? # % ; [ ] and non-ASCII are escaped */
    CHECK(launch_url_encode("/dev_hdd0/PS2ISO/God of War (USA).iso", out, sizeof(out)) == 0);
    CHECK_STR(out, "/dev_hdd0/PS2ISO/God%20of%20War%20(USA).iso");
    CHECK(launch_url_encode("/a/R&C+1 #2;x?y[z]%.iso", out, sizeof(out)) == 0);
    CHECK_STR(out, "/a/R%26C%2B1%20%232%3Bx%3Fy%5Bz%5D%25.iso");
    CHECK(launch_url_encode("/a/Pok\xc3\xa9.iso", out, sizeof(out)) == 0);
    CHECK_STR(out, "/a/Pok%C3%A9.iso");
    CHECK(launch_url_encode("/dev_hdd0/PS2ISO/a much longer name here.iso", small, sizeof(small)) == -1 && small[0] == '\0');
    /* webMAN config: version 0x1337, ps1emu [581], autoplay [582], ps2emu [583] */
    memset(cfg, 0, sizeof(cfg));
    cfg[0] = 0x12; cfg[1] = 0x34;
    CHECK(!launch_parse_wm_config(cfg, sizeof(cfg), &wm) && !wm.valid);       /* wrong version */
    cfg[0] = 0x13; cfg[1] = 0x37; cfg[581] = 1; cfg[582] = 0; cfg[583] = 1;
    CHECK(launch_parse_wm_config(cfg, sizeof(cfg), &wm) && wm.valid && wm.ps1_netemu && !wm.autoplay && wm.ps2_netemu);
    CHECK(!launch_parse_wm_config(cfg, 584, &wm));                             /* too short */
    /* 0.6.2: an older webMAN's file (version 0, 1024 bytes) is read too */
    cfg[0] = 0; cfg[1] = 0; cfg[582] = 1; cfg[584] = 1;
    CHECK(launch_parse_wm_config(cfg, 1024, &wm) && wm.autoplay && wm.ps2config_off);
    CHECK(!launch_parse_wm_config(cfg, 1000, &wm));                            /* version 0 + other size: no */
    CHECK(!launch_parse_wm_config(NULL, 0, &wm));
    /* PS3 ISO, Auto-play: wait for the XMB, mount, then play */
    memset(&r, 0, sizeof(r));
    r.plat = PLAT_PS3; r.path = "/dev_hdd0/PS3ISO/Demon's Souls.iso"; r.autoplay = 1;
    CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
    CHECK_STR(out, "/wait.ps3?xmb;/mount_ps3/dev_hdd0/PS3ISO/Demon's%20Souls.iso?random=0;/wait.ps3?2;/play.ps3");
    /* Auto-play off: only mount (the disc icon waits on the XMB) */
    r.autoplay = 0;
    CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
    CHECK_STR(out, "/wait.ps3?xmb;/mount_ps3/dev_hdd0/PS3ISO/Demon's%20Souls.iso?random=0");
    /* offline comes before emu (webMAN cuts the query in front of emu=) */
    r.plat = PLAT_PS2; r.path = "/dev_hdd0/PS2ISO/Okami.iso"; r.offline = 1; r.emulator = 1; r.autoplay = 1;
    CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
    CHECK_STR(out, "/wait.ps3?xmb;/mount_ps3/dev_hdd0/PS2ISO/Okami.iso?random=0&offline=1&emu=ps2_netemu.self;/wait.ps3?2;/play.ps3");
    /* PS2 Default: no config -> webMAN decides; with config -> the user's saved emulator, named */
    r.offline = 0; r.emulator = 0;
    CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
    CHECK(strstr(out, "emu=") == NULL);
    memset(&wm, 0, sizeof(wm)); wm.valid = 1; wm.ps2_netemu = 0;
    CHECK(launch_build(&r, &wm, out, sizeof(out)) == 0);
    CHECK(strstr(out, "?random=0&emu=ps2_emu.self;") != NULL);
    wm.ps2_netemu = 1;
    CHECK(launch_build(&r, &wm, out, sizeof(out)) == 0);
    CHECK(strstr(out, "?random=0&emu=ps2_netemu.self;") != NULL);
    /* webMAN's own Auto-play starts the game itself: no second /play.ps3 */
    wm.autoplay = 1;
    CHECK(launch_build(&r, &wm, out, sizeof(out)) == 0);
    CHECK(strstr(out, "/play.ps3") == NULL);
    /* PS1: always names its emulator; offline doesn't apply */
    memset(&r, 0, sizeof(r));
    r.plat = PLAT_PS1; r.path = "/dev_hdd0/PSXISO/Crash.bin"; r.offline = 1;
    CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
    CHECK_STR(out, "/wait.ps3?xmb;/mount_ps3/dev_hdd0/PSXISO/Crash.bin?random=0&emu=ps1_emu.self");
    r.emulator = 1;
    CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
    CHECK(strstr(out, "&emu=ps1_netemu.self") != NULL);
    /* PS3 folder game */
    memset(&r, 0, sizeof(r));
    r.plat = PLAT_PS3; r.path = "/dev_hdd0/GAMES/BLES00932-[Demon]"; r.autoplay = 1;
    CHECK(launch_build(&r, NULL, out, sizeof(out)) == 0);
    CHECK(strstr(out, ";/mount_ps3/dev_hdd0/GAMES/BLES00932-%5BDemon%5D?random=0;") != NULL);
    /* bad input */
    r.path = ""; CHECK(launch_build(&r, NULL, out, sizeof(out)) == -1 && out[0] == '\0');
    r.path = "relative.iso"; CHECK(launch_build(&r, NULL, out, sizeof(out)) == -1);
    r.path = "/x.iso"; r.plat = 7; CHECK(launch_build(&r, NULL, out, sizeof(out)) == -1);
    r.plat = PLAT_PS3; CHECK(launch_build(&r, NULL, small, sizeof(small)) == -1 && small[0] == '\0');
    CHECK(launch_build(NULL, NULL, out, sizeof(out)) == -1);
    {   /* a path too long for webMAN's 2 KB request */
        char p[2000];
        memset(p, 'A', sizeof(p) - 1); p[0] = '/'; p[sizeof(p) - 1] = '\0';
        r.path = p; r.plat = PLAT_PS3;
        CHECK(launch_build(&r, NULL, out, sizeof(out)) == -1);
    }
}

/* milestone 3: the app sends the request and exits; settings go with it */
static void test_launch_app(void)
{
    AppHost host;
    GameCfg c;
    printf("launching: app\n");
    lib_clear(&g_lib);
    memset(&host, 0, sizeof(host));
    host.fs_list = fake_discs;
    host.launch = fake_launch;
    host.read_wm_config = fake_wm_config;
    memset(&g_fake_wm, 0, sizeof(g_fake_wm));
    g_fake_wm.valid = 1; g_fake_wm.ps2_netemu = 1;
    g_launch_calls = 0; g_launch_fail = 0;
    app_init(&g_app, &g_lib, &host);
    g_app.settings.last_tab = PLAT_PS1;
    run_frames(0, LOAD_MIN_TIME + 1.5f);
    g_app.tab = PLAT_PS1;
    app_select(&g_app, PLAT_PS1, idx_of(PLAT_PS1, "Crash Bandicoot"), 0);
    /* per-game: Netemu + Auto-play on */
    c = settings_game_get(&g_app.settings, lib_key(&g_lib, app_selected_game(&g_app, PLAT_PS1)));
    c.emulator = 1; c.autoplay = AP_ON;
    settings_game_set(&g_app.settings, &c);
    tap(SFB_CROSS);
    CHECK(g_launch_calls == 1);
    CHECK(strstr(g_launch_req, "&emu=ps1_netemu.self;/wait.ps3?2;/play.ps3") != NULL);
    CHECK(g_app.exiting && g_app.launched);
    run_frames(0, EXIT_FADE_TIME + 0.3f);
    CHECK(g_app.quit);                                         /* back to the XMB, where webMAN takes over */
    relaunch_reset();
    /* webMAN not running: a message, and spookyflow stays */
    g_launch_fail = 1;
    run_frames(0, 0.2f);
    tap(SFB_CROSS);
    CHECK(g_launch_calls == 2 && !g_app.exiting && g_app.toast_t < 0.1f);
    g_launch_fail = 0;
    run_frames(0, TOAST_TIME + 0.1f);
}

/* fix 7: discs named without "(Disc N)" are grouped too */
static void test_multi_disc_names(void)
{
    char base[TITLE_MAX];
    int d, i, mk = -1, gr = -1;
    printf("multi-disc names\n");
    CHECK(title_disc_split("Grandia CD2", base, sizeof(base), &d) && d == 2);
    CHECK_STR(base, "Grandia");
    CHECK(title_disc_split("Parasite Eve - Disc 1", base, sizeof(base), &d) && d == 1);
    CHECK_STR(base, "Parasite Eve");
    CHECK(title_disc_split("Xenogears_disc2", base, sizeof(base), &d) && d == 2);
    CHECK(!title_disc_split("Tekken 3", base, sizeof(base), &d));
    CHECK(!title_disc_split("Ridge Racer Type 4", base, sizeof(base), &d));
    CHECK(!title_disc_split("ABCD2", base, sizeof(base), &d));             /* "CD" must be its own word */
    lib_clear(&g_lib);
    lib_add(&g_lib, PLAT_PS2, "/p/Mortal Kombat - Deception (USA).iso", "Mortal Kombat - Deception (USA).iso", 4, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS2, "/p/Mortal Kombat - Deception (USA) (Bonus Disc).iso", "Mortal Kombat - Deception (USA) (Bonus Disc).iso", 1, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS2, "/p/Mortal Kombat - Shaolin Monks (USA).iso", "Mortal Kombat - Shaolin Monks (USA).iso", 3, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS2, "/p/Grandia CD2.iso", "Grandia CD2.iso", 2, NULL, 0, 0, 0);
    lib_add(&g_lib, PLAT_PS2, "/p/Grandia CD1.iso", "Grandia CD1.iso", 2, NULL, 0, 0, 0);
    CHECK(lib_group_discs(&g_lib, PLAT_PS2) == 2);
    CHECK(g_lib.tab[PLAT_PS2].count == 3);
    for (i = 0; i < g_lib.tab[PLAT_PS2].count; i++) {
        const char *t = lib_title(&g_lib, &g_lib.tab[PLAT_PS2].games[i]);
        if (strcmp(t, "Mortal Kombat - Deception") == 0) mk = i;
        if (strcmp(t, "Grandia") == 0) gr = i;
    }
    CHECK(mk >= 0 && gr >= 0);
    if (mk >= 0) {
        const Game *g = &g_lib.tab[PLAT_PS2].games[mk];
        char n0[TITLE_MAX], n1[TITLE_MAX];
        CHECK(g->ndisc == 2 && g->size == 5);
        lib_disc_name(&g_lib, lib_game_disc(&g_lib, g, 0), 0, n0, sizeof(n0));
        lib_disc_name(&g_lib, lib_game_disc(&g_lib, g, 1), 1, n1, sizeof(n1));
        CHECK_STR(n0, "USA");
        CHECK_STR(n1, "USA, Bonus Disc");
    }
    if (gr >= 0) {
        const Game *g = &g_lib.tab[PLAT_PS2].games[gr];
        char n0[TITLE_MAX];
        lib_disc_name(&g_lib, lib_game_disc(&g_lib, g, 0), 0, n0, sizeof(n0));
        CHECK(g->ndisc == 2);
        CHECK_STR(n0, "Disc 1");
        CHECK_STR(lib_path(&g_lib, g), "/p/Grandia CD1.iso");
    }
}

/* release years: no source for now, so every game's year stays unknown */
static void test_release_year(void)
{
    static CoverLoader L;
    CoverIO io;
    CoverCache cc;
    uint32_t len;
    unsigned char *img;
    printf("release years (none for now)\n");
    lib_clear(&g_lib);
    lib_add(&g_lib, PLAT_PS2, "/p/x.iso", "Game [SLUS-20914].iso", 1, NULL, 0, 0, 0);
    CHECK(g_lib.tab[PLAT_PS2].games[0].year == 0);
    memfs_reset();
    img = make_disc("BOOT2 = cdrom0:\\SLUS_209.14;1\r\n", NULL, 0, &len);
    memfs_put("/dev_hdd0/PS2ISO/Game.iso", img, len); free(img);
    mem_io(&io, "/cache");
    cover_loader_init(&L, &io);
    L.on_result = NULL;
    cc_init(&cc, 8, NULL, NULL);
    cc.slot[0].state = CC_LOADING;
    cover_request(&L, 0, PLAT_PS2, PLAT_PS2, 0, "/dev_hdd0/PS2ISO/Game.iso", 0, len, "");
    cover_worker_step(&L);
    CHECK(L.job[0].done_year == 0);
    cover_loader_stop(&L);
    memfs_reset();
}

/* fix 12: background picture and reflections */
static void test_bg_and_reflect(void)
{
    Settings s;
    GameCfg g;
    char t[32];
    int i;
    printf("backgrounds, reflections\n");
    settings_defaults(&s);
    memset(&g, 0, sizeof(g));
    CHECK(s.reflections == 1);
    CHECK(menu_change(ROW_F_REFLECT, 0, 0, &s, &g) == MENU_FX_CHANGED && s.reflections == 0);
    menu_value_text(ROW_F_REFLECT, 0, &s, &g, t, sizeof(t));
    CHECK_STR(t, "No");
    /* Background cycles through the 7 styles (the picture option is gone) */
    CHECK(s.background == BG_STAGE);                            /* fix 13: Stage is the default */
    for (i = 0; i < 6; i++) { menu_change(ROW_F_BACKGROUND, 0, 1, &s, &g); CHECK(s.background != BG_CUSTOM && s.background != BG_STAGE); }
    CHECK(s.background == BG_TWILIGHT);
    CHECK(menu_change(ROW_F_BACKGROUND, 0, 1, &s, &g) == MENU_FX_NONE);   /* 0.8.2: the last one: no wrap */
    CHECK(s.background == BG_TWILIGHT);
    menu_change(ROW_F_BACKGROUND, 0, -1, &s, &g);
    CHECK(s.background == BG_HALO);
    settings_parse_line(&s, "background2=1");                    /* 0.7.0: 1 = a picture of your own again */
    CHECK(s.background == BG_CUSTOM);
    settings_parse_line(&s, "background_image=Night Sky.jpg");
    CHECK_STR(s.bg_image, "Night Sky.jpg");
    settings_parse_line(&s, "background2=7");
    settings_parse_line(&s, "background=0");                     /* the pre-fix-13 key is ignored... */
    CHECK(s.background == BG_STAGE);
    settings_parse_line(&s, "background2=3");                    /* ...the new one is used */
    CHECK(s.background == BG_FIREFLIES);
    ui_set_stage(TEX_STAGE, TEX_STAGE_SHAPES);
    ui_set_stage_fx(TEX_STAGE_FX);
    ui_set_stage_highlight(TEX_STAGE_HI);
    g_app.settings.background = BG_STAGE;
    ui_draw(&g_app);
    ui_set_stage(-1, -1);
    ui_set_stage_fx(-1);
    ui_set_stage_highlight(-1);
    CHECK_STR(settings_background_name(BG_FIREFLIES), "Luciérnagas");
    /* every background, with and without reflections, draws */
    for (i = 0; i < BG_COUNT; i++) { g_app.settings.background = i; ui_draw(&g_app); }
    g_app.settings.reflections = 0;
    for (i = 0; i < BG_COUNT; i++) { g_app.settings.background = i; ui_draw(&g_app); }
    g_app.settings.background = BG_ANIMATED;
    g_app.settings.reflections = 1;
}
