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

path = r'C:\CrossingVoid\Content\BaseC\Mode\GM_Main.uasset'
data = open(path, 'rb').read()

for base_needle, before, after in [(b'NewVariables', 64, 800), (b'ItemSaveSingleData', 400, 300)]:
    for h in find_all(data, base_needle):
        lo = max(0, h - before)
        print(f'===== {base_needle!r} @ {h:#x} =====')
        print(hexdump(data, lo, before + after))
        print()
