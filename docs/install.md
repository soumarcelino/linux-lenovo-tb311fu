# Device layout

The boot init in `adaptation/src/boot/arch-init.c` pivots into `/arch`
and starts systemd. The original root is mounted at `/.device-root` and
provides shared modules, firmware, vendor data and Waydroid storage.

`stage-rootfs.py` combines hardware configuration and the Plasma Mobile
profile into `build/rootfs/`. Account options render the session service,
runtime directory and process helpers for the selected user.
The Plasma service uses PAM autologin.

Install the libraries from `dist/userspace/` in the staged rootfs at their
matching paths. Runtime services use the modules built under `build/modules/`
and the device firmware from the original root.
