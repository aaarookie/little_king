# 战斗、家园与开始界面技术设计


## 2026-09-16：多格卡与战斗状态

LKCardRules 统一计算 8 格预算；ULKRunSubsystem::ChooseRewardReplacingCards 原子提交多卡替换，单卡接口继续兼容。手牌数量和最低不同卡数仍为 4/5。ULKUnitStatusComponent 管理控制、强化、烧伤、巨魔王支援和目标上的冰火历史；由单位战斗 Tick 推进，结束/死亡/跨房清理。普攻在实际出手时调用 BeginAttack/AfterAttack；弹丸携带头部快照并在回收时清空。CanPursueTarget 的建筑过滤和投石炮锁定抛射共同保证只伤害建筑。生命支援用可移除的 GAS 最大生命修饰，按旧比例同步当前生命，不发伤害或治疗事件。Run Schema 8 从 6/7 迁移只升版本。完整接口目录见 [34](34-CardDevelopmentInterfaces.md)。


## 阶段三增补（2026-09-16）

`LKExpeditionMercenaryContent` 注册八种临时佣兵及其卡牌模板，`LKUnitContent` 合并全部 21 个单位的稳定身份与表格调参。Quality、Race、ActiveAbility 属于代码身份；品质不是运行时乘算器。`ELKSpellGrade` 单独定义 14 档法术等级。`LKCardPresentation` 统一家园、奖励和手牌中的中文分类、颜色、基础数值及技能说明。

`ULKUnitActiveComponent` 由单位战斗 Tick 调度背刺、模仿施法和冲刺，部署阶段不计时；停止战斗撤销冲刺和锁定。背刺锁定与嘲讽免疫是角色特例，其他单位的嘲讽优先级不变。`ULKUnitPassiveComponent` 消费实际伤害/治疗与批次死亡事件，治疗复制带独立 ActionId 防递归。冲刺、击退与闪现落点共用移动组件的实体/边界约束；伤害仍走原 GAS 结算。

`ULKRunSubsystem::ChooseReward(Index, ReplacedCardId)` 作为卡组满额替换入口；最多 8 张，验证旧卡/唯一性，保存失败回滚，强化不继承。`ULKRunRewardWidget` 用两列原生按钮呈现替换对象，返回选择不消费批次。底层加卡接口也拒绝第九张。Schema 7 标记新内容契约；Schema 6 的钱包和图原样迁移，超额旧卡先保留，显示明确裁减 UI 后再推进。手牌仍为四槽即时 FIFO 补牌。

更新：2026-09-13。规则以 [01-GDD](01-GDD.md) 为准，地牢交接见 [18](18-DungeonChangeLog.md)，家园交接见 [27](27-HomeChangeLog.md)，优化阶段交接见 [29](29-OptimizationChangeLog.md)。15 / 16 保留 Sprint 5 历史和迁移。旧王座、平局、倒计时和法术门设计已撤销。

## 模块职责

