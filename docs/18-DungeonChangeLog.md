# 地牢开发 · 协作变更记录

更新：2026-09-08，UE 5.8.1。当前已完成 **D0～D3 + BUG-015 修复**，不再归已结束的 Sprint 5，也不恢复 Sprint 6。最近发布基线为 v0.5；本文记录其后的工作区修改。

## 本轮交付

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| D0-01 | 定稿三战切片、战后全员 +40%、奖励可跳过、首版无远征货币 | 旧“阵亡者 50% / 每房满血”建议失效；正式规则见 01 / 17 |
| D0-02 | 实现 RunState、Hero/CardState、Node、BattleContext、BattleOutcome、阶段枚举 | 反射值类型，不持有 Actor/ASC；RunSubsystem、切图、奖励和存档执行器留 D1～D5 |
| D0-03 | 记录初始化/结算接入入口；新增 GetBattleOutcome | 独立战斗有唯一 AttemptId；D1 才注入远征 RunId/NodeId 并防重复推进 |
| D0-04 | 固定遭遇、节点、奖励 ID 与资产路径/归属 | 清单在 17；本轮不新增或保存 .uasset |
| D0-R01 | 英雄 0 血失能，实例不销毁，不能攻击、移动、被选为攻击目标或被普通治疗救起 | bDead / Die / OnUnitDied 保留旧接口名，英雄语义为失能；佣兵、普通建筑照常死亡销毁 |
| D0-R02 | 每场结束双方全部英雄增加最大生命的 40%，封顶；失能者回到 40% | 20%→60%、90%→100%、0%→40%；恢复不增加战斗治疗统计，不改胜负，不重开战斗 |
| D0-R03 | Outcome 保留每名英雄的失能标记、恢复前血量、恢复后生命/最大生命/特性 | D1 只消费一次恢复后值；英雄失能后仍保留营地，当场不能指挥 |
| D0-C01 | 新增骷髅兵、骷髅射手、死灵法师、骷髅巨人、骷髅王 | 较低基础数值；新单位无需 Sprite，以不同颜色方框和名称显示，见 19 |
| D0-C02 | 独立被动组件：亡灵召唤、巨骨、朽骨再生 | 事件驱动；不需要制作三个 GA；被动与可增删的英雄特性分开 |
| D0-C03 | 新增献祭与直面恐惧特性 | 通过 AddTrait / RemoveTrait 可动态增删；删献祭只取消自伤，不删除召唤被动 |
| D0-C04 | 伤害来源记录远程分类，发射后来源销毁仍能正确减伤 | 法术/远程建筑/远程单位技能均受 30% 减伤；近战、虚弱、自伤代价不受此减伤 |
| D0-C05 | 批量失能事件先触发召唤、计数、复活，再判断胜负 | 同批王先倒下/小兵先倒下结果一致；献祭追加失能继续排队处理 |
| D0-C06 | 提供 Patrol / Elite / Boss 三个部署期遭遇预设 | 敌方名单独立；玩家仍必须部署全部三英雄；亡灵用有限波次，不偷偷使用旧人类牌组 |

具体歧义采用以下实现：同一具佣兵遗体只转换一次，由最近的仍可行动死灵法师召唤，等距按实例名稳定选择；转换不限距离，保留死亡位置和攻击类型。成功生成才付 8% 最大生命，献祭可让自己失能且穿透无敌；单位上限或实体占用阻止生成时不扣血、不延后补召。死灵法师不属于骷髅，不能为王加 5 点。王不计自己的失能，满额只在再次倒下时触发复活，不能让此前已经失能的王靠以后死亡补满再起身。巨人失能后也不能靠巨骨起身。

## 文件交接

