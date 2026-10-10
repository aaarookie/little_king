# 开发历史

本文合并原型、Sprint、地牢、家园、优化及 v0.8～v0.8.2 的阶段记录。日期、数量、Schema 和验证数字均为**对应阶段的历史结果**。当前接手入口是 [64 交接](64-ProjectHandoff.md)；现行玩法、内容与扩展契约分别见 [01 GDD](01-GDD.md)、[19 图鉴](19-ContentCatalog.md)、[34 开发接口](34-CardDevelopmentInterfaces.md)。素材来源、许可和生成记录统一见 [36 来源台账](36-ArtAudioSources.md)。

后续阶段各保留一份记录：[v0.8.3](53-v0.8.3Changes.md)、[v0.8.4](57-v0.8.4Changes.md)、[工坊与 v0.8.5](62-TerrainAndEncounterDesigner.md)。历史规划不产生新的开发任务；Sprint 6 按用户决定跳过，打包和硬件验收仍后置。

## 原型与 Sprint 0～5

| 阶段 | 已完成结果 |
| --- | --- |
| 原型 / Sprint 0 | UE 5.8 C++、Paper2D 与 GAS 骨架；单位、英雄、建筑、弹道、银币、卡库、战斗阶段机及敌方波次。建立 `L_BattleTest`、单位表和 GameData，完成编译及基础 PIE。修正 Maps & Modes 配置归属、无 Pawn 时相机接管和左右半场坐标。 |
| Sprint 1 / v0.1 | 部署、四槽手牌、银币、结算和重开 HUD；修正固定塔失效目标、射程抖动、边界与死目标。首次 Git 交付。 |
| Sprint 2 / v0.2 | 卡牌经济和内容、调试命令、原槽补牌；Paper2D 朝向与显示缩放独立于碰撞体积。 |
| Sprint 3 / v0.3 | 英雄主动技能、对局反馈和无敌调试；修正超时伤害遍历中的数组变化。 |
| Sprint 4 / v0.4 | 敌方选牌、战术与内容配置接入。 |
| Sprint 5 / v0.5，2026-09-06 | 三英雄完整部署、实体营地与移动命令、四槽唯一队列循环、头顶血条、攻击建筑射程预览和常显中线；仅火球小震。修正伤害批次、实际伤害/治疗统计、目标优先级、连续弹道命中及失败出牌退款。 |

战斗入口为 [ALKBattleGameMode](../Source/little_king/ALKBattleGameMode.cpp)、[ALKPlayerController](../Source/little_king/ALKPlayerController.cpp)、[ULKDeckState](../Source/little_king/ULKDeckState.cpp)、[ALKPresentationHUD](../Source/little_king/ALKPresentationHUD.cpp)。逻辑根节点和精灵变换分开；伤害、召唤与复活批次结束后再判胜负；UI 读取结果，不累计战斗事实。

Sprint 5 历史验证为 UE 5.8.1 Development 构建成功、专项 **10/10**、五个项目蓝图零错误/警告，见 [Sprint5 摘要](validation/Sprint5-Validation.json)。用户保存的 GameData、单位/特性表和 HUD 二进制改动随协作交付保留；该批没有逐项目视验收新增范围圈。原型至 Sprint 4 的旧记录为编译/PIE 记录，没有统一全量报告。

地牢期间又完成营地距离限制解除、指挥移动打断战斗和旧档英雄身份恢复。关键回归证据为 [BUG017](validation/BUG017-Validation.json)（36 项）与 [BUG018](validation/BUG018-Validation.json)（37 项）。旧法术门、部署倒计时、团队总血条、营地追击半径及早期“两英雄”建议已被后续决定覆盖。

## 地牢 D0～D5，2026-09-07～08

| 阶段 | 已完成结果 | 历史全量验证 |
| --- | --- | --- |
| D0 | 远征值类型契约、英雄失能与战后恢复、五种亡灵和事件驱动被动、双方独立阵容。 | [16/16](validation/D0-Validation.json) |
| D1 | RunSubsystem、三战串联、BattleContext/Outcome 注入、AttemptId 幂等提交；十三种内置单位身份统一由代码兜底。 | [20/20](validation/D1-Validation.json) |
| D2 | 遭遇目录与快照、英雄永久属性和特性跨房间继承、普通/精英/首领配置；修正敌方状态误继承上一房。 | [24/24](validation/D2-Validation.json) |
| D3 | 三选一奖励、领取/跳过事务、局内加牌与升级、原生奖励 UI。 | [26/26](validation/D3-Validation.json) |
| D4 | 分支节点、确定性敌阵容、休息服务和原生选路 UI。 | [29/29](validation/D4-Validation.json) |
| D5 | 安全点存档、完整校验、战前恢复、终态摘要与恢复 UI；坏档保留、重复结算和奖励防重。 | [32/32](validation/D5-Validation.json) |

