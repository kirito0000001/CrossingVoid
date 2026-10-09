# -*- coding: utf-8 -*-
"""把 ZDBpVar 报告里的 26 个角色，拼成 GM_Main 里 ALLChar 变量默认值的数组文本。

统一数值：Health/Attack/PhyDefense/MagDefense/Critical/CriticalC/Synchronize/
SkillLevel/CharShapeHas —— 26 个角色本来就完全一致，直接取。
每个角色只保留自己的：Class / Name / Description / KeyWords / Speed。
"""
import re, json, io

SRC = r'C:\CrossingVoid\_work\bpvar\out\values2.log'
OUT_TXT = r'C:\CrossingVoid\_work\bpvar\out\ALLChar_DefaultValue.txt'
OUT_MD = r'C:\CrossingVoid\_work\bpvar\out\角色数值总表.md'

lines = open(SRC, encoding='utf-8').read().splitlines()

chars, cur, section = [], None, None
for line in lines:
    m = re.match(r'^=== (.+) ===$', line)
    if m:
        cur = {'asset': m.group(1), 'item': {}, 'char': {}}
        chars.append(cur); section = None; continue
    if cur is None: continue
    m = re.match(r'^Class = (.+)$', line)
    if m:
        cur['class'] = m.group(1).strip(); continue
    m = re.match(r'^  \[(\w+)\] (\w+)$', line)
    if m:
        section = m.group(2); continue
    m = re.match(r'^  (\w+)\s+= ?(.*)$', line)
    if m and section is None:
        cur['item'][m.group(1)] = m.group(2).strip(); continue
    m = re.match(r'^    (\w+)\s+= ?(.*)$', line)
    if m and section == 'CharData':
        cur['char'][m.group(1)] = m.group(2).strip()

assert len(chars) == 26, f'期望 26 个角色，实到 {len(chars)}'

# 统一数值：按用户指定的「模板那组」（原 ALLChar 里那条 Item_ALO_Yuki 的数值）。
# 角色蓝图自身那组是 Health 2000 / Attack 50 / 双防 50 / 双暴 10 / SkillLevel(1,1,1,1)，
# 这里刻意换成模板组，等于把 26 个角色整体拉到模板水平。
unified = {
    'SkillLevel':   '(5,5,5,5)',
    'CharShapeHas': '(True,False)',
    'Health':       '6000',
    'Attack':       '800',
    'PhyDefense':   '600',
    'MagDefense':   '600',
    'Critical':     '600',
    'CriticalC':    '600',
}
# 断言：角色蓝图里除了 SkillLevel，其余本来就是全体一致的（防止源数据变了没人发现）
for k in ['CharShapeHas', 'Health', 'Attack', 'PhyDefense', 'MagDefense', 'Critical', 'CriticalC']:
    vals = {c['char'].get(k) for c in chars}
    assert len(vals) == 1, f'{k} 在角色蓝图里并不一致: {vals}'

# NSLOCTEXT("ns", "key", "text") 原样保留
def field(c, name):
    return c['item'].get(name, '')

entries = []
for c in chars:
    name = field(c, 'Name')
    desc = field(c, 'Description') or '""'
    kw = field(c, 'KeyWords') or '()'
    ch = c['char']
    entry = (
        '('
        f'Class={c["class"]},'
        f'Name={name},'
        f'Description={desc},'
        f'KeyWords={kw},'
        f'SkillLevel={unified["SkillLevel"]},'
        f'CharShapeHas={unified["CharShapeHas"]},'
        f'Speed={ch["Speed"]},'
        f'Health={unified["Health"]},'
        f'Attack={unified["Attack"]},'
        f'PhyDefense={unified["PhyDefense"]},'
        f'MagDefense={unified["MagDefense"]},'
        f'Critical={unified["Critical"]},'
        f'CriticalC={unified["CriticalC"]}'
        ')'
    )
    entries.append(entry)

array_text = '(' + ','.join(entries) + ')'
open(OUT_TXT, 'w', encoding='utf-8', newline='').write(array_text)

# ---- 对照：和蓝图里现有的那一条比一比
existing = '((Class="/Script/Engine.BlueprintGeneratedClass\'/Game/ITems/CharItemS/Item_ALO_Yuki.Item_ALO_Yuki_C\'",Name=NSLOCTEXT("[59528C6F356C27FC6BD1C409E0F77DB9]", "356EEDD04EC031E0F541C381F2978F38", "优纪[ALO]"),Description="",KeyWords=("优纪","Alice","绀野木棉季","刀剑神域","异能攻击","绝剑","沉睡骑士"),SkillLevel=(5,5,5,5),CharShapeHas=(True,False),Speed=800,Health=6000,Attack=800,PhyDefense=600,MagDefense=600,Critical=600,CriticalC=600))'
print('生成条目数 :', len(entries))
print('数组总长度 :', len(array_text))
print()
print('已有那一条 :', existing[:110], '...')
print('本次第一条 :', entries[0][:110], '...')
print()
print('=== 差异提示 ===')
print('已有那条的数值: SkillLevel=(5,5,5,5) Speed=800 Health=6000 Attack=800 PhyDefense=600 MagDefense=600 Critical=600 CriticalC=600')
print(f'本次沿用的数值: SkillLevel={unified["SkillLevel"]} Health={unified["Health"]} Attack={unified["Attack"]} '
      f'PhyDefense={unified["PhyDefense"]} MagDefense={unified["MagDefense"]} Critical={unified["Critical"]} '
      f'CriticalC={unified["CriticalC"]}（取自 26 个角色蓝图自身的值）')

# ---- 总表
rows = []
for c in chars:
    ch = c['char']
    rows.append((c['asset'], field(c, 'Name'), ch['Speed'], ch['Health'], ch['Attack'],
                 ch['PhyDefense'], ch['MagDefense'], ch['Critical'], ch['CriticalC']))

md = io.StringIO()
md.write('# 全角色数值总表\n\n')
md.write(f'来源：`/Game/ITems/CharItemS` 下 {len(chars)} 个角色蓝图 CDO 的 `ItemData`（`FItemInformation`）。\n\n')
md.write('## 统一数值（按用户指定：模板那组）\n\n')
md.write('| 字段 | 值 | 角色蓝图自身原来是 |\n|---|---|---|\n')
original = {'SkillLevel': '(1,1,1,1)', 'CharShapeHas': '(True,False)', 'Health': '2000', 'Attack': '50',
            'PhyDefense': '50', 'MagDefense': '50', 'Critical': '10', 'CriticalC': '10'}
for k in ['SkillLevel', 'CharShapeHas', 'Health', 'Attack', 'PhyDefense', 'MagDefense', 'Critical', 'CriticalC']:
    md.write(f'| {k} | {unified[k]} | {original[k]} |\n')
md.write('\n## 每个角色\n\n')
md.write('| 资产 | 名字 | Speed | Health | Attack | PhyDefense | MagDefense | Critical | CriticalC |\n')
md.write('|---|---|---|---|---|---|---|---|---|\n')
for r in rows:
    md.write('| ' + ' | '.join(str(x) for x in r) + ' |\n')
md.write('\n> Speed 是唯一 26 个各不相同的数值，按用户要求**保留各自的值**。\n')
open(OUT_MD, 'w', encoding='utf-8').write(md.getvalue())

print()
print('已写出:', OUT_TXT)
print('已写出:', OUT_MD)