| 模块 | 责任 |
|---|---|
| ULKSaveSlotSubsystem | 8 个逻辑档、最近档、创建/读取/删除、实例存储路由、初次 Play 入口检查 |
| ALKStartMenuGameMode / PlayerController / ULKStartMenuWidget | 独立开始地图、鼠标与控制台输入、原生选档 UI、删除确认和设置事件 |
| ULKGameViewportClient / LKMoneyCommand | 在引擎 SHOW 命令前匹配金币短语，验证数量并提交到当前永久档 |
| ALKBattleGameMode / GameState | 部署门槛、战斗阶段、合法性、出牌事务、单位登记、批次结算、对局统计 |
| ALKPlayerController | 战场 Z=0 求交、卡/英雄/营地放置状态、右键取消、独立视觉随机震动 |
| ALKUnitBase / Hero / Building | GAS 属性、普攻状态机、目标优先级、英雄技能与营地限制、兵营/哨塔 |
| ALKHeroCamp | 固定非目标建筑，持有英雄弱引用；实体占地，生命不参与战斗 |
| ULKUnitMovementComponent / LKNavigation | 建筑膨胀障碍、可见图寻路、营地/战场边界、帧时间相关软分离 |
| ULKDeckState / SilverComponent | 满手唯一、FIFO 循环、固定槽位、开局种子洗牌、银币预扣和退款 |
| ULKTraitAuraComponent | 特性光环刷新、GE 句柄和按来源计数的嘲讽状态，移除/死亡清理 |
| ULKUnitPassiveComponent | 单位死亡/英雄失能触发的召唤、巨骨治疗、王计数与复活；不 Tick，不依赖 GA |
| LKUnitContent | 二十一个内置单位的稳定规则、默认调整数值和数据表合并；自定义 ID 保持完全数据驱动 |
| LKUndeadContent | 五个亡灵单位默认定义，由 LKUnitContent 纳入统一规则合并 |
| LKEncounterContent / DT_Encounters | 遭遇零资产回退、表行规范化、固定三行完整性及结构校验；敌阵容/波次/经济/AI/奖励档 |
| LKRunTypes / ULKRunSubsystem | SchemaVersion 5 远征状态、分层路线、遭遇目录、英雄/卡组跨房、奖励批次原子消费和当前档持久化 |
| LKGameplayHelpers / GameplayLibrary | 统一真实血量变化事件，普通伤害/治疗/技能 AoE；批次嵌套 |
| ALKProjectile | 发射来源快照、整段路径圆碰撞、最早敌方命中、预热 32 条对象池 |
| ALKOpponentBrain | 波次、集火预警、反制排序、蓄费爆发、合法候选出牌 |
| ALKPresentationHUD | 原生 Canvas 头顶条、常显黄线、营地、法术/占地/建筑射程圈、弹道、预警、飘字和提示 |
| ULKBattleHUDWidget + WBP_BattleHUD | UMG 手牌、银币、部署/结算面板；自动托管 D3 奖励面板并兼容旧资产 |
| ULKRunRewardWidget | 零资产原生三选一 UI、真实升级值、图标/占位、牌组摘要、跳过与 BP 换肤入口 |

使用 GAS 属性/GE 与英雄 GA；C++ 承担法术卡和规则状态。当前英雄技能仍共用简单冷却，不是每个技能独立冷却系统。新特性可独立于技能运行时增删。

## 关键执行契约

出牌：ValidateCardPlay → 开始批次 → TrySpend → ResolveCard → 成功才 PlayCard 和计数 / 失败退款 → 结束批次 → 必要时结算。非法请求不消耗；合法空放消耗。同步出牌重入被拒绝，避免事件回调再次操作同一手牌。

牌库：InitDeck 去重并过滤 None，数量必须大于手牌槽数，失败返回 false 且不改旧状态；有效牌库始终是满手 + 至少一张待抽牌。只开局洗牌，PlayCard 原槽替换队首、已出牌排队尾，然后一次性广播 OnHandChanged。GetNextCard 读队首，GetDiscardSize 为兼容返回 0，DrawCard 保留但不额外抽牌。Add/Remove/Transform 返回 bool 并保护全副牌唯一和最低数量，不会制造空槽。HasValidDecks 检查所有参与出牌的阵营；亡灵固定波次明确 bEnemyUsesCards=false，只检查玩家，不用两种骷髅伪造四槽牌组。

放置预览：PlayerController.GetPlacementPreview 同时输出占地/法术半径与可选建筑攻击半径；GameMode.GetBuildingPlacementAttackRange 只对 Turret 建筑返回对应单位行基础 AttackRange。原生 HUD 同时绘制小占地圈与大射程圈；取消放置后两圈退出。Y=0 中线独立于 bDrawFieldBounds，由 Canvas 绘制以避开地面深度遮挡。临时战斗增益暂不计入基础射程预览。

伤害：记录源与目标快照 → 开始批次 → 远程分类减伤 → GE 修改并钳制 Health → 死亡/失能回调更新存活登记、加入 PendingDefeats → 记录实际变化及致死标记 → 最外层排空死亡被动与献祭追加事件 → 同批全部计数后复活 → 检查胜负。复活事件若引发新死亡也先排空。复活晚于原始伤害入账，避免生命先回满而吞掉最后一击；bProcessingDefeats 阻止递归提前结算。

生命周期：IsHero 包含 Boss；IsIncapacitated=IsHero&&IsDead。英雄 Die 保留 Actor，停止战斗、碰撞、技能与光环，普通治疗拒绝失能目标；佣兵/建筑仍在死亡动画后销毁。朽骨再生可在 Battle 阶段恢复生命和碰撞、重新登记存活数。EndMatch 在 Result 阶段冻结全部单位后执行一次 FinalizeHeroRecovery，恢复生命但不恢复战斗。GetHeroCount / AliveHeroes 结算后保留战斗结束瞬间的意义；UI 与 D1 继承读取 GetBattleOutcome 的 RecoveredState。

