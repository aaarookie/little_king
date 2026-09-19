# Bug 修复记录（Bug Log）

> 2026-09-06：新增 Sprint 5 的 P0/P1 修复、回归范围和实际结果集中记录在 [15 协作变更记录](15-Sprint5ChangeLog.md)，对应测试位于 Source/little_king/Tests/LKSprint5Tests.cpp。本文以下是此前缺陷记录；相关旧规则按 [01](01-GDD.md) / [16](16-Sprint5MigrationTutorial.md)迁移。


**用途**：记录每次修改的 bug——影响、原因、修复方法。新 bug 按编号追加在最后，保持编号递增。

**格式**：现象（影响）→ 原因 → 修复 → 涉及文件 → 验证

---

## BUG-001：`GlobalDefaultGameMode` 不生效（PIE 用默认 GameMode）

- **现象**：PIE 无任何 LogLK 日志、无战场框、无单位，日志显示 `LogLoad: Game class is 'GameModeBase'`。整个游戏逻辑完全不启动。
- **原因**：`UGameMapsSettings` 是 `config=Engine`，`GlobalDefaultGameMode` 必须写在 **`DefaultEngine.ini`**；误写进 `DefaultGame.ini` 会被静默忽略（不报错，极难排查）。
- **修复**：把 `[/Script/EngineSettings.GameMapsSettings]` 段（含 `GlobalDefaultGameMode` / `GlobalDefaultPlayerControllerClassName`）移到 `Config/DefaultEngine.ini`。
- **涉及文件**：`Config/DefaultEngine.ini`、`Config/DefaultGame.ini`
- **验证**：重启编辑器后 Project Settings → Maps & Modes 显示 ALKBattleGameMode；PIE 日志出现 `Game class is 'ALKBattleGameMode'`。

## BUG-002：PIE 视角停在 (0,0,0)，不使用场景相机

- **现象**：画面黑屏/地平线视角，看不到战场。
- **原因**：引擎的 `bAutoManageActiveCameraTarget` 只在 `SetPawn`/`ClientRestart`/观战等路径触发；本项目 `DefaultPawnClass=nullptr` 从不 SetPawn → 自动接管相机永不执行。
- **修复**：`ALKPlayerController::BeginPlay` 手动接管：优先 `ACameraActor`，其次任意带 `UCameraComponent` 的 Actor（CineCameraActor 等）。
- **涉及文件**：`ALKPlayerController.h/.cpp`
- **验证**：PIE 日志出现 `视角接管场景相机 XXX`，画面正确俯视战场。

## BUG-003：战场呈现"上下分"而非"左右分"

- **现象**：玩家在屏幕下方、敌方在屏幕上方，与设计（左/右分）相反；黄色中线为水平线。
- **原因**：相机 Pitch=-90 俯视时，**屏幕上下 = 世界 X 轴、屏幕左右 = 世界 Y 轴**；而左右分界原实现放在 X 轴上。
- **修复**：左右分界改到世界 **Y 轴**（玩家 Y≤0 / 敌方 Y≥0），战场尺寸对调（HalfWidth 1200 / HalfHeight 2000），中线沿 X 绘制；敌方 AI 出兵/打牌位置、英雄自动部署、兵营出兵方向全部同步改 Y 轴。
- **涉及文件**：`ULKGameData.h`、`ALKBattleGameMode.cpp`、`ALKOpponentBrain.cpp`、`ALKUnitBuilding.cpp`
- **验证**：PIE 后屏幕左绿右红，中线为竖直分隔线。

## BUG-004：编辑器运行时外部编译被 Live Coding 拦截（流程问题）

- **现象**：命令行 UBT 编译报 `Unable to build while Live Coding is active`。
- **原因**：编辑器进程持有 Live Coding 锁，外部编译被拒（设计如此）。
- **修复**：工作流约定——**改 C++ 前先保存并关闭编辑器**；或编辑器内按 Ctrl+Alt+F11 用 Live Coding 热加载。
- **涉及文件**：无（流程约定，已写入任务清单）
- **验证**：关闭编辑器后编译正常。

