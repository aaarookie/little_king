# 小小国王 · Little King

[简体中文](README.md) | [English](README.en.md)

UE 5.8 · C++ / Paper2D / GAS / UMG · 单机 PvE 自动战斗原型。

玩家在家园配置战备、从大门出征，在战场部署三名英雄、指挥营地并循环出牌。当前可完成家园 → 五区域固定大地图（各区 10～20 个随机节点、普通/精英/Boss/市场/休息）→ 家园的循环，包括随机敌阵容、房间奖励、路线选择、存档恢复及永久金币升级。

## 当前版本：v0.8 · 素材制作 A/B/C/D 批完成

D 批 UE 5.8.1 构建成功，83/83 全量自动化通过，720p/1080p 实际画面已检查，五个玩家存档哈希不变。

A 批已接入菜单插画、七座家园建筑、绘本 UI 与九条声音；B 批补 20 个战斗单帧和 17 张卡面；C 批再补五区地面、三种营地、五类节点徽记、三张 FX 零件和 28 条专属声音。A/B/C 合计新增 136 个引擎资产，28 种实体与 24 张卡牌均有图片，战斗 HUD、技能/状态、服务节点和升级反馈已接入，固定曝光避免场景过曝。素材清单见 [12](docs/12-AssetRequest.md)，来源见 [36](docs/36-ArtAudioSources.md)，B/C 完整提示词见 [39](docs/39-BattleArtPrompts.md) / [41](docs/41-WorldArtPrompts.md)，各批协作记录见 [37](docs/37-ArtAudioIntegration.md) / [38](docs/38-BattleArtIntegration.md) / [40](docs/40-WorldSkillsArtIntegration.md)，试玩见 [13](docs/13_Asset_Solutions.md)。无须手动下载或补建蓝图。D 批新增 28 组动画（448 姿势）、四首本地 Music3 循环音乐、七个建筑等级外观、中文字体与换场动效，共 639 个资产；四批累计 775 个。[D 协作记录与教程](docs/42-PolishArtIntegration.md)、[实际完整提示词](docs/43-PolishArtPrompts.md)、[验证摘要](docs/validation/PolishArt-Validation.json)。本次统一纳入 v0.8，工程版本为 `0.8.0`；见 [发布说明](docs/44-v0.8ReleaseNotes.md)。

阶段三新增五档角色品质、14 档法术等级和十五张远征临时卡。部队容量上限 8 格：巨魔占 2 格、巨像占 3 格，其余占 1 格；四槽手牌仍每卡一槽。超额获取时勾选一张或多张旧卡并确认替换，至少保留五种卡；三名英雄单独编队。精灵回血与共沐春色、背刺、战利品、模仿施法和冲刺击退均已接入，手牌、奖励、替换与家园详情同步显示分类。初版数值见 [32](docs/32-CharacterQualityAndBalance.md)，UE 5.8 试玩和调参教程见 [33](docs/33-Stage3CharactersTutorial.md)，改动记录见 [29](docs/29-OptimizationChangeLog.md)。

阶段二把正式远征升级为固定五区域地图：每轮生成各区 10～20 个节点及有向无环路线，多入口/出口支持相邻区域汇合。大门直接确认队伍、牌组和携带金币；普通、精英、Boss、市场与休息节点已接入。金库携带上限暂定 100～300（每级 +50）；钱包剩余金币在成功、失败或主动结束后全额带回。操作与截图见 [31](docs/31-WorldMapExpeditionTutorial.md)，协作记录见 [29](docs/29-OptimizationChangeLog.md)。

优化阶段一新增 `L_StartMenu`：每次 Play 先进入开始界面，可继续最近游戏、创建新游戏、选择或删除最多 8 个独立存档；设置保留接口。旧档自动显示为存档 01。控制台支持 `show me the money`（默认 100 金币）及 `show me the money 500`。协作记录见 [29](docs/29-OptimizationChangeLog.md)，试玩见 [30](docs/30-StartMenuTutorial.md)，验证见 [优化一摘要](docs/validation/Optimization1-Validation.json)。

家园 H0～H5 已交付：永久档、七座可点击建筑（本批已替换方块外观）、收藏详情、战备英雄/牌组分页、固定起点出征、金币结算与神像/金库升级。`L_Home`、相机、材质和 GameData 绑定均已制作；奖励/路线选择页可暂回家园，大门可继续或确认放弃本轮。无需用户手工创建 UI 或地图。家园操作见 [28](docs/28-HomeTutorial.md)，规则见 [01](docs/01-GDD.md)，家园改动见 [27](docs/27-HomeChangeLog.md)。