移动：营地中心固定；英雄中心到营地中心 ≤ 移动半径−英雄半径。建筑/营地按自身半径加移动体半径膨胀，再进行路径查询；每段位移检查，避免单帧穿过建筑。移动体分离只推自身且校验障碍和范围。路径最多约每 0.3 秒重算，大幅新指令立即重算；复杂窄路可能保守拒绝。

碰撞层级：LogicRoot 下分别是 Sprite 和 Body（Sphere）。精灵旋转、SpriteScale、攻击脉冲和死亡缩放不参与体积计算。玩法占用以二维圆为准，不依赖 OverlapMultiByChannel 的 blocking 返回值。

## 数据与扩展

单位仍使用 DT_Units 的 FLKUnitRow，普通单位体积来自 DA_GameData.UnitBodyRadius。`LKUnitContent` 注册全部二十一个内置战斗单位，并统一合并同名表行。代码拥有 UnitId、UnitClass、AttackType、ProjectileId、骷髅/法师身份、被动规则参数、固有 HeroTraits、PlaceholderColor、BuildingBehavior 和 SpawnUnitId；数据表拥有名称、生命、攻击、射程、攻击间隔/前摇、移速、技能冷却、产兵间隔、Sprite 与 SpriteScale。表里填错身份字段不会改变核心玩法；数值非法仍由生成入口明确拒绝，不会静默回退。没有表行时采用与当前图鉴一致的代码默认数值。不在注册表中的新 UnitId 仍完全按数据表读取，因此内容扩展无需修改注册表。

DefaultHeroTraits 是 `HeroId → {Traits: FName[]}`；InitUnit 合并数据行特性与此默认值。内置 `Trait_MageSpellReach`、`Trait_KnightTauntAura` 和 `Taunt` 可直接使用。旧默认 `Trait_MageMight`、`Trait_KnightAura` 在英雄初始化时剔除；需要其他属性加成请用明确的新特性 ID。

DT_Traits 的 Effect 支持 Attributes / Taunt / GlobalSpellPlacement / MeleeSoldierTauntAura，以及 D0 新增 SummoningHealthCost / RangedDamageReduction。新 EffectValue 保存比例，一般属性效果仍用 Modifiers；运行时调用 AddTrait / RemoveTrait / HasTrait / HasTraitEffect / GetTraitEffectValue。内置 Trait_Sacrifice=0.08、Trait_FaceFear=0.30 可无表使用，自定义 ID 可通过表配置其他值；多个同类数值效果相加后钳制 0～1。删献祭只取消代价，不删被动。骑士光环仍按来源独立清理。

DT_Encounters 使用 FLKEncounterRow。每行包含稳定 EncounterId、显示名、Normal/Elite/Boss、RewardTier、EnemyHeroIds、Waves、敌方出牌/牌组、银币产速/上限/初值及 FLKEncounterAISettings。配置表后固定路线三行必须齐全；英雄/单位/卡牌 ID、唯一性、波次数量、有限数值、Min/Max 和牌组最低数量在进入远征前校验。额外合法行会进入目录快照，供调试和后续路线扩展。表未配置时使用同参数的三个代码回退行。

FLKCombatSource：来源阵营是否有效、Team、UnitId、InstanceId、ActionId、Kind（Attack / Projectile / Spell / Skill / Overtime / HealthCost / Revival）及 bRangedSource。弹道发射时快照 AttackType，发射者销毁后仍可归属伤害和远程减伤；Spell 无实体施法者也按远程处理。Overtime/HealthCost 明确排除远程减免。法术卡以 CardId 归属；既有 GA AoE 用英雄 ID，新被动用 Skill_GiantBones / Skill_BoneRegeneration / Trait_Sacrifice。

FLKCombatEvent：Source、目标阵营/定义 ID/实例 ID、位置、RequestedAmount、ActualAmount、HealthBefore/After、bIsHeal、bKilled。OnCombatEvent 是完整事件；OnDamageEvent 是实际正数的表现事件，不用于重新累计统计。结果用 GetMatchStats 获取。

如新增技能直接写 Health 或自行 Apply GE，将绕开统一来源统计；应调用 GameplayLibrary / GameplayHelpers。延迟多段技能应逐段检查阶段/施法者状态，明确取消逻辑。当前 FireballSkillHeroIds 默认只有 Hero_Mage：该英雄成功触发当前技能即视为一次火球释放；扩成多种主动技能前应改为逐技能标识，不能把列表当成通用法术分类。

