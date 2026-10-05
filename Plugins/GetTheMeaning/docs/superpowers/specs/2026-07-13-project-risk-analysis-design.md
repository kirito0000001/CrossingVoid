# GetTheMeaning 项目风险分析设计

## 目标

在现有蓝图导出结果之上增加项目级静态风险分析和可选 SaveGame 存档审计，用于发现结构体成员未写入、同类型标识字段可能重复、未检查的 Map Find、RPC 标识参数缺少有效性验证，以及存档中实际存在的无效标识等问题。

分析只在用户执行导出或主动点击审计按钮时运行，不挂接 Tick，不修改蓝图，不修改存档。

## 功能范围

### 1. 结构体成员读写索引

扫描项目内已导出的蓝图，记录结构体成员的访问情况：

- `Break Struct` 中已连接输出引脚记为读取。
- `Make Struct` 中已连接输入引脚记为明确写入。
- `Make Struct` 中未连接且使用默认值的成员记为默认构造。
- `Set Members in Struct` 中已公开并参与赋值的成员记为明确写入。
- 每条记录包含资产、图表、节点 ID、节点标题、结构体路径、成员名、成员类型和数据来源。

项目报告必须区分：

- `NoKnownWriter`：导出的蓝图中没有发现写入点。
- `DefaultOnly`：只发现默认构造，没有非默认来源。
- `ExternalOrSerialized`：值来自 SaveGame、DataTable、RPC 输入、函数输入、C++ 属性或其他无法静态证明的外部来源。
- `KnownWriter`：至少存在一个明确写入点。

报告不得把 `NoKnownWriter` 描述为“变量必定未设置”，必须使用“未发现已知蓝图写入”。

### 2. 同类型标识字段一致性检测

分析 Map、Set、结构体和字段数据流，查找以下通用模式：

- `TMap<KeyType, FSomeStruct>` 的 Value 结构体内存在与 Key 相同类型、且名称或元数据表明其承担标识用途的成员。
- Map Key 与 Value 内身份字段被分别写入不同来源。
- Value 内身份字段没有已知写入，但被下游身份逻辑读取。

重复身份字段只作为候选风险，不自动建议删除。分析器必须区分：

- `IntentionalMirror`：Value 内标识字段有明确写入，并且与 Map Key 来自同一数据源；输出 Info。
- `UnverifiedMirror`：字段用途可能是脱离 Map 后继续携带身份，但静态分析无法证明一致；输出 Hint。
- `InconsistentIdentity`：Map Key 与 Value 身份来源不同，或 Value 身份只使用默认值；输出 Warning。

生产规则不得硬编码任何项目业务类名、结构体名、变量名或本地化 DisplayName。字段名只作为低权重启发信息，主要依据必须是属性类型、引脚连接、来源一致性、网络标记和实际存档值。

### 3. 关键字段未初始化风险

当 GUID、UID、ID、Identifier 等标识字段流向以下通用关键接收点时，提高风险等级：

- Replicated 或 RepNotify 属性。
- Map Key 或 Set 元素。
- Server RPC 参数。
- Branch 条件、对象查找或索引操作。
- SaveGame 写入。
- 其他网络输出参数。

若关键字段状态为 `NoKnownWriter` 或 `DefaultOnly`，输出 Warning；若来源为外部数据但没有验证，输出 Hint。

### 4. Map Find 专项检查

识别 `Map Find` 调用并检查 Bool 返回值：

- Bool 未连接到 Branch、变量或返回值时，输出 `UncheckedMapFind`。
- 警告必须包含 Map、Key 来源，以及 Value 继续流向的节点。
- 提示文字明确说明：查找失败时会继续使用默认 Value，数组可能为空、对象可能为空、数字可能为零。

警告描述应根据 Value 的实际类型生成，例如数组默认值为空、对象默认值为空、数值默认值为零，而不是包含任何项目专用文本。

### 5. RPC 身份参数验证

扫描 `RunOnServer` RPC 的输入参数。对于 `FGuid` 等强标识类型，或名称/元数据符合可配置标识规则的参数：

- 检查调用端数据来源。
- 检查调用前有限深度执行流内是否存在有效性判断。
- 检查服务器 RPC 入口后有限深度执行流内是否存在有效性判断或 Map Find 成功分支。

如果完全没有验证，输出 `UnvalidatedIdentityRpc`。如果服务器通过受保护的 Map Find 验证，则降级为 Info，不重复报高风险。

### 6. SaveGame 存档审计

在 GetTheMeaning 编辑器菜单中新增“审计 SaveGame 存档”操作：

- 弹出目录选择器，默认定位项目 `Saved/SaveGames`。
- 允许选择项目外的打包客户端或服务器 `SaveGames` 目录。
- 只读扫描目录内 `.sav` 文件。
- 使用 Unreal SaveGame 反序列化能力加载对象，再通过反射递归检查属性。
- 不保存、不重写、不迁移任何存档。

审计内容包括：

