# 卡牌与战斗扩展接口

基线：v0.8.5，46 单位、48 卡。现行数值、品质与技能结算边界集中在 [内容图鉴](19-ContentCatalog.md)，全局生命周期见 [技术设计](02-BattlePrototypeDesign.md)，工坊格式和运行桥见 [62](62-TerrainAndEncounterDesigner.md)。

## 身份、目录与作者数据

运行链为 `原生目录 → ULKGameData::EnsureCardLibrary → 获取／牌组 → GameMode 出牌事务 → LKUnitContent 合并 DT_Units → ALKUnitBase::InitUnit → 战斗组件 → 事件与结算`。

| 入口 | 契约 |
| --- | --- |
| `LKUnitContent::Units/Find/MergeAuthoredTuning` | 全部内置单位的统一注册与身份校正；合法作者数值保留，未注册 UnitId 可完全数据驱动 |
| `LKUndeadContent` | 亡灵单位与被动默认值 |
| `LKExpeditionMercenaryContent::All` | 十四临时佣兵及历史同目录的攻城投石炮定义；投石炮当前走永久研究，不能单凭所在目录判断获取资格 |
| `LKV083Content::Units/ConfigureCard` | 十一天使单位、九法术身份；天使不注册独立佣兵卡 |
| `LKV084Content::UnitIdForCard/EnsureCards` | 七建筑及显式 CardId → UnitId 映射，未知 CardId 不推断名称 |
| `LKResearchContent::All/Find/EnsureCards` | 十六法术书与八图纸的材料类型、阶位、门槛、价格、权重及默认卡牌 |
| `LKHomeContent` | 初始／永久解锁和战备资格，注册目录不等于玩家已解锁 |
| `LKCardPresentation` | 名称、品质、种族、治疗／攻击、费用、占格及技能详情的统一显示模型 |

`FLKUnitRow` 的 UnitId、类别、品质、种族、职业、技能类型、占格、建筑行为、产出单位等属于内置身份。基础生命、攻击、射程、间隔／前摇、移速、主动冷却、产兵间隔、部分技能参数和 Sprite／SpriteScale 由作者配置。品质参考系数只用于设计，不在运行时再次乘算。

`DeckSlots` 和 `bTargetsBuildingsOnly` 由原生目录保证；未知 CardId 默认占一格。`EmpowerDuration/EmpowerMoveMultiplier/EmpowerIntervalMultiplier` 等为可调参数。墙的合法施工／几何／反弹／范围数值保留，无效值回退对应代码基线；普通墙无反弹，高阶墙不能因旧表漏字段丢失能力。

GameData 与已有卡牌 DataAsset 先复制为运行时私有定义再补身份，不能写回共享资产。CardId 和 UnitId 分别管理：早期佣兵恰好相同，新建筑使用 `Card_* → Unit_*`；`LKUnitContent::Find` 只支持明确登记的卡别名。工坊 `catalog.js`（48 CardId）与 `units.js`（46 UnitId）在注册内容变化时同步。

`LKTypes.h` 反射枚举仅在尾部追加，保持旧序号。存档保存稳定 ID 与本轮等级，临时战斗状态不写档；新增内容不会自动解锁旧档或重抽旧商品。

## 获取、容量与保存事务

敌方专属卡必须通过 `LKCardRules::FactionOf/IsPlayerObtainable` 登记。`ULKDeckState::CardAllowed` 保护玩家初始化／添加／转换，GameMode 再校验权限；仅在 UI 隐藏不构成限制。`IsTemporaryMercenary` 同时检查单位类别和临时目录，新卡奖励只允许十四佣兵集合。

`BuildRunRewardOffers` 根据远征种子生成冻结候选。`ChooseRewardReplacingCards` 一次提交完整弃牌 ID 清单，旧单卡 `ChooseReward` 转入同一实现；`ULKRunRewardWidget::ConfirmReplacements` 是确认入口，勾选只改 UI 草稿。保存失败恢复牌组、强化与奖励批次，新卡不继承旧卡等级。

`LKCardRules::Slots/Used` 计算占格；`DeckCapacityMinimum/Maximum` 从出征快照读取，默认 8/8。`ValidateReplacement` 用按释放格数／张数的动态规划选择固定上限的最少必要方案；动态区间只检验最终区间与五种卡底线。正式出征至少七种。`SetDeckCapacityRule` 可在安全点更新规则，UI 与读档不硬编码八格。

