/* SpookyFlow V15.72: OPTIONAL PC arcade host for sets unavailable on PS3.
 * Bridge server is on the SAME PRIVATE LAN and separately runs MAME/FBNeo.
 * The PS3 never executes arbitrary remote commands or touches ROM/BIOS files.
 * All network interactions are opt-in (disabled config by default).
 *
 * HTTP is deliberately LAN-only and carries a one-time user-generated bearer
 * token (not printed in log or diagnostic). Do not port-forward this service.
 * The accepted PC game can be watched in a separate Moonlight PS3 session;
 * this feature does not claim direct client switching or 7000-game validation.
 */
#include "ps3_arcade_bridge.h"
#include <net/net.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#ifndef SF_PC_ARCADE_CONFIG
#define SF_PC_ARCADE_CONFIG "/dev_hdd0/game/SPKF00001/USRDIR/ARCADE/pc_bridge.cfg"
#endif

typedef struct {
    unsigned enabled,prefer_pc,port;
    unsigned octets[4];
    char token[120];
} BridgeConfig;

static void reason_set(char *out,size_t size,const char *str)
{
    if(out && size)snprintf(out,size,"%s",str?str:"");
}
static int is_private_ip(const unsigned a[4])
{
    if(a[0]==10 && a[1]<256)return 1;
    if(a[0]==172 && a[1]>=16 && a[1]<=31)return 1;
    if(a[0]==192 && a[1]==168)return 1;
    return 0; /* Never send bearer tokens to public IPs. */
}
static int read_config(BridgeConfig *c)
{
    FILE *f; char line[240];
    char ip[80]="";
    int k;
    memset(c,0,sizeof(*c));
    c->port=47371;
    f=fopen(SF_PC_ARCADE_CONFIG,"rb");
    if(!f)return 0;
    while(fgets(line,sizeof line,f)){
        char *equal=strchr(line,'=');
        char *p;
        if(!equal || line[0]=='#')continue;
        *equal++=0;p=equal;
        p[strcspn(p,"\r\n")]=0;
        while(*p==' ' || *p=='\t')p++;
        if(!strcmp(line,"ENABLED"))c->enabled=!strcmp(p,"1");
        else if(!strcmp(line,"PREFER_PC"))c->prefer_pc=!strcmp(p,"1");
        else if(!strcmp(line,"PC_IP"))snprintf(ip,sizeof ip,"%s",p);
        else if(!strcmp(line,"PC_PORT"))c->port=(unsigned)strtoul(p,NULL,10);
        else if(!strcmp(line,"TOKEN"))snprintf(c->token,sizeof c->token,"%s",p);
    }
    fclose(f);
    if(!c->enabled)return 0;
    if(c->port<1024 || c->port>65535)return -1;
    if(strlen(c->token)<24)return -1;
    for(k=0;c->token[k];k++)
        if(!isalnum((unsigned char)c->token[k]) &&
           c->token[k]!='_' && c->token[k]!='-')return -1;
    {
        unsigned a,b,d,e;
        char extra;
        if(sscanf(ip,"%u.%u.%u.%u%c",&a,&b,&d,&e,&extra)!=4 ||
           a>255 || b>255 || d>255 || e>255)return -1;
        c->octets[0]=a;c->octets[1]=b;c->octets[2]=d;c->octets[3]=e;
    }
    if(!is_private_ip(c->octets))return -1;
    return 1;
}
int sf_pc_arcade_preferred(void)
{
    BridgeConfig c;
    return read_config(&c)==1 && c.prefer_pc;
}
static int shortname(const char *path,char *code,size_t cap)
{
    const char *base,*end;
    size_t n,i;
    if(!path || strncmp(path,"/dev_hdd0/ARCADE/",sizeof("/dev_hdd0/ARCADE/")-1)!=0)return 0;
    base=strrchr(path,'/');base=base?base+1:path;
    n=strlen(base);
    if(n<5)return 0;
    end=base+n-4;
    if(strcasecmp(end,".zip")==0)n-=4;
    else if(n>=4 && strcasecmp(base+n-3,".7z")==0)n-=3;
    else return 0;
    if(n==0 || n>90 || n>=cap)return 0;
    for(i=0;i<n;i++){
        unsigned char ch=(unsigned char)base[i];
        if(!isalnum(ch) && ch!='_' && ch!='-' && ch!='+' && ch!='.')return 0;
        code[i]=(char)tolower(ch);
    }
    code[n]=0;
    return strcmp(code,"neogeo")!=0 && strcmp(code,"pgm")!=0;
}
int sf_pc_arcade_try_launch(const char *rom_path,char *reason,size_t cap)
{
    BridgeConfig cfg;
    char code[96],http[500],reply[640];
    int conf=read_config(&cfg);
    int len,sent=0,total=0,ok=0;
    s32 sock;
    struct sockaddr_in addr;
    struct timeval tv;
    if(conf==0){reason_set(reason,cap,"PC_BRIDGE_DESACTIVADO");return 0;}
    if(conf!=1){reason_set(reason,cap,"PC_BRIDGE_CONFIG_INVALIDA");return -1;}
    if(!shortname(rom_path,code,sizeof code)){
        reason_set(reason,cap,"PC_BRIDGE_NOMBRE_ROM_NO_VALIDO");
        return -2;
    }
    if(netInitialize()<0){reason_set(reason,cap,"PC_BRIDGE_RED_NO_INICIALIZA");return -3;}
    sock=netSocket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(sock<0){
        netDeinitialize();reason_set(reason,cap,"PC_BRIDGE_SOCKET_ERROR");return -3;
    }
    memset(&tv,0,sizeof tv);
    tv.tv_sec=3;tv.tv_usec=0;
    netSetSockOpt(sock,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);
    netSetSockOpt(sock,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);
    memset(&addr,0,sizeof addr);
    addr.sin_family=AF_INET;
    addr.sin_port=htons((unsigned short)cfg.port);
    addr.sin_addr.s_addr=htonl((cfg.octets[0]<<24) |
                               (cfg.octets[1]<<16) |
                               (cfg.octets[2]<<8) | cfg.octets[3]);
    if(netConnect(sock,(struct sockaddr*)&addr,sizeof addr)<0){
        reason_set(reason,cap,"PC_BRIDGE_SIN_CONEXION_LAN");goto done;
    }
    {
        char body[132];
        int bn=snprintf(body,sizeof body,"{\"game\":\"%s\"}",code);
        len=snprintf(http,sizeof http,
            "POST /launch HTTP/1.0\r\n"
            "Host: %u.%u.%u.%u\r\n"
            "X-Arcade-Token: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
            cfg.octets[0],cfg.octets[1],cfg.octets[2],cfg.octets[3],
            cfg.token,bn,body);
    }
    /* The token is intentionally never logged or included in the TXT. */
    if(len<=0 || len>=(int)sizeof http){
        reason_set(reason,cap,"PC_BRIDGE_HTTP_TOO_LONG");goto done;
    }
    while(sent<len){
        int n=(int)netSend(sock,http+sent,(size_t)(len-sent),0);
        if(n<=0){
            reason_set(reason,cap,"PC_BRIDGE_HTTP_SEND_FAILED");goto done;
        }
        sent+=n;
    }
    while(total<(int)sizeof(reply)-1){
        int n=(int)netRecv(sock,reply+total,sizeof(reply)-1-(size_t)total,0);
        if(n<=0)break;
        total+=n;
        reply[total]=0;
        if(strstr(reply,"\r\n\r\n") && strstr(reply,"\"ok\": true"))break;
    }
    reply[total]=0;
    if(total>16 &&
       (!strncmp(reply,"HTTP/1.0 200",12) || !strncmp(reply,"HTTP/1.1 200",12)) &&
       strstr(reply,"\"ok\": true")){
        reason_set(reason,cap,"PC_BRIDGE_JUEGO_INICIADO_ABRIR_MOONLIGHT_PS3");
        ok=1;
    }else reason_set(reason,cap,"PC_BRIDGE_ROM_NO_VERIFICADA_O_PC_RECHAZO");
done:
    netClose(sock);
    netDeinitialize();
    return ok?1:-4;
}
