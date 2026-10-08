# _src · 官方 wiki 抓取记录

> 源表 `wiki`。外部活源：`https://gui.toolchain.64hz.cn/docs/`（Docusaurus，中英双语）。
> 抓取方式：`WebFetch`（站点免登录，返回 200）。抓取时间 **2026-10-08**。
> ⚠️ 只登记**页面级**结构与关键原文片段，不整段抄正文 —— 正文引用一律回原页。

## 站点可达性

| 探测 | 结果 |
| --- | --- |
| `https://gui.toolchain.64hz.cn/docs/` | 200，正常渲染，无需登录 |
| `https://gui.toolchain.64hz.cn/sitemap.xml` | **404**（站点未提供 sitemap，故页面清单靠导航页与 Docusaurus 卡片反推，不完整） |

## 已确认存在的页面

来源：`/docs/` 首页卡片 + `/docs/concepts/widgets/` 与 `/docs/controls/` 两页正文内链 + 插件 `README.zh-CN.md` 的文档表。

**start/**

- `/docs/start/installation/` 安装；`/docs/start/first-screen/` 第一个界面；`/docs/start/about/`

**concepts/**（核心概念）

- `/docs/concepts/widgets/` 控件模型
- `/docs/concepts/canvas/` 画布
- `/docs/concepts/widget-blueprints/` 控件蓝图
- `/docs/concepts/layout/` 布局
- `/docs/concepts/text/` 文本
- `/docs/concepts/rendering/` 渲染
- `/docs/concepts/input/` 输入
- `/docs/concepts/animation/` 动画

**dui/**（.dui 语言）

- `/docs/dui/overview/` 总览；`/docs/dui/nodes-and-values/` 节点与值

**controls/**

- `/docs/controls/` 控件库总览；`/docs/controls/styles/` 样式与样式表；`/docs/controls/umg-parity/` 与 UMG 对照

**其他**

- `/docs/diagnostics/` 诊断码；`/docs/reference/` 类参考；`/docs/changelog/` 更新日志
- `/docs/tools/designer/` 工具·设计器
- `/docs/guides/migration/`、`/docs/guides/fonts-and-packaging/`、`/docs/guides/platforms/`
- `/docs/reference/dreamwidget/`、`/docs/reference/dreambutton/`、`/docs/reference/dreamtoggle/`、`/docs/reference/dreaminputkeyselector/`、`/docs/reference/dreamslider/`、`/docs/reference/dreamspinbox/`、`/docs/reference/dreamprogressbar/`、`/docs/reference/dreamthrobber/`、`/docs/reference/dreamtextinput/`、`/docs/reference/dreameditabletext/`、`/docs/reference/dreammultilineeditabletext/`、`/docs/reference/dreamrichtextblock/`、`/docs/reference/dreamlistview/`、`/docs/reference/dreamtileview/`、`/docs/reference/dreamtreeview/`、`/docs/reference/dreamscrollbox/`、`/docs/reference/dreamscrollbar/`、`/docs/reference/dreamlistviewbase/`、`/docs/reference/dreamborder/`、`/docs/reference/dreamexpandablearea/`、`/docs/reference/dreamtabview/`、`/docs/reference/dreamdialog/`、`/docs/reference/dreammenuanchor/`、`/docs/reference/dreamdropdown/`、`/docs/reference/dreamringmenu/`、`/docs/reference/dreamnativewidgethost/`、`/docs/reference/dreamuicontrol/`

## 关键原文片段（够核对即可）

以下三段是抓到的原文关键句，用于支撑账本里对应断言的「证据」字段。

**控件模型页（`/docs/concepts/widgets/`）**

> 「DreamGUI 里的一个控件是一个 `UDreamWidget`：一个 `UObject`，有矩形、轴心和锚点，排成一棵树……它不是 Actor，也不是 ActorComponent；没有 `SWidget` 在后面」

> 「坐标是 y 朝上的：`Pivot`、`RenderTransformPivot`、`PerspectiveOrigin` 都以左下为原点，这和 UMG 的左上原点不同，是有意的」

> 「视觉分三类（`EDreamVisualType`）：`BatchMesh` 由画布合批……`DirectMesh` 直接写自己的网格段……`PostProcess` 是读写屏幕的效果」

**控件库页（`/docs/controls/`）**

> 「控件库在 `DreamGUIControls` 模块里，位于输入系统之上（L3）。它提供两层东西：`Dream*` 控件……**层级由代码搭建**……`UI*` 行为」

> 「`UDreamUIControl` 在 `NativeOnInitialized` 里按这个顺序跑四步：树 / 部件 / 行为 / 外观」

> 「一个类在它的 `.cpp` 里用一行宏把自己登记到注册表：`DECLARE_DREAM_GUI_WIDGET("Native", "Toggle", UDreamToggle)`」

**首页（`/docs/`）**

> 「插件版本 `1.0.0`，第一个公开版本……引擎 Unreal Engine `5.8`……平台 Win64 是唯一构建并运行过的平台……许可证 MIT……来历 Lex Liu 的 LGUI / LexUI 的分叉，不是它的直接替代品」

## 与仓库内文档的对应关系

插件仓库自带的 `Docs/` 是 wiki 的**子集/镜像**，可离线核对：

| wiki 页 | 仓库内对应 |
| --- | --- |
| `/docs/dui/*` | `Docs/DuiLanguage.md`（1,075 行） |
| `/docs/guides/migration/` | `Docs/Migration.md`（585 行） |
| `/docs/guides/fonts-and-packaging/` | `Docs/FontsAndPackaging.md`（149 行） |
| `/docs/reference/` | `Docs/Reference/`（82 个类页 + `index.md`） |

⚠️ 反过来**不成立**：wiki 有 `concepts/`、`controls/`、`diagnostics/`、`tools/`、`guides/platforms/` 这些**仓库内没有对应文件**的页 —— 这些内容只存在于 wiki，属于外部活源，采集时已按 90d 复审周期登记。
