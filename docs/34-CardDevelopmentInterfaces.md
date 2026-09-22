# 新增卡牌：代码入口、接口与接入顺序

日期：2026-09-16。先于本轮七张新卡实施编写。适用 UE 5.8；沿用“原生代码保证身份，数据表调整数值，缺美术使用色块文字”。本轮具体规则与数值见 [35](35-TrollAndSiegeCards.md)，实施结果归入 [29](29-OptimizationChangeLog.md)。

## 1. 从卡牌到战斗单位

2026-09-20 补充：现有角色已完成 B 批单帧/卡面接入，见 [38](38-BattleArtIntegration.md)。新增素材可登记 `LKBattleArt::Sprite/CardIcon` 的默认映射；数据表 Sprite、卡牌 Icon 非空时优先使用自定义图。制作后保存完整提示词、参考图与哈希，运行 `Scripts/ImportBattleArt.py` 导入；新增目录项时同步更新脚本校验数量和测试，不能只放源 PNG。精灵组件抬高 8cm 防地面遮挡，逻辑根与碰撞留在 Z=0。

`原生内容目录 → ULKGameData::EnsureCardLibrary → 奖励/卡组 → GameMode 出牌 → LKUnitContent 合并 DT_Units → ALKUnitBase::InitUnit → 战斗组件 → 伤害事件 → 结算/存档`。

| 需求 | 文件与接口 | 责任与注意事项 |
| --- | --- | --- |
| 新单位/建筑的默认数值、名字、技能 | `LKExpeditionMercenaryContent::All`，`FLKTemporaryMercenaryDefinition`、`FLKUnitRow` | 当前远征临时卡统一注册在此；CardId 与 UnitId 相同。建筑须明确 UnitClass 与 BuildingBehavior，不能只在名字里写建筑。 |
| 普通永久单位或敌人 | `LKUnitContent`、`LKUndeadContent` | 注册单位不等于解锁卡牌。是否进入家园解锁由 `LKHomeContent` 另行控制。 |
| 新规则类型 | `LKTypes.h` | 品质、种族、主动/被动类型等反射枚举。新增值只追加，避免改变已有序号损坏旧档。 |
| 可调参数 | `LKDataTypes.h` 的 `FLKUnitRow` | 基础生命、攻击、间隔、射程、移速、冷却；为新技能增加必要参数。默认值必须支持没有数据表行。 |
| 内置身份兜底 | `LKUnitContent::MergeAuthoredTuning` | 合并后恢复品质/种族/技能/建筑行为等身份字段，保留表内合法调参数值。不要让复制错的表行改变一个内置角色。 |
| 卡牌定义 | `ULKCardDefinition`、`ULKGameData::EnsureCardLibrary` | 单位/建筑/法术类型、银币、图标、法术等级、临时标记。现有资产先复制到运行时再补全身份，不修改共享资产。 |
| 新卡获取 | `ALKBattleGameMode::BuildRunRewardOffers` | 从临时目录生成候选，排除已持有 ID，使用远征种子、冻结候选。新增卡不必再逐个硬编码奖励。 |
| 部队容量 | `LKCardRules::Slots/Used` | 按 CardId 查询占格数、求总占格。8 是部队容量，4 是手牌数量；不能把所有 `Cards.Num()` 都简单替换。 |
| 领取和替换 | `ULKRunSubsystem::ChooseReward/ChooseRewardReplacingCards` | 校验占格、唯一性和至少五种卡；先构造完整新牌组，保存成功才发布。失败恢复原卡组、强化和奖励批次。 |
| 手牌过牌 | `ULKDeckState::InitDeck/PlayCard/AddCardToDeck/TransformCard` | 四槽一牌一格不变；增加/转换卡牌也须校验部队容量。卡组保留第五种卡用于循环，始终四槽满且无同类重复。 |
| 普通攻击 | `ALKUnitBase::PerformAttack` | 攻击次数型技能在实际出手时结算，前摇被打断不计次；弹丸附带攻击快照，命中时处理效果。 |
| 冷却主动技能 | `ULKUnitActiveComponent::Initialize/TickAbility/TryActivate` | 只在战斗中推进，不在部署自动施法；检查死亡、控制、营地移动、冷却。新增一个类型和明确执行函数。 |
| 事件被动 | `ULKUnitPassiveComponent::ObserveCombatEvent/ObserveDefeat` | 普攻命中、实际治疗、死亡等事件。区分每次出手与每次命中，避免技能/持续伤害递归触发普攻。 |
| 本轮通用状态 | 新增 `ULKUnitStatusComponent` | 眩晕、冰冻、强化、灼烧、目标上的冰火历史。统一计时、清理、查询，不让各张卡分别写一套状态。 |
| 伤害/治疗 | `LKGameplay::ApplyDamage/ApplyHeal` | 统一无敌、减伤、实际数值、击杀、事件批次。最大生命按比例变更不是治疗，不能走 ApplyHeal。 |
| 弹丸命中 | `ALKProjectile::ApplyLaunchParams/Tick/DeactivateToPool` | 新冰火类型在发射时随机并快照，命中目标保存历史。池化回收必须清空所有新状态，不能污染下一发。 |
| 索敌限制 | `ALKUnitBase::CanPursueTarget/FindNearestEnemy/SetTarget` | 只能打建筑应作为统一资格过滤，嘲讽、集火和弹丸拦截也不得绕过。营地始终不可攻击。 |
| 位移 | `ULKUnitMovementComponent::MoveSkillDelta` | 世界单位厘米；3 米等于 300。击退不能推动建筑，不能穿过建筑、营地或场地边界。 |
| UI 文字、分类与详情 | `LKCardPresentation` | 统一中文品质、种族、费用、占格和技能说明。可调技能参数从合并后的单位行读取。 |
| 领取 UI | `ULKRunRewardWidget`、`ULKBattleHUDWidget`、`ALKBattleGameMode_Run` | 超额时选择一张或多张旧卡，显示释放容量和最终容量，最后一次性确认；返回不消费奖励。 |
| 家园与地图 | `ALKHomeGameMode`、`LKHomeContent`、`ULKRunNodeSelectWidget` | 展示“容量 x/8”和卡牌张数；临时卡仍不变成永久解锁。 |
| 存档 | `LKRunTypes`、`ULKRunSubsystem::ValidateStoredRun/MigrateStoredRun/SaveExpeditionToSlot` | 卡组存稳定 ID 与强化。新增内容规则版本应显式迁移，旧档超额让玩家裁减，不能静默删除。战斗临时状态不跨房保存。 |