## BUG-005：部署阶段单位提前开战

- **现象**：部署阶段部署的英雄立刻索敌、移动、攻击（与敌方自动部署英雄隔线对打），部署失去意义。
- **原因**：单位 Tick 无战斗开关，FSM 全程运转。
- **修复**：新增 `bCombatEnabled` 战斗开关：单位 Tick 冻结（不索敌/移动/攻击，强制 Idle）；英雄技能、兵营出兵同样受控；GameMode 在 `SetPhase` 时统一开关全场单位（部署→关，开战→开，结算→关），新生成单位按当前阶段初始化。
- **涉及文件**：`ALKUnitBase.h/.cpp`、`ALKUnitHero.cpp`、`ALKUnitBuilding.cpp`、`ALKBattleGameMode.h/.cpp`
- **验证**：部署阶段双方静止对峙，点"开始战斗"瞬间全部激活。

## BUG-006：哨塔目标卡死发呆（FSM 走查发现）

- **现象**：敌人走进哨塔射程又离开后，哨塔永远发呆；即使有其他敌人进入射程也不攻击。
- **原因**：`AcquireTarget` 对"仍存活的目标"直接保留；哨塔的 `UpdateStateMachine` 发现目标超射程时只置 Idle 却**不清除目标** → 索敌永远返回旧目标。
- **修复**：哨塔判定目标无效（死亡/超射程）时 `TargetActor = nullptr`，下个 0.5s 重试周期重新索敌。
- **涉及文件**：`ALKUnitBuilding.cpp`
- **验证**：放箭塔 + 两个敌人，一个走出射程，塔应转火射程内的另一个。

## BUG-007：射程边界抖动（FSM 走查发现）

- **现象**：目标在射程边界来回走动时，单位"移动→停下→攻击→移动"反复抽搐，观感差。
- **原因**：状态切换只有一个距离阈值（Dist > Range 移动，否则攻击），边界处来回穿越阈值即抖动。
- **修复**：加入**滞回区（Hysteresis）**：进入攻击用 `AttackRange`，退出攻击用 `AttackRange + AttackStopBuffer`（默认 30，可配）。移动中必须进入 Range 才停；攻击中要到 Range+30 才追。
- **涉及文件**：`ALKUnitBase.cpp`（UpdateStateMachine）、`ULKGameData.h`（新增 `AttackStopBuffer`）
- **验证**：让目标沿射程边界移动，单位稳定站定攻击不再抽搐。

## BUG-008：单位可走出战场边界（FSM 走查发现）

- **现象**：混战时单位被软碰撞挤出/追敌追出白色战场框。
- **原因**：移动组件只有位移+分离，无边界约束。
- **修复**：`ULKUnitMovementComponent` 新增战场边界钳制（`SetFieldBounds`，InitUnit 时由 GameData 注入），移动+分离后每帧把位置钳制回战场矩形内。
- **涉及文件**：`ULKUnitMovementComponent.h/.cpp`、`ALKUnitBase.cpp`（InitUnit 注入）
- **验证**：把单位推到角落混战，任何单位不越过白框。

## BUG-009：攻击已死亡目标（鞭尸）（FSM 走查发现）

- **现象**：目标死亡后 0.5 秒销毁期内，攻击者继续对其发起攻击（无伤害但浪费攻击帧）。
- **原因**：`TryAttack` 只查了 `IsValid`（未销毁），没查 `IsDead`（已死亡未销毁）。
- **修复**：`TryAttack` 增加 `IsDead()` 检查，命中即清除目标。
- **涉及文件**：`ALKUnitBase.cpp`
- **验证**：目标死亡瞬间攻击者立刻停手并转火（配合索敌线调试可观察）。

## BUG-010：打牌后手牌整体左移、新牌固定补在末尾（Sprint 2 测试发现）

