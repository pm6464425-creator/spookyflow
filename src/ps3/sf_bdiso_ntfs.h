/* BDISO movies on NTFS: mount USB volumes for artwork and file discovery.
 * This does not replace or unload Cobra/webMAN plugins. */
#ifndef SF_BDISO_NTFS_H
#define SF_BDISO_NTFS_H
#include <stddef.h>
#include <stdint.h>
#include "scanner.h"
int sf_bdntfs_start(void);
/* Pure media-presence probes; never mount a volume or touch a removed disk. */
uint32_t sf_bdntfs_usb_present_mask(void);
int sf_bdntfs_index_online(const unsigned char *data, size_t len);
/* BDISO raw-sector Cobra mount: does not unload any existing plugin slot. */
int sf_bdntfs_mount_raw_iso(const char *path);
int sf_bdntfs_has_port(int port);
int sf_bdntfs_path(const char *public_path, char *native_path, size_t cap);
int sf_bdntfs_list(const char *dir, FsEntry *out, int max);
int sf_bdntfs_is_file(const char *path);
int sf_bdntfs_is_dir(const char *path);
int sf_bdntfs_read(const char *path, uint64_t off, void *data, uint32_t len);
int sf_bdntfs_load(const char *path, unsigned char **bytes, uint32_t *len, uint32_t max_bytes);
int sf_bdntfs_write(const char *path, const void *a, uint32_t alen, const void *b, uint32_t blen);
int sf_bdntfs_unlink(const char *path);
int sf_bdntfs_mkdir(const char *path);
#endif
