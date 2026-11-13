#ifndef SF_TEST_MBEDTLS_AES_H
#define SF_TEST_MBEDTLS_AES_H
#include <stddef.h>
typedef struct { unsigned char key[16]; } mbedtls_aes_context;
#define MBEDTLS_AES_ENCRYPT 1
#define MBEDTLS_AES_DECRYPT 0
void mbedtls_aes_init(mbedtls_aes_context *ctx);
void mbedtls_aes_free(mbedtls_aes_context *ctx);
int mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx, const unsigned char key[16], unsigned int bits);
int mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,int mode,const unsigned char in[16],unsigned char out[16]);
int mbedtls_aes_crypt_cbc(mbedtls_aes_context *ctx,int mode,size_t n,unsigned char iv[16],const unsigned char *in,unsigned char *out);
#endif
