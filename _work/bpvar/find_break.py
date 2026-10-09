# -*- coding: utf-8 -*-
"""找出数组文本在 uasset 里是从哪一段开始被打断的。"""
path = r'C:\CrossingVoid\Content\BaseC\Mode\GM_Main.uasset'
data = open(path, 'rb').read()
expect = open(r'C:\CrossingVoid\Saved\ZDBpVar_ALLChar.txt', encoding='utf-8').read()

def found(s):
    return data.count(s.encode('utf-16-le'))

# 用二分找最长可匹配前缀
lo, hi = 0, len(expect)
while lo < hi:
    mid = (lo + hi + 1) // 2
    if found(expect[:mid]):
        lo = mid
    else:
        hi = mid - 1
print(f'最长可匹配前缀长度: {lo} / {len(expect)}')
if lo < len(expect):
    print()
    print('匹配到的最后 120 字符:')
    print('  ...' + expect[max(0, lo-120):lo])
    print()
    print('断点处往后 120 字符（这段在文件里找不到连续形式）:')
    print('  ' + expect[lo:lo+120])
    # 断点后面的片段单独找找
    for probe in [expect[lo:lo+40], expect[lo+1:lo+41], expect[lo:lo+20]]:
        print(f'  片段 {probe[:24]!r}... 出现 {found(probe)} 次')

# 用后缀也验一下
lo2, hi2 = 0, len(expect)
while lo2 < hi2:
    mid = (lo2 + hi2 + 1) // 2
    if found(expect[len(expect)-mid:]):
        lo2 = mid
    else:
        hi2 = mid - 1
print(f'\n最长可匹配后缀长度: {lo2} / {len(expect)}')
