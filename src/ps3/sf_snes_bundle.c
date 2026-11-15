/* SNES-only native pSNES 7.2 integration. Independent of webMAN/RetroArch.
 * pSNES is distributed with its own PS3 SELF and hardcodes the game data
 * folder /dev_hdd0/game/PSNE00001/USRDIR. The one SpookyFlow PKG carries
 * the original release payload in USRDIR/SNESENGINE, staged atomically on
 * first SNES use. Existing pSNES installations and user ROMs are untouched.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "ps3_platform.h"
#include "sf_snes_bundle.h"
#include "sf_psnes_filelist.h"
#include "log.h"
#ifdef SF_HAVE_TLS
#include <mbedtls/sha256.h>
#endif

#define PSNES_BUNDLE "/dev_hdd0/game/" SF_APPID "/USRDIR/SNESENGINE"
#define PSNES_HOME "/dev_hdd0/game/PSNE00001"
#define PSNES_EXEC PSNES_HOME "/USRDIR/EBOOT.BIN"
#define PSNES_STAGE SF_DATA_DIR "/psnes_v72_stage"
#define PSNES_MARKER PSNES_STAGE "/.spookyflow_psnes_7_2"

/* SHA-256 of EBOOT.BIN in theheroGAC/pemu-ps3, release 7.2 (Oct 30 2026). */
static const unsigned char k_eboot_sha256[32] = {
    0x6a,0x0c,0x3f,0x62,0xc9,0xb4,0xc4,0x57,
    0x15,0xe0,0x04,0x1d,0xe9,0xb7,0x68,0x86,
    0x52,0x61,0x4e,0x99,0x33,0xd0,0xe0,0xa4,
    0xd9,0xc6,0xf6,0xcc,0xb0,0xeb,0xd7,0x07
};
static unsigned char s_psnes_io[32768];

static int is_dir(const char *path)
{
    sysFSStat st;
    return sysFsStat(path, &st) == 0 &&
           (st.st_mode & 0170000) == 0040000;
}

static int mkdir_safe(const char *path)
{
    if (is_dir(path)) return 1;
    if (ps3_fs_exists(path)) return 0;
    return sysFsMkdir(path, 0777) == 0 && is_dir(path);
}

static int create_parents(const char *root, const char *relative)
{
    char path[512];
    size_t rootlen = strlen(root), j;
    if (rootlen + strlen(relative) + 2 >= sizeof(path)) return 0;
    strcpy(path, root);
    path[rootlen] = '/';
    strcpy(path + rootlen + 1, relative);
    for (j = rootlen + 1; path[j]; ++j) {
        if (path[j] == '/') {
            path[j] = '\0';
            if (!mkdir_safe(path)) return 0;
            path[j] = '/';
        }
    }
    return 1;
}

/* Fails closed without mbedTLS SHA256; an EBOOT with an unexpected checksum
 * is never launched. Does not modify any source or destination file. */
static int verified_eboot(const char *path)
{
#ifdef SF_HAVE_TLS
    unsigned char hash[32] = {0};
    mbedtls_sha256_context ctx;
    FILE *f;
    size_t n;
    unsigned long long total = 0;
    int ok = 1;
    f = fopen(path, "rb");
    if (!f) return 0;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts_ret(&ctx, 0) != 0) ok = 0;
    while (ok && (n = fread(s_psnes_io, 1, sizeof(s_psnes_io), f)) != 0) {
        total += n;
        if (total > 2594800ULL ||
            mbedtls_sha256_update_ret(&ctx, s_psnes_io, n) != 0) ok = 0;
    }
    if (ferror(f)) ok = 0;
    if (ok && mbedtls_sha256_finish_ret(&ctx, hash) != 0) ok = 0;
    mbedtls_sha256_free(&ctx);
    (void)fclose(f);
    return ok && total == 2594800ULL &&
           memcmp(hash, k_eboot_sha256, sizeof(hash)) == 0;
#else
    (void)path;
    return 0;
#endif
}

/* V15.41: launch the genuine standalone APP SELF directly from
 * SpookyFlow/USRDIR. An NPDRM EBOOT.BIN is not valid for sysProcessExitSpawn2;
 * even though it exists, its dispatch can silently return to XMB.
 * This original pSNES code was independently decoded and signed with
 * PSL1GHT make_self (type 4 APP) at build time. We verify the exact
 * output expected from the official pSNES 7.2 ELF.
 */
