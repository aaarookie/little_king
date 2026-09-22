# 任务清单 · 当前与历史

## v0.8.1：战斗显示修复（2026-09-22～23）

- [x] 工程版本 `0.8.1`、中英文 README 与 [47 发布说明](47-v0.8.1ReleaseNotes.md) 同步；发布目标为 `main` 与附注标签 `v0.8.1`。

- [x] 营地/角色/敌人以稳定脚底位置分层，关闭精灵投影，修复重叠深度竞争。
- [x] 主动移动决定左右朝向，站定面向攻击目标；纯上下保持左右朝向。
- [x] 24 个双足单位更新步行帧，累计主动距离推进；短暂停步不重置步态，关闭战场运动模糊。
- [x] 24 原图、96 新帧、144 独立引擎资源及完整提示词归档；旧 D 批可独立重导。
- [x] UE 5.8.1 构建、87/87 全量回归、720p/1080p 真实画面及原五存档哈希核对：见 [45](45-BattlePresentationFixes.md) 与 [验证摘要](validation/MovementFix-Validation.json)。

## v0.8 发布（2026-09-22）

- [x] A～D 批代码、775 个引擎素材、源文件、许可及验证记录统一纳入版本。
- [x] 工程 `ProjectVersion=0.8.0`，新增 [英文 README](../README.en.md)，中英文互相链接。
- [x] [发布说明](44-v0.8ReleaseNotes.md) 与 [发布验证](validation/v0.8-Release.json)；Git 标签 `v0.8` 对应本次发布提交。

## 已完成：素材制作 D 批（2026-09-21～22）

- [x] 28 组动画、448 姿势、140 动作，控制/失能/复活与旧图回退。
- [x] 本地 Music3 四首循环音乐，跨场景淡化与两声部并发上限。
- [x] 神像 Lv2～4、金库 Lv2～5 七张图，保留点击与升级规则。
- [x] 中文正文/标题字体、许可、菜单署名，转场遮罩和 UI 淡入。
- [x] 639 引擎资源、导入与来源审计、83/83 自动化及真实视口验证。
- [x] [42 协作记录与教程](42-PolishArtIntegration.md)、[43 实际提示词](43-PolishArtPrompts.md)、[验证摘要](validation/PolishArt-Validation.json)。

## 已完成：素材制作 C 批（2026-09-21）

- [x] 16 张图片：五区地面、三营地、五节点徽记、火球/叶片/尘点三 FX 零件；28 个原创合成声音，共 52 个引擎资产。
- [x] 固定战场曝光、场外暗底与边界，保持中线、血条和物理营地；地图纹理等比裁切及节点/出入口显示。
- [x] 战斗 HUD 主题与部署头像/文字、技能和控制状态反馈、银币/指令/服务/升级提示。
- [x] 短时 FX 队列上限与过期清理，音频节流与并发限制，保留自定义 SoundMap/显式静音。
- [x] UE 5.8.1 最终编译、79/79 全量回归、52 资产重复导入、720p/1080p 真实画面、五个玩家存档哈希检查。
- [x] [协作记录及教程 40](40-WorldSkillsArtIntegration.md)、[完整提示词 41](41-WorldArtPrompts.md)、[验证摘要](validation/WorldArt-Validation.json)。

## 已完成：素材制作 B 批（2026-09-20）

- [x] 制作并接入 20 个透明战斗单帧、17 张同设计卡面，共 57 个引擎资产。
- [x] 复核 8 个旧精灵，修正战斗兵营纵向拉伸；单位碰撞与玩法数值不变。
- [x] 缺图默认绑定、保留自定义图片、避免污染共享卡牌资产。
- [x] 手牌等比显示与文字分区，奖励插画扩大到 128px。
- [x] 最终构建成功、76/76 全量回归、57 资源重复导入、720p/1080p 画面检查；5 个玩家存档哈希不变。
- [x] 完整提示词、来源哈希、自动导入、UE 5.8 教程与验证见 [38](38-BattleArtIntegration.md) / [39](39-BattleArtPrompts.md)。

