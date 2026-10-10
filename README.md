# 小小国王 · Little King

[简体中文](README.md) | [English](README.en.md)

UE 5.8 · C++ / Paper2D / GAS / UMG · 单机 PvE 自动战斗原型。当前版本 **v0.8.6**，工程版本 `0.8.6`。

在家园配置三名英雄与牌组，沿随机路线远征，通过战斗、市场和休息带回金币、道具与研究材料，发展七座家园建筑。当前支持五个固定区域、最多八个存档、研究解锁、暂停与远征恢复；内容目录为 **46 单位、48 卡、16 种研究法术、8 种建筑图纸**。

v0.8.6 重构市场三类商品、阶段品质概率与研究供给，接入远征背包和道具仓库，完善敌方 AI、动画中断、存档事务及战斗统计。道具目前为可替换的饰品／装备占位定义，具有五级品质，暂无战斗属性。详见[本版更新](docs/OptimizationImplementation.md#v086-发布记录)。

v0.8.5 引入的离线[战场工坊](Tools/BattleDesigner/index.html)：32 种积木、四种地形、条件/定时增援、敌方卡牌经济和 JSON 回读。[独立加载脚本](Scripts/PlayBattleLayout.ps1)可将设计交给 UE 战场，不写入远征存档。自动分配到远征节点尚未实现。

## 文档入口

新协作者先读 [项目交接](docs/64-ProjectHandoff.md)。文档按现行主题与开发阶段组织，旧教程和被覆盖的规划已移除。

| 文档 | 内容 |
| --- | --- |
| [游戏设计](docs/01-GDD.md) | 当前玩法、远征、家园、战斗与存档规则 |
| [技术架构](docs/02-BattlePrototypeDesign.md) | 模块职责、状态流转与数据所有权 |
| [内容图鉴](docs/19-ContentCatalog.md) | 当前单位、卡牌、数值和技能 |
| [内容开发接口](docs/34-CardDevelopmentInterfaces.md) | 注册、身份、表现与扩展契约 |
| [美术与声音](docs/36-ArtAudioSources.md) | 统一规范、批次来源、原始提示词与许可 |
| [基础阶段记录](docs/DevelopmentHistory.md) | 原型、地牢、家园、优化及 v0.8～v0.8.2 |
| [v0.8.3 法术与天使](docs/53-v0.8.3Changes.md) | 阶段交付与验证 |
| [v0.8.4 城防与驻守](docs/57-v0.8.4Changes.md) | 阶段交付与验证 |
| [v0.8.5 战场工坊](docs/62-TerrainAndEncounterDesigner.md) | 地形、JSON 契约、集成边界与发布验证 |
| [v0.8.6 调研优化](docs/OptimizationImplementation.md) | 市场、道具、AI、动画、存档保护与发布验证 |
| [GitHub 调研与建议](docs/GitHubResearchAndRecommendations.md) | 24 个项目的动画、AI、平衡、经济与商品概率研究；采纳范围见 v0.8.6 实施记录 |

## 验证与边界

v0.8.6 验证：UE 5.8.1 Development Editor 构建成功，LittleKing **193/193** 通过（95 项带警告），重点回归 **43/43**，Node **37/37**；完成 32,000 次市场生成审计、18 场脚本战斗采样和 720p／1080p 市场画面检查，六个既有存档哈希不变。发布准备只更新版本元数据和文档，沿用同一份已测试源码的结果。详见[发布证据](docs/validation/v0.8.6-Release.json)。

Run Schema 11、Profile Schema 3 支持兼容旧档迁移；原始验证报告保留历史版本含义。长期平衡仍需试玩反馈，Sprint 6 已跳过，打包及硬件验收后置。

## 仓库结构

- `Source/little_king`：游戏逻辑、原生界面与测试。
- `Content`：地图、蓝图、数据资产及运行时素材。
- `Tools/BattleDesigner`：离线编辑器与设计格式。
- `Scripts`：导入、审计及开发入口。
- `ArtSource/StorybookV1`：素材原图、提示词、回执与许可。
- `docs/validation`、`docs/images`：历史验证报告与实际截图。
- `Saved`：本地日志和玩家存档，不提交 Git。

内置单位/卡牌身份和行为由 C++ 默认值保证，数据表调节数值与表现，运行时使用私有副本。素材分别遵循归档许可；Noto 字体附 OFL，MiniMax-Music3 音乐保留许可与署名，生成图片不标为 CC0。
