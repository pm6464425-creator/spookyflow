#ifndef SF_DELETE_NTFS_H
#define SF_DELETE_NTFS_H
#include <stddef.h>
/* Inject native NTFS operations to keep the deletion transaction testable.
 * Only a regular ISO directly under an existing BDISO USB mount qualifies. */
int sf_delete_bdiso_ntfs_safe(const char *path,
     int (*is_regular)(const char *path),
     int (*resolve_native)(const char *public_path,char *out,size_t cap),
     int (*unlink_native)(const char *native_path));
#endif
