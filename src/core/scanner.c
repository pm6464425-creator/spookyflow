/* scanner.c - incremental library scanner. */
#include "scanner.h"
#include "title.h"
#include "movie_spanish.h"
#include "log.h"
#include "util.h"
#include "sfo.h"
#include <stdio.h>
#include <string.h>

static const ScanRoot k_roots[] = {
    { PLAT_PS1, "/dev_hdd0/PSXISO",   SRC_HDD_ISO_DIR },
    { PLAT_PS1, "/dev_hdd0/PSXGAMES", SRC_HDD_ISO_DIR },
    { PLAT_PS2, "/dev_hdd0/PS2ISO",   SRC_HDD_ISO_DIR },
    { PLAT_PS3, "/dev_hdd0/PS3ISO",   SRC_HDD_ISO_DIR },
    { PLAT_PS3, "/dev_hdd0/GAMES",    SRC_HDD_GAME_FOLDERS },
    { PLAT_PS3, "/dev_hdd0/GAMEZ",    SRC_HDD_GAME_FOLDERS },
    { PLAT_BDISO, "/dev_hdd0/BDISO", SRC_HDD_ISO_DIR },
    { PLAT_PS3, NTFS_INDEX_DIR,        SRC_NTFS_INDEX },       /* NTFS drives (PS3 + PS1), via webMAN */
};

/* Scratch buffers (single scanner instance; keeps the stack small). */
static FsEntry s_ent[SCAN_DIR_MAX];
#define CUE_MAX 256
static char s_cue_title[CUE_MAX][TITLE_MAX];
static int s_cue_game[CUE_MAX];

const ScanRoot *scanner_default_roots(int *n)
{
    *n = SF_ARRAY_LEN(k_roots);
    return k_roots;
}

/* Some PS3 NTFS devices are available through native libntfs but their
 * /dev_usb000 path is NOT exposed to lv2 sysFs.  V15.4 hardware diagnosis
 * found exactly this: the NTFS volume and BDISO are readable, but the
 * traditional USB-root probe excluded the media from the scan roots.
 *
 * Pass native_ready_mask from the PS3 host to inject BDISO-only scan roots
 * for those ports, without inventing USB paths for PS1/PS2/PS3. The
 * original 6 USB game root types are unchanged when lv2 detects a device. */
const ScanRoot *scanner_roots_with_usb_native(const ScanRoot *base, int nbase,
                                              FsListFn list, void *ctx,
                                              uint32_t native_ready_mask,
                                              int *n, int *ndrives)
{
    static const struct { const char *dir; int plat, kind; } k_usb[] = {
        { "PS3ISO", PLAT_PS3, SRC_HDD_ISO_DIR }, { "GAMES", PLAT_PS3, SRC_HDD_GAME_FOLDERS },
        { "GAMEZ", PLAT_PS3, SRC_HDD_GAME_FOLDERS }, { "PS2ISO", PLAT_PS2, SRC_HDD_ISO_DIR },
        { "PSXISO", PLAT_PS1, SRC_HDD_ISO_DIR }, { "PSXGAMES", PLAT_PS1, SRC_HDD_ISO_DIR },
        { "BDISO", PLAT_BDISO, SRC_HDD_ISO_DIR },
    };
    enum { NU = (int)(sizeof(k_usb) / sizeof(k_usb[0])), MAXR = 32 + SCAN_USB_PORTS * NU };
    static ScanRoot roots[MAXR];
    static char paths[SCAN_USB_PORTS * NU][32];
    int i, p, m = 0, np = 0;
    *ndrives = 0;
    for (i = 0; i < nbase && m < 32; i++) roots[m++] = base[i];
    for (p = 0; p < SCAN_USB_PORTS && list; p++) {
        char dev[16];
        FsEntry one;
        snprintf(dev, sizeof(dev), "/dev_usb%03d", p);
        /* Native NTFS is not necessarily visible via lv2.  When a native
         * BDISO partition is mounted, add its BDISO root even if the lv2
         * /dev_usb00N probe returned 0 or -1. The filesystem listing of
         * /dev_usb00N/BDISO is resolved independently by the PS3 host. */
        int lv2_visible = list(ctx, dev, &one, 1) > 0;
        int ntfs_bdiso_ready = p < 8 &&
            (native_ready_mask & ((uint32_t)1u << p)) != 0;
        if (!lv2_visible && !ntfs_bdiso_ready) continue;
        (*ndrives)++;
        if (ntfs_bdiso_ready && !lv2_visible)
            log_printf("scan: native-only NTFS USB%03d: injecting /BDISO root (lv2 root absent)",p);
        else
            log_printf("scan: USB drive %s (native BDISO=%d)", dev,ntfs_bdiso_ready);
        for (i = 0; i < NU; i++) {
            /* On an NTFS-only path, never add fake game roots; retain
             * existing PS1/PS2/PS3 behavior and avoid unnecessary probes. */
            if (!lv2_visible && k_usb[i].plat != PLAT_BDISO) continue;
            snprintf(paths[np], sizeof(paths[np]), "%s/%s", dev, k_usb[i].dir);
            roots[m].plat = k_usb[i].plat;
            roots[m].path = paths[np];
            roots[m].kind = k_usb[i].kind;
            m++; np++;
        }
    }
    *n = m;
    return roots;
}

