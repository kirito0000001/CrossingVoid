# -*- coding: utf-8 -*-
"""探查：unreal 模块里跟 asset 创建相关的 API 有哪些。"""

import unreal
import os

OUT = os.path.join(unreal.Paths.project_dir(), "Saved", "PackLogs", "probe_api.txt")
lines = []

for kw in ["Factory", "AssetTools", "AssetLibrary", "PrimaryAssetLabel", "make_directory"]:
    lines.append("=" * 20 + " 关键词: " + kw)
    hits = sorted(n for n in dir(unreal) if kw.lower() in n.lower())
    for h in hits[:60]:
        lines.append("   " + h)
    lines.append("   共 {0} 个".format(len(hits)))
    lines.append("")

# AssetTools 的方法
lines.append("=" * 20 + " unreal.AssetTools 的方法")
at = unreal.AssetToolsHelpers.get_asset_tools()
for m in sorted(dir(at)):
    if not m.startswith("_"):
        lines.append("   " + m)
lines.append("")

# PrimaryAssetLabel 的属性
lines.append("=" * 20 + " unreal.PrimaryAssetLabel 的属性")
for m in sorted(dir(unreal.PrimaryAssetLabel)):
    if not m.startswith("_"):
        lines.append("   " + m)

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines) + "\n")

unreal.log("probe written")
