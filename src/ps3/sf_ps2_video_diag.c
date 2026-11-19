/* SpookyFlow V15.75 - PS2-ONLY forensic video diagnostics.
 *
 * In HEN, SpookyFlow stages PS2 Classics but PS2U10000/ps2_netemu renders
 * after our process ends. We do NOT assert that PS3 frontend video state
 * represents a frame from the PS2 emulator. We do NOT write XRegistry,
 * PS2bootparam.dat, encrypted CONFIG, ISO, PS2 emulator, or graphics memory.
 *
 * Exactly ONE diagnostic TXT, overwritten on EVERY selected PS2 ISO.
 * Bounded at 48 KiB, and includes post-return passive bootparam probes.
 *
 * PS2bootparam.dat contains wifi credentials: NEVER dump raw bytes; read
 * ONLY display port, supported video-mode bitfield, upscaler and smoothing
 * byte fields plus sanitized PS2 title ID for comparison.
 */
#include "sf_ps2_video_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef SF_PS2_VIDEO_HOST_TEST
#include <sysutil/video.h>
#endif

#define DIAG_DIR "/dev_hdd0/tmp/spookyflow"
#ifndef SF_PS2_VIDEO_BOOTPARAM_PATH
#define SF_PS2_VIDEO_BOOTPARAM_PATH "/dev_hdd0/tmp/game/ps2bootparam.dat"
#endif
#define PS2_BOOTPARAM SF_PS2_VIDEO_BOOTPARAM_PATH
#define PS2_HEN_PLACEHOLDER "/dev_hdd0/game/PS2U10000/USRDIR"
#define PS2_HEN_PS2CONFIG "/dev_hdd0/game/PS2CONFIG/USRDIR/CONFIG/ENC"
#define PS2_HEN_BUNDLED "/dev_hdd0/game/SPKF00001/USRDIR/ps2configs"
#define DIAG_CAP (48L*1024L)
#ifdef SF_PS2_VIDEO_HOST_TEST
#ifndef SF_PS2_VIDEO_DIAG_PATH
#error "host test requires path override"
#endif
#endif