主要入口：[LKRunTypes](../Source/little_king/LKRunTypes.h)、[ULKRunSubsystem](../Source/little_king/ULKRunSubsystem.cpp)、[ALKBattleGameMode_Run](../Source/little_king/ALKBattleGameMode_Run.cpp)、[LKEncounterContent](../Source/little_king/LKEncounterContent.cpp)、[LKUnitContent](../Source/little_king/LKUnitContent.cpp)。RunState 跨地图保存值类型；GameMode 负责单场；临时光环、Actor、ASC 和 GE 句柄不进入远征档。作者资产先复制再应用配置，身份和行为由内置注册表校正。

此阶段的三战/四层切片、玩家骷髅奖励和早期恢复建议仅属历史。路线、合法卡与恢复加成后来已更新，当前规则见 01/19/34。

## 家园 H0～H5，2026-09-09～10

| 阶段 | 已完成结果 |
| --- | --- |
| H0 | 七建筑、永久进度、战备及一次出征输入/加成快照契约。 |
| H1 | 永久档 A/B 双槽与 Revision；家园 GameMode、建筑实体、俯视输入和原生 HUD。 |
| H2 | 收藏详情、三英雄/牌组战备草稿、校验、保存与未保存确认。 |
| H3 | 大门出征、冻结战备和永久加成、恢复已有远征及终态回家。 |
| H4 | 金币暂存、终态冻结与幂等入账；神像/金库升级和实际加成。 |
| H5 | 远征 Schema 4→5 迁移、坏档保护、调试入口与回归。随后交付 `L_Home` 场景、七建筑点击及面板。 |

入口：[LKHomeTypes](../Source/little_king/LKHomeTypes.h)、[LKHomeContent](../Source/little_king/LKHomeContent.cpp)、[ULKProfileSubsystem](../Source/little_king/ULKProfileSubsystem.cpp)、[ALKHomeGameMode](../Source/little_king/ALKHomeGameMode.cpp)、[ULKHomeHUDWidget](../Source/little_king/ULKHomeHUDWidget.cpp)。永久操作采用候选写入、读回校验后发布；结算 ID 防重复；家园实体与战斗单位分开。测试使用独立槽，不拿玩家档验证迁移。

历史验证：[家园 44/44](validation/Home-Validation.json)、[场景/UI 47/47](validation/HomeUI-Validation.json)，含 720p/1080p 实际渲染；[继续远征修复 48/48](validation/BUG019-Validation.json)。家园 C++ 初稿与后续场景/UI 集成来自不同协作者，既有二进制资产保留。早期失败保留 50%、放弃无收益及“暂回家园”流程后来均已调整。

## 优化阶段与 v0.7，2026-09-13～17

| 阶段 | 已完成结果与重要入口 | 历史验证 |
| --- | --- | --- |
| 优化一 | 开始菜单、最多八存档、最近有效档继续、新建/读取/确认删除、独立家园和远征持久化；`ALKStartMenuGameMode`、`ULKStartMenuWidget`、`ULKProfileSubsystem`。 | [52/52](validation/Optimization1-Validation.json) |
| 优化二 | 固定五区域地理、确定性节点与分支汇合、市场/休息状态机、远征钱包和本金交接；`LKWorldMapContent`、`ULKWorldMapWidget`、`ULKRunSubsystem`。 | [58/58](validation/Optimization2-Validation.json)，含 200 种子与 12 条受控路线 |
| 优化三 | 品质/种族、临时佣兵注册、分类详情、满额替换和保存失败回滚；`LKExpeditionMercenaryContent`、`LKCardPresentation`。 | [65/65](validation/Optimization3-Validation.json) |
| 七卡扩展 | 巨魔、双头龙、攻城炮、巨像；加权容量、多选原子替换、控制/灼烧与专属技能；`LKCardRules`、`ULKUnitStatusComponent`。 | [72/72](validation/TrollCards-Validation.json)，最终 UI 复测 1/1 |
| v0.7 | 地牢、家园、开始菜单、五区域远征及以上优化统一交付。 | [发布 72/72](validation/v0.7-Release.json) |

各批完成 Development 构建及其记录范围内的界面验证，玩家存档哈希核对未变。旧阶段的“所有卡共用八张上限”、5～8 张战备、10～20 节点和失败全额返金币已由 v0.8.2 覆盖。容量按 CardId 查询；旧档迁移先验证临时副本，失败不覆盖原档；运行时注册不写回共享卡牌资产。

