#!/usr/bin/env node
// DreamGUI 功能全景专题 · 自检脚本（wiki-collect 技术专题自带检查器）
// 用法：node check.mjs [--topic <专题目录>] [--base <被采仓库根>]
// 输出契约：首行 selftest，末行 found N (major=… advisory=… info=…)；有 major 即退出码 1。
// 判据分级：structural_invariant 走 major；语义义务走 advisory；口径提示走 info。
import fs from "node:fs";
import path from "node:path";

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 && argv[i + 1] ? argv[i + 1] : d; };
const TOPIC = path.resolve(arg("--topic", "."));
const BASE = path.resolve(arg("--base", "D:/UnrealEngine-5.8.2/Engine/Plugins/Marketplace/DreamGUI"));
const LEDGER = path.join(TOPIC, "01_素材账本.md");

const findings = [];
const add = (level, id, file, line, msg, evidence) =>
  findings.push({ level, id, file, line, msg, evidence });
const major = (...a) => add("major", ...a);
const adv = (...a) => add("advisory", ...a);
const info = (...a) => add("info", ...a);

// ── 闭集（写在脚本一处，供机检） ───────────────────────────────────────────
const STATUS = ["一手已核", "自测", "二手线索", "待复审", "已推翻"];
const FIELDS = ["状态", "断言", "定位", "引用路径", "证据", "拿到", "核对", "钉死",
  "复审周期", "失效条件", "检查方式", "影响", "取代", "被取代", "矛盾", "变更"];
const REQUIRE_A = ["状态", "断言", "定位", "引用路径", "证据"];
const REQUIRE_B = ["状态", "断言", "定位", "影响"];

// ── selftest：正/负样本（负样本照真实误报造） ────────────────────────────
function selftest() {
  let n = 0, f = 0;
  const ok = (c, what) => { n++; if (!c) throw new Error("selftest 断言失败: " + what); };
  const idRe = /^([AB])-(\d{2})$/;
  // 夹具 1：两位编号（正）
  ok(idRe.test("A-01") && idRe.test("B-04"), "两位编号应通过");
  f++;
  // 夹具 2：三位编号（负 —— 实测曾有 A-100，检查器静默丢弃）
  ok(!idRe.test("A-100"), "三位编号必须不通过");
  f++;
  // 夹具 3：状态闭集（正）
  ok(STATUS.includes("一手已核"), "一手已核 应在闭集内");
  ok(!STATUS.includes("已核实"), "自造状态必须不在闭集内");
  f++;
  // 夹具 4：单行多字段切分（正/负）—— 断言里含全角竖线不得切断字段
  const cut = (s) => fieldSpans(s).map((x) => x.name);
  ok(cut("- **A-01**｜状态：一手已核｜断言：a ｜ b ｜ c").join(",") === "状态,断言",
    "断言内的 ｜ 不得被当成字段边界");
  ok(cut("  - 证据：x ｜y").join(",") === "证据", "无字段前缀的续段不得被当成字段");
  f++;
  // 夹具 5：裸文件名（负 —— 实测代价 20 条 advisory）
  ok(!hasDir("Config.h"), "裸文件名应被判为无目录");
  ok(hasDir("Source/Config.h"), "带目录路径应通过");
  f++;
  return { n, f };
}