#define PSNES_PLAIN_SELF PSNES_BUNDLE "/PSNES_ENGINE.SELF"
#include "sf_psnes_plain_hash.h"  /* generated during build from real APP SELF */

int sf_psnes_plain_ready(void)
{
#ifdef SF_HAVE_TLS
    FILE *f;
    mbedtls_sha256_context ctx;
    unsigned char hash[32]={0};
    unsigned long long total=0;
    size_t n;
    int ok=1, stage=0;
    unsigned char hdr[128] = {0};
    f=fopen(PSNES_PLAIN_SELF,"rb");
    if(!f) {
        log_printf("snes: DIAG_APP_SELF_ERROR=FILE_NOT_PRESENT");
        return -80;
    }
    if(fread(hdr,1,sizeof(hdr),f)!=sizeof(hdr) ||
       memcmp(hdr,"SCE\x00",4)!=0 ||
       hdr[0x2f]!=0x70 || hdr[0x7f]!=0x04) {
        log_printf("snes: DIAG_APP_SELF_ERROR=INVALID_APP_SELF_HEADER");
        (void)fclose(f);
        return -81;
    }
    if(fseek(f,0,SEEK_SET)!=0) {
        (void)fclose(f);
        return -81;
    }
    mbedtls_sha256_init(&ctx);
    if(mbedtls_sha256_starts_ret(&ctx,0)!=0)ok=0;
    while(ok && (n=fread(s_psnes_io,1,sizeof(s_psnes_io),f))!=0) {
        total += n;
        if(total>PSNES_PLAIN_SIZE ||
           mbedtls_sha256_update_ret(&ctx,s_psnes_io,n)!=0)ok=0;
    }
    if(ferror(f))ok=0;
    if(ok && mbedtls_sha256_finish_ret(&ctx,hash)!=0)ok=0;
    mbedtls_sha256_free(&ctx);
    if(fclose(f)!=0)ok=0;
    stage=ok && total==PSNES_PLAIN_SIZE &&
          memcmp(hash,k_plain_sha256,sizeof(hash))==0;
    if(!stage) {
        log_printf("snes: DIAG_APP_SELF_ERROR=SHA256_OR_SIZE_MISMATCH bytes=%llu",
                   total);
        return -82;
    }
    log_printf("snes: DIAG_APP_SELF=VERIFIED type=APP non_npdrm=YES bytes=%llu",
               total);
    return 0;
#else
    log_printf("snes: DIAG_APP_SELF_ERROR=SHA256_UNAVAILABLE");
    return -82;
#endif
}

