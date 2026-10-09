import struct, sys
path = sys.argv[1]
d = open(path, 'rb').read()
off = int(sys.argv[2], 16)
ln = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x90
for r in range(0, ln, 16):
    c = d[off+r:off+r+16]
    print(f'{off+r:08X}  ' + ' '.join(f'{b:02X}' for b in c))
print()
for o in range(off, off + ln, 4):
    print(f'  {o:#x}: i32={struct.unpack_from("<i", d, o)[0]:<12} u32={struct.unpack_from("<I", d, o)[0]:#x}')
