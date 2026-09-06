# v0.5 · Sprint 5 修订 · 协作变更记录

日期：2026-09-06。执行者：本次 Codex 协作。全部内容属于**新的 Sprint 5 范围**，Sprint 6 已按最新要求删除并跳过。

## 接手前先看

这是在此前未提交的 Sprint 5 工作上继续完成的协作交付，整个 diff 包含已有代码、文档和用户资产修改，不能全部归为本轮助手独立产出。上一轮开始前的本地快照在忽略目录 `Saved/Sprint5RevisionBaseline`；长期追溯以包含本文的 Git 提交为准。

本次追加内容：**始终满手的队列过牌、攻击建筑射程预览、正式黄色中线、删除 Sprint 6、详细地牢任务规划**。源码、测试、教程与规则文档全部归新的 Sprint 5；地牢只完成规划，代码尚未开始。

用户已保存 `Content/Data/DA_GameData.uasset`、`DT_Traits.uasset`、`DT_Units.uasset` 和 `Content/blueprint/WBP_BattleHUD.uasset`，并反馈数次试玩效果不错。助手本轮保留这四份二进制改动，仅只读加载/编译检查，未覆盖或重建资产；没有将用户反馈当作逐项视觉测试报告。五个项目蓝图目前 **0 错误、0 警告**。

用户本轮要求 Git 提交，交付包括已有未提交的 Sprint 5 文件和上述资产；不额外打版本标签。Git HTTP 代理为 `http://127.0.0.1:18081`；提交身份和最终提交号用 `git log` 查询，避免在提交内写回自身哈希。

当前规则入口是 [01-GDD](01-GDD.md)，技术契约见 [02](02-BattlePrototypeDesign.md)，编辑器短检查见 [16](16-Sprint5MigrationTutorial.md)，下一阶段详细安排见 [17](17-DungeonDevelopmentPlan.md)。原始评审 14 保留建议来源，不覆盖最新用户决定。

## 已实现的规则

| 编号 | 修改 | 协作者需要知道的边界 |
|---|---|---|
| S5-R01 | 取消部署倒计时，必须布置全部三英雄才可开始，拒绝重复英雄；敌方预先自动部署 | StartBattle 调试命令也不能绕过门槛；旧时间字段只保留兼容 |
| S5-R02 | 每英雄生成固定营地，点击营地后指挥移动；营地实体不可攻击、无血条 | 英雄生成在营地旁；营地死后保留，不计任何战斗/建筑卡数量上限 |
| S5-R03 | 英雄移动受营地圆与战场边界约束，绕实体建筑，移动时取消前摇和技能，到达恢复攻击 | 无次数/冷却；已发射弹道继续；没有目标时返回最后指定点；窄路可保守拒绝 |
| S5-R04（修订） | 固定四槽始终有牌；开局洗牌一次，队首补原槽、打出牌排队尾，CardId 唯一 | 删除旧“候选不足留空”和弃牌洗回逻辑；去重后至少五种，默认七种；不足或 ID 无法解析禁止开战 |
| S5-R05 | 特性系统增加可运行时增删的规则效果，法师提供全场施法，失去后仅限制敌方半场 | 法术不锁死、不因法师死亡灰卡；卡费不足仍可显示不可用 |
| S5-R06 | 骑士给附近己方近战佣兵嘲讽；嘲讽高于集火和其他目标指定，包括技能选敌辅助 | 排除英雄、远程、建筑、营地；多个光环来源分开记录；离圈最多半秒刷新 |
| S5-R07 | 全部战斗单位头顶简易血条，移除原团队血条路径；营地/移动圈/法术预览/预警/弹道可见 | 标准名字旧条运行时折叠；彻底删除旧 UMG 控件需执行迁移教程 |
| S5-R08 | 仅火球卡和当前法师火球技能释放时小震；其他死亡、攻击、治疗不震 | 默认强度 10、持续 0.15 秒；重叠仅当前一轮合并；后续震动不继承旧峰值 |