## 2. 最小接入流程

1. 写明卡牌身份、数值档位、银币、占格、获取途径、技能边界。
2. 在内容目录新增稳定 ID，必要时扩展枚举与行字段；确认原生注册和身份合并。
3. 接入卡库并校验类型，确认无需任何 `.uasset` 就能生成。
4. 优先复用公共战斗接口；新技能至少明确触发时间、目标、死亡/控制/结算清理、伤害来源和是否触发其他被动。
5. 统一更新卡牌详情、手牌提示、奖励与容量校验，确认有路径实际获得和部署。
6. 测试边界与原流程回归，记录修改文件、存档影响、验证报告和新手操作。不得把未执行的测试写为通过。

## 3. 本轮特别容易遗漏的点

- 多格是卡组预算，不是在四张手牌里复制同一张牌。入队或替换必须按总占格计算；多卡删除须原子提交。
- 巨魔王支援效果按“是否至少存在一位存活友方对应单位”判断，不按数量叠加；最后一位死亡才移除。
- 生命上限变化前保存百分比，再同时更新最大生命和当前生命；不触发治疗、伤害、共沐春色或战斗统计。
- 眩晕/冰冻同时阻止 FSM 移动、移动组件、普攻和主动技能；英雄 GAS 自动施法入口也必须检查。
- 双头龙的上次头部类型存放在受击目标，不能放在某一条龙身上。其他普通伤害不清空冰火历史；新 Actor 无历史。
- 巨像费用必须有可达到的银币上限，UI 应明确当前上限不足。全图炮的射程、索敌和弹道都应能到达目标。
- 所有战斗临时效果在死亡/房间重置/战斗结束时清理；不能让结算界面继续掉血。

## 4. 如何验证

自动化放在 `Source/little_king/Tests`，使用独立测试存档。至少覆盖目录与类型、加权容量及多卡失败回滚、强化计时/控制/免疫、生命比例、六次攻击、共享冰火历史/灼烧刷新叠加、仅建筑索敌和最大距离命中。完整引擎构建后运行 `Automation RunTests LittleKing`；界面另外使用渲染模式检查 720p/1080p。测试不替代长期数值平衡试玩。

## 5. 本轮可复用接口

```cpp
// 预算根据稳定 CardId 的原生定义查询；不是手牌槽数。
const int32 CostInSlots = LKCardRules::Slots(TEXT("Unit_Colossus")); // 3
const int32 UsedSlots = LKCardRules::Used(Run->GetRunState().Cards);

// 替换界面仅维护选中的 ID；确认时一次提交完整清单。
const TArray<FName> ReplacedIds = { TEXT("Unit_Swordsman"), TEXT("Unit_Archer") };
const bool bSaved = Run->ChooseRewardReplacingCards(RewardIndex, ReplacedIds);

// 状态由单位拥有，受战斗时钟推进，并在死亡/结算/重置时统一清理。
Target->GetStatusComponent()->Stun(2.f);
Target->GetStatusComponent()->Freeze(1.f);
Caster->GetStatusComponent()->Empower(8.f, 1.3f, .75f);
```

以上是调用示意，调用方需先取得有效的 `Run`、`Target`、`Caster` 和当前奖励索引。`ChooseReward` 的旧单卡签名保留并转入新实现；多卡界面调用新接口。`ULKRunRewardWidget::ConfirmReplacements` 是确认按钮入口，勾选本身不修改远征状态。

`DeckSlots` 与 `bTargetsBuildingsOnly` 属于代码保证的身份字段；表内同名字段不会覆盖内置规则。未注册的新 CardId 默认占 1 格，需要多格时先注册到原生目录。`EmpowerDuration/EmpowerMoveMultiplier/EmpowerIntervalMultiplier` 则是可由数据表调整的技能参数。实际实现已完成，验证与接手说明见 [35](35-TrollAndSiegeCards.md)。

## 6. C 批后的表现接口

新增卡牌的技能表现复用 `ULKPresentationSubsystem::Emit(World, Type, Location, Radius, Origin)` 和 `Sound(World, Id, Location)`；伤害、治疗、眩晕等仍由原战斗接口执行，不能从特效计时反向触发伤害。通用治疗、退场、控制状态已在底层生效处接好，不要在每张新卡里重复播放。新专属声音先加入 `LKWorldArt::SoundIds/SoundInterval` 及导入源目录，再由 GameData 的默认映射补齐；资源覆盖/显式静音继续通过 SoundMap 配置。

表现实例不写入远征存档，不消耗 `GetBattleRandom()`。队列自动限额和过期清理，火球震动仍由 GameMode 的既有释放入口控制。营地/地图/FX 路径、实际绑定表与素材来源见 [40](40-WorldSkillsArtIntegration.md)。