## 已完成：素材制作 A 批（2026-09-19）

- [x] 重新盘点 28 个战斗实体、24 张卡牌、七建筑、地图/UI/FX/声音，更新 [07](07-ArtStyleGuide.md)、[12](12-AssetRequest.md)、[13](13_Asset_Solutions.md)。
- [x] 制作菜单插画与七张透明建筑图，整合共用松绿/旧金/羊皮纸 UI。
- [x] 接入八个战斗音效键及按钮声；6 条 Kenney CC0、3 条原创合成短句。
- [x] 保存原图/原始 OGG/WAV、许可、来源/哈希、完整提示词和可复现导入脚本；[来源台账](36-ArtAudioSources.md)。
- [x] UE 5.8.1 编译成功、74/74 全量回归、最终菜单 1/1 复测、720p/1080p 实际画面检查、27 资源重复导入；原有 5 个玩家存档哈希不变。
- [x] [协作记录与截图](37-ArtAudioIntegration.md)、[验证摘要](validation/StorybookAssets-Validation.json)；本批无需用户手动下载。
- [x] B 批已完成，见上方独立记录。
- [x] D 批已完成，见上方独立记录。Sprint 6 仍跳过，打包与硬件测试后置。



## 已完成：阶段三扩展 · 巨魔与攻城卡（2026-09-16～17）

- [x] 先编写 [新增卡牌代码接口](34-CardDevelopmentInterfaces.md) 与 [七卡规则数值](35-TrollAndSiegeCards.md)。
- [x] 七卡运行时注册、远征奖励、品质/种族/占格/建筑分类。
- [x] 八格加权预算、多选旧卡原子替换、四槽唯一满手、Schema 8。
- [x] 巨魔强化与自眩晕、王的支援和第六击、共享冰火吐息与点燃、全图仅建筑炮击。
- [x] UI 占格、选择/确认、状态文字、全图建筑范围预览；金库费用可达。
- [x] UE 5.8.1 编译，72/72 全量回归及最终 UI 1/1 复测；720p/1080p 截图、教程与协作记录收尾。见 [验证摘要](validation/TrollCards-Validation.json)。


## 已完成：优化阶段三首批卡（2026-09-16）

- [x] 先更新角色图鉴与初版品质/职能数值系统，明确品质与基础档位的例外。
- [x] 五档角色品质、种族、14 档法术等级及原生身份兜底。
- [x] 手牌、家园详情、奖励与替换 UI 分类显示；长技能说明可滚动，缺美术用色块文字。
- [x] 8 张部队卡组上限、满额显式替换、四槽不重复即时补牌。
- [x] 八种临时佣兵原生注册、奖励池接入及全部主动/被动技能。
- [x] 领卡/替换/跳过/裁减的保存失败回滚，Schema 7 与超额旧档手动裁减。
- [x] UE 5.8 新手试玩、调参和旧档说明：[33](33-Stage3CharactersTutorial.md)。没有必须手工补建的资产。
- [x] UE 5.8.1 完整编译、65/65 自动化回归；720p 手牌及 720p/1080p 奖励、替换界面截图复核。见 [验证摘要](validation/Optimization3-Validation.json)。

## 前阶段：优化阶段二（2026-09-15）

实现与兼容记录见 [29](29-OptimizationChangeLog.md)，UE 5.8 操作见 [31](31-WorldMapExpeditionTutorial.md)。

- [x] 固定五区域地理、固定出发点、多入口/出口与相邻前区汇合。
- [x] 每轮新种子，各区 10～20 节点，内部 DAG、无孤岛/死路，完整图随档保留。
- [x] 普通 1 英雄、精英 2 英雄、Boss + 2 英雄；市场停留与钱包、休息恢复及扩展接口。
- [x] 全图与区域放大、节点详情、任意数量候选、确认前往、市场/休息按钮。
- [x] 大门确认英雄/牌组/金币后直接出发，金库携带上限；成功/失败/主动结束全额带回。
- [x] 本金预留/退款、交易与终态回执防重复、Schema 6 和旧图兼容。
- [x] UE 5.8.1 构建成功、58/58 回归通过；200 种子与 12 条受控路线、真实按钮/Slate 命中、720p/1080p 截图；玩家存档不变。见 [验证摘要](validation/Optimization2-Validation.json)。

