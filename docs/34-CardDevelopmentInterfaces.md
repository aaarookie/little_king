# 新增卡牌：代码入口、接口与接入顺序

2026-10-02 v0.8.3：新增九研究法术、十一召唤单位、统一法术执行器、按来源的黑云与飓风状态、治疗职业普攻以及施放后不可抽取的冷却接口。当前完整目录为 41 卡、39 单位；天使只注册单位，不注册佣兵卡。新增接口见本页末尾，变更记录见 [53](53-v0.8.3Changes.md)，素材与教程见 [54](54-v0.8.3Assets.md) / [55](55-v0.8.3Guide.md)。

2026-09-28 新接口：敌方专属卡必须登记 `LKCardRules::FactionOf/IsPlayerObtainable`，不能只从奖励 UI 隐藏。`ULKDeckState::CardAllowed` 保护玩家牌组初始化/添加/转换，GameMode 还执行最终权限校验。持续召唤区用 `ELKSpellEffect::SummonZone`、`FLKSkeletonCircleParams`、`ALKSkeletonCircle`，敌方通过 `CastEnemyTacticalSpell` 扣费，不走普通手牌入口。减速使用 `ApplyAreaSlow/RemoveAreaSlow` 按来源清理；敌方生成倍率由 `LKBalanceRules::InstanceScalesFor` 统一应用一次。完整协作记录和资产同步步骤见 [49](49-BalanceV1Implementation.md)。

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

## v0.8.2：获取来源、研究和休息升级接口

v0.8.2 时注册目录为 32 卡，v0.8.3 扩展为 41 卡。`LKCardRules::IsTemporaryMercenary` 同时检查原生兵种与临时目录，远征新卡仅允许此集合；`IsPlayerObtainable` 拒绝三张敌方专属。攻城投石炮作为建筑不再进入临时佣兵掉落池。

研究目录为 `LKResearchContent::All/Find/EnsureCards`：稳定 CardId、材料类别、法术阶位、价格、最早区域深度、权重、默认数值。运行时定义复制到 GameData 私有目录；既有法术图标被复用，阶位由代码保证，效果数值允许资产覆盖。新增图纸也须注册建筑定义。永久材料存于 Profile，购买后本轮携带于 Run，终态结算以同一 ID 幂等交付；`ULKProfileSubsystem::ResearchCard` 消耗材料并永久解锁，保存失败回滚。

`LKResearchContent::GenerateMarket` 按稳定 CRC + RunSeed 生成后冻结。`ULKRunSubsystem::PurchaseMarketOffer` 在一次存档事务中修改金币、售出回执和牌组／材料；禁止重复买、重复卡、未知材料。不要由 UI 直接修改钱包或材料。

`ULKRunSubsystem::UpgradeAtRest` 仅在未消费休息节点、已有合法法术／建筑、等级匹配时成功，关闭回血选项；`LKCardRules::UpgradeMultiplier` 数值为 `1.1^Lv`。战斗奖励只允许佣兵升级。`HeroHealPercent` 是英雄最大生命的附加治疗比例，普通与英雄的固定治疗项都应用升级系数；学徒模仿沿用被模仿卡的本轮等级。

容量来自冻结的 `DeckCapacityMinimum/Maximum`，默认 8/8；`SetDeckCapacityRule` 可在安全点改变未来规则。`ValidateReplacement` 使用按释放格数／张数的动态规划求固定上限的最小必要方案，区间规则仅检验最终区间和五种牌底线。正式出征至少七种，UI 同步读取当前动态上限。跨房保存和重新加载不回到硬编码八格。

新增技能或界面前先阅读 [50](50-v0.8.2ExpeditionChanges.md)，验证获取权限、回滚、旧档、暂停、等级展示和素材归档，不从旧教程恢复建筑奖励或临时回家入口。

## v0.8.3 法术与召唤单位接口

`LKV083Content::Units` 登记天使的角色身份和默认数值，`ConfigureCard` 登记九法术的规则身份。法术的阶位、书价、市场权重和数值仍通过 `LKResearchContent::All/EnsureCards` 接入研究流程。已有资产先复制成 GameData 私有定义，再恢复规则字段；身份不能因为调表而从治疗职业变成伤害单位，也不能把增援天使放进佣兵奖励池。