/* Portable original API unchanged for previews, tests and other hosts.
 * The PS3 host supplies native-ready ports with the dedicated variant. */
const ScanRoot *scanner_roots_with_usb(const ScanRoot *base, int nbase,
                                      FsListFn list, void *ctx,
                                      int *n, int *ndrives)
{
    return scanner_roots_with_usb_native(base,nbase,list,ctx,0,n,ndrives);
}

void scanner_begin(Scanner *s, const ScanRoot *roots, int nroots)
{
    memset(s, 0, sizeof(*s));
    s->roots = roots;
    s->nroots = nroots;
}

void scanner_set_reader(Scanner *s, ReadAtFn rd, void *ctx)
{
    s->rd = rd;
    s->rd_ctx = ctx;
}

void scanner_set_bdiso_pointer_filter(Scanner *s, int (*online)(const unsigned char *, size_t))
{
    s->ntfs_bdiso_online = online;
}

int scanner_ntfs_kind(const char *name, char *stem, size_t cap)
{
    static const struct { const char *tag; int plat; } k[] = {
        { ".ntfs[PS3ISO]", PLAT_PS3 }, { ".ntfs[PSXISO]", PLAT_PS1 }, { ".ntfs[BDISO]", PLAT_BDISO },
    };
    size_t n = name ? strlen(name) : 0;
    int i;
    if (cap) stem[0] = '\0';
    for (i = 0; i < SF_ARRAY_LEN(k); i++) {
        size_t lt = strlen(k[i].tag);
        if (n > lt && sf_ends_with_i(name, k[i].tag)) {
            size_t sl = n - lt;
            if (sl + 1 > cap) return -1;
            memcpy(stem, name, sl);
            stem[sl] = '\0';
            return k[i].plat;
        }
    }
    return -1;
}

static uint32_t be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

uint64_t scanner_ntfs_size(const unsigned char *buf, size_t len)
{
    uint32_t ns, i;
    uint64_t sectors = 0;
    if (!buf || len < 20) return 0;
    ns = be32(buf + 12);
    if (ns == 0 || ns > 16384 || 20u + 8u * (size_t)ns > len) return 0;
    for (i = 0; i < ns; i++) sectors += be32(buf + 20 + 4 * (size_t)ns + 4 * (size_t)i);
    return sectors * NTFS_SECTOR;
}

static int join(char *out, size_t cap, const char *dir, const char *name);

