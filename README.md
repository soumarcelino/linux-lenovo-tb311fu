# Linux on Lenovo Tab TB311FU

Arch Linux ARM port with KDE Plasma Mobile.
Uses a downstream Android kernel. Started as a postmarketOS port.

Working:

- Wi-Fi
- Bluetooth
- GPU acceleration (Panfrost)
- Touchscreen
- Power and volume buttons
- USB
- Audio
- Camera (low image quality)
- Brightness control
- Battery status and charging detection
- Waydroid

## Hardware

- Model: Lenovo Tab (TB311FU)
- SoC: MediaTek Helio G85
- CPU: 2× Cortex-A75 @ 2.0 GHz + 6× Cortex-A55 @ 1.8 GHz
- GPU: Mali-G52 MC2
- RAM: 4GB

## Development

Device kernel sources, hardware helpers and the Plasma Mobile profile are
included here. Build scripts produce the kernel, modules and rootfs overlay.

- [Build](docs/build.md)
- [Installation](docs/install.md)
- [Project layout](docs/layout.md)
- [Source migration](docs/migration.md)
- [References](docs/references.md)
