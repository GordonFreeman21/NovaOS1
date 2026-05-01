#!/bin/bash
# NovaOS QEMU Test Script
# Launch the built ISO in a virtual machine for testing

set -e

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(dirname "${SCRIPT_DIR}")"
OUTPUT_DIR="${BUILD_DIR}/output"
ISO_NAME="novaos-1.0-amd64.iso"
ISO_PATH="${OUTPUT_DIR}/${ISO_NAME}"

# Default VM settings
RAM_SIZE=${RAM_SIZE:-4096}
CPU_CORES=${CPU_CORES:-2}
DISK_SIZE=${DISK_SIZE:-50G}
DISK_FILE=${DISK_FILE:-/tmp/novaos-test.qcow2}
NETWORK=${NETWORK:-user}
UEFI=${UEFI:-false}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_qemu() {
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        log_error "QEMU not found. Install with: sudo apt install qemu-system-x86 qemu-kvm"
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
}

create_disk() {
    if [ ! -f "${DISK_FILE}" ]; then
        log_info "Creating test disk: ${DISK_FILE} (${DISK_SIZE})"
        qemu-img create -f qcow2 "${DISK_FILE}" "${DISK_SIZE}"
    else
        log_info "Using existing disk: ${DISK_FILE}"
    fi
}

get_network_args() {
    case "${NETWORK}" in
        user)
            echo "-netdev user,id=net0 -device virtio-net-pci,netdev=net0"
            ;;
        bridge)
            echo "-netdev bridge,id=net0 -device virtio-net-pci,netdev=net0"
            ;;
        tap)
            echo "-netdev tap,id=net0 -device virtio-net-pci,netdev=net0"
            ;;
        none)
            echo ""
            ;;
        *)
            echo "-netdev user,id=net0 -device virtio-net-pci,netdev=net0"
            ;;
    esac
}

get_uefi_args() {
    if [ "${UEFI}" = true ]; then
        # Check for OVMF
        if [ -f "/usr/share/OVMF/OVMF_CODE.fd" ]; then
            echo "-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
                  -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS.fd"
        else
            log_info "OVMF not found, falling back to BIOS boot"
            echo ""
        fi
    else
        echo ""
    fi
}

run_qemu() {
    log_info "Starting QEMU VM..."
    log_info "RAM: ${RAM_SIZE}MB, CPUs: ${CPU_CORES}, Network: ${NETWORK}"
    
    local network_args=$(get_network_args)
    local uefi_args=$(get_uefi_args)
    
    qemu-system-x86_64 \
        -enable-kvm \
        -m ${RAM_SIZE} \
        -smp ${CPU_CORES} \
        -cpu host \
        -machine q35,accel=kvm:tcg \
        ${uefi_args} \
        -drive file=${ISO_PATH},format=raw,if=ide,media=cdrom \
        -drive file=${DISK_FILE},format=qcow2,if=virtio \
        -boot d \
        -vga virtio \
        -display gtk,gl=on \
        -usb \
        -device usb-tablet \
        -device usb-kbd \
        -soundhw hda \
        ${network_args} \
        -serial stdio \
        -no-reboot \
        -rtc base=localtime \
        "$@"
}

show_help() {
    echo "NovaOS QEMU Test Script"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --ram SIZE        RAM size in MB (default: 4096)"
    echo "  --cpus N          Number of CPU cores (default: 2)"
    echo "  --disk FILE       Disk image file (default: /tmp/novaos-test.qcow2)"
    echo "  --disk-size SIZE  Disk size (default: 50G)"
    echo "  --network TYPE    Network type: user, bridge, tap, none (default: user)"
    echo "  --uefi            Use UEFI boot instead of BIOS"
    echo "  --iso PATH        Path to ISO file"
    echo "  --help            Show this help"
    echo ""
    echo "Environment variables:"
    echo "  RAM_SIZE, CPU_CORES, DISK_SIZE, DISK_FILE, NETWORK, UEFI"
    echo ""
    echo "Examples:"
    echo "  $0 --ram 8192 --cpus 4"
    echo "  $0 --uefi --network bridge"
    echo "  RAM_SIZE=8192 $0"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --ram)
            RAM_SIZE="$2"
            shift 2
            ;;
        --cpus)
            CPU_CORES="$2"
            shift 2
            ;;
        --disk)
            DISK_FILE="$2"
            shift 2
            ;;
        --disk-size)
            DISK_SIZE="$2"
            shift 2
            ;;
        --network)
            NETWORK="$2"
            shift 2
            ;;
        --uefi)
            UEFI=true
            shift
            ;;
        --iso)
            ISO_PATH="$2"
            shift 2
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

# Main
log_info "NovaOS QEMU Test"
echo "==================="
check_qemu
check_iso
create_disk
echo ""
log_info "Launching VM..."
echo "Press Ctrl+A then X to exit QEMU"
echo ""
run_qemu
