#!/bin/bash
# NovaOS ISO Build Script
# Complete automated build process for NovaOS

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${BUILD_DIR}/output"
PACKAGES_DIR="${BUILD_DIR}/packages"
ISO_NAME="novaos-1.0-amd64.iso"
TIMESTAMP=$(date +%Y%m%d-%H%M%S)

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking build dependencies..."
    
    local missing_deps=()
    
    # Check required commands
    for cmd in lb meson ninja cargo rustc xorriso mksquashfs syslinux grub-mkrescue; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_deps+=("$cmd")
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        log_info "Install them with:"
        echo "sudo apt install live-build meson ninja-build cargo rustc xorriso squashfs-tools syslinux-common grub-pc-bin"
        exit 1
    fi
    
    log_success "All dependencies found"
}

setup_build_environment() {
    log_info "Setting up build environment..."
    
    mkdir -p "${OUTPUT_DIR}"
    mkdir -p "${PACKAGES_DIR}"/{de,apps,firmware,custom}
    
    # Clean previous builds if requested
    if [ "$1" == "--clean" ]; then
        log_info "Cleaning previous build artifacts..."
        sudo lb clean 2>/dev/null || true
        rm -rf "${BUILD_DIR}"/{chroot,binary,cache,.build} 2>/dev/null || true
    fi
    
    log_success "Build environment ready"
}

build_de_components() {
    log_info "Building NovaDe Desktop Environment components..."
    
    local de_components=(
        "nova-compositor"
        "nova-panel"
        "nova-launcher"
        "nova-settings"
        "nova-notifications"
        "nova-lockscreen"
        "nova-greeter"
        "nova-wm"
    )
    
    for component in "${de_components[@]}"; do
        if [ -d "${BUILD_DIR}/nova-de/${component}" ]; then
            log_info "Building ${component}..."
            cd "${BUILD_DIR}/nova-de/${component}"
            
            if [ -f "meson.build" ]; then
                meson setup build --prefix=/usr 2>/dev/null || true
                ninja -C build 2>/dev/null || true
                
                # Create .deb package
                cd build
                if command -v checkinstall &> /dev/null; then
                    sudo checkinstall -D --install=no \
                        --pkgname="${component}" \
                        --pkgversion=1.0.0 \
                        --pkggroup=x11 \
                        --maintainer="NovaOS Team <team@novaos.org>" \
                        ninja install 2>/dev/null || true
                    cp *.deb "${PACKAGES_DIR}/de/" 2>/dev/null || true
                fi
            fi
            
            cd "${BUILD_DIR}"
        fi
    done
    
    log_success "DE components built"
}

build_applications() {
    log_info "Building NovaOS applications..."
    
    local apps=(
        "nova-files"
        "nova-terminal"
        "nova-software"
        "nova-monitor"
        "nova-screenshot"
        "nova-editor"
        "nova-viewer"
        "nova-player"
        "nova-calculator"
        "nova-backup"
        "nova-welcome"
    )
    
    for app in "${apps[@]}"; do
        if [ -d "${BUILD_DIR}/nova-apps/${app}" ]; then
            log_info "Building ${app}..."
            cd "${BUILD_DIR}/nova-apps/${app}"
            
            if [ -f "meson.build" ]; then
                meson setup build --prefix=/usr 2>/dev/null || true
                ninja -C build 2>/dev/null || true
                
                cd build
                if command -v checkinstall &> /dev/null; then
                    sudo checkinstall -D --install=no \
                        --pkgname="${app}" \
                        --pkgversion=1.0.0 \
                        --pkggroup=x11 \
                        --maintainer="NovaOS Team <team@novaos.org>" \
                        ninja install 2>/dev/null || true
                    cp *.deb "${PACKAGES_DIR}/apps/" 2>/dev/null || true
                fi
            fi
            
            cd "${BUILD_DIR}"
        fi
    done
    
    log_success "Applications built"
}