/* Copy a SHA-pinned package resource into private stage only. */
static int copy_asset(const char *rel, unsigned long long expected)
{
    char source[512], target[512];
    FILE *in, *out;
    unsigned long long total = 0;
    size_t n;
    int ok = 1;
    if (snprintf(source, sizeof(source), "%s/%s", PSNES_BUNDLE, rel) >= (int)sizeof(source) ||
        snprintf(target, sizeof(target), "%s/%s", PSNES_STAGE, rel) >= (int)sizeof(target))
        return -1;
    if (!create_parents(PSNES_STAGE, rel)) return -1;
    in = fopen(source, "rb");
    if (!in) return -1;
    out = fopen(target, "wb");
    if (!out) { (void)fclose(in); return -1; }
    while ((n = fread(s_psnes_io, 1, sizeof(s_psnes_io), in)) != 0) {
        total += n;
        if (total > expected || fwrite(s_psnes_io, 1, n, out) != n) {
            ok = 0; break;
        }
    }
    if (ferror(in) || total != expected) ok = 0;
    if (fflush(out) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;
    if (fclose(in) != 0) ok = 0;
    if (!ok) { (void)remove(target); return -1; }
    return 0;
}

/* Negative result means SpookyFlow remains open and shows an error.
 * -74: foreign/partial existing PSNE00001, never overwrite
 * -75: foreign staging dir, never overwrite
 * -73: bundled file read/copy failure
 * -76: bundled EBOOT corrupt or wrong executable
 * -77: atomic activation failed
 */
int sf_psnes_prepare(void)
{
    unsigned i;
    FILE *mark;
    char marker[48] = {0};
    static const char stamp[] = "SpookyFlow-PS3-pSNES7.2-bundle-v1\n";

    log_printf("snes: DIAG_BUNDLE_PRECHECK home=%s installed=%d", PSNES_HOME, ps3_fs_exists(PSNES_HOME));
    if (ps3_fs_exists(PSNES_HOME)) {
        if (verified_eboot(PSNES_EXEC)) {
            log_printf("snes: DIAG_ENGINE_STATUS=INSTALLED verified_sha256=YES");
            log_printf("snes: verified pSNES 7.2 already installed; no overwrite");
            return 0;
        }
        log_printf("snes: DIAG_ENGINE_STATUS=INCOMPATIBLE verified_sha256=NO rc=-74");
        log_printf("snes: PSNE00001 exists but differs; untouched");
        return -74;
    }
    if (!is_dir(SF_DATA_DIR)) {
        log_printf("snes: DIAG_STAGE_ERROR=DATA_DIRECTORY_MISSING rc=-72");
        return -72;
    }
    if (ps3_fs_exists(PSNES_STAGE)) {
        mark = fopen(PSNES_MARKER, "rb");
        if (!mark) {
            log_printf("snes: DIAG_STAGE_ERROR=FOREIGN_STAGING_DIRECTORY rc=-75");
            return -75;
        }
        if (fread(marker, 1, strlen(stamp), mark) != strlen(stamp) ||
            memcmp(marker, stamp, strlen(stamp)) != 0) {
            (void)fclose(mark);
            log_printf("snes: DIAG_STAGE_ERROR=WRONG_STAGE_MARKER rc=-75");
            return -75;
        }
        (void)fclose(mark);
    } else {
        int written_ok;
        if (!mkdir_safe(PSNES_STAGE)) {
            log_printf("snes: DIAG_STAGE_ERROR=CANNOT_CREATE_STAGE rc=-72");
            return -72;
        }
        mark = fopen(PSNES_MARKER, "wb");
        if (!mark) return -72;
        written_ok = fwrite(stamp, 1, sizeof(stamp)-1, mark) == sizeof(stamp)-1;
        if (fclose(mark) != 0 || !written_ok) return -72;
    }

    for (i = 0; i < (unsigned)SF_PSNES_FILE_COUNT; ++i) {
        if (copy_asset(sf_psnes_files[i].relative, sf_psnes_files[i].size) != 0) {
            log_printf("snes: DIAG_STAGE_COPY_ERROR item=%u/%u file=%s rc=-73",
                       (unsigned)(i+1), (unsigned)SF_PSNES_FILE_COUNT,
                       sf_psnes_files[i].relative);
            log_printf("snes: missing bundled asset %s", sf_psnes_files[i].relative);
            return -73;
        }
    }
    log_printf("snes: DIAG_STAGE_COPIED files=%u", (unsigned)SF_PSNES_FILE_COUNT);
    if (!verified_eboot(PSNES_STAGE "/USRDIR/EBOOT.BIN")) {
        log_printf("snes: DIAG_STAGE_HASH_ERROR=EBOOT_SHA256_MISMATCH rc=-76");
        log_printf("snes: embedded pSNES EBOOT failed SHA256 check");
        return -76;
    }
    if (ps3_fs_exists(PSNES_HOME) || rename(PSNES_STAGE, PSNES_HOME) != 0) {
        log_printf("snes: DIAG_ACTIVATION_ERROR=RENAME_FAILED_OR_COLLISION rc=-77");
        log_printf("snes: could not atomically activate pSNES 7.2");
        return -77;
    }
    if (!verified_eboot(PSNES_EXEC)) {
        log_printf("snes: DIAG_ACTIVATION_ERROR=EBOOT_HASH_AFTER_ACTIVATION rc=-78");
        return -78;
    }
    log_printf("snes: DIAG_ENGINE_STATUS=READY_FROM_BUNDLE engine=%s", PSNES_EXEC);
    log_printf("snes: bundled pSNES 7.2 ready (%u assets, no webMAN)",
               (unsigned)SF_PSNES_FILE_COUNT);
    return 0;
}
