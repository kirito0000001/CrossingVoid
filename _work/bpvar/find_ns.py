# -*- coding: utf-8 -*-
import struct

path = r'C:\CrossingVoid\Content\BaseC\Mode\GM_Main.uasset'
data = open(path, 'rb').read()

probes = [
    '7D0634922DF8BDF04ACC1618A3458E6D',   # 新 Name 的 namespace（资产现用）
    '08012575404569CFED2BD1A70F22ABF9',   # 新 Name 的 key
    '59528C6F356C27FC6BD1C409E0F77DB9',   # 旧模板的 namespace
    '356EEDD04EC031E0F541C381F2978F38',   # 旧模板的 key
    '优纪[ALO]',
    '爱丽丝[UW]',
    'Item_ALO_Yuki.Item_ALO_Yuki_C',
]
print('=== 逐编码查找 ===')
for p in probes:
    n8 = data.count(p.encode('utf-8'))
    n16 = data.count(p.encode('utf-16-le'))
    print(f'  {p[:44]:<46} utf8={n8:<4} utf16le={n16}')

print()
print('=== 新 namespace 是否在名字表里（名字表是 utf8 + 4 字节 hash） ===')
ns = '7D0634922DF8BDF04ACC1618A3458E6D'
i = data.find(ns.encode('utf-8'))
print(f'  utf8 命中: {hex(i) if i >= 0 else "无"}')
if i >= 0:
    print('  上下文:', data[max(0,i-24):i+len(ns)+16].hex(' '))

print()
print('=== 旧 namespace 出现位置 ===')
o = '59528C6F356C27FC6BD1C409E0F77DB9'
s = 0
while True:
    i = data.find(o.encode('utf-8'), s)
    if i < 0: break
    print(f'  utf8 @ {hex(i)}')
    s = i + 1
s = 0
while True:
    i = data.find(o.encode('utf-16-le'), s)
    if i < 0: break
    print(f'  utf16 @ {hex(i)}')
    s = i + 1
