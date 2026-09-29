# CrossingChunkEditor

> **UE 5.8 编辑器插件：在内容浏览器里右键文件夹，直接把它划成（或取消）一个 pak chunk。**
> 规则写进 `Config\DefaultCrossingChunk.ini`——**和打包脚本读的是同一份**。

适配引擎：**Unreal Engine 5.8**（源码版 / Launcher 版均可，纯编辑器插件，不含任何 Runtime 代码）。

---

## 为什么需要它

先把前提说清楚，不然这个插件的存在没道理。

UE 5.8 里想「按目录分包」，能走的路只有一条是活的：**pak 阶段的 `ApplyPakFileRules`**。
`UPrimaryAssetLabel`（`AST_*` 标签）那条路在 cook 阶段不会执行——规则注册代码写在
`UpdateAssetBundleData()` 末尾，而它是 `WITH_EDITORONLY_DATA`；引擎注释里那句
「PostLoad in PrimaryAssetLabel sets PrimaryAssetRules overrides」在 5.8 已经过时
（`UPrimaryAssetLabel` 没有 `PostLoad` 覆写）。引擎自带的 `DefaultChunkAssigner` 也不认标签，
它只有两条规则：启动包 → chunk 0，其余 → chunk 10。

所以分工是：

```
Config\DefaultCrossingChunk.ini          ← 唯一的规则源（人看的、本插件读写的）
        │
        │  Tools\Build-PakFileRules.ps1   翻译（脚本跟着插件走，见「打包脚本」一节）
        ▼
Config\DefaultPakFileRules.ini           ← 引擎吃的（整份重写，标注「自动生成勿手改」）
        │
        │  pak 阶段 ApplyPakFileRules()   逐文件改写 chunk 归属
        ▼
pakchunk0-Windows.utoc / .ucas ...
```

**「编辑器里点右键」和「打包时读的」必须是同一份 ini。** 这就是本插件存在的全部理由——
让你不必去手编那份 `+Chunks=(...)` 长行，也不必担心两边不同步。

> 引擎侧那部分排查（Zen oplog / `-RunChunkAssigner` / ZenServer 生命周期 / `DefaultPakFileRules.ini`）
> 是另一篇文章的内容，见文末「相关文档」。

---

## 功能

### 1. 内容浏览器：文件夹右键 → 指定 Chunk

扩展点 `FContentBrowserMenuExtender_SelectedPaths`，段名 `CrossingChunk`（「Chunk 分块」）。

菜单分三段：

| 段 | 内容 |
|---|---|
| 状态行（只读） | `当前：Chunk 3 · 第三章-角色与主界面（本目录直接标记）`<br>`当前：Chunk 0 · 基础包（继承自 /Game/GameActor2D）`<br>`当前：未分类 —— 本次新增，会进新建的 Chunk 8` |
| 指定为 Chunk ▸ | 列出所有现有 chunk（RadioButton，当前所属打勾）+ 最后一项 `新建 Chunk N · 「目录名」` |
| 取消本目录的标记 | 取消后重新跟随最近的上级目录；该目录是这个 chunk 的最后一个标记时，规则一并删掉 |

状态行那三种文案是有意写死的：

- **直接标记**：本目录自己有一条规则，子目录里没另外标的都跟它走。
- **继承自 X**：本目录没标记，跟最近的上级目录走（逐级向上找，`FindLastChar('/')` + 左截）。
- **未分类**：上级也没标过 ⇒ 这批资源是这次新加的，打包时会被收进一个新的 chunk，
  基础包不动，启动器只需要下一个新包。**想让它单独成包，就右键指定一个。**

第 3 条是这个插件最常用的入口：美术/策划新加一批资源，右键一看就知道「它现在算哪个包」。

### 2. 菜单入口：《二游打包》面板

主菜单挂载点为 `LevelEditor.MainMenu.Platforms`，段名 `CrossingChunkPack`（「二游打包」）。
打开的是一个 NomadTab（`ETabSpawnerMenuType::Hidden`——不占编辑器主菜单的「窗口」列表）。

面板是左右两栏：

**左栏**

- 标题行：`共 N 个分块、M 个目录标记   规则文件：<路径>`
- 刷新 / 目标下拉 / 模式下拉 / 开始打包 / 复制命令 / 撤销解散 / 日志目录 / 清除缓存
- 输出目录（`-ArchiveDir`）、地图摘要、玩家版本、基线版本（只读）、基线目录
- 状态行：`报告：` / `产物：` / `基线：`
- 选择地图（扫 `AssetRegistry` 里的 `World` 资产，带搜索框 + 重新扫描 + 全部不勾）
- 规则列表：`号 | 名字（双击可改） | 目录数 | 文件数 | 体积 | 变化 | 解散`
- 「运行设置（额外）」折叠区：Cook 进程数、基线位置

