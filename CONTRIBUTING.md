# JimOS Contributing Guidelines

Thank you for your interest in contributing to JimOS! This document provides guidelines and instructions for contributing.

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [Development Setup](#development-setup)
4. [Coding Standards](#coding-standards)
5. [Pull Request Process](#pull-request-process)
6. [Issue Reporting](#issue-reporting)
7. [Testing](#testing)
8. [Documentation](#documentation)

---

## Code of Conduct

### Our Pledge

We pledge to make participation in JimOS a harassment-free experience for everyone, regardless of age, body size, disability, ethnicity, sex characteristics, gender identity and expression, level of experience, education, socio-economic status, nationality, personal appearance, race, religion, or sexual identity and orientation.

### Our Standards

Examples of behavior that contributes to creating a positive environment:

- Using welcoming and inclusive language
- Being respectful of differing viewpoints and experiences
- Gracefully accepting constructive criticism
- Focusing on what is best for the community
- Showing empathy towards other community members

---

## Getting Started

### Prerequisites

- Git
- Rust toolchain (for DE components)
- Meson and Ninja build systems
- GTK4 development libraries
- Debian/Ubuntu-based system for building

### Fork and Clone

```bash
# Fork the repository on GitHub, then clone:
git clone https://github.com/YOUR_USERNAME/jimos.git
cd jimos

# Add upstream remote
git remote add upstream https://github.com/jimos/jimos.git
```

---

## Development Setup

### Install Dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    git \
    meson \
    ninja-build \
    cargo \
    rustc \
    libgtk-4-dev \
    libadwaita-1-dev \
    libwlroots-dev \
    wayland-protocols \
    libwayland-dev \
    cmake \
    pkg-config
```

### Build System Overview

```
JimOS/
├── base-system/          # Debian live-build configuration
├── jim-de/              # Desktop Environment components
│   ├── jim-compositor/  # Wayland compositor (Rust)
│   ├── jim-panel/       # Taskbar (GTK4/Rust)
│   └── ...
├── jim-apps/            # System applications
├── branding/             # Themes, icons, wallpapers
└── tools/                # Build and utility scripts
```

---

## Coding Standards

### Rust Code Style

We follow the official Rust style guide with rustfmt:

```rust
// Use descriptive variable names
let window_manager = WindowManager::new();

// Document public APIs
/// Creates a new compositor instance
/// 
/// # Arguments
/// * `config` - Configuration options
/// 
/// # Returns
/// Result containing Compositor or error
pub fn new(config: Config) -> Result<Self, Error> {
    // Implementation
}

// Handle errors properly
match result {
    Ok(value) => value,
    Err(e) => return Err(Error::from(e)),
}
```

Run rustfmt before committing:
```bash
cargo fmt
```

### C/C++ Code Style

Follow the Linux kernel coding style:

```c
/* Function documentation */
int function_name(int param1, char *param2)
{
    int local_var;
    
    if (condition) {
        /* Comment for complex logic */
        do_something();
    }
    
    return 0;
}
```

### GTK4/Rust Guidelines

```rust
use gtk::prelude::*;
use adw::prelude::*;

// Use LibAdwaita for modern UI
fn create_settings_page() -> adw::PreferencesPage {
    let page = adw::PreferencesPage::builder()
        .title("Settings")
        .icon_name("preferences-system-symbolic")
        .build();
    
    page
}
```

### Commit Messages

Follow Conventional Commits specification:

```
feat: add snap layouts to window manager
fix: resolve memory leak in compositor
docs: update build instructions
style: format code with rustfmt
refactor: simplify notification handling
test: add unit tests for settings module
chore: update dependencies
```

---

## Pull Request Process

### Before Submitting

1. **Fork** the repository
2. **Create a branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. **Make changes** following coding standards
4. **Test thoroughly**:
   ```bash
   cargo test
   meson test -C build
   ```
5. **Update documentation** if needed
6. **Run linters**:
   ```bash
   cargo clippy
   ```

### Creating the PR

1. Push your branch:
   ```bash
   git push origin feature/your-feature-name
   ```
2. Open a Pull Request on GitHub
3. Fill out the PR template completely
4. Link any related issues

### PR Review

- Be responsive to feedback
- Make requested changes promptly
- Keep discussions professional and constructive

---

## Issue Reporting

### Bug Reports

Include the following information:

1. **System Information**:
   - JimOS version
   - Hardware specifications
   - Kernel version
   
2. **Steps to Reproduce**:
   - Clear, numbered steps
   - Expected behavior
   - Actual behavior
   
3. **Logs and Screenshots**:
   ```bash
   journalctl -xe
   dmesg | tail -50
   ```

### Feature Requests

Describe:
- What problem the feature solves
- How it should work
- Any relevant use cases

---

## Testing

### Unit Tests

```bash
# Run all tests
cargo test

# Run specific test
cargo test test_compositor_init

# Run with coverage
cargo tarpaulin --out Html
```

### Integration Tests

```bash
# Build and test in VM
./tools/test-vm.sh

# Test specific component
meson test -C build jim-panel
```

### Manual Testing Checklist

- [ ] Boot process works
- [ ] Desktop loads correctly
- [ ] All pre-installed apps launch
- [ ] Network connectivity works
- [ ] Audio playback functions
- [ ] Display scaling works
- [ ] Multi-monitor setup supported
- [ ] Power management (suspend/resume)
- [ ] Installation process completes

---

## Documentation

### Code Documentation

All public APIs must be documented:

```rust
/// Calculate the optimal window layout
/// 
/// This function implements the snap layout algorithm
/// used when dragging windows to screen edges.
/// 
/// # Arguments
/// * `windows` - List of visible windows
/// * `zone` - The snap zone identifier
/// 
/// # Returns
/// A Layout describing window positions
/// 
/// # Example
/// ```
/// let layout = calculate_snap_layout(&windows, SnapZone::Left);
/// ```
pub fn calculate_snap_layout(
    windows: &[Window],
    zone: SnapZone
) -> Layout {
    // Implementation
}
```

### User Documentation

Write clear, concise documentation for users:

- Include screenshots where helpful
- Provide step-by-step instructions
- Explain both how and why

---

## Areas Needing Contribution

### High Priority

1. **Wayland Compositor** - Complete XDG shell implementation
2. **Panel/Taskbar** - Live thumbnail previews
3. **Settings App** - Additional configuration modules
4. **File Manager** - Cloud storage integration
5. **Installer** - Improved hardware detection

### Good First Issues

Look for issues labeled:
- `good first issue`
- `help wanted`
- `beginner-friendly`

---

## Communication

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General discussion and questions
- **Discord**: Real-time chat (link in README)
- **Matrix**: #jimos:matrix.org

---

## License

By contributing to JimOS, you agree that your contributions will be licensed under the GPL-3.0-or-later license.

---

## Questions?

Don't hesitate to ask! We're happy to help new contributors get started.

- Check existing documentation
- Search closed issues for similar questions
- Ask in GitHub Discussions or Discord

Thank you for contributing to JimOS! 🚀
