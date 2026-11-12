/* ps3_webman.c - send a web command to webMAN MOD on this console
 * (http://127.0.0.1/<command>): clean power off, and game launching.
 *
 * Verified against PSL1GHT ppu/include/net/net.h (netInitialize,
 * netDeinitialize, netSocket, netConnect, netSend, netRecv, netClose,
 * netSetSockOpt), net/socket.h (SOL_SOCKET, SO_RCVTIMEO, SO_SNDTIMEO) and
 * arpa/inet.h (htons/htonl are no-ops: the PS3 is big-endian). */
#include <net/net.h>
#include <net/netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "app.h"
#include "log.h"
#include "ps3_platform.h"
#ifdef SF_HAVE_TLS
#include <lv2/system.h>
/* random bytes from the console (TLS keys) */
static int sf_random(void *p, size_t n) { return sysGetRandomNumber(p, (u64)n) == 0 ? 0 : -1; }
#endif

/* connect to webMAN; returns the socket or -1 (network initialized on success) */
static s32 webman_connect_t(int timeout_s)
{
    struct sockaddr_in addr;
    struct timeval tv;
    s32 s;
    if (netInitialize() < 0) { log_printf("webman: network init failed"); return -1; }
    s = netSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) { log_printf("webman: socket failed (%d)", (int)s); netDeinitialize(); return -1; }
    tv.tv_sec = timeout_s; tv.tv_usec = 0;
    netSetSockOpt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    tv.tv_sec = 2;
    netSetSockOpt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = htonl(0x7F000001);          /* 127.0.0.1 */
    if (netConnect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_printf("webman: can't connect to 127.0.0.1:80 - is webMAN MOD running?");
        netClose(s);
        netDeinitialize();
        return -1;
    }
    return s;
}

static s32 webman_connect(void) { return webman_connect_t(2); }

/* Send a request that webMAN answers only later (a launch: it first waits for
 * the XMB). 0 = connected and sent. */
int ps3_webman_send(const char *command)
{
    char req[2300];
    s32 s;
    int n, rc = -1;
    n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n", command);
    if (n <= 0 || n >= (int)sizeof(req)) { log_printf("webman: request too long"); return -1; }
    s = webman_connect();
    if (s < 0) return -1;
    if (netSend(s, req, (size_t)n, 0) == n) {
        rc = 0;
        usleep(300 * 1000);            /* let webMAN read it before the socket closes */
        log_printf("webman: sent %s", command);
    } else {
        log_printf("webman: send failed for %s", command);
    }
    netClose(s);
    netDeinitialize();
    return rc;
}

int ps3_webman_get(const char *command)
{
    char req[512], resp[64];
    s32 s;
    int n, rc = -1;
    n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n", command);
    if (n <= 0 || n >= (int)sizeof(req)) { log_printf("webman: request too long"); return -1; }
    s = webman_connect();
    if (s < 0) return -1;
    if (netSend(s, req, (size_t)n, 0) == n) {
        /* any reply means webMAN took the command */
        n = (int)netRecv(s, resp, sizeof(resp) - 1, 0);
        if (n > 0) { resp[n] = '\0'; rc = 0; log_printf("webman: %s -> %.12s", command, resp); }
        else log_printf("webman: %s sent, no reply", command);
    } else {
        log_printf("webman: send failed for %s", command);
    }
    netClose(s);
    netDeinitialize();
    return rc;
}

/* ---- 0.9.2: HTTP GET from the internet (Download Missing Covers) ----
 * HTTP/1.0 (no chunked replies), one request per connection. The reply is
 * read into buf; the body is then moved to its start.
 * 0.9.3: HTTPS too, when the build has mbedTLS (SF_HAVE_TLS; ci/build_ps3.sh
 * builds it). The PS3's own SSL only knows ciphers today's servers refuse.
 * The server's certificate is not checked (there is no certificate store
 * here): fine for fetching public cover pictures, nothing private is sent. */
#ifdef SF_HAVE_TLS
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#endif

