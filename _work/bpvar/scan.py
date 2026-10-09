import re, sys, os

def find_all(data, needle):
    out = []
    start = 0
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

path = sys.argv[1]
data = open(path, 'rb').read()
print(f'file={path} size={len(data)}')

for needle in [b'ALLChar', b'D72312BF462D8CF4F1E0819C730E9874', b'ItemSaveSingleData', b'NewVariables', b'FBPVariableDescription', b'\x0c\x00\x00\x00ALLChar']:
    hits = find_all(data, needle)
    print(f'--- {needle!r}: {len(hits)} hits at {[hex(h) for h in hits[:20]]}')

# dump around first ALLChar
hits = find_all(data, b'ALLChar')
if hits:
    h = hits[0]
    lo = max(0, h - 96)
    print('\n=== context around first ALLChar ===')
    print(hexdump(data, lo, 400))
