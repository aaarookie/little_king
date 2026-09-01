# 战斗原型技术设计（Battle Prototype）

**配套文档**：[01-GDD.md](01-GDD.md) · **目标周期**：6~9 周（含 GAS 学习缓冲） · **环境**：UE5.8 / C++ / Paper2D

## 0.5 v0.2 更新记录（已定稿设计 → 代码现状）

| 议题 | v0.2 实现 |
|---|---|
| 国王不出征 | 战场无王座格；部署的是英雄（`GameMode::DeployHero`，上限 `MaxHeroesPerTeam`） |
| 无平局 | 超时 480s → `bOvertimeActive`，每 `OvertimeWeaknessTick` 对英雄按 `BasePct × (1+Growth×时间)` 扣最大生命百分比（当前双方英雄都虚弱，可配置） |
| 法术门 | `GameMode::CanCastSpell(Team)`：场上法师英雄存活才可打法术卡；AI 同样受限 |
| 建筑上限 | `GameData::BuildingTypeLimit`（同类 ≤2，-1 不限）+ 卡牌级 `BuildingTypeLimitOverride` 覆盖 |
| 战场 | 矩形平地，**左右分界在 Y 轴**：玩家 Y≤0 / 敌方 Y≥0；配合相机 Pitch=-90（Yaw=0, Roll=0）后屏幕呈现"左玩家/右敌方"，中线沿 X 方向绘制；尺寸参数化（`FieldHalfWidth=1200` / `FieldHalfHeight=2000`，即屏幕纵向 2400 × 横向 4000） |
| 新卡来源 | 牌库已预留接口：`DeckState::AddCardToDeck / RemoveCardFromDeck / TransformCard`（地牢事件用） |
| UE5.8 技术事实 | GAS 已移至 `Engine\Plugins\Runtime\GameplayAbilities`（uproject 需显式启用）；`FSetByCallerFloat` 用 **FName** 而非 GameplayTag；`UAttributeSet` 不再要求 `GetLifetimeReplicatedProps`；Paper2D 头文件在旧式 `Classes/` 目录 |

**代码现状（本会话已完成）**：完整 C++ 骨架已写入 `Source/little_king/`（19 个文件），模块依赖/uproject 插件已配置，UBT 编译验证通过。
原型在**零美术资产**下即可运行：无 DT/精灵时自动回退内置默认值 + 调试色块绘制（`bDrawDebugShapes`）。



---

## 0. 目标与验收标准

**目标**：做出可玩的 PvE 自动战斗原型，验证"银币经济 + 英雄保护"的乐趣；架构能承载后续地牢/家园系统而不返工。

**验收标准**：
- [ ] 完整对局可跑通：部署 → 战斗 → 胜负结算（3~5 分钟）
- [ ] 有操作感：打牌、放置、英雄技能，能感觉到"放什么、放哪、何时放"的决策
- [ ] 全部数值在数据表，改数值不动代码
- [ ] 调试命令齐全（加钱/刷牌/秒杀/直接胜利）
- [ ] 连打 3 局无阻塞性 Bug

---

## 1. 技术选型

| 项 | 选择 | 理由 |
|---|---|---|
| 2D 方案 | Paper2D + PaperZD（免费插件） | 官方精灵系统 + 社区动画增强 |
| 相机 | 正交相机（Orthographic） | 2D 标准做法 |
| 语言 | 核心逻辑 C++，UI 用 UMG 蓝图 | 求职硬通货 + 迭代速度平衡 |
| 属性/伤害/技能 | **GAS**（GameplayAbilitySystem） | 求职加分项；本作（技能/特性/Buff）是 GAS 典型场景 |
| 数据驱动 | DataTable（单位/卡牌/技能/波次）+ DataAsset（全局配置） | 平衡与内容扩展全靠它 |
| 移动 | 自研轻量 MovementComponent（直线移动 + 径向分离软碰撞） | 2D 下 UE 自带 NavMesh 很别扭；皇室战争式挤开更符合手感 |
| 网络 | 无。单机 PvE，GameMode 即本地权威 | PvP 是远期话题，不为它预先付出架构成本 |

> **GAS 使用范围收敛**（防学习成本失控）：只用核心三件套 —— `UAttributeSet`（属性）、`UGameplayEffect`（伤害/治疗/增益）、`UGameplayAbility`（技能/法术）。**不碰**：网络预测、GameplayCue 可视化、复杂 Cost 结构。单机不需要。

---

## 2. 架构总览

