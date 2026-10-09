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
for start in [0x212]:
    print(f'--- parse name table from {start:#x} ---')
    p = start
    for i in range(12):
        st = p
        nm, p = fstr(d, p)
        h = d[p:p+4]
        print(f'  [{i}] @{st:#x} {nm!r} hash={h.hex()}')
        p += 4
