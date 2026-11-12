#include "bdiso_usb.h"

static uint64_t usb_id(int port)
{
    /* Same USB_MASS_STORAGE layout as cobra.h, used here for host tests. */
    return (uint64_t)(port < 6 ? 0x10300000000000AULL + (uint64_t)port
                               : 0x10300000000001FULL + (uint64_t)(port - 6));
}

int bdiso_usb_pointer_port(const unsigned char *raw, size_t len)
{
    uint64_t be = 0, le = 0;
    int i, port;
    if (!raw || len < 8) return -1;
    for (i = 0; i < 8; i++) {
        be = (be << 8) | raw[i];
        le = (le << 8) | raw[7 - i];
    }
    for (port = 0; port < 8; port++)
        if (be == usb_id(port) || le == usb_id(port)) return port;
    return -1;
}

int bdiso_usb_pointer_visible(const unsigned char *raw, size_t len,
                              uint32_t present_mask)
{
    int port;
    present_mask &= 0xffu; /* Only the 8 removable NTFS ports. */
    if (!present_mask) return 0; /* No disk: hide all stale webMAN movies. */
    port = bdiso_usb_pointer_port(raw, len);
    /* Some valid legacy/index records have different headers or cannot be
     * read. Do not repeat V15.2's fail-closed bug: the disk is physically
     * present, so the original movie entry is kept. */
    if (port < 0) return 1;
    return (present_mask & (1u << port)) != 0;
}
