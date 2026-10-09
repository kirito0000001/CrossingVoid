import struct, sys

def find_all(d, needle):
    out, s = [], 0
    while True:
        i = d.find(needle, s)
        if i < 0:
            return out
        out.append(i); s = i + 1

path = sys.argv[1]
d = open(path, 'rb').read()
print(f'{path} size={len(d)}')
for nm, needle in [('magic', bytes.fromhex('C1832A9E')),
                   ('pkgname-ascii', b'/Game/BaseC/Mode/GM_Main\x00'),
                   ('name-len-prefix', struct.pack('<i', 24) + b'/Game/BaseC/Mode/GM_Main')]:
    hits = find_all(d, needle)
    print(f'  {nm}: {len(hits)} -> {[hex(h) for h in hits[:20]]}')