- `FGuid` 是否有效；其他标识类型只做静态来源检查，不猜测业务合法值。
- Map 的 FGuid Key 是否有效。
- `TMap<FGuid, Struct>` 中 Key 与 Value 内身份字段是否一致。
- 身份字段为零、Key 与 Value 不一致、重复身份字段时记录完整属性路径。
- 无法反序列化的文件单独记录，不中断其他文件。

输出：

- `Saved/GetTheMeaningExports/SaveGameAudit.json`
- `Saved/GetTheMeaningExports/SaveGameAudit.md`

## 通用性约束

- 生产代码不得引用宿主项目的类名、结构体名、变量名或本地化 DisplayName。
- 规则只依赖 Unreal 反射类型、节点类别、引脚类型、连接关系、函数网络标记和 SaveGame 实际值。
- 标识名称模式放入插件设置，默认提供 `Guid`、`Uid`、`Id`、`Identifier`，允许项目扩展或关闭。
- 项目示例只作为自动化测试夹具和人工集成验证，不参与规则匹配。
- 无法证明业务语义时输出 Hint 或 Info，不输出确定性错误。

## 架构

新增独立的项目风险分析模块，避免继续扩大 `BlueprintToTextExporter.cpp` 的职责：

- `BlueprintRiskAnalyzer.h/.cpp`：从蓝图节点收集结构体成员、Map、RPC 和关键数据流事实。
- `ProjectRiskReport.h/.cpp`：汇总全部资产事实，应用跨资产规则并写入项目报告。
- `SaveGameAuditor.h/.cpp`：加载选定目录中的存档并执行递归反射审计。
- `BlueprintToTextExporter.cpp`：保留现有单蓝图导出和单节点风险提示，并调用新的事实收集器生成兼容数据。
- `GetTheMeaning.cpp`：在批量导出结束后生成项目风险报告，并注册存档审计菜单命令。

静态报告输出：

- `Saved/GetTheMeaningExports/ProjectRiskReport.json`
- `Saved/GetTheMeaningExports/ProjectRiskReport.md`

## 性能

- 静态扫描只在批量导出时执行一次。
- 每个图表按节点与引脚线性遍历，目标复杂度为 `O(Nodes + Pins + Links)`。
- 跨资产规则基于按结构体路径、字段名和资产路径建立的索引执行，避免全量两两比较。
- SaveGame 审计只在用户主动点击后运行，逐文件处理并显示进度。
- 不注册 Tick、资产变更监听或运行时钩子。

## 风险等级与误报控制

- `Error`：存档已经确认存在无效身份 Key 或无法反序列化。
- `Warning`：关键身份值无已知写入、Map Find 未检查、Key 与 Value 身份不一致。
- `Hint`：存在同类型标识镜像、标识来源为外部且静态无法证明。
- `Info`：存在验证路径或仅用于说明数据来源。

每条静态警告必须包含 `confidence`：`Confirmed`、`High`、`Medium`、`Low`。静态分析不得把存档未知值标记为 `Confirmed`。

## 兼容性

- 保留现有 `ReadableCode.txt`、`Logic.json`、`ExportIndex` 和 `ExportGraph` 格式。
- 新字段只追加，不删除或重命名现有字段。
- 新项目报告使用独立 schemaVersion。
- SaveGame 审计失败不会导致蓝图批量导出失败。

## 测试策略

使用 Unreal Automation Tests，核心规则先以纯事实数据进行单元测试：

- 结构体字段只有读取、没有写入时产生 `NoKnownWriter`。
- 默认 Make Struct 产生 `DefaultOnly`，明确连接输入产生 `KnownWriter`。
- 合成测试结构 `FExampleIdentityRecord` 的标识成员与同类型 Map Key 产生一致性分析结果。
- Map Key 与 Value 标识来源相同时只产生 Info，来源不同或 Value 标识为默认值时产生 Warning。
- 普通结构体数组和不承担标识用途的同类型字段不产生一致性警告。
- 关键 GUID 无写入并流向 Replicated 属性时产生 Warning。
- 未检查 Map Find 产生 `UncheckedMapFind`，已连接 Branch 时不产生。
- 未验证的 Server RPC GUID 产生警告，受 Map Find 成功分支保护时降级。
- SaveGame Key 有效但 Value.GUID 为零时产生确认警告。
- 非身份字段和普通 Map 不产生上述警告。

最后使用中性的合成蓝图夹具复现“Map Key 有效、Value 内同类型标识保持默认值、默认值继续流向网络属性”的数据流，确认通用规则能够推导出风险。夹具不得包含玩家、房间、卡牌或其他宿主项目语义。现有导出文件必须仍可正常生成。

## 完成标准

- 六项能力全部可用。
- 批量导出自动生成项目风险报告。
- 存档审计可选择项目外目录并生成独立报告。
- 报告能通过通用规则复现本次 GUID 漏洞的静态风险和存档确认结果。
- 自动化测试通过，宿主 Unreal 项目的 Editor 目标编译通过。
- 不改变任何蓝图和存档内容。
