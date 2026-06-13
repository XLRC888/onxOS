#!/usr/bin/env python3
import os
import struct
import sys

OUTSIDERS_DIR = 'outsiders'
OUTPUT_IMG = 'build/outsiders.img'
SECTOR_SIZE = 512
NODE_DISK_SECTORS = 9
NODE_CONTENT_SIZE = NODE_DISK_SECTORS * SECTOR_SIZE - SECTOR_SIZE  # 4096
FS_MAGIC = 0x58464E4F  # 'ONX' little-endian

def pack():
    if not os.path.isdir(OUTSIDERS_DIR):
        print(f"outsiders: {OUTSIDERS_DIR} not found, creating empty")
        os.makedirs(OUTSIDERS_DIR, exist_ok=True)

    files = sorted([f for f in os.listdir(OUTSIDERS_DIR)
                    if os.path.isfile(os.path.join(OUTSIDERS_DIR, f))])

    total_nodes = 1 + len(files)  # root + files
    total_sectors = 1 + total_nodes * NODE_DISK_SECTORS
    img_size = total_sectors * SECTOR_SIZE

    img = bytearray(img_size)

    # superblock at LBA 0
    struct.pack_into('<I', img, 0, FS_MAGIC)
    struct.pack_into('<I', img, 4, total_nodes)
    struct.pack_into('<I', img, 8, 0)  # root_index = 0

    def write_node(index, name, ntype, content, parent_index, children):
        base_lba = 1 + index * NODE_DISK_SECTORS
        base_byte = base_lba * SECTOR_SIZE
        meta = bytearray(SECTOR_SIZE)
        name_bytes = name.encode('utf-8')[:63]
        meta[0:len(name_bytes)] = name_bytes
        meta[64:68] = struct.pack('<I', ntype)
        content_size = len(content)
        meta[68:72] = struct.pack('<I', min(content_size, NODE_CONTENT_SIZE))
        meta[72:76] = struct.pack('<i', parent_index)
        meta[76:80] = struct.pack('<I', len(children))
        for j, c in enumerate(children):
            off = 80 + j * 4
            meta[off:off+4] = struct.pack('<I', c)
        img[base_byte:base_byte+SECTOR_SIZE] = meta
        if ntype == 1 and content_size > 0:
            copy_len = min(content_size, NODE_CONTENT_SIZE)
            img[base_byte+SECTOR_SIZE:base_byte+SECTOR_SIZE+copy_len] = content[:copy_len]

    write_node(0, '', 0, b'', 0, list(range(1, 1 + len(files))))

    for i, fname in enumerate(files):
        fpath = os.path.join(OUTSIDERS_DIR, fname)
        with open(fpath, 'rb') as fh:
            content = fh.read()
        write_node(1 + i, fname, 1, content, 0, [])

    os.makedirs(os.path.dirname(OUTPUT_IMG), exist_ok=True)
    with open(OUTPUT_IMG, 'wb') as fh:
        fh.write(img)

    print(f"outsiders: {len(files)} files packed into {OUTPUT_IMG} ({len(img)} bytes)")

if __name__ == '__main__':
    pack()