/* webMAN's NTFS index: one pointer file per game (see scanner.h) */
static unsigned char s_ntfs_buf[64 * 1024];
static void scan_ntfs_index(Scanner *s, Library *lib, const char *dir, int n)
{
    int i;
    char path[SCAN_PATH_MAX], stem[FS_NAME_MAX], raw[FS_NAME_MAX + 24];
    for (i = 0; i < n; i++) {
        const FsEntry *e = &s_ent[i];
        uint64_t size = 0;
        char id[64] = "";
        int plat, got = 0;
        if (e->is_dir || e->name[0] == '.') continue;
        plat = scanner_ntfs_kind(e->name, stem, sizeof(stem));
        if (plat < 0 || !join(path, sizeof(path), dir, e->name)) continue;
        if (s->rd) {
            got = s->rd(s->rd_ctx, path, 0, s_ntfs_buf, sizeof(s_ntfs_buf));
            if (got > 0) size = scanner_ntfs_size(s_ntfs_buf, (size_t)got);
            /* Accept old/short webMAN BDISO pointer files when a physical
             * NTFS USB is present. V15.2's got<20 hard rejection removed
             * valid movies on real consoles; the PS3 callback decides. */
            if (plat == PLAT_BDISO && s->ntfs_bdiso_online &&
                !s->ntfs_bdiso_online(got > 0 ? s_ntfs_buf : NULL,
                                      got > 0 ? (size_t)got : 0u)) {
                log_printf("bdiso: skipping offline webMAN movie %s", e->name);
                continue;
            }
            if (plat == PLAT_PS3) {                    /* title ID from the PARAM.SFO webMAN extracted */
                char sfo[SCAN_PATH_MAX];
                if (snprintf(sfo, sizeof(sfo), "%s/%s.SFO", dir, stem) < (int)sizeof(sfo)) {
                    got = s->rd(s->rd_ctx, sfo, 0, s_ntfs_buf, sizeof(s_ntfs_buf));
                    if (got > 0 && sfo_get_str(s_ntfs_buf, (size_t)got, "TITLE_ID", id, sizeof(id)) != 0) id[0] = '\0';
                }
            }
        }
        if (plat == PLAT_BDISO && s->ntfs_bdiso_online && !s->rd &&
            !s->ntfs_bdiso_online(NULL, 0)) continue;
        /* the display name: the stem as an .iso (webMAN may keep the ".iso"),
         * with the title ID as a tag so the library picks it up */
        {
            size_t sl = strlen(stem);
            if (sl > 4 && sf_ends_with_i(stem, ".iso")) stem[sl - 4] = '\0';
            else if (sl > 4 && (sf_ends_with_i(stem, ".bin") || sf_ends_with_i(stem, ".img"))) stem[sl - 4] = '\0';
        }
        if (id[0]) snprintf(raw, sizeof(raw), "%s [%s].iso", stem, id);
        else snprintf(raw, sizeof(raw), "%s.iso", stem);
        if (lib_add(lib, plat, path, raw, size, NULL, 0, 0, 0) >= 0) { s->found[plat]++; s->found_ntfs++; }
    }
}

static int join(char *out, size_t cap, const char *dir, const char *name)
{
    int n = snprintf(out, cap, "%s/%s", dir, name);
    return n > 0 && (size_t)n < cap;
}

/* ".iso" or ".iso.N" -> returns N (0 for plain .iso is reported as -1) */
static int iso_part(const char *name, size_t *stem_len)
{
    size_t n = strlen(name), k = n;
    while (k > 0 && sf_isdigit(name[k - 1])) k--;
    if (k < n && k >= 5 && name[k - 1] == '.' && sf_strnicmp(name + k - 5, ".iso", 4) == 0) {
        int v = 0; size_t i;
        for (i = k; i < n && v < 100000; i++) v = v * 10 + (name[i] - '0');
        *stem_len = k;          /* includes the trailing '.' */
        return v;
    }
    if (sf_ends_with_i(name, ".iso")) { *stem_len = n; return -1; }
    return -2;
}

