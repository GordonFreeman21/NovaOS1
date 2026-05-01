# NovaOS Build Instructions

Complete step-by-step guide to build NovaOS from source.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Build Environment Setup](#build-environment-setup)
3. [Building the Base System](#building-the-base-system)
4. [Compiling NovaDe Desktop Environment](#compiling-novade-desktop-environment)
5. [Building NovaOS Applications](#building-novaos-applications)
6. [Creating the ISO](#creating-the-iso)
7. [Testing](#testing)
8. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware Requirements

- **Host System**: Debian 12 (Bookworm) or Ubuntu 22.04+
- **RAM**: Minimum 8GB (16GB recommended)
- **Storage**: Minimum 50GB free space (100GB recommended)
- **CPU**: Quad-core or better
- **Internet**: Required for downloading packages

### Software Dependencies

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install essential build tools
sudo apt install -y \
    build-essential \
    git \
    wget \
    curl \
    rsync \
    squashfs-tools \
    xorriso \
    isolinux \
    syslinux-efi \
    grub-pc-bin \
    grub-efi-amd64-bin \
    mtools \
    dosfstools
```

---

## Build Environment Setup

### Step 1: Install live-build

```bash
sudo apt install -y live-build debootstrap cdebootstrap
```

### Step 2: Clone and Navigate

```bash
cd /workspace/NovaOS
```

### Step 3: Set Up Build Configuration

The build system uses the `live-build` framework with custom configurations.
All configuration files are in `base-system/live-build-config/`.

### Step 4: Install Additional Dependencies

```bash
# For compiling NovaDe components
sudo apt install -y \
    meson \
    ninja-build \
    cargo \
    rustc \
    libgtk-4-dev \
    libadwaita-1-dev \
    libwlroots-dev \
    wayland-protocols \
    libwayland-dev \
    libxkbcommon-dev \
    libpixman-1-dev \
    libdrm-dev \
    libegl-dev \
    libgles2-mesa-dev \
    libinput-dev \
    libudev-dev \
    libdbus-1-dev \
    libsystemd-dev \
    libglib2.0-dev \
    libjson-glib-dev \
    libpolkit-gobject-1-dev \
    libupower-glib-dev \
    libnm-dev \
    libpulse-dev \
    libbluetooth-dev \
    libcanberra-dev \
    libaccountsservice-dev \
    liblightdm-gobject-1-dev \
    libgdm-dev \
    libclutter-1.0-dev \
    libcogl-dev \
    libmutter-dev \
    pkg-config \
    cmake \
    debhelper \
    devscripts \
    lintian
```

---

## Building the Base System

### Step 1: Configure live-build

```bash
cd /workspace/NovaOS/base-system/live-build-config

# The configuration is already set up with:
# - config/bootstrap.conf
# - config/chroot.conf
# - config/binary.conf
# - config/common.conf
# - package lists in config/package-lists/
```

### Step 2: Initialize Build Directory

```bash
cd /workspace/NovaOS

# Create symbolic links for live-build to find configs
ln -sf base-system/live-build-config/config ./config

# Or copy configurations
# cp -r base-system/live-build-config/config ./
```

### Step 3: Download and Build

```bash
# Clean any previous builds
lb clean --purge 2>/dev/null || true

# Bootstrap the base system
sudo lb bootstrap

# Chroot stage - install packages
sudo lb chroot

# Binary stage - create ISO
sudo lb binary
```

### Alternative: Use Makefile

```bash
cd /workspace/NovaOS
make build-iso
```

This automates all the above steps.

---

## Compiling NovaDe Desktop Environment

### Step 1: Build Nova Compositor (Wayland)

```bash
cd /workspace/NovaOS/nova-de/nova-compositor

# Configure
meson setup build --prefix=/usr

# Compile
ninja -C build

# Create .deb package
cd build
sudo checkinstall -D --install=no \
    --pkgname=nova-compositor \
    --pkgversion=1.0.0 \
    --pkggroup=x11 \
    --requires="libwlroots-dev,libwayland-dev,libinput-dev" \
    ninja install

# Install the package
sudo dpkg -i nova-compositor_1.0.0_amd64.deb
```

### Step 2: Build Nova Panel

```bash
cd /workspace/NovaOS/nova-de/nova-panel

meson setup build --prefix=/usr
ninja -C build

cd build
sudo checkinstall -D --install=no \
    --pkgname=nova-panel \
    --pkgversion=1.0.0 \
    --pkggroup=x11 \
    --requires="libgtk-4-dev,libadwaita-1-dev" \
    ninja install

sudo dpkg -i nova-panel_1.0.0_amd64.deb
```

### Step 3: Build Remaining DE Components

Repeat similar process for:
- nova-launcher
- nova-settings
- nova-notifications
- nova-lockscreen
- nova-greeter
- nova-wm

Or use the master build script:

```bash
cd /workspace/NovaOS
make build-de
```

---

## Building NovaOS Applications

### Build All Applications

```bash
cd /workspace/NovaOS
make build-apps
```

### Build Individual Applications

```bash
# Example: Nova Terminal
cd /workspace/NovaOS/nova-apps/nova-terminal
meson setup build --prefix=/usr
ninja -C build
cd build
sudo checkinstall -D --install=no \
    --pkgname=nova-terminal \
    --pkgversion=1.0.0 \
    ninja install
```

---

## Creating the ISO

### Method 1: Using Makefile (Recommended)

```bash
cd /workspace/NovaOS
make build-iso
```

Output will be in: `./binary.hybrid.iso`

### Method 2: Manual Build

```bash
cd /workspace/NovaOS

# Ensure all packages are built and in packages/ directory
make build-all

# Run live-build
sudo lb config \
    --mode debian \
    --distribution bookworm \
    --archive-areas "main contrib non-free non-free-firmware" \
    --debian-installer none \
    --bootloader grub \
    --debian-installer-distribution bookworm \
    --apt-indices false \
    --memtest none \
    --iso-application "NovaOS" \
    --iso-publisher "NovaOS Project" \
    --iso-volume "NOVAOS_INSTALL"

# Copy custom configurations
cp -r base-system/live-build-config/config/* ./config/

# Build
sudo lb bootstrap
sudo lb chroot
sudo lb binary
```

---

## Testing

### Test in QEMU

```bash
cd /workspace/NovaOS
make test-vm
```

Or manually:

```bash
qemu-system-x86_64 \
    -m 4096 \
    -cpu host \
    -enable-kvm \
    -drive file=binary.hybrid.iso,format=raw,if=ide \
    -boot d \
    -vga virtio \
    -usb \
    -device usb-tablet
```

### Test in VirtualBox

```bash
# Create new VM
VBoxManage createvm --name "NovaOS Test" --register
VBoxManage modifyvm "NovaOS Test" --memory 4096
VBoxManage modifyvm "NovaOS Test" --cpus 2
VBoxManage storagectl "NovaOS Test" --name "IDE Controller" --add ide
VBoxManage createhd --filename ~/VirtualBox\ VMs/NovaOS\ Test/disk.vdi --size 50000
VBoxManage storageattach "NovaOS Test" --storagectl "IDE Controller" --port 0 --device 0 --type hdd --medium ~/VirtualBox\ VMs/NovaOS\ Test/disk.vdi
VBoxManage storageattach "NovaOS Test" --storagectl "IDE Controller" --port 1 --device 0 --type dvddrive --medium binary.hybrid.iso

# Start VM
VBoxHeadless --startvm "NovaOS Test"
```

---

## Create Bootable USB

```bash
cd /workspace/NovaOS
sudo make create-usb USB_DEVICE=/dev/sdX
```

**WARNING**: Replace `/dev/sdX` with your actual USB device (e.g., `/dev/sdb`).
This will erase all data on the device!

Manual method:

```bash
sudo dd if=binary.hybrid.iso of=/dev/sdX bs=4M status=progress oflag=sync
sudo sync
```

---

## Troubleshooting

### Build Fails During Bootstrap

```bash
# Check internet connection
ping -c 3 debian.org

# Clear cache and retry
sudo lb clean --cache
sudo lb bootstrap
```

### Package Installation Fails

```bash
# Check for broken packages
sudo apt-get check

# Fix broken dependencies
sudo apt-get install -f

# Retry chroot
sudo lb chroot
```

### ISO Too Large

Remove unnecessary packages from package lists:
```bash
# Edit package lists
nano base-system/live-build-config/config/package-lists/my.list.chroot
```

### Memory Issues During Build

Add swap space:
```bash
sudo fallocate -l 8G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Wayland Compositor Won't Start

Check dependencies:
```bash
ldd /usr/bin/nova-compositor | grep "not found"
```

Install missing libraries and rebuild.

---

## Post-Build Verification

After building, verify the ISO:

```bash
# Check ISO integrity
isoinfo -d -i binary.hybrid.iso

# Mount and inspect
sudo mount -o loop binary.hybrid.iso /mnt
ls -la /mnt
sudo umount /mnt

# Verify package list
unsquashfs -l binary.hybrid.iso | grep pool
```

---

## Automated Full Build

For a complete automated build:

```bash
cd /workspace/NovaOS

# One command to build everything
make all

# This will:
# 1. Build all DE components
# 2. Build all applications
# 3. Create .deb packages
# 4. Configure live-build
# 5. Generate ISO
# 6. Place ISO in output directory
```

Build time: Approximately 2-4 hours depending on hardware.

---

## Customization

### Change Default Wallpaper

Edit: `branding/gtk-theme/gtk-4.0/settings.ini`

### Modify Package Selection

Edit: `base-system/live-build-config/config/package-lists/`

### Change Theme Colors

Edit: `branding/gtk-theme/gtk-4.0/gtk.css`

### Add Custom Packages

Add to: `base-system/live-build-config/config/package-lists/custom.list.chroot`

---

## Build Output Files

After successful build:

```
/workspace/NovaOS/
├── binary.hybrid.iso          # Main ISO file (~3-4GB)
├── binary.log                 # Build log
├── chroot/                    # Chroot environment (can be deleted after build)
├── binary/                    # Binary build artifacts
└── packages/                  # Built .deb packages
```

---

## Next Steps

After building and testing:

1. Report any issues to the issue tracker
2. Contribute improvements via pull requests
3. Share feedback on community forums
4. Help document features and workflows

For contribution guidelines, see [CONTRIBUTING.md](../CONTRIBUTING.md).
