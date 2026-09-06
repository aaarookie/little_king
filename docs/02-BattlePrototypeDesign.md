# 战斗原型技术设计 · Sprint 5 修订

更新：2026-09-06。规则以 [01-GDD](01-GDD.md) 为准，接手改动见 [15](15-Sprint5ChangeLog.md)，UE 5.8 资产迁移见 [16](16-Sprint5MigrationTutorial.md)。旧王座、平局、倒计时和法术门设计已撤销。

## 模块职责

| 模块 | 责任 |
|---|---|
| ALKBattleGameMode / GameState | 部署门槛、战斗阶段、合法性、出牌事务、单位登记、批次结算、对局统计 |
| ALKPlayerController | 战场 Z=0 求交、卡/英雄/营地放置状态、右键取消、独立视觉随机震动 |
| ALKUnitBase / Hero / Building | GAS 属性、普攻状态机、目标优先级、英雄技能与营地限制、兵营/哨塔 |
| ALKHeroCamp | 固定非目标建筑，持有英雄弱引用；实体占地，生命不参与战斗 |
| ULKUnitMovementComponent / LKNavigation | 建筑膨胀障碍、可见图寻路、营地/战场边界、帧时间相关软分离 |
| ULKDeckState / SilverComponent | 满手唯一、FIFO 循环、固定槽位、开局种子洗牌、银币预扣和退款 |
| ULKTraitAuraComponent | 特性光环刷新、GE 句柄和按来源计数的嘲讽状态，移除/死亡清理 |
| LKGameplayHelpers / GameplayLibrary | 统一真实血量变化事件，普通伤害/治疗/技能 AoE；批次嵌套 |
| ALKProjectile | 发射来源快照、整段路径圆碰撞、最早敌方命中、预热 32 条对象池 |
| ALKOpponentBrain | 波次、集火预警、反制排序、蓄费爆发、合法候选出牌 |
| ALKPresentationHUD | 原生 Canvas 头顶条、常显黄线、营地、法术/占地/建筑射程圈、弹道、预警、飘字和提示 |
| ULKBattleHUDWidget + WBP_BattleHUD | UMG 手牌、银币、部署/结算面板；兼容旧资产并关闭旧条/法术锁定转发 |

使用 GAS 属性/GE 与英雄 GA；C++ 承担法术卡和规则状态。当前英雄技能仍共用简单冷却，不是每个技能独立冷却系统。新特性可独立于技能运行时增删。

## 关键执行契约

出牌：ValidateCardPlay → 开始批次 → TrySpend → ResolveCard → 成功才 PlayCard 和计数 / 失败退款 → 结束批次 → 必要时结算。非法请求不消耗；合法空放消耗。同步出牌重入被拒绝，避免事件回调再次操作同一手牌。

牌库：InitDeck 去重并过滤 None，数量必须大于手牌槽数，失败返回 false 且不改旧状态；有效牌库始终是满手 + 至少一张待抽牌。只开局洗牌，PlayCard 原槽替换队首、已出牌排队尾，然后一次性广播 OnHandChanged。GetNextCard 读队首，GetDiscardSize 为兼容返回 0，DrawCard 保留但不额外抽牌。Add/Remove/Transform 返回 bool 并保护全副牌唯一和最低数量，不会制造空槽。GameMode.HasValidDecks 在开战前检查双方是否就绪及所有 CardId 是否存在。

放置预览：PlayerController.GetPlacementPreview 同时输出占地/法术半径与可选建筑攻击半径；GameMode.GetBuildingPlacementAttackRange 只对 Turret 建筑返回对应单位行基础 AttackRange。原生 HUD 同时绘制小占地圈与大射程圈；取消放置后两圈退出。Y=0 中线独立于 bDrawFieldBounds，由 Canvas 绘制以避开地面深度遮挡。临时战斗增益暂不计入基础射程预览。

伤害：记录源与目标快照 → 开始批次 → GE 修改并钳制 Health → 死亡回调更新存活登记、标记待结算 → 记录实际变化及致死标记 → 结束批次。仅最外层批次可发布胜负，因此最后一击与同批双方死亡都纳入统计。

移动：营地中心固定；英雄中心到营地中心 ≤ 移动半径−英雄半径。建筑/营地按自身半径加移动体半径膨胀，再进行路径查询；每段位移检查，避免单帧穿过建筑。移动体分离只推自身且校验障碍和范围。路径最多约每 0.3 秒重算，大幅新指令立即重算；复杂窄路可能保守拒绝。

