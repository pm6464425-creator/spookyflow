/* BDISO removable-drive index compatibility (portable, no PS3 I/O).
 * webMAN/prepISO can store a rawseciso USB ID in either byte ordering.
 * Unknown/short legacy records must NOT hide a movie while a USB drive
 * is physically present; absent media must never show cached entries.
 */
#ifndef SF_BDISO_USB_H
#define SF_BDISO_USB_H
#include <stddef.h>
#include <stdint.h>
int bdiso_usb_pointer_port(const unsigned char *raw, size_t len);
int bdiso_usb_pointer_visible(const unsigned char *raw, size_t len, uint32_t present_mask);
#endif
