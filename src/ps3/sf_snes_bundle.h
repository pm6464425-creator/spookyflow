#ifndef SF_SNES_BUNDLE_H
#define SF_SNES_BUNDLE_H
/* Prepare bundled pSNES 7.2 for an independent (no webMAN) PS3 launch.
 * Copies only our bundled assets. Existing PSNE00001 is never overwritten.
 * Returns 0 only if the destination PS3 SELF is verified by SHA256. */
int sf_psnes_prepare(void);
#endif
