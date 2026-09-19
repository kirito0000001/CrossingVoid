# -*- coding: utf-8 -*-
"""
只读：把工程里所有 PrimaryAssetLabel 的 Rules 打出来，重点看 SAO_Kirito。

用途：分辨「标签字段没写对」和「AssetRegistry 缓存是旧的」这两种情况。
输出：Saved/PackLogs/read_label_result.txt

用法：
  UnrealEditor-Cmd.exe CrossingVoid.uproject -run=pythonscript -script="Tools/_read_label.py" -unattended -nop4 -nosplash -NullRHI
"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_saved_dir(), "PackLogs", "read_label_result.txt")
lines = []


def say(s=""):
    lines.append(str(s))


registry = unreal.AssetRegistryHelpers.get_asset_registry()

class_path = unreal.TopLevelAssetPath("/Script/Engine", "PrimaryAssetLabel")
flt = unreal.ARFilter(class_paths=[class_path], recursive_paths=True)
assets = registry.get_assets(flt)

say("PrimaryAssetLabel 总数: {0}".format(len(assets)))
say("")
say("ChunkId | Priority | Recursive | 资产")
say("-" * 70)

found_target = False
chunk_ids = {}

for data in assets:
    try:
        label = data.get_asset()
    except Exception as e:
        say("!! get_asset 失败: {0} -> {1}".format(data.package_name, e))
        continue
    if label is None:
        continue

    try:
        rules = label.get_editor_property("rules")
    except Exception as e:
        say("!! 读 Rules 失败: {0} -> {1}".format(data.package_name, e))
        continue

    def prop(obj, name, default="?"):
        try:
            return obj.get_editor_property(name)
        except Exception:
            return default

    cid = prop(rules, "chunk_id")
    pri = prop(rules, "priority")
    rec = prop(rules, "b_apply_recursively")
    if rec == "?":
        rec = prop(rules, "bApplyRecursively")

    # 标签自己那两个位域（Python 侧可能读不到，读到就报出来）
    in_dir = prop(label, "b_label_assets_in_my_directory")
    if in_dir == "?":
        in_dir = prop(label, "bLabelAssetsInMyDirectory")
    is_runtime = prop(label, "b_is_runtime_label")
    if is_runtime == "?":
        is_runtime = prop(label, "bIsRuntimeLabel")

    pkg = str(data.package_name)
    chunk_ids[cid] = chunk_ids.get(cid, 0) + 1

    if "SAO_Kirito" in pkg or "GameActor2D" in pkg:
        found_target = True
        say("{0:>7} | {1:>8} | {2:>9} | {3} | 目录位域={4} 运行期={5}".format(
            cid, pri, rec, pkg, in_dir, is_runtime))

say("")
say("=== SAO_Kirito 标签单独看 ===")

sa = "/Game/GameActor2D/SAO_Kirito/AST_SAO_Kirito"
if unreal.EditorAssetLibrary.does_asset_exist(sa):
    lbl = unreal.EditorAssetLibrary.load_asset(sa)
    r = lbl.get_editor_property("rules")
    say("chunk_id       = {0}".format(prop(r, "chunk_id")))
    say("priority       = {0}".format(prop(r, "priority")))
    say("recursive      = {0}".format(prop(r, "b_apply_recursively")))
    say("cook_rule      = {0}".format(prop(r, "cook_rule")))
    say("in_my_dir      = {0}".format(prop(lbl, "b_label_assets_in_my_directory")))
    say("is_runtime     = {0}".format(prop(lbl, "b_is_runtime_label")))
    say("explicit_count = {0}".format(len(prop(lbl, "explicit_assets", []))))
else:
    say("!! 资产不存在: {0}".format(sa))

say("")
say("=== bundle 对比（这才是「标签管了哪些资产」的真身）===")

def bundle_summary(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return "资产不存在"
    a = unreal.EditorAssetLibrary.load_asset(path)
    try:
        bd = a.get_editor_property("asset_bundle_data")
    except Exception as e:
        return "读 asset_bundle_data 失败: {0}".format(e)
    out = []
    for attr in ("bundles", "bundle_assets", "bundles_data"):
        try:
            v = bd.get_editor_property(attr)
            names = []
            total = 0
            for e in v:
                try:
                    bn = e.get_editor_property("bundle_name")
                except Exception:
                    bn = "?"
                try:
                    arr = e.get_editor_property("asset_paths")
                    n = len(arr)
                except Exception:
                    n = -1
                names.append("{0}:{1}".format(bn, n))
                total += max(n, 0)
            out.append("{0} -> {1}（合计 {2}）".format(attr, ", ".join(names) or "空", total))
        except Exception:
            continue
    return " | ".join(out) or "三个属性名都读不到"

say("新标签 AST_SAO_Kirito   : {0}".format(bundle_summary(sa)))
say("旧标签 AST_GameActor2D  : {0}".format(bundle_summary("/Game/GameActor2D/AST_GameActor2D")))
say("旧标签 AST_CharsImage   : {0}".format(bundle_summary("/Game/AssetMaterial/ImageS/CharaterS/AST_CharsImage")))

say("")
say("=== 全工程 ChunkId 分布 ===")
for cid in sorted(chunk_ids.keys()):
    say("  ChunkId {0:>4} : {1} 个标签".format(cid, chunk_ids[cid]))

say("")
say("找到目标目录的标签: {0}".format(found_target))

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