## D1～D5 远征运行链

LKRunTypes.h 提供反射值结构：FLKRunState（当前 SchemaVersion=6、种子、阶段、队伍、唯一卡组与升级等级、遭遇目录、节点、待结算上下文、待选/已消费奖励批次、战斗历史、家园快照与回执）、FLKBattleContext 和 FLKBattleOutcome。它们不保存 Actor、ASC 或 GE 句柄，由 D5 的 ULKRunSaveGame 持久化到当前选档的 Run 文件（存档 01 为 LittleKing_Run），旧版本按迁移分支升级。

ULKRunSubsystem 由 GameInstance 持有，调用 LKWorldMapContent 生成五区域地图，各区 10～20 个节点；区域边界和入口/出口固定，内部 DAG 与敌阵容使用每次出发的新种子。UI 展示全图，只有当前节点有向相邻节点可进入。旧四层生成器仅供旧档/显式独立调试。新远征先复制并校验整个遭遇目录；GameMode 消费 PendingBattle，把玩家英雄、卡组、完整遭遇和种子写入其运行时 GameData 副本，OpponentBrain 按快照重建波次、牌组、银币与战术计时。每房重新建立营地、单位、弹道和临时状态。玩家英雄恢复 BaseMaxHealth、永久特性和钳制后的生命。EndMatch 按本轮神像恢复快照结算一次（默认 40%），先按 RunId/NodeId/AttemptId 提交 Outcome，再通知 HUD。ProcessedAttemptIds、奖励批次和阶段前置条件共同防止双击、旧世界回调和重复结算。

现有 WBP_BattleHUD 无需重存：ULKBattleHUDWidget 在初始化/重建时绑定 `Btn_Restart` 与三个旧英雄按钮。英雄按钮按本轮 AvailableHeroes 顺序更新名称与实际部署 ID；奖励、节点选择和恢复面板由原生 UMG 维护唯一实例。面板读取 RunState 候选，入口调用 RunSubsystem 的校验命令。奖励/路线页的“暂回家园”调用 ReturnToHome：仅结果安全点、恢复地图/服务节点或终态允许，先保存再排队切图，重复请求被拒绝。

## H0～H5 家园运行链与 UI

永久档由 ULKProfileSubsystem 管理，每个选档都有 A/B 双副本与 Revision（存档 01 为 LittleKing_Profile_A/B），包含金币、建筑等级、永久解锁和已保存战备。ULKHomeHUDWidget 只持草稿及显示模型，升级/保存/出征/放弃交给 ALKHomeGameMode 与子系统，操作失败不由 UI 自行修改金币或等级。

L_Home 的 WorldSettings 指向 BP_HomeGameMode，并绑定同一 authored DA_GameData；运行时复制数据，代码身份规则与 DT_Units 调参按战斗相同规则合并。家园建筑是独立 Actor，使用有 Color 参数的 Unlit 方块和屏幕空间 UMG 中文标签，不生成战斗状态、血条或 AI。新地图、材质与生成脚本归属见 27。

家园 HUD 使用 ScaleBox 容纳固定设计尺寸的七种面板，列表与详情各自滚动，底部操作固定。战备草稿按英雄/牌组分页；DisplayedModel 是当前实际显示与按钮回调的同一份模型，确认面板不会误用原面板操作表。升级反馈优先显示 LastMessage；关闭时整块 PanelSize 折叠，面板打开时控制器拒绝场景命中。建筑快捷栏与实际 BuildingId 共用一份目录。

出征请求在第一次 Run 存盘前冻结 ProfileId、完整地图、队伍、卡组、神像恢复、金库产速/上限、携带金币和收益规则。已有轮不追套家园变化；终态通过稳定 SettlementId 幂等转入永久钱包。家园冷启动先加载旧 Run，再处理结算和显示继续入口。放弃必须由 UI 明确确认，Run 保存失败时回滚，成功后把剩余远征钱包全额带回。

## 优化二：地图与服务节点、金币交接

`LKWorldMapContent` 用固定地理种子裁剪五块凸多边形；正式出征请求独立生成新 Seed，`Generate` 按区域内五层生成 10～20 个节点，覆盖每个节点的入/出边并添加随机边，区域间只连接相邻前区出口与后区入口。`Validate` 检查节点数量、坐标归属、类型、层数、遭遇引用、无悬空/重复/反向边、全图可达以及最终首领出口。动态遭遇模板先复制再向目录追加，避免 TArray 扩容使模板指针失效。