- **现象**：打出第 1 张手牌后，其余 3 张向左移动一格，新牌总是出现在最后一个槽位——玩家视线被迫追踪卡片，且多次出牌后"记住哪个槽位是什么牌"的肌肉记忆失效。
- **原因**：`ULKDeckState::PlayCard` 用 `RemoveAt(打出位置)` + 末尾 `Append` 补牌，数组语义是"队列滑动"而非"固定槽位替换"。
- **修复**：改为**原地替换**——打出的槽位直接写入牌堆顶的新牌（其他手牌位置不动）；牌堆空时先洗回弃牌堆。皇室战争式标准手牌行为。
- **涉及文件**：`ULKDeckState.cpp`（PlayCard）
- **验证**：连打多张不同位置的牌，各槽位牌保持原位刷新，无左移。

## BUG-011：Paper2D 精灵在俯视战场中"立着"像卡片（Sprint 2 测试发现）

- **现象**：部署英雄后，精灵像一张立着的卡片（竖在 XZ 平面），不是 2D 俯视常见的"平铺在地面"画面。
- **原因**：查 5.8 引擎源码（`Paper2DModule.cpp`）确认：Paper2D 精灵几何轴为 `宽边→世界 X、高边→世界 Z、正面→世界 Y`——精灵**天生竖立**，是为横版游戏设计的；俯视相机（Pitch=-90，从 +Z 看）正好看到侧影。
- **修复**：`ALKUnitBase` 构造函数中给精灵组件设旋转（`FRotationMatrix::MakeFromXY` 由基向量构造，避免手算欧拉角）：正面→世界 +Z（朝相机）、宽边→世界 +Y（屏幕右）、头→世界 +X（屏幕上方）。
- **涉及文件**：`ALKUnitBase.cpp`（构造函数，含 `Math/RotationMatrix.h`）
- **验证**：PIE 部署英雄，精灵平铺地面、角色头朝屏幕上方。（若头朝下/镜像：把 `MakeFromXY` 第一个向量 Y 取反。）

## BUG-012：调整过 SpriteScale 的单位精灵变形（BUG-011 修复引入）

- **现象**：在 `DT_Units` 中设置非等比 SpriteScale（如 X≠Y）的单位，精灵被错误拉伸变形。
- **原因**：旋转加在精灵组件上，但 SpriteScale 缩放加在整个 **Actor** 上——Actor 缩放沿**世界轴**生效，旋转后精灵"宽"沿世界 Y、"高"沿世界 X，导致 `SpriteScale.X` 缩了高、`.Y` 缩了宽（宽高互换）；且 Actor 缩放还连带拉伸碰撞盒与调试色块。
- **修复**：改为 `SpriteComponent->SetRelativeScale3D(FVector(Scale.X, 1.f, Scale.Y))`——缩放作用在组件**本地坐标**（宽=本地X、高=本地Z），组件变换"先本地缩放再旋转"，任何朝向都不变形；移除 Actor 级缩放。
- **涉及文件**：`ALKUnitBase.cpp`（InitUnit）
- **验证**：PIE 部署调过非等比 SpriteScale 的单位，宽高比正确、碰撞不受影响。
- **教训**（面试可讲）：UE 中"缩放作用域"三选——组件本地（随旋转）、Actor 世界轴、Actor 本地——选错会在旋转场景下产生轴错位。

> **待排查（数据侧，非代码）**：手牌单位仍显示绿框（英雄正常）——代码路径相同，疑为 `DT_Units` 佣兵行 Sprite 字段为空或行名与卡 SpawnUnitId 不一致。排查命令：`SpawnUnit Unit_Swordsman 0 0 -300`（出绿框=行数据问题，出精灵=卡牌链路问题）。

## BUG-013：调试命令 `SpawnUnit` 的坐标与阵营半场不一致（测试发现）