| 文件（Source/little_king 下） | 变化 |
|---|---|
| **新增** LKRunTypes.h | D0 数据契约、恢复公式 |
| **新增** LKUndeadContent.h/.cpp | 五单位默认行；D2 已将遭遇定义迁到 LKEncounterContent |
| **新增** ULKUnitPassiveComponent.h/.cpp | 召唤、巨人治疗、王计数/复活 |
| **新增** ALKBattleGameMode_Undead.cpp | 批次被动处理、战后快照、遭遇切换 |
| **新增** Tests/LKD0Tests.cpp | 六组世界集成检查；只读读取现有数据/技能参数用于图鉴 |
| ALKUnitBase.h/.cpp | Boss 身份、失能保留、恢复、特性效果值、占位色、被动组件 |
| ALKBattleGameMode.h/.cpp | 敌方阵容、生成 Boss、运行时配置副本、死亡队列、恢复结算、原生内容回退 |
| LKTypes.h、LKDataTypes.h、ULKGameData.h | 被动/种族/颜色字段、新特性效果、远程来源、恢复比例和可选遭遇 |
| LKGameplayHelpers.cpp | 远程来源快照与减伤；保留实际伤害统计 |
| ALKOpponentBrain.h/.cpp | D0 固定波次入口；D2 扩展为按遭遇配置波次、牌组、经济与战术开关 |
| ALKPresentationHUD.cpp | 五种彩色方框、中文名称、失能标记、王的计数/门槛 |
| ULKCheatManager.h/.cpp | UndeadEncounter；KillAll 使用同批次快照 |
| README、docs/01、02、03、15、16、17 | 当前入口/规则更新，历史说明与 D0 状态同步 |
| **新增** docs/18、19、20、validation/D0-Validation.json | 本记录、全内容图鉴、UE 5.8 教程、可追溯验证摘要 |

D0 当时先对五个亡灵单位合并 DT_Units 同名行。D1-07 已把同一模式扩展到全部十三个内置战斗单位：名称、战斗数值和表现资源使用作者值，玩法身份、被动、固有特性、建筑行为和产兵类型由代码注册表校正；内置单位缺表行时使用当前图鉴对应的代码默认值。未知的自定义 UnitId 仍完全由数据表定义。已有 DA_GameData 在开始时复制为运行时对象，切换遭遇不会保存到原资产。

## 验证与已知问题

2026-09-08 D3 最终验证：UE **5.8.1-56057345**，Editor / Win64 / Development 构建通过；完整 `LittleKing` 自动化 **26/26 Success，0 failed，0 notRun**，其中 10 项无警告、16 项带已知测试环境警告。奖励专项覆盖候选边界、原子领取、重复请求、牌组升级和领取前“下一关”锁定；真实 `BP_ALKBattleGameMode + WBP_BattleHUD` 集成检查确认待领奖励会打开原生面板，通过 HUD 跳过后关闭并解锁下一关。五个项目蓝图只读编译 **0 errors / 0 warnings / 0 failed to load**。完整摘要见 [validation/D3-Validation.json](validation/D3-Validation.json)。NullRHI 不检查像素级布局和鼠标手感，实际画面验收步骤见 [23-D3RewardTutorial.md](23-D3RewardTutorial.md)。

2026-09-07 本机实测：UE **5.8.1-56057345**，Editor / Win64 / Development 构建通过；完整 `LittleKing` 自动化 **16/16 Success，0 failed，0 notRun**（原 Sprint 5 十组 + D0 六组）。五个项目蓝图只读编译 **0 errors / 0 warnings / 0 failed to load**。

新增检查覆盖：存活/失能英雄恢复、封顶、结算重复调用、战内不能治疗救起；近远程遗体转换、多个死灵法师、献祭移除/恢复、自伤致失能、满单位不扣血；巨骨阵营/种族/上限；王 1/5 计数、封顶、10→15→20 门槛、同批次两种死亡顺序、复活后存活登记、最后一击统计；远程普攻/建筑/技能/法术、近战与虚弱、发射者销毁后弹道分类；新单位在实际数据表未添加行时仍可用、敌方阵容不影响玩家部署门槛、只执行亡灵波次。

13 组带有测试世界无相机、GameplayCue 路径回退等日志警告；召唤上限用例还会主动触发一次单位上限提示。全部为 Success，不把警告写成无警告。完整报告在忽略目录 `Saved/Automation/D0Undead`，摘要见 [validation/D0-Validation.json](validation/D0-Validation.json)。