**右栏**

- 本次新增（未分类目录）清单 + 一键指派 + 定位
- 打包日志（实时，按行着色）

### 3. 规则列表

- **双击改名**：提交后立刻写回 ini，右键菜单同步（`ChunkId` 不动——名字只影响显示，
  已经发出去的包不受影响）。
- **解散**：二次确认弹窗，删掉这条规则，它下面的目录回到未分类；确认框里明确写了「可以撤回」。
- **撤销解散**：会话内保留一份被删的规则（`UndoRule`），点一下原样塞回去。
- **文件数 / 体积 / 变化**：来自最近一次打包报告，`+新增 -删除 ~修改`（和上一次报告比）。

### 4. 本次新增（未分类）清单一键指派

清单来自「上一次打包报告」的 `unclassified` 段，但**渲染时会拿当前规则再核一遍**：
已经被某个分块覆盖的，标成 `[已归类] → Chunk N · 名字（下次打包生效）`（绿色）。

不直接删掉这一行是有意的——用户刚点完指派，得看得见「它去哪了」。

旁边还有一个「指派到」下拉，不用跳回内容浏览器右键。它会先做一次 `RefreshRules()`，
**但刷新推迟到下一帧**（`FTSTicker` + `TWeakPtr`）：因为回调正跑在 combo 自己的回调里，
当场重建整棵树会踩到自己。

### 5. 打包（目标 × 模式）

面板本质是个**命令拼装器 + 日志窗口**，真正的活儿全在脚本里。

| 目标 | `-Platform` | `-Target` | 说明 |
|---|---|---|---|
| Windows | `Win64` | `Client` | PC 客户端 |
| 安卓 | `Android` | `Client` | 安卓客户端 |
| 服务器 | `Win64` | `Server` | **出散件、走 git，不分包** |

| 模式 | 值 | 说明 |
|---|---|---|
| 常规补丁更新（新资源单独成包） | `Patch` | 基于已有基线算差异 |
| 版本更新（完整包 + 建基线） | `Base` | 一条基线约 1.5 GB |
| 快速验证（只测试：不产发布包） | `Quick` | 报告单独一份 |

- 选「服务器」时，模式下拉被禁掉、文案变成「服务器散件（不分包，走 git）」，
  命令里强制传 `Quick`——散件没有「版本/补丁」这一说，禁掉比报错友好。
- 打包进行中，除「开始打包/停止打包」和日志框之外**所有控件一律灰掉**。
  中途改目标/改地图/改规则只会让这次打包的结果对不上。
- 命令里的**内层引号写成 `\"`**。这两个值最终会被塞进 `-Command "..."` 的双引号里，
  Windows 解析命令行时裸引号会被吃掉：逗号被当成数组分隔符 → `-Maps` 绑不定 `[string]`
  （实测报 `ParameterArgumentTransformationError`）；带空格的路径会被拆成两个参数。

### 6. 两条前置检查（把「跑到一半才失败」提前到点击那一刻）

1. **目标 Shipping 二进制不存在**
   打包脚本会给 UAT 加 `-build`，而编辑器开着（Live Coding 激活）时 UE 不允许从外部编译，
   UBT 会抛 `Unable to build while Live Coding is active`。
   → 不硬闯，也不 return：把提示写进日志，让脚本自己去**单独编译那个目标**
   （单独编译只针对这个目标，不会被 Live Coding 挡住，所以编辑器不用关）。

2. **《常规补丁》但找不到基线**
   → 弹窗问「要先切到《版本更新》跑一次吗？」，点是就自动切好（并在状态栏说明「再点一次开始打包」）。
   脚本自己也会拦，但这里先拦能把「该怎么做」直接说清楚。

### 7. 停止打包

杀的是**整个进程树**（`TerminateProc(..., KillTree=true)`）：脚本是
`powershell.exe → RunUAT.bat → UnrealEditor-Cmd`，只杀 PowerShell 的话后台 cook 会继续跑。

被杀时脚本末尾的「还原 `DefaultGame.ini`」没机会执行，所以面板替它收尾：
把 `DefaultGame.ini.bak-maps-tmp` 移回去、删掉 `Saved\PackLogs\maps-override.txt`，
日志里写明 `[已停止] 地图清单已还原`。否则工程配置会停在
「只 cook 勾选的那几张地图」的临时状态。

### 8. 清除缓存

只删三样，都能自动重建：

