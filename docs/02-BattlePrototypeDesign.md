# 战斗、远征与家园技术设计

基线：v0.8.6。玩法见 [GDD](01-GDD.md)，内容与默认数值见 [图鉴](19-ContentCatalog.md)，技能／卡牌扩展契约见 [34](34-CardDevelopmentInterfaces.md)。本文保留当前架构，阶段演进和旧版验证汇总见 [开发历史](DevelopmentHistory.md)。

## 模块职责

| 模块 | 责任 |
| --- | --- |
| `ULKSaveSlotSubsystem` | 八个逻辑档、最近档、创建／读取／删除、实例存储路由和入口选档检查 |
| `ALKStartMenuGameMode/PlayerController`、`ULKStartMenuWidget` | 开始地图、输入与原生选档界面 |
| `ULKProfileSubsystem`、`ALKHomeGameMode`、`ULKHomeHUDWidget` | 永久档 A/B 双副本、家园升级／研究／战备、出征本金和结算 |
| `LKRunTypes`、`ULKRunSubsystem` | Run Schema 11、远征地图与快照、奖励／市场／休息事务和存档 |
| `LKWorldMapContent`、`ULKWorldMapWidget`、`ULKRunNodeSelectWidget` | 固定地理与随机节点、当前区域展示和服务节点 |
| `ALKBattleGameMode/GameState` | 部署、出牌事务、单位登记、战斗阶段、批次结算和统计 |
| `ALKPlayerController`、`ALKPresentationHUD`、`ULKBattleHUDWidget` | 场地求交与指令、放置预览、原生 Canvas 战斗表现和 UMG 手牌／结果 |
| `ALKUnitBase/Hero/Building`、`ALKHeroCamp` | GAS 属性与普攻、英雄生命周期、施工／驻守／就诊、固定非目标营地 |
| `ULKUnitMovementComponent`、`LKNavigation`、`LKBuildingGeometry` | 共享障碍几何、寻路／分离、技能位移及破墙 |
| `ULKDeckState`、`ULKSilverComponent` | 唯一满手、FIFO 和抽牌冷却，银币预扣／退款 |
| `ULKTraitAuraComponent`、`ULKUnitActive/Passive/StatusComponent` | 按来源光环、主动技能、事件被动、控制／灼烧／隐藏／风场等状态 |
| `LKUnitContent`、`LKUndeadContent`、`LKExpeditionMercenaryContent`、`LKV083/084Content` | 46 个内置单位的稳定身份、默认数值和作者调参合并 |
| `LKResearchContent`、`LKCardRules`、`LKCardPresentation` | 研究获取、卡牌权限／容量／升级及统一显示模型 |
| `LKEncounterContent`、`LKBalanceRules`、`ALKOpponentBrain` | 遭遇校验、一次性实例倍率、波次／敌方经济／集火／出牌 |
| `LKGameplayHelpers/GameplayLibrary`、`ALKProjectile` | 统一伤害／治疗来源与实际变化事件、连续弹道和对象池 |
| `LKSpellExecutor`、`ALKSpellField`、`ALKSkeletonCircle` | 公共法术执行、持续区域与延迟效果、独立敌方法阵 |
| `LKBattleLayout`、`ALKBattleGameMode_Layout`、`ALKBattleTerrain` | 工坊 JSON 解析、独立布局入口及地形规则，详见 [62](62-TerrainAndEncounterDesigner.md) |

GAS 负责属性、GE 和既有英雄 GA；原生代码负责法术卡、规则及状态。英雄技能仍共用简单冷却，尚无逐技能独立冷却系统。表现随机数与战斗／牌库随机数分离。

## 战斗事务与生命周期

出牌事务为 `ValidateCardPlay → BeginCombatBatch → TrySpend → ResolveCard → 成功后过牌和计数／失败退款 → EndCombatBatch`。合法空放消耗，非法或生成失败不消费；同步重入被拒绝。墙修补／替换和召唤均走同一事务，UI 不直接扣费或销毁旧实体。

玩家 `InitDeck` 去重、过滤空 ID，要求卡种数大于四槽；失败不改变旧状态。成功出牌只替换原槽，一次广播 `OnHandChanged`，冷却卡保留队列但暂时跳过。`GetDiscardSize` 兼容返回 0，`DrawCard` 不额外抽牌。添加／移除／转换保护唯一性、容量和最低卡种数。敌方使用 `InitEnemyDeck`，可单卡循环；不开卡牌的遭遇不要求敌方牌库合法。

伤害事务先快照来源，进行减伤与 GE 血量钳制，将死亡加入 `PendingDefeats`，记录实际伤害，再由最外层批次排空死亡被动及献祭追加事件。全部同批计数完成后处理复活，最后检查胜负；`bProcessingDefeats` 防止递归提前结算。最后一击必须在复活前入账。

