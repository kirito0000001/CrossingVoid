"""Locate export records by searching for the (serial_size, serial_offset) i64 pair
that covers a target payload offset."""
import struct, sys

def find_pair(d, target, search_lo=0, search_hi=None):
    hits = []
    hi = search_hi or len(d)
    for p in range(search_lo, hi - 16, 4):
        size, off = struct.unpack_from('<qq', d, p)
        if off <= target < off + size and 0 < size < len(d) and 0 < off < len(d):
            hits.append((p, size, off))
    return hits

path = sys.argv[1]
target = int(sys.argv[2], 16)
lo = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0
hi = int(sys.argv[4], 16) if len(sys.argv) > 4 else None
d = open(path, 'rb').read()
print(f'file={path} size={len(d)} target={target:#x}')
for p, size, off in find_pair(d, target, lo, hi):
    print(f'  pair@ {p:#x}  size={size}  off={off:#x}  end={off+size:#x}')
    # dump the export record candidate: 20 bytes before the pair
    s = max(0, p - 24)
    for r in range(s, p + 24, 16):
        c = d[r:r+16]
        print(f'    {r:08X}  ' + ' '.join(f'{b:02X}' for b in c))
