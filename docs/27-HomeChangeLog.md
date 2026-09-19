# 家园开发 · 协作变更记录

更新：2026-09-10。发布基线 v0.6（地牢 D0~D5 已完成）。家园使用 **H0～H5** 编号。下方按批次保留历史；当前状态以末尾“场景与 UI 收尾”一节为准。

## 2026-09-09 · 家园规划交付（历史）

操作者：Codex 助手。依据：用户给定的七座建筑、神像四级数值、只读查看边界、战备处位置，以及当前代码和文档。本轮只写规划，未改源码/资产。

## 2026-09-09 · H0～H5 C++ 全部交付

操作者：DSH 助手。依据：用户"完成全部 C++ 部分（H1~H6）"的要求 + 规划第 3~4 节数值；用户指定**升级货币名为"金币"**（规划中的内部 ID `HomeSupplies` 统一改名 `Gold` / 金币，显示与代码一致）。

### 行为变化（玩家现在能做什么）

| 批次 | 交付 |
|---|---|
| H0 契约 | `LKHomeTypes.h`（七建筑枚举、稳定 ID、永久档、区域、加成/收益快照、面板模型）、`LKHomeContent`（默认数值、解锁集、战备校验、费用曲线、区域目录）、`FLKExpeditionStartRequest`（一次出征的完整输入） |
| H1 永久档 | `ULKProfileSubsystem` + `ULKProfileSaveGame`：A/B 双槽 + 递增 Revision + "候选写入→读回校验→发布"，坏档保留原文件并禁用永久写；金币、建筑等级、解锁集、已保存战备、已处理结算 |
| H1 家园场景 | `ALKHomeGameMode`（无敌人/无计时；自动生成七座占位建筑与地面；组装面板模型与命令）、`ALKHomePlayerController`（俯视相机接管、鼠标点建筑、Esc 关面板、数字键 1~7）、`ALKHomeBuildingActor`（位置 + 命中体积 + BuildingId + 名称/等级标签 + 悬停反馈，不继承 ALKUnitBase） |
| H1/H2 原生 UI | `ULKHomeHUDWidget` + `ULKHomeListButtonWidget`：顶部金币/远征状态、建筑快捷栏、单张面板（遮罩、关闭、Esc）、战备草稿未保存时先确认；蓝图子类可换肤 |
| H2 收藏/战备 | 图书馆（只列永久解锁法术）、英雄之家（只列三名玩家英雄，敌方不入选）、军营（佣兵/战斗建筑两页签）、战备处（三英雄 + 5~7 卡草稿、保存/恢复、平均费用、非法草稿禁用保存） |
| H3 出征 | 大门区域地图（首区"亡灵边境"）；`StartNewRun/RestartRun(FLKExpeditionStartRequest)` 在第一次 AutoSave 前写入区域/ProfileId/战备/加成快照/收益规则；`FLKBattleContext` 带上快照；战斗玩家银币与战后恢复读取快照；终态结算按钮改为"返回家园"（`L_Home` 不存在时退回"从头开始"） |
| H4 金币与升级 | 每房胜利按奖励档暂存金币（10/20/40），通关 +20，失败保留 50% 向下取整，放弃 0；终态冻结 `FLKSettlementReceipt` 并由 Profile 幂等入账（同 SettlementId 只加一次）；`UpgradeBuilding` 单次提交（校验等级/费用/余额 → 写盘读回 → 发布）；神像 1~4 级恢复 40/60/80/100%，金库 1~5 级产速 +10%/级、上限 +1/级 |
| H5 迁移与回归 | 远征档 Schema 4 → 5 迁移（补区域/战备/加成按 v0.6 基线冻结/标记旧轮不参与金币结算）；新增 `Tests/LKHomeTests.cpp` 七组；调试命令 `HomeGold/HomeUpgrade/HomeReset/ListHomeState` |

### 规则与数值（本轮定稿或沿用建议值）

| 项目 | 采用值 | 状态 |
|---|---|---|
| 升级货币 | **金币**（内部字段 `Gold`） | 用户确定 |
| 神像 | 1~4 级，恢复 40/60/80/100%（+20 个百分点/级），费用 60/120/200 | 用户确定（费用为建议值） |
| 金库 | 1~5 级，产速 ×[1+0.1×(级-1)]，上限 +（级-1），费用 50/100/160/240 | 暂定（用户授权） |
| 每房金币 | 普通 10 / 精英 20 / 首领 40；通关额外 20；失败保留已获胜的 50%（向下取整）；放弃 0 | 建议值（可调） |
| 初始牌组 | 5~7 种唯一卡；三英雄必须唯一且永久解锁 | 建议值 |
| 加成生效时点 | 创建 RunId 时冻结，整轮不变；家园升级/改战备只影响下一轮 | 规划边界 |
| 旧档 | Schema 1~4 载入即迁移到 5；旧轮不参与金币结算，避免追算 | 规划边界 |