`LKMarketContent::Generate` 为新 Gen2 市场按三类商品生成独立随机流；旧 `LKResearchContent::GenerateMarket` 只服务兼容路径。新店首次进入时按冻结的银币成本／上限、出征时永久研究排除集、当前携带材料和牌组过滤；类别候选耗尽不制造重复商品。`ValidateMarketPurchase` 提供 UI 与提交共用的只读原因；`PurchaseMarketOffer` 原子更新金币、售出回执及牌组／材料／道具，保存失败不发布。

`ULKProfileSubsystem::ResearchCard` 消耗材料并永久解锁，失败回滚。`UpgradeAtRest` 要求当前未消费休息节点、已持有合法法术／建筑和预期等级，成功后关闭回血选项。`UpgradeMultiplier` 为 `1.1^Lv`；战斗奖励只升级佣兵，效果升级字段见下表。

## 普攻、主动技能与状态

| 接口 | 执行边界 |
| --- | --- |
| `ALKUnitBase::PerformAttack` | 实际出手才计次数，前摇取消不计；命中类效果等待真实命中事件 |
| `ULKUnitActiveComponent::Initialize/TickAbility/TryActivate` | 背刺、模仿、冲刺、巨魔强化统一调度；检查阶段、死亡、控制、手动／服务移动和冷却 |
| `ULKUnitPassiveComponent::ObserveCombatEvent/ObserveDefeat` | 消费实际正数变化与批次死亡，区分出手、命中、技能与持续伤害 |
| `ULKUnitStatusComponent` | 眩晕、冰冻、强化、灼烧、巨魔支援及目标冰火历史；单位 Tick 推进、统一 `Clear` |
| `LKGameplay::ApplyDamage/ApplyHeal` | 统一来源、减伤、无敌、实际变化、事件批次与击杀，禁止以直接改 Health 代替 |
| `ALKProjectile::ApplyLaunchParams/DeactivateToPool` | 发射快照和池化回收；附加效果不得污染下一发 |
| `ALKUnitBase::CanPursueTarget/CanDiscoverTarget/SetTarget` | 统一阵营、建筑限定、隐藏及目标合法性，嘲讽／集火不得绕过 |
| `ULKUnitMovementComponent::MoveSkillDelta/MoveWindDelta` | 冲刺、击退、风移共享障碍与边界；固定建筑和高台驻军不被强制推移 |

眩晕／冻结同时阻止自主移动、普攻与主动技能，英雄 GAS 入口也检查控制。巨魔王只免疫眩晕，冰冻仍有效。`BeginAttack/AfterAttack` 记录王的第六击与强化副作用，取消前摇不能积累次数。巨魔支援按存活种类存在性，不按个数叠加；最大生命修饰前保存当前百分比，再同步新最大／当前生命，不触发治疗或伤害。

`ReceiveBreath/Ignite` 把上次冰火头与点燃保存在受击目标；所有双头龙共享该历史，其他伤害不清空。冰火类型在发射时使用战斗随机流并快照，发射者死亡不改变在途弹丸。点燃刷新时保留每秒跳伤进度，持续伤害不再触发吐息。死亡、结算、换房及对象池回收分别清理所属状态。

精灵恢复只监听自身普攻／弹丸实际伤害。共沐春色按精灵实际治疗量复制，`Skill_SharedSpring` ActionId 阻止递归；并列最低血比英雄按稳定实例排序。大盗背刺锁定是目标优先级特例，但隐藏等发现资格仍可解除锁定。

## 公共法术与职业治疗

`LKSpellExecutor::Validate` 返回 `ELKPlayResult`，检查位置、权限、参数及召唤容量。`PlanSummons` 确定性规划整组不重叠位置，考虑建筑、营地、单位和边界；任一成员失败拒绝整组，验证不消耗战斗 RNG。

`Execute` 执行效果，普通卡与学徒模仿共用；真正手牌施放仍经 `PlayCardForTeam` 做费用、权限、过牌和冷却事务。执行器自身不扣费或改手牌；模仿作为单位技能，不给原卡施加抽牌冷却。`bIgnorePlacement` 仅用于明确授权的内部技能／隔离测试，不能绕过普通出牌权限。