D0 读取技能图时曾发现 `GA_MageNova` / `GA_RangerShot` 的取敌输入漏连，且游侠误用治疗节点。用户随后修正并保存这两份蓝图；D1 自动图断言确认两者 Unit 已连接，游侠已有伤害节点且无旧治疗节点。操作仍可按 [20 第 6 节](20-D0UndeadTutorial.md) 复查。

该 D0 批次当时未做实际画面/鼠标 PIE 目视复核、音效/美术、打包、硬件与性能专项。之后 D2 已完成遭遇数据化与永久状态，D3 已完成奖励和原生 UI；分支路线与存档仍未实现。

## 下一位协作者

原 D2 交接已由 D3 落地。下一位代码协作者从 17 的 D4 清单继续，先读 ULKRunSubsystem、LKRunTypes、LKEncounterContent 和 ULKRunRewardWidget；路线必须复用已存在的遭遇/奖励原子入口，不修改 DT_Units、DT_Encounters 或共享 DA_GameData 来保存单次远征变化。

用户维护的 `DT_Units`、`DT_Traits`、`GA_MageNova`、`GA_RangerShot` 保存改动未被本轮覆盖或重存。D2 新增的 `Content/Data/DT_Encounters.uasset`、JSON 源和生成脚本由本轮助手维护。后续对奖励界面换肤或修改 GA 时，在这里追加“操作者、资产路径、修改目的、编译/PIE 结果”，避免多人保存同一资产。原 v0.5、Sprint 5、D0 和 D1 验证文件保留历史。

## D1 追加交付（2026-09-08）

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| D1-01 | 新增 `ULKRunSubsystem` | GameInstance 生命周期内持有 RunState；创建、重开、进入战斗、提交结果、推进唯一下一房均检查阶段 |
| D1-02 | 固定三房节点和三种亡灵遭遇 | `Node_Battle01 → Node_Battle02 → Node_Boss03`；每次重载同一战斗地图，不保留上一房 Actor |
| D1-03 | GameMode 消费 BattleContext | 玩家英雄、远征卡组、敌方阵容、遭遇和独立种子写入运行时配置；部署英雄时恢复特性及生命 |
| D1-04 | Outcome 幂等提交 | RunId、NodeId、AttemptId、bFinalized 和三名英雄快照全部有效才接受；历史与已处理 AttemptId 留在 RunState |
| D1-05 | 结算按钮原生接管 | 复用 WBP 的 `Btn_Restart`；清除旧无效点击，前两房胜利显示“下一关”，失败或第三房结束显示“从头开始” |
| D1-06 | 亡灵表行防漏合并 | 测试发现四行没有有效占位颜色，因此将五个亡灵的身份/被动/特性/占位色设为代码不变量，仍允许数据表调战斗数值 |
| D1-07 | 将规则/调参分层扩展到全部旧单位 | 新增十三单位统一注册表；骑士/法师/游侠、剑士/弓手/盾卫、箭塔/兵营也由代码锁定玩法身份，自定义 ID 继续完全数据驱动 |

新增/主要修改文件：

- `ULKRunSubsystem.h/.cpp`：远征状态机与固定图。
- `ALKBattleGameMode_Run.cpp`、`ALKBattleGameMode.h/.cpp`：上下文应用、英雄状态恢复、结果按钮命令和提交顺序。
- `ULKBattleHUDWidget.h/.cpp`：运行时绑定现有结算按钮、刷新中文标签、阻止双击。
- `ALKUnitBase.h/.cpp`：`ApplyRunHeroState`。
- `LKRunTypes.h`、`ULKGameData.h`：BattleHistory 与 `bEnableExpeditionFlow`。
- `LKUnitContent.h/.cpp`：十三个内置单位的代码默认值、稳定规则和表内调参合并；替代 GameMode 内零散旧回退行。
- `Tests/LKD1Tests.cpp`：固定路线、失败/重开、首领完成和真实 BP GameMode/HUD 集成；`LKD0Tests.cpp` 增加技能图语义检查。
- `docs/21-D1ExpeditionTutorial.md`、`docs/validation/D1-Validation.json`：新手操作和验证摘要。

