import sys
d = open(r'C:\CrossingVoid\Content\BaseC\Mode\GM_Main.uasset', 'rb').read()

def find_all(n):
    out, s = [], 0
    while True:
        i = d.find(n, s)
        if i < 0:
            return out
        out.append(i); s = i + 1

for label in ['LocalVarName', 'BPVar', 'ALLChar', 'ALLCharacterGet', 'ItemSaveSingleData']:
    for enc, name in (('utf-16-le', 'u16'), ('utf-8', 'utf8')):
        hits = find_all(label.encode(enc))
        if hits:
            print(f'{label:<20} {name:<5} {len(hits):3d}  {[hex(h) for h in hits[:12]]}')
        else:
            print(f'{label:<20} {name:<5}   0')

print()
for off in [0x151bf0, 0xcde1f]:
    print(f'--- context @ {off:#x} ---')
    for r in range(off - 0x30, off + 0x60, 16):
        c = d[r:r+16]
        print(f'{r:08X}  ' + ' '.join(f'{b:02X}' for b in c))
    print()