新增补充项：

| 编号 | 修改 | 影响/约束 |
|---|---|---|
| S5-R09 | 队列过牌与构筑接口保护 | 添加重复牌、转换成已持有种类、删到不足五种均返回 false；保持原状态；GetNextCard 可查下一张，GetDiscardSize 兼容返回 0 |
| S5-R10 | 攻击建筑落点预览同时画占地圈和射程圈 | 读 Turret 建筑单位行 AttackRange；无效落点红色，取消放置消失；兵营无攻击圈；不计临时战斗增益 |
| S5-R11 | 黄色中线改为独立原生 HUD 投影 | 原实现是 bDrawFieldBounds 控制的 Z=0 调试线；新线不依赖 Debug 开关，也不被地面深度遮挡 |
| S5-R12 | 删除原 Sprint 6 指南及任务，直接进入地牢规划 | 音效/素材、打包/硬件测试后置；新增 17，详细拆 D0～D5，明确英雄跨房间规则待定 |

游侠本轮不加新特性；“驻守后扩大远程支援范围”只放在 GDD 候选建议。没有擅自采用两英雄部署、改开局银币、加入地牢/家园或大规模渲染配置变更。

## P0/P1 修复对照

| 原问题 | 本轮处理 | 主要验证 |
|---|---|---|
| 纯 Overlap 结果因没有 blocking 被漏掉 | 规则用二维位置/半径直接筛选；占用和软分离不依赖查询布尔值 | 重复营地拒绝、分离/路径和连续战斗 |
| 精灵根节点带动碰撞缩放 | LogicRoot 下 Sprite / Sphere Body 分开 | 精灵缩放至非均匀大尺寸，球体积保持不变 |
| 移动时冷却停走、前摇叠加、换目标沿用前摇 | 间隔按起手定义，统一战斗计时，前摇绑定目标，反馈不冻结逻辑 | 换目标取消、不能转嫁旧命中、移动时冷却流逝 |
| 超时按阵营顺序即时判负 | 嵌套批次结束再判胜负；同批全灭按实际输出/开局种子决胜 | 同批两方全灭；排除虚弱归属；最后一击完整统计 |
| 名义伤害/治疗当作实际量，来源不完整 | Source + CombatEvent，Health 实际差值、身份快照、先事件后最终统计 | 满血治疗=0、过量伤害截断、无敌=0、来源死亡后的弹道归属 |
| 团队总条死亡后反涨 | 用户确认改为全单位头顶条；旧 API 弃用返回 0 | 原生编译；旧蓝图两条引用定位到迁移步骤 B |
| 集火过期仍黏目标、远方嘲讽吸走塔目标 | 到期清当前目标并重评估，嘲讽独立半径，固定塔只选射程内 | 嘲讽覆盖集火/直接设目标/技能查询，结束后取最近者 |
| 相机保留历史最大强度 | 一轮结束归零，独立视觉 RNG，切换相机时恢复旧相机 | 源码检查；火球与非火球的视觉验收见 16-F |
| 弹道不可见、跨过小目标、池闲置仍活跃 | 原生最小弹道显示，整段圆扫过取最早敌方命中，池隐藏/停 Tick/停碰撞 | 1 秒大步长飞越目标、命中回收、重复使用、结算全回收 |
| 先扣钱换牌，再发现效果失败 | 验证 → 预扣 → 执行 → 成功换牌/失败退款，拒绝同步重入 | 强制无效单位数据导致生成失败，核对银币、手牌和出牌计数 |

附带修复：兵营出兵点离开自身实体；英雄手动移动在 Result 阶段立即取消；虚弱只累计真正进入超时后的时间；运行时属性修改 GE 不复用同名对象；波次表先检查行结构；AI 尝试多张可用牌、治疗选己方受伤集群、有限时间蓄费、加强反制排序并显示集火预警。

## 修改了哪些文件

路径均相对于项目根目录，`.h/.cpp` 表示同名两文件；这是职责分组，完整本轮源码比对清单见 [验证摘要](validation/Sprint5-Validation.json)。