验证基于 UE 5.8.1-56057345：Editor / Win64 / Development 构建成功；`LittleKing` **20/20 Success，0 failed，0 notRun**。D1 测试验证完整三房、失败重开、真实 `BP_ALKBattleGameMode`、`Btn_Restart` 原生绑定，以及十三个核心单位在身份字段损坏时仍恢复规则并保留调参数值。五个项目蓝图只读编译为 **0 errors / 0 warnings / 0 failed to load**。命令行测试使用 NullRHI，测试世界无相机和无 GameViewport 的警告属于夹具限制；实际视口按钮文字/鼠标点击仍按 21 做一次短 PIE。

## D2 追加交付（2026-09-08）

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| D2-01 | 新增完整 `FLKEncounterRow` / `FLKEncounterAISettings` | 敌方英雄、波次、牌组、银币、集火/反制/爆发和 RewardTier 一次性进入本房 Context |
| D2-02 | 新增 `DT_Encounters` 与统一校验 | 配置表为权威来源；缺固定三行、重复 ID、非法时间/数量/MinMax、未知单位或卡牌会明确拒绝开局 |
| D2-03 | 遭遇目录深复制进 RunState | 已开始远征不再读取共享表；GameMode 继续使用 DA_GameData 的运行时副本，不污染原资产 |
| D2-04 | 英雄快照新增 `BaseMaxHealth` | 新房先恢复永久基础最大生命，再重算特性并钳制 Health；支持房间间升级且新远征恢复默认 |
| D2-05 | 永久特性与临时状态分界 | 合法 Add/Remove 下一房生效；外来光环、ForcedTarget、预警、攻击冷却、技能冷却和动作状态清空 |
| D2-06 | 三类遭遇连战调平衡 | 普通/精英/首领奖励档 1/2/3；波次总量下调，巡逻不集火，精英/首领逐级加快集火 |
| D2-07 | 新增调试与自动验证 | `RunHeroMaxHealth`、`RunHeroTrait`、`ListRunState`；四组 D2 测试覆盖表、快照、状态边界和真实资产 |

新增/主要修改文件：

- `LKEncounterContent.h/.cpp`、`LKDataTypes.h`：遭遇行、内置回退、规范化和目录校验。
- `Content/Data/DT_Encounters.uasset`、`Content/DataSources/DT_Encounters.json`、`Scripts/CreateD2EncounterTable.py`：实际三行资产、可审查源和可重复生成入口。
- `LKRunTypes.h`、`ULKRunSubsystem.h/.cpp`：SchemaVersion 2、遭遇目录快照、BaseMaxHealth 和房间间永久变化。
- `ALKBattleGameMode_Run.cpp`、`ALKBattleGameMode_Undead.cpp`：资产目录加载、定义 ID 校验、完整上下文应用和运行时隔离。
- `ALKOpponentBrain.h/.cpp`：波次、出牌和集火独立开关；按遭遇重建经济、牌组与战术计时。
- `ALKUnitBase.h/.cpp`、`ALKUnitHero.h/.cpp`：永久状态恢复及房间临时状态清理。
- `ULKCheatManager.h/.cpp`、`Tests/LKD2Tests.cpp`：D2 调试命令和四组回归。
- `docs/22-D2EncounterTutorial.md`、`docs/validation/D2-Validation.json`：新手操作和可追溯验证摘要。

验证基于 UE 5.8.1-56057345：Editor / Win64 / Development 构建成功；`LittleKing` **24/24 Success，0 failed，0 notRun**，其中 8 组无警告、16 组仅有已记录的测试夹具警告。D2 定向测试 **4/4**；`DT_Encounters` 生成命令 0 errors / 0 warnings；五个项目蓝图只读编译 **0 errors / 0 warnings / 0 failed to load**。完整结果见 `validation/D2-Validation.json`。

## BUG-015 修复（敌方状态不跨房，2026-09-08）

