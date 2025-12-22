# Tools
ASM := nasm
QEMU := qemu-system-x86_64
CC := x86_64-elf-gcc        # For 64-bit kernel
LD := x86_64-elf-ld
OBJCOPY := x86_64-elf-objcopy

# Directories
BUILD_DIR := build
IMAGE_DIR := images
BOOT_DIR := boot
KERNEL_DIR := kernel
KERNEL_SRC_DIR := $(KERNEL_DIR)/src

# Output files
MBR_BIN := $(BUILD_DIR)/mbr.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
DISK_IMG := $(IMAGE_DIR)/boot.img

# Bootloader sources
MBR_SRC := $(BOOT_DIR)/mbr.asm
STAGE2_SRC := $(BOOT_DIR)/stage2.asm
STAGE2_DEPS := $(BOOT_DIR)/print16_string.asm \
               $(BOOT_DIR)/print32_string.asm \
               $(BOOT_DIR)/gdt.asm \
               $(BOOT_DIR)/switch_to_pm.asm 

# Kernel sources
KERNEL_C_SRC := $(shell find $(KERNEL_SRC_DIR) -name "*.c")
KERNEL_C_OBJ := $(patsubst $(KERNEL_SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRC))
KERNEL_ELF := $(BUILD_DIR)/kernel.elf

# Compiler flags
CFLAGS := -m64 \
          -ffreestanding \
          -fno-stack-protector \
          -fno-builtin \
          -O2 \
          -Wall \
          -Wextra \
          -I$(KERNEL_SRC_DIR) \
		  -Ikernel/include \
		  -mcmodel=large \
		  -mno-red-zone \
		  -mno-mmx \
		  -mno-sse \
		  -mno-sse2

LDFLAGS := -T linker.ld \
           -nostdlib \
           -static

# Targets
all: $(DISK_IMG)

# Create build directories
$(BUILD_DIR) $(IMAGE_DIR):
	@mkdir -p $@

# Assemble MBR
$(MBR_BIN): $(MBR_SRC) | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

# Assemble Stage2 (16/32/64-bit loader)
$(STAGE2_BIN): $(STAGE2_SRC) $(STAGE2_DEPS) | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@ -I $(BOOT_DIR)/

# Compile kernel C code
$(BUILD_DIR)/%.o: $(KERNEL_SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link kernel
$(KERNEL_ELF): $(KERNEL_C_OBJ) | $(BUILD_DIR)
	$(LD) -m elf_x86_64 $(LDFLAGS) -o $@ $(KERNEL_C_OBJ)

# Convert kernel to flat binary
$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD_DIR)
	$(OBJCOPY) -O binary --strip-all $< $@

# Create disk image
$(DISK_IMG): $(MBR_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(IMAGE_DIR)
	@cat $(MBR_BIN) $(STAGE2_BIN) $(KERNEL_BIN) > $@

# Run in QEMU with CPU feature display
run: $(DISK_IMG)
	$(QEMU) -m 512M -drive format=raw,file=$(DISK_IMG),if=ide -serial stdio -no-reboot -cpu max

# Debug version with more verbose output
debug: $(DISK_IMG)
	$(QEMU) -m 512M -drive format=raw,file=$(DISK_IMG),if=ide -serial stdio -no-reboot -cpu max -d int,cpu_reset

clean:
	@rm -rf $(BUILD_DIR) $(IMAGE_DIR)

.PHONY: all clean run debug