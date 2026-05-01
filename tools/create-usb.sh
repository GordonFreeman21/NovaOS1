#!/bin/bash
# NovaOS Bootable USB Creator
# Create a bootable USB drive from the NovaOS ISO

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(dirname "${SCRIPT_DIR}")"
OUTPUT_DIR="${BUILD_DIR}/output"
ISO_NAME="novaos-1.0-amd64.iso"
ISO_PATH="${OUTPUT_DIR}/${ISO_NAME}"

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

show_help() {
    echo "NovaOS Bootable USB Creator"
    echo ""
    echo "Usage: sudo $0 --device /dev/sdX [OPTIONS]"
    echo ""
    echo "Required:"
    echo "  --device DEV    USB device (e.g., /dev/sdb)"
    echo ""
    echo "Options:"
    echo "  --iso PATH      Path to ISO file (default: ${ISO_PATH})"
    echo "  --verify        Verify write after creation"
    echo "  --eject         Eject USB after completion"
    echo "  --force         Skip confirmation prompt"
    echo "  --help          Show this help"
    echo ""
    echo "Examples:"
    echo "  sudo $0 --device /dev/sdb"
    echo "  sudo $0 --device /dev/sdc --verify --eject"
    echo ""
    echo "WARNING: All data on the target device will be erased!"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

check_dependencies() {
    if ! command -v dd &> /dev/null; then
        log_error "dd not found. Please install coreutils."
        exit 1
    fi
    
    if ! command -v sync &> /dev/null; then
        log_error "sync not found. Please install coreutils."
        exit 1
    fi
}

check_iso() {
    if [ ! -f "${ISO_PATH}" ]; then
        log_error "ISO not found at ${ISO_PATH}"
        log_info "Build the ISO first with: make build-iso"
        exit 1
    fi
    log_info "Found ISO: ${ISO_PATH}"
    log_info "ISO size: $(du -h "${ISO_PATH}" | cut -f1)"
}

validate_device() {
    local device="$1"
    
    # Check if device exists
    if [ ! -b "${device}" ]; then
        log_error "Device ${device} does not exist or is not a block device"
        exit 1
    fi
    
    # Basic sanity check - should start with /dev/
    if [[ ! "${device}" =~ ^/dev/(sd[a-z]|nvme[0-9]+n[0-9]+)$ ]]; then
        log_warning "Device name '${device}' looks unusual. Make sure this is correct!"
        if [ "${FORCE}" != true ]; then
            read -p "Continue anyway? (yes/no): " confirm
            if [ "${confirm}" != "yes" ]; then
                log_info "Aborted."
                exit 0
            fi
        fi
    fi
    
    # Check if device is mounted
    if mount | grep -q "^${device}"; then
        log_warning "Device ${device} appears to be mounted. It will be unmounted."
        if [ "${FORCE}" != true ]; then
            read -p "Continue? (yes/no): " confirm
            if [ "${confirm}" != "yes" ]; then
                log_info "Aborted."
                exit 0
            fi
        fi
        
        # Unmount all partitions
        for part in ${device}*; do
            umount "${part}" 2>/dev/null || true
        done
        umount "${device}" 2>/dev/null || true
    fi
    
    # Get device info
    local device_size=$(blockdev --getsize64 "${device}" 2>/dev/null || echo "unknown")
    log_info "Device: ${device}"
    log_info "Device size: ${device_size} bytes"
}

confirm_destruction() {
    if [ "${FORCE}" = true ]; then
        return
    fi
    
    echo ""
    log_warning "WARNING: This will ERASE ALL DATA on ${DEVICE}!"
    echo ""
    echo "Target device information:"
    lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,LABEL 2>/dev/null | grep -E "^$(basename ${DEVICE})" || true
    echo ""
    read -p "Type 'YES' to confirm and create bootable USB: " confirm
    
    if [ "${confirm}" != "YES" ]; then
        log_info "Operation cancelled."
        exit 0
    fi
}

create_usb() {
    log_info "Creating bootable USB..."
    echo ""
    
    # Use dd with progress
    dd if="${ISO_PATH}" of="${DEVICE}" bs=4M status=progress oflag=sync conv=fsync
    
    # Sync to ensure all data is written
    log_info "Syncing data to disk..."
    sync
    
    # Flush buffers
    blockdev --flushbufs "${DEVICE}" 2>/dev/null || true
    
    log_success "Bootable USB created successfully!"
}

verify_write() {
    if [ "${VERIFY}" != true ]; then
        return
    fi
    
    log_info "Verifying write..."
    
    # Calculate checksums
    local iso_hash=$(sha256sum "${ISO_PATH}" | cut -d' ' -f1)
    local usb_hash=$(dd if="${DEVICE}" bs=4M count=$(( $(stat -c%s "${ISO_PATH}") / 4096 )) 2>/dev/null | sha256sum | cut -d' ' -f1)
    
    if [ "${iso_hash}" = "${usb_hash}" ]; then
        log_success "Verification passed! Checksums match."
    else
        log_error "Verification FAILED! Checksums do not match."
        log_info "ISO hash:  ${iso_hash}"
        log_info "USB hash:  ${usb_hash}"
        exit 1
    fi
}

eject_device() {
    if [ "${EJECT}" != true ]; then
        return
    fi
    
    log_info "Ejecting device..."
    
    # Try to eject
    if command -v eject &> /dev/null; then
        eject "${DEVICE}" 2>/dev/null || true
    else
        # Alternative: use blockdev
        blockdev --flushbufs "${DEVICE}" 2>/dev/null || true
    fi
    
    log_success "Device can now be safely removed."
}

show_post_instructions() {
    echo ""
    echo "=========================================="
    log_success "NovaOS Bootable USB Ready!"
    echo "=========================================="
    echo ""
    echo "Device: ${DEVICE}"
    echo "ISO: ${ISO_PATH}"
    echo ""
    echo "Next steps:"
    echo "1. Remove the USB drive"
    echo "2. Insert it into the target computer"
    echo "3. Boot from USB (may require changing boot order in BIOS/UEFI)"
    echo "4. Select 'Try NovaOS' or 'Install NovaOS' from the boot menu"
    echo ""
    echo "For UEFI systems, you may need to:"
    echo "- Disable Secure Boot, or"
    echo "- Enroll the NovaOS key in your firmware settings"
    echo ""
}

# Variables
DEVICE=""
VERIFY=false
EJECT=false
FORCE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --device|-d)
            DEVICE="$2"
            shift 2
            ;;
        --iso|-i)
            ISO_PATH="$2"
            shift 2
            ;;
        --verify|-v)
            VERIFY=true
            shift
            ;;
        --eject|-e)
            EJECT=true
            shift
            ;;
        --force|-f)
            FORCE=true
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Main execution
main() {
    echo ""
    echo "=========================================="
    echo -e "${BLUE}NovaOS Bootable USB Creator${NC}"
    echo "=========================================="
    echo ""
    
    # Check requirements
    check_root
    check_dependencies
    
    # Validate device parameter
    if [ -z "${DEVICE}" ]; then
        log_error "No device specified. Use --device /dev/sdX"
        show_help
        exit 1
    fi
    
    # Check ISO
    check_iso
    
    # Validate and prepare device
    validate_device "${DEVICE}"
    
    # Confirm destruction
    confirm_destruction
    
    echo ""
    
    # Create USB
    create_usb
    
    # Verify if requested
    verify_write
    
    # Eject if requested
    eject_device
    
    # Show instructions
    show_post_instructions
}

# Run main
main