| 文件/组 | 变化 |
|---|---|
| **新增** ALKHeroCamp.h/.cpp | 营地 Actor、英雄链接与体积 |
| **新增** LKNavigation.h/.cpp | 二维障碍可见图路径与线段校验 |
| **新增** ALKPresentationHUD.h/.cpp | 头顶条、营地、范围预览、警告、弹道、实际飘字 |
| **新增** Tests/LKSprint5Tests.cpp | 十组自动化，更新队列测试，增加建筑射程及实际资产牌组/HUD 配置检查 |
| ALKBattleGameMode.h/.cpp | 部署验证、生成、出牌事务、法术范围、批次与统计、原生 HUD、随机/声音预加载 |
| ALKPlayerController.h/.cpp | 鼠标平面求交、营地模式、共用合法性预览、相机小震状态 |
| ALKUnitBase.h/.cpp、ALKUnitHero.h/.cpp、ALKUnitBuilding.cpp | 稳定体积、目标优先级、攻击计时、特性、营地移动和建筑出兵 |
| ULKUnitMovementComponent.h/.cpp | 建筑硬障碍、软分离、边界与英雄范围、路径缓存 |
| ULKTraitAuraComponent.h/.cpp | 多来源嘲讽光环、增删/死亡清理 |
| ALKProjectile.h/.cpp | 连续飞行命中、发射来源快照、池状态 |
| ULKDeckState.h/.cpp、ULKSilverComponent.cpp | 满手队列、唯一牌组及最低数量保护、下一张查询、零费支付 |
| LKTypes.h、LKDataTypes.h、ULKGameData.h/.cpp、ULKCardDefinition.h | 事件/统计、特性类型、营地/AI/显示配置、默认牌库、建筑上限语义 |
| LKGameplayHelpers.h/.cpp、ULKGameplayLibrary.h/.cpp、ULKUnitAttributeSet.cpp | 来源/实际量、血量钳制、技能选敌优先级与 AoE 批次 |
| ALKOpponentBrain.h/.cpp | 种子随机、预警、候选排序和蓄费 |
| ULKBattleHUDWidget.h/.cpp | 旧条兼容折叠、取消法术锁定、按费用/初始化状态/阶段刷新 UI，法师死亡不锁卡 |
| ULKCheatManager.h/.cpp | HeroTrait、DamageUnit 两个验收命令，营地不在 KillAll 中计数 |
| README、docs/01、02、03、10 | 当前规则和任务同步满手循环、射程/黄线、地牢排期 |
| **删除** docs/11-Sprint6Guide.md | 取消整个 Sprint 6，不再作为地牢开发前置 |
| docs/03~09、12~14 | 当前 Sprint 5 入口及历史说明，避免按旧规则继续制作资产 |
| **新增** docs/15、16、17、validation/Sprint5-Validation.json | 协作台账、UE 5.8 功能教程、详细地牢任务、可追溯验证摘要 |
| 用户已修改的四个 .uasset | 完整保留并纳入协作交付；详见本文开头 |

助手本轮未改模块依赖、地图、二进制资产或渲染配置；用户保存的四份资产包含在提交内，不能将它们写作“没有资产变更”。Saved 中的编辑辅助脚本、构建输出和完整自动化报告被 Git 忽略，不作为交付源码。

## 最新验证结果（本机实际执行）

日期 2026-09-06；UE **5.8.1-56057345**，Visual Studio 工具链 14.50.35737，Windows SDK 10.0.22621.0。仅进行开发用编译和功能回归，没有新做打包或硬件专项。

