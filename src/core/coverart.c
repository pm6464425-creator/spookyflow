/* coverart.c - find, decode, fit, cache and stream cover art (coverart.h). */
#include "coverart.h"
#include "layout.h"
#include "log.h"
#include "render.h"
#include "title.h"
#include "util.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char sf_static_slots[(R_COVER_SLOTS >= CC_MAX_SLOTS) ? 1 : -1];

/* SpookyFlow owns one authoritative source folder. Images copied beside
 * ISOs by multiMAN are commonly 320x176 ICON0 screenshots, not box art.
 * Never pick them or foreign manager folders as a cover. */
enum { F_SF_DL = 0, F_COUNT = 1 };
static const char *const k_folders[F_COUNT] = { COVER_DL_DIR };

static const char *const k_exts[] = { ".JPG", ".PNG", ".jpg", ".png" };
static const char *const k_plat_dir[PLAT_COUNT] = { "ps1", "ps2", "ps3", "bdiso" };

#define CVR_HEADER 16
#define CVR_VERSION 6     /* source-policy change: invalidate old cached 320x176 sidecar icons */

int cover_folder_count(void) { return F_COUNT; }
const char *cover_folder(int i) { return (i >= 0 && i < F_COUNT) ? k_folders[i] : ""; }

/* ------------------------------------------------------------------ */
/* Geometry                                                            */

void cover_tex_size(int plat, int *w, int *h)
{
    switch (plat) {
    case PLAT_PS1: *w = 384; *h = 384; break;   /* square jewel case */
    case PLAT_PS2: *w = 320; *h = 448; break;   /* tall DVD case */
    default:       *w = 384; *h = 448; break;   /* Blu-ray case */
    }
}

float cover_art_aspect(int plat)
{
    return layout_window_aspect(plat);
}

/* ------------------------------------------------------------------ */
/* Candidates                                                          */
/* Some fan mods reuse the base game's title ID, so prefer file-name
 * overrides and avoid automatically choosing the original game's cover. */
static int cover_name_is_custom(const char *path)
{
    static const char *const terms[] = {
        "[MOD]", "(MOD)", " - MOD ", "[PATCH]", "(PATCH)",
        "[CUSTOM]", "(CUSTOM)", "FC 27", "FIFA 27", "PES 2027",
        "EFOOTBALL 2027"
    };
    int i;
    const char *s;
    if (!path) return 0;
    s = strrchr(path, '/');
    s = s ? s+1 : path;
    for (i=0;i<SF_ARRAY_LEN(terms);i++) {
        const char *p;
        size_t n=strlen(terms[i]);
        for (p=s; *p; p++)
            if (sf_strnicmp(p, terms[i], n)==0) return 1;
    }
    return 0;
}


static int strip_one_ext(char *s)
{
    static const char *const exts[] = { ".iso", ".bin", ".cue", ".img", ".mdf", ".enc", ".dec", ".chd", ".pbp" };
    size_t n = strlen(s), k = n;
    int i;
    {   /* webMAN's NTFS pointer files: "Name.ntfs[PS3ISO]" (cover: Name.PNG next to it) */
        char *t = strstr(s, ".ntfs[");
        if (t && n > 0 && s[n - 1] == ']' && t > s) { *t = '\0'; return 1; }
    }
    while (k > 0 && sf_isdigit(s[k - 1])) k--;
    if (k < n && k > 0 && s[k - 1] == '.') {          /* ".iso.0" split part */
        char *dot = s + k - 1;
        *dot = '\0';
        for (i = 0; i < SF_ARRAY_LEN(exts); i++) if (sf_ends_with_i(s, exts[i])) return 1;
        *dot = '.';
        return 0;
    }
    for (i = 0; i < SF_ARRAY_LEN(exts); i++) {
        size_t le = strlen(exts[i]);
        if (n > le && sf_ends_with_i(s, exts[i])) { s[n - le] = '\0'; return 1; }
    }
    return 0;
}

static int add(CoverCand *out, int n, int max, const char *a, const char *b, const char *c)
{
    char tmp[COVER_PATH_MAX];
    if (n >= max) return n;
    sf_strlcpy(tmp, a, sizeof(tmp));
    if (b) sf_strlcat(tmp, b, sizeof(tmp));
    if (c) sf_strlcat(tmp, c, sizeof(tmp));
    if (strlen(tmp) + 5 >= COVER_PATH_MAX) return n;    /* room for ".JPG" */
    sf_strlcpy(out[n].base, tmp, COVER_PATH_MAX);
    return n + 1;
}