- **现象**：`SpawnUnit Unit_Swordsman 1 0 -300`（期望敌方半场）实际生成在玩家半场；单位有精灵后无阵营色标，极易误判"己方出兵"。
- **原因**：命令按世界坐标原样生成；半场约定是**玩家 Y<0（左）/ 敌方 Y>0（右）**，而示例与用户直觉用了反号坐标；文档示例本身也写错（`1 300 -400` 是敌方生成在玩家半场）。
- **修复**：`ULKCheatManager::SpawnUnit` 增加**半场自动镜像**——坐标落在对方半场时镜像回本阵营半场并打 Warning 日志；同时修正头文件注释与教程文档示例。
- **涉及文件**：`ULKCheatManager.cpp/.h`、`docs/06-Sprint2Guide.md`
- **验证**：`SpawnUnit Unit_Swordsman 1 0 -300` → 日志提示镜像，单位出现在敌方半场；`0 0 -300` → 玩家半场不变。
- **教训**：调试工具的"坐标约定"也要文档化并给出正确示例；有精灵后建议尽快加阵营标识（如脚下光环），否则分不清敌我。

## BUG-014：超时虚弱结算导致 EnsureFailed（"Array has changed during ranged-for"）

- **现象**：战斗进入超时一段时间后（虚弱反复扣血把英雄打死时），日志报 `EnsureFailed: Array has changed during ranged-for iteration`（`ALKBattleGameMode.cpp` TickOvertime），场上己方英雄"突然消失"、结算行为错乱。
- **原因**：`TickOvertime` 用 range-for 直接遍历 `AliveHeroes[TeamIdx]`，循环内 `ApplyMaxHealthPercentDamage` 把英雄打死 → 同步触发死亡回调 `HandleUnitDied` → 从**同一个数组** `Remove` 该英雄 → 遍历中途数组被修改，checked 迭代器检测报错（英雄消失其实是死亡后 0.5s 销毁，ensure 让本次结算循环漏结算/错乱）。
- **修复**：遍历前先复制快照 `const TArray<ALKUnitBase*> HeroesSnapshot = AliveHeroes[TeamIdx];` 再遍历副本——循环内删原数组安全。同类隐患自查：其余 `AliveHeroes` 使用点（HasMage/GetTeamHeroHealthRatio/GetRandomAliveHero）均为只读，无此问题。
- **涉及文件**：`ALKBattleGameMode.cpp`（TickOvertime）
- **验证**：`BattleTimeLimit=60` 拖到超时，双方英雄陆续被虚弱磨死（日志 `[Unit] xxx 阵亡` 正常、无 ensure），一方全灭判负。
- **教训**（面试可讲）：UE 里"回调同步修改容器"是最常见的迭代陷阱——凡是循环体里可能触发广播/回调的遍历，先拷快照（或延迟处理）；开发版 checked 迭代器会替你抓出来，发行版则是静默 UB，务必修在源头。

## BUG-015：远征房间敌方英雄继承上一房状态（"第三间亡灵法师开局 40%"）

- **现象**：地牢远征（D0~D2）中，第三间"骷髅王座"的敌方亡灵法师开局只有 40% 生命。房间切换时本应只有己方英雄跨房继承（战后 +40% 恢复），敌方单位每房按遭遇配置满血重建。
- **原因**（两层）：
  1. **规则层**：D0 定稿的 `FinalizeHeroRecovery` 对**双方**英雄都执行战后 +40% 恢复（失能者 0%→40% 并在结算画面"复活站场"）。这套行为在独立单场成立（LKD0Tests 也断言敌方恢复），但在**跨房远征**里敌方恢复没有任何消费方——敌方下一房由遭遇配置全新建造，恢复值既不继承也无意义，只造成"敌方带 40% 状态进入下一房"的错误观感与潜在误用（Outcome.EnemyHeroes 快照唯一读者是 BattleHistory）。
  2. **边界层**：跨房继承入口（`DeployHero`）虽已限定 `Team==Player`，`ULKRunSubsystem::SubmitBattleOutcome` 也只消费 `PlayerHeroes` 且校验 ID 集合——但"敌方英雄结算后以 40% 复活站场"的中间状态没有显式规则与日志，难以区分"生成链路带血"与"结算恢复复活"。