static int is_ps1_image(const char *name)
{
    return sf_ends_with_i(name, ".bin") || sf_ends_with_i(name, ".img") ||
           sf_ends_with_i(name, ".iso") || sf_ends_with_i(name, ".mdf");
}

/* For BDISO, translated titles can be stored on the SAME USB volume in
 * BDISO/COVERS/<exact ISO stem>.title (UTF-8, one line).
 * The original English filename remains untouched and determines 3D status. */
static int bd_movie_sidecar_title(Scanner *s, const char *dir,
                                 const char *filename, char *out, size_t cap)
{
    char stem[FS_NAME_MAX],file[SCAN_PATH_MAX],raw[TITLE_MAX+16];
    size_t n;
    int got,i;
    if(!s->rd || cap<8) return 0;
    sf_strlcpy(stem,filename,sizeof(stem));
    n=strlen(stem);
    if(n>6 && sf_ends_with_i(stem,".iso.0")) stem[n-6]=0;
    else if(n>4 && sf_ends_with_i(stem,".iso"))stem[n-4]=0;
    else return 0;
    if(snprintf(file,sizeof(file),"%s/COVERS/%s.title",dir,stem)>=(int)sizeof(file))return 0;
    got=s->rd(s->rd_ctx,file,0,raw,sizeof(raw)-1);
    if(got<2 || got>=TITLE_MAX)return 0;
    raw[got]=0;
    if(got>=3 && (unsigned char)raw[0]==0xef &&
       (unsigned char)raw[1]==0xbb && (unsigned char)raw[2]==0xbf)
        memmove(raw,raw+3,(size_t)got-2);
    for(i=0;raw[i];i++) {
        if(raw[i]=='\n' || raw[i]=='\r') {raw[i]=0;break;}
        if((unsigned char)raw[i]<32 || raw[i]=='/' || raw[i]=='\\')return 0;
    }
    if(strlen(raw)<2)return 0;
    sf_strlcpy(out,raw,cap);
    return 1;
}
/* A webMAN .ntfs[BDISO] pointer and its native USB listing represent the
 * SAME movie. If native access works, prefer it so the external COVERS path
 * can be used. Compare original file stems, not translated display titles. */
static int bd_movie_pointer_duplicate(const Library *lib,const char *name)
{
    const GameList *gl=&lib->tab[PLAT_BDISO];
    char wanted[FS_NAME_MAX];
    int i;
    size_t n;
    sf_strlcpy(wanted,name,sizeof(wanted));
    n=strlen(wanted);
    if(n>6 && sf_ends_with_i(wanted,".iso.0"))wanted[n-6]=0;
    else if(n>4 && sf_ends_with_i(wanted,".iso"))wanted[n-4]=0;
    for(i=0;i<gl->count;i++) {
        const char *p=lib_path(lib,&gl->games[i]),*base;
        char prev[FS_NAME_MAX];
        size_t k;
        if(strncmp(p,NTFS_INDEX_DIR "/",sizeof(NTFS_INDEX_DIR))!=0)continue;
        base=strrchr(p,'/');
        base=base?base+1:p;
        sf_strlcpy(prev,base,sizeof(prev));
        k=strlen(prev);
        if(k>=12 && sf_ends_with_i(prev,".ntfs[BDISO]"))
            prev[k-12]=0;
        if(sf_ends_with_i(prev,".iso"))prev[strlen(prev)-4]=0;
        if(sf_stricmp(prev,wanted)==0)return i;
    }
    return -1;
}