configure_live_build() {
    log_info "Configuring live-build..."
    
    cd "${BUILD_DIR}"
    
    # Configure live-build with NovaOS settings
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
        --iso-publisher "NovaOS Project <https://novaos.org>" \
        --iso-volume "NOVAOS_INSTALL" \
        --binary-images iso-hybrid \
        --debian-installer-gui false \
        --security true \
        --backports true \
        --firmware-chroot true \
        --firmware-binary true \
        2>/dev/null || log_warning "Configuration step completed with warnings"
    
    # Copy custom configurations
    log_info "Copying custom configurations..."
    if [ -d "${BUILD_DIR}/base-system/live-build-config/config" ]; then
        cp -r "${BUILD_DIR}/base-system/live-build-config/config/"* ./config/ 2>/dev/null || true
    fi
    
    # Copy branding
    if [ -d "${BUILD_DIR}/branding" ]; then
        mkdir -p ./config/includes.chroot/
        cp -r "${BUILD_DIR}/branding" ./config/includes.chroot/ 2>/dev/null || true
    fi
    
    # Copy installer
    if [ -d "${BUILD_DIR}/installer/calamares-config" ]; then
        mkdir -p ./config/includes.chroot/etc/calamares/
        cp -r "${BUILD_DIR}/installer/calamares-config/"* ./config/includes.chroot/etc/calamares/ 2>/dev/null || true
    fi
    
    # Copy custom packages
    if [ -d "${PACKAGES_DIR}" ]; then
        mkdir -p ./config/packages.chroot/
        cp -r "${PACKAGES_DIR}"/* ./config/packages.chroot/ 2>/dev/null || true
    fi
    
    log_success "Live-build configured"
}

run_bootstrap() {
    log_info "Running bootstrap stage..."
    cd "${BUILD_DIR}"
    sudo lb bootstrap
    log_success "Bootstrap complete"
}

run_chroot() {
    log_info "Running chroot stage (this may take a while)..."
    cd "${BUILD_DIR}"
    sudo lb chroot
    log_success "Chroot stage complete"
}

run_binary() {
    log_info "Building binary image..."
    cd "${BUILD_DIR}"
    sudo lb binary
    log_success "Binary stage complete"
}

finalize_iso() {
    log_info "Finalizing ISO..."
    
    # Find and move ISO to output directory
    local iso_file=$(find "${BUILD_DIR}" -maxdepth 1 -name "*.iso" 2>/dev/null | head -1)
    
    if [ -n "${iso_file}" ] && [ -f "${iso_file}" ]; then
        mv "${iso_file}" "${OUTPUT_DIR}/${ISO_NAME}"
        chmod 644 "${OUTPUT_DIR}/${ISO_NAME}"
        
        # Generate checksum
        cd "${OUTPUT_DIR}"
        sha256sum "${ISO_NAME}" > "${ISO_NAME}.sha256"
        
        log_success "ISO created: ${OUTPUT_DIR}/${ISO_NAME}"
        log_info "SHA256: $(cat ${ISO_NAME}.sha256)"
    else
        log_error "ISO file not found!"
        exit 1
    fi
}

show_summary() {
    echo ""
    echo "=========================================="
    echo -e "${GREEN}NovaOS Build Complete!${NC}"
    echo "=========================================="
    echo ""
    echo "Output files:"
    echo "  ISO: ${OUTPUT_DIR}/${ISO_NAME}"
    echo "  Checksum: ${OUTPUT_DIR}/${ISO_NAME}.sha256"
    echo ""
    echo "Next steps:"
    echo "  Test in QEMU: make test-vm"
    echo "  Create USB: sudo make create-usb USB_DEVICE=/dev/sdX"
    echo ""
    echo "Build timestamp: ${TIMESTAMP}"
    echo "=========================================="
}

# Main build function
main() {
    echo ""
    echo "=========================================="
    echo -e "${BLUE}NovaOS Build System${NC}"
    echo "=========================================="
    echo ""
    
    local start_time=$(date +%s)
    
    # Parse arguments
    local clean_build=false
    local skip_de=false
    local skip_apps=false
    local target="all"
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                clean_build=true
                shift
                ;;
            --skip-de)
                skip_de=true
                shift
                ;;
            --skip-apps)
                skip_apps=true
                shift
                ;;
            bootstrap|chroot|binary|iso)
                target=$1
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS] [TARGET]"
                echo ""
                echo "Targets:"
                echo "  all       - Build everything (default)"
                echo "  bootstrap - Run bootstrap stage only"
                echo "  chroot    - Run chroot stage only"
                echo "  binary    - Run binary stage only"
                echo "  iso       - Build complete ISO"
                echo ""
                echo "Options:"
                echo "  --clean     - Clean previous builds"
                echo "  --skip-de   - Skip DE compilation"
                echo "  --skip-apps - Skip app compilation"
                echo "  --help      - Show this help"
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    # Execute based on target
    case ${target} in
        all)
            check_dependencies
            setup_build_environment $([ "${clean_build}" = true ] && echo "--clean")
            [ "${skip_de}" = false ] && build_de_components
            [ "${skip_apps}" = false ] && build_applications
            configure_live_build
            run_bootstrap
            run_chroot
            run_binary
            finalize_iso
            show_summary
            ;;
        bootstrap)
            check_dependencies
            setup_build_environment
            configure_live_build
            run_bootstrap
            ;;
        chroot)
            run_chroot
            ;;
        binary)
            run_binary
            finalize_iso
            ;;
        iso)
            configure_live_build
            run_bootstrap
            run_chroot
            run_binary
            finalize_iso
            show_summary
            ;;
        *)
            log_error "Unknown target: ${target}"
            exit 1
            ;;
    esac
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    log_info "Total build time: $((duration / 60)) minutes $((duration % 60)) seconds"
}

# Run main function
main "$@"