- **修复**（按用户规则"敌方单位状态不跨房间继承"落地，且不写死到骷髅单位——敌方可以是骑士/法师等任何单位，规则按**阵营**分类）：
  1. `FinalizeHeroRecovery`：远征房间（`bExpeditionBattle`）**只对玩家英雄执行战后恢复**；敌方英雄跳过恢复，快照如实记录结算时状态（失能即 0），仅存 BattleHistory。独立单场保持旧的双方恢复（测试与调试兼容）。
  2. 继承边界显式化：`DeployHero` 玩家分支注释"跨房继承仅限玩家英雄，按 HeroId 精确匹配"；`SubmitBattleOutcome` 注释并 Verbose 日志声明"Outcome.EnemyHeroes 永不写入 State.Heroes"，既有 ID 集合校验继续挡住任何敌方/未知 ID 混入。
  3. 部署血量埋点日志：`AutoDeployDefaultHeroes`（敌方部署后）、`DeployHero`（玩家继承后）、`SpawnUnitForTeam`（任何英雄生成）分别打印 `HP 当前/最大（BaseMax）`——若再出现非满血开局，日志可直接区分"生成链路问题"与"开战后被改血"。
- **涉及文件**：`ALKBattleGameMode_Undead.cpp`（FinalizeHeroRecovery）、`ALKBattleGameMode.cpp`（部署/生成埋点+注释）、`ULKRunSubsystem.cpp`（边界注释）、`docs/01-GDD.md`、`docs/17-DungeonDevelopmentPlan.md`、`docs/18-DungeonChangeLog.md`
- **验证**：远征房 1→2→3 全程日志：敌方部署日志 `[Run] 敌方自动部署：Hero_Necromancer HP 100%/100%`（每房满血）；`[Run] 敌方英雄 xxx 跳过战后恢复` 在房 1/2 结算出现；`ListRunState` 的 Heroes 始终只有玩家三英雄；LKD0 的独立单场敌方恢复断言不变（非远征路径保留）。
- **教训**（面试可讲）：一条规则（"战后全员恢复"）在引入"跨房持久化"后会悄悄越过边界——恢复/存档这类"为下一阶段准备状态"的逻辑，必须与"谁会消费这个状态"绑定；无人消费的恢复既是浪费也是 bug 温床。修复时先按**阵营/角色分类**定规则（我方英雄=跨房继承的唯一载体），再在生成、结算、提交三个边界各加一道显式校验与日志。

## BUG-016：放开营地范围后，指挥英雄到敌人身边会"无限追赶、永不攻击"

- **现象**：营地活动范围放开（英雄可全图移动）后，把英雄指挥/自动走向敌方英雄身边，双方贴脸却不动手（至少我方英雄永不攻击）。
- **原因**：手动移动的"到达判定"要求距指令点 ≤5 世界单位。把指令点下在**移动中的敌方英雄身上/附近**时，敌人一直走动（或互相分离推挤），英雄永远追不到指令点 → `bManualMoving` 永远为真 → `ALKUnitHero::UpdateStateMachine` 的手动分支每帧 `return` 早退，**永不进入索敌/攻击逻辑**。放开营地圈之前，指挥范围被营地半径拴住（指令点基本是静态可达点），该场景几乎不出现；放开后玩家可把英雄指挥到敌人脸上，问题暴露。
- **修复**：手动移动中额外检查两个"结束手动"条件——① 已到达指令点；② **敌人已进入攻击距离**（最近可追敌人 ≤ 攻击射程）。任一满足即 `bManualMoving=false` 并**直接锁定该敌人**交给战斗 FSM（下方 Super 立即索敌交战）。这样"指挥到敌人面前=进攻指令，走到即开打"；追不上移动目标时，一旦被敌人贴近也会立即反击。
- **涉及文件**：`ALKUnitHero.cpp`（UpdateStateMachine 手动分支）
- **验证**：新增自动化 `LittleKing.Sprint5.Heroes.FreeRoamFightRegression`（开战后指挥骑士直奔敌方英雄 + 贴脸生成单位）：修复前 `Manual order ends once combat is possible` 断言失败（手动永不结束），修复后通过；全量 `LittleKing` **33/33 Success**。
- **教训**（面试可讲）：状态机"手动接管/自动接管"切换的终止条件必须收敛——任何以"到达某个点"为唯一退出条件的接管逻辑，在目标可移动/目标点不可达时都会死锁；退出条件应包含"能力已经完成它的目的"（这里是：能打了就该打），而不是只测几何距离。

