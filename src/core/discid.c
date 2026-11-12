/* discid.c - title IDs from inside disc images (see discid.h). */
#include "discid.h"
#include "layout.h"   /* PLAT_* */
#include "sfo.h"
#include "title.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#define SECTOR        2048u
#define ROOT_MAX_SECT 8u        /* root directory sectors examined */

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int discid_parse_system_cnf(const char *txt, size_t len, char *out, size_t cap)
{
    size_t i = 0;
    int pass;
    /* BOOT2 (PS2) wins over BOOT (PS1) if a disc somehow has both */
    for (pass = 0; pass < 2; pass++) {
        const char *key = pass == 0 ? "BOOT2" : "BOOT";
        size_t kl = strlen(key);
        for (i = 0; i < len; ) {
            size_t ls = i, le = i, p;
            while (le < len && txt[le] != '\n' && txt[le] != '\r') le++;
            p = ls;
            while (p < le && (txt[p] == ' ' || txt[p] == '\t')) p++;
            if (le - p > kl && sf_strnicmp(txt + p, key, kl) == 0) {
                size_t q = p + kl, start, end;
                while (q < le && (txt[q] == ' ' || txt[q] == '\t')) q++;
                if (q < le && txt[q] == '=') {
                    q++;
                    while (q < le && (txt[q] == ' ' || txt[q] == '\t')) q++;
                    start = q;
                    for (end = q; end < le && txt[end] != ';' && txt[end] != ' ' && txt[end] != '\t'; end++) {
                        if (txt[end] == '\\' || txt[end] == '/' || txt[end] == ':') start = end + 1;
                    }
                    if (end > start && title_normalize_id(txt + start, end - start, out, cap)) return 1;
                }
            }
            i = le + 1;
        }
    }
    if (cap) out[0] = '\0';
    return 0;
}

int discid_cue_first_file(const char *cue_path, const char *txt, size_t len, char *out, size_t cap)
{
    size_t i;
    for (i = 0; i < len; ) {
        size_t le = i, p = i;
        while (le < len && txt[le] != '\n' && txt[le] != '\r') le++;
        while (p < le && (txt[p] == ' ' || txt[p] == '\t')) p++;
        if (le - p > 5 && sf_strnicmp(txt + p, "FILE", 4) == 0 && (txt[p + 4] == ' ' || txt[p + 4] == '\t')) {
            size_t s = p + 5, e;
            char name[256];
            const char *slash = strrchr(cue_path, '/');
            size_t dir_len = slash ? (size_t)(slash - cue_path) + 1 : 0, n;
            while (s < le && (txt[s] == ' ' || txt[s] == '\t')) s++;
            if (s < le && txt[s] == '"') {
                s++;
                for (e = s; e < le && txt[e] != '"'; e++) { }
            } else {
                for (e = s; e < le && txt[e] != ' ' && txt[e] != '\t'; e++) { }
            }
            n = e - s;
            if (n == 0 || n >= sizeof(name)) return 0;
            memcpy(name, txt + s, n);
            name[n] = '\0';
            /* names in a cue are relative to its folder; keep only the base name */
            {
                const char *b = name, *q;
                for (q = name; *q; q++) if (*q == '/' || *q == '\\') b = q + 1;
                if (dir_len + strlen(b) + 1 > cap) return 0;
                memcpy(out, cue_path, dir_len);
                strcpy(out + dir_len, b);
            }
            return 1;
        }
        i = le + 1;
    }
    return 0;
}

/* Parse ALL FILE declarations, not just the first file or the title.
 * Real Redump CUE/BIN sets commonly contain one .cue and 10+ tracks named
 * "Game (Track 01).bin", "Game (Track 02).bin" etc. Comparing a BIN's
 * cleaned title to the CUE name left those tracks as duplicate games
 * without covers in earlier SpookyFlow versions.
 *
 * Return only FILE basenames. No files are opened or altered here. */
