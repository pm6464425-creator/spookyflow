/* Safe authorization for deleting a selected user game in MI PS3.
 * Pure C: no I/O; source paths from the game library are never trusted blindly.
 * Only the file-system backend performs the actual deletion.
 */
#ifndef SF_DELETE_POLICY_H
#define SF_DELETE_POLICY_H
enum {
    SF_DEL_OK=0,
    SF_DEL_DENIED=-1,
    SF_DEL_UNSUPPORTED=-2
};
int sf_delete_path_allowed(int platform, const char *path, int is_dir);
#endif
