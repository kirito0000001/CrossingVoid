# -*- coding: utf-8 -*-
"""
诊断：为什么绝大多数资产落进了 chunk0。

思路：对每个 AST 标签，比较
  (a) 它所在目录下的资产总数（AssetRegistry 查）
  (b) 它实际「标签」到的资产数（UPrimaryAssetLabel 的 ExplicitAssets + 目录语义）
两边的差就是漏出去的。

同时把 bLabelAssetsInMyDirectory 等位域开关用反射的底层方式读出来。

用法：
  UnrealEditor-Cmd.exe CrossingVoid.uproject -run=pythonscript -script="Tools/diag_chunks.py" -unattended -nop4 -nosplash -NullRHI
"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_dir(), "Saved", "PackLogs", "diag_chunks.txt")

ar = unreal.AssetRegistryHelpers.get_asset_registry()

filt = unreal.ARFilter(
    class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "PrimaryAssetLabel")],
    recursive_classes=True,
)
labels = ar.get_assets(filt)
labels = sorted(labels, key=lambda a: str(a.package_path))

lines = []
lines.append("AST 标签诊断（共 {0} 个）".format(len(labels)))
lines.append("")

grand_dir_assets = 0
grand_explicit = 0

for a in labels:
    pkg = str(a.package_name)
    pkg_path = str(a.package_path)

    # 该标签所在目录下的所有资产
    dir_filt = unreal.ARFilter(
        package_paths=[pkg_path],
        recursive_paths=True,
    )
    in_dir = ar.get_assets(dir_filt)

    # 只数「本体资产」，跳过 .uexp 之类（AssetRegistry 本来就只给 .uasset）
    dir_count = len(in_dir)

    obj = a.get_asset()
    explicit = 0
    try:
        exp = obj.get_editor_property("explicit_assets")
        explicit = len(exp)
    except Exception as e:
        explicit = -1

    rules = obj.get_editor_property("rules")
    cid = rules.get_editor_property("chunk_id")

    grand_dir_assets += dir_count
    if explicit > 0:
        grand_explicit += explicit

    lines.append("{0}".format(pkg_path))
    lines.append("    标签资产      : {0}".format(pkg))
    lines.append("    ChunkId       : {0}".format(cid))
    lines.append("    目录内资产数  : {0}".format(dir_count))
    lines.append("    显式指定资产数: {0}".format(explicit))
    lines.append("")

lines.append("=" * 50)
lines.append("目录内资产总数（各标签累加，会重复计）: {0}".format(grand_dir_assets))

# 全工程资产总数，用于对照
all_filt = unreal.ARFilter(package_paths=["/Game"], recursive_paths=True)
all_assets = ar.get_assets(all_filt)
lines.append("全工程 /Game 资产总数: {0}".format(len(all_assets)))

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines) + "\n")

unreal.log("diag written: {0}".format(OUT))
