"""Minimal UE5 .uasset package reader: header, name table, import/export tables."""
import struct, sys, os

class Reader:
    def __init__(self, data, pos=0):
        self.d = data
        self.p = pos

    def u8(self):
        v = self.d[self.p]; self.p += 1; return v

    def i32(self):
        v = struct.unpack_from('<i', self.d, self.p)[0]; self.p += 4; return v

    def u32(self):
        v = struct.unpack_from('<I', self.d, self.p)[0]; self.p += 4; return v

    def i64(self):
        v = struct.unpack_from('<q', self.d, self.p)[0]; self.p += 8; return v

    def u16(self):
        v = struct.unpack_from('<H', self.d, self.p)[0]; self.p += 2; return v

    def fstr(self):
        n = self.i32()
        if n == 0:
            return ''
        if n > 0:
            raw = self.d[self.p:self.p + n]
            self.p += n
            return raw.split(b'\x00', 1)[0].decode('utf-8', 'replace')
        n = -n
        raw = self.d[self.p:self.p + 2 * n]
        self.p += 2 * n
        return raw.decode('utf-16-le', 'replace').split('\x00', 1)[0]

    def guid(self):
        a, b, c, d = self.u32(), self.u32(), self.u32(), self.u32()
        return f'{a:08X}-{b:08X}-{c:08X}-{d:08X}'


class Package:
    def __init__(self, path):
        self.path = path
        self.data = open(path, 'rb').read()
        self.size = len(self.data)
        self.parse()

    def parse(self):
        r = Reader(self.data)
        self.magic = r.u32()
        self.legacy_file_version = r.i32()
        self.legacy_ue3 = r.i32()
        self.file_version_ue4 = r.i32()
        self.file_version_ue5 = r.i32()
        self.file_version_licensee = r.i32()
        self.custom_version_count = r.i32()
        self.custom_versions = [(r.guid(), r.i32()) for _ in range(self.custom_version_count)]
        self.total_header_size = r.i32()
        self.package_name = r.fstr()
        self.package_flags = r.u32()
        self.name_count = r.i32()
        self.name_offset = r.i32()
        save = r.p
        r.p = self.name_offset
        self._scan_name_table()
        r.p = save
        self.soft_obj_paths_count = r.i32()
        self.soft_obj_paths_offset = r.i32()
        self.localization_id = r.fstr()
        self.gatherable_text_data_count = r.i32()
        self.gatherable_text_data_offset = r.i32()
        self.export_count = r.i32()
        self.export_offset = r.i32()
        self.import_count = r.i32()
        self.import_offset = r.i32()
        self.cell_export_count = r.i32()
        self.cell_export_offset = r.i32()
        self.cell_import_count = r.i32()
        self.cell_import_offset = r.i32()
        self.import_type_hierarchies_count = r.i32()
        self.import_type_hierarchies_offset = r.i32()
        self.import_optional_package_data_count = r.i32()
        self.import_optional_package_data_offset = r.i32()
        self.asset_registry_data_offset = r.i32()
        self.asset_registry_data_size = r.i32()
        self.depends_offset = r.i64()
        self.total_header_size2 = r.i32()
        self.pad = r.u32()
        self.parse_imports()
        self.parse_exports()

    def _scan_name_table(self):
        r = Reader(self.data, self.name_offset)
        names = []
        for i in range(self.name_count):
            start = r.p
            name = r.fstr()
            h1, h2 = r.u16(), r.u16()
            names.append({'index': i, 'name': name, 'start': start, 'end': r.p})
        self.name_entries = names
        self.name_table_end = r.p

    def parse_imports(self):
        r = Reader(self.data, self.import_offset)
        self.imports = []
        for i in range(self.import_count):
            start = r.p
            class_pkg = r.i32()
            class_name = r.i32()
            outer = r.i32()
            obj_name = r.i32()
            optional = r.i32() if self.file_version_ue5 >= 31 else 0
            self.imports.append({'index': i, 'class_package': class_pkg, 'class_name': class_name,
                                 'outer': outer, 'object_name': obj_name, 'optional': optional,
                                 'start': start, 'end': r.p})
        self.import_table_end = r.p

    def parse_exports(self):
        r = Reader(self.data, self.export_offset)
        keep_guid = self.file_version_ue5 >= 32
        self.exports = []
        for i in range(self.export_count):
            rec = {'index': i, 'start': r.p}
            rec['class_index'] = r.i32()
            rec['super_index'] = r.i32()
            rec['template_index'] = r.i32()
            rec['outer_index'] = r.i32()
            rec['object_name'] = r.i32()
            rec['object_flags'] = r.u32()
            rec['serial_size'] = r.i64()
            rec['serial_offset'] = r.i64()
            rec['forced_export'] = r.i32()
            if self.file_version_ue5 < 43:
                rec['not_for_client'] = r.i32()
                rec['not_for_server'] = r.i32()
            rec['package_guid'] = r.guid()
            rec['is_inherited_instance'] = r.i32()
            if self.file_version_ue5 >= 49:
                rec['package_flags'] = r.u32()
            if self.file_version_ue5 < 96 and False:
                pass
            rec['generate_public_hash'] = r.i32()
            if self.file_version_ue5 >= 74 or self.file_version_ue5 < 74:
                # UE5.3+ (FileVersionUE5 >= 1008?) : ScriptSerializationStartOffset/EndOffset
                pass
            rec['end'] = r.p
            self.exports.append(rec)
        self.export_table_end = r.p

    def name(self, idx):
        if 0 <= idx < self.name_count:
            return self.name_entries[idx]['name']
        return f'<bad:{idx}>'

    def import_path(self, idx):
        if idx == 0:
            return 'None'
        i = -idx - 1
        if not (0 <= i < len(self.imports)):
            return f'<bad:{idx}>'
        rec = self.imports[i]
        outer = rec['outer']
        if outer < 0:
            base = self.import_path(outer)
        elif outer == 0:
            base = self.package_name
        else:
            base = self.ref(outer)
        return f"{base}.{self.name(rec['object_name'])}"

    def ref(self, idx):
        if idx < 0:
            return self.import_path(idx)
        if idx == 0:
            return 'None'
        i = idx - 1
        if i < len(self.exports):
            return self.name(self.exports[i]['object_name'])
        return f'<bad:{idx}>'

    def dump(self):
        print(f'=== {os.path.basename(self.path)} ===')
        print(f'  size={self.size} legacy={self.legacy_file_version} ue4={self.file_version_ue4} ue5={self.file_version_ue5}')
        print(f'  name_count={self.name_count}@{self.name_offset:#x} name_end={self.name_table_end:#x}')
        print(f'  import_count={self.import_count}@{self.import_offset:#x} import_end={self.import_table_end:#x}')
        print(f'  export_count={self.export_count}@{self.export_offset:#x} export_end={self.export_table_end:#x}')
        print(f'  header={self.total_header_size} (2nd={self.total_header_size2}) pkg={self.package_name!r}')
        print(f'  exports:')
        for e in self.exports:
            print(f"   [{e['index']:3d}] {self.name(e['object_name']):<44} cls={self.name(self.imports[-e['class_index']-1]['object_name']) if e['class_index'] < 0 else self.ref(e['class_index']):<34}"
                  f" size={e['serial_size']:<8} off={e['serial_offset']:#010x} end={e['serial_offset']+e['serial_size']:#x}")


if __name__ == '__main__':
    for arg in sys.argv[1:]:
        p = Package(arg)
        p.dump()
        print()
