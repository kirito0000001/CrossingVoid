"""
验证「目录扫描」是否会自动发现 AST 标签。

背景：DefaultGame.ini 里 PrimaryAssetTypesToScan(PrimaryAssetLabel) 配的是
  Directories=((Path="/Game")) + 一长串 SpecificAssets

我们要确认：Directories=/Game 是否已经足够扫到所有子目录下的标签。
若是，则新生成的标签无需登记 SpecificAssets。

输出：Saved/PackLogs/verify_label_scan.txt
"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_saved_dir(), "PackLogs", "verify_label_scan.txt")
lines = []


def say(s):
    lines.append(str(s))


say("=== AST 标签扫描验证 ===")

# 1. 所有 UPrimaryAssetLabel 的实例
ar_filter = unreal.ARFilter(
    class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "PrimaryAssetLabel")],
    recursive_classes=True,
    package_paths=["/Game"],
    recursive_paths=True,
)
all_labels = unreal.AssetRegistryHelpers.get_asset_registry().get_assets(ar_filter)
say(f"\n[1] /Game 下的 UPrimaryAssetLabel 总数：{len(all_labels)}")

# 按目录归类
from collections import Counter
dir_counter = Counter()
for a in all_labels:
    pkg = str(a.package_name)
    d = pkg.rsplit("/", 1)[0]
    dir_counter[d] += 1

say("\n[2] 按所在目录（前 50 条）：")
for d, n in sorted(dir_counter.items()):
    say(f"    {d}  ×{n}")

# 3. AssetManagerSettings 里的 PrimaryAssetLabel 扫描配置
say("\n[3] AssetManagerSettings 的 PrimaryAssetLabel 扫描项：")
try:
    ams = unreal.get_default_object(unreal.AssetManagerSettings)
    ok = True
except Exception as e:
    ams = None
    ok = False
    say(f"    取不到 AssetManagerSettings: {e}")

if ams:
    try:
        types = ams.get_editor_property("primary_asset_types_to_scan")
        for t in types:
            tname = str(t.get_editor_property("primary_asset_type"))
            if "Label" not in tname:
                continue
            dirs = t.get_editor_property("directories")
            specs = t.get_editor_property("specific_assets")
            say(f"    Type={tname}")
            say(f"      Directories   = {[str(d.path) for d in dirs]}")
            say(f"      SpecificAssets 条数 = {len(specs)}")
            for s in specs[:5]:
                say(f"        - {s}")
            if len(specs) > 5:
                say(f"        ...（省略 {len(specs)-5} 条）")
            rules = t.get_editor_property("rules")
            say(f"      Rules = Priority {rules.get_editor_property('priority')}, "
                f"ChunkId {rules.get_editor_property('chunk_id')}, "
                f"Recursive {rules.get_editor_property('apply_recursively')}")
    except Exception as e:
        say(f"    读属性失败：{e}")

# 4. 已存在的标签资产，看它们的 Rules 实际值
say("\n[4] 现有标签的 Rules 实际值（前 20 个）：")
for a in sorted(all_labels, key=lambda x: str(x.package_name))[:20]:
    name = str(a.asset_name)
    pkg = str(a.package_name)
    try:
        obj = unreal.load_asset(pkg)
        if not obj:
            say(f"    {pkg}  (加载失败)")
            continue
        rules = obj.get_editor_property("rules")
        chunk_id = rules.get_editor_property("chunk_id")
        prio = rules.get_editor_property("priority")
        try:
            in_dir = obj.get_editor_property("label_assets_in_my_directory")
        except Exception:
            in_dir = "?"
        try:
            runtime = obj.get_editor_property("is_runtime_label")
        except Exception:
            runtime = "?"
        say(f"    {pkg}")
        say(f"        ChunkId={chunk_id}  Priority={prio}  InMyDir={in_dir}  Runtime={runtime}")
    except Exception as e:
        say(f"    {pkg}  读取异常：{e}")

say("\n=== 完成 ===")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
print("WROTE", OUT)