## BUG-017：法师在场却"没有全场施法特性"（旧档快照丢失身份特性）

- **现象**：远征里法师明明还活着站在场上，在**敌方半场**点法术却提示"没有全场施法特性：法术只能放在己方半场"（旧蓝图 HUD 分支文案为"法术未解锁（需要法师英雄在场）"）；按规则只有法师死亡后才应退回"仅己方半场"。
- **原因**（调用链取证）：
  1. 全场施法只有一个判据：`ALKBattleGameMode::HasGlobalSpellPlacement` 遍历 `AliveHeroes[阵营]`，要求某个存活英雄 `HasTraitEffect(GlobalSpellPlacement)`；该效果由特性 `Trait_MageSpellReach` 解析而来。法师在场时该判据为假，只可能是**英雄身上没有这条特性**。
  2. 英雄上场时的永久特性来自远征快照：`ALKUnitBase::ApplyRunHeroState` 用 `FLKRunHeroState.Traits` **整体替换** `HeroTraits`（这是 D2"房间间增删永久特性"的设计）。因此若某个远征快照里没写身份特性，`InitUnit` 里由 `DA_GameData.DefaultHeroTraits` 加上的 `Trait_MageSpellReach` 会被随后上场的快照覆盖掉——法师"有身份、没特性"，全场施法被静默关掉。
  3. 快照缺身份特性的来源：早期构建的 `BuildInitialRunHeroes` 尚未接入 `DefaultHeroTraits`（或旧存档），这类远征一旦落盘，后续每次载入都缺。
- **修复**（按"身份特性属于英雄身份，不随旧档缺失而丢失"落地）：
  1. `ULKRunSubsystem::RestoreHeroIdentityTraits(IdentityTraits)`：载入存档后按当前代码补全缺失的身份特性（只加不删，保持 D2 的"房间间临时增删其他特性"语义），补全后按安全点自动保存。
  2. **同步待开战上下文**：`PendingBattle.PlayerHeroes` 是英雄快照的副本，补全后必须一并刷新，否则部署时仍按旧副本应用特性（首次修复漏了这一步，被新自动化当场抓住：`[Run] Hero_Mage 应用永久特性：无`）。
  3. 身份特性表由 GameMode 汇总（`CollectIdentityTraits`）：`DA_GameData.DefaultHeroTraits` + 单位行内代码特性（如盾卫 `Taunt`），不写死英雄列表。
  4. 排查埋点：`ApplyRunHeroState` 打印 `[Run] %s 应用永久特性：…`；敌方半场施法被拒时打印存活英雄及其特性（`[Spell] … 施法被拒绝：存活英雄 …`）。
- **涉及文件**：`ULKRunSubsystem.h/.cpp`、`ALKBattleGameMode.h`、`ALKBattleGameMode_Run.cpp`、`ALKBattleGameMode.cpp`、`ALKUnitBase.cpp`、`Tests/LKSprint5Tests.cpp`、`Tests/LKD5Tests.cpp`
- **验证**：
  - 新增 `LittleKing.Sprint5.Heroes.AuthoredSpellTerritory`：手工造一份"缺身份特性"的旧档写入固定槽 → 真实 `BP_ALKBattleGameMode` 冷启动 → 日志出现 `身份特性补全：Hero_Mage + Trait_MageSpellReach` → 部署三英雄 → `HasGlobalSpellPlacement` 为真、敌方半场可施法；法师死亡后敌方半场重新锁定、己方半场仍可用。
  - 新增 `LittleKing.D5.Save.IdentityTraitsRestoredOnLoad`：旧档载入后补全 2 条身份特性、玩家自加特性不受影响、二次补全为 no-op、`BeginCurrentBattle` 上下文带上补全后的特性。
  - 全量 `LittleKing` **36/36 Success，0 failed**（`Saved/Automation/Bug017b/index.json`，精简摘要 [validation/BUG017-Validation.json](validation/BUG017-Validation.json)）。
