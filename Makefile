CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -ffreestanding -nostdlib -no-pie -fno-pic -fno-stack-protector \
         -fno-exceptions -Os -fno-builtin -Wall -Wextra -Wno-unused-variable \
         -Wno-dangling-pointer -Wno-misleading-indentation -I src/kernel/core \
         -I src/kernel/drivers -I src/kernel/fs -I src/kernel/mm \
         -I src/kernel/shell -I src/kernel/cpu -I build \
         -ffunction-sections -fdata-sections -fomit-frame-pointer \
         -fno-asynchronous-unwind-tables -fmerge-all-constants \
         -mgeneral-regs-only -MMD -MP
LDFLAGS = -m elf_i386 -T src/ld/linker.ld --gc-sections -z noexecstack

SRC_DIR = src/kernel
BUILD_DIR = build

-include $(BUILD_DIR)/*.d

C_SRCS = $(shell find $(SRC_DIR) -name '*.c')
ASM_SRCS = $(SRC_DIR)/cpu/gdt_flush.asm $(SRC_DIR)/cpu/idt_load.asm $(SRC_DIR)/cpu/isr_entry.asm
C_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %.c,%.o,$(C_SRCS))))
ASM_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %.asm,%.o,$(ASM_SRCS))))
BOOT_OBJ = $(BUILD_DIR)/boot.o

all: $(BUILD_DIR)/onxos.bin

SRC_DIRS = $(sort $(dir $(C_SRCS)))
VPATH = $(SRC_DIRS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm | $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(BOOT_OBJ): src/boot/boot.asm | $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/bootloader.bin: src/boot/bootloader.asm | $(BUILD_DIR)
	$(AS) -f bin $< -o $@

$(BUILD_DIR)/bootblob.h: $(BUILD_DIR)/bootloader.bin
	python3 tools/patch_boot.py $< --header $@

$(BUILD_DIR)/setup.o: src/kernel/core/setup.c $(BUILD_DIR)/bootblob.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/onxos.elf: $(BOOT_OBJ) $(ASM_OBJS) $(C_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	python3 tools/patch_boot.py $(BUILD_DIR)/bootloader.bin --verify $@

$(BUILD_DIR)/onxos.bin: $(BUILD_DIR)/onxos.elf
	objcopy -O binary $< $(BUILD_DIR)/onxos_tmp.bin
	python3 tools/patch_boot.py $(BUILD_DIR)/bootloader.bin --patch $(BUILD_DIR)/onxos_tmp.bin --header $(BUILD_DIR)/bootblob.h
	rm -f $(BUILD_DIR)/setup.o
	$(CC) $(CFLAGS) -c src/kernel/core/setup.c -o $(BUILD_DIR)/setup.o
	$(LD) $(LDFLAGS) -o $(BUILD_DIR)/onxos.elf $(BOOT_OBJ) $(ASM_OBJS) $(C_OBJS)
	objcopy -O binary $(BUILD_DIR)/onxos.elf $@
	rm -f $(BUILD_DIR)/onxos_tmp.bin
	@echo "Built: $@"
	@ls -lh $@

ISODIR = $(BUILD_DIR)/isoroot
ISO = $(BUILD_DIR)/onxos.iso
DISK = $(BUILD_DIR)/disk.img

$(DISK):
	python3 -c 'import struct; f=open("$(DISK)","wb"); f.write(struct.pack("<III",0x58464E4F,1,0)+b"\x00"*500); f.write(struct.pack("<64sIIiI"+"I"*32+"132s",b"",0,0,0,0,*([0]*32),b"\x00"*132)); f.write(b"\x00"*(5120-512-340)); f.close()'
	@echo "Created: $(DISK)"

iso: $(ISO)

$(ISO): $(BUILD_DIR)/onxos.elf | $(BUILD_DIR)
	rm -rf $(ISODIR) $(ISO)
	mkdir -p $(ISODIR)/boot/grub
	cp $< $(ISODIR)/boot/onxos.bin
	if [ -f $(DISK) ]; then cp $(DISK) $(ISODIR)/boot/; fi
	printf 'set gfxpayload=text\nset timeout=0\nset default=0\n\nmenuentry "onxOS" {\n\tmultiboot2 /boot/onxos.bin\n' > $(ISODIR)/boot/grub/grub.cfg
	if [ -f $(DISK) ]; then printf '\tmodule2 /boot/disk.img\n' >> $(ISODIR)/boot/grub/grub.cfg; fi
	printf '\tboot\n}\n' >> $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR) --compress=xz --fonts="" --locales="" --themes="" 2>/dev/null
	@echo "Built: $(ISO)"
	@ls -lh $(ISO)

clean:
	rm -rf $(BUILD_DIR)

HDD_QEMU = $(BUILD_DIR)/hdd_qemu.img

$(HDD_QEMU): | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1M count=256 2>/dev/null
	@echo "Created: $@"

run: $(ISO) $(HDD_QEMU)
	qemu-system-i386 -boot d -cdrom $(ISO) -drive file=$(HDD_QEMU),format=raw -m 64 -serial stdio -monitor none -nographic

run-hdd: $(HDD_QEMU)
	qemu-system-i386 -drive file=$(HDD_QEMU),format=raw -m 64 -serial stdio -monitor none -nographic

.PHONY: all iso clean run run-hdd