static int s_resume_checked=0;
static void safe(FILE *f,const char *s)
{
    size_t i;
    if(!s){fputs("(null)",f);return;}
    for(i=0;s[i] && i<500;i++){
        unsigned char c=(unsigned char)s[i];
        fputc(c>=32 && c<127 ? c : '_',f);
    }
    if(s[i])fputs("...",f);
}
static void field(FILE *f,const char *name,const char *value)
{
    fprintf(f,"%s=",name);
    safe(f,value);fputc('\n',f);
}
static long filebytes(const char *path)
{
    struct stat st;
    if(!path || stat(path,&st)!=0 || !S_ISREG(st.st_mode))return -1;
    return (long)st.st_size;
}
static FILE *append(void)
{
    FILE *f=fopen(SF_PS2_VIDEO_DIAG_PATH,"ab");
    long length;
    if(!f)return NULL;
    if(fseek(f,0,SEEK_END)!=0 || (length=ftell(f))<0 ||
       length>=DIAG_CAP-2048L){fclose(f);return NULL;}
    return f;
}
static void bootparam(FILE *f,const char *phase,const char *expected)
{
    FILE *b=fopen(PS2_BOOTPARAM,"rb");
    unsigned char port=0,up=255,smooth=255,vm[2]={0};
    char id[12]={0},clean[12]={0},wanted[12]={0};
    size_t i,j;
    long len=-1;
    fprintf(f,"BOOTPARAM_PHASE=%s\n",phase);
    field(f,"BOOTPARAM_PATH",PS2_BOOTPARAM);
    if(!b){fputs("BOOTPARAM_ESTADO=NO_DISPONIBLE\n",f);return;}
    if(fseek(b,0,SEEK_END)!=0 || (len=ftell(b))<0x4ce ||
       fseek(b,0x0c,SEEK_SET)!=0 || fread(id,1,10,b)!=10 ||
       fseek(b,0x11a,SEEK_SET)!=0 || fread(vm,1,2,b)!=2 ||
       fseek(b,0x11c,SEEK_SET)!=0 || fread(&port,1,1,b)!=1 ||
       fseek(b,0x4cc,SEEK_SET)!=0 || fread(&up,1,1,b)!=1 ||
       fread(&smooth,1,1,b)!=1){
        fputs("BOOTPARAM_ESTADO=INCOMPLETO_O_FORMATO_INESPERADO\n",f);
        fclose(b);return;
    }
    fclose(b);
    for(i=0,j=0;i<10 && j<10 && id[i];i++){
        unsigned char c=(unsigned char)id[i];
        if(isalnum(c))clean[j++]=(char)toupper(c);
    }
    if(expected){
        for(i=0,j=0;expected[i] && j<10;i++){
            unsigned char c=(unsigned char)expected[i];
            if(isalnum(c))wanted[j++]=(char)toupper(c);
        }
    }
    fputs("BOOTPARAM_ESTADO=LEIDO_SOLO_CAMPOS_VIDEO\n",f);
    field(f,"BOOTPARAM_TITULO_ANTERIOR",clean[0]?clean:"(sin_id)");
    if(wanted[0])fprintf(f,"BOOTPARAM_COINCIDE_JUEGO=%s\n",
                           strcmp(wanted,clean)==0?"SI":"NO_O_DESCONOCIDO");
    else fputs("BOOTPARAM_COINCIDE_JUEGO=ID_NO_SUMINISTRADO\n",f);
    fprintf(f,"BOOTPARAM_PORT_TIPO=%u\nBOOTPARAM_VIDEO_MODES_BITFIELD=0x%02X%02X\n",
            (unsigned)port,(unsigned)vm[0],(unsigned)vm[1]);
    fprintf(f,"BOOTPARAM_ESCALADOR=%u\nBOOTPARAM_SUAVIZADO=%u\n",
            (unsigned)up,(unsigned)smooth);
    fputs("BOOTPARAM_ESCALADOR_INTERPRETACION=",f);
    if(up==0)fputs("OFF_SIN_ESCALADO\n",f);
    else if(up==1)fputs("NORMAL_RELACION_ORIGINAL_4_3\n",f);
    else if(up==2)fputs("FULL_ESCALADO_16_9_ESTIRADO\n",f);
    else fputs("DESCONOCIDO\n",f);
    fputs("BOOTPARAM_AVISO=ARCHIVO_DEL_ULTIMO_PS2_EJECUTADO_NO_ES_MEDICION_EN_VIVO\n",f);
}
static void frontend_video(FILE *f)
{
#ifdef SF_PS2_VIDEO_HOST_TEST
    fputs("PS3_VIDEO_ESTADO=HOST_TEST_NO_HARDWARE\n",f);
#else
    videoState state;
    videoResolution res;
    int rc,resrc=-99;
    memset(&state,0,sizeof state);
    memset(&res,0,sizeof res);
    rc=videoGetState(0,0,&state);
    if(rc==0)resrc=videoGetResolution(state.displayMode.resolution,&res);
    fprintf(f,"PS3_VIDEO_GET_STATE_RC=%d\nPS3_VIDEO_GET_RESOLUTION_RC=%d\n",
            rc,resrc);
    if(rc==0 && resrc==0)
        fprintf(f,"PS3_FRONTEND_VIDEO_ANCHO=%u\nPS3_FRONTEND_VIDEO_ALTO=%u\n"
                  "PS3_FRONTEND_VIDEO_ASPECT_ID=%u\nPS3_FRONTEND_RESOLUTION_ID=%u\n",
                  (unsigned)res.width,(unsigned)res.height,
                  (unsigned)state.displayMode.aspect,
                  (unsigned)state.displayMode.resolution);
#endif
    fputs("VIDEO_ESTADO_ALCANCE=SOLO_FRONTEND_SPOOKYFLOW_NO_PANTALLA_PS2_NETEMU\n",f);
}
static void fileprobe(FILE *f,const char *kind,const char *path)
{
    long sz=filebytes(path);
    fprintf(f,"%s_EXISTE=%s\n%s_BYTES=%ld\n",
            kind,sz>=0?"SI":"NO",kind,sz);
}
void sf_ps2_video_diag_begin(const char *iso_path)
{
    FILE *f;
    long sz;
    /* Installer may not have created tmp/spookyflow yet. */
    (void)mkdir("/dev_hdd0/tmp/spookyflow",0777);
    f=fopen(SF_PS2_VIDEO_DIAG_PATH,"wb");
    if(!f)return;
    fputs("SPOOKYFLOW_PS2_VIDEO_DIAG=V15.75\n"
          "SCOPE=PS2_ONLY\nFORMATO=DIAGNOSTICO_AVANZADO_XMB_PS2_CLASSICS\n"
          "ARCHIVO_UNICO=SI_REEMPLAZADO_CADA_INTENTO\n"
          "MODIFICA_ROM=NO\nMODIFICA_CONFIG_PS2=NO\n"
          "MODIFICA_XREGISTRY=NO\nMODIFICA_PS2_NETEMU=NO\n"
          "FUNDAMENTO=PS2_NETEMU_SE_EJECUTA_FUERA_DE_SPOOKYFLOW\n",f);
    field(f,"ISO_RUTA",iso_path);
    sz=filebytes(iso_path);
    fprintf(f,"ISO_BYTES=%ld\nISO_ACCESIBLE=%s\n",sz,sz>=0?"SI":"NO");
    if(iso_path){
        size_t n=strlen(iso_path);
        if(n>=6 && !strcmp(iso_path+n-6,".iso.0"))
            fputs("ISO_TIPO=ISO_DIVIDIDO_NO_ADMITIDO_EN_HEN\n",f);
        else if(n>=4 && !strcmp(iso_path+n-4,".iso"))
            fputs("ISO_TIPO=ISO_SIN_DIVIDIR\n",f);
        else
            fputs("ISO_TIPO=EXTENSION_NO_VERIFICADA\n",f);
    }
    frontend_video(f);
    bootparam(f,"ANTES_DEL_LANZAMIENTO",NULL);
    fputs("PS2_VIDEO_FIX_MODO=PROTEGIDO_POR_JUEGO\n"
          "PS2_VIDEO_FIX_BASE=RESPETAR_ESCALADOR_DE_PS3_Y_CONFIG_NETEMU_ORIGINAL\n"
          "PS2_VIDEO_FIX_PROHIBIDO=FORZAR_720P_O_1080I_DENTRO_DEL_GS_PS2_NETEMU\n"
          "PS2_VIDEO_REAL_16_9=REQUIERE_SOPORTE_JUEGO_O_PARCHE_VERIFICADO\n"
          "SI_HAY_BARRAS_NEGRAS=COMPROBAR_AJUSTES_JUEGO_PS2_Y_UPSCALER_FULL_PS3\n"
          "SI_SE_ESTIRA=UPSCALER_FULL_NO_ES_WIDESCREEN_3D_REAL\n",f);
    fflush(f);fclose(f);
}
void sf_ps2_video_diag_preflight(int rc)
{
    FILE *f=append();if(!f)return;
    fprintf(f,"PRELIMINAR_RC=%d\n",rc);
    if(rc==-41)fputs("MOTOR_PREVISTO=HEN_PS2_CLASSICS_PS2_NETEMU\n",f);
    else if(rc==0)fputs("MOTOR_PREVISTO=COBRA_PS2_EMULATOR_SEGUN_CONFIG\n",f);
    else fputs("MOTOR_PREVISTO=LANZAMIENTO_RECHAZADO_ANTES_DE_EMULADOR\n",f);
    fflush(f);fclose(f);
}
void sf_ps2_video_diag_game(const char *iso_path,const char *game_id)
{
    char p[512];
    FILE *f=append();
    if(!f)return;
    field(f,"JUEGO_ID",game_id && *game_id?game_id:"DESCONOCIDO");
    field(f,"JUEGO_ISO",iso_path);
    fileprobe(f,"PS2_CLASSICS_EBOOT",PS2_HEN_PLACEHOLDER "/EBOOT.BIN");
    fileprobe(f,"PS2_CLASSICS_ISO_BIN_ENC",PS2_HEN_PLACEHOLDER "/ISO.BIN.ENC");
    fileprobe(f,"CONFIG_ACTUAL",PS2_HEN_PLACEHOLDER "/CONFIG");
    fileprobe(f,"CONFIG_OWNERSHIP",PS2_HEN_PLACEHOLDER "/CONFIG.sf-source");
    if(game_id && strlen(game_id)==11 && strchr(game_id,'_')){
        size_t i;int clean=1;
        for(i=0;i<11;i++)if(!isalnum((unsigned char)game_id[i]) &&
                              game_id[i]!='_' && game_id[i]!='.')clean=0;
        if(clean){
            snprintf(p,sizeof p,"%s/%.11s.ENC",PS2_HEN_PS2CONFIG,game_id);
            fileprobe(f,"CONFIG_EXACTO_PS2CONFIG",p);
            snprintf(p,sizeof p,"%s/%.11s.ENC",PS2_HEN_BUNDLED,game_id);
            fileprobe(f,"CONFIG_EXACTO_EMPAQUETADO",p);
        }
    }
    bootparam(f,"REFERENCIA_DEL_ANTERIOR_JUEGO_NO_ACTUAL",game_id);
    fputs("CONFIG_FINAL=LA_ETAPA_DE_PREPARACION_PS2_CLASSICS_ELIGE_EL_ENC_DEL_ID_EXACTO\n"
          "NO_SE_PUEDE_DEDUCIR_16_9_DE_LA_SIMPLE_EXISTENCIA_DEL_CONFIG\n",f);
    fflush(f);fclose(f);
}
void sf_ps2_video_diag_event(const char *phase,int rc,const char *detail)
{
    FILE *f=append();
    if(!f)return;
    fputs("ETAPA=",f);safe(f,phase);fputc('\n',f);
    fprintf(f,"ETAPA_RC=%d\nETAPA_DETALLE=",rc);
    safe(f,detail);fputc('\n',f);
    fflush(f);fclose(f);
}
/* Called only after PS2 Classics preparation has finished. Inspect the
 * actual selected CONFIG, not a guess made from the initial ISO name. */