`ELKSpellEffect` 追加 `Freeze / Reinforcements / BlackCloud / Lightning / Hurricane / DivineBlessing`，旧枚举序号保持不变。`ELKRace::Angel` 也是末尾追加。新增法术无需生成 DataAsset 才能运行，原生默认可用；研究资格仍由永久档校验，不能把“目录存在”等同于“玩家已解锁”。

| 卡牌字段 | 用途与单位 | 升级行为 |
| --- | --- | --- |
| `EffectDuration` | 冰冻、黑云、飓风持续秒数 | 冰冻和黑云随本轮等级放大；飓风维持五秒 |
| `SpellHalfExtents` | 飓风矩形半边长，厘米，X/Y 对应世界轴 | 范围不随本轮等级扩大 |
| `ForceMoveSpeed` | 飓风朝画面右侧推移速度，厘米/秒 | 乘本轮升级系数 |
| `DrawCooldown` | 施放成功后禁止再次抽取的战斗秒数 | 神圣祝福维持 150 秒，不随升级缩短 |
| `SecondarySpellValue` | 雷电第二击的固定伤害 | 乘本轮升级系数 |
| `MaxHealthDamageFraction` | 雷电第一击目标最大生命比例 | 保持 20%，不能随本轮等级扩大 |
| `SpellValue` | 原伤害/治疗数值；雷电第一击伤害上限 | 雷电上限 200 随升级放大；实际伤害为比例项与上限的较小值 |
| `SecondaryDelay` | 雷电第二击延迟秒数 | 保持 0.2 秒 |
| `bGlobalPlacement` | 法术自身允许全场落点，与法师存活独立 | 增援与祝福由原生身份保证；不允许数值升级改变施法权限 |
| `SummonedUnitIds` | 增援一次召唤的完整稳定 UnitId 清单 | 品质和角色清单不变；出生生命与攻击/治疗量受升级影响 |
| `HeroHealPercent` | 治疗波的英雄附加项；神圣祝福的英雄恢复比例 | 祝福为 20% 乘系数、封顶 100%；非英雄始终恢复 100% 最大生命 |

`SpellRadius` 继续服务圆形范围。祝福是全场效果，允许半径为零，不能被统一的“半径必须正数”校验拒绝。雷电最高和最低按当前绝对生命选择，延迟第二击必须重新选择目标；两击只选敌方非英雄，不因为黑云而禁止地面效果命中。

### 统一施法入口

`LKSpellExecutor::Validate` 返回明确的 `ELKPlayResult`，先检查合法位置、权限、参数与召唤容量。`PlanSummons` 使用确定性的环形候选，在建筑、营地、单位体积与战场边界之外，为完整队伍规划不重叠的位置；验证与失败不消耗战斗随机流。任何一个位置无法找到时拒绝整次召唤，不只生成部分角色。

`LKSpellExecutor::Execute` 负责法术的实际效果，GameMode 普通出牌与学徒法师模仿共用它。真正的手牌施放由 `PlayCardForTeam` 执行权限与费用事务；执行器自身不扣费、不改变手牌。模仿没有手牌出牌事务，也不会给被模仿的卡施加抽取冷却。调用方必须清楚自己是在施放卡牌还是释放单位技能，不能直接绕过外层权限。

`ALKSpellField` 承担黑云、飓风与雷电延迟的世界生命周期。场效只在战斗阶段推进，结算、销毁或换房清理来源状态，不留下跨房定时器。当前黑云作用于双方单位，离开区域即恢复可发现；飓风作用于双方可移动角色，建筑和营地保持固定。飓风方向为世界 `+Y`，与当前相机的画面右侧一致。

```cpp
// 普通卡牌从 GameMode 的事务入口施放，验证失败不会扣费或过牌。
const ELKPlayResult Result = GM->PlayCardForTeam(ELKTeam::Player, HandIndex, Location);

// 场效或单位技能执行器调用示意；GM、Card、Instigator 均需有效。
// bIgnorePlacement=true 仅供明确授权的内部技能与隔离测试使用。
const ELKPlayResult Check = LKSpellExecutor::Validate(GM, Card, Team, Location);
if (Check == ELKPlayResult::Success)
{
    LKSpellExecutor::Execute(GM, Card, Team, Location, Instigator);
}
```