- **教训**（面试可讲）："快照覆盖"型状态恢复一定要区分**身份数据**与**可变数据**——身份特性（职业能力）应由代码/配置拥有，快照只记录可变部分；否则一次格式演进就会让老存档悄悄丢掉核心能力，而且不报错、只在玩法层表现为"技能不生效"，极难定位。定位这类问题的关键是：先把"判据 → 数据来源 → 数据写入点"整条链路的唯一可能失效点找出来（本例只有"快照缺特性"一种可能），再用日志把该点钉死。

## BUG-018：战斗中点击营地移动"没反应"（移动指令被战斗状态当场取消）

- **现象**：英雄正在战斗中时，点自己的营地再点地面下达移动指令，英雄原地不动（也没有任何提示），看起来像"点击无效"。
- **原因**：`ALKUnitHero::UpdateStateMachine` 的手动移动分支里有一条"敌人已进入攻击距离就结束手动"的退出条件（BUG-016 为修"指挥到移动目标身上永不交战"而加）。战斗中英雄的目标几乎总在攻击距离内，于是 `CommandMove` 刚把 `bManualMoving` 置真，下一帧就被这条条件清掉并直接转回战斗 FSM——指令等于没下。这个条件对"贴着敌人"下达的移动指令尤其致命（而玩家想做的正是战斗中撤退/换位）。
- **修复**（按用户规则"移动指令强行打断战斗，到达后恢复自动战斗"）：
  1. 手动移动期间**不再**因敌人进入攻击距离而结束：不索敌、不攻击，只走向落点（`CancelAttackWindup` + 提前 return）。
  2. 指令结束条件改为两种：**到达落点**（≤5 世界单位）或**卡住超时**——连续 `DA_GameData → Camps → HeroMoveStuckTimeout`（默认 1.5 秒）无位移（落点被单位/建筑占据、或贴脸被软分离推挤）就结束指令并立即恢复自动战斗。这条是 BUG-016 场景的收敛出口，避免"走不到点也永不打架"。
  3. 结束时清空目标让战斗 FSM 重新索敌（保留嘲讽/集火优先级）；`RallyPoint` 就是落点，之后没有敌人时英雄会回到该位置。
- **涉及文件**：`ALKUnitHero.h/.cpp`（手动移动分支 + 卡住计时）、`ULKGameData.h`（`HeroMoveStuckTimeout`）、`Tests/LKSprint5Tests.cpp`
- **验证**：
  - 新增 `LittleKing.Sprint5.Heroes.ManualMoveInterruptsCombat`：贴脸生成敌人 → 确认已交战 → 下移动指令 → 断言"立即打断战斗（目标清空）"、"0.5 秒后仍在手动移动且真的在走"、"移动期间不重新索敌"、"到达/卡住后结束并重新索敌"；再用"指令落点=敌人当前位置"验证卡住超时后能恢复战斗。
  - `LittleKing.Sprint5.Heroes.FreeRoamFightRegression` 按新语义更新（指令在到达或卡住后结束，不再因"能打"而结束）。
  - 全量 `LittleKing` **37/37 Success，0 failed**（`Saved/Automation/Bug018b/index.json`，摘要 [validation/BUG018-Validation.json](validation/BUG018-Validation.json)）。
