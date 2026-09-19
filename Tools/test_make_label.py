# -*- coding: utf-8 -*-
"""
可行性验证：用代码创建一个 UPrimaryAssetLabel，设好字段并保存。

先验证最小的那一环 —— 「能不能程序化建标签资产并落盘」。
跑通之后再接进右键菜单。

输出写的日志文件：Saved/PackLogs/mklabel_result.txt

用法：
  UnrealEditor-Cmd.exe CrossingVoid.uproject -run=pythonscript -script="Tools/test_make_label.py" -unattended -nop4 -nosplash -NullRHI
"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_dir(), "Saved", "PackLogs", "mklabel_result.txt")

LABEL_DIR = "/Game/BaseC/ExCordLibrary/AST"
LABEL_NAME = "AST_ZZ_TestProbe"      # 用 ZZ 前缀，方便一眼认出是测试产物
CHUNK_ID = 99
TARGET_DIR = "/Game/Movies"          # 随便找个真目录，验证「覆盖目录」怎么写

log = []


def say(s):
    log.append(str(s))


def dump():
    p = os.path.join(unreal.Paths.project_dir(), "Saved", "PackLogs", "mklabel_result.txt")
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(log) + "\n")


try:
    say("=== 1. 检查目录是否存在 ===")
    say("标签目录: {0}".format(LABEL_DIR))

    pkg_path = "{0}/{1}".format(LABEL_DIR, LABEL_NAME)
    say("目标包名: {0}".format(pkg_path))

    say("")
    say("=== 2. 创建资产（用 AssetTools + new_object 两条路都试）===")
    at = unreal.AssetToolsHelpers.get_asset_tools()

    # 先删掉上次的残留
    if unreal.EditorAssetLibrary.does_asset_exist(pkg_path):
        say("已存在，先删除")
        unreal.EditorAssetLibrary.delete_asset(pkg_path)

    new_obj = None

    # 路 A：AssetTools.create_asset，factory 传 None 看看行不行
    try:
        new_obj = at.create_asset(LABEL_NAME, LABEL_DIR,
                                  unreal.PrimaryAssetLabel, None)
        say("路 A（factory=None）返回: {0}".format(new_obj))
    except Exception as e:
        say("路 A 失败: {0}".format(e))

    # 路 B：手工建包 + new_object
    if new_obj is None:
        say("改用路 B：建包 + new_object")
        try:
            pkg = unreal.EditorAssetLibrary.make_directory(LABEL_DIR)  # 确保目录在
            asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
            new_obj = asset_tools.create_asset(
                LABEL_NAME, LABEL_DIR, unreal.PrimaryAssetLabel,
                unreal.AssetToolsHelpers.get_asset_tools().__class__ and None)
        except Exception as e:
            say("路 B 失败: {0}".format(e))

    if new_obj is None:
        # 路 C：直接找引擎里对应的 factory 类
        say("尝试路 C：遍历 factory 类名")
        try:
            cand = [n for n in dir(unreal) if "Label" in n and "Factory" in n]
            say("  候选: {0}".format(cand))
        except Exception as e:
            say("  失败: {0}".format(e))

    if new_obj is None:
        say("!! 三条路都没建成")
        dump()
        raise SystemExit(1)

    say("创建成功: {0}".format(new_obj.get_path_name()))

    say("")
    say("=== 3. 写 Rules ===")
    rules = new_obj.get_editor_property("rules")
    rules.set_editor_property("chunk_id", CHUNK_ID)
    rules.set_editor_property("priority", 0)
    rules.set_editor_property("apply_recursively", True)
    rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
    new_obj.set_editor_property("rules", rules)

    # 位域属性：Python 能不能写？试试看
    say("--- 尝试写 bLabelAssetsInMyDirectory / bIsRuntimeLabel ---")
    for prop in ["b_label_assets_in_my_directory", "b_is_runtime_label"]:
        try:
            new_obj.set_editor_property(prop, True)
            say("   {0} 写入成功".format(prop))
        except Exception as e:
            say("   {0} 写入失败: {1}".format(prop, e))

    say("")
    say("=== 4. 读回来核对 ===")
    r2 = new_obj.get_editor_property("rules")
    say("ChunkId     = {0}".format(r2.get_editor_property("chunk_id")))
    say("Priority    = {0}".format(r2.get_editor_property("priority")))
    say("Recursive   = {0}".format(r2.get_editor_property("apply_recursively")))
    say("CookRule    = {0}".format(r2.get_editor_property("cook_rule")))

    say("")
    say("=== 5. 保存 ===")
    saved = unreal.EditorAssetLibrary.save_asset(pkg_path, only_if_is_dirty=False)
    say("save_asset 返回: {0}".format(saved))

    disk = os.path.join(
        unreal.Paths.project_dir(), "Content", "BaseC", "ExCordLibrary", "AST",
        LABEL_NAME + ".uasset")
    say("磁盘文件存在: {0}  ({1} bytes)".format(
        os.path.isfile(disk),
        os.path.getsize(disk) if os.path.isfile(disk) else -1))

    say("")
    say("=== 6. 检查 AssetManagerSettings 是否认得它 ===")
    ams = unreal.get_default_object(unreal.AssetManagerSettings)
    try:
        types = ams.get_editor_property("primary_asset_types_to_scan")
        say("PrimaryAssetTypesToScan 有 {0} 项".format(len(types)))
        for t in types:
            tn = t.get_editor_property("primary_asset_type")
            if str(tn) == "PrimaryAssetLabel":
                sp = t.get_editor_property("specific_assets")
                say("  PrimaryAssetLabel 的 SpecificAssets 有 {0} 项".format(len(sp)))
                say("  含本测试标签? {0}".format(
                    any(LABEL_NAME in str(x) for x in sp)))
    except Exception as e:
        say("读 AssetManagerSettings 失败: {0}".format(e))

    say("")
    say("=== DONE ===")

except Exception as e:
    import traceback
    say("!!! 异常: {0}".format(e))
    say(traceback.format_exc())

dump()
unreal.log("mklabel result written")