增援生成时把对应法术 ID 传入 `SpawnUnitForTeam` 的 `SourceCardId`，在出生属性副本中应用本轮强化。不要先用普通数值出生、再通过伤害或治疗事件“补成升级值”，否则会误触发被动和统计。

### 新状态与治疗普攻

黑云使用 `ULKUnitStatusComponent::ApplyConcealment/RemoveConcealment`，飓风使用 `ApplyWind/RemoveWind`，每个场效 Actor 的名称作为独立来源 ID。同类场效重叠时只清自己的来源，不能清除另一个仍在生效的场效。`Clear` 和来源过期统一解除状态。

`IsConcealed` 与 `IsTargetable` 分离：隐藏单位仍然存活，也仍能被地面法术影响。单位索敌使用 `CanDiscoverTarget` 与 `RefreshDiscoveredTarget`，常规目标过滤、嘲讽、集火、技能和弹丸追踪共同遵守可发现规则。不要把隐藏实现成无敌或删除碰撞。

`IsWindDriven` 只阻止自主走路，不等同于眩晕或冰冻。`ALKUnitBase::ApplyWindDisplacement` 经 `ULKUnitMovementComponent::MoveWindDelta` 检查建筑、营地与边界，将实际位移积入动画旅行量；原生朝向和步态跟随风的方向。强制风移不能与逆向自动移动抵消，也不能用直接 `SetActorLocation` 穿越建筑。

`FLKUnitRow::bBasicAttackHeals` 是代码保证的职业身份。牧师的 `AttackDamage` 表示单次基础治疗量，其他攻击间隔、射程、前摇仍使用公共普攻管线。治疗单位选择可发现的受伤友军，忽略敌方嘲讽；治疗弹丸用 `ALKProjectile::SetHealingPayload` 固定其友方目标，不拦截为敌人，也不触发普攻伤害类被动。真正恢复仍通过 `LKGameplay::ApplyHeal`，英雄失能后不能在战内救起。

### 冷却与满手过牌

出牌前调用 `ULKDeckState::CanPlayWithCooldown` 验证冷却施放后是否仍能维持四种可用手牌。成功执行法术后，通过 `PlayCard(HandIndex, DrawCooldownSeconds)` 开始冷却，并把第一个未冷却队列项补进原槽；失败不能先设冷却、扣银币或修改队列。

`GetCooldownRemaining(CardId)` 只查询当前战斗的剩余冷却，`AdvanceCooldowns` 由 GameMode 战斗 Tick 推进，暂停不推进。`GetNextCard` 返回第一个未冷却项；暂不可抽的卡仍留在原队列，到期恢复正常 FIFO 资格，既不丢失也不重复。换房 `InitDeck` 清空冷却；退出重试仍从战斗前状态开始，不保存战中冷却。

正式出征至少七种卡。旧兼容的五卡牌组若有一张冷却，仅余四种可用卡；此时打出非冷却卡后可以立即重抽同一张，保留四槽完整且唯一。如果再施放其他冷却卡会不足四种，预校验会拒绝。`TransformCard` 保留旧卡的剩余冷却，避免转化绕过冷却。

### 新素材路径

`LKV083Art::Sprite/Animation/CardIcon` 负责五套图集、九卡图的默认映射，`LKBattleArt` 与 `LKPolishArt` 统一分派。原有 `LKPolishArt::WalkingUnitIds` 只列历史 24 个 MovementFix 覆盖对象；天使移动使用自己的新图集，不必加入这份历史覆盖名单。39 单位共 528 个运行时独立姿态，三品质共享角色图集。

新增声音使用 `LKV083Art::SoundIds/SoundPath/SoundInterval`，GameData 补缺省映射，Presentation 同时允许新旧声音 ID。明确自定义 Sprite/Icon/SoundMap、显式静音和关闭默认声音的选择仍优先。图片原始字节、完整提示词与六条声音参数归档到 `ArtSource/StorybookV1/V083`，引擎资源只写 `/Game/Art/StorybookV1/V083`。