## 前阶段：优化阶段一（2026-09-13）

新增记录见 [29](29-OptimizationChangeLog.md)，操作见 [30](30-StartMenuTutorial.md)，验证见 [Optimization1-Validation](validation/Optimization1-Validation.json)。

- [x] 独立 `L_StartMenu` 与原生开始界面，每次 Play 先选档。
- [x] 继续最近有效档、新建、8 档读取/确认删除、设置接口。
- [x] 每档独立家园与远征持久化、旧档 01 兼容、满额/坏档/中断删除保护。
- [x] `show me the money [数量]` 控制台命令，默认 100 金币，写入当前档。
- [x] 新增协作记录、UE 5.8 操作教程与当前文档同步。
- [x] 52 项完整回归通过，720p/1080p 菜单截图检查，原玩家存档哈希不变。

## 前阶段：地牢 D0～D5 + 家园 H0～H5 场景与 UI 已接通（2026-09-10）

地牢交付见 [18](18-DungeonChangeLog.md)：三选一奖励、D4 分层路线/休息、D5 安全点存档与恢复均已完成。当前路线有四个推进层，因休息选择实际战斗数为 2～4。**以上地牢、家园、开始界面、五区域远征、优化阶段一/二/三与巨魔攻城卡已统一随 v0.7 发布**（构建成功 + `LittleKing` 72/72 全量回归 0 失败）。

家园（阶段 3）按 [26 规划](26-HomeDevelopmentPlan.md) 实施，实际改动登记 [27](27-HomeChangeLog.md)。**H0～H5 代码、家园资产及 UI 接入已完成**，本轮验证见 [HomeUI-Validation](validation/HomeUI-Validation.json)：

