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
print(f'size={len(data)}')

probes = []
for s in ['优纪[ALO]', '优纪', '沉睡骑士', 'Item_ALO_Yuki', 'KismetSchema',
          '59528C6F356C27FC6BD1C409E0F77DB9', '356EEDD04EC031E0F541C381F2978F38',
          'CharShapeHas', 'CriticalC', 'Description', 'SkillLevel']:
    probes.append(s.encode('utf-8'))

for enc, label in [('utf-8', 'utf8'), ('utf-16-le', 'utf16le')]:
    print(f'==== encoding {label} ====')
    for p in probes:
        needle = None
        if enc == 'utf-8':
            needle = p
        else:
            needle = p.decode('utf-8').encode('utf-16-le')
        hits = find_all(data, needle)
        if hits:
            print(f'  {p.decode("utf-8")!r}: {len(hits)} hits {[hex(h) for h in hits[:10]]}')
        else:
            print(f'  {p.decode("utf-8")!r}: none')

hits = find_all(data, 'Item_ALO_Yuki'.encode('utf-8'))
for h in hits[:3]:
    lo = max(0, h - 400)
    print(f'\n===== Item_ALO_Yuki @ {h:#x} =====')
    print(hexdump(data, lo, 1400))
