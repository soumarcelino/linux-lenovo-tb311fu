# Build

Kernel: Android Common `android15-6.6.142_r00`, commit
`1782065b5f3dad41d3785247e50820553c3473e6`.
Toolchain: Android Clang `r510928` (18.0.0, build 11368308).

```sh
export KERNEL_SRC="$HOME/Projects/tb311fu-kernel/common"
export CLANG_BIN="$HOME/Projects/tb311fu-kernel/toolchains/clang-r510928/bin"
./scripts/build-kernel
./scripts/build-modules gpu bluetooth scanout waydroid
./scripts/build-userspace
python3 scripts/stage-rootfs.py --user alarm --uid 1000 --gid 1000
```

Apply `kernel/patches/0001-host-openssl.patch` to the kernel checkout.
Build tools: make, Git, Python 3, OpenSSL development headers, flex, bison,
Clang and an AArch64 cross compiler (`aarch64-linux-gnu-gcc`).
Outputs go to `build/` and `dist/`.

`build-modules` also accepts `panel-tuning` and `video`.
Vendor modules go in `vendor-input/modules/`; compare their kernel symbol
CRCs with `scripts/check-vendor-abi.py build/kernel --report build/abi.json`.
`build-wifi-charging-driver.py` creates the charging-boot Wi-Fi driver.
`build-adb-root.py` builds the pinned USB root adbd from its source manifest.
