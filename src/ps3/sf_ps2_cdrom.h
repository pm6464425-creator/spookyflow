/* SPDX-License-Identifier: GPL-3.0-or-later
 * Generate complete CD-ROM XA Mode2 Form1 2352-byte data sectors from
 * a single-track cooked 2048-byte ISO, without modifying source images.
 * ECC/EDC based on Neill Corlett's unecm.c (GNU GPL) and PSX CD standard.
 * NOTE: a cooked ISO cannot restore lost CD audio, subchannel or Form2 data.
 */
#ifndef SF_PS2_CDROM_H
#define SF_PS2_CDROM_H
#include <stdint.h>
void sf_cdrom_init(void);
void sf_cdrom_mode2_form1(uint8_t output[2352],
                          const uint8_t userdata[2048], uint32_t lba);
#endif