static void scan_iso_dir(Scanner *s, Library *lib, int plat, const char *dir, int n, int depth)
{
    int i, j, ncue = 0;
    char path[SCAN_PATH_MAX];

    for (i = 0; i < n; i++) {
        const FsEntry *e = &s_ent[i];
        if (e->name[0] == '.' ) continue;
        if (e->is_dir) {
            if (plat == PLAT_BDISO && sf_stricmp(e->name,"COVERS") == 0) continue;
            if (depth == 0 && s->qn < SCAN_QUEUE_MAX && join(path, sizeof(path), dir, e->name))
                sf_strlcpy(s->queue[s->qn++], path, SCAN_PATH_MAX);
            continue;
        }
        if (plat == PLAT_PS1) {
            if (sf_ends_with_i(e->name, ".cue") && join(path, sizeof(path), dir, e->name)) {
                int gi = lib_add(lib, plat, path, e->name, e->size, NULL, 0, 0, 0);
                if (gi >= 0) {
                    s->found[plat]++;
                    if (ncue < CUE_MAX) {
                        title_clean(e->name, s_cue_title[ncue], TITLE_MAX);
                        s_cue_game[ncue] = gi;
                        ncue++;
                    }
                }
            }
        } else {
            size_t stem;
            int part = iso_part(e->name, &stem);
            if (part == -1 || part == 0) {
                uint64_t size = e->size;
                if (part == 0) {      /* split image: sum .iso.1, .iso.2 ... */
                    for (j = 0; j < n; j++) {
                        size_t st2;
                        int p2;
                        if (j == i || s_ent[j].is_dir) continue;
                        p2 = iso_part(s_ent[j].name, &st2);
                        if (p2 > 0 && st2 == stem && strncmp(s_ent[j].name, e->name, stem) == 0) size += s_ent[j].size;
                    }
                }
                if (join(path, sizeof(path), dir, e->name)) {
                    char spanish[TITLE_MAX],titlebuf[TITLE_MAX+4];
                    const char *display=e->name;
                    int idx,dup=-1;
                    if(plat==PLAT_BDISO){
                        char sidecar[TITLE_MAX]="";
                        int have_sidecar=bd_movie_sidecar_title(s,dir,e->name,
                                                                  sidecar,sizeof(sidecar));
                        /* A V7 auto-generated English .title must not replace
                         * an available Spanish display title.  Custom
                         * Spanish .title files still take precedence. */
                        if(movie_spanish_title(e->name,
                            have_sidecar?sidecar:NULL,spanish,sizeof(spanish))){
                            snprintf(titlebuf,sizeof(titlebuf),"%s.iso",spanish);
                            display=titlebuf;
                        }else if(have_sidecar){
                            snprintf(titlebuf,sizeof(titlebuf),"%s.iso",sidecar);
                            display=titlebuf;
                        }
                        dup=bd_movie_pointer_duplicate(lib,e->name);
                    }
                    idx=lib_add(lib,plat,path,display,size,NULL,0,0,0);
                    if(idx>=0){
                        s->found[plat]++;
                        if(dup>=0 && dup<idx){
                            GameList *gl=&lib->tab[PLAT_BDISO];
                            memmove(&gl->games[dup],&gl->games[dup+1],
                                    (size_t)(idx-dup)*sizeof(Game));
                            gl->count--;
                            s->found[plat]--;
                        }
                    }
                }
            }
        }
    }

    if (plat == PLAT_PS1) {
        /* .bin/.img: attach to a .cue with the same cleaned title, else standalone */
        for (i = 0; i < n; i++) {
            const FsEntry *e = &s_ent[i];
            char t[TITLE_MAX];
            int matched = 0;
            if (e->is_dir || e->name[0] == '.' || !is_ps1_image(e->name)) continue;
            title_clean(e->name, t, sizeof(t));
            for (j = 0; j < ncue; j++) {
                if (sf_stricmp(t, s_cue_title[j]) == 0) {
                    lib->tab[plat].games[s_cue_game[j]].size += e->size;
                    matched = 1;
                    break;
                }
            }
            if (!matched && join(path, sizeof(path), dir, e->name) &&
                lib_add(lib, plat, path, e->name, e->size, NULL, 0, 0, 0) >= 0)
                s->found[plat]++;
        }
    }
}