## v0.8 与 v0.8.1，2026-09-19～23

v0.8 统一交付绘本美术 A～D 批、战斗/UI/声音、五态动画、循环音乐、中文字体和转场。**本版历史素材计数为 775 个 UE 资源**；制作来源归 36，历史批次验证保留为 [A](validation/StorybookAssets-Validation.json)、[B](validation/BattleArt-Validation.json)、[C](validation/WorldArt-Validation.json)、[D](validation/PolishArt-Validation.json)。[v0.8 发布报告](validation/v0.8-Release.json) 记录构建成功和 **83/83**（39 无警告、44 带警告），发布整理复用 D 批已验证内容。

v0.8.1 修复重叠闪黑、朝向反转与步行滑行：按稳定脚底排序，主动行走位移驱动左右朝向/步态，关闭精灵投影和运动模糊。入口为 [ULKUnitAnimationComponent](../Source/little_king/ULKUnitAnimationComponent.cpp)、[ULKUnitMovementComponent](../Source/little_king/ULKUnitMovementComponent.cpp)、[ULKPresentationSubsystem](../Source/little_king/ULKPresentationSubsystem.cpp)。只改 Sprite 显示变换；逻辑坐标、碰撞和自定义图片保留；纯上下移动保持最近左右朝向。

**v0.8.1 历史新增 24 图、96 帧、144 个独立资源**，旧 D 批可独立重导。[修复验证](validation/MovementFix-Validation.json) 与 [发布报告](validation/v0.8.1-Release.json) 记录构建成功、**87/87**（43 无警告、44 带警告）、720p/1080p 48 张真实画面及五个玩家档哈希不变。最终展示采样修订没有被记成新一轮全量回归。

## Balance V1 与 v0.8.2，2026-09-28～30

Balance V1 完成英雄/骷髅耐久、地理深度预算、有限增援、敌方专属骷髅法阵及旧档事务迁移；献祭调整为 3%。入口为 [LKBalanceRules](../Source/little_king/LKBalanceRules.cpp)、[LKSkeletonCircle](../Source/little_king/LKSkeletonCircle.cpp)、[ALKOpponentBrain](../Source/little_king/ALKOpponentBrain.cpp)。DeepSeek Harness 提供初稿和限定修复，Codex 完成编译、资产同步、UI、采样调参与集成验证；这次分工不形成后续协作权限限制。

[历史 Balance V1 报告](validation/BalanceV1-Validation.json) 为 **101/101**（49 无警告、52 带警告）及 48 场实际 UE 采样，详见 [基准 18 场](validation/BalanceV1/battle-samples.csv)、[独立复核 30 场](validation/BalanceV1/battle-holdout.csv)、[事件 CSV](validation/BalanceV1/battle-events.csv)。当时治疗波为 40；独立满血组 10 胜/8 负，胜局中位数 252.63 秒；带伤未强化组 12 场全负。该结果记录单场耐久调参，完整远征成长仍需连战反馈。

v0.8.2 在上述实现上交付七卡出征、动态合法容量/最少必要弃牌、临时佣兵奖励/市场、材料研究、休息专属法术/建筑升级、失败 80% 金币与材料带回、暂停及锁定战前恢复、每区约十五站的局部地图。治疗波同步代码与实际资产为 120 + 英雄最大生命 6%。

入口：[LKCardRules](../Source/little_king/LKCardRules.cpp)、[LKResearchContent](../Source/little_king/LKResearchContent.cpp)、[ULKRunSubsystem_Services](../Source/little_king/ULKRunSubsystem_Services.cpp)、[ULKBattlePauseWidget](../Source/little_king/ULKBattlePauseWidget.cpp)。购买、研究、加卡/弃牌与结算都在保存成功后发布；市场、分支、敌阵和战前快照不因重试重抽。本版历史 Schema 为 Run 10 / Profile 2。

[v0.8.2 发布报告](validation/v0.8.2-Release.json) 记录 Development 构建成功、**109/109**（53 无警告、56 带警告）、八项新增专项、20,000 次冻结市场采样、720p/1080p 原生 UI 12 图和五个玩家档哈希不变。[本版 18 场战斗 CSV](validation/V082/battle-samples.csv) 为 14 胜/4 负、总体中位数 246.78 秒，沿用调参种子，不能当作独立验证组。

以上报告含测试环境及预期拒绝路径警告。原始过程日志在本机 `Saved`；仓库 JSON/CSV 保留结果与证据路径。本次文档合并未重跑这些历史验证。