`IsHero` 包括 Boss，`IsIncapacitated` 为英雄且已死亡。英雄失能保留 Actor，停用战斗、碰撞、技能和光环；普通治疗拒绝失能目标。佣兵／建筑在退场后销毁。骷髅王可在 Battle 阶段恢复生命、碰撞与存活登记。Result 阶段冻结战斗并只执行一次 `FinalizeHeroRecovery`，恢复不重开战斗；结算后的英雄计数仍代表战斗结束瞬间，后续远征读取 `GetBattleOutcome().RecoveredState`。

主动技能只随战斗 Tick 推进，部署、死亡、控制和手动移动禁止启动；结束时取消冲刺与锁定。被动消费实际伤害／治疗及批次死亡，复制治疗以 `ActionId` 防递归。状态由单位持有，死亡、结算和换房清理；弹丸在发射时快照来源及冰火类型，回池清空附加负载。

## 几何与表现

逻辑根位于 Z=0，Sprite 与 Body 分离。精灵旋转、缩放、攻击脉冲及退场表现不改变玩法体积。普通实体使用二维圆，墙体使用胶囊轮廓；放置、移动、近战、连续弹道和 HUD 共享 `LKBuildingGeometry`，不依赖 Overlap 查询替代玩法判断。

英雄活动不限营地半径。普通移动、闪现、冲刺、击退和风移共用场地／实体约束；营地固定，移动单位软分离。寻路按需要缓存和重算；最多 12 个障碍使用可见图，更多使用最多 8,192 点的网格 A*，具体预算与局限见 [34](34-CardDevelopmentInterfaces.md)。

`GetPlacementPreview` 输出占地、法术范围和可选建筑射程；`GetBuildingPlacementAttackRange` 对 Turret 返回基础 AttackRange。墙预览使用完整形状和朝向。Canvas 黄线独立于调试边框和地面深度；临时属性增益不计入基础射程预览。

表现使用 `ULKPresentationSubsystem`，不反向驱动伤害，不写远征存档或消耗战斗随机流。伤害／治疗飘字只显示实际量；火球震动由释放入口触发一次。资源来源和加工归档统一见 [36](36-ArtAudioSources.md)。

## 内容与事件数据

`FLKUnitRow` 承载数值与表现。`LKUnitContent::MergeAuthoredTuning` 保持内置 UnitId、类别、品质、种族、职业、主动／被动、特性、建筑行为、产出单位及占格等稳定身份，保留允许调节的作者值。自定义未注册 UnitId 仍可完全从数据表读取。运行时先复制 authored GameData 与卡牌，不能写回共享资产；非法参数按对应生成／字段校验处理，墙几何的回退规则见接口文档。

`DefaultHeroTraits` 为 HeroId 到特性数组的映射；内置全场施法、骑士嘲讽光环和盾卫嘲讽有身份兜底。旧 `Trait_MageMight/Trait_KnightAura` 在英雄初始化剔除。特性效果包括属性、嘲讽、全场施法、近战佣兵嘲讽光环、召唤生命代价和远程减伤。`Trait_Sacrifice=0.03`、`Trait_FaceFear=0.30`；同名内置身份不能靠调表改变，自定义特性可用 `EffectValue`，同类比例相加后钳制 0–1。

`FLKEncounterRow` 保存稳定 EncounterId、级别、奖励档、英雄名单、有限波次、敌方卡牌／经济／AI，以及已冻结的平衡倍率。目录进入远征前校验 ID、有限值、唯一性、数量及范围；固定三个内置 ID 保留，无表时使用代码回退。`LKBalanceRules::InstanceScalesFor` 对每个敌方出生实例只应用一次生命／攻击倍率，英雄份额与地图进度不重复相乘。

`FLKCombatSource` 包含阵营有效性、Team、UnitId、InstanceId、ActionId、来源种类和 `bRangedSource`。弹丸保留发射者快照，发射者销毁后仍可归属；法术按远程来源，Overtime／HealthCost 排除普通远程减免。卡牌效果以 CardId 归属，单位技能／被动使用明确的技能或特性 ID。

`FLKCombatEvent` 包含目标阵营／定义／实例、位置、请求量、实际量、前后生命、治疗和致死标记。`OnCombatEvent` 是完整事件；`OnDamageEvent` 仅用于实际正数变化的表现，不能再次累计统计。新增伤害／治疗调用公共 Gameplay 接口；最大生命按比例调整不属于治疗事件。延迟效果逐段检查战斗与来源有效性，并在结束时清理。

## 远征与家园持久化

`FLKRunState`（Schema 11）、`FLKBattleContext`、`FLKBattleOutcome` 都是值快照，不保存 Actor、ASC 或 GE 句柄。Run 保存地图、唯一牌组与等级、遭遇目录、节点、道具／材料／钱包、奖励批次、历史、家园快照和回执；Profile Schema 3 保存永久金币、道具实例、材料、解锁、建筑等级及战备。

