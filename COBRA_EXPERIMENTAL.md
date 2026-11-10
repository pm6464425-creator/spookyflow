# SpookyFlow Cobra backend (experimental)

This branch contains a source-level PS3HEN/Cobra mount adapter.
It is **NOT VERIFIED ON PS3 HARDWARE**. Do not assume an Actions green build
proves HEN compatibility. Build and test only with a safe homebrew test game.

- Main branch and original repository remain unchanged.
- PS3 ISO: internal /dev_hdd0/PS3ISO and FAT32 USB /dev_usb00N/PS3ISO.
- PS3 folder: internal /dev_hdd0/GAMES, /dev_hdd0/GAMEZ and FAT32 USB equivalents.
- PS1, PS2 and external NTFS are not supported by the new Cobra callback.
- No webMAN MOD runtime, FTP, fan-control, system-flash writes or boot plugins.
- Normal game flow: select game -> mount -> return to XMB, then launch via virtual disc.
- For logs: /dev_hdd0/tmp/spookyflow/log.txt

Main integration is a new optional AppHost.launch_ps3 callback. Existing
SpookyFlow core test behavior remains available when the callback is absent.
The official SpookyFlow's other webMAN-related features such as menu power-off
or NTFS scanning are disabled in the PS3 host. Original visuals are unchanged.

This is an engineering prototype. Failures may require a reboot.
The 128-KiB blank ISO / Cobra mounting technique is adapted from:
https://github.com/aldostools/IRISMAN (GPLv3 or later)
Distribute derived source and license notices as required.
