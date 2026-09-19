# -*- coding: utf-8 -*-
"""
读工程里所有 AST_* 标签（UPrimaryAssetLabel），把「目录 / ChunkId / 优先级 / 递归」
打成 CSV，供人工对照填写分包规则表。

用法（在项目根目录）：
  UnrealEditor-Cmd.exe CrossingVoid.uproject -run=pythonscript -script="Tools/dump_ast_labels.py"
"""

import unreal
import os

OUT_CSV = os.path.join(unreal.Paths.project_dir(), "Saved", "PackLogs", "ast_labels.csv")

ar = unreal.AssetRegistryHelpers.get_asset_registry()

filt = unreal.ARFilter(
    class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "PrimaryAssetLabel")],
    recursive_classes=True,
)

assets = ar.get_assets(filt)

rows = []
for a in assets:
    try:
        obj = a.get_asset()
    except Exception as e:
        unreal.log_warning("load failed: {0} ({1})".format(a.package_name, e))
        continue

    if obj is None:
        continue

    rules = obj.get_editor_property("rules")
    chunk_id = rules.get_editor_property("chunk_id")
    priority = rules.get_editor_property("priority")
    recursive = rules.get_editor_property("apply_recursively")
    cook_rule = rules.get_editor_property("cook_rule")

    # 注意：b_label_assets_in_my_directory / b_is_runtime_label 是 uint32:1 位域，
    # Python 侧 get_editor_property 访问不到（会抛 "Failed to find property"），
    # 所以不取。对本用途没影响 —— 我们关心的是 ChunkId 和 Priority。

    rows.append({
        "label": str(a.package_name),
        "dir": str(a.package_path),
        "chunk_id": int(chunk_id),
        "priority": int(priority),
        "recursive": bool(recursive),
        "cook_rule": str(cook_rule),
    })

rows.sort(key=lambda r: (r["chunk_id"], r["dir"]))

os.makedirs(os.path.dirname(OUT_CSV), exist_ok=True)
with open(OUT_CSV, "w", encoding="utf-8-sig", newline="") as f:
    f.write("目录,ChunkId,优先级,递归,CookRule,标签资产\n")
    for r in rows:
        f.write("{0},{1},{2},{3},{4},{5}\n".format(
            r["dir"], r["chunk_id"], r["priority"],
            "是" if r["recursive"] else "否",
            r["cook_rule"],
            r["label"],
        ))

unreal.log("========== AST 标签对照表（{0} 条）==========".format(len(rows)))
for r in rows:
    unreal.log("{0}  ->  ChunkId={1}  Priority={2}  递归={3}".format(
        r["dir"], r["chunk_id"], r["priority"], r["recursive"]))
unreal.log("CSV 已写到: {0}".format(OUT_CSV))
