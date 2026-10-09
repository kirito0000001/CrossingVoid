import struct, sys

def fstr(d, p):
    n = struct.unpack_from('<i', d, p)[0]
    p += 4
    if n == 0:
        return '', p
    if n > 0:
        return d[p:p+n-1].decode('utf-8', 'replace'), p + n
    n = -n
    return d[p:p+2*n-2].decode('utf-16-le', 'replace'), p + 2 * n

path = sys.argv[1]
d = open(path, 'rb').read()
start = int(sys.argv[2], 16)
count = int(sys.argv[3]) if len(sys.argv) > 3 else 10

# raw bytes before
for off in range(start - 0x30, start + 0x20, 16):
    c = d[off:off+16]
    print(f'{off:08X}  ' + ' '.join(f'{b:02X}' for b in c))
print()

p = start
out = []
for i in range(count):
    st = p
    nm, p = fstr(d, p)
    h = d[p:p+4]
    out.append(f'  [{i}] @{st:#x} {nm!r} hash={h.hex()}')
    p += 4
sys.stdout.buffer.write(('\n'.join(out) + '\n').encode('utf-8', 'replace'))
print(f'end of {count} names: {p:#x}')