```
┌─────────────────────────── ALKBattleGameMode（流程权威）───────────────────────────┐
│  PhaseMachine: Deployment → Battle → Result（状态机 + 事件广播）                    │
├────────────────────────────────────────────────────────────────────────────────────┤
│  ALKPlayerState（玩家侧状态）            AOpponentBrain（敌方 AI）                  │
│   ├─ USilverComponent（银币）             ├─ 脚本波次（DT_Waves）                   │
│   └─ UDeckState（牌库/手牌/弃牌堆）       └─ 简单打牌规则（银币够就按优先级出）      │
├────────────────────────────────────────────────────────────────────────────────────┤
│  AUnitBase（C++ 基类，数据表驱动）                                                  │
│   ├─ AUnitHero：技能槽 + 特性（光环/羁绊）                                          │
│   ├─ AUnitBuilding：静态/哨塔/兵营                                                │
│   ├─ 状态机：Idle / Moving / Attacking / Casting / Dead                            │
│   ├─ 索敌：最近敌人优先（"嘲讽"特性可改写）                                         │
│   └─ 伤害：全部走 GAS（GE + SetByCaller）                                           │
├────────────────────────────────────────────────────────────────────────────────────┤
│  UMG UI（蓝图为主）：HUD / 手牌 / 部署界面 / 血条 / 结算                            │
└────────────────────────────────────────────────────────────────────────────────────┘
```

**数据流（打出一张牌）**：
`玩家点击手牌` → `UDeckState::PlayCard(Index)` → `校验银币足够` → `进入放置模式（高亮可放置区）` → `点击位置` → `校验（区域/占用）` → `结算分发`：
- 角色卡 → `SpawnActor(AUnitBase)`（扣银币、入弃牌堆、补一张牌）
- 法术卡 → 施放对应 `UGameplayAbility`（AoE 查询 → 施加 GE）
- 建筑卡 → `SpawnActor(AUnitBuilding)`

---

## 3. 战斗流程状态机

```cpp
enum class EGamePhase : uint8
{
    Deployment,   // 部署英雄（倒计时 20s 或双方就绪）
    Battle,       // 银币累积、抽牌、打牌、自动战斗
    Result,       // 胜负结算（超时判定在此）
};
```

- 状态机放在 GameMode，`OnPhaseChanged` 多播事件驱动 UI 切换。
- **Battle 阶段循环**（GameMode Tick）：
  1. 银币 += PerSecond（封顶 Cap，玩家与敌方 AI 各自结算）
  2. 检查胜负：任一队英雄存活数 == 0 → 结束
  3. **无平局**：超时（480s）后进入虚弱阶段——每 5s 对英雄造成最大生命百分比伤害，强度随时间递增（`OvertimeWeaknessBasePct/Growth/Tick` 可调），英雄先全灭者负
- 英雄阵亡事件 `OnUnitDied` → 更新存活计数 → 广播 UI（顶部英雄血条）。

---

## 4. 模块设计

### 4.1 单位系统 `AUnitBase`

**属性**（GAS `ULKUnitAttributeSet`）：Health / MaxHealth / MoveSpeed / AttackRange / AttackDamage / AttackInterval。

**状态机**（自定义轻量 FSM，不用 UE 状态树——原型期足够且易调试）：
- `Idle`：无目标或目标不可达时待机
- `Moving`：朝目标移动，进入 AttackRange 停止
- `Attacking`：攻击前摇 → 结算伤害 → 后摇 → 回到 Moving/Idle
- `Casting`：施放技能（GAS Ability 激活中）
- `Dead`：播死亡表现 → 销毁（对象池，阶段 6 再做）

**索敌规则**：每 0.25s 更新——取"最近敌方单位"；被"嘲讽"目标强制改写；英雄默认无特殊仇恨，敌方 AI 的"集火英雄"指令通过 `TargetOverride` 字段实现。

**移动**：`ULKUnitMovementComponent`——朝目标直线移动 + 与周围单位径向分离（软碰撞挤开），边界钳制在战场矩形内。无需物理引擎。

**伤害结算**：近战 = 直接 `ApplyGameplayEffect`；远程 = 生成简单直线弹道 `AProjectile`（命中触发 GE）。伤害数值一律用 **SetByCaller** 传入，全游戏共用一个伤害 GE 模板——这样所有增伤/减伤/暴击都能在未来通过 GE 修改器统一生效。

### 4.2 经济系统 `USilverComponent`

- 字段：`Current` / `Cap` / `PerSecond`；事件：`OnSilverChanged`
- 初始值来自 `ULKGameData`（DataAsset 全局配置），可被局内效果（GE/技能）修改 PerSecond 与 Cap
- 银币**不进 GAS**（它是经济资源不是战斗属性），保持独立组件，避免 GAS 复杂度扩散

### 4.3 卡牌系统

