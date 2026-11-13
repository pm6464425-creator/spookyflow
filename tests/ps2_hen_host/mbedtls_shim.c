/* Exact mbedTLS AES/SHA1 signatures via independent OpenSSL test backend.
 * Host-test-only; NOT linked into the PS3 PKG.
 */
#include "mbedtls/aes.h"
#include "mbedtls/sha1.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <string.h>
void mbedtls_aes_init(mbedtls_aes_context *ctx){memset(ctx,0,sizeof(*ctx));}
void mbedtls_aes_free(mbedtls_aes_context *ctx){memset(ctx,0,sizeof(*ctx));}
int mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx,const unsigned char *key,unsigned int bits)
{if(bits!=128)return -1;memcpy(ctx->key,key,16);return 0;}
static int crypt(const unsigned char *key,int mode,int cbc,size_t n,unsigned char *iv,
                 const unsigned char *in,unsigned char *out)
{
    EVP_CIPHER_CTX *x=EVP_CIPHER_CTX_new();
    int k=0,l=0,ok=0;
    if(!x || n>0x7fffffffU)goto done;
    if(mode==MBEDTLS_AES_ENCRYPT) {
        if(EVP_EncryptInit_ex(x,cbc?EVP_aes_128_cbc():EVP_aes_128_ecb(),NULL,key,cbc?iv:NULL)!=1)goto done;
        if(EVP_CIPHER_CTX_set_padding(x,0)!=1)goto done;
        if(EVP_EncryptUpdate(x,out,&k,in,(int)n)!=1 || k!=(int)n)goto done;
        if(EVP_EncryptFinal_ex(x,out+k,&l)!=1 || l!=0)goto done;
        if(cbc)memcpy(iv,out+n-16,16);
    }else{
        if(EVP_DecryptInit_ex(x,cbc?EVP_aes_128_cbc():EVP_aes_128_ecb(),NULL,key,cbc?iv:NULL)!=1)goto done;
        if(EVP_CIPHER_CTX_set_padding(x,0)!=1)goto done;
        if(EVP_DecryptUpdate(x,out,&k,in,(int)n)!=1 || k!=(int)n)goto done;
        if(EVP_DecryptFinal_ex(x,out+k,&l)!=1 || l!=0)goto done;
    }
    ok=1;
done:
    if(x)EVP_CIPHER_CTX_free(x);
    return ok?0:-1;
}
int mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,int mode,const unsigned char *in,unsigned char *out)
{return crypt(ctx->key,mode,0,16,NULL,in,out);}
int mbedtls_aes_crypt_cbc(mbedtls_aes_context *ctx,int mode,size_t n,unsigned char *iv,const unsigned char *in,unsigned char *out)
{return n>=16&&n%16==0?crypt(ctx->key,mode,1,n,iv,in,out):-1;}
int mbedtls_sha1_ret(const unsigned char *input,size_t n,unsigned char out[20])
{return SHA1(input,n,out)?0:-1;}
