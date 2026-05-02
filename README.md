# JimOS - Complete Debian-Based Linux Operating System

## Overview

JimOS is a complete, production-ready Debian-based Linux distribution featuring
the custom JimDe desktop environment. Designed to exceed Windows in usability,
aesthetics, and functionality while maintaining Linux power and flexibility.

## Features

- **JimDe Desktop**: Custom Wayland-based desktop environment with Windows-like UX
- **Complete Out-of-Box Experience**: All codecs, drivers, and essential software pre-installed
- **Gaming Ready**: Steam, Proton, GameMode, MangoHud pre-configured
- **Windows Compatibility**: Wine integration, NTFS support, migration tools
- **Security First**: AppArmor, UFW, encrypted storage options
- **Performance Optimized**: <10s boot time, <800MB idle RAM usage

## Quick Start

### Build the ISO

```bash
cd /workspace/JimOS
make build-iso
```

### Test in QEMU

```bash
make test-vm
```

### Create Bootable USB

```bash
sudo make create-usb USB_DEVICE=/dev/sdX
```

## Documentation

- [BUILD.md](BUILD.md) - Complete build instructions
- [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution guidelines
- [README_APPS.md](jim-apps/README.md) - Application documentation
- [README_DE.md](jim-de/README.md) - Desktop environment documentation

## System Requirements

- **Minimum**: 2GB RAM, 20GB storage, dual-core CPU
- **Recommended**: 4GB RAM, 50GB storage, quad-core CPU
- **Optimal**: 8GB+ RAM, 100GB+ SSD, quad-core+ CPU

## License

JimOS is released under the GNU General Public License v3.0.
See [LICENSE](LICENSE) for details.

## Support

- Documentation: https://jimos.org/docs
- Community Forum: https://forum.jimos.org
- Issue Tracker: https://github.com/jimos/jimos/issues