int cover_candidates(int plat, const char *game_path, int is_dir, const char *id,
                     const unsigned char *dir_ok, CoverCand *out, int max)
{
    char name[COVER_PATH_MAX];
    const char *slash;
    int n = 0;
    /* One source folder for downloaded and manually selected images.
     * Name.jpg takes precedence over <TITLE_ID>.jpg, so modified titles
     * can have individual artwork without colliding with the base game.
     * Never use the sidecar PNG next to an ISO or an external manager cache. */
    (void)plat;
    (void)is_dir;
    (void)dir_ok;
    if (!game_path || !game_path[0] || !out || max <= 0) return 0;
    slash = strrchr(game_path, '/');
    sf_strlcpy(name, slash ? slash + 1 : game_path, sizeof(name));
    while (strip_one_ext(name)) { }
    /* Movie art always resides on the same volume as the ISO.
     * Neither original images nor processed Blu-ray art is cached on PS3 HDD. */
    if (plat == PLAT_BDISO) {
        const char *sep = strrchr(game_path, '/');
        char dir[COVER_PATH_MAX], covers[COVER_PATH_MAX];
        size_t npath = sep ? (size_t)(sep - game_path) : 0;
        if (npath && npath + 9 < sizeof(dir)) {
            memcpy(dir, game_path, npath); dir[npath] = '\0';
            memcpy(covers,dir,npath); memcpy(covers+npath,"/COVERS",8);
            /* FlowMovie 3D already accepted manually placed JPGs matching
             * the ISO name in the BDISO directory. Prefer that custom
             * artwork to any previously automatically downloaded image. */
            n=add(out,n,max,dir,"/",name);
            n=add(out,n,max,covers,"/",name);
            /* Some users name artwork with the full ISO filename, e.g.
             * "Avatar 3D.iso.jpg" instead of "Avatar 3D.jpg". */
            if (slash && slash[1]) {
                n=add(out,n,max,dir,"/",slash+1);
                n=add(out,n,max,covers,"/",slash+1);
            }
        }
        return n;
    }
    if (name[0]) n = add(out, n, max, COVER_DL_DIR, "/", name);
    if (id && id[0] && !cover_name_is_custom(game_path) &&
        sf_stricmp(name, id) != 0)
        n = add(out, n, max, COVER_DL_DIR, "/", id);
    return n;
}

void cover_cache_stem(const char *cache_dir, int plat, const char *game_path, uint64_t size, char *out, size_t cap)
{
    uint64_t h = 1469598103934665603ULL;
    const unsigned char *p;
    int i;
    for (p = (const unsigned char *)game_path; *p; p++) { h ^= *p; h *= 1099511628211ULL; }
    for (i = 0; i < 8; i++) { h ^= (size >> (i * 8)) & 0xFF; h *= 1099511628211ULL; }
    if (plat < 0 || plat >= PLAT_COUNT) plat = PLAT_PS3;
    snprintf(out, cap, "%s/%s/%08x%08x", cache_dir ? cache_dir : "", k_plat_dir[plat],
             (unsigned)(h >> 32), (unsigned)(h & 0xFFFFFFFFu));
}

/* ------------------------------------------------------------------ */
/* Image processing                                                    */

#define BG_R 20.0f
#define BG_G 20.0f
#define BG_B 22.0f

static const unsigned char k_bayer[4][4] = {
    {  0,  8,  2, 10 }, { 12,  4, 14,  6 }, {  3, 11,  1,  9 }, { 15,  7, 13,  5 }
};

typedef struct { int i0, n; float w[64]; } Taps;   /* kernel taps for one output sample */

/* Tent filter: radius = max(1, scale) source pixels (smooth when shrinking,
 * bilinear when enlarging). Taps outside [0, len) are clamped (crop mode)
 * or marked with negative index -1 (fit mode: backdrop). */
static void make_taps(float c, float scale, Taps *t)
{
    float r = scale > 1.0f ? scale : 1.0f, sum = 0.0f;
    int i, a, b;
    if (r > 31.0f) r = 31.0f;
    a = (int)ceilf(c - r);
    b = (int)floorf(c + r);
    if (b - a + 1 > 64) b = a + 63;
    t->i0 = a;
    t->n = b - a + 1;
    for (i = 0; i < t->n; i++) {
        float d = sf_absf((float)(a + i) - c) / r, w = 1.0f - d;
        if (w < 0.0f) w = 0.0f;
        t->w[i] = w;
        sum += w;
    }
    if (sum <= 0.0f) { t->n = 1; t->i0 = (int)floorf(c + 0.5f); t->w[0] = 1.0f; return; }
    for (i = 0; i < t->n; i++) t->w[i] /= sum;
}

