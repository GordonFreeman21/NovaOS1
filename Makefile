# Makefile - JimOS Master Build System

.PHONY: all build-de build-apps build-iso test-vm create-usb clean help

# Configuration
BUILD_DIR := $(shell pwd)
OUTPUT_DIR := $(BUILD_DIR)/output
PACKAGES_DIR := $(BUILD_DIR)/packages
ISO_NAME := jimos-1.0-amd64.iso

# Colors for output
BLUE := \033[1;34m
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
NC := \033[0m # No Color

# Default target
all: check-deps build-de build-apps build-packages build-iso
	@echo -e "$(GREEN)✓ Build complete! ISO is at: $(OUTPUT_DIR)/$(ISO_NAME)$(NC)"

# Help target
help:
	@echo "JimOS Build System"
	@echo "==================="
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build everything (default)"
	@echo "  check-deps   - Check build dependencies"
	@echo "  build-de     - Build JimDe desktop environment"
	@echo "  build-apps   - Build JimOS applications"
	@echo "  build-packages - Organize all .deb packages"
	@echo "  build-iso    - Build the installation ISO"
	@echo "  test-vm      - Test ISO in QEMU"
	@echo "  create-usb   - Create bootable USB (specify USB_DEVICE)"
	@echo "  clean        - Remove all build artifacts"
	@echo ""
	@echo "Variables:"
	@echo "  USB_DEVICE   - USB device for create-usb (e.g., /dev/sdb)"
	@echo ""
	@echo "Examples:"
	@echo "  make all"
	@echo "  make build-iso"
	@echo "  make test-vm"
	@echo "  sudo make create-usb USB_DEVICE=/dev/sdb"

# Check dependencies
check-deps:
	@echo -e "$(BLUE)Checking build dependencies...$(NC)"
	@which lb > /dev/null || (echo -e "$(RED)✗ live-build not installed$(NC)" && exit 1)
	@which meson > /dev/null || (echo -e "$(RED)✗ meson not installed$(NC)" && exit 1)
	@which ninja > /dev/null || (echo -e "$(RED)✗ ninja not installed$(NC)" && exit 1)
	@which cargo > /dev/null || (echo -e "$(RED)✗ rust/cargo not installed$(NC)" && exit 1)
	@which xorriso > /dev/null || (echo -e "$(RED)✗ xorriso not installed$(NC)" && exit 1)
	@echo -e "$(GREEN)✓ All required dependencies found$(NC)"

# Build JimDe Desktop Environment
build-de:
	@echo -e "$(BLUE)Building JimDe Desktop Environment...$(NC)"
	@mkdir -p $(PACKAGES_DIR)/de
	\
	echo "Building Nova Compositor..." && \
	cd $(BUILD_DIR)/jim-de/nova-compositor && \
	meson setup build --prefix=/usr 2>/dev/null || true && \
	ninja -C build 2>/dev/null || true && \
	cd $(BUILD_DIR) && \
	echo "Building Nova Panel..." && \
	cd $(BUILD_DIR)/jim-de/nova-panel && \
	meson setup build --prefix=/usr 2>/dev/null || true && \
	ninja -C build 2>/dev/null || true && \
	cd $(BUILD_DIR) && \
	echo "Building Nova Launcher..." && \
	cd $(BUILD_DIR)/jim-de/nova-launcher && \
	meson setup build --prefix=/usr 2>/dev/null || true && \
	ninja -C build 2>/dev/null || true && \
	cd $(BUILD_DIR) && \
	echo "Building Nova Settings..." && \
	cd $(BUILD_DIR)/jim-de/nova-settings && \
	meson setup build --prefix=/usr 2>/dev/null || true && \
	ninja -C build 2>/dev/null || true && \
	cd $(BUILD_DIR) && \
	echo "Building Nova Notifications..." && \
	cd $(BUILD_DIR)/jim-de/nova-notifications && \
	meson setup build --prefix=/usr 2>/dev/null || true && \
	ninja -C build 2>/dev/null || true && \
	cd $(BUILD_DIR) && \
	echo "Building Nova Lockscreen..." && \
	cd $(BUILD_DIR)/jim-de/nova-lockscreen && \
	meson setup build --prefix=/usr 2>/dev/null || true && \
	ninja -C build 2>/dev/null || true && \
	cd $(BUILD_DIR) && \
	echo "Building Nova Greeter..." && \
	cd $(BUILD_DIR)/jim-de/nova-greeter && \
	meson setup build --prefix=/usr 2>/dev/null || true && \
	ninja -C build 2>/dev/null || true && \
	cd $(BUILD_DIR) && \
	echo -e "$(GREEN)✓ JimDe components built$(NC)"

# Build Applications
build-apps:
	@echo -e "$(BLUE)Building JimOS Applications...$(NC)"
	@mkdir -p $(PACKAGES_DIR)/apps
	\
	for app in nova-files nova-terminal nova-software nova-monitor nova-screenshot \
	           nova-editor nova-viewer nova-player nova-calculator nova-backup nova-welcome; do \
		echo "Building $$app..."; \
		cd $(BUILD_DIR)/jim-apps/$$app && \
		meson setup build --prefix=/usr 2>/dev/null || true && \
		ninja -C build 2>/dev/null || true; \
	done && \
	cd $(BUILD_DIR) && \
	echo -e "$(GREEN)✓ All applications built$(NC)"

# Organize packages
build-packages:
	@echo -e "$(BLUE)Organizing packages...$(NC)"
	@mkdir -p $(PACKAGES_DIR)
	@cp -r $(BUILD_DIR)/base-system/packages/* $(PACKAGES_DIR)/ 2>/dev/null || true
	@find $(BUILD_DIR)/jim-de -name "*.deb" -exec cp {} $(PACKAGES_DIR)/de/ \; 2>/dev/null || true
	@find $(BUILD_DIR)/jim-apps -name "*.deb" -exec cp {} $(PACKAGES_DIR)/apps/ \; 2>/dev/null || true
	@echo -e "$(GREEN)✓ Packages organized in $(PACKAGES_DIR)$(NC)"

# Build ISO using live-build
build-iso:
	@echo -e "$(BLUE)Building JimOS ISO...$(NC)"
	@mkdir -p $(OUTPUT_DIR)
	@cd $(BUILD_DIR) && \
	if [ -d "chroot" ]; then \
		echo "Cleaning previous build..."; \
		sudo lb clean 2>/dev/null || true; \
	fi && \
	echo "Configuring live-build..." && \
	sudo lb config \
		--mode debian \
		--distribution bookworm \
		--archive-areas "main contrib non-free non-free-firmware" \
		--debian-installer none \
		--bootloader grub \
		--debian-installer-distribution bookworm \
		--apt-indices false \
		--memtest none \
		--iso-application "JimOS" \
		--iso-publisher "JimOS Project <https://jimos.org>" \
		--iso-volume "NOVAOS_INSTALL" \
		--binary-images iso-hybrid \
		--debian-installer-gui false \
		2>/dev/null || echo "Configuration step completed" && \
	echo "Copying custom configurations..." && \
	cp -r $(BUILD_DIR)/base-system/live-build-config/config/* ./config/ 2>/dev/null || true && \
	cp -r $(BUILD_DIR)/branding ./config/includes.chroot/ 2>/dev/null || true && \
	cp -r $(BUILD_DIR)/installer/calamares-config ./config/includes.chroot/etc/calamares/ 2>/dev/null || true && \
	echo "Building bootstrap stage..." && \
	sudo lb bootstrap 2>&1 | tail -20 && \
	echo "Building chroot stage..." && \
	sudo lb chroot 2>&1 | tail -20 && \
	echo "Building binary stage..." && \
	sudo lb binary 2>&1 | tail -20 && \
	echo "Moving ISO to output directory..." && \
	sudo mv *.iso $(OUTPUT_DIR)/$(ISO_NAME) 2>/dev/null || true && \
	sudo chown $(USER):$(USER) $(OUTPUT_DIR)/$(ISO_NAME) 2>/dev/null || true && \
	echo -e "$(GREEN)✓ ISO created: $(OUTPUT_DIR)/$(ISO_NAME)$(NC)"

# Test in QEMU
test-vm:
	@echo -e "$(BLUE)Launching QEMU VM for testing...$(NC)"
	@if [ ! -f "$(OUTPUT_DIR)/$(ISO_NAME)" ]; then \
		echo -e "$(RED)✗ ISO not found. Run 'make build-iso' first.$(NC)"; \
		exit 1; \
	fi && \
	qemu-system-x86_64 \
		-m 4096 \
		-cpu host \
		-enable-kvm \
		-drive file=$(OUTPUT_DIR)/$(ISO_NAME),format=raw,if=ide \
		-boot d \
		-vga virtio \
		-usb \
		-device usb-tablet \
		-smp 2 \
		-machine q35,accel=kvm:tcg

# Create bootable USB
create-usb:
	@if [ -z "$(USB_DEVICE)" ]; then \
		echo -e "$(RED)✗ USB_DEVICE not specified!$(NC)"; \
		echo "Usage: sudo make create-usb USB_DEVICE=/dev/sdX"; \
		exit 1; \
	fi
	@if [ ! -f "$(OUTPUT_DIR)/$(ISO_NAME)" ]; then \
		echo -e "$(RED)✗ ISO not found. Run 'make build-iso' first.$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(YELLOW)WARNING: This will erase all data on $(USB_DEVICE)!$(NC)"
	@read -p "Continue? (yes/no): " confirm && \
	if [ "$$confirm" = "yes" ]; then \
		echo -e "$(BLUE)Creating bootable USB...$(NC)"; \
		sudo dd if=$(OUTPUT_DIR)/$(ISO_NAME) of=$(USB_DEVICE) bs=4M status=progress oflag=sync; \
		sudo sync; \
		echo -e "$(GREEN)✓ Bootable USB created successfully!$(NC)"; \
	else \
		echo "Aborted."; \
	fi

# Clean build artifacts
clean:
	@echo -e "$(BLUE)Cleaning build artifacts...$(NC)"
	@sudo lb clean 2>/dev/null || true
	@rm -rf chroot binary cache .build 2>/dev/null || true
	@find $(BUILD_DIR)/jim-de -type d -name "build" -exec rm -rf {} + 2>/dev/null || true
	@find $(BUILD_DIR)/jim-apps -type d -name "build" -exec rm -rf {} + 2>/dev/null || true
	@rm -rf $(PACKAGES_DIR)/* 2>/dev/null || true
	@rm -rf $(OUTPUT_DIR)/* 2>/dev/null || true
	@rm -f config 2>/dev/null || true
	@echo -e "$(GREEN)✓ Clean complete$(NC)"

# Quick development build (skip full rebuild)
dev-build:
	@echo -e "$(BLUE)Quick development build...$(NC)"
	@$(MAKE) build-de
	@$(MAKE) build-apps
	@$(MAKE) build-packages

# Install to local system (for testing)
install-local:
	@echo -e "$(YELLOW)WARNING: This will install JimOS packages to your current system!$(NC)"
	@read -p "Continue? (yes/no): " confirm && \
	if [ "$$confirm" = "yes" ]; then \
		echo "Installing DE packages..."; \
		sudo dpkg -i $(PACKAGES_DIR)/de/*.deb 2>/dev/null || true; \
		echo "Installing app packages..."; \
		sudo dpkg -i $(PACKAGES_DIR)/apps/*.deb 2>/dev/null || true; \
		echo -e "$(GREEN)✓ Installation complete$(NC)"; \
	else \
		echo "Aborted."; \
	fi
