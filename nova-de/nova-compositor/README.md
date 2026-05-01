# NovaCompositor - Wayland Compositor for NovaDe

High-performance Wayland compositor written in C with assembly optimizations for the NovaOS desktop environment.

## Features

- **Wayland Protocol**: Full Wayland compositor using wlroots
- **Assembly Optimizations**: SIMD-optimized rendering (SSE, AVX, AVX2)
- **Smooth Animations**: Spring physics and bezier curve animations at 60 FPS
- **Multi-Monitor Support**: Full output layout management
- **Advanced Input**: Keyboard, pointer, and touch input handling
- **Window Management**: Snap layouts, virtual desktops, tiling support

## Architecture

```
src/
├── nova-compositor.c    # Core compositor server and event loop
├── nova-input.c         # Input device handling (keyboard, pointer)
├── nova-output.c        # Output/monitor management
├── nova-view.c          # Window/view management
├── nova-render.c        # GPU-accelerated rendering
├── nova-keybindings.c   # Keyboard shortcut system
└── nova-animations.c    # Animation engine with spring physics

include/
└── nova-compositor.h    # Public API header
```

## Building

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt install meson ninja-build \
    libwayland-dev wayland-protocols \
    libwlroots-dev libpixman-1-dev \
    libxkbcommon-dev libdrm-dev \
    libegl-dev libgles-dev libgbm-dev
```

### Build Commands

```bash
# Configure
meson setup build --buildtype=release

# Compile
ninja -C build

# Install
sudo ninja -C build install
```

## CPU Optimizations

The compositor automatically detects and uses available CPU features:

| Feature | Flag | Benefit |
|---------|------|---------|
| SSE | `-msse` | Basic SIMD operations |
| SSE2 | `-msse2` | Integer SIMD |
| SSE4.1 | `-msse4.1` | Enhanced blending |
| AVX | `-mavx` | Wider vector ops |
| AVX2 | `-mavx2` | Best performance |

## Keybindings

| Shortcut | Action |
|----------|--------|
| `Super+Enter` | Open terminal |
| `Super+Q` | Close window |
| `Alt+Tab` | Switch windows |
| `Super+[1-9]` | Switch workspace |
| `Super+L` | Lock screen |
| `Super+D` | Show desktop |
| `Super+Shift+Arrows` | Snap windows |

## Performance Targets

- Boot to desktop: < 10 seconds
- Idle RAM usage: < 50 MB (compositor only)
- Frame latency: < 16.67 ms (60 FPS)
- Input latency: < 5 ms

## License

GPL-3.0-or-later

## Copyright

Copyright (C) 2024 NovaOS Project
