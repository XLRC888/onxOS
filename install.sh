#!/bin/bash
set -e

SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cat << 'EOF'
  ___    _   _   _   _   ___    ____
 / _ \  | \ | | \ \ / / / _ \  | ___|
| | | | |  \| |  \ | / | | | | |___ |
| |_| | | |\  |  / | \ | |_| |  __| |
 \___/  |_| \_| /_/ \_\ \___/  |____|
EOF
echo ""

TARGET="${1:-/dev/sda}"

echo "Target: $TARGET"
echo "WARNING: ALL DATA ON $TARGET WILL BE DESTROYED"
read -p "Continue? (y/N) " confirm
[[ "$confirm" == [yY] ]] || exit 1

if [ ! -b "$TARGET" ] && [ ! -f "$TARGET" ]; then
    echo "Error: $TARGET not found"
    exit 1
fi

echo ""
echo "=== Creating partition table ==="
dd if=/dev/zero of="$TARGET" bs=1M count=2 conv=notrunc status=none 2>/dev/null || dd if=/dev/zero of="$TARGET" bs=1M count=2 conv=notrunc 2>/dev/null
parted -s "$TARGET" mklabel msdos
parted -s "$TARGET" mkpart primary ext2 2048s 526335s
parted -s "$TARGET" set 1 boot on

echo "=== Creating data partition (type DA) ==="
parted -s "$TARGET" mkpart primary 526336s 100% || true
python3 -c "
import struct
with open('$TARGET', 'r+b') as f:
    mbr = bytearray(f.read(512))
    # Just change the type byte of partition 2 (at offset 462+4=466)
    mbr[466] = 0xDA
    f.seek(0)
    f.write(mbr)
    ls = struct.unpack('<I', mbr[470:474])[0]
    sz = struct.unpack('<I', mbr[474:478])[0]
    print(f'  Data partition at LBA {ls}, size {sz} sectors')
"

echo "=== Formatting boot partition ==="
mkfs.ext2 -q "${TARGET}1" 2>/dev/null
sync

echo "=== Installing GRUB ==="
MP=$(mktemp -d)
mount "${TARGET}1" "$MP"
mkdir -p "$MP/boot/grub"
grub-install --target=i386-pc --boot-directory="$MP/boot" "$TARGET" 2>&1

echo "=== Copying kernel and disk image ==="
cp "$SELF/build/onxos.bin" "$MP/boot/" 2>/dev/null || {
    echo ""
    echo "Error: build/onxos.bin not found"
    echo "Run 'make iso' first, then re-run this script"
    umount "$MP" 2>/dev/null
    rm -rf "$MP"
    exit 1
}
cp "$SELF/build/disk.img" "$MP/boot/" 2>/dev/null || {
    echo "Warning: build/disk.img not found, creating blank"
    dd if=/dev/zero of="$MP/boot/disk.img" bs=512 count=4608 status=none
}

cat > "$MP/boot/grub/grub.cfg" << 'GRUB'
set timeout=0
set default=0
menuentry "onxOS" {
    multiboot /boot/onxos.bin
    module /boot/disk.img
    boot
}
GRUB

echo "=== Writing FS seed to data partition ==="
PART2_START=$(python3 -c "
import struct
with open('$TARGET', 'rb') as f:
    mbr = f.read(512)
    ls = struct.unpack('<I', mbr[470:474])[0]
    print(ls)
")
dd if="$MP/boot/disk.img" of="$TARGET" bs=512 seek=$PART2_START conv=notrunc 2>/dev/null || true

sync
umount "$MP"
rm -rf "$MP"

echo ""
echo "=== Done! onxOS installed to $TARGET ==="
echo ""
echo "Next steps:"
echo "  1. Remove USB, boot from $TARGET"
echo "  2. onxOS boots and finds the data partition"
echo "  3. Create files with tau, type exit to save"
echo "  4. Reboot, files persist"
echo ""
