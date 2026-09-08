# 任务清单 · 当前与历史

## 当前：阶段 2 地牢 D3 已完成（2026-09-08）

本轮交付见 [18](18-DungeonChangeLog.md)，奖励界面与验收见 [23](23-D3RewardTutorial.md)。前两房胜利会弹出原生三选一面板，显示升级真实数值、新卡信息、当前牌组和跳过入口；领取/跳过后才解锁下一关。现有 `WBP_BattleHUD` 无需编辑或重存，后续仍可用 `ULKRunRewardWidget` 蓝图子类换肤。

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