- [x] H0：永久进度、出征快照、配置与旧档迁移契约（`LKHomeTypes` / `LKHomeContent` / `FLKExpeditionStartRequest`）。
- [x] H1：家园世界、七座占位建筑、永久档（A/B 双槽 + Revision）与原生 HUD 基础。
- [x] H2：图书馆/英雄之家/军营只读详情 + 战备草稿/校验/保存。
- [x] H3：大门区域地图、出征快照冻结、继续远征与"返回家园"。
- [x] H4：金币暂存/终态冻结/幂等入账 + 神像与金库升级及实际生效。
- [x] H5：Schema 4→5 迁移、坏档保护、七组自动化、调试命令。
- [x] 家园 UI 收尾：实际 `L_Home`、相机/道路/材质、BP GameMode 绑定、默认家园入口、中文名称牌与七座彩色方块建筑。
- [x] 收藏详情、战备英雄/牌组分页、未保存确认、升级反馈；部署按钮跟随本轮队伍顺序。
- [x] 奖励/路线页暂回家园，终态回家，大门继续/放弃确认；重复返回和放弃写盘失败保护。
- [x] 原生按钮集成测试、实际分辨率截图、协作记录及 UE 5.8 试玩教程。
- [ ] 用户体验确认：按 [28 第 6 节](28-HomeTutorial.md#6-需要你亲自做的部分确认实际操作手感) 感受鼠标命中、文字大小及完整往返；无需制作缺失资产。

## 上一批：Sprint 5 · 已发布 v0.5

本轮所有规则、源码、测试和资产迁移统一计入 Sprint 5。可执行清单见 [10](10-Sprint5Guide.md)，最新验证结果与协作范围见 [15](15-Sprint5ChangeLog.md)，编辑器操作见 [16](16-Sprint5MigrationTutorial.md)。当前规则见 [01](01-GDD.md)。

- 源码：全部三英雄无限部署、实体营地与指挥、满手队列循环、攻击建筑射程预览、常显半场黄线、全场施法/近战佣兵嘲讽特性、头顶条与火球小震。
- 规则质量：P0/P1 修复、实际战斗事件与结算统计、路径/弹道、出牌失败退款、种子拆分和 AI 可读反馈。
- 验证：引擎内自动化与构建结果按 15 记录；最新 10 组逻辑测试通过；五个项目蓝图编译无错误/警告；用户反馈已做数次基础试玩，新增画面用短 PIE 检查。
- 文档：README、GDD、技术设计与 Sprint 5 已统一；Sprint 6 删除，新增 17 地牢详细规划。音效、表现素材、打包和硬件测试后置。

## 历史记录（Sprint 0~5 原计划）

以下保留早期任务、提交和验收记录用于追溯，不能据此判定新的 Sprint 5 已全部完成。法术门、旧团队血条、自动开战和旧震动规则已撤销。任何“本会话”“我/你”“下次提交”都仅描述当时工作，不是新的协作指令；发布操作应以当前协作安排为准。


**配套文档**：[01-GDD.md](01-GDD.md) · [02-BattlePrototypeDesign.md](02-BattlePrototypeDesign.md) · 项目：`little_king`（UE5.8，引擎 `E:\epic\UE_5.8`）

---

## ✅ 已完成（本会话）

- [x] GDD v0.2 定稿（国王不出征 / 无平局虚弱机制 / 法术门 / 建筑上限规则）
- [x] 技术设计 v0.2 更新（含 UE5.8 技术事实：GAS 变插件、SetByCaller 用 FName 等）
- [x] **C++ 骨架 19 个文件**写入 `Source/little_king/`：单位基类/英雄/建筑/弹道、GAS 属性集与伤害管线、银币/牌库/卡牌、GameMode 阶段机（部署→战斗→结算+超时虚弱）、敌方 AI（波次+打牌）、PlayerController 命令接口、数据表行结构与全局配置
- [x] `.uproject` 启用 Paper2D + GameplayAbilities 插件；`Build.cs` 依赖补全；`DefaultGame.ini` 指定 GameMode/PlayerController
- [x] UBT 命令行编译验证（little_kingEditor Win64 Development）——**通过**

### 📌 编译修复记录（UE5.8 API 备忘，面试可讲）

| 坑 | 5.8 写法 |
|---|---|
| `ATTRIBUTE_ACCESSORS` 未定义 | 5.8 内置宏叫 **`ATTRIBUTE_ACCESSORS_BASIC`**（旧名只是注释示例） |
| `FGameplayEffectDurationMagnitude` 不存在 | 5.8 中 `DurationMagnitude` 类型改为 **`FGameplayEffectModifierMagnitude`** |
| `FGameplayEffectModCallbackData` 未定义 | 定义在 **`GameplayEffectExtension.h`** |
| `TWeakObjectPtr` 不能 `!ptr` 判空 | 用 `ptr.IsValid()`（`operator bool` 被 delete） |
| `AddAttributeSetSubobject(TObjectPtr)` 推导失败 | 传裸指针 `AddAttributeSetSubobject(UnitAttributes.Get())` |
| 局部变量 `Tags` 遮蔽 `AActor::Tags` | 改名（C4458 警告） |
| `Engine/EngineUtils.h` 不存在 | 5.8 中 `TActorIterator` 直接 `#include "EngineUtils.h"` |
| 编辑器开着时外部编译被拒 | UBT 报 "Unable to build while Live Coding is active"：**外部编译前先关编辑器**（或编辑器内按 Ctrl+Alt+F11 用 Live Coding） |
| **`GlobalDefaultGameMode` 不生效（PIE 用默认 GameModeBase）** | `UGameMapsSettings` 是 **`config=Engine`** → 该段必须写在 **`DefaultEngine.ini`**；写在 `DefaultGame.ini` 会被静默忽略（日志特征：`LogLoad: Game class is 'GameModeBase'`） |

### 🔭 视角接管说明（已修复）

- 现象：PIE 视角停在 (0,0,0)，不用场景相机
- 原因：引擎的 `bAutoManageActiveCameraTarget` 只在 `SetPawn`/`ClientRestart`/观战等路径触发；本项目 `DefaultPawnClass=nullptr`，从不 SetPawn → 自动接管永不执行
- 修复：`ALKPlayerController::BeginPlay` → `FindAndUseLevelCamera()` 手动接管场景中第一个 `CameraActor`（`TActorIterator<ACameraActor>`），所有关卡通用
- 注意：PIE 时日志应出现 `[Battle] 视角接管场景相机 XXX`



## 🔧 Sprint 0：把骨架跑起来（编辑器操作，约 2~3 天）

> 骨架**零资产可运行**：没有数据表/精灵时会回退内置默认值 + 调试色块（绿=玩家/红=敌方）。

- [x] **D0**：用 VS 打开 `little_king.sln`，编译运行 → 启动 UE 编辑器（首次加载会自动编译 C++）
- [x] **D0**：新建地图 `L_BattleTest`（基础关卡），保存到 `Content/Maps/`
- [x] **D0**：放一个正交相机（**规范：Location (0,0,2000)，Rotation (Pitch -90, Yaw 0, Roll 0)，Ortho Width ≈ 4800**）
- [x] **D0**：PIE 测试通过——战场框/中线/自动开战/波次出兵/自动互殴均正常（绿=玩家/红=敌方色块）
- [x] **D0**：战场左右分修复——分界轴改为世界 Y，中线沿 X；玩家左（Y<0）/敌方右（Y>0）
- [x] **D1**：创建 `DT_Units`（8 行：3 英雄+3 佣兵+2 建筑，行结构 `FLKUnitRow`）
- [x] **D1**：创建 `DA_GameData`（`ULKGameData`），UnitTable 指向 DT_Units
- [x] **D1**：创建 `BP_ALKBattleGameMode`（父类 ALKBattleGameMode），类默认值 GameData = DA_GameData
- [x] **D1**：World Settings → GameMode Override = BP_ALKBattleGameMode；PIE 复测正常
- [ ] **D1**：导入占位贴图 → 创建 Paper Sprite → 填 DT_Units 的 Sprite 字段（可选，色块可继续用）
- [ ] **D2**：创建 `DT_Waves`（敌方波次）与 `DT_Skills` / `DT_Traits`（可后置）
- [x] **验收**：完整对局（部署→战斗→胜负/虚弱）无崩溃，日志正常

## ⚔️ Sprint 1：从"看"到"玩"——最小可操作对局（1 周）

- [x] **C++ 地基**（已编译）：`ALKPlayerController` 放置状态机（左键放置/右键取消）+ 鼠标光标/输入模式；`ULKBattleHUDWidget` 事件基类（阶段/手牌/银币/胜负/放置/出牌结果 6 事件）；GameMode 自动创建 HUD（`HUDWidgetClass`）
- [x] **按 [04-HUDTutorial.md](04-HUDTutorial.md) 搭 WBP_BattleHUD**：部署面板（3 英雄按钮+开始）、HUDMain（银币+4 卡槽）、结算面板、提示文本
- [x] BP_ALKBattleGameMode 类默认值 → HUDWidgetClass = WBP_BattleHUD
- [x] 验证：可部署英雄、可打牌、银币增长、右键取消、胜负结算+重开（PIE 可玩）
- [x] 单位 FSM 走查完成，修复 4 个问题（详见 [05-BugLog.md](05-BugLog.md)：哨塔目标卡死 / 射程抖动滞回 / 战场边界钳制 / 攻击已死目标）+ 新增索敌线调试
- [x] **Git 首次提交并推送 GitHub**：git init + .gitignore + 首次 commit（2dea2c6，206 文件）+ tag **v0.1** + push 到 https://github.com/aaarookie/little_king（走本机代理 127.0.0.1:18081，已写入仓库 git config）
- [ ] 每 Sprint 结束：Git tag + 3 行技术复盘（**下次提交前：git add -A → commit → push（代理开着时）**）

## 🪙 Sprint 2：经济与卡牌完整化 —— ✅ 完成（commit de5b00c，tag v0.2）

**教程**：[06-Sprint2Guide.md](06-Sprint2Guide.md)（C++ 已完成说明 + A 卡牌图标 / B 单位精灵 / C 法术门 UI / D 波次表 四个教程）

- [x] **C++（已编译）**：调试命令 `ULKCheatManager`（7 条命令，PIE 按 `~` 使用；5.8 中 CheatClass 在 PlayerController 上；SpawnUnit 半场自动镜像）；`OnSpellLockChanged` 事件；`OnHandChanged` 3 参数（+bPlayable）；`GetCardDefinition`/`GetCardIcon`；手牌原地补牌（BUG-010）；精灵朝向（BUG-011）+ SpriteScale 组件缩放（BUG-012）；单位行缺失自诊断；脚下阵营色环 `bDrawTeamRing`；ForceEndMatch/FindCard BP 可调
- [x] **教程 A**：卡牌图标（7 张卡资产 + CardLibrary + HUD 卡槽图标显示）
- [x] **教程 B**：单位精灵（抠图流程 + Paper Sprite + DT_Units.Sprite；佣兵行已核对）
- [x] **教程 C**：法术门锁定 UI（bPlayable 卡槽变灰）
- [x] **教程 D**：DT_Waves 波次表
- [x] 卡牌循环验证（原地补牌）+ 建筑行为验证
- [x] Git 提交 Sprint 2（tag v0.2）

## 🦸 Sprint 3：英雄技能与对局反馈（完成 ✅，tag v0.3）

**指南**：[08-Sprint3Guide.md](08-Sprint3Guide.md)（分工 + C++ 设计 + 新手教程 A~F）

- [x] **C++（我）**：`ULKGameplayLibrary`（BP 技能用范围伤害/治疗/索敌/读血）→ `HeroAbilityMap`（FLKHeroSkillEntry）+ 英雄技能授予与简单冷却 + `FLKUnitRow::SkillCooldown` → `GetTeamHeroHealthRatio` + `GetTarget` BP 暴露 + 原生注册标签 `LK.Ability` → 编译通过（含 2 次修复：UHT 嵌套容器、漏包含头文件）
- [x] **C++（我，追加）**：通用**无敌**状态 `SetInvulnerable`（英雄/佣兵/建筑，免疫普通伤害、虚弱真伤穿透、金色标识）+ 控制台 `InvulnerableHeroes 秒数`（教程 F）→ 编译通过
- [x] **教程 A~B（你）**：创建 GA_KnightHeal / GA_MageNova / GA_RangerShot（父类 UGameplayAbility，**AssetTags 必须 = LK.Ability**，连 LK_ApplyHealInRadius/LK_ApplyDamageInRadius + End Ability）→ DA_GameData.HeroAbilityMap 挂给 3 英雄
- [x] **教程 C（你）**：顶部双方英雄血条（PlayerHeroBar/EnemyHeroBar + Event Tick + Set Percent，每帧读 GetTeamHeroHealthRatio）
- [x] **教程 D（你）**：DT_Units 填 SkillCooldown（8/6/5）+ 技能数值 + BattleTimeLimit=60 快速验证超时虚弱手感
- [x] 教程 F：无敌调试（InvulnerableHeroes）验证"虚弱穿透无敌"
- [x] 验收 + BugLog（BUG-014：虚弱结算遍历快照修复）+ Git 提交（tag v0.3）

## 🤖 Sprint 4：敌方 AI 与内容（完成 ✅，tag v0.4）

**指南**：[09-Sprint4Guide.md](09-Sprint4Guide.md)（AI 集火/反制/爆发 + DT_Traits 特性 + 首次平衡）

- [x] **C++（我）**：AI 集火指令（SetForcedTarget 优先级索敌 + 周期性集火血量最低英雄）／反制评估选牌／爆发一波流／法术智能目标；特性运行时（自身修饰 + ULKTraitAuraComponent 光环进出圈 + Taunt 嘲讽标记）；`MaxUnitsPerTeam` 上限（打牌 UnitLimitReached + 出兵拦截）；调试可视化（青色集火线、ListUnits 标记）；AI 参数集中 DA_GameData → AI 分类 → 编译通过
- [x] **教程 A（你）**：DT_Traits 建表（行名 Taunt 踩坑已记录在 09 文档 + 风险表）+ DA_GameData.TraitTable + DT_Units.HeroTraits
- [x] **教程 B（你）**：AI 手感验证（集火/反制/爆发日志与调试线观察）
- [x] **教程 C（你，持续项）**：首次平衡——银币节奏调整（上限 5 / 每 3 秒 1 个，AI 爆发门槛联动降到 4）；3 轮记录可继续追加
- [x] 验收清单 + Git 提交（tag v0.4）

## Sprint 5 历史计划的覆盖关系

早期“飘字资产、所有死亡震屏、先接音效再验收”的安排已由当前 Sprint 5 规则替代。实际代码、资产协作和验证结果见 [15](15-Sprint5ChangeLog.md)，避免按旧清单重做已删除的团队血条或法术锁定。

## 地牢任务状态

详细任务、依赖、交付标准与分工见 [17](17-DungeonDevelopmentPlan.md)。

- [x] D0：规则基线、状态所有权、值类型与战斗接入契约；稳定内容 ID/资产归属。
- [x] 本轮内容预备：失能保留/战后恢复、五亡灵单位、独立被动与特性、三种单场遭遇。
- [x] D1：固定三战串联；结算按钮推进唯一节点，跨地图继承恢复后英雄状态，失败/通关可从头开始。
- [x] D2：原生遭遇迁入 `DT_Encounters`；完整遭遇快照、英雄永久最大生命/特性跨房和三连战第一版平衡已完成。
- [x] D2 修复（BUG-015）：敌方单位状态不跨房——远征房间结算只恢复玩家英雄，敌方每房按遭遇满血重建；部署血量埋点日志。详见 05 BUG-015。
- [x] D3：三选一奖励——升级已有单位/建筑卡（攻击/生命 +10%/级）或获得骷髅兵/骷髅射手新卡；批次生成存定、原子领取/跳过并防重复。
- [x] D3 UI：新增原生 `ULKRunRewardWidget`，自动接入现有战斗 HUD；三张奖励卡、真实前后数值、图标/文字占位、牌组摘要、响应式缩放和跳过均已完成。
- [x] D4：分层路线图（普通 → 普通/休息 → 精英/休息 → Boss），每排打完领奖后只显示下一排可选节点；普通房随机 1 敌方英雄、精英随机 2、首领房 1 首领 + 2（目录驱动池、确定性种子）；休息节点回血 30% 唯一选项；原生 `ULKRunNodeSelectWidget` 面板；`LittleKing` 29/29。
- [x] D4（你）：PIE 手工验收通过。
- [x] D5：安全节点存档（固定槽自动保存点 + 版本/内容校验拒绝损坏档）、冷启动恢复（战斗中退出回到开战前 / 路线奖励中断回选择界面 / 终态摘要）、防重复读档领奖；原生 `ULKRunResumeWidget`；`LittleKing` 32/32。
- [ ] D5（你）：按 [25](25-D5SaveTutorial.md) 第 3 节 PIE 验收（继续/战斗中退出/终态/损坏档）。
- [ ] 阶段 3 评估：家园局外循环或商店/事件内容扩充（17 第 9 节）。

原 Sprint 6 整段删除，11 指南删除，不再安排对应验收或 v0.6 标签。地牢后再评估家园、商店/事件扩充、正式美术音效和发布工作；不以打包、硬件测试阻塞当前开发。
