# Scripts

- `build-kernel`: build the configured Android Common kernel.
- `build-modules`: build and sign device modules with the kernel key.
- `build-userspace`: compile the boot init and battery library.
- `stage-rootfs.py`: assemble rootfs configuration for a selected account.
- `check-vendor-abi.py`: compare vendor module symbol CRCs.
- `build-wifi-charging-driver.py`: patch the matching charging-boot Wi-Fi module.
- `build-adb-root.py`: build USB root adbd from pinned sources.
- `diagnostics/`: DRM capture, touch timing and display tracing on the tablet.