### 文件与资产

| 路径 | 新增/修改 | 说明 |
|---|---|---|
| `Source/little_king/LKHomeTypes.h` | 新增 | 家园值类型与稳定 ID |
| `Source/little_king/LKHomeContent.h/.cpp` | 新增 | 默认数值、解锁集、区域、战备校验、出征输入构造 |
| `Source/little_king/ULKProfileSubsystem.h/.cpp` | 新增 | 永久档与事务 |
| `Source/little_king/ULKProfileSaveGame.h/.cpp` | 新增 | 永久档文件（SaveVersion + FLKProfileState） |
| `Source/little_king/ALKHomeGameMode.h/.cpp` | 新增 | 家园世界与面板模型 |
| `Source/little_king/ALKHomePlayerController.h/.cpp` | 新增 | 相机/点击/快捷键 |
| `Source/little_king/ALKHomeBuildingActor.h/.cpp` | 新增 | 占位建筑 |
| `Source/little_king/ULKHomeHUDWidget.h/.cpp`、`ULKHomeListButtonWidget.h/.cpp` | 新增 | 原生家园 UI |
| `Source/little_king/LKRunTypes.h` | 修改 | Schema 5：ProfileId/RegionId/InitialLoadout/BonusSnapshot/RewardRules/PendingGold/PendingSettlement；`FLKExpeditionStartRequest`、`FLKSettlementReceipt` |
| `Source/little_hero/ULKRunSubsystem.h/.cpp` | 修改 | 出征/结算/放弃/迁移/金币暂存；测试槽名钩子 |
| `Source/little_king/ALKBattleGameMode*.cpp/.h` | 修改 | 快照银币与恢复、返回家园、按永久档战备出征、共享遭遇目录校验 |
| `Source/little_king/ULKGameData.h/.cpp` | 修改 | `HomeMapName`/`BattleMapName`；`EnsureCardLibrary()`（家园与战斗共用运行时卡牌目录） |
| `Source/little_king/LKEncounterContent.h/.cpp` | 修改 | 抽出 `BuildValidatedCatalog`（不依赖战斗世界） |
| `Source/little_king/ULKCheatManager.h/.cpp` | 修改 | `HomeGold/HomeUpgrade/HomeReset/ListHomeState` |
| `Source/little_king/little_king.Build.cs` | 修改 | **关闭 Unity 合并编译**（多个 .cpp 定义同名文件内辅助符号，合并后报重定义） |
| `Source/little_king/Tests/LKHomeTests.cpp` | 新增 | 七组家园自动化 |
| `docs/28-HomeTutorial.md`、`docs/validation/Home-Validation.json` | 新增 | 新手教程与验证摘要 |
| `Content/Maps/L_Home.umap` | 当时未制作；现已交付 | 2026-09-10 由 Codex 助手生成并验证，见下文 |

**没有**改动：`DA_GameData`、`DT_Units`、`DT_Traits`、三个 GA、`WBP_BattleHUD`、`L_BattleTest`。

### 存档影响

- 新增独立永久档槽 `LittleKing_Profile_A/B`（A/B 轮换 + Revision）。远征槽 `LittleKing_Run` 保留，`SchemaVersion` 4 → 5 自动迁移。
- 无永久档时首次进入家园自动创建默认档（三英雄七卡、神像/金库 1 级、0 金币、首区解锁），写盘成功后才允许永久操作。
- 自动化测试默认**不读写真实永久档**（`ULKProfileSubsystem` 内的自动化守卫 + 测试专用槽名），地牢回归不会给玩家加金币。

### 实际检查

- 构建：`little_kingEditor Win64 Development` 通过（UE 5.8.1-56057345）。
- 自动化：`LittleKing` **44/44 Success，0 failed**（`Saved/Automation/Home6/index.json`，摘要见 `validation/Home-Validation.json`）。
  - 新增：`H1.Profile.CreateRotateAndRecover`、`H2.Loadout.ValidationAndPersistence`、`H3.Expedition.SnapshotAndApplication`、`H3.World.BuildingsAndPanels`、`H4.Upgrade.TransactionAndCurves`、`H4.Settlement.MathAndHandoff`、`H5.Migration.LegacyRunToSchema5`。
  - 既有 37 组（D0~D5 + Sprint5）全部保持通过。
- **未做**：PIE 目视验收（需要先创建 `L_Home`）、打包、性能与美术。

### 接手事项

1. 当时待制作的 L_Home 已在下一批完成；现在按 [28 教程](28-HomeTutorial.md) 直接试玩现有地图。
2. 验收后把结果补到本文件（操作者/日期/现象）。
3. 后续可选：把家园建筑换成正式美术、加更多区域、法术研究/英雄培养（都属于新解锁玩法，需要先扩展永久解锁集与出征快照）。

