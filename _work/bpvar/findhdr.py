"""Brute-force locate the header field layout: name table must parse exactly and end at import offset."""
import struct, sys

def fstr(d, p):
    n = struct.unpack_from('<i', d, p)[0]
    p += 4
    if n == 0:
        return '', p
    if n > 0:
        raw = d[p:p+n]; p += n
        return raw.split(b'\x00', 1)[0].decode('utf-8', 'replace'), p
    n = -n
    raw = d[p:p+2*n]; p += 2*n
    return raw.decode('utf-16-le', 'replace').split('\x00', 1)[0], p

for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    print('===', path, len(d))
    # locate package name string: a 4-byte len equal to len('/Game/...') followed by that ASCII text
    cands = []
    for p in range(0, 0x400):
        try:
            s, end = fstr(d, p)
        except Exception:
            continue
        if s.startswith('/Game/') or s.startswith('/Engine/'):
            cands.append((p, s, end))
    for p, s, end in cands:
        print(f'  pkgname @ {p:#x}: {s!r}')
        q = end
        flags = struct.unpack_from('<I', d, q)[0]; q += 4
        print(f'    flags={flags:#x}')
        for probe in range(q, q + 32, 4):
            nc, no = struct.unpack_from('<ii', d, probe)
            if 0 < nc < 200000 and 0 < no < len(d):
                # try parsing name table
                try:
                    pp = no
                    ok = True
                    last = None
                    for i in range(nc):
                        nm, pp = fstr(d, pp)
                        pp += 4
                        last = nm
                    ni, nio = struct.unpack_from('<ii', d, pp)
                    print(f'    probe+{probe-q:#x}: name_count={nc}@{no:#x} parse_end={pp:#x} -> next ints {ni},{nio} last={last!r}  {"OK" if 0 < ni < 5000 and 0 < nio < len(d) else ""}')
                except Exception as e:
                    print(f'    probe+{probe-q:#x}: name_count={nc}@{no:#x} parse FAILED {e}')
    print()
