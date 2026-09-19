# -*- coding: utf-8 -*-
"""
按 Unreal 的 ini 序列化规则，生成 UCrossingChunkRuleSet 的配置段。

输入：Saved/PackLogs/ast_labels.csv（由 dump_ast_labels.py 产出）
输出：Config/DefaultCrossingChunk.ini 里的规则段（追加/替换）

FCrossingChunkDefinition 的字段：
  ChunkName, ChunkId, Folders(TArray<FDirectoryPath>), Priority,
  bIncludeInInstallPackage, ForceExcludeFolders, Comment

ini 写法（对照 DefaultGame.ini 里 PrimaryAssetTypesToScan 的既有格式）：
  +Chunks=(ChunkName="基础包",ChunkId=0,Folders=((Path="/Game/BaseC"),(Path="/Game/MapS")),Priority=0,bIncludeInInstallPackage=True)
"""

import csv
import io
import os

CSV = r"C:\CrossingVoid\Saved\PackLogs\ast_labels.csv"
OUT = r"C:\CrossingVoid\Saved\PackLogs\generated_rules_section.txt"

# 每个 ChunkId 的中文名（看目录推出来的语义）
CHUNK_NAMES = {
    0: "基础包",
    1: "第一章-基础资源",
    2: "第二章-养成与立绘",
    3: "第三章-角色与主界面",
    4: "登录资源",
    5: "材质",
    6: "音效",
}

# ChunkId 0 视为进安装包
def in_install_pkg(cid):
    return cid == 0


rows = []
with open(CSV, encoding="utf-8-sig", newline="") as f:
    for r in csv.DictReader(f):
        rows.append(r)

# 按 ChunkId 聚合目录
buckets = {}
for r in rows:
    cid = int(r["ChunkId"])
    if cid < 0:
        # 未指定 ChunkId 的标签，单独列出来让人决定
        continue
    buckets.setdefault(cid, []).append(r["目录"])

parts = []
parts.append("[/Script/CrossingChunk.CrossingChunkRuleSet]")
parts.append("")

for cid in sorted(buckets.keys()):
    dirs = sorted(set(buckets[cid]))
    folders = ",".join('(Path="{0}")'.format(d) for d in dirs)
    name = CHUNK_NAMES.get(cid, "Chunk_{0}".format(cid))
    install = "True" if in_install_pkg(cid) else "False"

    parts.append(
        '+Chunks=(ChunkName="{name}",ChunkId={cid},'
        'Folders=({folders}),Priority=0,'
        'bIncludeInInstallPackage={install})'.format(
            name=name, cid=cid, folders=folders, install=install
        )
    )

parts.append("")
parts.append("; GlobalExcludeFolders 留空 —— 需要时自己加：")
parts.append("; +GlobalExcludeFolders=(Path=\"/Game/Editor\")")

text = "\n".join(parts) + "\n"

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write(text)

print(text)
print("=" * 60)
print("未指定 ChunkId 的标签（需要你决定归属）：")
for r in rows:
    if int(r["ChunkId"]) < 0:
        print("   ", r["目录"], "  <-", r["标签资产"])