- **教训**（面试可讲）：给状态机加"提前退出条件"时要问清楚它会不会把玩家刚下达的指令吃掉。BUG-016 的退出条件解决的是"自动追击追不上移动目标"，却顺手让"战斗中主动换位"失效——两个需求其实需要**不同的**退出条件（前者用"打得到就打"，后者用"到达或卡住"）。正确做法是把"指令是否仍然有效"与"能不能打到敌人"解耦，并给指令配一个可收敛的兜底（卡住超时）。

---

## BUG-019：安全点"暂回家园"后继续远征，战斗不开始（双方面对面却不攻击）

- **现象**：一间房打完后点"暂回家园"回到家园，再点"继续远征"回到战斗地图，部署三名英雄后双方单位面对面站着、谁也不攻击；画面看起来像 AI 坏了。
- **排查（取证）**：
  1. 自动化按同一路径复现（打一房 → 跳过奖励停在节点选择 → 保存 → 新 GameInstance 载入存档 → 开战斗地图 → 部署三英雄）：`CanStartBattle()` 为真、阶段进入 Battle、把骑士挪到敌人身边后双方互相掉血 —— 说明 C++ 状态机与"继续"链路本身是正确的。
  2. 用户日志里那次失败：`[Run] 加载第 2/4 战 Enc_Dyn_R2` 与三名英雄的部署日志都在，但**完全没有 `[Battle] 阶段切换 -> 1`** → 战斗阶段从未开始；而 `SetPhase` 在 `CanStartBattle()` 为假时是**静默 return** 的，所以既没有日志也没有任何提示，只能看到"单位冻结、面对面不攻击"。
  3. 顺带发现一个真实缺陷：若在**奖励面板**点"暂回家园"，继续时 `BeginCurrentBattle` 会因为运行阶段还是 ChoosingReward 而失败，世界**静默退化成独立单场假战斗**（没有远征身份、也推进不了远征，回来还会再进同一场假战斗）。
- **修复**：
  1. `ALKBattleGameMode::SetPhase` 拒绝进入战斗阶段时打印完整判据（阶段 / 玩家状态就绪 / 牌库有效 / 双方名单与部署数、场上英雄数）——下次一眼定位。
  2. `ALKPlayerController::RequestStartBattle` 每次请求都记一行（可确认"点击是否到达 C++"以及被哪条判据挡住）。
  3. 原生兜底：**空格 / 回车**也可以开始战斗（不依赖蓝图 HUD 按钮的界面状态），原生 HUD 的部署提示同步写明。
  4. 真缺陷修复：远征进行中但当前**不是开战点**（例如停在奖励面板）时，战斗地图进入"恢复中枢"显示奖励/路线面板，不再创建假战斗；恢复中枢/终态世界**禁止部署英雄**（否则会出现"能摆兵却永远开不了战"的世界）。
- **涉及文件**：`ALKBattleGameMode.h/.cpp`、`ALKPlayerController.cpp`、`ALKPresentationHUD.cpp`、`Tests/LKHomeTests.cpp`
- **验证**：
  - 新增 `LittleKing.H3.Resume.SafePointLeaveAndContinue`：完整复现"打一房 → 安全点保存 → 新会话载入 → 战斗地图 → 部署 → 开始"，断言进入 Battle 阶段且贴脸后双方真的互相掉血。
  - 全量 `LittleKing` **48/48 Success，0 failed**（`Saved/Automation/Home7/index.json`）。
- **教训**（面试可讲）：状态机里"静默 return"是这类"看起来像 AI 坏了"的问题的元凶。凡是拒绝一次玩家可见的状态转移（开战、出牌、阶段推进），都应该留下**可诊断日志**说明被哪条判据挡住；同时给玩家留一条不依赖 UI 状态的兜底通路（这里是键盘开始），否则一个界面小问题会被误判成战斗逻辑崩了。

---

## 附：调试工具（同批加入）

- **索敌线**：`bDrawDebugShapes` 开启时，每个单位绘制"单位→当前目标"的连线（绿=玩家/红=敌方），调 AI 行为一眼可见。文件：`ALKUnitBase.cpp`（DrawDebugShape）