int discid_cue_files(const char *txt,size_t len,
                     char out[][PS1_CUE_REF_NAME_MAX],int max)
{
    size_t i=0;
    int count=0;
    if(!txt || !out || max<=0)return 0;
    if(max>PS1_CUE_REF_MAX)max=PS1_CUE_REF_MAX;
    while(i<len && count<max){
        size_t p=i,end=i,start,finish,st;
        char file[PS1_CUE_REF_NAME_MAX],*base;
        int j,duplicate=0;
        while(end<len && txt[end]!='\n' && txt[end]!='\r')end++;
        while(p<end && (txt[p]==' '||txt[p]=='\t'))p++;
        if(end-p>5 && sf_strnicmp(txt+p,"FILE",4)==0 &&
           (txt[p+4]==' '||txt[p+4]=='\t')){
            p+=4;
            while(p<end && (txt[p]==' '||txt[p]=='\t'))p++;
            if(p<end && txt[p]=='"'){
                start=++p;
                while(p<end && txt[p]!='"')p++;
                finish=p;
            }else{
                start=p;
                while(p<end && txt[p]!=' ' && txt[p]!='\t')p++;
                finish=p;
            }
            if(finish>start && finish-start<sizeof(file)){
                memcpy(file,txt+start,finish-start);
                file[finish-start]='\0';
                base=file;
                for(st=0;file[st];st++)
                    if(file[st]=='/'||file[st]=='\\')base=file+st+1;
                if(base[0] && strlen(base)<PS1_CUE_REF_NAME_MAX){
                    for(j=0;j<count;j++)
                        if(sf_stricmp(out[j],base)==0){duplicate=1;break;}
                    if(!duplicate)
                        sf_strlcpy(out[count++],base,PS1_CUE_REF_NAME_MAX);
                }
            }
        }
        i=end;
        while(i<len && (txt[i]=='\r'||txt[i]=='\n'))i++;
    }
    return count;
}

/* PS1 filename serial fallback for dumps that lack ISO9660 metadata,
 * including .chd/.pbp and CUE dumps with unusual layouts. Only trust
 * unambiguous PlayStation serial prefixes followed by exactly five digits;
 * never guess a serial from a game's display title. The cover downloader
 * still rejects custom ROM hacks that reuse the base game's serial. */
static int ps1_serial_prefix(const char *p)
{
    static const char *const known[]={
        "SLUS","SCUS","SLES","SCES","SLPS","SLPM","SCPS","SIPS",
        "SLKA","SLED","SCED","PAPX","PCPX","PEPX","PBPX","SCAJ",
        "SLSA","CPCS","ESPM","PSRM"
    };
    int k;
    for(k=0;k<SF_ARRAY_LEN(known);k++)
        if(sf_strnicmp(p,known[k],4)==0)return 1;
    return 0;
}
static int ps1_filename_serial(const char *path,char *out,size_t cap)
{
    const char *base=path,*p;
    if(!path)return 0;
    for(p=path;*p;p++)if(*p=='/'||*p=='\\')base=p+1;
    for(p=base;*p;p++) {
        const char *q;
        int digits=0,skip=0;
        if(p>base && (sf_isalpha(p[-1])||sf_isdigit(p[-1])))continue;
        if(strlen(p)<9 || !ps1_serial_prefix(p))continue;
        q=p+4;
        /* Common forms SLUS00594, SLUS-00594 and SLUS_005.94.
         * Allow one separator before digits and one in a 3+2 split.
         * Do not interpret other longer alphanumeric sequences as IDs. */
        if(*q=='-'||*q=='_'||*q==' '){q++;skip++;}
        for(;*q&&digits<5;q++){
            if(sf_isdigit(*q))digits++;
            else if((*q=='.'||*q=='-'||*q=='_')&&digits==3&&skip<2)
                skip++;
            else break;
        }
        if(digits!=5 || sf_isalpha(*q)||sf_isdigit(*q))continue;
        if(title_normalize_id(p,(size_t)(q-p),out,cap))return 1;
    }
    if(cap)out[0]='\0';
    return 0;
}

/* ------------------------------------------------------------------ */
/* ISO 9660                                                            */

