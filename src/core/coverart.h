/* coverart.h - find, decode, fit, cache and stream cover art.
 *
 * Authoritative art: /dev_hdd0/tmp/spookyflow/downloaded/
 *   - Manual (highest priority): <ISO filename without extension>.jpg/.png
 *   - Automatic for official games: <Title ID>.jpg/.png
 * An ISO's adjacent ICON0.PNG/sidecar, webMAN and multiMAN artwork caches
 * are deliberately ignored. This avoids treating 320x176 XMB icons as box art.
 *
 * Downloaded art is adapted using the original platform-specific dimensions
 * and cached as .cvr files for the worker. Versioned cache rejects old images.
 * Manually replacing an image requires Rescan Library to invalidate .cvr.
 * Launch/graphics pipeline is unaffected.
 */
#ifndef SF_COVERART_H
#define SF_COVERART_H

#include <stddef.h>
#include <stdint.h>
#include "cover_cache.h"
#include "discid.h"
#include "scanner.h"   /* FsListFn */

#define COVER_PATH_MAX   512
#define COVER_MAX_CANDS  24
#define COVER_SRC_MAX_DIM 2048     /* larger (decoded) images are skipped: memory stays bounded */
#define COVER_STRETCH_MAX 1.20f  /* shape difference stretched to fill; more is letterboxed */
#define COVER_LOG_LINES  400       /* per-cover log lines (then only a summary) */

typedef struct {
    ReadAtFn read_at;
    int (*is_file)(void *ctx, const char *path);
    /* Optional BDISO poster fetch; runs exclusively in the cover worker thread. */
    int (*ensure_movie_art)(void *ctx, const char *iso_path);
    /* Manual Download Missing Covers restarts unsuccessful movie lookups. */
    void (*reset_movie_attempts)(void *ctx);
    int (*is_dir)(void *ctx, const char *path);
    /* Decode a .jpg/.png into malloc'd RGBA bytes (R,G,B,A). 0 = ok. */
    int (*decode)(void *ctx, const char *path, int *w, int *h, uint8_t **rgba);
    /* Create/replace path with a then b. 0 = ok. */
    int (*write_file)(void *ctx, const char *path, const void *a, uint32_t alen, const void *b, uint32_t blen);
    int (*remove)(void *ctx, const char *path);
    int (*mkdir)(void *ctx, const char *path);
    FsListFn list;
    void (*sleep_ms)(void *ctx, int ms);
    void *ctx;
    const char *cache_dir;        /* NULL = no disk cache */
    uint16_t *(*pixels)(int slot);   /* slot texture memory (render.h r_cover_pixels) */
} CoverIO;

typedef struct {
    volatile int req_gen;   /* bumped by the main thread for every request */
    volatile int done_gen;  /* set by the worker when that request finished */
    volatile int done_ok;
    int done_year;          /* release year found for the game (0 = unknown) */
    int plat, tab, index, is_dir, w, h;
    uint64_t size;
    char path[COVER_PATH_MAX];
    char id[16];
} CoverJob;

typedef struct {
    CoverIO io;
    CoverJob job[CC_MAX_SLOTS];
    volatile int lock;
    volatile int focus_tab, focus_index;
    volatile int quit, running, wipe;
    int probed;             /* which cover folders exist (worker only) */
    unsigned char dir_ok[16];
    uint8_t *scratch;       /* worker: processed pixels before the copy into the slot */
    /* main thread: called by cover_poll for each finished slot (year may be 0) */
    void (*on_result)(void *user, int tab, int index, int year);
    void *on_result_user;
    /* worker statistics (logged) */
    int n_cache, n_art, n_none, n_logged;
} CoverLoader;

/* ---- pure helpers (unit tested) ---- */

/* Texture size for a platform's cover slot (fits R_COVER_MAX_W/H). */
void  cover_tex_size(int plat, int *w, int *h);
/* Width / height of the case's art area (physical proportions). */
float cover_art_aspect(int plat);

typedef struct { char base[COVER_PATH_MAX]; } CoverCand;   /* path without extension */
/* Fill candidate base paths in lookup order. dir_ok (may be NULL = all
 * folders exist) skips multiMAN/webMAN folders known to be missing.
 * Returns the count. */
int   cover_candidates(int plat, const char *game_path, int is_dir, const char *id,
                       const unsigned char *dir_ok, CoverCand *out, int max);
/* The exclusive SpookyFlow artwork source folder. */
int   cover_folder_count(void);
const char *cover_folder(int i);

/* Crop/fit src (w x h RGBA) to `aspect`, resize to tw x th, composite over
 * a dark backdrop and write dithered A1R5G5B5 (opaque). */
void  cover_process(const uint8_t *rgba, int w, int h, float aspect, uint16_t *dst, int tw, int th);
void  cover_process_fill(const uint8_t *rgba, int w, int h, float aspect, uint16_t *dst, int tw, int th);

