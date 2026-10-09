"""Dump uexp sub-header at an offset and probe for export records."""
import struct, sys

path = sys.argv[1]
off = int(sys.argv[2], 16)
d = open(path, 'rb').read()

print(f'--- uexp sub-header @ {off:#x} ---')
for r in range(0, 0x40, 16):
    c = d[off+r:off+r+16]
    print(f'{off+r:08X}  ' + ' '.join(f'{b:02X}' for b in c))

vals = struct.unpack_from('<15I', d, off)
print('  magic=%#x legacy=%d ue3=%d ue4=%d ue5=%d lic=%d cookie=%d' % vals[:7])
print('  rest:', [hex(v) for v in vals[7:]])

P = []
for k in range(14):
    a, b = vals[k], vals[k+1]
    if 0 < a < len(d) and 0 < b < len(d) and a % 4 == 0:
        P.append((k, a, b))
print('  candidate (count,offset) pairs in sub-header:', [(k, hex(a), hex(b)) for k, a, b in P])


def probe_records(base, count, size, limit=6):
    ok = 0
    rows = []
    for k in range(min(count, limit)):
        o = base + k * size
        if o + 40 > len(d):
            break
        ci, si, ti, oi, ni = struct.unpack_from('<iiiii', d, o)
        fl = struct.unpack_from('<I', d, o+20)[0]
        sz, so = struct.unpack_from('<qq', d, o+24)
        rows.append((k, ci, si, ti, oi, ni, hex(fl), sz, hex(so)))
    return rows


for k, a, b in P:
    print(f'\n  try count={a} offset={b:#x} with record sizes 44..80')
    for size in (44, 48, 52, 56, 60, 64, 68, 72, 76, 80):
        rows = probe_records(b, a, size)
        good = [r for r in rows if 0 <= r[5] < 5000 and 0 < r[7] < len(d) and 0 < r[8] < len(d)]
        if len(good) == len(rows) and rows:
            print(f'    size={size}: OK  {rows}')
