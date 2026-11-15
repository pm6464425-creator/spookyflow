#ifndef SF_PS3_SNES_LAUNCH_H
#define SF_PS3_SNES_LAUNCH_H
/* On success webMAN accepted the staged-ROM launch. Caller exits to XMB. */
int sf_snes_launch(const char *rom_path);
#endif