/* Cache file name stem for a game: <cache>/<plat>/<16 hex>. */
void  cover_cache_stem(const char *cache_dir, int plat, const char *game_path, uint64_t size, char *out, size_t cap);

/* ---- Download Missing Covers (0.9.2) ----------------------------------
 * For every game with no cover in any of the folders above, the cover is
 * fetched by title ID and saved as COVER_DL_DIR/<ID>.jpg (or .png), where
 * the normal lookup finds it. Sources, in order:
 *   all:     ManaGunZ's       http://gamecovers.free.fr/download.php?file=<BLUS30177|SLUS_209.14>.jpg
 *   PS3:     GameTDB          http://art.gametdb.com/ps3/cover/<region>/<ID>.jpg
 *   PS1/PS2: RPDB (rom-properties' mirror of the xlenore collections)
 *                             https://rpdbs3.gerbilsoft.com/<ps1|ps2>/cover/<SLUS>/<SLUS-20914>.jpg
 *                             (http://rpdb.gerbilsoft.com/... redirects there)
 *            the xlenore collections on GitHub (HTTPS only)
 * The PS3's own SSL is too old for most sites, so HTTPS needs a build with
 * its own TLS; a source that only answers over HTTPS is reported once. */
#define COVER_DL_DIR       "/dev_hdd0/tmp/spookyflow/downloaded"
#define COVER_URL_MAX      512
#define COVER_DL_MAX_BYTES (3u * 1024u * 1024u)
#define COVER_DL_URLS      8

/* GET url. The body goes to buf (at most cap bytes), *len = its size;
 * loc = the Location header (redirects). Returns the HTTP status (200,
 * 404, 301...) or one of COVER_HTTP_* (< 0). */
typedef int (*CoverHttpFn)(void *ctx, const char *url, uint8_t *buf, uint32_t cap, uint32_t *len, char *loc, size_t loc_cap);
enum { COVER_HTTP_NET = -1, COVER_HTTP_TLS = -2, COVER_HTTP_BIG = -3, COVER_HTTP_DNS = -4 };

enum { DL_PENDING = 0, DL_HAVE, DL_NOID, DL_GOT, DL_MISSING, DL_ERROR };
typedef struct {
    int plat, is_dir;
    uint64_t size;
    char path[COVER_PATH_MAX];
    char id[16];
    volatile int state;          /* DL_* */
} CoverDlItem;

typedef struct {
    CoverDlItem *item;
    int n;
    const CoverIO *io;
    CoverHttpFn http;
    void *http_ctx;
    volatile int pos;            /* games looked at so far (item[0..pos) are final) */
    volatile int cancel, finished;
    int n_have, n_noid, n_got, n_missing, n_error;
    int auto_mode;               /* automatic background scan; respect known 404 */
    int tls_needed;              /* a source answered only over HTTPS */
    char note[96];               /* why it stopped early ("" = it didn't) */
} CoverDl;

/* The URLs to try for a game, best first. Returns the count. */
int  cover_dl_urls(int plat, const char *id, char out[][COVER_URL_MAX], int max);
/* PS1: exact filename+region fallback from Libretro's official boxart CDN.
 * No guessed game ID or fuzzy regional matching; percent-encode path safely. */
int  cover_dl_ps1_title_url(const char *game_path,char *out,size_t cap);
/* 1 if the game already has a cover (cache or any lookup folder). */
int  cover_has_art(const CoverIO *io, int plat, const char *path, int is_dir, uint64_t size, const char *id);
/* Thread body: goes through d->item, sets d->finished at the end. */
void cover_dl_run(CoverDl *d);

/* ---- loader (main thread) ---- */

void cover_loader_init(CoverLoader *L, const CoverIO *io);
/* Queue slot for (plat, path). id may be "" (read from the image). */
void cover_request(CoverLoader *L, int slot, int plat, int tab, int index, const char *path,
                   int is_dir, uint64_t size, const char *id);
/* Forget every queued request (library changed). */
void cover_cancel_all(CoverLoader *L);
/* Tell the worker which game is selected (nearest covers are loaded first). */
void cover_set_focus(CoverLoader *L, int tab, int index);
/* Mark finished slots in cc (defines their textures). Call once per frame. */
void cover_poll(CoverLoader *L, CoverCache *cc);
/* Delete the disk cache (worker does it before its next job). */
void cover_wipe_cache(CoverLoader *L);
/* Ask the worker to stop and wait for it (up to ~2 s). */
void cover_loader_stop(CoverLoader *L);

/* ---- worker ---- */

/* Thread body: loops until cover_loader_stop(). */
void cover_worker_main(void *loader);
/* One unit of work (wipe or one job). Returns 1 if it did something.
 * Host tests call this directly instead of running a thread. */
int  cover_worker_step(CoverLoader *L);

#endif