- `UCardDefinition`（DataAsset，每张卡一个资产）：
  `CardId / Cost / ECardType(Unit|Spell|Building) / SpawnUnitId / SpellAbilityId / Icon / Description`
- `UDeckState`（每方一个）：`DrawPile`（有序牌库）/ `Hand`（上限 4）/ `DiscardPile`
  - 开局：牌库洗牌 → 抽满手牌
  - 打牌成功 → 该牌入弃牌堆 → 抽一张补手
  - 牌库空 → 弃牌堆洗回牌库（杀戮尖塔式循环）
- 原型示例牌库（8 张）：剑士×3、弓箭手×2、盾卫×2、火球×1
- **预留接口**（地牢阶段用）：`AddCardToDeck / RemoveCardFromDeck / TransformCard`（局内构筑、删牌、升级卡）

### 4.4 技能与特性（GAS）

- **英雄技能**：每个 `UGameplayAbility` 子类一个技能（如：国王·鼓舞=范围内友军攻速/移速 Buff；剑士·剑气斩=AoE 伤害）。冷却用 `CooldownGameplayEffect`（Duration + CooldownTags）实现。
- **技能 AI**：英雄 `Tick` 中简单判定——目标在射程内 & 冷却完毕 → `TryActivateAbilitiesByTag`。原型期不做复杂行为树。
- **被动特性/光环**：生成时施加无限时长 GE（如：范围内友军 +10% 攻击）；"嘲讽"特性用标记（GameplayTag）+ 索敌规则配合。
- **法术卡**：复用同一套 GA（法术 = 以鼠标位置为目标的 Ability）。

### 4.5 敌方 AI `AOpponentBrain`

- **波次脚本**（`DT_Waves`：Time / CardId / Count）——保证对局节奏有起伏、可调参
- **反应式补充**：银币 >= 当前最便宜可打牌费用时，按优先级（近战前排→远程后排→建筑）在随机合法位置打出
- **集火指令**：每 30s 对英雄发起一波 10s 集火（TargetOverride）
- 后续迭代：反制玩家兵种、时机推塔式爆发——都是改规则，不动架构

### 4.6 放置与校验

- 放置区域：己方半场矩形（部署阶段放英雄，战斗阶段放角色/建筑）；法术全场
- 校验链：银币足够 → 法术门（法师在场）→ 点在合法区域 → 不与已有单位/建筑重叠（AABB 查询）→ 同类建筑数量未超上限 → 允许
- 交互：点击手牌进入放置模式（鼠标位置显示幽灵预览 + 可放置区高亮）→ 左键放置 / 右键取消（`ALKPlayerController` 已提供 `TryPlayCardAtMouse/DeployHeroAtMouse` 与射线投影）
- 部署阶段：英雄自由放置于己方半场（国王不出征，无王座格）；同一英雄不可重复部署，数量 ≤ MaxHeroesPerTeam

### 4.7 UI（UMG，蓝图为主）

| 界面 | 内容 |
|---|---|
| WBP_HUD | 银币数字、手牌（图标+费用）、对局计时、双方英雄血条（顶部）、提示消息 |
| WBP_Deployment | 英雄选择栏、已放置计数、倒计时、"开始战斗"按钮 |
| WBP_CardEntry | 卡牌图标/费用/选中态（可复用为地牢商店 UI） |
| WBP_Result | 胜/负、时长、击杀数、剩余银币 |
| 单位血条 | `UWidgetComponent`（原型够用，后续可换屏幕空间方案） |

### 4.8 调试与工具（单人开发的生命线）

- `UCheatManager` 子类 + `UFUNCTION(Exec)`：`AddSilver N` / `DrawCard` / `SpawnUnit Id Team X Y` / `KillAll Team` / `WinMatch` / `SetPhase Battle`
- 战斗日志 `ULKLog`（LogCategory 分模块），伤害全链路可查（GAS 自带日志开关）
- 数值快照：每局结束打印双方单位 DPS/存活时长统计，供平衡用

---

## 5. 数据表设计（原型字段，后续只增不改）

**DT_Units**（单位）：

| 字段 | 类型 | 说明 |
|---|---|---|
| UnitId | FName | 主键 |
| DisplayName | FText | 显示名 |
| BaseHealth / MoveSpeed | float | 基础生命 / 移速 |
| AttackRange / AttackDamage / AttackInterval | float | 射程 / 伤害 / 攻速 |
| AttackType | enum | Melee / Ranged |
| ProjectileId | FName | 远程弹道（近战留空） |
| SpriteRef / SpriteScale | SoftPtr / FVector2D | 精灵与缩放 |
| UnitClass | enum | Soldier / Hero / Building |
| HeroTraits | TArray\<FName\> | 英雄特性 ID（非英雄留空） |
| BuildingBehavior | enum | None / Turret(哨塔) / Barracks(兵营) |