typedef struct {
    ReadAtFn rd;
    void *ctx;
    const char *path;
    uint32_t pitch, data_off;   /* bytes per sector in the file, user data offset */
} Iso;

static int read_sector(const Iso *iso, uint32_t lba, unsigned char *buf)
{
    uint64_t off = (uint64_t)lba * iso->pitch + iso->data_off;
    return iso->rd(iso->ctx, iso->path, off, buf, SECTOR) == (int)SECTOR ? 0 : -1;
}

static int iso_open(Iso *iso, unsigned char *buf)
{
    static const uint32_t layouts[][2] = { { 2048u, 0u }, { 2352u, 24u }, { 2352u, 16u } };
    int i;
    for (i = 0; i < SF_ARRAY_LEN(layouts); i++) {
        iso->pitch = layouts[i][0];
        iso->data_off = layouts[i][1];
        if (read_sector(iso, 16, buf) == 0 && buf[0] == 1 && memcmp(buf + 1, "CD001", 5) == 0) return 0;
    }
    return -1;
}

static int is_exe_name(const unsigned char *n, size_t len)
{
    /* "SLUS_209.14;1" */
    size_t i;
    if (len < 11) return 0;
    for (i = 0; i < 4; i++) if (!sf_isalpha((char)n[i])) return 0;
    if (n[4] != '_' || n[8] != '.') return 0;
    for (i = 5; i < 8; i++) if (!sf_isdigit((char)n[i])) return 0;
    return sf_isdigit((char)n[9]) && sf_isdigit((char)n[10]) && (len == 11 || n[11] == ';');
}

static int psx_id(const Iso *iso, unsigned char *buf, char *out, size_t cap)
{
    uint32_t root_lba, root_len, s, nsect;
    uint32_t cnf_lba = 0, cnf_len = 0;
    char exe[TITLEID_MAX] = "";
    root_lba = le32(buf + 156 + 2);
    root_len = le32(buf + 156 + 10);
    nsect = (root_len + SECTOR - 1) / SECTOR;
    if (nsect == 0 || nsect > ROOT_MAX_SECT) nsect = nsect == 0 ? 1 : ROOT_MAX_SECT;
    for (s = 0; s < nsect && !cnf_lba; s++) {
        uint32_t i = 0;
        if (read_sector(iso, root_lba + s, buf) != 0) return 0;
        while (i + 33 < SECTOR) {
            uint32_t rl = buf[i], nl;
            const unsigned char *name;
            if (rl == 0) break;                       /* rest of this sector is padding */
            if (rl < 34 || i + rl > SECTOR) break;
            nl = buf[i + 32];
            name = buf + i + 33;
            if (33u + nl <= rl && !(buf[i + 25] & 2)) {
                if (nl >= 10 && sf_strnicmp((const char *)name, "SYSTEM.CNF", 10) == 0 && (nl == 10 || name[10] == ';')) {
                    cnf_lba = le32(buf + i + 2);
                    cnf_len = le32(buf + i + 10);
                    break;
                }
                if (!exe[0] && is_exe_name(name, nl)) title_normalize_id((const char *)name, 11, exe, sizeof(exe));
            }
            i += rl;
        }
    }
    if (cnf_lba) {
        if (read_sector(iso, cnf_lba, buf) == 0) {
            size_t n = cnf_len < SECTOR ? cnf_len : SECTOR;
            if (discid_parse_system_cnf((const char *)buf, n, out, cap)) return 1;
        }
    }
    if (exe[0]) { sf_strlcpy(out, exe, cap); return 1; }
    return 0;
}

static int ps3_iso_id(ReadAtFn rd, void *ctx, const char *path, char *out, size_t cap)
{
    unsigned char b[16];
    if (rd(ctx, path, 0x810, b, 10) != 10) return 0;
    return title_normalize_id((const char *)b, 10, out, cap);
}

