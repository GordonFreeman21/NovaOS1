project('jim-compositor', 'c',
  version: '1.0.0',
  license: 'GPL-3.0-or-later',
  default_options: [
    'c_std=c11',
    'warning_level=3',
    'werror=false',
    'optimization=2',
    'b_lto=true'
  ]
)

# Compiler checks
cc = meson.get_compiler('c')

# Check for SIMD support
has_sse = cc.has_argument('-msse')
has_sse2 = cc.has_argument('-msse2')
has_sse3 = cc.has_argument('-msse3')
has_ssse3 = cc.has_argument('-mssse3')
has_sse41 = cc.has_argument('-msse4.1')
has_sse42 = cc.has_argument('-msse4.2')
has_avx = cc.has_argument('-mavx')
has_avx2 = cc.has_argument('-mavx2')

message('CPU Optimization Support:')
message('  SSE: ' + has_sse.to_string())
message('  SSE2: ' + has_sse2.to_string())
message('  SSE4.1: ' + has_sse41.to_string())
message('  AVX: ' + has_avx.to_string())
message('  AVX2: ' + has_avx2.to_string())

# Build flags for optimizations
simd_args = []
if has_avx2
  simd_args += '-mavx2'
elif has_avx
  simd_args += '-mavx'
elif has_sse41
  simd_args += '-msse4.1'
elif has_sse2
  simd_args += '-msse2'
endif

# Dependencies
wayland = dependency('wayland-server', version: '>=1.20.0')
wayland_protos = dependency('wayland-protocols', version: '>=1.27')

pixman = dependency('pixman-1', version: '>=0.40.0')
xkbcommon = dependency('xkbcommon', version: '>=1.4.0')
drm = dependency('libdrm', version: '>=2.4.109')

wlroots_ver = '>=0.16.0'
wlroots = dependency('wlroots-0.16', version: wlroots_ver, required: false)
if not wlroots.found()
  wlroots = dependency('wlroots', version: wlroots_ver)
endif

# Required system libraries
pthread = dependency('threads')
math_lib = cc.find_library('m')

# Collect all dependencies
deps = [
  wayland,
  pixman,
  xkbcommon,
  drm,
  wlroots,
  pthread,
  math_lib
]

# Include directories
inc = include_directories('include')

# Source files
sources = files(
  'src/jim-compositor.c',
  'src/jim-input.c',
  'src/jim-output.c',
  'src/jim-view.c',
  'src/jim-render.c',
  'src/jim-keybindings.c',
  'src/jim-animations.c'
)

# Header files
headers = files(
  'include/jim-compositor.h'
)

# Main executable
executable('jim-compositor',
  sources,
  include_directories: inc,
  dependencies: deps,
  c_args: [
    '-DNOVA_COMPOSITOR_VERSION="@0@"'.format(meson.project_version()),
    '-D_GNU_SOURCE',
    '-fno-omit-frame-pointer'
  ] + simd_args,
  link_args: ['-Wl,--as-needed'],
  install: true,
  install_dir: get_option('prefix') / 'bin'
)

# Install headers
install_headers(headers, subdir: 'jim-de')

# Configuration file
conf_data = configuration_data()
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set('HAVE_SSE', has_sse ? 1 : 0)
conf_data.set('HAVE_SSE2', has_sse2 ? 1 : 0)
conf_data.set('HAVE_AVX', has_avx ? 1 : 0)
conf_data.set('HAVE_AVX2', has_avx2 ? 1 : 0)

configure_file(
  output: 'config.h',
  configuration: conf_data,
  install_dir: get_option('prefix') / 'include' / 'jim-de'
)

# pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  name: 'jim-compositor',
  description: 'JimDe Wayland Compositor',
  version: meson.project_version(),
  filebase: 'jim-compositor',
  subdirs: ['jim-de'],
  requires_private: [
    'wayland-server',
    'pixman-1',
    'xkbcommon',
    'libdrm',
    'wlroots-0.16'
  ]
)

# Tests
if get_option('buildtype') != 'release'
  test_exe = executable('test-compositor',
    'tests/test-main.c',
    include_directories: inc,
    dependencies: deps,
    c_args: simd_args
  )
  
  test('basic', test_exe)
endif

# Summary
summary({
  'Version': meson.project_version(),
  'Prefix': get_option('prefix'),
  'Build type': get_option('buildtype'),
  'SSE support': has_sse,
  'SSE2 support': has_sse2,
  'AVX support': has_avx,
  'AVX2 support': has_avx2,
  'LTO': get_option('b_lto'),
}, section: 'User defined options')

summary({
  'Wayland': wayland.version(),
  'wlroots': wlroots.version(),
  'Pixman': pixman.version(),
  'xkbcommon': xkbcommon.version(),
}, section: 'Dependencies')
