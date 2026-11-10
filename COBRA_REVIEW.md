## Engineering review: Cobra launch integration

This branch preserves the original SpookyFlow interface, and adds an optional
PS3-only local Cobra launch callback.

Important findings after first build:
- Previous revision 18fc1e64 unintentionally disabled optional host services
  (power off, NTFS refresh, webMAN config) and intercepted PS1/PS2.
- Those host callbacks have been restored to upstream behavior. PS3 mounting
  is still handled by the direct Cobra backend, without webMAN MOD.
- Original PS1/PS2 launching and some optional utility features STILL rely on
  webMAN as in upstream. They are not covered by the PS3 launcher.
- A successful CI build is not proof of mounting correctness, nor proof of
  crash-free PS3HEN behavior.
- Mount failure rollback cannot restore a previously mounted disc. A reboot
  might be needed after a failed mount. Do not treat it as transaction-safe.
- Current 'host-tests' cover the original portable core. New static integration
  guards are run before packaging but do not exercise LV2 syscalls on a PS3.
- The new folder-mount implementation uses a blank ISO and syscall 35.
  Its runtime behavior remains unverified.

Do not install onto the main PS3 without an installation backup and a
controlled test environment. The earlier 18fc1e64 build is superseded.