| `ULKCardDefinition` 字段 | 含义与本轮升级 |
| --- | --- |
| `SpellValue`、`HeroHealPercent` | 固定伤害／治疗与英雄附加比例乘升级系数；祝福英雄比例封顶 100%，非英雄固定全额恢复 |
| `EffectDuration` | 冰冻／黑云持续时间随升级增加；飓风固定 5 秒 |
| `SpellRadius`、`SpellHalfExtents` | 圆形半径／飓风矩形半边长，厘米，范围不随升级变化；祝福全场允许半径为零 |
| `ForceMoveSpeed` | 飓风世界 +Y（画面右侧）速度，随升级增加 |
| `DrawCooldown` | 成功施放后禁止抽取秒数，祝福固定 150 秒 |
| `MaxHealthDamageFraction` | 雷电第一击固定最大生命 20% |
| `SpellValue`、`SecondarySpellValue` | 雷电首击上限 200／次击 100，均乘升级系数 |
| `SecondaryDelay` | 雷电延迟固定 0.2 秒，到期重新选择最低当前生命目标 |
| `bGlobalPlacement` | 增援／祝福自身全场落点权限，由身份保证，不随升级变化 |
| `SummonedUnitIds` | 整组单位清单；人数／品质不变，出生生命与攻击／治疗随升级放大 |

增援将法术 CardId 传给 `SpawnUnitForTeam` 的 `SourceCardId`，在出生属性副本中放大数值；不以出生后治疗／伤害修正，避免误触发被动和统计。雷电只选敌方存活非英雄，首领排除；黑云不拦地面效果。

`ALKSpellField` 管理黑云、飓风与雷电延迟，只随战斗推进，结算／销毁／换房清理来源。`ApplyConcealment/RemoveConcealment`、`ApplyWind/RemoveWind` 以场效 Actor 名称分别登记，重叠场效只清自己的来源。`IsConcealed` 与 `IsTargetable` 分开，隐藏不等于死亡、无敌或无碰撞。

`IsWindDriven` 只抑制自主行走，不是眩晕／冻结。`ApplyWindDisplacement` 经移动组件检查真实障碍与边界，实际位移计入动画，不能用直接 SetActorLocation 穿墙；逆向自主走路不能抵消风。法阵用 `ELKSpellEffect::SummonZone`、`FLKSkeletonCircleParams`、`ALKSkeletonCircle`，敌方经 `CastEnemyTacticalSpell` 独立消费；`ApplyAreaSlow/RemoveAreaSlow` 按来源取最强减速，不叠乘成定身。

`bBasicAttackHeals` 是牧师职业身份，AttackDamage 为单次基础治疗，间隔、射程和前摇复用普攻。牧师选择可发现受伤友军，忽略敌方嘲讽；`ALKProjectile::SetHealingPayload` 固定友方目标，不拦截成敌人，也不触发伤害型普攻被动。实际回血仍走 `ApplyHeal`，不能救起失能英雄。

## 抽牌冷却

`CanPlayWithCooldown` 在消费前保证冷却后仍有四种可用手牌；执行成功才 `PlayCard(HandIndex, DrawCooldownSeconds)`，首个未冷却项补原槽。失败不能扣费、改队列或提前设冷却。

`GetCooldownRemaining` 查询当场剩余时间，`AdvanceCooldowns` 由战斗 Tick 推进，暂停停止。`GetNextCard` 跳过冷却项，冷却牌仍在原队列位置，到期恢复 FIFO 资格。换房 `InitDeck` 清空冷却，退出重试沿用战前状态。

兼容的五卡组若一张冷却，剩余四种可用卡打出后可立即重抽自身，以维持四槽唯一；再施放冷却牌会不足四种时拒绝。`TransformCard` 保留剩余冷却，不能靠转换绕过限制。

## 建筑几何、事务与服务

`ELKBuildingBehavior` 包括 `Wall/SiegeCart/MageTower/HealingStation`，`FLKUnitRow` 的 `ConstructionSeconds/WallLength/WallThickness/ReflectDamage/WallAuraRange` 是秒或厘米数值。具体品质、数值、修补距离与服务规则见图鉴。