## 2026-09-10 · H1～H5 场景与 UI 收尾

操作者：Codex 助手。承接用户已有的 H0～H5 C++ 工作，完成实际家园资产、交互修正、远征界面接入和教程。保留上一位协作者的永久档/金币/规则实现；此批不引入新货币、新英雄或正式美术。

### 玩家可见变化

- 默认启动与编辑器启动均进入 `L_Home`。地图已保存：正交俯视相机、广场/道路、家园 GameMode 及 authored GameData 引用齐全。
- 七座建筑使用不同颜色的方块、中文名称牌、用途说明和可升级等级。战备处紧邻大门；鼠标悬停有反馈，底部按钮与数字键可打开对应面板。
- 中文名称使用屏幕空间 UMG，避免 TextRender 默认离线字体缺字。占位材质有真实 Color 参数，相机固定曝光，方块不会被名称牌完全遮住。
- 七个面板统一为可缩放布局：左侧列表、右侧色块图标和详情、底部状态与操作。列表/长说明可滚动，关闭时整个面板容器折叠，打开时阻止场景点击。
- 图书馆/英雄之家/军营自动显示首项详情，保留只读范围；英雄技能/特性使用可读说明。单位基础数值和战斗一致，合并代码默认身份及 DT_Units 数值覆盖。
- 战备处分为“出征牌组 / 出征英雄”两页。已选牌显示 √，英雄槽可选择并交换；草稿离开确认的三个按钮可用，保存/丢弃后继续前往原先点选建筑。重复打开同一建筑不会重置草稿。
- 修复状态刷新覆盖操作结果的问题，保存、升级及放弃反馈会保留。保存/出征/升级由原有 GameMode/Subsystem 校验和提交，UI 不自行扣钱。
- 奖励页与路线页增加“暂回家园”，保存当前安全点后返回；待选奖励和路线保留。终态恢复页也返回家园。部署/战斗中拒绝回家，切图已排队后重复点击无效，同时拒绝领取/跳过奖励或再选节点，避免过渡期间改变已保存安全点。
- 家园冷启动先加载已有 Run，再显示继续/结算状态。大门可“继续远征”或“放弃本轮”；放弃必须在游戏内确认，收益为 0，保存失败会恢复原内存状态。
- 修复旧 `WBP_BattleHUD` 部署按钮写死英雄身份的问题：三个按钮在运行时按 `AvailableHeroes` 顺序绑定名称及实际部署 ID，已部署英雄禁用。没有重存用户维护的 Widget Blueprint。

### 本批文件归属

| 路径/范围 | 本批处理 | 后续协作说明 |
|---|---|---|
| `Content/Maps/L_Home.umap` | 新增并保存 | Codex 助手制作，正式家园入口 |
| `Content/blueprint/Home/BP_HomeGameMode.uasset` | 新增并绑定 DA_GameData | 家园自有 BP；不要改为无配置的原生 GameMode |
| `Content/Materials/Home/M_HomePlaceholder.uasset`、五个 `MI_Home*` | 新增 | 家园专用色块/道路材质 |
| `Scripts/CreateHomeAssets.py` / `InspectHomeAssets.py` | 新增 | UE 5.8 创建/只读检查，生成器保留已有布局并迁移自身旧相机设置 |
| `Config/DefaultEngine.ini` | 修改 | GameDefaultMap / EditorStartupMap → L_Home |
| `ALKHomeGameMode.*` / `ALKHomePlayerController.*` | 在已有 H 实现上修改 | 合并数值、冷启动 Run、面板文案、放弃入口、调试类、遮罩后悬停控制 |
| `ALKHomeBuildingActor.*` / `ULKHomeBuildingLabelWidget.*` / `LKHomeUIStyle.h` | 修改/新增 | 场景方块、可点击范围、中文名称牌和共享显示配色 |
| `ULKHomeHUDWidget.*` / `ULKHomeListButtonWidget.*` / `LKHomeTypes.h` | 修改 | 自适应面板、详情、页签、英雄槽索引、草稿确认、操作结果 |
| `ULKRunRewardWidget.*` / `ULKRunNodeSelectWidget.*` / `ULKRunResumeWidget.cpp` | 修改 | 安全点回家和终态返回入口 |
| `ALKBattleGameMode.h` / `ALKBattleGameMode_Run.cpp` | 修改 | 公共回家命令，阶段/保存/重复点击检查 |
| `ULKBattleHUDWidget.*` / `ALKPlayerController.h` | 修改 | 复用既有三个部署按钮，读取本轮队伍顺序；当前待部署 HeroId 可查询 |
| `ULKRunSubsystem.cpp` | 修改 | 放弃前快照，写盘失败时回滚，避免界面误报已放弃 |
| `Tests/LKHomeUITests.cpp` | 新增三组 | 家园资产、真实按钮交互、战斗部署顺序与安全点回家 |
| README、01、02、03、26、27、28、`docs/validation/HomeUI-Validation.json`、`docs/images/Home*.png` | 更新/新增 | 同步当前入口和交付状态，保留历史验证，教程使用实机截图 |