```
Saved\StagedBuilds   —— UAT 组装的产物
Saved\Cooked         —— cooked 数据
Saved\Shaders        —— shader 缓存
```

**不动** `Intermediate\Build`（编译产物）、`Content`、归档目录。有任务在跑时先拦一道。

### 9. 状态行与平台设置「防忘」

状态由脚本写成一个小 json（`Saved\PackLogs\pack-status.json`），面板只读它——
**不去解析 stdout**，那东西会随终端宽度换行。

`报告：09-19 12:32 · 版本更新` / `产物：D:\...\pakchunk0-Windows.utoc ✓` / `基线：1.4 ✓`

解析不出来时有三句人肉兜底（`报告：—` / `（状态文件没生成）` / `（状态解析失败）`）——
留一格空白是最难查的毛病。

还有一份「平台设置」摘要（包名 / 商店号 / 玩家版本 / minSDK / targetSDK / arm64 /
安卓图标张数 / Windows 图标），在**页面打开时**和**每次点打包时**顶到日志区最上面：

```
===== 当前平台设置（打包前过目一遍）=====
  安卓   版本 1.4.2（商店号 12）· com.xxx.yyy
         SDK 23/34 · arm64 · 图标 6 张（...）
  [警告] 图标 6 张全是引擎默认 → 去「项目设置 → 平台 → Android → 图标」换
  （图标放这里：Build\Windows\Application.ico 与 Build\Android\res\drawable*/icon.png）
```

- 安卓图标是不是引擎默认，是脚本**按 hash 比出来的**，不靠感觉。
- Windows 图标只能放 `Build\Windows\Application.ico`（这项没有界面设置，所以专门给一句提醒）。

### 10. 打完给个动静

复刻编辑器自己「编译完成」那一套：

- 音效同两个 cue：`/Engine/EditorSounds/Notifications/Compile{Success,Failed}_Cue`
- 原生 toast（`FSlateNotificationManager`），成功绿勾 / 失败红叉，带一个「打开日志目录」的链接

清缓存是顺手操作，不吵人——不播不弹。

---

## 用起来是什么样

1. 加了资源 → 在内容浏览器右键它所在的目录 → 「指定为 Chunk」；
   或者**什么都不做**，让它进新 chunk（未分类 = 本次新增，不会污染基础包）。
2. 打开《二游打包》页 → 确认「本次新增」清单和预期一致。
3. 选目标、选模式、勾地图 → 开始打包。
4. 打完看报告：每个 chunk 多少文件、多大体积、比上次变了多少。

**这一页的定位是「打包前的检查台」，不是「打包按钮页」。**
「我这次只动了 3 个角色图，面板却说 chunk3 新增了 500 个文件」——
那说明有东西误入或误删了。这才是它存在的意义。

---

## 规则文件

默认路径：`<工程>\Config\DefaultCrossingChunk.ini`

段名：`/Script/CrossingChunk.CrossingChunkRuleSet`

```ini
[/Script/CrossingChunk.CrossingChunkRuleSet]
+Chunks=(ChunkName="基础包",ChunkId=0,Folders=((Path="/Game/BaseC"),(Path="/Game/MapS")),Priority=0,bIncludeInInstallPackage=True,ForceExcludeFolders=,Comment="随安装包发布。")
+Chunks=(ChunkName="登录资源",ChunkId=4,Folders=((Path="/Game/AssetMaterial/ImageS/Login_Image"),(Path="/Game/UIWidget/LoginUI")),Priority=0,bIncludeInInstallPackage=False,ForceExcludeFolders=,Comment="登录相关图片与UI。")
+GlobalExcludeFolders=
```

写回时的行为：**保留文件里其它段和注释，只替换 `+Chunks=` 那些行**，
并在规则数变多时把新行插在 `+GlobalExcludeFolders=` 之前。新建 chunk 时
`bIncludeInInstallPackage = (ChunkId == 0)`——只有 0 号包默认进安装包。

> 这个段名和格式是给插件的 `Tools\Build-PakFileRules.ps1` 看的，本插件只负责读写、不参与翻译。

---

## 目录结构

