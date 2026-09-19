# -*- coding: utf-8 -*-
"""
只读：检查几个 PrimaryAssetLabel 在 AssetRegistry 里有没有被登记成 Primary Asset。

用途：验证「标签没被引擎主动发现」这个假设。
输出：Saved/PackLogs/check_pa.txt

用法：
  UnrealEditor-Cmd.exe CrossingVoid.uproject -run=pythonscript -script="C:/CrossingVoid/Tools/_check_primaryasset.py" -unattended -nop4 -nosplash -NullRHI
"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_saved_dir(), "PackLogs", "check_pa.txt")
lines = []
reg = unreal.AssetRegistryHelpers.get_asset_registry()

TAGS = [
    "PrimaryAssetType",
    "PrimaryAssetName",
    "ChunkId",
    "Priority",
    "AssetBundleData",
    "HasBlueprintClasses",
]


def dump(pkg_path):
    name = pkg_path.split("/")[-1]
    obj_path = "{0}.{1}".format(pkg_path, name)
    lines.append("=== " + pkg_path)
    try:
        # 注意：这个接口要的是 Name（完整对象路径的字符串），传 SoftObjectPath 会报
        # "Cannot nativize 'SoftObjectPath' as 'ObjectPath' (NameProperty)"。
        ad = reg.get_asset_by_object_path(obj_path)
    except Exception as e:
        lines.append("  get_asset_by_object_path 失败: {0}".format(e))
        return
    if not ad or not ad.is_valid():
        lines.append("  !! AssetData 无效（注册表里没有这个资产）")
        return
    for tag in TAGS:
        try:
            v = ad.get_tag_value(tag)
            v = str(v)
            if len(v) > 160:
                v = v[:160] + "…"
            lines.append("  {0:<18} = {1}".format(tag, v))
        except Exception as e:
            lines.append("  {0:<18} -> {1}".format(tag, e))


# 我们新建的标签
dump("/Game/GameActor2D/SAO_Kirito/AST_SAO_Kirito")
# 父目录的旧标签
dump("/Game/GameActor2D/AST_GameActor2D")
# 已知有资产进了 chunk2 的那个标签
dump("/Game/AssetMaterial/ImageS/Cultivate_Image/AST_YC")
# 已知有资产进了 chunk3 的那个标签
dump("/Game/AssetMaterial/ImageS/Item_Image/AST_ItemImage")

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