本批**未重存**：`DA_GameData`、`DT_Units`、`DT_Traits`、`DT_Encounters`、三个 GA、`WBP_BattleHUD`、`L_BattleTest`。工作区里此前 H0～H5 未提交的文件仍保留，不能把全部差异算成本批新增。

### 本批验证与边界

- UE **5.8.1-56057345**：`little_kingEditor Win64 Development` 构建成功（`Saved/Logs/HomeUIBuild11.log`）。
- 全部 **47/47** 自动化成功，**0 failed / 0 notRun**；20 项无警告，27 项带测试世界/回退路径等警告。完整报告 `Saved/Automation/HomeUIFinal3/index.json`，摘要 [HomeUI-Validation](validation/HomeUI-Validation.json)。
- 新增三组：`HomeUI.AuthoredAssetsAndContent`、`HomeUI.DraftConfirmationAndDetails`、`HomeUI.RosterButtonsAndSafeReturn`。最后一组实际触发旧 BP 按钮，检查选择的 HeroId，并触发奖励页回家按钮、验证保留奖励批次与 RunId。
- 六个项目 Blueprint 编译，0 错误、0 警告、0 载入失败；包含新 BP_HomeGameMode 与原五个 Blueprint。只读编译不重存这些资产。
- UE D3D12 **RenderOffscreen** 实际渲染 1280×720、1920×1080，各八张（场景和七面板）。修正了地图边缘空黑、曝光、名称牌遮住方块、底部提示重叠和详情过大；示例存于 `docs/images`，全部截图位于 `Saved/Screenshots/HomeUI`。
- 截图模式 `-HomeUIPreview` 仅 Editor 构建启用，使用随机命名的独立永久档，完成后清理；演示 300 金币不会写入正式 Profile。本轮回归前后正式 SaveGames 内容一致（本次基线为空）。
- 自动化覆盖真实按钮回调及排队切图的状态，截图覆盖家园像素布局；**未宣称已替用户用鼠标完整手打一次往返远征**。体验确认步骤见 [28 第 6 节](28-HomeTutorial.md#6-需要你亲自做的部分确认实际操作手感)。
- 没有必须由用户补做的地图、蓝图引用或素材制作。最终美术/音效、打包和硬件测试继续后置；本批不涉及新发布标签。

## 2026-09-10 · BUG-019 修复：安全点"暂回家园"后继续远征，战斗不开始

用户反馈：一间房打完后点"暂离"回家，再次"继续远征"，双方单位面对面却不攻击。

| 项目 | 内容 |
|---|---|
| 取证 | 用户日志里那次失败有 `[Run] 加载第 2/4 战` 与三名英雄的部署，但**没有 `[Battle] 阶段切换 -> 1`** → 战斗阶段从未开始（单位因此保持冻结）。原因是 `SetPhase` 在 `CanStartBattle()` 为假时静默 return，既不提示也不报错 |
| 修复 1 | `SetPhase` 拒绝开战时打印完整判据（阶段/玩家状态/牌库/双方名单与部署数）；`RequestStartBattle` 每次请求都记一行，能确认"点击是否到达 C++" |
| 修复 2 | 原生兜底：**空格 / 回车**也能开始战斗（不依赖蓝图按钮状态），原生 HUD 提示同步说明 |
| 修复 3（真实缺陷） | 远征进行中但当前不是开战点时（例如在**奖励面板**点暂离后继续），原先会静默退化成独立单场假战斗 → 现在进入"恢复中枢"显示奖励/路线面板；恢复中枢/终态世界禁止部署英雄 |
| 测试 | 新增 `LittleKing.H3.Resume.SafePointLeaveAndContinue`（完整复现该路径：打一房 → 安全点保存 → 新会话载入 → 战斗地图 → 部署 → 开始；断言进入 Battle 阶段且贴脸后双方互相掉血）；全量 `LittleKing` **48/48** |
| 结论 | 自动化证明"继续"链路与战斗状态机本身正确；若再次出现，日志里的 `[Battle] 无法进入战斗阶段…` 会直接指出被哪条判据挡住 |
| 文件 | `ALKBattleGameMode.h/.cpp`、`ALKPlayerController.cpp`、`ALKPresentationHUD.cpp`、`Tests/LKHomeTests.cpp`、`docs/05-BugLog.md`（BUG-019） |
| 存档影响 | 无（不改存档结构、不改数值） |