碰撞层级：LogicRoot 下分别是 Sprite 和 Body（Sphere）。精灵旋转、SpriteScale、攻击脉冲和死亡缩放不参与体积计算。玩法占用以二维圆为准，不依赖 OverlapMultiByChannel 的 blocking 返回值。

## 数据与扩展

单位仍使用 DT_Units 的 FLKUnitRow，普通单位体积来自 DA_GameData.UnitBodyRadius。没有配置表时允许内置示例；明确配置了表却缺少行时拒绝生成，不能悄悄用默认兵替代。

DefaultHeroTraits 是 `HeroId → {Traits: FName[]}`；InitUnit 合并数据行特性与此默认值。内置 `Trait_MageSpellReach`、`Trait_KnightTauntAura` 和 `Taunt` 可直接使用。旧默认 `Trait_MageMight`、`Trait_KnightAura` 在英雄初始化时剔除；需要其他属性加成请用明确的新特性 ID。

DT_Traits 新增 Effect（Attributes / Taunt / GlobalSpellPlacement / MeleeSoldierTauntAura）与 EffectRadius。一般属性效果通过 Modifiers 配置；运行时调用 AddTrait / RemoveTrait / HasTrait / HasTraitEffect。内置骑士光环半径来自 GameData，自定义光环 ID 使用该行 EffectRadius。多个来源分别清理，删除一个来源不应抹掉另一个。

FLKCombatSource：来源阵营是否有效、Team、UnitId、InstanceId、ActionId、Kind（Attack / Projectile / Spell / Skill / Overtime）。弹道在发射时快照来源，发射者死亡仍可归属伤害。法术卡以 CardId 归属；目前技能 AoE 的 ActionId 使用英雄 ID，细分多技能 ID 留待独立技能扩展。

FLKCombatEvent：Source、目标阵营/定义 ID/实例 ID、位置、RequestedAmount、ActualAmount、HealthBefore/After、bIsHeal、bKilled。OnCombatEvent 是完整事件；OnDamageEvent 是实际正数的表现事件，不用于重新累计统计。结果用 GetMatchStats 获取。

如新增技能直接写 Health 或自行 Apply GE，将绕开统一来源统计；应调用 GameplayLibrary / GameplayHelpers。延迟多段技能应逐段检查阶段/施法者状态，明确取消逻辑。当前 FireballSkillHeroIds 默认只有 Hero_Mage：该英雄成功触发当前技能即视为一次火球释放；扩成多种主动技能前应改为逐技能标识，不能把列表当成通用法术分类。

## 兼容和限制

- 原生 HUDClass = LKPresentationHUD，HUDWidgetClass 仍是 WBP_BattleHUD，两者职责不同；旧 BP 若覆盖 HUDClass，按 16 调整。
- GetTeamHeroHealthRatio 保留为弃用接口并返回 0，只为旧蓝图仍能加载；旧血条需从 UMG 删除。
- CanCastSpell 保留但恒 true；旧 OnSpellLockChanged 不再广播。法术合法落点改用 CanPlaceSpellAt；旧 ELKPlayResult::SpellLocked 枚举位置不变，语义改为敌方半场被限制。
- DeploymentTime、bRequireMageForSpells、MaxHeroesPerTeam、AttackStopBuffer 是兼容字段；本轮逻辑分别改为无限部署、特性范围、必须全部 AvailableHeroes、严格射程，旧值不作为新规则开关。
- BuildingTypeLimitOverride：-2 继承、-1 不限、≥0 自定；旧资产保存的 -1 需核对迁移。
- 手中去重按 CardId，不按名字或卡面；当前只区分定义 ID，尚无跨关卡卡牌实例、升级履历或重放系统。
- 战斗 RNG、两副牌洗牌 RNG、相机 RNG 分离；种子有助复现，但不同帧率和输入时序仍可能改变结果。
- Native Canvas 为最低可读表现；NullRHI 自动测试不验证像素位置、中文字体、音乐或实际鼠标遮挡。

## 验证

测试在 Source/little_king/Tests/LKSprint5Tests.cpp，以 `LittleKing.Sprint5` 为前缀，使用 UE 5.8 FTestWorldWrapper 创建临时 Game 世界。执行命令、结果和覆盖边界见 15 / 16。本阶段只做与改动相关的编译、逻辑自动化和短流程 PIE；音效/表现素材、打包/硬件测试后置，Sprint 6 删除。下一阶段的 RunSubsystem、战斗上下文、奖励和存档边界见 [17](17-DungeonDevelopmentPlan.md)，尚未实现。