typedef struct {
    s32 s;
    int tls;
#ifdef SF_HAVE_TLS
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context ent;
    mbedtls_ctr_drbg_context drbg;
#endif
} HttpConn;

#ifdef SF_HAVE_TLS
static int tls_entropy(void *data, unsigned char *out, size_t len, size_t *olen)
{
    (void)data;
    if (sf_random(out, len) != 0) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    *olen = len;
    return 0;
}
static int tls_send(void *ctx, const unsigned char *b, size_t n)
{
    int r = (int)netSend(*(s32 *)ctx, b, n, 0);
    return r < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : r;
}
static int tls_recv(void *ctx, unsigned char *b, size_t n)
{
    int r = (int)netRecv(*(s32 *)ctx, b, n, 0);
    return r < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : r;
}
#endif

static void conn_close(HttpConn *c)
{
#ifdef SF_HAVE_TLS
    if (c->tls) {
        mbedtls_ssl_close_notify(&c->ssl);
        mbedtls_ssl_free(&c->ssl);
        mbedtls_ssl_config_free(&c->conf);
        mbedtls_ctr_drbg_free(&c->drbg);
        mbedtls_entropy_free(&c->ent);
        c->tls = 0;
    }
#endif
    if (c->s >= 0) netClose(c->s);
    c->s = -1;
}

/* 0 = connected (and the TLS handshake done) */
static int conn_open(HttpConn *c, const char *host, int port, int tls)
{
    struct hostent *he;
    struct sockaddr_in addr;
    struct timeval tv;
    memset(c, 0, sizeof(*c));
    c->s = -1;
    he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) return COVER_HTTP_DNS;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], 4);
    c->s = netSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c->s < 0) return COVER_HTTP_NET;
    tv.tv_sec = 10; tv.tv_usec = 0;
    netSetSockOpt(c->s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    netSetSockOpt(c->s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (netConnect(c->s, (struct sockaddr *)&addr, sizeof(addr)) < 0) { conn_close(c); return COVER_HTTP_NET; }
    if (!tls) return 0;
#ifdef SF_HAVE_TLS
    {
        int r;
        c->tls = 1;
        mbedtls_ssl_init(&c->ssl);
        mbedtls_ssl_config_init(&c->conf);
        mbedtls_ctr_drbg_init(&c->drbg);
        mbedtls_entropy_init(&c->ent);
        if (mbedtls_entropy_add_source(&c->ent, tls_entropy, NULL, 32, MBEDTLS_ENTROPY_SOURCE_STRONG) != 0 ||
            mbedtls_ctr_drbg_seed(&c->drbg, mbedtls_entropy_func, &c->ent, (const unsigned char *)"spookyflow", 10) != 0 ||
            mbedtls_ssl_config_defaults(&c->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
            log_printf("https: setup failed");
            conn_close(c);
            return COVER_HTTP_NET;
        }
        mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_NONE);
        mbedtls_ssl_conf_rng(&c->conf, mbedtls_ctr_drbg_random, &c->drbg);
        if (mbedtls_ssl_setup(&c->ssl, &c->conf) != 0 || mbedtls_ssl_set_hostname(&c->ssl, host) != 0) {
            log_printf("https: setup failed");
            conn_close(c);
            return COVER_HTTP_NET;
        }
        mbedtls_ssl_set_bio(&c->ssl, &c->s, tls_send, tls_recv, NULL);
        while ((r = mbedtls_ssl_handshake(&c->ssl)) != 0) {
            if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            log_printf("https: handshake with %s failed (-0x%04x)", host, (unsigned)-r);
            conn_close(c);
            return COVER_HTTP_NET;
        }
        return 0;
    }
#else
    conn_close(c);
    return COVER_HTTP_TLS;
#endif
}

static int conn_send(HttpConn *c, const char *buf, int n)
{
    int done = 0;
    while (done < n) {
        int r;
#ifdef SF_HAVE_TLS
        if (c->tls) {
            r = mbedtls_ssl_write(&c->ssl, (const unsigned char *)buf + done, (size_t)(n - done));
            if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        } else
#endif
        r = (int)netSend(c->s, buf + done, (size_t)(n - done), 0);
        if (r <= 0) return -1;
        done += r;
    }
    return 0;
}

/* > 0 bytes, 0 = the end, < 0 = error */
static int conn_recv(HttpConn *c, uint8_t *buf, uint32_t n)
{
#ifdef SF_HAVE_TLS
    if (c->tls) {
        for (;;) {
            int r = mbedtls_ssl_read(&c->ssl, buf, n);
            if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == MBEDTLS_ERR_SSL_CONN_EOF) return 0;
            return r;
        }
    }
#endif
    return (int)netRecv(c->s, buf, n, 0);
}

