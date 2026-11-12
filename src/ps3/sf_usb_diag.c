/* V15.4 diagnostic build: read-only USB/NTFS/webMAN enumeration.
 * Never modifies or deletes files on the external drive, remounts media,
 * or calls webMAN refresh. Each snapshot is flushed to /dev_hdd0 immediately.
 * The report is intentionally generated from actual PS3 calls, not CI mocks.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/file.h>
#include <ntfs.h>
#include "sf_usb_diag.h"
#include "ps3_platform.h"
#include "sf_bdiso_ntfs.h"
#include "bdiso_usb.h"
#include "log.h"
#include "util.h"

#define MAX_DIAG_SNAPSHOTS 24
static unsigned s_snaps;

int sf_usb_diag_begin(void)
{
    FILE *f=fopen(SF_USB_DIAG_PATH,"w");
    s_snaps=0;
    if(!f)return -1;
    fputs("SPOOKYFLOW V15.4 - INFORME REAL USB/NTFS/BDISO\n"
          "Generado por la PS3. No se ha modificado el disco USB.\n"
          "INSTRUCCIONES: abrir SpookyFlow con el disco conectado,\n"
          "esperar a que termine la carga, cerrar desde el menu,\n"
          "copiar ESTE archivo completo para revisar el fallo.\n"
          "No es una prueba de laboratorio: son resultados del hardware real.\n"
          "------------------------------------------------------------\n",f);
    fflush(f);
    fclose(f);
    return 0;
}

static int probe_lv2(const char *path)
{
    sysFSStat st;
    return sysFsStat(path,&st);
}
static int probe_directory(const char *path)
{
    s32 fd;
    int rc=sysFsOpendir(path,&fd);
    if(!rc)sysFsClosedir(fd);
    return rc;
}
static void dump_webman(FILE *f, uint32_t physical_mask)
{
    s32 fd;
    sysFSDirent ent;
    u64 got;
    int bd=0,others=0,samples=0;
    int rc=sysFsOpendir(NTFS_INDEX_DIR,&fd);
    fprintf(f,"WEBMAN_INDEX path=%s opendir_rc=%d\n",NTFS_INDEX_DIR,rc);
    if(rc!=0)return;
    for(;;){
        unsigned char pointer[64];
        char path[SCAN_PATH_MAX],stem[FS_NAME_MAX];
        int kind,n,port,visible;
        got=0;
        if(sysFsReaddir(fd,&ent,&got)!=0 || got==0)break;
        if(!ent.d_name[0] || ent.d_name[0]=='.')continue;
        kind=scanner_ntfs_kind(ent.d_name,stem,sizeof(stem));
        if(kind!=PLAT_BDISO){others++;continue;}
        bd++;
        if(samples>=12)continue;
        if(snprintf(path,sizeof(path),"%s/%s",NTFS_INDEX_DIR,ent.d_name)>=(int)sizeof(path))continue;
        n=ps3_read_at(NULL,path,0,pointer,sizeof(pointer));
        port=bdiso_usb_pointer_port(n>0?pointer:NULL,n>0?(size_t)n:0);
        visible=bdiso_usb_pointer_visible(n>0?pointer:NULL,n>0?(size_t)n:0,
                                          physical_mask);
        fprintf(f,"WEBMAN_MOVIE name=%.130s bytes_read=%d parsed_usb=%d accepted_by_v15_3=%d\n",
                ent.d_name,n,port,visible);
        samples++;
    }
    sysFsClosedir(fd);
    fprintf(f,"WEBMAN_TOTAL movies_BDISO=%d other_entries=%d sample_count=%d\n",
            bd,others,samples);
}

void sf_usb_diag_snapshot(const char *phase, const Library *lib,
                          const Scanner *scan, uint32_t app_mask)
{
    FILE *f;
    uint32_t present=sf_bdntfs_usb_present_mask();
    int port;
    if(s_snaps>=MAX_DIAG_SNAPSHOTS)return;
    f=fopen(SF_USB_DIAG_PATH,"a");
    if(!f){log_printf("usb_diag: could not open report for snapshot %s",phase?phase:"?");return;}
    fprintf(f,"\n=== SNAPSHOT %u - %s ===\n",++s_snaps,phase?phase:"UNKNOWN");
    fprintf(f,"USB_MASK application=0x%08x native_ntfs_present=0x%02x\n",
            (unsigned)app_mask,(unsigned)present);
    for(port=0;port<SCAN_USB_PORTS;port++){
        char root[32],bd[48];
        int st,br,dp,parts=0,part=-1,mount_rc=0;
        snprintf(root,sizeof(root),"/dev_usb%03d",port);
        snprintf(bd,sizeof(bd),"/dev_usb%03d/BDISO",port);
        st=probe_lv2(root);
        br=probe_directory(bd);
        dp=probe_lv2(bd);
        if(port<8)
            sf_bdntfs_probe_port(port,&parts,&part,&mount_rc);
        fprintf(f,"PORT %03d lv2_root_stat=%d lv2_bdiso_stat=%d lv2_bdiso_opendir=%d "
                  "physical_ntfs=%d native_parts=%d bdiso_partition=%d last_mount_rc=%d\n",
                port,st,dp,br,port<8?((present>>port)&1u):0,
                parts,part,mount_rc);
    }
    /* Probe only native volumes already mounted by the ordinary application.
     * Do not invoke sf_bdntfs_start() or NTFS mount functions here. */
    for(port=0;port<8;port++){
        if(sf_bdntfs_has_port(port)){
            char bd[48];
            FsEntry list[4];
            int nb,i;
            snprintf(bd,sizeof(bd),"/dev_usb%03d/BDISO",port);
            /* Do not list the public root: that path triggers NTFS startup. */
            nb=sf_bdntfs_list(bd,list,4);
            fprintf(f,"NATIVE_LIST port=%03d bdiso_entries=%d\n",port,nb);
            for(i=0;i<nb && i<4;i++)
                fprintf(f,"NATIVE_SAMPLE port=%03d entry=%.160s dir=%d\n",
                        port,list[i].name,list[i].is_dir);
        }
    }
    dump_webman(f,present);
    fprintf(f,"INTERNAL_BDISO /dev_hdd0/BDISO stat_rc=%d opendir_rc=%d\n",
            probe_lv2("/dev_hdd0/BDISO"),probe_directory("/dev_hdd0/BDISO"));
    if(scan){
        int i,usb_movie_roots=0;
        fprintf(f,"SCANNER roots=%d dirs=%d errors=%d finished=%d scanned_BDISO=%d scanned_NTFS_total=%d\n",
                scan->nroots,scan->dirs_listed,scan->errors,scan->done,
                scan->found[PLAT_BDISO],scan->found_ntfs);
        for(i=0;i<scan->nroots && i<160;i++){
            const ScanRoot *root=&scan->roots[i];
            if(root->plat!=PLAT_BDISO && root->kind!=SRC_NTFS_INDEX)continue;
            fprintf(f,"SCANNER_ROOT kind=%d platform=%d path=%s\n",
                    root->kind,root->plat,root->path);
            if(root->plat==PLAT_BDISO && strncmp(root->path,"/dev_usb",8)==0)
                usb_movie_roots++;
        }
        fprintf(f,"SCANNER_USB_MOVIE_ROOTS %d\n",usb_movie_roots);
    }else{
        fputs("SCANNER not started yet\n",f);
    }
    if(lib){
        int i;
        const GameList *gl=&lib->tab[PLAT_BDISO];
        fprintf(f,"LIBRARY BDISO_visible=%d hidden=%d PS3=%d PS2=%d PS1=%d\n",
                gl->count,gl->nhidden,
                lib->tab[PLAT_PS3].count,lib->tab[PLAT_PS2].count,
                lib->tab[PLAT_PS1].count);
        for(i=0;i<gl->count && i<12;i++)
            fprintf(f,"LIBRARY_MOVIE name=%.130s path=%.255s\n",
                    lib_title(lib,&gl->games[i]),lib_path(lib,&gl->games[i]));
    }else fputs("LIBRARY not built yet\n",f);
    if(lib && lib->tab[PLAT_BDISO].count>0)
        fputs("RESULT: BDISO exists in the library. If the screen is empty, investigate UI tab/filters.\n",f);
    else if(present==0 && (app_mask&0x7ffu)==0)
        fputs("RESULT: no USB visible to both native NTFS and lv2 probes: check connection/device enumeration.\n",f);
    else if(present!=0)
        fputs("RESULT: USB detected but no BDISO movies in library: compare native mount, BDISO listings, webMAN index, and SCANNER_ROOT.\n",f);
    else
        fputs("RESULT: lv2 sees a USB, native NTFS doesn't; check FAT/exFAT vs NTFS or NTFS initialization.\n",f);
    fputs("=== END SNAPSHOT ===\n",f);
    fflush(f);
    fclose(f);
    log_printf("usb_diag: %s saved at %s (native mask=%02x)",
               phase?phase:"?",SF_USB_DIAG_PATH,(unsigned)present);
}
