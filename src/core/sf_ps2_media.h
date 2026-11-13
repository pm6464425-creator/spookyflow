/* PS2 disc region and sector-layout selection, independent of firmware.
 * Region is identified by SYSTEM.CNF disc ID, never by filename alone.
 * Generic 2048 ISO images are NOT assumed to be CD just because small.
 */
#ifndef SF_PS2_MEDIA_H
#define SF_PS2_MEDIA_H
enum SfPs2ImageMode {
    SF_PS2_IMAGE_DVD_2048=1,
    SF_PS2_IMAGE_CD_RAW_2352=2,
    SF_PS2_IMAGE_CD_COOKED_2048=3
};
const char *sf_ps2_region(const char *id);
int sf_ps2_is_known_cooked_cd(const char *id);
int sf_ps2_select_image_mode(const char *id,int has_2048_pvd,int has_2352_pvd);
#endif