用户试玩发现：第三间"骷髅王座"的敌方亡灵法师开局 40% 生命。修复按阵营分类定规则：

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| BUG-015 | 远征房间结算**只对玩家英雄**执行战后 +40% 恢复；敌方英雄跳过恢复（快照如实记录结算时状态，仅存 BattleHistory） | 独立单场（非远征）保留旧"双方恢复"调试行为；LKD0 敌方恢复断言不受影响 |
| BUG-015 | 继承边界显式化 | `DeployHero` 继承仅限 Player 分支（注释）；`SubmitBattleOutcome` 声明 EnemyHeroes 永不写 RunState.Heroes（既有 ID 集合校验挡住任何混入） |
| BUG-015 | 部署血量埋点日志 | `[Run] 敌方自动部署：%s HP …` / `[Run] 玩家英雄 %s 部署：继承 HP …` / `[Unit] 英雄生成：%s 阵营 HP …`——再出现非满血开局可区分"生成链路"与"开战后改血" |

主要修改文件：`ALKBattleGameMode_Undead.cpp`、`ALKBattleGameMode.cpp`、`ULKRunSubsystem.cpp`；规则同步 `docs/01-GDD.md`、`docs/17-DungeonDevelopmentPlan.md`；详见 [05-BugLog](05-BugLog.md) BUG-015。验证：编译通过 + 远征日志检查（敌方部署满血、结算 `[Run] 敌方英雄 xxx 跳过战后恢复`）；自动化按 `LittleKing` 全量重跑确认 24/24。

## D3 追加交付（2026-09-08，代码与 UI 完成）

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| D3-01 | 奖励状态机：`OfferRewardBatch` / `ChooseReward` / `SkipReward` 原子入口 | 仅"胜利且可推进下一间"窗口发批（ChoosingNode → ChoosingReward）；同一次胜利只发一批（`bRewardOfferedForCurrentNode`）；领取/跳过消费批次回 ChoosingNode，重复领取/旧回调拒绝；Boss 房通关与失败不发奖励 |
| D3-02 | 奖励生成（GameMode）：升级候选 + 骷髅新卡 | 候选 = 升级一张持有单位/建筑卡（法术除外）或获得未持有的 `Unit_Skeleton`/`Unit_SkeletonArcher`；奖励档越高新卡越多（tier1 至多 1 张、tier2 至多 2 张）；确定性种子 = Run.Seed + 房号×7919 + 档位；一次生成存定，UI 不重抽 |
| D3-03 | 卡升级应用：出牌生成单位 ×1.1^Lv | `SpawnUnitForTeam` 增加 `SourceCardId`；仅远征玩家出牌生效（波次/兵营产兵/敌方不受影响）；作用于行副本（攻击与生命），不污染共享表/资产；日志 `[Run] 卡升级应用` |
| D3-04 | 骷髅卡运行时注入 | 骷髅兵/骷髅射手 1 费文字卡：无论 DA_GameData 是否配置 CardLibrary 都注入运行时副本（不写资产）；默认牌组不变，奖励领到才进牌组 |
| D3-05 | HUD 奖励接口与防呆 | `OnRunRewardReadyBP` 事件 + `GetRewardOptionText` 等便捷函数；奖励窗口锁定"下一关"按钮（C++ 与请求入口双重防呆） |
| D3-06 | 自动化与文档 | 新增 `Tests/LKD3Tests.cpp` 两组（状态机原子性、领取边界）；D1 集成断言按新流程更新（胜利 → 奖励窗口 → 跳过 → 下一关）；教程见 [23](23-D3RewardTutorial.md) |
| D3-07 | 原生三选一奖励 UI | 新增 `ULKRunRewardWidget`；全屏遮罩、响应式三卡布局、真实升级前后数值、图标/文字占位、牌组摘要和跳过；现有 `WBP_BattleHUD` 自动接入且无需重存 |
| D3-08 | UI 生命周期与换肤入口 | 待选批次恢复唯一面板，按钮先锁后原子消费，成功后关闭并解锁下一关；`RewardWidgetClass` / `OnRewardDataReadyBP` 允许后续蓝图换肤 |

新增/主要修改文件：`LKRunTypes.h`（FLKRunRewardOffer、SchemaVersion 3）、`ULKRunSubsystem.h/.cpp`、`ALKBattleGameMode.h/.cpp`、`ALKBattleGameMode_Run.cpp`、`ULKBattleHUDWidget.h/.cpp`、`ULKRunRewardWidget.h/.cpp`、`little_king.Build.cs`、`Tests/LKD3Tests.cpp`、`Tests/LKD1Tests.cpp`、`docs/23-D3RewardTutorial.md`、`docs/03-TaskList.md`。