uint64_t scanner_dir_size(FsListFn list, void *ctx, const char *dir, FsEntry *ent,
                          char (*queue)[SCAN_PATH_MAX], int qmax, volatile const int *cancel,
                          int *ndirs, int *skipped)
{
    uint64_t total = 0;
    int head = 0, count = 0, i, n;
    if (!list || !dir || !ent || !queue || qmax <= 0) return 0;
    sf_strlcpy(queue[0], dir, SCAN_PATH_MAX);
    count = 1;
    while (count > 0) {
        char cur[SCAN_PATH_MAX];
        if (cancel && *cancel) break;
        memcpy(cur, queue[head], SCAN_PATH_MAX);
        head = (head + 1) % qmax;
        count--;
        n = list(ctx, cur, ent, SCAN_DIR_MAX);
        if (ndirs) (*ndirs)++;
        for (i = 0; i < n; i++) {
            if (!ent[i].is_dir) { total += ent[i].size; continue; }
            if (ent[i].name[0] == '.') continue;
            if (count < qmax && join(queue[(head + count) % qmax], SCAN_PATH_MAX, cur, ent[i].name)) count++;
            else if (skipped) (*skipped)++;
        }
    }
    return total;
}

static void scan_folders(Scanner *s, Library *lib, int plat, const char *dir, int n)
{
    int i;
    char path[SCAN_PATH_MAX];
    for (i = 0; i < n; i++) {
        const FsEntry *e = &s_ent[i];
        if (!e->is_dir || e->name[0] == '.') continue;
        /* size: added after loading by a background walk (0.9.0, app.c sizes_*) */
        if (join(path, sizeof(path), dir, e->name) &&
            lib_add(lib, plat, path, e->name, 0, NULL, 0, 0, GAME_DIR) >= 0)
            s->found[plat]++;
    }
}

static void process(Scanner *s, Library *lib, const ScanRoot *r, const char *dir, int depth, FsListFn list, void *ctx)
{
    int n = list(ctx, dir, s_ent, SCAN_DIR_MAX);
    s->dirs_listed++;
    if (n < 0) {
        if (depth == 0) log_printf("scan: %s not found", dir);
        else { log_printf("scan: cannot open %s", dir); s->errors++; }
        return;
    }
    if (n >= SCAN_DIR_MAX) log_printf("scan: %s has more than %d entries; extra ignored", dir, SCAN_DIR_MAX);
    log_printf("scan: %s (%d entries)", dir, n);
    switch (r->kind) {
    case SRC_HDD_GAME_FOLDERS: if (depth == 0) scan_folders(s, lib, r->plat, dir, n); break;
    case SRC_NTFS_INDEX: if (depth == 0) scan_ntfs_index(s, lib, dir, n); break;
    case SRC_HDD_ISO_DIR:
    default: scan_iso_dir(s, lib, r->plat, dir, n, depth); break;
    }
}

int scanner_step(Scanner *s, Library *lib, FsListFn list, void *ctx)
{
    const ScanRoot *r;
    if (s->done) return 1;
    if (s->root_i >= s->nroots) { s->done = 1; return 1; }
    r = &s->roots[s->root_i];
    if (s->phase == 0) {
        s->qn = 0; s->qi = 0;
        process(s, lib, r, r->path, 0, list, ctx);
        s->phase = 1;
    } else if (s->qi < s->qn) {
        process(s, lib, r, s->queue[s->qi], 1, list, ctx);
        s->qi++;
    } else {
        s->root_i++;
        s->phase = 0;
    }
    if (s->root_i >= s->nroots) s->done = 1;
    return s->done;
}

int scanner_platform(const Scanner *s)
{
    if (s->nroots <= 0) return PLAT_PS3;
    if (s->root_i >= s->nroots) return s->roots[s->nroots - 1].plat;
    return s->roots[s->root_i].plat;
}

float scanner_progress(const Scanner *s)
{
    float sub = 0.0f;
    if (s->nroots <= 0 || s->done) return 1.0f;
    if (s->phase == 1) sub = (float)(s->qi + 1) / (float)(s->qn + 1);
    return sf_clampf(((float)s->root_i + sub) / (float)s->nroots, 0.0f, 1.0f);
}
