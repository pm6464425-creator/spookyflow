/* SpookyFlow V15.53: independent PCSX-ReARMed PS3 graphics engine.
 * Only the new user-mode standalone PS1 APP SELF is verified.
 * Does not touch Sony's ps1_emu, flash, VM1 cards, ROMs or Sony settings.
 */
#include "sf_ps1_engine.h"
#include "ps3_platform.h"
#include "log.h"
#include "sf_ps1_engine_hash.h" /* generated only from native signed PSX SELF */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#ifdef SF_HAVE_TLS
#include <mbedtls/sha256.h>
#endif
#define SF_PS1_ENGINE_EXEC "/dev_hdd0/game/" SF_APPID "/USRDIR/PS1ENGINE/PS1_ENGINE.SELF"
static unsigned char s_ps1_hash_io[32768];
const char *sf_ps1_engine_path(void){return SF_PS1_ENGINE_EXEC;}
int sf_ps1_engine_ready(void)
{
#ifdef SF_HAVE_TLS
    FILE *f=fopen(SF_PS1_ENGINE_EXEC,"rb");
    mbedtls_sha256_context c;
    unsigned char hdr[128],hash[32]={0};
    uint64_t bytes=0;
    size_t n;
    int ok=1;
    if(!f){log_printf("PS1_DIAG ENGINE=ABSENT path=" SF_PS1_ENGINE_EXEC);return -91;}
    if(fread(hdr,1,sizeof(hdr),f)!=sizeof(hdr) ||
        memcmp(hdr,"SCE\x00",4)!=0 ||
        hdr[0x2f]!=0x70 || hdr[0x7f]!=0x04) {
        (void)fclose(f);
        log_printf("PS1_DIAG ENGINE=WRONG_SELF_TYPE");
        return -92;
    }
    if(fseek(f,0,SEEK_SET)!=0){(void)fclose(f);return -92;}
    mbedtls_sha256_init(&c);
    if(mbedtls_sha256_starts_ret(&c,0)!=0)ok=0;
    while(ok && (n=fread(s_ps1_hash_io,1,sizeof(s_ps1_hash_io),f))!=0){
        bytes+=n;
        if(bytes>SF_PS1_ENGINE_SIZE ||
            mbedtls_sha256_update_ret(&c,s_ps1_hash_io,n)!=0)ok=0;
    }
    if(ferror(f))ok=0;
    if(ok && mbedtls_sha256_finish_ret(&c,hash)!=0)ok=0;
    mbedtls_sha256_free(&c);
    if(fclose(f)!=0)ok=0;
    if(!ok || bytes!=SF_PS1_ENGINE_SIZE ||
       memcmp(hash,k_sf_ps1_engine_hash,32)!=0){
        log_printf("PS1_DIAG ENGINE=SELF_SHA_MISMATCH size=%llu expected=%llu",
                    (unsigned long long)bytes,(unsigned long long)SF_PS1_ENGINE_SIZE);
        return -93;
    }
    log_printf("PS1_DIAG ENGINE=VERIFIED_NATIVE_APP_SELF bytes=%llu sha256=" SF_PS1_ENGINE_SELF_SHA256,
               (unsigned long long)bytes);
    return 0;
#else
    log_printf("PS1_DIAG ENGINE=SHA256_SUPPORT_REQUIRED");
    return -94;
#endif
}