/* Some modern poster CDNs send chunked bodies even to HTTP/1.0 clients.
 * Turn the received body into a contiguous JSON/JPEG buffer before any
 * consumer validates the JPEG signature or parses its JSON. */
static int http_dechunk(uint8_t *body, uint32_t *len)
{
    uint32_t rd=0,wr=0,total=*len;
    for (;;) {
        uint32_t n=0;
        int digits=0;
        while(rd<total && body[rd]!='\r' && body[rd]!='\n') {
            unsigned char c=body[rd++];
            unsigned v;
            if(c==';') {
                while(rd<total && body[rd]!='\r' && body[rd]!='\n')rd++;
                break;
            }
            if(c>='0' && c<='9')v=c-'0';
            else if(c>='a' && c<='f')v=c-'a'+10;
            else if(c>='A' && c<='F')v=c-'A'+10;
            else return -1;
            if(n>0x0fffffffu)return -1;
            n=(n<<4)|v;
            digits++;
        }
        if(!digits || rd+1>=total || body[rd]!='\r' || body[rd+1]!='\n')
            return -1;
        rd+=2;
        if(n==0) {
            *len=wr;
            return 0;   /* optional trailers are irrelevant to the body */
        }
        if(n>total-rd || wr>total-n) return -1;
        memmove(body+wr,body+rd,n);
        wr+=n;rd+=n;
        if(rd+1>=total || body[rd]!='\r' || body[rd+1]!='\n')return -1;
        rd+=2;
    }
}

static int lower_eq(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return 1;
}