验证基于 UE 5.8.1：Editor / Win64 / Development 构建成功；`LittleKing` 完整自动化覆盖 D3 两组状态机测试和真实 HUD 奖励面板生命周期。原生 UI 不写任何 `.uasset`；最终计数与蓝图编译结果见 `validation/D3-Validation.json`。

## D4 追加交付（2026-09-08，代码与原生 UI 完成）

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| D4-01 | 分层节点图（固定形状、内容随机）：Start → R1 普通战 → R2（普通战/休息）→ R3（精英战/休息）→ Boss | 每局同形状：4 个战斗槽 + 2 个休息位；节点 NextNodeIds 即"下一排"（可见性天然限定，看不到后续） |
| D4-02 | 随机敌阵容（目录驱动池 + 确定性种子） | 普通房从英雄池随机 1、精英随机 2（不同）、首领房 = 首领池 1 + 英雄池 2；首领判定 = 注册表 Boss 类或 Boss 行首位；同种子同阵容 |
| D4-03 | 动态遭遇生成 `MakeDynamicEncounter` | 以模板行（固定三行）复制波次/AI/经济/奖励档，仅替换敌阵容；动态 ID `Enc_Dyn_R1/R2/R3/Boss` 入运行快照 |
| D4-04 | 状态机：`SelectNode` / `CanSelectNextNode` / `GetNextNodeIds`；休息节点回血 30% 封顶 | 休息结算后停留选择阶段继续显示下一排；战斗节点进入该房；非下一排/已结算/阶段不符一律拒绝；`AdvanceToNextBattle` 保留为"自动选下一排第一个战斗节点"兼容入口 |
| D4-05 | 原生节点选择面板 `ULKRunNodeSelectWidget` + HUD 生命周期 | 与 D3 奖励面板同模式（换肤入口 NodeSelectWidgetClass）；奖励消费后自动弹出；休息后原地刷新；结算按钮在奖励/节点选择期间锁定 |
| D4-06 | 自动化与文档 | 新增 `Tests/LKD4Tests.cpp` 三组（图结构与可见性/随机阵容规则/休息回血全通流程）；D1~D3 断言按 4 战与动态遭遇更新；教程见 [24](24-D4NodeTutorial.md) |

主要修改文件：`LKEncounterContent.h/.cpp`（池/动态遭遇）、`ULKRunSubsystem.h/.cpp`（图生成/选择）、`ALKBattleGameMode.h/.cpp`、`ALKBattleGameMode_Run.cpp`（RequestResultAction 仅服务失败/通关；节点选择 API 与文案）、`ULKBattleHUDWidget.h/.cpp`（SyncResultPanels/节点面板）、新增 `ULKRunNodeSelectWidget.h/.cpp`、`Tests/LKD4Tests.cpp`，既有测试更新 `LKD1Tests/LKD2Tests/LKD3Tests`，文档 `docs/24-D4NodeTutorial.md`、`docs/17`、`docs/03`、`docs/validation/D4-Validation.json`。

验证基于 UE 5.8.1：Editor / Win64 / Development 构建成功；`LittleKing` **29/29 Success，0 failed，0 notRun**。PIE 手工验收脚本见 [24](24-D4NodeTutorial.md) 第 3 节。