function hasDir(p) { return /[\\/]/.test(p) || /^https?:\/\//.test(p); }

// 在一条记录里定位所有「已知字段名：」的区间
function fieldSpans(text) {
  const hits = [];
  for (const name of FIELDS) {
    const needle = name + "：";
    let from = 0;
    for (;;) {
      const i = text.indexOf(needle, from);
      if (i < 0) break;
      hits.push({ name, start: i, vStart: i + needle.length });
      from = i + needle.length;
    }
  }
  hits.sort((a, b) => a.start - b.start);
  for (let i = 0; i < hits.length; i++) hits[i].end = i + 1 < hits.length ? hits[i + 1].start : text.length;
  // 值必须在**下一条项目符号**处收口，否则后一条以「- 」开头的行会被吃进前一个字段（实测：矛盾 把后面三条都吞了）
  for (const h of hits) {
    const nl = text.slice(h.vStart).search(/\n[ \t]*[-*][ \t]/);
    if (nl >= 0) h.end = Math.min(h.end, h.vStart + nl);
  }
  // 值尾部会带上字段分隔符与下一行的条目符号，必须剥掉（否则「一手已核｜」会被判成不在闭集）
  return hits.map((h) => ({
    name: h.name,
    value: text.slice(h.vStart, h.end).replace(/\s+$/, "").replace(/[-｜]+\s*$/, "").trim(),
  }));
}

// ── 主流程 ────────────────────────────────────────────────────────────────
const st = selftest();
console.log(`selftest: OK（${st.n} 项断言，${st.f} 个夹具）`);

if (!fs.existsSync(LEDGER)) { major("LEDGER-MISSING", "01_素材账本.md", 1, "账本不存在", "path=" + LEDGER); }
else {
  const raw = fs.readFileSync(LEDGER, "utf8");
  const lines = raw.split(/\r?\n/);

  // 源表登记
  const tables = {};
  // 位置列允许在反引号路径后追加尾注（如「（**外部活源**）」），否则带尾注的源表会被漏登
  const tblRe = /^\|\s*([A-Za-z_]+)\s*\|\s*`([^`]+)`[^|]*\|\s*([^|]*)\|$/;
  lines.forEach((ln, i) => {
    const m = tblRe.exec(ln.trim());
    if (m && m[1] !== "源表名") tables[m[1]] = { loc: m[2], line: i + 1 };
  });
  if (Object.keys(tables).length === 0) major("SRC-TABLE-EMPTY", "01_素材账本.md", 1, "源表登记为空", "至少登记 1 张源表");

  // 切条目
  const entries = [];
  let cur = null;
  lines.forEach((ln, i) => {
    const h = /^- \*\*([AB]-\d+)\*\*｜(.*)$/.exec(ln);
    if (h) { cur = { id: h[1], line: i + 1, text: h[2] }; entries.push(cur); return; }
    if (cur && /^\s*-\s/.test(ln) && !/^\s*-\s*\*\*/.test(ln)) cur.text += "\n" + ln;
    else if (/^(#|####|\|)/.test(ln)) cur = null;
  });

  const seen = new Map();
  const byId = new Map();
  for (const e of entries) {
    const m = /^([AB])-(\d{2})$/.exec(e.id);
    if (!m) { major("ID-FORM", "01_素材账本.md", e.line, `编号 ${e.id} 不合法（只许两位 A-00…A-99 / B-00…B-99）`, "见素材账本字段.md §3 编号"); continue; }
    if (seen.has(e.id)) major("ID-DUP", "01_素材账本.md", e.line, `编号 ${e.id} 重复（首次在 ${seen.get(e.id)} 行）`, "编号不得回收/重用");
    seen.set(e.id, e.line);
    byId.set(e.id, e);

    const f = Object.fromEntries(fieldSpans(e.text).map((x) => [x.name, x.value]));
    const need = e.id.startsWith("A") ? REQUIRE_A : REQUIRE_B;
    for (const k of need) {
      if (!f[k] || !f[k].length) major("FIELD-MISSING", "01_素材账本.md", e.line, `${e.id} 缺必填字段「${k}」`, `必填集：${need.join(" / ")}`);
    }
    if (f["状态"] && !STATUS.includes(f["状态"])) {
      major("STATUS-CLOSED", "01_素材账本.md", e.line, `${e.id} 状态「${f["状态"]}」不在闭集内`, "闭集：" + STATUS.join(" / "));
    }
    if (e.id.startsWith("A") && f["影响"]) {
      major("A-NO-IMPACT", "01_素材账本.md", e.line, `${e.id} 是 A 级却带「影响」字段`, "影响 是 B 级专属（COL-R09）");
    }
    // 定位：判据是「能不能解析到真实文件」，不是「有没有目录」（仓库根下的 README 之类本就无目录）
    if (f["定位"]) {
      for (const piece of f["定位"].split("；")) {
        let p = piece.replace(/（[^）]*）/g, "").replace(/[（）()]+$/g, "").replace(/`/g, "").trim();
        if (!p) continue;
        if (/^https?:\/\//.test(p)) continue;
        p = p.replace(/:\d+(-\d+)?$/, "").replace(/[\\/]+$/, "");
        if (!/\.[A-Za-z0-9]+$/.test(p) && !/[\\/]/.test(p)) continue; // 符号名/配置键，跳过
        const abs = p.startsWith("_src/") ? path.join(TOPIC, p) : path.join(BASE, p);
        if (!fs.existsSync(abs)) adv("LOC-UNRESOLVED", "01_素材账本.md", e.line, `${e.id} 定位「${p}」按仓库根解析不到真实文件`, "COL-R12：定位要到位，不能只到文件名");
      }
    }
    // 引用路径 → 源表必须已登记 + 可达 + 主键字面命中
    if (f["引用路径"]) {
      for (const piece of f["引用路径"].split("；")) {
        const m2 = /^\s*([A-Za-z_]+)\[([^\]]*)\]/.exec(piece.replace(/`/g, "").trim());
        if (!m2) { adv("REFPATH-FORM", "01_素材账本.md", e.line, `${e.id} 引用路径「${piece.trim()}」不是三段式 源表[主键]`, "见素材账本字段.md §3"); continue; }
        const [, tname, key] = m2;
        if (!tables[tname]) { major("REFPATH-DANGLING", "01_素材账本.md", e.line, `${e.id} 引用路径的源表「${tname}」未在 §0 登记`, "源表名必须唯一解析到真实位置（COL-S02）"); continue; }
        if (/^https?:\/\//.test(tables[tname].loc)) continue;
        const loc = tables[tname].loc;
        const abs = loc.startsWith("_src/") ? path.join(TOPIC, loc) : path.join(BASE, loc);
        if (!fs.existsSync(abs)) { adv("SRCTABLE-MISSING", "01_素材账本.md", tables[tname].line, `源表 ${tname} 的位置不存在：${loc}`, "未入库文件只报 advisory（素材账本字段.md §2.1）"); continue; }
        if (loc.endsWith("/")) {
          const kids = fs.readdirSync(abs).map((x) => x.replace(/\.[^.]+$/, ""));
          if (!kids.includes(key)) major("SRCTABLE-KEY", "01_素材账本.md", e.line, `${e.id} 源表 ${tname} 的目录里没有「${key}」`, `子项样例：${kids.slice(0, 4).join(", ")}`);
        } else {
          const body = fs.readFileSync(abs, "utf8");
          if (!body.includes(key)) major("SRCTABLE-KEY", "01_素材账本.md", e.line, `${e.id} 源表 ${tname} 的文件里字面命中不到「${key}」`, "主键必须能字面命中（素材账本字段.md §2.0）");
        }
      }
    }
  }

  // 双轨：矛盾 字段 ↔ 未决 小节
  const ji = raw.indexOf("\n#### 未决");
  const judge = ji >= 0 ? raw.slice(ji) : "";
  const inJudge = new Set((judge.match(/[AB]-\d{2}/g) || []));
  for (const e of entries) {
    const f = Object.fromEntries(fieldSpans(e.text).map((x) => [x.name, x.value]));
    // 矛盾 的值是条目编号；按编号形状抽，不按逗号切（正文里的逗号会切出垃圾）
    for (const t of (f["矛盾"] || "").match(/[AB]-\d{2}/g) || []) {
      if (!byId.has(t)) major("CONTRA-TARGET", "01_素材账本.md", e.line, `${e.id} 的「矛盾」指向 ${t}，该条目不存在`, "矛盾对方必须是真的条目");
      if (!inJudge.has(t) || !inJudge.has(e.id)) adv("CONTRA-UNDECLARED", "01_素材账本.md", e.line, `${e.id} ↔ ${t} 有 矛盾 字段但未进入未决小节`, "§5 双轨不可互替");
    }
    if (inJudge.has(e.id) && !f["矛盾"]) major("JUDGE-NO-FIELD", "01_素材账本.md", e.line, `${e.id} 出现在未决小节却没有「矛盾」字段`, "只有小节没有字段＝失败（COL-S06）");
  }
  if (!judge) info("JUDGE-ABSENT", "01_素材账本.md", 1, "没有未决小节", "本批有 2 项未决，应有小节");

  // ── 数值不变量：文档声明的数值 ↔ 被采仓库实际值 ──────────────────────
  const walk = (dir, pred, out = []) => {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      const p = path.join(dir, e.name);
      if (e.isDirectory()) { if (!/^(Intermediate|Binaries|Saved|\.git)$/.test(e.name)) walk(p, pred, out); }
      else if (pred(p)) out.push(p);
    }
    return out;
  };
  // 行数口径 = `wc -l`（数 \n 出现次数）。不用 split 长度——无尾换行的文件会多算 1 行。
  const countLines = (files) => files.reduce((a, p) => a + (fs.readFileSync(p, "utf8").match(/\n/g) || []).length, 0);
  const num = (s) => Number(String(s).replace(/,/g, ""));

  const upluginPath = path.join(BASE, "DreamGUI.uplugin");
  let up = null;
  if (fs.existsSync(upluginPath)) up = JSON.parse(fs.readFileSync(upluginPath, "utf8").replace(/^\uFEFF/, ""));

  // 1) 模块数 / 版本 / 引擎
  const decl = {};
  decl.modules = num((/\*\*(\d+) 个模块\*\*/.exec(raw) || [])[1]);
  decl.version = (/版本号 `([\d.]+)`/.exec(raw) || [])[1];
  decl.engine = (/引擎要求 Unreal Engine `([\d.]+)`/.exec(raw) || [])[1];
  // 未声明 ≠ 不一致：声明值缺失时只报 advisory，不报 major（否则没抄这套数字的专题会被误判）
  if (up) {
    const miss = [];
    if (!Number.isFinite(decl.modules)) miss.push("模块数");
    else if (decl.modules !== up.Modules.length) major("NUM-MODULES", "01_素材账本.md", 1, `账本声明模块数 ${decl.modules}，.uplugin 实际 ${up.Modules.length}`, "数值不变量：模块数");
    if (!Number.isFinite(decl.version) || !decl.version) miss.push("版本号");
    else if (decl.version !== up.VersionName) major("NUM-VERSION", "01_素材账本.md", 1, `账本声明版本 ${decl.version}，.uplugin 实际 ${up.VersionName}`, "数值不变量：版本号");
    if (!Number.isFinite(decl.engine) || !decl.engine) miss.push("引擎版本");
    else if (decl.engine !== up.EngineVersion) major("NUM-ENGINE", "01_素材账本.md", 1, `账本声明引擎 ${decl.engine}，.uplugin 实际 ${up.EngineVersion}`, "数值不变量：引擎版本");
    if (miss.length) adv("NUM-DECL-MISSING", "01_素材账本.md", 1, `账本未声明：${miss.join(" / ")}，无法对账`, "数值不变量缺少被比较的一方");
  }

  // 2) Source 规模
  const srcDir = path.join(BASE, "Source");
  if (fs.existsSync(srcDir)) {
    const cpp = walk(srcDir, (p) => p.endsWith(".cpp"));
    const hdr = walk(srcDir, (p) => p.endsWith(".h"));
    const cppL = countLines(cpp), hdrL = countLines(hdr);
    const m = /\*\*([\d,]+) 文件 \/ ([\d,]+) 行\*\*/.exec(raw);
    if (m) {
      const dF = num(m[1]), dL = num(m[2]);
      const aF = cpp.length + hdr.length, aL = cppL + hdrL;
      if (dF !== aF) major("NUM-SRCFILES", "01_素材账本.md", 1, `账本声明 Source 文件数 ${dF}，实测 ${aF}`, "数值不变量：Source 文件数（口径：.cpp+.h）");
      if (dL !== aL) major("NUM-SRCLINES", "01_素材账本.md", 1, `账本声明 Source 行数 ${dL}，实测 ${aL}`, `数值不变量：Source 行数；实测 cpp=${cppL} h=${hdrL}`);
    } else adv("NUM-SRC-UNPARSED", "01_素材账本.md", 1, "账本里找不到 Source 规模声明，无法对账", "期望形如 **N 文件 / M 行**");
  }

  // 3) 测试定义数
  if (fs.existsSync(srcDir)) {
    const srcFiles = walk(srcDir, (p) => /\.(cpp|h)$/.test(p));
    let simple = 0, complex = 0, tags = 0;
    for (const p of srcFiles) {
      const b = fs.readFileSync(p, "utf8");
      simple += (b.match(/IMPLEMENT_SIMPLE_AUTOMATION_TEST\(/g) || []).length;
      complex += (b.match(/IMPLEMENT_COMPLEX_AUTOMATION_TEST\(/g) || []).length;
      tags += (b.match(/REGISTER_SIMPLE_AUTOMATION_TEST_TAGS\(/g) || []).length;
    }
    const dm = /测试定义共 \*\*([\d,]+) 个\*\*/.exec(raw);
    if (dm && num(dm[1]) !== simple + complex) major("NUM-TESTS", "01_素材账本.md", 1, `账本声明测试 ${dm[1]} 个，实测 ${simple + complex}（simple=${simple} complex=${complex}）`, "数值不变量：测试定义数");
    const dt = /另有 ([\d,]+) 处 `REGISTER_SIMPLE_AUTOMATION_TEST_TAGS`/.exec(raw);
    if (dt && num(dt[1]) !== tags) major("NUM-TAGTESTS", "01_素材账本.md", 1, `账本声明打 tag 数 ${dt[1]}，实测 ${tags}`, "数值不变量：tag 注册数");
    // README 徽章 2400+ 不得大于实测
    const badge = /automation tests-(\d+)%2B/.exec(fs.readFileSync(path.join(BASE, "README.zh-CN.md"), "utf8"));
    if (badge && num(badge[1]) > simple + complex) major("NUM-BADGE", "README.zh-CN.md", 1, `README 徽章声称 ${badge[1]}+，实测只有 ${simple + complex}`, "数值不变量：徽章不得高于实测");
  }

  // 4) 参考页数：index.md 条目数 == Docs/Reference/*.md - 1
  const idxPath = path.join(BASE, "Docs/Reference/index.md");
  if (fs.existsSync(idxPath)) {
    const body = fs.readFileSync(idxPath, "utf8");
    const bullets = (body.match(/^- \[/gm) || []).length;
    const files = fs.readdirSync(path.dirname(idxPath)).filter((x) => x.endsWith(".md")).length;
    if (bullets !== files - 1) major("NUM-REFINDEX", "01_素材账本.md", 1, `index.md 列出 ${bullets} 条，而 Docs/Reference 有 ${files - 1} 个类页`, "数值不变量：参考页数与索引条目一致");
  }

  // 5) UMGParity：文件数 - 1 == UMG 类数
  const upDir = path.join(BASE, "Resources/UMGParity");
  if (fs.existsSync(upDir)) {
    const all = fs.readdirSync(upDir).filter((x) => x.endsWith(".json"));
    const cls = all.filter((x) => !x.startsWith("_")).length;
    const m1 = /`Resources\/UMGParity\/` 下 \*\*([\d,]+) 个文件\*\*/.exec(raw);
    const m2 = /其余 \*\*([\d,]+) 个\*\*一一对应 UMG 类/.exec(raw);
    if (m1 && num(m1[1]) !== all.length) major("NUM-UMGPARITY", "01_素材账本.md", 1, `账本声明 UMGParity ${m1[1]} 个文件，实测 ${all.length}`, "数值不变量：UMGParity 文件数");
    if (m2 && num(m2[1]) !== cls) major("NUM-UMGPARITY-CLS", "01_素材账本.md", 1, `账本声明 ${m2[1]} 个 UMG 类，实测 ${cls}`, "数值不变量：UMG 类数 = 文件数 − 例外名单");
  }

  // 6) .dui 文件数与行数
  const dui = walk(BASE, (p) => p.endsWith(".dui"));
  const mD = /`\.dui` 文件共 \*\*([\d,]+) 个 \/ ([\d,]+) 行\*\*/.exec(raw);
  if (mD) {
    if (num(mD[1]) !== dui.length) major("NUM-DUI", "01_素材账本.md", 1, `账本声明 .dui ${mD[1]} 个，实测 ${dui.length}`, "数值不变量：.dui 文件数");
    const dl = countLines(dui);
    if (num(mD[2]) !== dl) adv("NUM-DUI-LINES", "01_素材账本.md", 1, `账本声明 .dui ${mD[2]} 行，实测 ${dl}`, "行数口径受尾换行影响，仅供核对");
  }

  // 7) 各模块行数自洽：十项之和 == 总行数
  const perMod = /`DreamGUI` ([\d,]+) ｜ `DreamGUITests` ([\d,]+) ｜ `DreamGUIEditor` ([\d,]+) ｜ `DreamGUIControls` ([\d,]+) ｜ `DreamGUIInput` ([\d,]+) ｜ `DreamGUIExtensions` ([\d,]+) ｜ `DreamTween` ([\d,]+) ｜ `DreamGUIRenderer` ([\d,]+) ｜ `DreamGUIK2Nodes` ([\d,]+) ｜ `DreamGUISamples` ([\d,]+)；十项合计 ([\d,]+)/.exec(raw);
  if (perMod) {
    const parts = perMod.slice(1, 11).map(num);
    const total = num(perMod[11]);
    const sum = parts.reduce((a, b) => a + b, 0);
    if (sum !== total) major("NUM-MODSUM", "01_素材账本.md", 1, `十个模块行数之和 ${sum} ≠ 声明合计 ${total}`, "数值不变量：各模块行数自洽");
    const totalDecl = num((/\*\*[\d,]+ 文件 \/ ([\d,]+) 行\*\*/.exec(raw) || [])[1]);
    if (totalDecl && total !== totalDecl) major("NUM-MODTOTAL", "01_素材账本.md", 1, `模块合计 ${total} ≠ Source 总行数声明 ${totalDecl}`, "数值不变量：两处总行数一致");
  } else adv("NUM-MODSUM-UNPARSED", "01_素材账本.md", 1, "找不到各模块行数的声明行，跳过自洽检查", "期望形如 十项合计 N");
}

// ── 收口 ──────────────────────────────────────────────────────────────────
const order = { major: 0, advisory: 1, info: 2 };
findings.sort((a, b) => order[a.level] - order[b.level] || String(a.file).localeCompare(String(b.file)) || a.line - b.line);
for (const f of findings) {
  console.log(`[${f.level}] ${f.id} @ ${f.file}:${f.line}`);
  console.log(`    ${f.msg}`);
  if (f.evidence) console.log(`    证据：${f.evidence}`);
}
const c = { major: 0, advisory: 0, info: 0 };
for (const f of findings) c[f.level]++;
console.log(`found ${findings.length} (major=${c.major} advisory=${c.advisory} info=${c.info})`);
process.exit(c.major > 0 ? 1 : 0);
