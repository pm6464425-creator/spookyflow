#ifndef SF_DELETE_ARCADE_GUARD_H
#define SF_DELETE_ARCADE_GUARD_H
/* 1 = independently deletable ROM; 0 = shared BIOS/parent or uncertain. */
int sf_delete_arcade_allowed(const char *path);
#endif