static int ps3_http_get_single(void *ctx, const char *url, uint8_t *buf, uint32_t cap, uint32_t *len, char *loc, size_t loc_cap)
{
    /* Movie image proxy requests carry a URL-encoded upstream poster URL.
     * 640 bytes silently rejected some long Amazon artwork URLs. */
    char host[128], req[1800];
    const char *p, *path;
    HttpConn c;
    uint32_t got = 0;
    int port, n, status = -1, tls, rc;
    (void)ctx;
    *len = 0;
    if (loc && loc_cap) loc[0] = '\0';
    if (strncmp(url, "https://", 8) == 0) { tls = 1; p = url + 8; port = 443; }
    else if (strncmp(url, "http://", 7) == 0) { tls = 0; p = url + 7; port = 80; }
    else return COVER_HTTP_NET;
#ifndef SF_HAVE_TLS
    if (tls) return COVER_HTTP_TLS;
#endif
    path = strchr(p, '/');
    if (!path) path = p + strlen(p);
    n = (int)(path - p);
    if (n <= 0 || n >= (int)sizeof(host)) return COVER_HTTP_NET;
    memcpy(host, p, (size_t)n); host[n] = '\0';
    {
        char *colon = strchr(host, ':');
        if (colon) { port = atoi(colon + 1); *colon = '\0'; }
    }
    n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: spookyflow/%s (PS3)\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
                 path[0] ? path : "/", host, APP_VERSION);
    if (n <= 0 || n >= (int)sizeof(req)) return COVER_HTTP_NET;
    if (netInitialize() < 0) return COVER_HTTP_NET;
    rc = conn_open(&c, host, port, tls);
    if (rc != 0) { netDeinitialize(); return rc; }
    if (conn_send(&c, req, n) != 0) { conn_close(&c); netDeinitialize(); return COVER_HTTP_NET; }
    for (;;) {
        int r;
        if (got >= cap) { conn_close(&c); netDeinitialize(); return COVER_HTTP_BIG; }
        r = conn_recv(&c, buf + got, cap - got);
        if (r < 0) { conn_close(&c); netDeinitialize(); return COVER_HTTP_NET; }
        if (r == 0) break;
        got += (uint32_t)r;
    }
    conn_close(&c);
    netDeinitialize();
    {   /* status line, headers, body */
        uint32_t i, hend = 0;
        int chunked = 0;
        for (i = 0; i + 3 < got; i++)
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') { hend = i + 4; break; }
        if (!hend || got < 12 || memcmp(buf, "HTTP/", 5) != 0) return COVER_HTTP_NET;
        {
            const char *sp = memchr(buf, ' ', hend);
            if (!sp) return COVER_HTTP_NET;
            status = atoi(sp + 1);
        }
        if (loc && loc_cap) {           /* Location: (redirects) */
            for (i = 0; i + 10 < hend; i++) {
                if ((i == 0 || buf[i - 1] == '\n') && lower_eq((const char *)buf + i, "location:", 9)) {
                    uint32_t j = i + 9, k = 0;
                    while (j < hend && buf[j] == ' ') j++;
                    while (j < hend && buf[j] != '\r' && k + 1 < loc_cap) loc[k++] = (char)buf[j++];
                    loc[k] = '\0';
                    break;
                }
            }
        }
        for (i=0;i+18<hend;i++) {
            if ((i==0 || buf[i-1]=='\n') &&
                lower_eq((const char *)buf+i,"transfer-encoding:",18)) {
                uint32_t j=i+18;
                while(j<hend && (buf[j]==' ' || buf[j]=='\t'))j++;
                if(j+7<hend && lower_eq((const char *)buf+j,"chunked",7))
                    chunked=1;
                break;
            }
        }
        *len = got - hend;
        memmove(buf, buf + hend, *len);
        if(chunked && http_dechunk(buf,len)!=0)return COVER_HTTP_NET;
    }
    return status;
}
/* PS3 game-cover and movie-cover workers share netInitialize/netDeinitialize.
 * Serialize public HTTP requests to avoid one worker deinitializing network
 * while the other has an open connection. */
static volatile int g_http_busy;
int ps3_http_get(void *ctx, const char *url, uint8_t *buf, uint32_t cap,
                 uint32_t *len, char *loc, size_t loc_cap)
{
    int result;
    while(__sync_lock_test_and_set(&g_http_busy,1)) usleep(1000);
    result=ps3_http_get_single(ctx,url,buf,cap,len,loc,loc_cap);
    __sync_lock_release(&g_http_busy);
    return result;
}

int ps3_webman_get_wait(const char *command, int timeout_s, char *out, size_t cap)
{
    char req[512];
    s32 s;
    int n, got = 0;
    if (out && cap) out[0] = '\0';
    n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n", command);
    if (n <= 0 || n >= (int)sizeof(req)) return -1;
    s = webman_connect_t(timeout_s);
    if (s < 0) return -1;
    if (netSend(s, req, (size_t)n, 0) == n) {
        char buf[2048];
        while (got < (int)sizeof(buf) - 1) {
            int r = (int)netRecv(s, buf + got, sizeof(buf) - 1 - (size_t)got, 0);
            if (r <= 0) break;
            got += r;
        }
        buf[got] = '\0';
        if (out && cap) {
            const char *body = strstr(buf, "\r\n\r\n");
            snprintf(out, cap, "%s", body ? body + 4 : buf);
        }
    }
    netClose(s);
    netDeinitialize();
    if (got <= 0) { log_printf("webman: %s: no answer", command); return -1; }
    return 0;
}
