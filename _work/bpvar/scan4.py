import sys

def find_all(data, needle):
    out, start = [], 0
    while True:
        i = data.find(needle, start)
        if i < 0:
            return out
        out.append(i)
        start = i + 1

def hexdump(data, off, ln, base=None):
    if base is None:
        base = off
    lines = []
    for r in range(0, ln, 16):
        chunk = data[off + r: off + r + 16]
        hx = ' '.join(f'{b:02X}' for b in chunk)
        asc = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        lines.append(f'{base + r:08X}  {hx:<47}  {asc}')
    return '\n'.join(lines)

path = sys.argv[1] if len(sys.argv) > 1 else r'C:\CrossingVoid\Content\BaseC\Mode\GM_Main.uasset'
data = open(path, 'rb').read()

probes = ['优纪[ALO]', '59528C6F356C27FC6BD1C409E0F77DB9', '356EEDD04EC031E0F541C381F2978F38', 'CharShapeHas']
hits = {}
for p in probes:
    hits[p] = find_all(data, p.encode('utf-16-le'))
    print(f'{p!r}: {[hex(h) for h in hits[p]]}')

for p in ['优纪[ALO]', '59528C6F356C27FC6BD1C409E0F77DB9']:
    for h in hits[p][:1]:
        lo = max(0, h - 500)
        print(f'\n===== around {p!r} @ {h:#x} (span {lo:#x}..{h+900:#x}) =====')
        print(hexdump(data, lo, 1500))