void cover_process(const uint8_t *rgba, int w, int h, float aspect, uint16_t *dst, int tw, int th)
{
    float cx, cy, cw, ch, sx, sy, src_aspect, ratio;
    int fit, x, y, row0, row1, nrows;
    float *tmp;
    if (!rgba || w <= 0 || h <= 0 || tw <= 0 || th <= 0) return;
    src_aspect = (float)w / (float)h;
    ratio = src_aspect > aspect ? src_aspect / aspect : aspect / src_aspect;
    /* never cut anything off: a cover close to the case shape is stretched
     * slightly to fill it (as ManaGunZ does); something very different (a
     * wide banner) is shown whole on the dark backdrop */
    fit = ratio > COVER_STRETCH_MAX;
    if (!fit) { cw = (float)w; ch = (float)h; }
    else if (src_aspect > aspect) { cw = (float)w; ch = cw / aspect; }
    else { ch = (float)h; cw = ch * aspect; }
    cx = ((float)w - cw) * 0.5f;
    cy = ((float)h - ch) * 0.5f;
    sx = cw / (float)tw;
    sy = ch / (float)th;

    /* source rows the vertical filter can touch */
    {
        float ry = sy > 1.0f ? sy : 1.0f;
        row0 = (int)floorf(cy - ry - 1.0f);
        row1 = (int)ceilf(cy + ch + ry + 1.0f);
        if (row0 < 0) row0 = 0;
        if (row1 > h) row1 = h;
    }
    nrows = row1 - row0;
    if (nrows <= 0) return;
    tmp = (float *)malloc((size_t)nrows * (size_t)tw * 3u * sizeof(float));
    if (!tmp) {
        for (x = 0; x < tw * th; x++) dst[x] = 0x8000u | (2u << 10) | (2u << 5) | 2u;
        return;
    }
    /* horizontal pass: composite over the backdrop, filter to tw columns */
    for (x = 0; x < tw; x++) {
        Taps t;
        int k;
        make_taps(cx + ((float)x + 0.5f) * sx - 0.5f, sx, &t);
        for (y = row0; y < row1; y++) {
            const uint8_t *row = rgba + (size_t)y * (size_t)w * 4u;
            float r = 0.0f, g = 0.0f, b = 0.0f;
            float *o = tmp + ((size_t)(y - row0) * (size_t)tw + (size_t)x) * 3u;
            for (k = 0; k < t.n; k++) {
                int i = t.i0 + k;
                float wt = t.w[k];
                if (i < 0 || i >= w) {
                    if (fit) { r += wt * BG_R; g += wt * BG_G; b += wt * BG_B; continue; }
                    i = i < 0 ? 0 : w - 1;
                }
                {
                    const uint8_t *p = row + (size_t)i * 4u;
                    float a = (float)p[3] / 255.0f;
                    r += wt * ((float)p[0] * a + BG_R * (1.0f - a));
                    g += wt * ((float)p[1] * a + BG_G * (1.0f - a));
                    b += wt * ((float)p[2] * a + BG_B * (1.0f - a));
                }
            }
            o[0] = r; o[1] = g; o[2] = b;
        }
    }
    /* vertical pass + dither to A1R5G5B5 */
    for (y = 0; y < th; y++) {
        Taps t;
        make_taps(cy + ((float)y + 0.5f) * sy - 0.5f, sy, &t);
        for (x = 0; x < tw; x++) {
            float c[3] = { 0.0f, 0.0f, 0.0f };
            int k, ch_i, q[3];
            float d = ((float)k_bayer[y & 3][x & 3] + 0.5f) / 16.0f;   /* ordered dither, mean 0.5 */
            for (k = 0; k < t.n; k++) {
                int i = t.i0 + k;
                const float *p;
                if (i < 0 || i >= h) {
                    if (fit) { c[0] += t.w[k] * BG_R; c[1] += t.w[k] * BG_G; c[2] += t.w[k] * BG_B; continue; }
                    i = i < 0 ? 0 : h - 1;
                }
                if (i < row0) i = row0;
                if (i >= row1) i = row1 - 1;
                p = tmp + ((size_t)(i - row0) * (size_t)tw + (size_t)x) * 3u;
                c[0] += t.w[k] * p[0]; c[1] += t.w[k] * p[1]; c[2] += t.w[k] * p[2];
            }
            for (ch_i = 0; ch_i < 3; ch_i++) {
                int v = (int)floorf(c[ch_i] * (31.0f / 255.0f) + d);
                q[ch_i] = v < 0 ? 0 : (v > 31 ? 31 : v);
            }
            dst[(size_t)y * (size_t)tw + (size_t)x] = (uint16_t)(0x8000u | ((unsigned)q[0] << 10) | ((unsigned)q[1] << 5) | (unsigned)q[2]);
        }
    }
    free(tmp);
}

/* ------------------------------------------------------------------ */
/* Loader: main thread side                                            */

static void lock(CoverLoader *L)
{
    int spins = 0;
    while (__sync_lock_test_and_set(&L->lock, 1)) {
        if (++spins > 2000) { spins = 0; if (L->io.sleep_ms) L->io.sleep_ms(L->io.ctx, 0); }
    }
}

static void unlock(CoverLoader *L) { __sync_lock_release(&L->lock); }

void cover_loader_init(CoverLoader *L, const CoverIO *io)
{
    memset(L, 0, sizeof(*L));
    L->io = *io;
    L->focus_tab = -1;
}

void cover_request(CoverLoader *L, int slot, int plat, int tab, int index, const char *path,
                   int is_dir, uint64_t size, const char *id)
{
    CoverJob *j;
    if (slot < 0 || slot >= CC_MAX_SLOTS) return;
    lock(L);
    j = &L->job[slot];
    j->plat = plat;
    j->tab = tab;
    j->index = index;
    j->is_dir = is_dir;
    j->size = size;
    sf_strlcpy(j->path, path ? path : "", sizeof(j->path));
    sf_strlcpy(j->id, id ? id : "", sizeof(j->id));
    j->req_gen++;
    unlock(L);
}

void cover_cancel_all(CoverLoader *L)
{
    int i;
    lock(L);
    for (i = 0; i < CC_MAX_SLOTS; i++) L->job[i].done_gen = L->job[i].req_gen;
    unlock(L);
}

void cover_set_focus(CoverLoader *L, int tab, int index)
{
    L->focus_tab = tab;
    L->focus_index = index;
}

void cover_poll(CoverLoader *L, CoverCache *cc)
{
    int i;
    lock(L);
    for (i = 0; i < cc->budget && i < CC_MAX_SLOTS; i++) {
        CoverJob *j = &L->job[i];
        if (cc->slot[i].state != CC_LOADING || j->done_gen != j->req_gen) continue;
        if (L->on_result) L->on_result(L->on_result_user, j->tab, j->index, j->done_year);
        if (j->done_ok) {
            r_cover_define(i, j->w, j->h);
            cc_mark(cc, i, TEX_COVER0 + i);
        } else {
            cc_mark(cc, i, -1);
        }
    }
    unlock(L);
}

