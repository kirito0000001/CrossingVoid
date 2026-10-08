# 《零境交错：空界幻境》Wiki 项目 · 长期约定

## 项目性质
- 《零境交错：空界幻境》= 已停服官方手游《电击文库：零境交错》(SEGA×91Act, 2018-08-07 ~ 2020-03-21, Unity) 的**复活 + 重置版**，**同一个游戏**，作者 TFAC-晓桀（GitHub `kirito0000001`）**纯自研**、UE 重写、公开源码、**仅学习使用、绝不盈利**、**一人开发**。
- 游戏正式名统一写：**零境交错：空界幻境**（原版才叫「电击文库：零境交错」）。

## 双工作区
- 研究 / 中间稿 → `C:\CrossingVoid\Docs\wiki-research\`
- 最终玩家向页面 → `D:\NewData\Wikis\crossing-void-wiki\docs`（Docusaurus，121 页）

## 硬约定
- **wiki（含 README）不出现作者本机的项目路径**（用户 2026-10-08 明确）。玩家页面禁止出现蓝图名、类名、导出文件名、引擎目录、服务器运维信息。
- 例外：`docs/download/pc.md` 的 `D:\TFAC-hz64\CrossingVoid`（启动器默认游戏安装目录，面向玩家）**保留原样**。
- 引外部资料时区分一手（导出/作者口述/游戏表现）与二手（萌娘百科等），二手只作基线参考。
- 剧情正文不得擅改原意；术语统一：摇光、空界幻境、主序光、载体、收束时刻。

## 抓取 B站 的正确姿势（已验证）
- 必须用**系统 Chrome** `C:\Program Files\Google\Chrome\Application\chrome.exe` + **复制后的独立 profile 副本**（Chrome 拒绝默认目录开调试；agent-browser 自带 Chromium 解不开 App-Bound 加密）。
- 动态/opus 页需登录态；视频简介 API `x/web-interface/view` 免鉴权；评论区需登录（已放弃）。
- 作者两号：官号 `452379907`（版本公告）、个人号 `385531871`（开发日志）。

## 上线部署（2026-10-08 实操验证）
仓库根 `D:\NewData\Wikis\README.md` 有四步流程（**先读它，别自己发明**）：
1. 本地：改 md → `git commit` / `git push`（远端 `github.com/kirito0000001/Wikis`，仓库根操作，不在子目录建仓库）
2. 构建：`cd crossing-void-wiki` → `npm install`（首次）→ 设 `DOCUSAURUS_URL=https://www.crossingvoid.top` + `DOCUSAURUS_BASE_URL=/wiki/crossing-void/` → `npm run build`
3. 打包 `build/*` → `scp` 到 `crossing-server:C:/Temp/`
4. 服务器展开替换 `C:\inetpub\wwwroot\wiki\crossing-void` → 验证线上 200

⚠️ **服务器上没有 node/npm**，构建只能在本地做。
⛔ **最大坑：Git Bash 会转换环境变量里的路径**。`DOCUSAURUS_BASE_URL='/wiki/crossing-void/'` 经 Git Bash 传给 node 后变成 `C:/Users/.../PortableGit/wiki/crossing-void/` → 产物资源前缀全错 → **CSS/JS 404、排版全丢**。
→ **构建必须加 `MSYS_NO_PATHCONV=1`**：
`cd crossing-void-wiki && MSYS_NO_PATHCONV=1 DOCUSAURUS_URL='https://www.crossingvoid.top' DOCUSAURUS_BASE_URL='/wiki/crossing-void/' npm run build`
（⛔ 别加 `MSYS2_ENV_CONV_EXCL='*'`，会让 Docusaurus 清理目录时撞上安全 shim 报错）
⛔ **打包别用 PowerShell 的 `Compress-Archive`（实测失败），用 Python zipfile**
⛔ **部署必须分步验证**：解压到 Stage → **验证 Stage 内容与资源路径** → 替换线上 → 验证 200
⛔ **替换线上目录前必须先备份**（上次 README 那条组合命令的 `Join-Path` 经 ssh 多层引号后不展开，导致线上被清空）

## 玩家向页面口径（2026-10-08）
- 署名 **TFAC 团队**，**不写"一人开发"**（置顶动态的"只有我一个人"是对粉丝口径，wiki 用对外口径）
- **不含**：分卷下载（作者已做到平台级下载器）、未实装内容写成已有（联机/第一章/3D 动作 PVP）
- 世界观的"各种世界交错处"叫 **空界幻境**；"零境"是**游戏简称**，两者别混
- 游戏是**双玩法**：2D 回合制 ＋ 3D 动作 PVP（并列；3D 尚在开发）

## 待办（用户说"先搜集资料"，暂缓）
- docs frontmatter 的 `source: D:\...` 路径清理
- `scripts/write_crossing_docs.py` 的硬编码路径改配置化
- README 去本机路径