void sf_ps2_video_diag_final_config(int finished_rc)
{
    FILE *f=append();
    if(!f)return;
    fprintf(f,"HEN_CONFIG_POST_PREPARACION_RC=%d\n",finished_rc);
    fileprobe(f,"HEN_POST_ISO_BIN_ENC",PS2_HEN_PLACEHOLDER "/ISO.BIN.ENC");
    fileprobe(f,"HEN_POST_CONFIG",PS2_HEN_PLACEHOLDER "/CONFIG");
    fileprobe(f,"HEN_POST_CONFIG_ORIGEN",PS2_HEN_PLACEHOLDER "/CONFIG.sf-source");
    fputs("HEN_CONFIG_NOTA=CONFIG_ES_ENCRIPTADO_NO_SE_DEDUCE_CORRECCION_VIDEO_SOLO_POR_EXISTIR\n"
          "HEN_POST_LANZAMIENTO=PS2_CLASSICS_CORRE_FUERA_DE_SPOOKYFLOW\n",f);
    fflush(f);fclose(f);
}

void sf_ps2_video_diag_resume(void)
{
    FILE *f,*a;char head[96]={0};
    if(s_resume_checked++)return;
    f=fopen(SF_PS2_VIDEO_DIAG_PATH,"rb");if(!f)return;
    if(!fgets(head,sizeof head,f) ||
       !strstr(head,"SPOOKYFLOW_PS2_VIDEO_DIAG=")){
        fclose(f);return;
    }
    fclose(f);
    a=append();if(!a)return;
    fputs("REAPERTURA_SPOOKYFLOW=SI\n",a);
    bootparam(a,"DESPUES_DE_REGRESAR_DE_PS2",NULL);
    fputs("LIMITACION=NO_OBSERVA_PIXELES_DE_PS2_NETEMU_FPS_GS_BUFFER_NI_HDMI_FINAL\n"
          "PARA_VERIFICAR_BORDES_SE_NECESITA_FOTO_DE_PANTALLA_EN_PS2\n",a);
    fflush(a);fclose(a);
}
