import struct, sys, subprocess

elf_path = sys.argv[1]
out_path = sys.argv[2]

data = open(elf_path, 'rb').read()
phoff = struct.unpack_from('<I', data, 28)[0]
phnum = struct.unpack_from('<H', data, 44)[0]
phent = struct.unpack_from('<H', data, 42)[0]

load_off = 0
load_fsz = 0
for i in range(phnum):
    off = phoff + i * phent
    ptype = struct.unpack_from('<I', data, off)[0]
    if ptype == 1:
        load_off = struct.unpack_from('<I', data, off + 4)[0]
        load_fsz = struct.unpack_from('<I', data, off + 16)[0]
        break

entry_s = subprocess.check_output(['readelf', '-h', elf_path]).decode()
entry = 0x100000
for line in entry_s.split('\n'):
    if 'Entry point' in line:
        entry = int(line.split()[-1], 16)

endk_s = subprocess.check_output(['nm', elf_path]).decode()
endk = 0x118000
for line in endk_s.split('\n'):
    if 'end_of_kernel' in line:
        endk = int(line.split()[0], 16)

flat = bytearray(data[load_off:load_off + load_fsz])
flags = struct.unpack_from('<I', flat, 4)[0] | (1 << 16)
struct.pack_into('<I', flat, 4, flags)
struct.pack_into('<I', flat, 8, (-(0x1BADB002 + flags)) & 0xFFFFFFFF)
struct.pack_into('<I', flat, 12, 0x200000)
struct.pack_into('<I', flat, 16, 0x200000)
struct.pack_into('<I', flat, 20, 0x200000 + load_fsz)
struct.pack_into('<I', flat, 24, endk)
struct.pack_into('<I', flat, 28, entry)

open(out_path, 'wb').write(flat)
