ASM := nasm
CC := x86_64-elf-gcc
LD := x86_64-elf-ld
OBJCOPY := x86_64-elf-objcopy
QEMU := qemu-system-x86_64

BUILD_DIR := build
IMAGE_DIR := images
BOOT_DIR := boot
KERNEL_DIR := kernel
KERNEL_SRC_DIR := $(KERNEL_DIR)/src
KERNEL_INC_DIR := $(KERNEL_DIR)/include

MBR_SRC := $(BOOT_DIR)/mbr.asm
STAGE2_SRC := $(BOOT_DIR)/stage2.asm
KERNEL_ENTRY_SRC := $(KERNEL_DIR)/src/kernel_entry.asm

MBR_BIN := $(BUILD_DIR)/mbr.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
DISK_IMG := $(IMAGE_DIR)/boot.img
SATA_IMG := $(IMAGE_DIR)/sata.img

KERNEL_C_SRC := \
	$(KERNEL_DIR)/src/kernel.c \
	$(shell find $(KERNEL_SRC_DIR) -name "*.c")

KERNEL_C_OBJ := $(KERNEL_C_SRC:%.c=$(BUILD_DIR)/%.o)
KERNEL_ENTRY_OBJ := $(BUILD_DIR)/kernel/kernel_entry.o

CFLAGS := -m64 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-builtin \
	-O2 \
	-Wall -Wextra \
	-mcmodel=large \
	-mno-red-zone \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-I$(KERNEL_INC_DIR)

LDFLAGS := -T linker.ld -nostdlib -static

SATA1_IMG := $(IMAGE_DIR)/sata1.img
SATA2_IMG := $(IMAGE_DIR)/sata2.img
CDROM_ISO := $(IMAGE_DIR)/cdrom.iso

all: $(DISK_IMG) $(SATA_IMG)

$(BUILD_DIR) $(IMAGE_DIR):
	mkdir -p $@

$(MBR_BIN): $(MBR_SRC) | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

$(STAGE2_BIN): $(STAGE2_SRC) | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@ -I $(BOOT_DIR)/

$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC) | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(ASM) -f elf64 $< -o $@

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(KERNEL_C_OBJ)
	$(LD) -m elf_x86_64 $(LDFLAGS) -o $@ $^

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary --strip-all $< $@

$(DISK_IMG): $(MBR_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(IMAGE_DIR)
	dd if=/dev/zero of=$@ bs=512 count=20480 2>/dev/null
	dd if=$(MBR_BIN) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=33 conv=notrunc 2>/dev/null

$(SATA_IMG): | $(IMAGE_DIR)
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null

$(SATA1_IMG): | $(IMAGE_DIR)
	dd if=/dev/zero of=$@ bs=1M count=128 2>/dev/null

$(SATA2_IMG): | $(IMAGE_DIR)
	dd if=/dev/zero of=$@ bs=1M count=256 2>/dev/null

$(CDROM_ISO): | $(IMAGE_DIR)
	mkdir -p /tmp/cdrom_content
	mkisofs -o $@ -V "TEST_CD" -J -R /tmp/cdrom_content 2>/dev/null || \
		genisoimage -o $@ -V "TEST_CD" -J -R /tmp/cdrom_content 2>/dev/null || \
		dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
	rm -rf /tmp/cdrom_content

run: $(DISK_IMG) $(SATA_IMG)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0 \
		-serial stdio -no-reboot -cpu max

run-wd: $(DISK_IMG) $(SATA_IMG)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0,model="WDC WD5000AAKX",serial=WD-WCAYUN123456 \
		-serial stdio -no-reboot -cpu max

run-seagate: $(DISK_IMG) $(SATA_IMG)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0,model="ST1000DM003",serial=S1D0ABCD \
		-serial stdio -no-reboot -cpu max

run-samsung: $(DISK_IMG) $(SATA_IMG)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0,model="Samsung SSD 850",serial=S21NX0AG123456 \
		-serial stdio -no-reboot -cpu max

run-ssd: $(DISK_IMG) $(SATA_IMG)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none,discard=unmap \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0,model="KINGSTON SV300",serial=50026B7261234567,rotation_rate=1 \
		-serial stdio -no-reboot -cpu max

run-hdd: $(DISK_IMG) $(SATA_IMG)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0,model="WDC WD10EZEX",serial=WD-WCC6Y1234567,rotation_rate=7200 \
		-serial stdio -no-reboot -cpu max

run-multi: $(DISK_IMG) $(SATA_IMG) $(SATA1_IMG) $(CDROM_ISO)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none \
		-drive id=sata1,format=raw,file=$(SATA1_IMG),if=none \
		-drive id=cdrom0,format=raw,file=$(CDROM_ISO),if=none,media=cdrom \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0,model="WD Blue 1TB",serial=WD001 \
		-device ide-hd,drive=sata1,bus=ahci.1,model="Seagate 2TB",serial=ST002 \
		-device ide-cd,drive=cdrom0,bus=ahci.2 \
		-serial stdio -no-reboot -cpu max

debug: $(DISK_IMG) $(SATA_IMG)
	$(QEMU) -m 512M \
		-drive id=boot,format=raw,file=$(DISK_IMG),if=ide \
		-drive id=sata0,format=raw,file=$(SATA_IMG),if=none \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=sata0,bus=ahci.0 \
		-serial stdio -no-reboot -cpu max \
		-d int,cpu_reset

clean:
	rm -rf $(BUILD_DIR) $(IMAGE_DIR)

.PHONY: all clean run debug run-wd run-seagate run-samsung run-ssd run-hdd run-multi