```
CrossingChunkEditor/
├─ CrossingChunkEditor.uplugin
├─ Source/CrossingChunkEditor/
│  ├─ CrossingChunkEditor.Build.cs
│  ├─ Public/
│  │  ├─ CrossingChunkEditorModule.h     FCrossingChunkRule / FCrossingChunkRuleService / FCrossingChunkEditorModule
│  │  └─ SCrossingChunkPanel.h           SCrossingChunkPanel（面板）
│  └─ Private/
│     ├─ CrossingChunkEditorModule.cpp   规则读写 + 两个菜单入口
│     └─ SCrossingChunkPanel.cpp         Slate UI + 打包调度 + 报告/状态解析
├─ Tools/                                ← 打包脚本（跟着插件走，见下一节）
│  ├─ Pack-CrossingVoid.ps1              打包 / 清缓存 / 状态查询：主入口
│  ├─ Build-PakFileRules.ps1             DefaultCrossingChunk.ini → DefaultPakFileRules.ini
│  ├─ Build-ChunkReport.ps1              产出分包报告 json
│  └─ 打包说明.md                          脚本侧的完整说明
└─ Docs/
```

模块类型 `Editor` / `LoadingPhase` `PostEngineInit`。
依赖：`Core CoreUObject Engine`（Public），
`Slate SlateCore InputCore ApplicationCore AssetRegistry UnrealEd ContentBrowser Json ToolMenus Projects`（Private）。

（`Projects` 是为了 `IPluginManager`——脚本路径要从插件目录推出来。）

---

## 编译 / 安装