| 检查 | 结果 |
|---|---|
| little_kingEditor / Win64 / Development | **通过**，本次源码编译未报告项目警告 |
| LittleKing.Sprint5 自动化 | **10/10 Success，0 failed，0 notRun**；3 组无警告，7 组仅有已知测试世界/引擎提示 |
| BP_ALKBattleGameMode、WBP_BattleHUD、三个英雄 GA | **0 errors、0 warnings、0 failed to load**；上一轮的两条旧血条接口警告已消失 |
| 已保存 DA_GameData 双方牌组 | 去重后均满足四槽 + 待抽队列，CardId 都在 CardLibrary，HandSize=4 |
| 已保存 GameMode 的 HUDClass | 继承 LKPresentationHUD，可接收本次黄线和射程绘制 |
| 用户基础试玩反馈 | 用户称稍微测试数次、效果不错；未提供逐项检查明细 |
| 新增范围圈/黄线画面与鼠标 | 本轮未目视检查；已验证相关射程数据和 HUD 配置，短操作见 16-F.1 |
| 音效/表现素材、Cook/打包、硬件/性能专项 | 按最新用户要求后置，不阻塞地牢 |

十组测试：UniqueHandAndCycling、CampPathGeometry、DeploymentAndManualMovement、TraitsTauntAndSpellTerritory、ActualAmountsAndCardTransaction、WindupCollisionAndProjectilePool、SimultaneousEliminationAndLastHit、LiveBattleAndBarracks、BuildingAttackRange、SavedDecksAndHUD。

新增队列检查覆盖 120 次变换出牌槽、最小五种卡组 50 次出牌、固定槽顺序、下张查询、无重复无 None、错误操作保持状态、删除/转换/添加的最低数量与唯一约束。30 秒临时世界测试同时检查玩家与 AI 每帧四槽有牌。射程测试修改单位行数值并与实际生成的箭塔射程比较，另检查兵营/法术/普通单位不画攻击圈，以及缺失卡 ID 不能绕过开战校验。

自动化警告来自无相机的临时世界和 GameplayCueNotifyPaths 未配置时的回退扫描；不是断言失败。NullRHI 不验证实际屏幕像素、中文字体或鼠标遮挡。蓝图检查曾因 AllowList 参数/绝对路径不符引擎解析而扩展到引擎资产，修正为 `-AllowListFile=Saved/Sprint5BlueprintAllowList.txt` 后以仅五个项目蓝图的最终报告为准，未保存任何资产。

最新本机证据：`Saved/Logs/Sprint5QueueBuild.log`、`Sprint5QueueAutomation.log`、`Sprint5QueueBlueprintFinal.log` 和 `Saved/Automation/Sprint5Queue/index.json`。精简结果随代码保存在 [validation/Sprint5-Validation.json](validation/Sprint5-Validation.json)。上一轮曾完成游戏目标构建及 8 组自动化，仅保留历史信息，不冒充本轮打包/发布验证。

## 协作者接手事项

已完成的旧 HUD/数据迁移不要求重做。助手已确认资产能加载、当前牌组合法、HUDClass 正确、指定蓝图无编译警告；无法据此判断所有手动操作均已逐条验收。

| 接手内容 | 资产/位置 | 状态与责任 |
|---|---|---|
| 已保存的 HUD / 数据调整 | WBP_BattleHUD、DA_GameData、DT_Units、DT_Traits | 用户已修改并保留；助手通过只读加载/编译检查 |
| 四槽循环、箭塔射程、中间黄线 | 16-C / F.1，L_BattleTest | 源码完成；如需复核只做教程短流程，无需制作资产 |
| 地牢任务 D0～D5 | 17-DungeonDevelopmentPlan.md | 规划完成，代码待开始；D1 先做固定三战 |
| 跨房间血量/阵亡处理 | 17 第 2 节 | D1 满血流程基线；D2 正式规则仍需定稿，不改现有战内复活规则 |
| 音效、美术表现 | 07 / 12 / 13 参考 | 后续阶段，不作为当前缺陷或前置任务 |
| 打包与硬件专项 | 发布阶段再安排 | 当前跳过，原 Sprint 6 已删除 |

每次协作更新记录资产名、所测规则、结果和对应提交。二进制资产避免多人同时保存；不要根据历史教程重新加回团队血条、死亡震动、法术灰卡或允许空槽的逻辑。