`ULKWorldMapWidget` 只绘制多边形、箭头、色块、节点命中；`ULKRunNodeSelectWidget` 负责地图放大、详情、动态候选列表及确认按钮。战斗世界识别 ChoosingNode / ChoosingReward / ResolvingNode，直接显示相应界面，不在回家继续或载入时自动挑第一场战斗。

市场、休息进入 ResolvingNode 并保存，然后广播 `OnServiceNodeEntered`。当前 `ResolveServiceNode` 支持 `RestHeal` / `LeaveMarket`，成功保存后才标记完成并返回 ChoosingNode；失败回滚。`ChangeWalletGold` 以交易 ID 防重复并拒绝透支，供将来的事件/市场事务接入。

Profile 的 `FundedRunId / FundedRunGold` 记录已扣本金：出发先 ReserveDepartureGold，再写 Run 安全点；写 Run 失败会撤销该次预留。加载已知有效 Run 或确认无 Run 后才允许 Reconcile，退款孤立扣款；A/B 回退缺失本金扣款时幂等补齐。已处理终态回执不可再扣本金。未知/损坏 Run 不触发猜测性退款。途中收益累加 WalletGold，终态保存冻结回执后才让 Profile 一次性入账并清预留；Run 的交接标记写失败可以重试。

Run Schema 7 保存完整地理、节点位置/连线、遭遇、钱包和交易 ID。Schema 5 迁移保留原图与冻结回执，PendingGold 迁入 WalletGold；更早版本保持既有不追赠永久收益的迁移约定。旧区域参数及四层独立调试签名保留兼容，正式家园不再选区域。

## 兼容和限制

- 默认入口为 L_StartMenu。ULKSaveSlotSubsystem 在当前 GameInstance 保存 ActiveSlot；Home/Battle GameMode 的首次 BeginPlay 检查选档状态，未选则返回菜单并停止自身初始化。正常地图往返沿用同一实例，不重新选档。
- Profile/Run 的 ConfigureStorage 清空内存状态后设置实例路径；菜单在成功读取 Profile 和 Run、更新时间后才发布选档结果。正式多存档不改全局测试路径。删除的 tombstone 与未知/损坏档保护见 29。
- LKGameViewportClient 的 Exec_Runtime 在内建 SHOW 处理前只匹配完整金币短语，ULKCheatManager 另提供 ProcessConsoleExec 入口；其他命令仍由引擎处理。金币数量逐位检查范围，累加使用 int64 后封顶。

- 原生 HUDClass = LKPresentationHUD，HUDWidgetClass 仍是 WBP_BattleHUD，两者职责不同；旧 BP 若覆盖 HUDClass，按 16 调整。
- GetTeamHeroHealthRatio 保留为弃用接口并返回 0，只为旧蓝图仍能加载；旧血条需从 UMG 删除。
- CanCastSpell 保留但恒 true；旧 OnSpellLockChanged 不再广播。法术合法落点改用 CanPlaceSpellAt；旧 ELKPlayResult::SpellLocked 枚举位置不变，语义改为敌方半场被限制。
- DeploymentTime、bRequireMageForSpells、MaxHeroesPerTeam、AttackStopBuffer 是兼容字段；本轮逻辑分别改为无限部署、特性范围、必须全部 AvailableHeroes、严格射程，旧值不作为新规则开关。
- BuildingTypeLimitOverride：-2 继承、-1 不限、≥0 自定；旧资产保存的 -1 需核对迁移。
- 手牌去重按 CardId，不按名字或卡面；远征保存卡牌等级和奖励消费记录，永久收藏与本轮临时牌组分离。尚无独立可交易卡牌实例或重放系统。
- 战斗 RNG、两副牌洗牌 RNG、相机 RNG 分离；种子有助复现，但不同帧率和输入时序仍可能改变结果。
- Native Canvas 为最低可读表现；NullRHI 自动测试不验证像素位置、中文字体、音乐或实际鼠标遮挡。

## 验证

测试在 Source/little_king/Tests 下使用 UE 5.8 FTestWorldWrapper 和真实 authored BP GameMode。D3 增加奖励状态机、领取边界，以及原生面板打开、HUD 跳过后关闭和下一关解锁的集成断言；完整结果见 18 / 23 / validation。NullRHI 不判断最终像素观感，音效/正式美术、打包和硬件测试继续后置。
