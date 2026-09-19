"""
用 Python 模拟「按规则表重建全部 AST 标签」的逻辑，先做一次干跑（dry-run）。

目的：在真正执行 C++ 重建之前，验证逻辑对不对。
输出：Saved/PackLogs/dryrun_rebuild.txt

注意：Python 读不到 uint32:1 位域（如 bLabelAssetsInMyDirectory），
      所以这里只对比 ChunkId / Priority，不碰位域。
"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_saved_dir(), "PackLogs", "dryrun_rebuild.txt")
lines = []


def say(s=""):
    lines.append(str(s))


def norm(p):
    p = str(p).replace("\\", "/").strip()
    while len(p) > 1 and p.endswith("/"):
        p = p[:-1]
    if p and not p.startswith("/"):
        p = "/Game/" + p
    return p


def priority_for(path):
    """与 C++ ComputePriorityForFolder 完全一致"""
    return 100 + norm(path).count("/")


say("=== 干跑：按规则表重建全部 AST 标签 ===")

# ---------- 1. 读规则表 ----------
rs_cls = unreal.load_class(None, "/Script/CrossingChunk.CrossingChunkRuleSet")
rs = unreal.get_default_object(rs_cls) if rs_cls else None

desired = {}      # 目录 → (ChunkId, ChunkName)
if rs:
    for c in rs.get_editor_property("chunks"):
        name = str(c.get_editor_property("chunk_name"))
        cid = c.get_editor_property("chunk_id")
        for f in c.get_editor_property("folders"):
            desired[norm(f.path)] = (cid, name)

say(f"期望状态：{len(desired)} 个目录")
say()

# ---------- 2. 扫现有标签，按目录索引 ----------
ar = unreal.AssetRegistryHelpers.get_asset_registry()
f = unreal.ARFilter(
    class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "PrimaryAssetLabel")],
    recursive_classes=True,
    package_paths=["/Game"],
    recursive_paths=True,
)
labels = ar.get_assets(f)

existing_by_folder = {}
for a in labels:
    pkg = str(a.package_name)
    folder = pkg.rsplit("/", 1)[0]
    existing_by_folder[folder] = (pkg, str(a.asset_name))

say(f"现有标签：{len(labels)} 个，分布在 {len(existing_by_folder)} 个目录")
say()

# ---------- 3. 逐目录推演 ----------
say("--- 推演结果 ---")
say()
n_update = n_create = n_orphan = 0
problems = []

for folder in sorted(desired.keys()):
    cid, cname = desired[folder]
    new_prio = priority_for(folder)

    if folder in existing_by_folder:
        pkg, assetname = existing_by_folder[folder]
        obj = unreal.load_asset(pkg)
        old_cid = old_prio = None
        if obj:
            r = obj.get_editor_property("rules")
            old_cid = r.get_editor_property("chunk_id")
            old_prio = r.get_editor_property("priority")

        action = "更新"
        n_update += 1
        note = ""
        if old_cid != cid:
            note += f"  ← ChunkId {old_cid}→{cid}"
        if old_prio != new_prio:
            note += f"  ← Priority {old_prio}→{new_prio}"

        say(f"[{action}] {folder}")
        say(f"        复用已有标签 {pkg}（名字 {assetname}）")
        say(f"        ChunkId {cid} ({cname})  Priority {new_prio}{note}")

        # 名字不标准 → 提示
        if not assetname.startswith("AST_"):
            say(f"        ⚠ 标签名不符合 AST_ 前缀，但会被复用（不新建）")
    else:
        say(f"[新建] {folder}")
        say(f"        ChunkId {cid} ({cname})  Priority {new_prio}")
        n_create += 1
    say()

# ---------- 4. 找出会被删除的（规则表里没有的目录） ----------
say("--- 规则表里没有对应目录的标签（重建时会删除）---")
for folder, (pkg, name) in sorted(existing_by_folder.items()):
    if folder not in desired:
        say(f"  [删除] {pkg}")
        n_orphan += 1
if n_orphan == 0:
    say("  （无）")

say()
say("=== 汇总 ===")
say(f"  更新：{n_update}")
say(f"  新建：{n_create}")
say(f"  删除：{n_orphan}")
say(f"  期望目录总数：{len(desired)}")
say(f"  现有标签总数：{len(labels)}")

# ---------- 5. 一致性检查 ----------
say()
say("=== 一致性检查 ===")
# 检查是否有目录存在多个标签（会导致冲突）
from collections import Counter
cnt = Counter(pkg.rsplit("/", 1)[0] for pkg in existing_by_folder)
dupes = {d: c for d, c in cnt.items() if c > 1}
if dupes:
    say("  ⚠ 同目录多标签：")
    for d, c in dupes.items():
        say(f"      {d} ×{c}")
else:
    say("  同目录多标签：无")

# 检查 Priority 冲突（父子同 Priority）
prios = {}
conflict = []
for folder in desired:
    p = priority_for(folder)
    for other in desired:
        if other == folder:
            continue
        if priority_for(other) == p and (other.startswith(folder + "/") or folder.startswith(other + "/")):
            conflict.append((folder, other, p))
if conflict:
    say("  ⚠ Priority 冲突（父子同值）：")
    for a, b, p in conflict[:10]:
        say(f"      {a}  vs  {b}  (都是 {p})")
else:
    say("  Priority 冲突：无")

say()
say("=== 完成 ===")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines))
print("WROTE", OUT)
