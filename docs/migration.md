# Sources

`source-manifest.json` records imported paths and original SHA-256 hashes.
The sources come from the TB311FU development tree, with build paths adapted
to this repository and account settings rendered during rootfs staging.

- `kernel/modules/`: Panfrost, DRM scheduler, Bluetooth, scanout, panel and video.
- `adaptation/`: boot init, battery quirk, hardware helpers, camera and services.
- `profiles/plasma-mobile/`: session, package versions and touch recovery.
- `ports/postmarketos/`: original device and kernel package definitions.

Android Common is kept in a separate checkout at the revision in `build.md`.
