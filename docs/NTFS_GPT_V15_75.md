# SpookyFlow V15.75 — NTFS/GPT USB discovery (experimental)

Scope: **only USB partition recognition and one new diagnostic**. This change is
based on the exact V15.74 CRC/arcade branch and intentionally leaves all game
launchers, BDISO ISO mounting code, existing NTFS/MBR, FAT32, webMAN and
`raw_iso.sprx` unchanged.

## Control path

1. Start the existing `ntfsMountDevice()` (MBR) implementation.
2. Only if it found no mount, query USB disk geometry through LV2 storage 609.
3. Read the protective MBR, GPT headers and entries through LV2 600/602/601,
   in read-only mode using 64-bit LBAs; validate GPT header/array CRC32.
4. Locate the NTFS volume boot record; check sector size, bounds and geometry.
5. If the start LBA fits legacy `sec_t` (uint32), attempt `ntfsMount()` with
   `NTFS_READ_ONLY`, to avoid writes to 4+ TB GPT drives.
6. If not possible, keep the old mount state unchanged, log why, never claim
   GPT+NTFS is working merely because the USB geometry was detected.

Diagnostic: `/dev_hdd0/tmp/spookyflow/diagnostico_gpt.txt`. Existing startup
cleanup removes earlier diagnostic text files within the application's own
directory and `sf_gpt_diag_begin()` creates a fresh report on every launch.
No external USB or user data is removed by diagnostics.

## Limits

The installed IRISMAN-derived `libntfs_ext` ABI exposes 32-bit `sec_t` and
`ps3ntfs_file_to_sectors()` sector maps. The legacy `raw_iso.sprx` also uses
32-bit extent addresses. These original binaries are **not changed**.
Consequently, GPT identification on a disk of 4/6/8 TB does not prove that
files past 2 TiB can be accessed or mounted. On disks with 512-byte logical
sectors, 32-bit sector addresses wrap around near 2 TiB. GPT volumes are
experimental and read-only. No 4/6/8 TB full-capacity ISO support is claimed.

## Testing

- 27 independent parser fixtures on the host, including 4/6/8 TB, 512/4096
  byte sectors, CRC corruption, primary-to-backup recovery and legacy MBR.
- 20 focused C regression tests in `tests/test_v15_75_gpt.c`, compiled and
  executed in the GitHub Actions host-tests job.
- Actual PS3 HEN USB detection, NTFS mount and ISO playback require a **real
  console with a test disk**, and must not be reported successful without it.

The pre-existing V15.74 PKG remains unchanged. The branch is intended for
validation until its PS3 build and hardware results are known.
