# -*- coding: utf-8 -*-
"""独立验证：GM_Main.uasset 落盘后，26 条数据是否真的在文件里。"""
import re

path = r'C:\CrossingVoid\Content\BaseC\Mode\GM_Main.uasset'
data = open(path, 'rb').read()
print(f'文件大小: {len(data)} 字节')

# 期望文本：Saved 里那份备份
expect = open(r'C:\CrossingVoid\Saved\ZDBpVar_ALLChar.txt', encoding='utf-8').read()
print(f'期望数组: {len(expect)} 字符')

# uasset 里的字符串是 UTF-16LE
blob = expect.encode('utf-16-le')
hits = []
start = 0
while True:
    i = data.find(blob, start)
    if i < 0:
        break
    hits.append(i)
    start = i + 1
print(f'完整数组在文件中的出现次数: {len(hits)}  -> {[hex(h) for h in hits]}')

# 逐段抽查：条目分隔处、每个角色名
probes = [
    ('条目分隔符 ),(', '),('),
    ('Item_UW_Alice 类路径', "/Game/ITems/CharItemS/Item_UW_Alice.Item_UW_Alice_C"),
    ('爱丽丝[UW] 名字', '爱丽丝[UW]'),
    ('优纪[ALO] 名字', '优纪[ALO]'),
    ('统一 Health', 'Health=6000'),
    ('ALO 修正后的 Speed', 'Speed=800'),
    ('诗乃的 Speed', 'Speed=120'),
]
print()
for label, s in probes:
    n = data.count(s.encode('utf-16-le'))
    print(f'  {label:<24} 出现 {n} 次')

# 每一个角色的 Class 路径都必须在文件里
classes = re.findall(r"BlueprintGeneratedClass'([^']+)'", expect)
missing = [c for c in classes if data.count(c.encode('utf-16-le')) == 0]
print()
print(f'角色类路径总数: {len(classes)}')
print(f'文件里缺失的  : {len(missing)} {missing if missing else "(全部就位)"}')

# 统一数值是否真的 26 份
print()
for key in ['Health=6000', 'Attack=800', 'PhyDefense=600', 'MagDefense=600', 'Critical=600', 'CriticalC=600', 'SkillLevel=(5,5,5,5)']:
    print(f'  {key:<22} 出现 {data.count(key.encode("utf-16-le"))} 次（应为 26）')

# Speed 是否 26 个各自的值
speeds = re.findall(r'Speed=(\d+)', expect)
print()
print(f'Speed 取值: {speeds}')
