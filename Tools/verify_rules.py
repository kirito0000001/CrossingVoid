# -*- coding: utf-8 -*-
"""
验证 DefaultCrossingChunk.ini 能否被引擎正确解析为 UCrossingChunkRuleSet。
只读，不写任何东西。

用法：
  UnrealEditor-Cmd.exe CrossingVoid.uproject -run=pythonscript -script="Tools/verify_rules.py" -unattended -nop4 -nosplash -NullRHI
"""

import unreal
import os

CDO = unreal.get_default_object(unreal.CrossingChunkRuleSet)
chunks = CDO.get_editor_property("chunks")

lines = []
lines.append("规则表解析结果：{0} 个分包".format(len(chunks)))

total_dirs = 0
for c in chunks:
    cid = c.get_editor_property("chunk_id")
    name = c.get_editor_property("chunk_name")
    folders = c.get_editor_property("folders")
    dirs = [f.path for f in folders]
    total_dirs += len(dirs)

    lines.append("")
    lines.append("Chunk {0}  [{1}]  目录 {2} 个".format(cid, name, len(dirs)))
    for d in dirs:
        lines.append("      {0}".format(d))

lines.append("")
lines.append("合计目录数：{0}".format(total_dirs))

out = os.path.join(unreal.Paths.project_dir(), "Saved", "PackLogs", "verify_rules_result.txt")
with open(out, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines) + "\n")

unreal.log("verify written: {0}".format(out))