v0.7 是 v0.6 之后的合并发布，把家园、开始界面、五区域远征与优化阶段二三一次性收进主线：家园 H0～H5（永久档 `LittleKing_Profile_A/B`、七建筑、战备、金币结算与神像/金库升级）、`L_StartMenu` 与 8 个独立存档、五区域大地图远征（各区 10～20 节点与钱包全额带回）、五档品质与临时佣兵、巨魔与攻城七卡，并修复 BUG-014~019（含"继续远征后双方不攻击"）。远征 Schema 已升至 8，旧档自动迁移。**Sprint 6 已删除并跳过**；发布时后置的音效与美术现已完成 A/B/C/D 批整合，打包和硬件测试仍后置。

v0.7 发布时状态：UE 5.8.1 `little_kingEditor / Win64 / Development` 构建成功，`LittleKing` 全量自动化 72/72 通过（0 失败，报告 `Saved/Automation/V07Release01`）。v0.8 的最终回归为上方所述 83/83，发布证据见 [v0.8-Release](docs/validation/v0.8-Release.json)。

- 开战前必须部署骑士、法师、游侠；部署不限时。
- 每个英雄有固定实体营地。点击己方营地再点**任意可达地点**即可下达移动指令：指令会强行打断战斗（移动中不索敌不攻击），到达落点或卡住超时（默认 1.5 秒）后恢复自动战斗；右键取消选中。
- 手牌固定 4 槽且始终补满，牌组按 CardId 去重。开局洗牌一次；成功出牌后队首补入原槽，打出的牌排到队尾，按序过牌。默认七种，最低五种。
- 法师的全场施法特性存在时可全场落点；失去特性后仍可在己方半场施法。骑士让附近己方近战佣兵获得嘲讽，嘲讽高于集火。游侠暂不配置新特性。
- 每个战斗单位都有头顶简易血条，营地无血条。只有火球卡和当前法师火球技能释放时产生小震动。
- 放置箭塔等攻击建筑时显示占地圈和基础射程圈；黄色半场中线独立显示，不依赖调试开关。
- 包含实际伤害/治疗统计、批量胜负判定、实体绕路、连续路径弹道命中与对象池。
- 英雄 0 血进入失能，实例保留；远征里玩家英雄每场结算增加 40% 最大生命，封顶，失能者恢复到 40%（敌方每房满血重建）。普通治疗不能战内救起。
- 新增骷髅兵、骷髅射手、死灵法师、骷髅巨人、骷髅王，已接入绘本单帧；召唤/献祭、巨骨治疗、王的计数复活与远程减伤均已实现。
- 二十八个内置单位采用“代码规则 + 数据表调参”：类别、攻击类型、固定特性、被动和建筑行为不会因表行复制错误而漂移；生命、伤害、射程、速度、冷却、名称与表现仍可在表中调整。自定义 UnitId 不受该注册表限制。
- 索敌范围默认 900（`DA_GameData → Combat → UnitAcquireRadius`，`DT_Units` 可逐行覆盖）：范围内才自动锁定，目标死亡或离开范围（含滞回）后重新索敌；无目标的佣兵会向全图最近敌人行军，避免对局停滞。
- D1 已实现 GameInstance RunSubsystem、路线状态机、上下文/结果身份校验和跨地图英雄生命继承。
- D2 已加入 `DT_Encounters`：每个遭遇独立配置敌方英雄、有限波次、牌组/银币、AI 节奏和奖励档位；远征开始时复制完整目录，运行中不受共享资产后改影响。英雄永久基础最大生命与特性跨房继承，临时光环、集火目标、预警和技能冷却每房清空。
- D3 已完成房间奖励与原生 UMG：胜利弹出三选一面板，可升级持有的单位/建筑卡或获得骷髅新卡，也可跳过；领取期间锁定下一关并防重复消费。
- D4 已完成随机路线与原生节点选择面板：历史四层路线保留供旧档/独立调试；正式新远征已升级为五区域 DAG 全图，休息恢复 30%。
- D5 已完成存档与恢复：当前选档对应的 Run 文件在安全点自动保存，冷启动按“战斗中/选择中/终态”恢复并防重复领奖；载入时还会按代码补全缺失的身份特性（BUG-017）。存档 01 兼容旧 `LittleKing_Run` 名称。

本轮另加入四种巨魔、双头龙、攻城投石炮、擎天巨像，以及强化/眩晕/冰冻/叠层点燃；金库银币上限为 5/7/9/11/13。新增卡牌的代码入口与接口见 [34](docs/34-CardDevelopmentInterfaces.md)，七卡数值及规则见 [35](docs/35-TrollAndSiegeCards.md)。

## 运行

