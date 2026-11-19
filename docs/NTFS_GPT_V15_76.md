# V15.76 — PS3 reports zero sectors (field diagnosis 2026-11-19)

The physical PS3 HEN report for USB000 showed:
- sys_storage_get_device_info returned success
- total_sectors = 0
- logical sector size = 512
- V15.75 rejected the drive before opening or reading it

This patch preserves the existing NTFS/MBR discovery. When device sector
size is 512 or 4096 but total sectors are 0, the alternative GPT path:

1. Opens the USB device through LV2 (600) without writing to it.
2. Attempts to READ LBA 0 and LBA 1 through LV2 (602).
3. Validates the protective MBR, GPT header signature/version, current-LBA,
   GPT CRC32, bounded partition geometry and partition-entry metadata.
4. Infers logical capacity from the alternate GPT-header LBA + 1 *only when
   the primary header validates*. This is an estimate, not a hardware size.
5. Uses the existing GPT parser and NTFS read-only mount fallback as before.
6. Logs the raw read failure return code, LBA and completed-sector count if
   the PS3 cannot read even the first two sectors.

Each launch replaces
`/dev_hdd0/tmp/spookyflow/diagnostico_gpt.txt`. The other application
diagnostics are cleaned only inside the SpookyFlow temporary directory.

**Limitations:** Historic PS3 developer reports show zero-capacity results
and failing storage_read for multi-TB external USB media. A valid GPT header
does NOT prove physical sectors above 2 TiB can be read, and the bundled
NTFS sector interface and raw_iso maps remain 32-bit. This version does NOT
patch LV2/HEN, NTFS library binaries, or raw_iso. No high-LBA ISO playback
support is promised. Check logs from a physical PS3 HEN before claiming
working multi-TB NTFS support.

Build/host tests: the existing GPT regression cases plus 15 additional
zero-capacity, CRC, header-bound and read-error checks.
