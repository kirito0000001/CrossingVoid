"""
对比「规则表」与「现有 AST 标签」的差异。

回答一个问题：如果现在执行「按规则表重建全部 AST 标签」，
每个标签的 ChunkId / Priority 会怎么变？

输出：Saved/PackLogs/diff_rules_vs_labels.txt
"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_saved_dir(), "PackLogs", "diff_rules_vs_labels.txt")
lines = []


def say(s=""):
    lines.append(str(s))


say("=== 规则表 vs 现有 AST 标签 · 差异对比 ===")

# ---------- 1. 读规则表 ----------
# 规则表是 UDeveloperSettings，优先用 CDO
rules_by_dir = {}   # 目录 → ChunkId
try:
    rs_cls = unreal.load_class(None, "/Script/CrossingChunk.CrossingChunkRuleSet")
    rs = unreal.get_default_object(rs_cls) if rs_cls else None
except Exception as e:
    rs = None
    say(f"load_class 异常：{e}")

if rs:
    try:
        chunks = rs.get_editor_property("chunks")
        say(f"规则表读取成功，{len(chunks)} 个分包定义")
        for c in chunks:
            name = c.get_editor_property("chunk_name")
            cid = c.get_editor_property("chunk_id")
            folders = c.get_editor_property("folders")
            say(f"  [{cid}] {name}  ({len(folders)} 个目录)")
            for f in folders:
                p = str(f.path)
                rules_by_dir[p] = cid
    except Exception as e:
        say(f"读规则表属性失败：{e}（Python 可能读不到 config 属性）")
else:
    say("取不到规则表 CDO")


def priority_for(path):
    """与 C++ ComputePriorityForFolder 一致：100 + 斜杠数"""
    p = path.rstrip("/")
    return 100 + p.count("/")


# ---------- 2. 读现有标签 ----------
ar = unreal.AssetRegistryHelpers.get_asset_registry()
f = unreal.ARFilter(
    class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "PrimaryAssetLabel")],
    recursive_classes=True,
    package_paths=["/Game"],
    recursive_paths=True,
)
labels = ar.get_assets(f)

say()
say(f"=== 现有 AST 标签：{len(labels)} 个 ===")
say()

changed = []
same = []
not_in_rules = []

for a in sorted(labels, key=lambda x: str(x.package_name)):
    pkg = str(a.package_name)
    folder = pkg.rsplit("/", 1)[0]
    obj = unreal.load_asset(pkg)
    if not obj:
        say(f"  [加载失败] {pkg}")
        continue

    r = obj.get_editor_property("rules")
    old_cid = r.get_editor_property("chunk_id")
    old_prio = r.get_editor_property("priority")

    new_cid = rules_by_dir.get(folder, None)

    if new_cid is None:
        not_in_rules.append(pkg)
        continue

    new_prio = priority_for(folder)

    if old_cid != new_cid or old_prio != new_prio:
        changed.append((pkg, old_cid, new_cid, old_prio, new_prio))
    else:
        same.append(pkg)

say(f"--- 会变化的标签：{len(changed)} 个 ---")
for pkg, oc, nc, op, np_ in changed:
    say(f"  {pkg}")
    say(f"      ChunkId {oc} → {nc}    Priority {op} → {np_}")

say()
say(f"--- 保持不变的标签：{len(same)} 个 ---")
for pkg in same:
    say(f"  {pkg}")

say()
say(f"--- 规则表里没有对应目录的标签：{len(not_in_rules)} 个 ---")
say("    （重建时会把这些标签删掉）")
for pkg in not_in_rules:
    say(f"  {pkg}")

say()
say("=== 完成 ===")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines))
print("WROTE", OUT)
