#ifndef SF_TEST_MBEDTLS_SHA1_H
#define SF_TEST_MBEDTLS_SHA1_H
#include <stddef.h>
int mbedtls_sha1_ret(const unsigned char *input,size_t n,unsigned char out[20]);
#endif
