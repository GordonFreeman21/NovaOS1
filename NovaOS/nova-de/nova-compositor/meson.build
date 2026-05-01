[project]
name = 'nova-compositor'
version = '1.0.0'
description = 'NovaOS Wayland Compositor based on wlroots and smithay'
license = { text = 'GPL-3.0-or-later' }
default_options = ['buildtype=release']

[dependencies]
gtk4 = { version = '>=4.10', required = true }
libadwaita-1 = { version = '>=1.3', required = true }
wlroots = { version = '>=0.17', required = true }
wayland-server = { version = '>=1.21', required = true }
wayland-protocols = { version = '>=1.31', required = true }
pixman-1 = { version = '>=0.42', required = true }
libdrm = { version = '>=2.4', required = true }
egl = { version = '>=1.5', required = true }
gles2 = { version = '>=3.0', required = true }
libinput = { version = '>=1.20', required = true }
libudev = { version = '>=250', required = true }
dbus-1 = { version = '>=1.14', required = true }
systemd = { version = '>=250', required = false }
xkbcommon = { version = '>=1.5', required = true }
cairo = { version = '>=1.16', required = true }
pango = { version = '>=1.50', required = true }

[programs]
nova-compositor = { sources = ['src/main.rs', 'src/compositor.rs', 'src/render.rs', 'src/input.rs', 'src/output.rs', 'src/seat.rs', 'src/cursor.rs', 'src/xdg_shell.rs', 'src/layer_shell.rs', 'src/desktop.rs'], rust_args = ['--edition', '2021'] }

[install]
bindir = 'bin'