1. 整个 `CrossingChunkEditor` 目录丢进 `<工程>\Plugins\`（或引擎的 `Engine\Plugins\`）。
2. 编译（生成工程文件后 Build，或直接 Build 编辑器目标）。
3. 编辑器里确认插件已启用，重启。

**Binaries / Intermediate 不入库**（见 `.gitignore`），拿到源码后自己编一遍。

---

## 打包脚本（插件自带）

面板所有「干活」的部分都是调用 PowerShell 脚本；**脚本就放在插件的 `Tools/` 里**——
插件装到哪儿，脚本就在哪儿，不会出现「插件更新了、脚本还是老的」。

| 脚本 | 谁在用 | 作用 |
|---|---|---|
| `Tools\Pack-CrossingVoid.ps1` | 开始打包 / 停止打包 / 复制命令 / 清除缓存 / 状态查询 | 主入口。参数：`-ProjectRoot -Mode -Platform -Target -ArchiveDir -ReleaseRoot -Maps -ReleaseVersion -PlayerVersion -CookProcessCount`；另有 `-ClearCache` / `-Status` |
| `Tools\Build-PakFileRules.ps1` | 被上面那个调用 | `DefaultCrossingChunk.ini` → `DefaultPakFileRules.ini` |
| `Tools\Build-ChunkReport.ps1` | 被上面那个调用 | 产出 `Saved\PackLogs\ChunkReports\<目标>\latest.json` |

面板读取的两份产物（都只读，不写）：

```
Saved\PackLogs\pack-status.json                      状态行（-Status 产出）
Saved\PackLogs\ChunkReports\<Win64-Client|Android-Client|Win64-Server>[-Quick]\latest.json
```

报告目录名 = `<Platform>-<Target>`；**《快速验证》的报告带 `-Quick` 后缀单独存一份**——
它不打 pak，体积列本来就是 0，不能冲掉正式报告里的体积/变化。

### 脚本怎么找到工程

脚本不再写死工程路径：从**脚本自己所在目录**逐级向上，找第一个含 `*.uproject` 的目录，
那就是工程根。所以下面两种摆放都能跑：

```
<工程>\Tools\                              ← 旧布局（脚本挂在工程里）
<工程>\Plugins\CrossingChunkEditor\Tools\  ← 插件自带（本仓库的形态）
```

面板调脚本时还会**显式传 `-ProjectRoot`**，两条保险都上：插件装到 `Engine\Plugins\`
下时向上找不到 `.uproject`，靠面板传的这个值也能跑。
（脚本之间互相调用用的是 `$PSScriptRoot`，所以三份脚本必须待在同一个目录里。）

### 换机器要改的默认值

脚本里有几处**与本机环境绑定**的默认值，换机器要改（都在文件头的 `param` 里）：

| 值 | 位置 | 说明 |
|---|---|---|
| `D:\UnrealEngine-5.8.2` | 三份脚本的 `-EngineRoot` / `-UatLogDir` | 引擎根目录。源码版才需要；Launcher 版改成对应安装路径 |
| `F:\DaBaoV\Crossingvoid` | `Pack-CrossingVoid.ps1` 的 `-ArchiveDir` / `-ReleaseRoot` | 默认输出目录与基线目录（面板上的「输出目录」「基线位置」会覆盖它） |
| `CrossingVoid` / `CrossingVoidServer` | 各处的工程名与 target 名 | 换项目就得跟着改 |

---

## 一些取舍（写在这里免得被「优化」掉）

### 设计原则（别走回头路）

- **不在编辑器里估算体积。** 估不准会误导，体积一律以打包报告为准。
- **页面不自己重算规则匹配。** 两份实现必然漂移——规则怎么匹配由打包脚本说了算，
  面板只读脚本产出的报告。
- **chunk 号不提供随意修改。** 号是下载器认的稳定标识，改了会让已发布的包对不上。
  名字只用于显示，随便改。
- **不改引擎。** 全部走 `DefaultPakFileRules.ini` 这套官方配置。

### 具体实现

**Cook 进程数上限卡在 4。**
引擎侧没这个限制（上限 254），卡得住是因为再高只会更慢：`CookDirector` 里
`CoreLimit = 物理核数 / 进程数`（**整除**），8 核机器 N=4 → 每进程 2 核；N≥5 直接掉到 1 核，
再往上只是多几个进程抢同一块 CPU，而每个 worker 都是一整个 editor 进程。
实测 N=5 比单进程慢 44%。换机器（物理核数变了）时这个值要跟着重算。
界面上那一行会实时把切分结果写出来：`多进程 cook：1 director + 3 worker；本机 8 核/16 线程 → 每进程 2 核 4 线程，合计 16 线程`。

**基线版本是派生值，不落盘、不给改。**
`1.4.2 → 1.4`（取主版本段）。这样永远不会出现「玩家版本和基线版本两边对不上」。

**勾选的地图按目标分开存**（`Maps_Client` / `Maps_Server`）：服务器和客户端要打的图不一样。
切换目标时先把旧目标的勾选存回去，再把新目标的贴回来。

**日志自己写 marshaller，不用现成的 TextBox。**
照 `SOutputLog.cpp` 的做法实现 `FBaseTextLayoutMarshaller`：既能按行上色
（红/黄/绿/青四色，关键词匹配），又保留选中和复制（`GetText` 原样交回 `PlainText`）。
注意 5.8 里 `FTextLayout::FNewLineData` 要两个参数（`Text` + `Runs`）。
日志只保留最后 20000 字符，避免长时间打包把内存堆满。

**UI 文案里不用 `▾` 这类符号**——编辑器默认字体里会渲染成方框。用文字。

**平台设置那个 `FString::Printf` 先落局部变量再拼**，不要写成
`FString::Printf(TEXT("%s"), *FString::Printf(...))`——`*` 取的是临时对象，会踩悬垂指针。

**规则文件自己读写 UTF-8。**
不用 `FFileHelper::LoadFileToStringArray`：那份实现在**没有 BOM** 时会按 ANSI 处理，
中文分块名会直接毁掉。

---

## 已知问题 / 待办

**① 安卓平台生不出分包报告。**
根因是 `Pack-CrossingVoid.ps1` 的收尾逻辑挂在 `if (Test-Path $PaksDir)`，而安卓的 pak
打进 APK/OBB，没有 `Content\Paks` 目录，掉进 else 就跳过了——体积统计和清单归档目前会被跳过。
详见 `Docs/2026-09-20-待办-安卓分包报告.md`。

**② 验证覆盖度。**
打包链路完整验证过的是 **Windows + Client**。安卓与专用服务器（Server 散件）在脚本层面支持，
验证还在推进。

**③ 「撤销解散」只在当前编辑器会话内有效。** 关掉编辑器就没了（要更久的可逆性走 git）。

**④ 「本次新增」清单来自上一次打包报告。** 刚打开、还没打过包时它是空的——
面板会明说「还没有打包报告 —— 先在「平台」菜单里跑一次打包」。

**⑤ 未在界面上暴露的字段。** `Priority` 和 `ForceExcludeFolders` 会原样保留在规则里，
但不提供 UI（语义未验证，见「设计原则」最后一条的延伸：
界面上每多一个开关，就多一个「它到底管不管用」的问题）。

---

## 相关文档

- **《UE5-按目录分Chunk记录》**——引擎侧那条链路怎么摸出来的
  （`PrimaryAssetLabel` 为什么失效、`-RunChunkAssigner`、`bUseZenStore` 必须配 `bUseIoStore`、
  ZenServer 生命周期、`DefaultPakFileRules.ini` 的匹配顺序与收尾三条）。
  <https://zhuanlan.zhihu.com/p/2084724901970289419>
- `Tools\打包说明.md`——脚本侧的打包说明（参数、产物、服务器散件、pdb 与符号文件等）。
- 参考：`UE5 非uasset资产Chunk/Pak 划分踩坑笔记` <https://zhuanlan.zhihu.com/p/689375430>
- 参考：`Engine\Config\BasePakFileRules.ini`（引擎自带，PakFileRules 全部字段的权威说明）