## D5 追加交付（2026-09-08，代码与原生 UI 完成 —— 阶段 2 D0~D5 全部完成）

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| D5-01 | 存档模型与自动保存点 | 新增 `ULKRunSaveGame`（SaveVersion + FLKRunState）；安全点自动写固定槽 `LittleKing_Run`：新远征、胜利结算、领取/跳过奖励、选择节点（战斗前=恢复点）；写失败仅 Warning 不中断 |
| D5-02 | 载入校验与版本保护 | `ValidateStoredRun`：结构版本 1~4、RunId/英雄数值/牌组≥5/节点/遭遇引用/战斗恢复点上下文；文件版本或内容损坏明确拒绝（保留原档，不静默覆盖）；旧版本结构载入后升到 4 |
| D5-03 | 冷启动恢复分支（GameMode） | 内存无远征且有档：战斗中退出 → 回到该房开战前（同 Encounter/种子/Attempt）；路线/奖励中断 → 恢复中枢（部署世界直接弹奖励/节点面板）；终态 → 摘要 + 开始新远征；恢复世界不部署敌人 |
| D5-04 | 终态摘要与原生恢复面板 | `ULKRunResumeWidget`（换肤入口 ResumeWidgetClass）；摘要 = 通关/失败 + 战斗数 + 最后一战击杀/伤害；`StartNewRunFromRecovery` 仅允许恢复世界触发 |
| D5-05 | 自动化与文档 | 新增 `Tests/LKD5Tests.cpp` 三组（校验规则/磁盘往返与防重/恢复语义）；既有集成测试开头/结尾清理固定槽防串扰；教程见 [25](25-D5SaveTutorial.md) |

主要修改文件：新增 `ULKRunSaveGame.h/.cpp`、`ULKRunResumeWidget.h/.cpp`、`Tests/LKD5Tests.cpp`；`ULKRunSubsystem.h/.cpp`（存档 API/自动保存挂钩）、`ALKBattleGameMode.h/.cpp`、`ALKBattleGameMode_Run.cpp`（恢复分支/摘要）、`ULKBattleHUDWidget.h/.cpp`（恢复面板生命周期/幂等同步）、`Tests/LKD1Tests.cpp`、`Tests/LKD2Tests.cpp`（槽清理与防串扰）、文档 `docs/25-D5SaveTutorial.md`、`docs/17`、`docs/03`、`docs/validation/D5-Validation.json`。

验证基于 UE 5.8.1：Editor / Win64 / Development 构建成功；`LittleKing` **32/32 Success，0 failed，0 notRun**（含 D5 三组：校验规则、磁盘往返防重、恢复语义）。PIE 手工验收脚本（继续/战斗中退出/终态/防重/损坏档）见 [25](25-D5SaveTutorial.md) 第 3 节。

## BUG-017 修复（身份特性不随旧档丢失，2026-09-08）

用户试玩发现：法师在场时在敌方半场点法术，却提示"没有全场施法特性"。

| 编号 | 实际改动 | 影响与边界 |
|---|---|---|
| BUG-017 | 全场施法判据链定位 | 判据 = 存活英雄是否带 `Trait_MageSpellReach`（`HasGlobalSpellPlacement` → `HasTraitEffect(GlobalSpellPlacement)`）；英雄上场特性来自远征快照，`ApplyRunHeroState` 整体替换，旧档缺该特性时法师被静默降级 |
| BUG-017 | `ULKRunSubsystem::RestoreHeroIdentityTraits` | 载入存档后按代码补全缺失的身份特性（只加不删，D2 房间间临时增删语义不变），并同步 `PendingBattle.PlayerHeroes`，补全后按安全点自动保存 |
| BUG-017 | 身份特性表来源 | GameMode `CollectIdentityTraits`：`DA_GameData.DefaultHeroTraits` + 单位行内代码特性；不写死英雄/特性名 |
| BUG-017 | 排查埋点 | `[Run] %s 应用永久特性：…`（每次上场）、`[Spell] … 施法被拒绝：存活英雄 …`（敌方半场被拒时） |

主要修改文件：`ULKRunSubsystem.h/.cpp`、`ALKBattleGameMode.h`、`ALKBattleGameMode_Run.cpp`、`ALKBattleGameMode.cpp`、`ALKUnitBase.cpp`、`Tests/LKSprint5Tests.cpp`、`Tests/LKD5Tests.cpp`；详见 [05-BugLog](05-BugLog.md) BUG-017。验证：新增 `LittleKing.Sprint5.Heroes.AuthoredSpellTerritory`（旧档冷启动 → 补全 → 部署 → 敌方半场可施法 → 法师死亡后锁定）与 `LittleKing.D5.Save.IdentityTraitsRestoredOnLoad`；全量 `LittleKing` **36/36 Success，0 failed**（摘要见 `validation/BUG017-Validation.json`）。