void cover_wipe_cache(CoverLoader *L) { L->wipe = 1; }

void cover_loader_stop(CoverLoader *L)
{
    int waited = 0;
    L->quit = 1;
    while (L->running && waited < 2000) {
        if (L->io.sleep_ms) L->io.sleep_ms(L->io.ctx, 10);
        waited += 10;
    }
    if (!L->running) {          /* never free under a still-running worker */
        free(L->scratch);
        L->scratch = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Worker                                                              */

static void probe(CoverLoader *L)
{
    int i;
    for (i = 0; i < F_COUNT; i++) L->dir_ok[i] = L->io.is_dir ? (unsigned char)(L->io.is_dir(L->io.ctx, k_folders[i]) != 0) : 1;
    L->probed = 1;
    {
        int n = 0;
        for (i = 0; i < F_COUNT; i++) if (L->dir_ok[i]) n++;
        log_printf("covers: %d of %d cover folders found", n, F_COUNT);
        for (i = 0; i < F_COUNT; i++) if (L->dir_ok[i]) log_printf("covers:   %s", k_folders[i]);
    }
}

static void wipe(CoverLoader *L)
{
    int p, removed = 0;
    FsEntry *ent;
    if (!L->io.cache_dir || !L->io.list || !L->io.remove) return;
    ent = (FsEntry *)malloc(sizeof(FsEntry) * 256);
    if (!ent) return;
    for (p = 0; p < PLAT_COUNT; p++) {
        char dir[COVER_PATH_MAX], f[COVER_PATH_MAX + FS_NAME_MAX + 2];
        int rounds;
        snprintf(dir, sizeof(dir), "%s/%s", L->io.cache_dir, k_plat_dir[p]);
        for (rounds = 0; rounds < 64; rounds++) {   /* 256 per listing, up to 16k files */
            int n = L->io.list(L->io.ctx, dir, ent, 256), i, got = 0;
            if (n <= 0) break;
            for (i = 0; i < n; i++) {
                if (ent[i].is_dir || ent[i].name[0] == '.') continue;
                /* Explicit Rescan Library: replacement JPG/PNG must defeat a
                 * processed .cvr image from the older version. */
                if (!sf_ends_with_i(ent[i].name, ".none") &&
                    !sf_ends_with_i(ent[i].name, ".cvr")) continue;
                snprintf(f, sizeof(f), "%s/%s", dir, ent[i].name);
                if (L->io.remove(L->io.ctx, f) == 0) { removed++; got++; }
            }
            if (!got) break;
        }
    }
    free(ent);
    log_printf("covers: explicit rescan invalidated %d processed images/markers", removed);
}

static int read_cache(CoverLoader *L, const char *cvr, int tw, int th, int *year)
{
    unsigned char h[CVR_HEADER];
    uint16_t bom;
    uint32_t n = (uint32_t)tw * (uint32_t)th * 2u;
    if (!L->io.read_at || L->io.read_at(L->io.ctx, cvr, 0, h, CVR_HEADER) != CVR_HEADER) return 0;
    memcpy(&bom, h + 10, 2);
    if (memcmp(h, "SFCV", 4) != 0 || h[4] != CVR_VERSION || bom != 0xFEFF) return 0;
    if (((h[6] << 8) | h[7]) != tw || ((h[8] << 8) | h[9]) != th) return 0;
    *year = 0;                     /* years in older caches are not shown */
    return L->io.read_at(L->io.ctx, cvr, CVR_HEADER, L->scratch, n) == (int)n;
}

static void write_cache(CoverLoader *L, const char *cvr, int plat, int tw, int th, int year)
{
    unsigned char h[CVR_HEADER];
    uint16_t bom = 0xFEFF;
    memset(h, 0, sizeof(h));
    memcpy(h, "SFCV", 4);
    h[4] = CVR_VERSION;
    h[5] = (unsigned char)plat;
    h[6] = (unsigned char)(tw >> 8); h[7] = (unsigned char)tw;
    h[8] = (unsigned char)(th >> 8); h[9] = (unsigned char)th;
    memcpy(h + 10, &bom, 2);
    h[12] = (unsigned char)(year >> 8); h[13] = (unsigned char)year;
    if (L->io.write_file(L->io.ctx, cvr, h, CVR_HEADER, L->scratch, (uint32_t)tw * (uint32_t)th * 2u) != 0)
        log_printf("covers: could not write %s", cvr);
}

static void ensure_cache_dirs(CoverLoader *L, int plat)
{
    char d[COVER_PATH_MAX];
    if (!L->io.mkdir || !L->io.cache_dir) return;
    L->io.mkdir(L->io.ctx, L->io.cache_dir);
    snprintf(d, sizeof(d), "%s/%s", L->io.cache_dir, k_plat_dir[plat]);
    L->io.mkdir(L->io.ctx, d);
}

/* PNG: refuse oversized images from the header, before the decoder
 * allocates width*height*4 bytes for them. 1 = fine to decode. */
static int png_size_ok(CoverLoader *L, const char *path)
{
    unsigned char h[24];
    uint32_t w, hh;
    if (!sf_ends_with_i(path, ".png") || !L->io.read_at) return 1;
    if (L->io.read_at(L->io.ctx, path, 0, h, 24) != 24) return 1;     /* let the decoder report it */
    if (memcmp(h, "\x89PNG\r\n\x1a\n", 8) != 0 || memcmp(h + 12, "IHDR", 4) != 0) return 1;
    w = ((uint32_t)h[16] << 24) | ((uint32_t)h[17] << 16) | ((uint32_t)h[18] << 8) | h[19];
    hh = ((uint32_t)h[20] << 24) | ((uint32_t)h[21] << 16) | ((uint32_t)h[22] << 8) | h[23];
    if (w > COVER_SRC_MAX_DIM || hh > COVER_SRC_MAX_DIM) {
        log_printf("covers: %s is %ux%u, skipped (max %d)", path, (unsigned)w, (unsigned)hh, COVER_SRC_MAX_DIM);
        return 0;
    }
    return 1;
}

/* Returns 1 with pixels in L->scratch. *src describes where they came from. */
static int load_job(CoverLoader *L, const CoverJob *j, int tw, int th, char *src, size_t src_cap, int *year)
{
    char stem[COVER_PATH_MAX], f[COVER_PATH_MAX + 16], id[16];
    CoverCand *cand;
    int nc, c, e, pass, ok = 0, had_movie_art = 0;
    int cache = j->plat != PLAT_BDISO && L->io.cache_dir && L->io.write_file;

    if (cache) {
        cover_cache_stem(L->io.cache_dir, j->plat, j->path, j->size, stem, sizeof(stem));
        snprintf(f, sizeof(f), "%s.cvr", stem);
        if (read_cache(L, f, tw, th, year)) { sf_strlcpy(src, "cache", src_cap); L->n_cache++; return 1; }
        snprintf(f, sizeof(f), "%s.none", stem);
        if (L->io.is_file && L->io.is_file(L->io.ctx, f)) {
            /* the marker keeps the year: [version, year hi, year lo]; an older marker is looked up again */
            unsigned char y[3];
            if (L->io.read_at && L->io.read_at(L->io.ctx, f, 0, y, 3) == 3 && y[0] == CVR_VERSION) {
                *year = 0;
                sf_strlcpy(src, "none (cached)", src_cap);
                return 0;
            }
        }
    }
    if (!L->probed) probe(L);

    sf_strlcpy(id, j->id, sizeof(id));
    if (!id[0]) discid_read(j->plat, j->path, j->is_dir, L->io.read_at, L->io.ctx, id, sizeof(id));
    *year = 0;                     /* no release-year source for now (the cache keeps the field) */

    cand = (CoverCand *)malloc(sizeof(CoverCand) * COVER_MAX_CANDS);
    if (!cand) return 0;
    nc = cover_candidates(j->plat, j->path, j->is_dir, id, L->dir_ok, cand, COVER_MAX_CANDS);
    if (j->plat == PLAT_BDISO && L->io.ensure_movie_art && L->io.is_file) {
        for(c=0;c<nc && !had_movie_art;c++) {
            for(e=0;e<SF_ARRAY_LEN(k_exts);e++) {
                snprintf(f,sizeof(f),"%s%s",cand[c].base,k_exts[e]);
                if(L->io.is_file(L->io.ctx,f)) { had_movie_art=1; break; }
            }
        }
        if(!had_movie_art)(void)L->io.ensure_movie_art(L->io.ctx,j->path);
    }
    /* A saved movie JPEG can exist yet be unrenderable on PS3 (for
     * example a progressive JPEG). Retry once to obtain an equivalent
     * compatible PNG without deleting or modifying the original JPG. */
    for(pass=0;pass<2 && !ok;pass++) {
        if(pass==1) {
            if(j->plat!=PLAT_BDISO || !had_movie_art ||
               !L->io.ensure_movie_art)break;
            log_printf("bdiso: stored cover could not decode, trying compatible PNG: %s",j->path);
            (void)L->io.ensure_movie_art(L->io.ctx,j->path);
        }
        for (c = 0; c < nc && !ok; c++) {
            for (e = 0; e < SF_ARRAY_LEN(k_exts) && !ok; e++) {
                uint8_t *rgba = NULL;
                int w = 0, h = 0;
                snprintf(f, sizeof(f), "%s%s", cand[c].base, k_exts[e]);
                if (!L->io.is_file || !L->io.is_file(L->io.ctx, f)) continue;
                if (!png_size_ok(L, f)) continue;
                if (!L->io.decode || L->io.decode(L->io.ctx, f, &w, &h, &rgba) != 0 || !rgba) {
                    log_printf("covers: could not decode %s", f);
                    free(rgba);
                    continue;  /* A compatible PNG may exist beside a JPEG. */
                }
                if (w > 0 && h > 0 && w <= COVER_SRC_MAX_DIM && h <= COVER_SRC_MAX_DIM) {
                    cover_process(rgba, w, h, cover_art_aspect(j->plat), (uint16_t *)L->scratch, tw, th);
                    snprintf(src, src_cap, "%s (%dx%d)", f, w, h);
                    ok = 1;
                } else {
                    log_printf("covers: %s is %dx%d, skipped (max %d)", f, w, h, COVER_SRC_MAX_DIM);
                }
                free(rgba);
            }
        }
    }
    free(cand);
    if (ok) L->n_art++; else L->n_none++;
    if (!ok) snprintf(src, src_cap, "none (id %s)", id[0] ? id : "unknown");
    if (cache) {
        ensure_cache_dirs(L, j->plat);
        if (ok) {
            snprintf(f, sizeof(f), "%s.cvr", stem);
            write_cache(L, f, j->plat, tw, th, *year);
        } else {
            unsigned char y[3];
            y[0] = CVR_VERSION; y[1] = (unsigned char)(*year >> 8); y[2] = (unsigned char)*year;
            snprintf(f, sizeof(f), "%s.none", stem);
            L->io.write_file(L->io.ctx, f, y, 3, "", 0);   /* the marker keeps the year */
        }
    }
    return ok;
}

int cover_worker_step(CoverLoader *L)
{
    CoverJob j;
    int i, best = -1, gen, ok = 0, tw, th, year;
    long best_score = 0;
    char src[COVER_PATH_MAX + 32];
    uint16_t *dst;

    if (L->wipe) {
        L->wipe = 0;
        wipe(L);
        L->probed = 0;      /* a cover pack may have been added too */
        return 1;
    }
    if (!L->scratch) {
        L->scratch = (uint8_t *)malloc(R_COVER_SLOT_BYTES);
        if (!L->scratch) return 0;
    }
    lock(L);
    for (i = 0; i < CC_MAX_SLOTS; i++) {
        const CoverJob *c = &L->job[i];
        long score;
        if (c->req_gen == c->done_gen) continue;
        score = (c->tab == L->focus_tab ? 0L : 1000000L) + (long)(c->index > L->focus_index ? c->index - L->focus_index : L->focus_index - c->index);
        if (best < 0 || score < best_score) { best = i; best_score = score; }
    }
    if (best < 0) { unlock(L); return 0; }
    j = L->job[best];
    gen = j.req_gen;
    unlock(L);

    cover_tex_size(j.plat, &tw, &th);
    src[0] = '\0';
    year = 0;
    ok = load_job(L, &j, tw, th, src, sizeof(src), &year);
    dst = L->io.pixels ? L->io.pixels(best) : NULL;
    if (ok && !dst) ok = 0;
    if (ok) memcpy(dst, L->scratch, (size_t)tw * (size_t)th * 2u);

    if (L->n_logged < COVER_LOG_LINES) {
        L->n_logged++;
        log_printf("covers: %s %s <- %s", k_plat_dir[j.plat < 0 || j.plat >= PLAT_COUNT ? PLAT_PS3 : j.plat], j.path, src);
        if (L->n_logged == COVER_LOG_LINES) log_printf("covers: (further covers not logged)");
    }

    lock(L);
    if (L->job[best].req_gen == gen) {
        L->job[best].w = tw;
        L->job[best].h = th;
        L->job[best].done_ok = ok;
        L->job[best].done_year = year;
        L->job[best].done_gen = gen;
    }
    unlock(L);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Download Missing Covers (0.9.2)                                     */

static int id_ok(const char *id)
{
    int i;
    if (!id || strlen(id) < 8) return 0;
    for (i = 0; id[i]; i++) if (!sf_isalpha(id[i]) && !sf_isdigit(id[i])) return 0;
    return 1;
}

int cover_dl_urls(int plat, const char *id, char out[][COVER_URL_MAX], int max)
{
    int n = 0, r;
    if (!id_ok(id) || max <= 0) return 0;
    /* 0.9.4: first the server ManaGunZ downloads from (Zar's, plain HTTP, every
     * console): PS3 by its ID (BLUS30177), PS1/PS2 the way SYSTEM.CNF names
     * them (SLUS_209.14). ManaGunZ/source/main.c, Download_covers(). */
    if (plat == PLAT_BDISO) return 0;
    if (plat == PLAT_PS3) snprintf(out[n++], COVER_URL_MAX, "http://gamecovers.free.fr/download.php?file=%s.jpg", id);
    else if (plat == PLAT_PS1 || plat == PLAT_PS2) {
        char cnf[16];
        int i;
        snprintf(cnf, sizeof(cnf), "%.4s_%.3s.%.2s", id, id + 4, id + 7);
        for (i = 0; cnf[i]; i++) cnf[i] = (char)sf_toupper(cnf[i]);
        snprintf(out[n++], COVER_URL_MAX, "http://gamecovers.free.fr/download.php?file=%s.jpg", cnf);
    }
    if (plat == PLAT_PS3) {
        /* GameTDB's region folders; the ID's third letter is its region (BLUS, BLES, BLJM, BCKS, BLAS) */
        const char *regs[4];
        int nr = 0;
        char c = (char)sf_toupper(id[2]);
        if (c == 'E') { regs[nr++] = "EN"; regs[nr++] = "US"; }
        else if (c == 'J') { regs[nr++] = "JA"; regs[nr++] = "US"; }
        else if (c == 'K') { regs[nr++] = "KO"; regs[nr++] = "US"; }
        else if (c == 'A' || c == 'H') { regs[nr++] = "ZH"; regs[nr++] = "US"; regs[nr++] = "EN"; }
        else { regs[nr++] = "US"; regs[nr++] = "EN"; }
        for (r = 0; r < nr && n < max; r++)
            snprintf(out[n++], COVER_URL_MAX, "http://art.gametdb.com/ps3/cover/%s/%s.jpg", regs[r], id);
    } else if (plat == PLAT_PS1 || plat == PLAT_PS2) {
        char serial[16], pre[5];
        const char *sys = plat == PLAT_PS2 ? "ps2" : "ps1", *repo = plat == PLAT_PS2 ? "ps2-covers" : "psx-covers";
        int i;
        snprintf(serial, sizeof(serial), "%.4s-%s", id, id + 4);              /* SLUS20914 -> SLUS-20914 */
        for (i = 0; serial[i]; i++) serial[i] = (char)sf_toupper(serial[i]);
        snprintf(pre, sizeof(pre), "%.4s", serial);
        /* 0.9.3: RPDB's own HTTPS address first (plain HTTP redirects there: one round trip less) */
        if (n < max) snprintf(out[n++], COVER_URL_MAX, "https://rpdbs3.gerbilsoft.com/%s/cover/%s/%s.jpg", sys, pre, serial);
        if (n < max) snprintf(out[n++], COVER_URL_MAX, "http://rpdb.gerbilsoft.com/%s/cover/%s/%s.jpg", sys, pre, serial);
        if (n < max) snprintf(out[n++], COVER_URL_MAX, "https://raw.githubusercontent.com/xlenore/%s/main/covers/default/%s.jpg", repo, serial);
    }
    return n;
}

int cover_has_art(const CoverIO *io, int plat, const char *path, int is_dir, uint64_t size, const char *id)
{
    char f[COVER_PATH_MAX + 16];
    CoverCand *cand;
    int nc, c, e, found = 0;
    (void)size;
    if (!io || !io->is_file) return 0;
    /* Processed .cvr is NOT an authoritative cover. Legacy caches may
     * contain multiMAN's horizontal sidecars and must not block a fetch. */
    cand = (CoverCand *)malloc(sizeof(CoverCand) * COVER_MAX_CANDS);
    if (!cand) return 0;
    nc = cover_candidates(plat, path, is_dir, id, NULL, cand, COVER_MAX_CANDS);
    for (c = 0; c < nc && !found; c++)
        for (e = 0; e < SF_ARRAY_LEN(k_exts) && !found; e++) {
            snprintf(f, sizeof(f), "%s%s", cand[c].base, k_exts[e]);
            if (io->is_file(io->ctx, f)) found = 1;
        }
    free(cand);
    return found;
}

static int is_image(const uint8_t *b, uint32_t len, int *png)
{
    if (len >= 4 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) { *png = 0; return 1; }
    if (len >= 8 && memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0) { *png = 1; return 1; }
    return 0;
}

void cover_dl_run(CoverDl *d)
{
    const CoverIO *io = d->io;
    uint8_t *buf = (uint8_t *)malloc(COVER_DL_MAX_BYTES);
    int i, streak = 0, logged_tls = 0, odd = 0;
    char (*urls)[COVER_URL_MAX] = (char (*)[COVER_URL_MAX])malloc((size_t)COVER_DL_URLS * COVER_URL_MAX);
    if (!buf || !urls || !io || !d->http) {
        sf_strlcpy(d->note, "Not enough memory to download", sizeof(d->note));
        free(buf); free(urls);
        __sync_synchronize();
        d->finished = 1;
        return;
    }
    if (io->mkdir) io->mkdir(io->ctx, COVER_DL_DIR);
    /* On a manual retry, previous failed movie downloads must be retried.
     * This is not an API-key feature; public movie sources work without one. */
    if (!d->auto_mode && io->reset_movie_attempts)
        io->reset_movie_attempts(io->ctx);
    log_printf("covers: downloading missing covers (%d games and movies to check)", d->n);
    for (i = 0; i < d->n && !d->cancel; i++) {
        CoverDlItem *it = &d->item[i];
        int nu, u, got = 0, neterr = 0, offline = 1, not_found_only = 1;
        char miss_path[COVER_PATH_MAX];
        /* Movies have no PS3 game Title ID. Previously they always landed
         * in DL_NOID, so the Download Missing Covers button could NEVER
         * download any BDISO movie regardless of console Internet access.
         * Use the movie metadata/filename downloader directly instead. */
        if (it->plat == PLAT_BDISO) {
            int result;
            if (cover_has_art(io, it->plat, it->path, it->is_dir, it->size, it->id)) {
                it->state = DL_HAVE; d->n_have++;
                goto next;
            }
            if (!io->ensure_movie_art) {
                log_printf("bdiso: movie poster callback unavailable for %s", it->path);
                it->state = DL_ERROR; d->n_error++;
                goto next;
            }
            result = io->ensure_movie_art(io->ctx, it->path);
            if ((result == 0 || result == 1) &&
                cover_has_art(io, it->plat, it->path, it->is_dir, it->size, it->id)) {
                it->state = DL_GOT; d->n_got++;
                log_printf("bdiso: movie poster found/created for %s", it->path);
            } else {
                it->state = DL_ERROR; d->n_error++;
                log_printf("bdiso: movie cover download failed for %s, code %d",it->path,result);
            }
            goto next;
        }
        if (!it->id[0]) discid_read(it->plat, it->path, it->is_dir, io->read_at, io->ctx, it->id, sizeof(it->id));
        /* A name-matched manual cover counts even when an ISO has no ID. */
        if (cover_has_art(io, it->plat, it->path, it->is_dir, it->size, it->id)) { it->state = DL_HAVE; d->n_have++; goto next; }
        if (!id_ok(it->id)) { it->state = DL_NOID; d->n_noid++; goto next; }
        if (cover_name_is_custom(it->path)) {
            it->state = DL_MISSING; d->n_missing++; goto next;
        }
        snprintf(miss_path, sizeof(miss_path), "%s/%s.missing", COVER_DL_DIR, it->id);
        if (d->auto_mode && io->is_file && io->is_file(io->ctx, miss_path)) {
            it->state = DL_MISSING; d->n_missing++; goto next;
        }
        nu = cover_dl_urls(it->plat, it->id, urls, COVER_DL_URLS);
        for (u = 0; u < nu && !got && !d->cancel; u++) {
            char url[COVER_URL_MAX], loc[COVER_URL_MAX];
            uint32_t len = 0;
            int st = 0, hops, png = 0;
            sf_strlcpy(url, urls[u], sizeof(url));
            for (hops = 0; hops < 3; hops++) {
                loc[0] = '\0';
                st = d->http(d->http_ctx, url, buf, COVER_DL_MAX_BYTES, &len, loc, sizeof(loc));
                if ((st == 301 || st == 302 || st == 303 || st == 307 || st == 308) && loc[0]) { sf_strlcpy(url, loc, sizeof(url)); continue; }
                break;
            }
            if (st != 404) not_found_only = 0;
            if (st == COVER_HTTP_TLS) {
                d->tls_needed = 1;
                if (!logged_tls) { logged_tls = 1; log_printf("covers: %s needs HTTPS, which this build can't do", url); }
                continue;
            }
            if (st != COVER_HTTP_DNS) offline = 0;
            if (st < 0) { neterr = 1; log_printf("covers: %s: %s", url, st == COVER_HTTP_DNS ? "name not found (no internet?)" : st == COVER_HTTP_BIG ? "too big" : "no connection"); continue; }
            if (st == 200 && len > 64 && is_image(buf, len, &png)) {
                char f[COVER_PATH_MAX];
                snprintf(f, sizeof(f), "%s/%s.%s", COVER_DL_DIR, it->id, png ? "png" : "jpg");
                if (io->write_file && io->write_file(io->ctx, f, buf, len, "", 0) == 0) {
                    got = 1;
                    log_printf("covers: downloaded %s <- %s (%u bytes)", f, url, (unsigned)len);
                } else {
                    log_printf("covers: could not write %s", f);
                }
            } else if (st != 404 && odd++ < 5) {          /* a few, not one per game */
                log_printf("covers: %s: HTTP %d%s", url, st, st == 200 ? " (not an image)" : "");
            }
        }
        if (got) {
            it->state = DL_GOT; d->n_got++; streak = 0;
            if (io->remove) (void)io->remove(io->ctx, miss_path);
            if (io->cache_dir && io->remove) {             /* forget "this game has no cover" */
                char stem[COVER_PATH_MAX], f[COVER_PATH_MAX + 8];
                cover_cache_stem(io->cache_dir, it->plat, it->path, it->size, stem, sizeof(stem));
                snprintf(f, sizeof(f), "%s.none", stem);
                io->remove(io->ctx, f);
            }
        } else if (neterr) {
            it->state = DL_ERROR; d->n_error++;
            /* 0.9.3: give up early only when nothing can be reached at all (no
             * internet); a site failing (one console's covers) doesn't stop the rest */
            if (offline) streak++; else streak = 0;
            if (streak >= 3 && d->n_got == 0) {
                sf_strlcpy(d->note, "Couldn't reach the cover sites. Is the PS3 online?", sizeof(d->note));
                i++;
                __sync_synchronize();
                d->pos = i;
                break;
            }
        } else {
            it->state = DL_MISSING; d->n_missing++; streak = 0;
            if (not_found_only && io->write_file)
                (void)io->write_file(io->ctx, miss_path, "404", 3, "", 0);
        }
    next:
        __sync_synchronize();
        d->pos = i + 1;
    }
    /* once more: a cover thread may have marked a game "none" meanwhile */
    if (io->cache_dir && io->remove)
        for (i = 0; i < d->pos; i++) {
            char stem[COVER_PATH_MAX], f[COVER_PATH_MAX + 8];
            if (d->item[i].state != DL_GOT) continue;
            cover_cache_stem(io->cache_dir, d->item[i].plat, d->item[i].path, d->item[i].size, stem, sizeof(stem));
            snprintf(f, sizeof(f), "%s.none", stem);
            io->remove(io->ctx, f);
        }
    log_printf("covers: download done: %d added, %d not found online, %d already had one, %d with no title ID, %d failed%s%s",
               d->n_got, d->n_missing, d->n_have, d->n_noid, d->n_error, d->tls_needed ? " (some sources need HTTPS)" : "",
               d->cancel ? " (stopped)" : "");
    free(buf); free(urls);
    __sync_synchronize();
    d->finished = 1;
}

void cover_worker_main(void *arg)
{
    CoverLoader *L = (CoverLoader *)arg;
    L->running = 1;
    while (!L->quit) {
        if (!cover_worker_step(L) && L->io.sleep_ms) L->io.sleep_ms(L->io.ctx, 5);
    }
    log_printf("covers: worker stopped (%d from cache, %d from art, %d without a cover)", L->n_cache, L->n_art, L->n_none);
    L->running = 0;
}
