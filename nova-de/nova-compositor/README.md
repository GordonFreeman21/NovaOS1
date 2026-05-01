# NovaOS Wayland Compositor

A modern, GPU-accelerated Wayland compositor built with Rust and wlroots/smithay.

## Features

- **Wayland Native**: Full Wayland protocol support with X11 compatibility via XWayland
- **GPU Acceleration**: OpenGL ES 2.0/3.0 rendering with DRM/KMS direct scanout
- **Multi-Monitor**: Support for multiple outputs with different resolutions and refresh rates
- **Input Handling**: Libinput integration for keyboard, mouse, touch, and tablet input
- **XDG Shell**: Modern window management with popup, fullscreen, and tiled states
- **Layer Shell**: Support for panels, overlays, backgrounds, and lock screens
- **Screen Capture**: Pipewire-based screen capture for screenshots and recording
- **DRM Lease**: VR headset and advanced display output support

## Building

### Prerequisites

```bash
sudo apt install \
    meson ninja-build cargo rustc \
    libwlroots-dev libwayland-dev libinput-dev \
    libdrm-dev libegl-dev libgles2-mesa-dev \
    libpixman-1-dev libxkbcommon-dev \
    libdbus-1-dev libsystemd-dev \
    libgtk-4-dev libadwaita-1-dev
```

### Build Commands

```bash
meson setup build --prefix=/usr
ninja -C build
sudo ninja -C build install
```

## Configuration

Configuration file location: `~/.config/nova/compositor.conf`

```toml
[general]
backend = "auto"  # auto, drm, wayland, x11
xwayland = true
idle_timeout = 300

[input]
natural_scroll = false
accel_profile = "adaptive"
scroll_factor = 1.0

[output]
# Auto-configure all outputs
mode = "auto"

[keybindings]
# Window management
alt_tab = "cycle_windows"
alt_f4 = "close_window"
super_return = "spawn terminal"
super_d = "toggle_desktop"

# Screenshots
print = "screenshot_full"
alt_print = "screenshot_window"
shift_print = "screenshot_region"
```

## Architecture

```
src/
├── main.rs           # Entry point and initialization
├── compositor.rs     # Core compositor state and event loop
├── render.rs         # OpenGL ES rendering pipeline
├── input.rs          # Input device handling
├── output.rs         # Display output management
├── seat.rs           # Input seat configuration
├── cursor.rs         # Cursor theme and rendering
├── xdg_shell.rs      # XDG shell window management
├── layer_shell.rs    # Layer shell protocol handler
└── desktop.rs        # Desktop environment integration
```

## License

GPL-3.0-or-later
