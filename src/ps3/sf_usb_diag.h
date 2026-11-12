/* Read-only BDISO/USB forensic report on the PS3 internal HDD. */
#ifndef SF_USB_DIAG_H
#define SF_USB_DIAG_H
#include "library.h"
#include "scanner.h"
#include <stdint.h>
int sf_usb_diag_begin(void);
void sf_usb_diag_snapshot(const char *phase, const Library *lib,
                          const Scanner *scan, uint32_t app_mask);
#endif