**DT_Cards**（卡牌）：CardId / CardName / Cost / CardType(Unit|Spell|Building) / SpawnUnitId / SpellAbilityId / Icon / Description

**DT_Skills**（技能）：SkillId / SkillName / Cooldown / TargetRule(Self|NearestEnemy|Position|AoE) / Range / EffectGEs(数组) / IsPassive

**DT_Traits**（特性）：TraitId / TraitName / Modifiers(数组：属性 + 数值 + 作用域 Self|AllAllies|AuraRadius)

**DT_Waves**（敌方波次）：WaveId / Time / CardId / Count

**ULKGameData**（DataAsset 全局配置）：银币初始 PerSecond / Cap / 手牌上限 / 部署倒计时 / 对局时限 / 战场尺寸 / 王座格位置

---

## 6. 项目结构

```
Source/LittleKing/
  Battle/    GameMode、PhaseMachine、BattleTypes        （流程）
  Economy/   SilverComponent、DeckState、CardDefinition （经济卡牌）
  Units/     UnitBase、UnitHero、UnitBuilding、Movement、TargetFinder
  Skills/    LKAttributeSet、LKGameplayEffect、HeroAbilities、SpellAbilities
  AI/        OpponentBrain、WavePlanner
  UI/        HUD 基类（少量 C++，主要蓝图）
  Data/      行结构体、DataTable 加载助手、ULKGameData

Content/
  Maps/        L_BattleTest（原型主图）
  Blueprints/  BP_UnitBase、BP_Hero_*、BP_Card_*、BP_Projectile
  UI/          WBP_*
  Data/        DT_Units、DT_Cards、DT_Skills、DT_Traits、DT_Waves、DA_GameData
  Art/         Sprites / Flipbooks / Effects（占位期用纯色方块）
  Audio/       （阶段 5 填充）
```

---

## 7. 开发顺序（Sprint 计划）

| Sprint | 内容 | 验收标准 |
|---|---|---|
| **S0**（1周） | 项目骨架：Paper2D 管线验证、GAS 插件启用、正交相机、Enhanced Input、空战斗图 | 地图可加载，Sprite 随输入移动 |
| **S1**（1周） | 最小战斗闭环：UnitBase + 移动/索敌/攻击 + 双方白盒子互殴 + 死亡 | 双方单位自动战斗至一方全灭 |
| **S2**（1周） | 经济与卡牌：银币组件 + 牌库/手牌 + 打牌生成单位 + 放置校验 | 银币够时能打牌生成单位 |
| **S3**（1周） | 英雄与胜负：部署阶段 + 王座格 + 英雄（1 个技能）+ 胜负判定 + 结算 UI | 完整对局流程跑通 |
| **S4**（1周） | 敌方 AI 与内容：波次脚本 + 6 佣兵/2 法术/2 建筑/3 英雄数据填充 + 数值初调 | 与 AI 对战 3 分钟有来有回 |
| **S5**（1-2周） | 体验打磨：HUD 完善、血条飘字、可放置区高亮、打击感（停顿/震屏/粒子）、占位音效 | 对局观感可接受，无崩溃 |
| **S6**（1周） | 工具与平衡：调试命令、数值平衡、性能粗查 | 连打 3 局无阻塞 Bug |

> 每 Sprint 结束：跑一遍验收清单 + 存档（Git tag）+ 写 3 行技术复盘。

---

## 8. 风险与对策

| 风险 | 对策 |
|---|---|
| GAS 学习曲线陡 | 范围收敛到核心三件套；**若 2 周内推不动，降级为自研轻量 Effect 系统（接口保持一致，未来可换回）** |
| 2D 动画工作量 | 程序动画兜底（浮动/闪白/缩放），Spine 按需 |
| 数值失控 | 全数据表 + 调试命令 + 每 Sprint 自测 3 局 |
| 范围蔓延 | 严格按 S0~S6 清单；"这局外的东西"进 backlog 不进本周 |
| 索敌/手感不对劲 | 软碰撞力度、索敌频率、攻击前摇都做成可调参数 |

---

## 9. 开放问题（不影响原型开工）

1. 超时平局细则（剩余英雄数 → 单位总数 → 判和）
2. 英雄复活机制（原型不做；留 OnUnitDied 事件位）
3. 分路战场（原型单路，战场尺寸参数化预留）
4. 战斗中获牌来源（地牢事件/英雄技能/奖励房间——地牢阶段定）
5. 建筑数量上限（原型：同类建筑 ≤ 2）