`ValidateCardPlay/PlayCardForTeam` 的可选 `PlacementYaw` 接收朝向。`SpawnUnitForTeam` 接收来源卡、朝向和经事务校验的同阵营 `ReplacedWall`；先成功创建新实体，再移除旧墙，失败保留旧墙与手牌。替换不增加最终单位数量，不以死亡事件移除旧墙，避免误触发被动／奖励。修补按品质决定，保持旧中心与正反朝向。

`LKBuildingGeometry::WallShape` 是平面胶囊：半径为厚度一半、中轴段长为总长减厚度。`CanPlaceWall` 校验完整足迹，`FindMatchingWall` 找修补对象，`RayHit` 返回连续弹道最早命中。`LKNavigation::FObstacle::HalfSegment=0` 表示圆，非零表示墙段；各管线复用这套形状。

`FindBlockingEnemyWall` 为无法抵达的战斗目标选择可接近敌墙，能绕行则仍绕行，治疗者不能变为打墙。施工完成的 `DisplaceOverlappingUnits` 只搬到合法空位，不制造伤害。

障碍 ≤12 时使用每圆／胶囊周围 16 候选点的可见图；更多障碍改网格 A*，目标约 6000 点、绝对上限 8192，每点最多扩展一次，最低采样间距 40 厘米，路径收直最多额外检查 512 条线段。网格候选点、八向边、起终点连接及收直仍检查精确几何，不得斜穿墙角；大量障碍下窄通道可能被保守拒绝。

最近四份纯数值几何网格缓存只在游戏线程使用，不持 Actor／World 引用；施工、移动、旋转、损毁或碰撞间隙变化导致几何不一致时切换缓存。可选 `LKNavigation::FSearchStats` 输出网格／缓存使用、节点和精确碰撞次数，不依赖硬时间阈值证明功能。

`ALKUnitBuilding` 与 `ALKUnitBase_Buildings` 管理驻守者／患者双向预约、移动、进入、释放。死亡、建筑损毁、结束战斗和无可达入口清理双方引用。法师 `bIsMage` 与牧师 `bBasicAttackHeals` 只定义职业，不授予英雄特性；塔只加速主动冷却。治疗站在实际治疗入口加倍率，患者生命低于 50% 才接诊，入站后治到满血。

## 表现与验证边界

`ULKPresentationSubsystem::Emit/Sound` 接收视觉／声音事件，通用治疗、退场和控制在底层生效处播放，专属卡不要重复触发。表现不保存、不消耗战斗 RNG，自动限额及过期清理；火球震动仍由 GameMode 释放入口控制。

`LKBattleArt/LKPolishArt/LKV083Art/LKV084Art` 提供默认 Sprite、Animation、CardIcon 与声音映射；明确作者素材、显式静音和关闭默认声音优先。历史 `WalkingUnitIds` 只表示 MovementFix 覆盖集合，不是完整单位目录。原图、提示词、声音参数与许可集中于 [素材来源](36-ArtAudioSources.md)。

相关自动化在 `Source/little_king/Tests`，按行为修改覆盖身份／权限、获取回滚、容量、实际出手／命中、状态清理、生命周期、隐藏／治疗、召唤整体失败、墙几何及服务关系。界面像素与主观音量另需实际渲染／听感，自动化不能证明长期平衡。当前已执行的发布证据见 [62](62-TerrainAndEncounterDesigner.md)，历史结果见 [开发历史](DevelopmentHistory.md)。

## 道具与市场接口

`LKItemContent::All/Find` 注册道具稳定 ItemId、名称、用途、饰品／装备类别和占位标记。`FLKItemInstance` 用独立 GUID 与品质保存实例；不能复用 CardId 或 UnitId。初版 `Item_Accessory01`、`Item_Equipment01` 均无属性。替换正式内容时保持已存 ItemId 与实例语义；若改 ID 或增加属性，要实现迁移，不能删除旧定义令玩家库存失效。

`FLKMarketOffer` 保留原枚举数值，新增临时法术、临时建筑、道具，卡牌与道具身份必须互斥。临时法术／建筑遵守当前牌组容量、最低五卡、重复卡限制和银币上限，结束后不写永久解锁。研究材料不立即加入牌组，也不受当前银币上限过滤。道具不装备、不占格，任何结局以冻结回执带回 Profile；必须验证实例 ID 唯一并保证重试不重复添加。
