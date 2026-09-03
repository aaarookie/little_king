# 任务清单（Task List）

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

## 🪙 Sprint 2：经济与卡牌完整化（1 周）

**教程**：[06-Sprint2Guide.md](06-Sprint2Guide.md)（C++ 已完成说明 + A 卡牌图标 / B 单位精灵 / C 法术门 UI / D 波次表 四个教程）

- [x] **C++（已编译）**：调试命令 `ULKCheatManager`（AddSilver/DrawCard/SpawnUnit/KillAll/WinMatch/StartBattle/ListUnits，PIE 按 `~` 使用；5.8 中 CheatClass 在 PlayerController 上）；`OnSpellLockChanged` 事件（开战/法师阵亡广播）；`OnHandChanged` 升级 3 参数（+bPlayable 可打出标记）；`GetCardDefinition` 卡牌查询
- [ ] **教程 A**：卡牌图标（AI 生成 → 导入 → 7 张 `ULKCardDefinition` 资产 → DA_GameData.CardLibrary → HUD 卡槽显示图标）
- [ ] **教程 B**：单位精灵（AI 透明底出图 → Paper Sprite → DT_Units.Sprite）
- [ ] **教程 C**：法术门锁定 UI（用 `bPlayable` 给卡槽变灰）——**先重连 OnHandChanged（3 参数）**
- [ ] **教程 D**：DT_Waves 波次表（9 行，接 DA_GameData.WaveTable）
- [ ] 卡牌循环验证：打牌→入弃牌→洗回（已实现，随 HUD 验证）
- [ ] 建筑：哨塔/兵营行为验证（已实现，补数值与精灵）
- [ ] Git 提交 Sprint 2

## 🦸 Sprint 3：英雄与胜负打磨（1 周）

- [ ] 英雄技能：按 `DT_Skills` 配 GAS Ability（BP 技能 + CooldownGameplayEffect），英雄 AI 施放（`TryCastAbilities` 已接 LK.Ability 标签）
- [ ] 胜负判定端到端验证（含超时虚弱数值手感）
- [ ] 顶部双方英雄血条 UI

## 🤖 Sprint 4：敌方 AI 与内容（1 周）

- [ ] AI 升级：集火英雄指令（TargetOverride）、反制玩家兵种、爆发时机
- [ ] 内容填充：3 英雄 / 6 佣兵 / 2 法术 / 2 建筑数据表完整数值 + 首次平衡
- [ ] 特性系统：`DT_Traits` + 光环 GE（`ApplyAttributeModifier` 已就绪）

## ✨ Sprint 5：体验打磨（1~2 周）

- [ ] 打击感：攻击前摇/停顿/震屏/飘字/粒子（程序动画）
- [ ] 占位音效接入；HUD 全面美化
- [ ] 性能粗查：单位数量上限、对象池（弹道先行）

## ⚖️ Sprint 6：工具与平衡（1 周）

- [ ] 数值平衡 3 连测（每局记录 DPS/时长，日志已带）
- [ ] 修阻塞 Bug、崩溃路径走查
- [ ] 验收：连打 3 局无阻塞问题

---

## 阶段 2~6 大目标（原型验收后）

- **阶段 2 地牢局内循环**：路线节点图、战斗/奖励/事件/商店/Boss 房、局内构筑（接 `AddCardToDeck` 等预留接口）
- **阶段 3 家园局外循环**：家园场景、建筑升级、英雄养成、SaveGame 存档
- **阶段 4 内容扩充** → **阶段 5 美术音乐替换+打磨**（AI 出图流程见 GDD） → **阶段 6 发布**
