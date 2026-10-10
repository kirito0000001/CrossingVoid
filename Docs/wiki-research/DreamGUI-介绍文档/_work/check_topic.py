#!/usr/bin/env python3
"""DreamGUI 介绍文档 · 专题体检（固化版）

用法：
    python check_topic.py --root "C:/CrossingVoid/Docs/wiki-research/DreamGUI-介绍文档" \
                          --base "D:/UnrealEngine-5.8.2/Engine/Plugins/Marketplace/DreamGUI"

三项检查：
  ① 角标可回指    — 抽 04_草稿.md 里所有 〔A/B/C-nn〕，按前缀分流到三本账本，逐条校验条目存在
  ② 出处块可解析  — 抽所有 HTML 注释里 `-> 路径:行` 的路径，校验文件存在 + 行号不越界
  ③ 章节体量      — 发布版每章的行数与 H2 数，对照硬线 ≤250 行 / ≤12 个 H2

退出码：0 = 全过；1 = 有问题（摘要打印到 stderr）

⚠️ 相比最初临时写的那版，这里把出处块的扩展名从 (h/cpp/md/json/cs/ini)
   扩到了脚本与配置（py/ps1/csv/txt/ush/usf/uplugin/dss/dsc）——
   因为「工具链」那一章的材料大量是 .py / .ps1，原先的正则会**静默漏掉**它们。
"""
import argparse
import pathlib
import re
import sys

EXTS = ("h", "cpp", "md", "json", "cs", "ini",
        "py", "ps1", "csv", "txt", "ush", "usf", "uplugin", "dss", "dsc")
PATH_RE = re.compile(r"->\s*([^\s:]+\.(?:" + "|".join(EXTS) + r"))(?::(\d+)(?:-(\d+))?)?")

# 角标前缀 -> (账本相对路径, 该账本内部编号前缀)
LEDGERS = {
    "A": ("../DreamGUI-明细/01_素材账本.md", "A"),
    "B": ("../DreamGUI-采集-控件模型/01_素材账本.md", "A"),
    "C": ("../DreamGUI/01_素材账本.md", "A"),
}
TAG_RE = re.compile(r"〔([ABC]-\d{2})〕")

# 已知的超线例外（用户已认可、不返工）：章节名里含这些关键字的，超线只提示、不算失败
KNOWN_OVER = ("控件基类",)   # §1：223 行 / 14 H2 —— 写在阈值定案之前，用户已看过并认可


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="专题目录（含 04_草稿.md / 06_发布版.md）")
    ap.add_argument("--base", required=True, help="被采插件仓库根")
    args = ap.parse_args()

    root = pathlib.Path(args.root)
    base = pathlib.Path(args.base)
    draft = (root / "04_草稿.md").read_text(encoding="utf-8")
    pub = (root / "06_发布版.md").read_text(encoding="utf-8")
    problems = []

    # ---------- ① 角标可回指 ----------
    tags = TAG_RE.findall(draft)
    uniq = sorted(set(tags))

    def has(rel, entry):
        p = (root / rel).resolve()
        if not p.exists():
            return False
        return re.search(r"^- \*\*" + re.escape(entry) + r"\*\*｜",
                         p.read_text(encoding="utf-8"), re.M) is not None

    dangling = [t for t in uniq if not has(LEDGERS[t[0]][0], LEDGERS[t[0]][1] + t[1:])]
    print(f"① 角标：{len(tags)} 处 / 去重 {len(uniq)} / 悬空 {len(dangling)}")
    if dangling:
        problems.append(f"角标悬空：{dangling}")

    # ---------- ② 出处块可解析 ----------
    total, bad = 0, []
    for block in re.findall(r"<!--(.*?)-->", draft, re.S):
        for m in PATH_RE.finditer(block):
            rel, start, end = m.group(1), m.group(2), m.group(3)
            total += 1
            p = base / rel
            if not p.exists():
                hits = list(base.rglob(pathlib.Path(rel).name))
                if hits:
                    p = hits[0]
                else:
                    bad.append(f"{rel} 不存在")
                    continue
            if start:
                n = len(p.read_text(encoding="utf-8", errors="ignore").split("\n"))
                if int(end or start) > n:
                    bad.append(f"{rel}:{end or start} 越界（文件 {n} 行）")
    print(f"② 出处块：{total} 条 / 有问题 {len(bad)}")
    for b in bad[:10]:
        print("     -", b)
    if bad:
        problems.append(f"出处问题 {len(bad)} 条")

    # ---------- ③ 章节体量 ----------
    lines = pub.split("\n")
    secs, cur = [], None
    for i, l in enumerate(lines):
        if l.startswith("# "):
            if cur:
                cur["end"] = i
                secs.append(cur)
            cur = {"name": l.strip(), "start": i}
    if cur:
        cur["end"] = len(lines)
        secs.append(cur)

    over = []
    print("③ 章节体量（阈值 ≤250 行 / ≤12 H2）：")
    for s in secs:
        body = lines[s["start"]:s["end"]]
        r, h = s["end"] - s["start"], sum(1 for x in body if x.startswith("## "))
        flag = ""
        if s["start"] > 0 and (r > 250 or h > 12):   # 跳过文档大标题那一节
            if any(k in s["name"] for k in KNOWN_OVER):
                flag = "⚠️ 超线（**已知例外**，用户已认可，不返工）"
            else:
                flag = "⚠️ 超线"
                over.append(f"{s['name'][:36]}（{r} 行 / {h} H2）")
        print(f"   {r:>4} 行 | {h:>2} H2 | {s['name'][:46]} {flag}")
    if over:
        problems.append("超线章节：" + "；".join(over))

    print()
    if problems:
        print("❌ 未通过：", file=sys.stderr)
        for p in problems:
            print("   -", p, file=sys.stderr)
        return 1
    print("✅ 三项全过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
