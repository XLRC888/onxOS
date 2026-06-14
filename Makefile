CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -ffreestanding -nostdlib -no-pie -fno-pic -fno-stack-protector \
         -fno-exceptions -Os -fno-builtin -Wall -Wextra -Wno-unused-variable \
         -Wno-dangling-pointer -Wno-misleading-indentation -I src/kernel \
         -ffunction-sections -fdata-sections -fomit-frame-pointer \
         -fno-asynchronous-unwind-tables -fmerge-all-constants
LDFLAGS = -m elf_i386 -T src/ld/linker.ld --gc-sections

SRC_DIR = src/kernel
BUILD_DIR = build
GEN_DIR = $(BUILD_DIR)/gen

C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.asm)
C_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SRCS))
BOOT_OBJ = $(BUILD_DIR)/boot.o
BOOTBLOB = $(BUILD_DIR)/bootblob.bin
BOOTBLOB_H = $(GEN_DIR)/bootblob_data.h
SIZE_MK = $(GEN_DIR)/size.mk
PRE_KERN = $(BUILD_DIR)/onxos.pre.bin
C_OBJS_NO_SETUP = $(filter-out $(BUILD_DIR)/setup.o, $(C_OBJS))

all: $(BUILD_DIR)/onxos.bin

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/boot.o: src/boot/boot.asm
	$(AS) -f elf32 $< -o $@

$(GEN_DIR)/dummy/bootblob_data.h:
	mkdir -p $(GEN_DIR)/dummy
	printf 'unsigned char build_bootblob_bin[]={1,' > $@; \
	for i in $$(seq 3071); do printf '0,' >> $@; done; \
	printf '};\nunsigned int build_bootblob_bin_len=3072;\n' >> $@

$(BUILD_DIR)/setup_pre.o: $(SRC_DIR)/setup.c $(GEN_DIR)/dummy/bootblob_data.h
	$(CC) $(CFLAGS) -I$(GEN_DIR)/dummy -c $< -o $@

$(PRE_KERN): $(BOOT_OBJ) $(ASM_OBJS) $(C_OBJS_NO_SETUP) $(BUILD_DIR)/setup_pre.o
	$(LD) $(LDFLAGS) -o $@ $^
	strip --strip-all -R .comment -R .note $@ 2>/dev/null || true

$(SIZE_MK): $(PRE_KERN)
	mkdir -p $(GEN_DIR)
	objcopy -O binary $(PRE_KERN) /tmp/_kflat 2>/dev/null; \
	F=$$(wc -c < /tmp/_kflat | awk '{print $$1}'); \
	S=$$(( (F + 511) / 512 )); \
	FS=$$(printf '%X' $$F); \
	MS=$$(readelf -l $(PRE_KERN) | awk '/LOAD/{addr=$$3; sz=$$6} END{print addr, sz}' | python3 -c "import sys; a,s=sys.stdin.read().split(); print('%X' % (int(a,16)+int(s,16)-0x100000))"); \
	echo "KERN_SECT=$$S" > $@; \
	echo "KERN_FSZ=0x$$FS" >> $@; \
	echo "BSS_SZ=0x$$MS" >> $@

-include $(SIZE_MK)

$(BOOTBLOB): src/boot/bootloader.asm $(SIZE_MK)
	$(AS) -f bin -DKERN_SECT=$(KERN_SECT) -DKERN_FSZ=$(KERN_FSZ) -DBSS_SZ=$(BSS_SZ) $< -o $@

$(BOOTBLOB_H): $(BOOTBLOB)
	mkdir -p $(GEN_DIR)
	xxd -i $< > $@

$(BUILD_DIR)/setup_final.o: $(SRC_DIR)/setup.c $(BOOTBLOB_H)
	$(CC) $(CFLAGS) -I$(GEN_DIR) -c $< -o $@

$(BUILD_DIR)/onxos.elf: $(BOOT_OBJ) $(ASM_OBJS) $(C_OBJS_NO_SETUP) $(BUILD_DIR)/setup_final.o
	$(LD) $(LDFLAGS) -o $@ $^
	strip --strip-all -R .comment -R .note $@ 2>/dev/null || true

$(BUILD_DIR)/onxos.bin: $(BUILD_DIR)/onxos.elf
	objcopy -O binary $< $@
	@echo "Built: $(BUILD_DIR)/onxos.bin"
	@ls -lh $@

ISODIR = $(BUILD_DIR)/isoroot
ISO = $(BUILD_DIR)/onxos.iso
DISK = $(BUILD_DIR)/disk.img

$(DISK):
	python3 -c 'import struct; f=open("$(DISK)","wb"); f.write(struct.pack("<III",0x58464E4F,1,0)+b"\x00"*500); f.write(struct.pack("<64sIIiI"+"I"*32+"132s",b"",0,0,0,0,*([0]*32),b"\x00"*132)); f.write(b"\x00"*(5120-512-340)); f.close()'
	@echo "Created: $(DISK)"

iso: $(ISO)

$(ISO): $(BUILD_DIR)/onxos.elf $(DISK)
	rm -rf $(ISODIR) $(ISO)
	mkdir -p $(ISODIR)/boot/grub
	cp $(BUILD_DIR)/onxos.elf $(ISODIR)/boot/onxos.bin
	cp $(DISK) $(ISODIR)/boot/disk.img
	printf 'set timeout=0\nset default=0\nset gfxpayload=text\nset gfxpayload_keep=text\nterminal_output console\n\nmenuentry "onxOS" {\n\tmultiboot /boot/onxos.bin\n\tmodule /boot/disk.img\n\tboot\n}\n' > $(ISODIR)/boot/grub/grub.cfg
	grub-mkimage -O i386-pc-eltorito -p '/boot/grub' -o $(ISODIR)/boot/grub/cdboot.img biosdisk iso9660 multiboot
	xorriso -as mkisofs -iso-level 3 -full-iso9660-filenames -R -J --grub2-boot-info --grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img -b boot/grub/cdboot.img -no-emul-boot -boot-load-size 4 -boot-info-table -o $(ISO) $(ISODIR) 2>/dev/null
	@echo "Built: $(ISO)"
	@ls -lh $(ISO)

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/onxos*.bin $(BUILD_DIR)/disk.img $(ISO) $(BOOTBLOB)
	rm -rf $(ISODIR) $(GEN_DIR)
	rm -f qemu.log

HDD_QEMU = $(BUILD_DIR)/hdd_qemu.img

$(HDD_QEMU):
	dd if=/dev/zero of=$@ bs=1M count=256 2>/dev/null
	@echo "Created: $@"

run: $(ISO) $(HDD_QEMU)
	qemu-system-i386 -boot d -cdrom $(ISO) -drive file=$(HDD_QEMU),format=raw -m 64 -nographic 2>qemu.log

run-hdd: $(HDD_QEMU)
	qemu-system-i386 -drive file=$(HDD_QEMU),format=raw -m 64 -serial file:qemu.log &

.PHONY: all iso clean run run-hdd
