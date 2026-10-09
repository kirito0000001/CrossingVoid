import re, io, sys, json

path = r'C:\CrossingVoid\_work\bpvar\out\values.log'
lines = open(path, encoding='utf-8').read().splitlines()

chars = []
cur = None
section = None
for line in lines:
    m = re.match(r'^=== (.+) ===$', line)
    if m:
        cur = {'asset': m.group(1), 'item': {}, 'char': {}, 'class': ''}
        chars.append(cur)
        section = None
        continue
    if cur is None:
        continue
    m = re.match(r'^Class = (.+)$', line)
    if m:
        cur['class'] = m.group(1).strip()
        continue
    m = re.match(r'^  \[(\w+)\] (\w+)$', line)
    if m:
        section = m.group(2)
        continue
    m = re.match(r'^  (\w+)\s+= (.*)$', line)
    if m and section is None:
        cur['item'][m.group(1)] = m.group(2)
        continue
    m = re.match(r'^    (\w+)\s+= (.*)$', line)
    if m and section == 'CharData':
        cur['char'][m.group(1)] = m.group(2)

print(f'角色数: {len(chars)}')
print()
hdr = ['asset', 'Name', 'SkillLevel', 'CharShapeHas', 'Speed', 'Health', 'Attack', 'PhyDef', 'MagDef', 'Crit', 'CritC', 'Synch']
print(' | '.join(hdr))
print('-' * 130)
for c in chars:
    ch = c['char']
    row = [c['asset'], c['item'].get('Name', '?'), ch.get('SkillLevel', '?'), ch.get('CharShapeHas', '?'),
           ch.get('Speed', '?'), ch.get('Health', '?'), ch.get('Attack', '?'), ch.get('PhyDefense', '?'),
           ch.get('MagDefense', '?'), ch.get('Critical', '?'), ch.get('CriticalC', '?'), ch.get('Synchronize', '?')]
    print(' | '.join(row))

print()
print('=== 数值分布 ===')
from collections import Counter
for key in ['Speed', 'Health', 'Attack', 'PhyDefense', 'MagDefense', 'Critical', 'CriticalC', 'Synchronize', 'SkillLevel', 'CharShapeHas']:
    vals = Counter(c['char'].get(key, '<missing>') for c in chars)
    print(f'{key:<14} {dict(vals)}')

json.dump(chars, open(r'C:\CrossingVoid\_work\bpvar\out\chars.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print()
print('已写出 chars.json')
