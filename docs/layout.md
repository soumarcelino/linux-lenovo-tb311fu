# Layout

| Path | Contents |
| --- | --- |
| `kernel/` | Kernel config, patches and source revision |
| `adaptation/` | Hardware helpers and system configuration |
| `packages/archlinux/` | Arch Linux packaging notes |
| `profiles/plasma-mobile/` | Session settings and package list |
| `ports/postmarketos/` | Original APKBUILDs and deviceinfo |
| `scripts/` | Build, staging and diagnostic tools |
| `docs/` | Build, installation and device notes |
| `assets/` | Screenshots and diagrams |

Package recipes should reference the sources above; avoid duplicate payloads.
Keep the full kernel tree in a separate checkout with a pinned revision.
Keep generated files under ignored `build/`, `downloads/` and `dist/` directories.
Publish images as release assets.
