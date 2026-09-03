# Bug 修复记录（Bug Log）

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

---

## 附：调试工具（同批加入）

- **索敌线**：`bDrawDebugShapes` 开启时，每个单位绘制"单位→当前目标"的连线（绿=玩家/红=敌方），调 AI 行为一眼可见。文件：`ALKUnitBase.cpp`（DrawDebugShape）