`ULKRunSubsystem` 由 GameInstance 持有；正式出征生成五区域、每区十五层地图。地理边界固定，内部 DAG 和敌阵容使用新种子，UI 只显示当前区域。生成器验证坐标、层次、类型、引用、可达性及边方向；追加动态遭遇前复制模板，避免数组扩容使引用失效。

GameMode 消费 `PendingBattle`，将玩家英雄、牌组、遭遇和种子写入私有数据副本。每房重建营地、临时单位、弹丸及状态；先恢复玩家基础最大生命与永久特性，再钳制生命。敌方每房按完整遭遇重建。结算按 RunId／NodeId／AttemptId 提交，`ProcessedAttemptIds`、奖励批次和阶段条件防止重复。

市场／休息进入 `ResolvingNode` 后保存；购买、休息升级及节点消费只有保存成功才发布。奖励和服务 UI 从状态快照生成显示模型，经子系统命令提交，不直接改钱包或牌组。战斗暂停退出保留战前 PendingBattle；恢复不改选节点。非战斗放弃需明确确认，活动远征不能直接回家研究。

出征前冻结 ProfileId、地图、队伍、牌组、神像恢复、金库经济、本金与收益规则。`FundedRunId/FundedRunGold` 记录已扣本金：预留后写 Run，失败撤回；只有已知有效 Run 或确认无 Run 才能协调孤立预留，损坏／未知 Run 不猜测退款。终态保存冻结回执，再由稳定 SettlementId 向 Profile 幂等交付金币、材料与道具，标记失败可重试。

永久档 A/B 轮换并增加 Revision，候选写入、读回校验后发布。每个选档通过 `ConfigureStorage` 路由到独立路径，切档清空旧内存状态；Home/Battle 首次 BeginPlay 未选档则返回菜单。菜单成功读取 Profile／Run 并更新使用时间后才发布选档结果。非法档保留，删除使用 tombstone 防止旧副本复活。

迁移保留旧图、种子、商品与已冻结回执；Schema 5 的 PendingGold 转入钱包，更早档沿用不追赠收益的规则。旧牌组超额保留并显式裁减；正式新出征执行七卡最低要求。

## 兼容与验证边界

- `HUDClass=LKPresentationHUD` 与 `HUDWidgetClass=WBP_BattleHUD` 分工不同；原生 HUD 可兼容旧 authored 界面。
- `GetTeamHeroHealthRatio` 为加载旧蓝图保留并返回 0；`CanCastSpell` 恒 true，实际位置使用 `CanPlaceSpellAt`；旧 `SpellLocked` 枚举位置保留，表示敌方半场限制。
- `DeploymentTime`、`bRequireMageForSpells`、`MaxHeroesPerTeam`、`AttackStopBuffer` 为兼容字段，不再决定部署倒计时、法师开关、玩家三英雄门槛或额外攻击范围。
- `BuildingTypeLimitOverride` 为 -2 继承、-1 不限、非负数指定上限。CardId 与 UnitId 不可通用转换；工坊两个目录分别同步。
- 战斗种子可辅助复现，但帧率和输入时序仍可能改变结果；尚无完整重放或可交易卡牌实例系统。
- 自动化位于 `Source/little_king/Tests`。NullRHI 覆盖逻辑，不能证明像素布局、中文字体、主观听感和鼠标遮挡；发布证据与实际运行范围见 [62](62-TerrainAndEncounterDesigner.md)。文档整理不重复全量测试。

## 调研优化扩展

- `LKMarketContent` 负责 Gen2 新市场；`LKResearchContent::GenerateMarket` 保留旧格式兼容。Run Schema 11 保存生成版本、研究排除集、卡牌银币成本快照与携带道具，Profile Schema 3 保存独立道具实例。新店在第一次进入时生成，读档不重抽；旧已生成库存与价格保留。
- `LKAIObservation` 从己方有效观察者的可见目标合集创建快照；`LKAIDecision` 按具体效果评估，`ALKOpponentBrain` 用独立随机流、稳定战术意图和有限候选预算执行，扣费前重新验证。
- `LKAnimationMetadata` 配置步幅、帧权重及接触事件；动作实例负责开始、结束／取消及临时表现组件回收。伤害、位移和冷却继续由玩法逻辑驱动。
- `FLKMatchStats` 追加实际成功出牌消耗、生产溢出、满银持续时间、首名英雄阵亡时间和逐帧采样单位峰值。`LKBalanceTelemetry` 将最近 128 场保存到 `Saved/Telemetry/Battles.json`；本地诊断写入失败不阻断结算，自动化样本独立写入 `Battles-Automation.json`，不挤占玩家记录。`Scripts/SummarizeBalanceTelemetry.py` 默认排除自动化，不把强制胜负测试当成玩家胜率。