1. 关闭编辑器，用 UE 5.8 构建 `little_kingEditor / Win64 / Development`。
2. 打开 `little_king.uproject`，默认地图为 `Content/Maps/L_StartMenu`，点击 Play 后选择“新的游戏”或继续/读取存档。实际界面和常见问题见 [30](docs/30-StartMenuTutorial.md)。
3. 在家园战备处保存队伍/牌组，再从大门确认英雄/卡组，设置携带金币并直接出发，从固定起点选择节点。直接从 `L_Home` / `L_BattleTest` 开始 Play 也会先选档。
4. 点英雄按钮后在己方半场放置，全部就位后点击开始；战斗中点击卡牌再点击战场。
5. 每房胜利后选择或跳过奖励，再沿地图箭头选择节点；奖励、地图和服务节点页可暂回家园。首领通关或失败后点“返回家园”，结算金币并准备下一轮。

## 文档入口

| 入口 | 用途 |
|---|---|
| [01 当前 GDD](docs/01-GDD.md) | 生效规则、边界和后续设计 |
| [02 技术设计](docs/02-BattlePrototypeDesign.md) | 模块职责、事件、数据与兼容约定 |
| [03 任务清单](docs/03-TaskList.md) | 当前验收与历史任务 |
| [10 Sprint 5 指南](docs/10-Sprint5Guide.md) | 本轮范围、配置、完成标准 |
| [15 协作变更记录](docs/15-Sprint5ChangeLog.md) | 修改了什么、影响哪些文件、验证结果与接手事项 |
| [16 UE 5.8 新手迁移教程](docs/16-Sprint5MigrationTutorial.md) | 需要编辑器完成的操作与验证步骤 |
| [17 地牢开发任务规划](docs/17-DungeonDevelopmentPlan.md) | D0～D5 里程碑、数据结构、接口、依赖、验收与分工 |
| [18 地牢协作变更记录](docs/18-DungeonChangeLog.md) | D0～D5 与 BUG-015/017 的文件改动、资产归属、验证与接手事项 |
| [19 全内容图鉴](docs/19-ContentCatalog.md) | 所有英雄、佣兵、建筑、法术及敌方单位的当前数值与机制 |
| [20 D0 · UE 5.8 新手教程](docs/20-D0UndeadTutorial.md) | 无资产试玩亡灵、配置/调试、旧技能蓝图接线修复 |
| [21 D1 · UE 5.8 连战教程](docs/21-D1ExpeditionTutorial.md) | 连战、结算按钮、生命继承与排障 |
| [22 D2 · UE 5.8 遭遇教程](docs/22-D2EncounterTutorial.md) | 编辑遭遇表、调 AI/波次/奖励档、验证永久英雄状态与房间清理 |
| [23 D3 · UE 5.8 奖励教程](docs/23-D3RewardTutorial.md) | 原生三选一界面、领取/跳过验收及可选蓝图换肤 |
| [24 D4 · UE 5.8 路线教程](docs/24-D4NodeTutorial.md) | 分层节点图、随机敌阵容、休息回血与节点选择验收 |
| [25 D5 · UE 5.8 存档教程](docs/25-D5SaveTutorial.md) | 自动保存点、冷启动恢复、防重复领奖与损坏档保护验收 |
| [26 家园开发规划](docs/26-HomeDevelopmentPlan.md) | 七建筑功能、数值与资源、战备/区域、永久档与 H0～H5 验收（已实现，见 27） |
| [27 家园协作记录](docs/27-HomeChangeLog.md) | 家园 H0～H5 实际交付清单、数值定稿、存档影响与验证 |
| [28 家园 UE 5.8 教程](docs/28-HomeTutorial.md) | 直接试玩已有家园、配队/升级/往返操作、实机截图与排障 |
| [29 优化协作记录](docs/29-OptimizationChangeLog.md) | 优化阶段一/二需求、改动文件、存档兼容和验证 |
| [30 开始界面 UE 5.8 教程](docs/30-StartMenuTutorial.md) | 新建/继续/读取/删除、金币命令与排障 |
| [31 五区域远征 UE 5.8 教程](docs/31-WorldMapExpeditionTutorial.md) | 大门金币设置、全图/区域查看、市场/休息、全额带回、旧档与扩展入口 |
| [32 品质与初版数值](docs/32-CharacterQualityAndBalance.md) | 五档品质、法术等级、精灵与哥布林基础规则 |
| [33 阶段三试玩教程](docs/33-Stage3CharactersTutorial.md) | 启动、调试生成、数值调整和旧档 |
| [34 新卡代码接口](docs/34-CardDevelopmentInterfaces.md) | 内容注册、容量、技能、状态、UI、存档和测试入口 |
| [35 巨魔与攻城卡](docs/35-TrollAndSiegeCards.md) | 本轮七卡、加权占格、多卡替换、技能边界与试玩 |
| [07 美术风格](docs/07-ArtStyleGuide.md) | 绘本主题、色板、规格与生成约束 |
| [12 素材需求](docs/12-AssetRequest.md) | 当前全量需求、已有资源和待制作批次 |
| [13 素材接入教程](docs/13_Asset_Solutions.md) | UE 5.8 试玩、来源下载、音频复现与重导入 |
| [36 素材来源与提示词](docs/36-ArtAudioSources.md) | 作者、许可证、原文件、处理步骤与实际生成提示词 |
| [37 素材整合记录](docs/37-ArtAudioIntegration.md) | A 批文件归属、画面、验证与后续缺口 |
| [38 战斗美术整合](docs/38-BattleArtIntegration.md) | B 批 20 单帧/17 卡面、显示比例、验证与 UE 5.8 教程 |
| [39 战斗素材提示词](docs/39-BattleArtPrompts.md) | B 批 37 张实际完整提示词与参考图记录 |
| [40 世界与技能素材整合](docs/40-WorldSkillsArtIntegration.md) | C 批 52 资源、技能/UI 接口、协作记录、验证与 UE 5.8 教程 |
| [41 世界素材提示词](docs/41-WorldArtPrompts.md) | C 批 16 张实际完整提示词、尺寸与来源清单 |
| [42 动画音乐等整合](docs/42-PolishArtIntegration.md) | D 批 639 资源、代码协作记录、验证与 UE 5.8 教程 |
| [43 动画与音乐提示词](docs/43-PolishArtPrompts.md) | 35 张图与四首 Music3 的实际完整输入 |
| [44 v0.8 发布说明](docs/44-v0.8ReleaseNotes.md) | A～D 批发布范围、版本号、英文 README 与验证证据 |
| [14 原始评审](docs/14-ProjectReview.md) | 建议来源；本轮采用项以 01 / 15 为准 |
| [05 BugLog](docs/05-BugLog.md) | 历史问题与新修复入口 |