static int ps3_folder_id(ReadAtFn rd, void *ctx, const char *path, char *out, size_t cap)
{
    enum { SFO_MAX = 64 * 1024 };
    char p[600], id[64];
    unsigned char *buf;
    int n, ok = 0;
    sf_strlcpy(p, path, sizeof(p));
    sf_strlcat(p, "/PS3_GAME/PARAM.SFO", sizeof(p));
    buf = (unsigned char *)malloc(SFO_MAX);
    if (!buf) return 0;
    n = rd(ctx, p, 0, buf, SFO_MAX);
    if (n > 0 && sfo_get_str(buf, (size_t)n, "TITLE_ID", id, sizeof(id)) == 0)
        ok = title_normalize_id(id, strlen(id), out, cap);
    free(buf);
    return ok;
}

/* A multi-file PlayStation CUE can list an AUDIO FILE before the actual
 * data track. The previous routine always followed the first FILE and
 * discarded a valid disc serial if it contained audio. Select the FILE
 * preceding the first MODE1/MODE2 data TRACK. Retain legacy first-FILE
 * fallback for simple/partial CUE sheets with no TRACK declarations. */
static int ps1_cue_data_file(const char *cue_path,const char *txt,size_t len,
                             char *out,size_t cap)
{
    char current[600]="";
    size_t i=0;
    while(i<len) {
        size_t end=i,p=i;
        while(end<len && txt[end]!='\n' && txt[end]!='\r')end++;
        while(p<end && (txt[p]==' ' || txt[p]=='\t'))p++;
        if(end-p>5 && sf_strnicmp(txt+p,"FILE",4)==0 &&
           (txt[p+4]==' '||txt[p+4]=='\t')) {
            if(!discid_cue_first_file(cue_path,txt+p,end-p,current,sizeof(current)))
                current[0]='\0';
        } else if(current[0] && end-p>10 && sf_strnicmp(txt+p,"TRACK",5)==0) {
            size_t q;
            for(q=p+5;q+5<end;q++) {
                if(sf_strnicmp(txt+q,"MODE1/",6)==0 ||
                   sf_strnicmp(txt+q,"MODE2/",6)==0) {
                    sf_strlcpy(out,current,cap);
                    return 1;
                }
            }
        }
        i=end;
        while(i<len && (txt[i]=='\n'||txt[i]=='\r'))i++;
    }
    return discid_cue_first_file(cue_path,txt,len,out,cap);
}

int discid_read(int plat, const char *path, int is_dir, ReadAtFn rd, void *ctx, char *out, size_t cap)
{
    char img[600];
    unsigned char *buf;
    Iso iso;
    int ok = 0;
    if (cap) out[0] = '\0';
    if (!rd || !path || !path[0] || cap == 0) return 0;
    if (strstr(path, ".ntfs[")) return 0;      /* webMAN NTFS pointer file: its ID comes from the scan */
    if (plat == PLAT_PS3) {
        ok = is_dir ? ps3_folder_id(rd, ctx, path, out, cap) : ps3_iso_id(rd, ctx, path, out, cap);
        if (!ok) out[0] = '\0';
        return ok;
    }
    if (is_dir) return 0;
    if (sf_ends_with_i(path, ".enc") || sf_ends_with_i(path, ".chd") || sf_ends_with_i(path, ".pbp"))
        return plat==PLAT_PS1 ? ps1_filename_serial(path,out,cap) : 0;
    buf = (unsigned char *)malloc(sf_ends_with_i(path,".cue") ? 8192u : SECTOR);
    if (!buf) return 0;
    sf_strlcpy(img, path, sizeof(img));
    if (sf_ends_with_i(path, ".cue")) {
        int n = rd(ctx, path, 0, buf, 8191u);
        if (n <= 0 || !ps1_cue_data_file(path,(const char *)buf,(size_t)n,
                                         img,sizeof(img))) {
            free(buf);
            return plat==PLAT_PS1 ? ps1_filename_serial(path,out,cap) : 0;
        }
    }
    iso.rd = rd; iso.ctx = ctx; iso.path = img;
    if (iso_open(&iso, buf) == 0) ok = psx_id(&iso, buf, out, cap);
    free(buf);
    if(!ok && plat==PLAT_PS1)
        ok=ps1_filename_serial(path,out,cap);
    if (!ok) out[0] = '\0';
    return ok;
}
