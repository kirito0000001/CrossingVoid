"""Robust UE5 package header parser: locate tables by structural validation."""
import struct, sys, os

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


class Pkg:
    def __init__(self, path):
        self.path = path
        self.d = open(path, 'rb').read()
        self.n = len(self.d)
        self.parse()

    def parse(self):
        d = self.d
        assert struct.unpack_from('<I', d, 0)[0] == 0x9E2A83C1
        self.legacy = struct.unpack_from('<i', d, 4)[0]
        self.ue3 = struct.unpack_from('<i', d, 8)[0]
        self.ue4 = struct.unpack_from('<i', d, 12)[0]
        self.ue5 = struct.unpack_from('<i', d, 16)[0]
        self.licensee = struct.unpack_from('<i', d, 20)[0]
        # find package name: first fstring starting with '/'
        p = 24
        self.extra_at_24 = struct.unpack_from('<i', d, 24)[0]
        if self.extra_at_24:
            p = 28
        self.custom_count = struct.unpack_from('<i', d, p)[0]; p += 4
        self.customs = []
        for _ in range(self.custom_count):
            g = d[p:p+16]; p += 16
            v = struct.unpack_from('<i', d, p)[0]; p += 4
            self.customs.append((g.hex(), v))
        self.header_size = struct.unpack_from('<i', d, p)[0]; p += 4
        self.pkg_name, p = fstr(d, p)
        self.pkg_flags = struct.unpack_from('<I', d, p)[0]; p += 4
        self.name_count = struct.unpack_from('<i', d, p)[0]
        self.name_offset = struct.unpack_from('<i', d, p+4)[0]
        p += 8
        # parse names
        q = self.name_offset
        self.names = []
        for i in range(self.name_count):
            st = q
            nm, q = fstr(d, q)
            q += 4
            self.names.append((nm, st, q))
        self.name_end = q
        # remaining header (values end with depends_offset i64 then header_size2 i32, pad u32)
        self.header_tail_start = p
        p += 8   # soft paths count+offset
        self.localization_id, p = fstr(d, p)
        p += 8   # gatherable count+offset
        self.export_count = struct.unpack_from('<i', d, p)[0]
        self.export_offset = struct.unpack_from('<i', d, p+4)[0]
        p += 8
        self.import_count = struct.unpack_from('<i', d, p)[0]
        self.import_offset = struct.unpack_from('<i', d, p+4)[0]
        p += 8
        self.soft_paths_count_addr = self.header_tail_start
        # header size sanity
        self.header_size2_addr = p + 4*4 + 8 + 4 + 4
        self.parse_imports()
        self.scan_exports()

    def parse_imports(self):
        d = self.d
        p = self.import_offset
        self.imports = []
        for i in range(self.import_count):
            st = p
            cp, cn, outer, on = struct.unpack_from('<iiii', d, p); p += 16
            if self.ue5 >= 31:
                p += 4
            self.imports.append(dict(i=i, class_package=cp, class_name=cn, outer=outer,
                                     object_name=on, start=st, end=p))
        self.import_end = p
        self.exports_after_imports = struct.unpack_from('<ii', d, p)

    def nm(self, i):
        return self.names[i][0] if 0 <= i < len(self.names) else f'<bad:{i}>'

    def scan_exports(self):
        """Try to find the export record array: E records of fixed size S at some offset."""
        d = self.d
        found = None
        start = self.import_end
        limit = min(self.export_offset + 64 if self.export_offset else len(d), len(d))
        for S in (44, 48, 56, 60, 64, 68, 72):
            for base in range(start, min(start + 512, len(d) - S * 2), 4):
                # validate export_count records
                ok = 0
                for k in range(self.export_count):
                    o = base + k * S
                    if o + 40 > len(d):
                        break
                    ci, si, ti, oi, ni = struct.unpack_from('<iiiii', d, o)
                    if not (0 <= ni < self.name_count):
                        break
                    if not (-self.import_count <= ci <= self.export_count):
                        break
                    if not (-self.import_count <= oi <= self.export_count):
                        break
                    ok += 1
                    if ok >= 4:
                        break
                if ok >= min(4, self.export_count):
                    found = (base, S)
                    break
            if found:
                break
        self.export_rec = found
        if not found:
            return
        base, S = found
        self.exports = []
        for k in range(self.export_count):
            o = base + k * S
            ci, si, ti, oi, ni = struct.unpack_from('<iiiii', d, o)
            fl = struct.unpack_from('<I', d, o + 20)[0]
            size, off = struct.unpack_from('<qq', d, o + 24)
            self.exports.append(dict(class_index=ci, super_index=si, template_index=ti,
                                     outer_index=oi, object_name=ni, flags=fl,
                                     serial_size=size, serial_offset=off, rec_off=o))

    def describe(self):
        print(f'=== {os.path.basename(self.path)} ===')
        print(f'  size={self.n} legacy={self.legacy} ue4={self.ue4} ue5={self.ue5} extra@24={self.extra_at_24}')
        print(f'  customs={self.custom_count}')
        print(f'  header_size={self.header_size} pkg={self.pkg_name!r} flags={self.pkg_flags:#x}')
        print(f'  names={self.name_count}@{self.name_offset:#x} name_end={self.name_end:#x} last={self.names[-1][0]!r}')
        print(f'  imports={self.import_count}@{self.import_offset:#x} import_end={self.import_end:#x}')
        print(f'  export_count={self.export_count} export_offset={self.export_offset:#x}')
        print(f'  ints right after import table: {self.exports_after_imports}')
        print(f'  export record array: {self.export_rec}')


if __name__ == '__main__':
    for a in sys.argv[1:]:
        p = Pkg(a)
        p.describe()
        if p.export_rec:
            for k, e in enumerate(p.exports[:20]):
                print(f"   [{k:3d}] {p.nm(e['object_name']):<44} size={e['serial_size']:<9} off={e['serial_offset']:#x} end={e['serial_offset']+e['serial_size']:#x} ci={e['class_index']} oi={e['outer_index']}")
        print()