04、06、08、09 保留早期操作教程；旧倒计时、法术门、团队血条与集火优先规则已失效。07、12、13 已按当前工程重新整理；素材完成状态以 12 的清单及 37/38/40/42 的实际验证为准。

## 协作与验收

A/C/D 批未重存既有战斗资产；B 批仅在 `DT_Units` 修正旧兵营 `SpriteScale.Y=0.5`，其他数值/字段不变。`DT_Traits`、三个 GA、`DA_GameData`、`L_BattleTest` 和 `WBP_BattleHUD` 未重存。家园地图、Home 目录下的 GameMode/材质和原生 UI 由 Codex 助手制作；旧战斗 HUD 的部署按钮在运行时按本轮英雄顺序绑定。接手先读 18、27 与素材记录 37/38/40/42，避免多人保存同一资产。素材验证见 [A 批](docs/validation/StorybookAssets-Validation.json)、[B 批](docs/validation/BattleArt-Validation.json)、[C 批](docs/validation/WorldArt-Validation.json) 与 [D 批](docs/validation/PolishArt-Validation.json)。**当前发布验证见 [v0.8-Release](docs/validation/v0.8-Release.json)**；[v0.7-Release](docs/validation/v0.7-Release.json) 保留当时构建及 72 项测试记录。分阶段证据见 [HomeUI-Validation](docs/validation/HomeUI-Validation.json)、[Optimization1](docs/validation/Optimization1-Validation.json)、[Optimization2](docs/validation/Optimization2-Validation.json)、[Optimization3](docs/validation/Optimization3-Validation.json)、[TrollCards](docs/validation/TrollCards-Validation.json)，更早的 44 项记录保留在 [Home-Validation](docs/validation/Home-Validation.json)。

存档 01 的家园永久档写在 `LittleKing_Profile_A/B`，远征档为 `LittleKing_Run`；02～08 使用编号前缀。远征 Schema 8 兼容旧图、钱包和已冻结回执；占格超过 8 的旧卡组先保留，再由玩家选择裁减。自动化测试默认不读写真实永久档，跑测试不会给玩家加金币。`little_king.Build.cs` 关闭了 Unity 合并编译（多个 .cpp 有同名文件内辅助符号）。

法师/游侠 GA 的旧接线问题已由用户修正；自动检查确认两者取敌输入已连接，游侠不再使用治疗节点。NullRHI 不验证实际画面/鼠标，短 PIE 检查见 21。Git HTTP 代理使用 `http://127.0.0.1:18081`。
