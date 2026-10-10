# 《小小国王》GitHub 项目调研与设计建议

调研日期：2026-10-10。项目基线：UE 5.8、v0.8.5，发布提交 `f256338e8d95731b0f46cd728d5dd7a48f530792`；同时核对了当前工作区源码。本文集中记录外部项目机制、对本游戏的适用性与建议，不改变现行规则，也不代表下一版本已经确定这些功能。当前规则仍以[游戏设计](01-GDD.md)、[内容图鉴](19-ContentCatalog.md)和[战场工坊](62-TerrainAndEncounterDesigner.md)为准。

研究采用项目官方仓库、固定提交源码、许可文件和官方发布说明。没有运行或构建外部游戏，没有进行玩家试玩或性能基准测试。本项目概率按默认配置推导；外部项目概率按所链接快照说明，均区别于实测频率。候选方案与试验参数均明确标为建议。

后续实施：按本报告选取的优化已纳入 v0.8.6。本文保留调研时的 v0.8.5 基线，实际采用的市场规则、AI、动画与验证结果集中记录在 [优化实施记录](OptimizationImplementation.md)，不把本报告中的候选参数直接视为当前规则。

阅读导航：[现状与关键计算](#2-当前实现中与调研直接相关的事实) · [24 项目总览](#3-调研项目总览) · [动画](#4-角色动画与内容生产) · [AI 与移动](#5-敌方-ai战术与移动) · [完整游戏的经济机制](#6-数值经济与随机资源完整游戏的经验) · [商品概率方案](#7-商品出现阶段与概率建议的设计方式) · [难度](#8-难度曲线与战斗数值的建议) · [经济](#9-经济系统把钱的用途和反馈回路画清楚) · [工具与远期方向](#10-关卡验证与中长期研究工具) · [推进顺序](#11-建议推进顺序与验收方式)。

## 1. 最值得优先处理的事情

《小小国王》已经有较完整的战斗规则、远征、家园、存档和关卡编辑基础。现在继续增加内容之前，最值得借鉴的是：让现有内容有机会出现、让 AI 理解这些内容、让玩家看清动作和战术反馈，并建立能解释平衡问题的数据。

1. **先检查商品能否被实际体验到。** 当前佣兵商店对所有地区使用同一池；永久研究商品则叠加“入口稀有率”和“池内低权重”，最稀有内容的出现机会极低。默认路线最后市场的钱包上界只有 990 金币，低于三项高价研究商品。商品出现、金币买得起、银币用得出、牌组放得下，应当一起设计。
2. **AI 先统一观察和效果理解，再增加复杂战术。** 当前单位索敌有隐藏规则，但部分指挥决策直接读取全场对象；通用法术落点也未充分区分新增效果。仅提高反应速度或加入行为树，不能解决这些问题。
3. **动画先增强动作表达与制作数据。** 本项目已有位移驱动步态、稳定脚底和动作优先级；优先补逐帧时长、接触事件、受击方向与清晰前摇。PaperZD 值得小规模验证，完整骨骼动画迁移暂列中长期。
4. **难度要按整个遭遇和整轮远征评估。** 敌军属性、免费增援、付费出牌、地形、英雄残血和家园成长会叠加；只调生命系数容易把难度变成拖时间。
5. **先有基线，再做自动调参。** 记录战斗与经济过程，使用成对种子、代表性牌组和留出关卡检验；自动搜索只能帮助找候选，不能替代体验判断。

## 2. 当前实现中与调研直接相关的事实

### 2.1 游戏结构决定了哪些机制适合借鉴

当前是单机 PvE 自动战斗：三英雄、循环手牌、战斗银币、远征金币、永久家园成长并存。目录含 46 单位、48 卡、16 种研究法术和 8 种建筑图纸，每队默认最多 20 个战斗单位。英雄全失能是失败条件，营地不可攻击，因此 RTS 中的拆基地胜利、无限扩张经济和无期限撤退不能直接照搬。

世界包含五个固定区域，完整默认远征固定经过四个区域，每区 15 层；市场位于 Layer 5、10（从 0 计数），走完访问 8 个市场，早退或失败另算。工坊已经支持地形、条件/定时波次和敌方经济，但作者关卡尚未自动分配到远征节点。参考入口：[GDD](01-GDD.md)、[世界地图生成](../Source/little_king/LKWorldMapContent.cpp)、[工坊边界](62-TerrainAndEncounterDesigner.md)。

### 2.2 商店目前没有统一的“阶段—品质—可用性”模型

`GenerateMarket` 从全部 14 个临时佣兵中洗牌、不放回取 3 个，没有按地区筛选品质，也没有按玩家银币上限筛选。池中是 7 个优秀、4 个稀有、3 个史诗佣兵。由此可得：

| 默认商店事件 | 理论概率 | 含义 |
| --- | ---: | --- |
| 指定佣兵出现在一次市场 | 3/14 ≈ 21.43% | 不随地区深度变化 |
| 三格中至少一个史诗佣兵 | `1-C(11,3)/C(14,3)` ≈ 54.67% | 第一地区也采用这个概率 |
| 出现一份法术书 | 1% | 再从符合地区要求的书池按权重抽取 |
| 出现一份建筑图纸 | 0.5% | 再从符合地区要求的图纸池按权重抽取 |

这是当前代码模型，不表示“史诗一定比稀有强”。精灵牧师、巨魔、巨像已有品质与基础数值不一致的特例。巨像需要 12 银币，默认金库上限按等级为 5/7/9/11/13，因此它可能在玩家无法使用的阶段出现在商店。生成函数也没有按永久收藏去掉已解锁研究商品。来源：[商店生成与研究目录](../Source/little_king/LKResearchContent.cpp)、[佣兵默认值](../Source/little_king/LKExpeditionMercenaryContent.cpp)、[品质及金库规则](19-ContentCatalog.md)。

商店已有稳定节点种子及生成完成标志，不能通过正常读档随意刷新。这是应保留的基础；改概率时不应重掷已经生成并保存的旧远征商品。

### 2.3 研究商品的两层稀有度相乘后，量级非常小

在各地区深度，符合条件的权重总和如下。深度 0–3 表示从起点向外的地理层级，与“第几个访问区域”或卡牌品质不是同一字段。

| 地区深度 | 法术书总权重 | 图纸总权重 |
| --- | ---: | ---: |
| 0 | 430 | 100 |
| 1 | 533 | 245 |
| 2 | 549 | 257 |
| 3 | 552.6 | 258 |

若完整访问 8 个市场，把各节点抽样按独立机会近似建模，则：

- 至少看到一本书：`1-0.99^8 ≈ 7.73%`。
- 至少看到一份图纸：`1-0.995^8 ≈ 3.93%`。
- 至少看到两类中的任意一种：`1-(0.99×0.995)^8 ≈ 11.35%`。

也就是说，约 **88.65% 的完整远征看不到任何研究商品**。这还没有扣除买不起、已拥有或提前失败等情况。

| 深度 3 才开放的商品 | 单个合格市场出现率 | 一轮只有两个合格市场时，至少出现一次 |
| --- | ---: | ---: |
| 圣玛丽亚的神圣增援，权重 0.6 | `1%×0.6/552.6` ≈ 0.001086% | 0.002172% |
| 神圣祝福，权重 1 | `1%×1/552.6` ≈ 0.001810% | 0.003619% |
| 禁军之墙，权重 1 | `0.5%×1/258` ≈ 0.001938% | 0.003876% |

以神圣增援为例，在每轮均走完、配置不变、两次合格市场近似独立的几何分布模型中，首次看见的平均等待量约 **46,050 轮**。这不是实测玩家时长，也不是保证值；它说明概率乘积的数量级，与一般“后期稀有但可追求”的体验差距很大。实际购买还受 1,400 金币价格约束。

建议先确定希望玩家在多少次合格机会内，以多大概率体验到某项内容，再反推单次概率：`p = 1-(1-q)^(1/N)`。例如希望 10 次合格机会内有 50% 的人见到它，对应单次概率约 6.70%，而不是凭直觉在多个层级各填一个很小的数。以上推导依据仍是[当前研究内容源码](../Source/little_king/LKResearchContent.cpp)，属于本报告计算。

### 2.4 三项研究商品超过默认路线的购买力上界

当前默认路线共 39 场战斗：3 个固定普通入口、3 个固定精英出口、1 个最终首领，以及 32 个随机普通/精英节点。最后一个市场在末区 Layer 10；后面还剩 Layer 11、13 和首领三场战斗，因此到店前最多结算 36 场。

即便按最宽松情形计算：满级金库携带 300 本金，全程不消费，3 个固定普通各给 10，其余 33 场全部按精英奖励 20，钱包也只有：

`300 + 3×10 + 33×20 = 990 金币`。

这个上界不要求证明存在一个实现全部精英的种子；任何更常见的路线只会更低。神圣祝福 1,000、禁军之墙 1,300、神圣增援 1,400，因此在**当前 C++ 默认、无额外金币入口或调参、正常远征流程**下，即使出现也无法购买。通关奖励在最后市场之后结算，不能用于此前购物；家园积蓄再多也受携带本金上限约束。

优先建议先修正内容可达性：降低对应价格、改变获取渠道，或明确设计新的高阶段购买场所/本金能力；选择哪种取决于预期解锁节奏。单纯提高出现概率无法解决此问题。来源：[节点结构](../Source/little_king/LKWorldMapContent.cpp)、[金库上限与奖励](../Source/little_king/LKHomeContent.cpp)、[胜利与终态结算](../Source/little_king/ULKRunSubsystem.cpp)、[商品价格](../Source/little_king/LKResearchContent.cpp)、[钱包购买校验](../Source/little_king/ULKRunSubsystem_Services.cpp)。

### 2.5 难度已有分段映射，不能把旧常量直接判为错误

世界地图每区 15 层，平衡预算仍使用每区 5 个档位：`b=Round(Layer×4/14)`，`d=5×RegionDepth+b`，`p=d/19`。普通敌方生命倍率为 `1+0.6p+0.4p²`，攻击倍率为 `1+0.3p+0.2p²`。

| 地区深度 | 区域起点生命倍率 | 区域末端生命倍率 | 区域末端攻击倍率 |
| --- | ---: | ---: | ---: |
| 0 | 1.000 | 1.144 | 1.072 |
| 1 | 1.186 | 1.374 | 1.187 |
| 2 | 1.427 | 1.659 | 1.330 |
| 3 | 1.723 | 2.000 | 1.500 |

因此 `LayersPerRegion=5` 是预算分档，实际地图通过显式映射使用它；不能仅凭看到 15 层地图就认定实现漏改。值得研究的是这些平台与跳变是否形成合适节奏，以及精英/首领额外预算、波次、法阵、地形的叠加。来源：[平衡规则](../Source/little_king/LKBalanceRules.cpp)、[地图映射](../Source/little_king/LKWorldMapContent.cpp)。

## 3. 调研项目总览

本次覆盖 **24 个项目**。完整游戏用于理解规则如何组合，运行库用于理解某个技术问题，编辑工具用于学习内容生产；它们的用途不同。表内许可是所读代码快照的主要声明，素材、编辑器和第三方依赖另看各仓说明。

| 方向 | 项目与源码仓库 | 最值得借鉴 | 采纳方式／主要许可 |
| --- | --- | --- | --- |
| UE 2D 动画 | [PaperZD](https://github.com/heavybullets/PaperZD) | 状态机、动画通知、序列与角色表现分离 | 小样接入候选；MIT；[许可](https://github.com/heavybullets/PaperZD/blob/91439935af58854ea741691e8bfb2535ff9c0a4b/LICENSE) |
| 精灵动画 | [Flare Engine](https://github.com/flareteam/flare-engine) | 帧时长、方向与动作定义 | 学数据结构；GPL-3.0-or-later；[许可](https://github.com/flareteam/flare-engine/blob/1969baeb89ebf94cf22e2bc2ed4e3d8413714d4f/COPYING) |
| 动画状态组织 | [Godot Demo Projects](https://github.com/godotengine/godot-demo-projects) | 参数驱动状态和方向动画 | 学机制；MIT，样例素材单独核对；[许可](https://github.com/godotengine/godot-demo-projects/blob/b761b4cd5718a7118f2c95eb55f181d0a5f004f1/LICENSE.md) |
| 2D 骨骼 | [DragonBonesCPP](https://github.com/DragonBones/DragonBonesCPP) | 插槽、混合、事件和动作生命周期 | 长期参考；MIT，不能假定有现成 UE5.8 适配；[许可](https://github.com/DragonBones/DragonBonesCPP/blob/f696143045e07b7c4c4590bdd95534e9e8074b37/LICENSE) |
| 2D 骨骼 | [Spine Runtimes](https://github.com/EsotericSoftware/spine-runtimes) | 轨道、混合、事件、换装与 UE 运行时 | 专有许可，单独评估工具与集成；[许可](https://github.com/EsotericSoftware/spine-runtimes/blob/1ad4e94bc3f16f4908a87a97d31e0de657f1788a/LICENSE) |
| 动画制作 | [Aseprite](https://github.com/aseprite/aseprite) | 图层、标签、逐帧时长、命令行导出 | 工具链参考；当前源码为专有 EULA；[许可](https://github.com/aseprite/aseprite/blob/14fe117e853bd642672e72018166239506060cd4/EULA.txt) |
| 地图制作 | [Tiled](https://github.com/mapeditor/tiled) | 逐帧时长、统一动画时钟、编辑数据 | 学编辑器数据；GPL-2.0／BSD 混合组件；[许可](https://github.com/mapeditor/tiled/blob/221be2066c4b0ed4bbefbcdfe25d0ad952fc51bd/COPYING) |
| 队伍战术 | [OpenRA](https://github.com/OpenRA/OpenRA) | 小队状态、战力估值、撤退与错峰思考 | 独立实现机制；GPL-3.0-or-later；[许可](https://github.com/OpenRA/OpenRA/blob/e7e9e4ad270221d7fb9725b348c75504d48c1ebd/COPYING) |
| 战术分工 | [0 A.D. 历史镜像](https://github.com/0ad/0ad) | 计划、集结、目标容量与分批更新 | 归档快照参考；相关 AI 代码 GPL-2.0-or-later；[许可](https://github.com/0ad/0ad/blob/61a3b9507d974084e6badb88a0826bd89a6d5b8b/LICENSE.txt) |
| 大量单位导航 | [Mindustry](https://github.com/Anuken/Mindustry) | 共享流场、代价地图、卡住恢复 | 规模扩大后参考；GPL-3.0；[许可](https://github.com/Anuken/Mindustry/blob/17012d61d9e4c3cbd9820d87e81c24495486ac53/LICENSE) |
| AI 执行框架 | [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP) | 取消契约、黑板版本、状态观察 | 先学接口；MIT；[许可](https://github.com/BehaviorTree/BehaviorTree.CPP/blob/62c4bb4ce0c4ce47c77f95e091b348f02ace6a32/LICENSE) |
| 导航与群体移动 | [RecastNavigation](https://github.com/recastnavigation/recastnavigation) | 分片寻路队列、走廊、局部避让 | 先评估 UE 已有集成；zlib；[许可](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/License.txt) |
| 局部避让 | [RVO2](https://github.com/snape/RVO2) | ORCA 速度约束 | 小型对照试验；Apache-2.0；[许可](https://github.com/snape/RVO2/blob/75822bba14462c2e872ca96423825017ecd24764/LICENSE) |
| 自动战斗经济 | [SNKRX](https://github.com/a327ex/SNKRX) | 商店等级、利息、升阶与阵容循环 | 学机制；MIT；[许可](https://github.com/a327ex/SNKRX/blob/6b93a64d694d59472375467648868ae4521d6706/LICENSE) |
| Roguelike 资源 | [Shattered Pixel Dungeon](https://github.com/00-Evan/shattered-pixel-dungeon) | 深度档位、耗尽式牌堆、商店结构 | 独立实现机制；GPL-3.0；[许可](https://github.com/00-Evan/shattered-pixel-dungeon/blob/57a4e06a4caf162446d1c28caa7983f0493fecf0/LICENSE.txt) |
| 资源与风险 | [Brogue CE](https://github.com/tmewett/BrogueCE) | 累计金币预算纠偏 | 独立实现机制；AGPL-3.0；[许可](https://github.com/tmewett/BrogueCE/blob/104a20fedbe7d9360d2dfc8ff665571694212ad8/LICENSE.txt) |
| 深度与物品 | [Dungeon Crawl Stone Soup](https://github.com/crawl/crawl) | 物品生成深度、商品估值、特殊分支 | 独立实现机制；GPL-2.0 系列，含文件例外；[许可](https://github.com/crawl/crawl/blob/c2a4187224ec29a7f3c4bf9a384ec070bc53e64b/LICENSE) |
| 战略经济 | [fheroes2](https://github.com/ihhub/fheroes2) | 遭遇奖池、交易与难度参数 | 独立实现机制；GPL-2.0；[许可](https://github.com/ihhub/fheroes2/blob/3042b215fe1afd575508e601d9da34ce3dfdce5d/LICENSE) |
| 长期经济 | [OpenTTD](https://github.com/OpenTTD/OpenTTD) | 收入和成本分别建模、价格目录 | 学经济建模；GPL-2.0；[许可](https://github.com/OpenTTD/OpenTTD/blob/d48d693d7ff09b643846e23a5f033cf3582ab27f/COPYING.md) |
| 自动棋商店 | [Pokémon Auto Chess](https://github.com/keldaanCommunity/pokemonAutoChess) | 等级品质矩阵、特殊商品独立规则 | 学概率结构；GPL-3.0；角色素材不作为可用资产；[许可](https://github.com/keldaanCommunity/pokemonAutoChess/blob/8efd8319787047a5758e417dac9c0d3a5cffe778/LICENSE) |
| 路线生成 | [Slay the Spire Map in Unity](https://github.com/silverua/slay-the-spire-map-in-unity) | 层、路径与节点配置分离 | 独立适配现有地图；MIT；[许可](https://github.com/silverua/slay-the-spire-map-in-unity/blob/a60376a1e501db2c76f8b4a96a59ec4a0e18ce03/LICENSE) |
| 自动调参 | [Optuna](https://github.com/optuna/optuna) | 参数试验、约束、多目标与 Pareto 候选 | 离线研究工具；MIT；[许可](https://github.com/optuna/optuna/blob/13a3ff1c78314b6a1117dcee9dbbe54ab66788f9/LICENSE) |
| 随机化验证 | [fast-check](https://github.com/dubzzz/fast-check) | 性质测试、缩减反例、状态机命令 | 适合工坊/数据测试；MIT；[许可](https://github.com/dubzzz/fast-check/blob/03d658ba016351599f43a085066dbb2b15f645ff/LICENSE) |
| 博弈研究 | [OpenSpiel](https://github.com/google-deepmind/open_spiel) | 状态、观察、合法动作和策略评估分离 | 长期研究接口；Apache-2.0；[许可](https://github.com/google-deepmind/open_spiel/blob/48401890ee9857e611678302371378175a8e4c6b/LICENSE) |

## 4. 角色动画与内容生产

### 4.1 PaperZD：最接近当前技术栈的候选

PaperZD 官方 [v2.2.4 发布说明](https://github.com/heavybullets/PaperZD/releases/tag/v2.2.4)明确加入 UE 5.8 支持；这是官方兼容声明，本项目尚未安装、编译或实测。其通知按前后播放区间判断，处理循环跨界，播放器维护活跃通知，对被中断的活跃通知调用 `OnNotifyAborted`，由具体通知实现清理。动画层和方向参数使表现组织更灵活。来源：[Notify](https://github.com/heavybullets/PaperZD/blob/91439935af58854ea741691e8bfb2535ff9c0a4b/Source/PaperZD/Private/Notifies/PaperZDAnimNotify.cpp)、[AnimPlayer](https://github.com/heavybullets/PaperZD/blob/91439935af58854ea741691e8bfb2535ff9c0a4b/Source/PaperZD/Private/AnimSequences/Players/PaperZDAnimPlayer.cpp)、[分层节点](https://github.com/heavybullets/PaperZD/blob/91439935af58854ea741691e8bfb2535ff9c0a4b/Source/PaperZD/Private/AnimNodes/PaperZDAnimNode_LayerAnimations.cpp)。

**适合借鉴：** 给脚步、挥动音、尘土、蓄力光建立动作实例、区间触发、去重和中断生命周期。当前动画不决定伤害、冷却或 RNG，建议保留这个边界：游戏逻辑发出权威命中事件，表现与其对齐，低帧率跳过某一显示帧不能漏掉或重复伤害。

**接入建议：** 先选一名双足角色，用现有距离相位驱动播放，比较堵路、停走、控制、风移、技能移动、死亡和复活。不要在一个版本内迁移全部单位。动画分层也不会自动把现有整张 PNG 拆成可独立运动的上身、下身。

### 4.2 Flare 与 Godot：低帧数也可以有清楚的动作节奏

Flare 的动画定义包含时长、有效帧/子帧和绘制偏移，附属动画可以同步帧、子帧与播放状态。它说明“起手—接触—回收”可以用数据表达，而不必靠大量新帧。来源：[Animation](https://github.com/flareteam/flare-engine/blob/1969baeb89ebf94cf22e2bc2ed4e3d8413714d4f/src/Animation.cpp)、[AnimationSet](https://github.com/flareteam/flare-engine/blob/1969baeb89ebf94cf22e2bc2ed4e3d8413714d4f/src/AnimationSet.cpp)。

Godot 官方骨骼示例将基础运动状态和 OneShot 动作分开，按移动速度调整播放；等距示例保存最后朝向，并将地面阴影、人物画面和碰撞节点分离。它不是足底锁定算法，八方向也需要对应素材。来源：[骨骼示例](https://github.com/godotengine/godot-demo-projects/blob/b761b4cd5718a7118f2c95eb55f181d0a5f004f1/2d/skeleton/player/player.gd)、[等距角色](https://github.com/godotengine/godot-demo-projects/blob/b761b4cd5718a7118f2c95eb55f181d0a5f004f1/2d/isometric/player/goblin.gd)、[节点结构](https://github.com/godotengine/godot-demo-projects/blob/b761b4cd5718a7118f2c95eb55f181d0a5f004f1/2d/isometric/player/goblin.tscn)。

**建议：** 给四帧攻击设置非等长停留，强化可读前摇与接触，回收阶段保留清晰结束。身体、武器与特效若拆层，使用同一动作实例和相位。可试验淡色接地阴影，但不改变逻辑根节点、碰撞与攻击距离；高台和驻守要单独检查。保持当前绘本美术，不因引擎示例而重画成像素风或八方向。

### 4.3 Aseprite 与 Tiled：制作元数据比继续猜测像素更稳妥

Aseprite 的 JSON 导出保存帧时长、裁剪区域、原始尺寸、标签和切片/枢轴；CLI 支持批处理与按层/标签导出。Tiled 的图块动画逐帧保存时长，由统一时钟推进，并正确跨过多个帧边界。来源：[Aseprite exporter](https://github.com/aseprite/aseprite/blob/14fe117e853bd642672e72018166239506060cd4/src/app/doc_exporter.cpp)、[CLI 选项](https://github.com/aseprite/aseprite/blob/14fe117e853bd642672e72018166239506060cd4/src/app/cli/app_options.cpp)、[Tiled tile](https://github.com/mapeditor/tiled/blob/221be2066c4b0ed4bbefbcdfe25d0ad952fc51bd/src/libtiled/tile.cpp)、[animation driver](https://github.com/mapeditor/tiled/blob/221be2066c4b0ed4bbefbcdfe25d0ad952fc51bd/src/libtiled/tileanimationdriver.cpp)。

**建议在现有素材清单中补充：**

| 字段/概念 | 用途 |
| --- | --- |
| `frameDuration`、`actionTag` | 明确每帧停留与动作类别 |
| `strideDistance` | 一个完整步态周期对应的实际行进距离 |
| `contactPhase` | 左/右脚接触、挥动等表现事件的相位 |
| `groundAnchor`、原始尺寸、裁剪偏移 | 稳定脚底、血条与导入枢轴 |
| 来源图哈希、生成/导出版本 | 保持来源可追溯与派生资源可再生成 |

当前步幅由精灵范围估算，已有脚底和原图哈希；应在这一套清单上增加作者确认的数据。旗帜、营火等环境动画可共享时钟并错开相位，角色步态继续用各自实际位移。不要让两个 manifest 分别决定同一枢轴，也不需要把 Tiled 编辑器整体带入运行时。来源：[动画组件](../Source/little_king/ULKUnitAnimationComponent.cpp)、[步行资源处理](../Scripts/PrepareMovementFix.py)、[现行表现规范](36-ArtAudioSources.md)。

### 4.4 DragonBones 与 Spine：有明确换装/拆件需求时再评估

DragonBonesCPP 提供骨骼遮罩、层/组、权重混合和插槽内部排序；其主仓列出的适配不构成本项目 UE5.8 已兼容证据。Spine 提供多轨动作、排队、混合和 Start/Interrupt/End/Dispose/Complete 等生命周期，混出事件有阈值控制；所读官方 UE README 只明确列 UE 4.27–5.4，不能据此宣称支持 5.8。来源：[DragonBones AnimationState](https://github.com/DragonBones/DragonBonesCPP/blob/f696143045e07b7c4c4590bdd95534e9e8074b37/DragonBones/src/dragonBones/animation/AnimationState.cpp)、[Armature](https://github.com/DragonBones/DragonBonesCPP/blob/f696143045e07b7c4c4590bdd95534e9e8074b37/DragonBones/src/dragonBones/armature/Armature.cpp)、[README](https://github.com/DragonBones/DragonBonesCPP/blob/f696143045e07b7c4c4590bdd95534e9e8074b37/README.md)、[Spine AnimationState](https://github.com/EsotericSoftware/spine-runtimes/blob/1ad4e94bc3f16f4908a87a97d31e0de657f1788a/spine-csharp/src/AnimationState.cs)、[spine-ue README](https://github.com/EsotericSoftware/spine-runtimes/blob/1ad4e94bc3f16f4908a87a97d31e0de657f1788a/spine-ue/README.md)。

**适用场景：** 主角换装、武器替换、上身施法同时下身走路、披风独立摆动。**成本：** 拆件补画、骨架绑定、工具/运行时版本锁定、UE 渲染与材质适配。现有整张人物 PNG 缺少被遮挡的身体部位，不能自动获得这些能力。

建议先借鉴“动作结束”和“动作被打断”分别清理持续特效的思想。骨架内部插槽排序与全战场脚底排序是两个问题；当前同场单位有限，没有本次性能证据支持仅为了排序改用骨骼系统。

### 4.5 表现验证的具体样本

选普通双足、巨人、远程施法者各一名。检查步幅是否与图中接触一致；攻击前摇是否可读；命中声/闪光是否与权威 Impact 对齐；被控制、改指令和死亡时是否留下旧特效；大位移是否触发一串不合理脚步。暂停后全部表现时钟应遵守现有暂停规则。

这些改进优先于单纯提高帧数。姿势本身脚底漂移、盔甲轮廓变化或关节错误，需要修画，无法靠播放速度和代码全部补救。

## 5. 敌方 AI、战术与移动

### 5.1 OpenRA：让敌军有短期意图

OpenRA 的小队状态包含待命、推进、交战和撤退，并处理集结与长时间不移动；分工、小队更新、进攻使用不同节奏。战力判断会用归一化生命、攻击能力和速度，超出了“人数更多就进攻”。来源：[小队管理](https://github.com/OpenRA/OpenRA/blob/e7e9e4ad270221d7fb9725b348c75504d48c1ebd/OpenRA.Mods.Common/Traits/BotModules/SquadManagerBotModule.cs)、[地面状态](https://github.com/OpenRA/OpenRA/blob/e7e9e4ad270221d7fb9725b348c75504d48c1ebd/OpenRA.Mods.Common/Traits/BotModules/Squads/States/GroundStates.cs)、[战力判断](https://github.com/OpenRA/OpenRA/blob/e7e9e4ad270221d7fb9725b348c75504d48c1ebd/OpenRA.Mods.Common/Traits/BotModules/Squads/AttackOrFleeFuzzy.cs)。

**建议：** 先实现少量可解释意图：保护治疗者、拆墙、压制暴露后排、集结后推进。每个意图有成员、目标、最短保持时间、终止条件与失败原因。单位仍执行现有嘲讽、攻击资格、前摇、控制和指挥规则。撤退需有时间或区域边界，避免拖到虚弱；“回基地躲避”不能照搬到营地不可攻击的本游戏。

### 5.2 0 A.D.：分工和减少无谓围攻

Petra 的攻击计划包含准备、集合和执行；它统计目标被多少单位锁定，并对不同目标设置投入容量，降低无谓围攻。普通更新还会分批处理单位。这里是目标分配启发式，不是精确预测每颗弹丸的剩余伤害。来源：[attackPlan.js](https://github.com/0ad/0ad/blob/61a3b9507d974084e6badb88a0826bd89a6d5b8b/binaries/data/mods/public/simulation/ai/petra/attackPlan.js)。GitHub 仓库已于 2024 年归档，当前官方开发转到 [Wildfire Games Gitea](https://gitea.wildfiregames.com/0ad/0ad)；本文核读的是历史 GitHub 快照。

**建议：** 给破墙、拦截、保护后排设置成员预约和所需有效输出预算。普通索敌可以惩罚过量投入，但玩家集火、嘲讽与背刺锁定仍保持优先。先解决“十个近战堵在同一目标旁边”，不必引入完整 RTS 的海运、基地、领土与生产管理。

### 5.3 本项目 AI 的两个具体核查点

第一，单位 `CanDiscoverTarget` 会考虑黑云、草丛等规则，但 `GetWeakestAliveHero`、近战/远程计数以及部分法术选点直接读取实际对象。因此可能出现“单位不能发现，指挥层仍知道其精确状态”的不一致。地面法术可以伤害隐藏者，与 AI 可以凭未观测坐标精确瞄准，是两件事。

第二，当前可负担卡牌排序主要是 `100-Cost×10+CounterBonus`；普通法术选点只特别处理治疗，且半径为零直接拒绝。这不足以表达神圣祝福、增援、矩形飓风、黑云双方影响、墙朝向与空塔价值。以上属于静态源码发现，尚未在实战中复现每一种表现。来源：[OpponentBrain](../Source/little_king/ALKOpponentBrain.cpp)、[法术选点与全局查询](../Source/little_king/ALKBattleGameMode.cpp)、[单位发现规则](../Source/little_king/ALKUnitBase.cpp)。

**建议的最小分层：**

1. **观察快照：** 己方资源、可用牌、已观察敌人、已知地形、最后目击信息。隐藏后不继续刷新敌人真实坐标和生命。普通 AI 默认使用公平观察；特殊首领若全知，应作为明确能力。
2. **候选生成：** 按单位、治疗、伤害、控制、增援和建筑分别产生少量候选；先排除硬非法项，把“暂时不出牌”也列为候选。
3. **效用评分：** `有效收益 + 降低威胁 + 协同 + 目标价值 - 银币机会成本 - 位置风险 - 过量投入 - 切换成本`。各项归一化并输出分数，不能再让费用线性项压倒一切。
4. **意图与执行：** 给小队短期任务，通过已有合法出牌事务与移动入口执行，执行前重新校验。目标死亡、隐藏、道路变化会取消或重评；已完成的扣费不重复执行。
5. **波次导演：** 免费波次与条件增援继续独立遵守关卡配置，不由 AI 智能度暗中加兵、加钱。

治疗收益应限制在实际缺血量，攻击收益应扣除过量伤害；控制收益按预期减少的敌方有效输出估值。黑云同时考虑己方治疗/发现受阻，增援先检查整组容量，墙评估朝向和施工风险，塔评估可达驻守者。以 `CardType/SpellEffect/BuildingBehavior` 分组，比给 48 张卡分别写一个 AI 类更容易维护。

### 5.4 BehaviorTree.CPP：优先学取消契约和调试

ReactiveSequence 在条件变化时停止其他运行子节点，黑板提供条目版本和时间信息，Observer 记录节点状态变化。这些能力帮助控制长期动作与解释决策，但不会自动提供好战术或公平视野。来源：[ReactiveSequence](https://github.com/BehaviorTree/BehaviorTree.CPP/blob/62c4bb4ce0c4ce47c77f95e091b348f02ace6a32/src/controls/reactive_sequence.cpp)、[黑板](https://github.com/BehaviorTree/BehaviorTree.CPP/blob/62c4bb4ce0c4ce47c77f95e091b348f02ace6a32/include/behaviortree_cpp/blackboard.h)、[Observer](https://github.com/BehaviorTree/BehaviorTree.CPP/blob/62c4bb4ce0c4ce47c77f95e091b348f02ace6a32/src/loggers/bt_observer.cpp)。

**建议：** 即使保留自写状态机，也统一 `Running/Succeeded/Failed/Cancelled`、释放预约与清理弱引用的规则。记录观测版本、前三候选、评分分项、拒绝原因、最终动作和执行结果。直接引库还需处理 UBT、异常/RTTI、UObject 生命周期和调试工具，成本高于借鉴这些契约；暂不建议为框架而换框架。

### 5.5 Mindustry、Recast、RVO2：三类不同问题

| 项目 | 代码中的机制 | 对本游戏的建议和边界 |
| --- | --- | --- |
| Mindustry | 按队伍/移动代价共享流场，敌建筑生命可进入路径成本，读上一份已完成权重 | 借鉴“绕行时间与拆墙时间比较”和卡住恢复。连续坐标、旋转墙与格子地图差异大；40 单位规模先测量，不急于替换导航 |
| Recast/DetourCrowd | 路径请求队列按迭代预算推进，再做路径走廊和局部速度选择 | 优先借鉴请求预算、过期和优先级。跨更新切片不等于后台线程；UE 已有相关模块，整体接入还涉及位置所有权 |
| RVO2 | 根据邻居和障碍建立 ORCA 速度约束，在速度上限内选择新速度 | 解决局部会车和拥挤；不负责全局寻路、拆墙、阵型目标或攻击资格。窄路、驻守、击退和风移必须独立验证 |

源码：[Mindustry Pathfinder](https://github.com/Anuken/Mindustry/blob/17012d61d9e4c3cbd9820d87e81c24495486ac53/core/src/mindustry/ai/Pathfinder.java)、[GroundAI](https://github.com/Anuken/Mindustry/blob/17012d61d9e4c3cbd9820d87e81c24495486ac53/core/src/mindustry/ai/types/GroundAI.java)、[DetourPathQueue](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/DetourCrowd/Source/DetourPathQueue.cpp)、[DetourCrowd](https://github.com/recastnavigation/recastnavigation/blob/9f4ce64458dfae86e1239c525ddc219c4e9e06f1/DetourCrowd/Source/DetourCrowd.cpp)、[RVO2 Agent](https://github.com/snape/RVO2/blob/75822bba14462c2e872ca96423825017ecd24764/src/Agent.cc)。

本项目已有有界网格 A*、几何缓存与精确碰撞。下一步可先统计每帧请求数、缓存命中、搜索节点数、请求年龄与卡住时长；必要时加入全队预算。任何软代价、避让或后台路径，最终仍通过真实精确碰撞。不能因为局部避让建议了某个速度，就允许穿过刚建成的墙。来源：[LKNavigation](../Source/little_king/LKNavigation.cpp)、[移动组件](../Source/little_king/ULKUnitMovementComponent.cpp)。

### 5.6 建议让敌人形成可辨认的战术风格

共享决策框架之上，可以给遭遇配置不同的权重和意图。以下均是候选设计，不是现有 AI 行为说明：

| 风格 | 优先行为 | 玩家可读信号与反制 |
| --- | --- | --- |
| 冲锋近战 | 集结后集中突破，减少零散送兵 | 出发前短预警；用墙、控制和侧翼拉扯打断节奏 |
| 远程阵地 | 保持前排保护，优先攻击可见暴露目标 | 前后排形状清楚；绕侧、控制或攻城拆支点 |
| 城防驻守 | 保持塔/治疗站与合适成员的关系，保护施工 | 施工和驻守可见；趁未成形进攻或切断入口 |
| 亡灵消耗 | 围绕能触发转化的战场持续交战，保护法师 | 明确转化与生命代价反馈；优先切断关键施法者 |

难度可以调反应延迟、候选数量、评分噪声、战术协同和保持时间。各档仍遵守相同观察与合法性规则；更难的敌军应更善于利用已知信息。主动技能前摇、集火预警和行动后的空窗让玩家有机会理解并反制。避免让所有阵营都变成同一套最快集火算法，也避免每次策略切换都打断普攻。

## 6. 数值、经济与随机资源：完整游戏的经验

### 6.1 SNKRX：商店升级与经济决策形成闭环

SNKRX 三槽商店实际读取 `level_to_shop_odds`，五级的 T1/T2/T3/T4 裸权重为：70/20/10/0、50/30/15/5、25/45/20/10、10/25/45/20、5/15/30/50。先抽档位再抽角色；全组三张都不会攻击时重新生成，最终分布会受这个条件影响。源码还有另一张 tier 表，不能不追调用就当作商店概率。来源：[实际权重表](https://github.com/a327ex/SNKRX/blob/6b93a64d694d59472375467648868ae4521d6706/main.lua#L1631)、[商店生成](https://github.com/a327ex/SNKRX/blob/6b93a64d694d59472375467648868ae4521d6706/buy_screen.lua#L343)。

基础战后利息为 `min(floor(gold/5),5)`，商人另有规则，刷新商店花 2 金。这形成“现在增强、刷新找牌、攒钱增收”的取舍。来源：[结算](https://github.com/a327ex/SNKRX/blob/6b93a64d694d59472375467648868ae4521d6706/arena.lua#L968)、[刷新扣费](https://github.com/a327ex/SNKRX/blob/6b93a64d694d59472375467648868ae4521d6706/buy_screen.lua#L988)。

**对本游戏的建议：** 学习显式阶段表和坏店修复，让市场至少有一定比例的有效构筑选择。利息与付费刷新先不照搬：本游戏市场很少，临时佣兵不带回，还有永久金币和银币两个层次，额外复利可能使囤积成为压倒性策略。若尝试刷新，应同时设计次数、费用增长、保存与 RNG，不能仅加一个按钮。

### 6.2 Shattered Pixel Dungeon：控制长期资源数量，保留短期随机

SPD 装备档位随 `depth/5` 分段，所读普通生成表中 T5 从浅层 1% 升到深层 80%。物品类别还采用两副交替耗尽的 35 次牌堆，每副含药水 8、卷轴 8、金币 10；抽走后减少余量，耗尽换堆。特定掉落入口可以用其他规则，不能把牌堆保证扩展到所有掉落。来源：[Generator 档位表](https://github.com/00-Evan/shattered-pixel-dungeon/blob/57a4e06a4caf162446d1c28caa7983f0493fecf0/core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/items/Generator.java#L613)、[类别配置](https://github.com/00-Evan/shattered-pixel-dungeon/blob/57a4e06a4caf162446d1c28caa7983f0493fecf0/core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/items/Generator.java#L221)。

商店混合阶段装备和特殊槽，库存洗牌使用单独的随机生成器上下文。来源：[ShopRoom](https://github.com/00-Evan/shattered-pixel-dungeon/blob/57a4e06a4caf162446d1c28caa7983f0493fecf0/core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/levels/rooms/special/ShopRoom.java)。

**对本游戏的建议：** 以区域或整轮为单位设计小型功能供应袋，让前排、治疗、攻城等机会的长期数量受控，顺序保持随机。大到几十次才耗尽的袋子不适合每轮仅约八家店的节奏。永久研究另设目标，不与临时战力共用保底；借鉴的是方差控制，不是直接采用它的装备掉落率。

### 6.3 Brogue CE：补偿供应不足，不补偿玩家花掉的钱

Brogue 默认一般物品数量存在随机长尾；从一定深度开始检查累计已生成金币是否在预算区间，偏低时增加后续金币堆，偏高时减少。它衡量生成供应，不是玩家消费后的钱包。部分关键物品也有随深度累计、掉落后扣减的计量规则。来源：[物品数与金币纠偏](https://github.com/tmewett/BrogueCE/blob/104a20fedbe7d9360d2dfc8ff665571694212ad8/src/brogue/Items.c#L577)、[计量条件](https://github.com/tmewett/BrogueCE/blob/104a20fedbe7d9360d2dfc8ff665571694212ad8/src/brogue/Items.c#L703)、[默认变体参数](https://github.com/tmewett/BrogueCE/blob/104a20fedbe7d9360d2dfc8ff665571694212ad8/src/variants/GlobalsBrogue.c)。

**对本游戏的建议：** 若未来随机金币或战利品变化增多，维护“该阶段计划提供多少、实际提供多少”的账本，用小幅后续供给纠偏。当前金币主要是节点固定奖励，近期更适合把这个思想用在构筑机会而非额外发钱。按当前钱包贫富自动补钱会奖励激进消费并削弱储蓄选择，不建议采用。

### 6.4 DCSS：生成预算、物品种类和价格分别控制

DCSS 商店先按深度和店型产生 item level，再生成符合店型与合法性条件的物品，价格另用价值与 greed 等参数。深度 6 普通鉴定店的候选预算可落在 6–19，但这个范围不是“品质 6–19”，也不能据预算阈值直接断言某种神器概率。来源：[商店生成预算](https://github.com/crawl/crawl/blob/c2a4187224ec29a7f3c4bf9a384ec070bc53e64b/crawl-ref/source/dungeon.cc#L6393)、[商店估价](https://github.com/crawl/crawl/blob/c2a4187224ec29a7f3c4bf9a384ec070bc53e64b/crawl-ref/source/shopping.cc)。

**对本游戏的建议：** 分开回答“这一阶段允许什么”“这次给什么水平的选择”“它值多少钱”。未来可以尝试基础补给店与高波动高级店，但每区仅两家店，店型分化太强容易让必要支援缺席。先做统一的阶段表，再考虑店型和地区特色商品。

### 6.5 fheroes2：排重与权重归一化都要追到执行代码

其神器池优先选择有效、允许且未用过的对象，空池时才放宽未用过条件，避免轻易重复。地图货车奖励的权重是 20 空、10 神器、50 资源，总和 80，经随机队列归一化，实际为 25%、12.5%、62.5%；不能把注释里的数字直接当百分比。来源：[神器池](https://github.com/ihhub/fheroes2/blob/3042b215fe1afd575508e601d9da34ce3dfdce5d/src/fheroes2/resource/artifact.cpp#L434)、[地图奖励](https://github.com/ihhub/fheroes2/blob/3042b215fe1afd575508e601d9da34ce3dfdce5d/src/fheroes2/maps/maps_tiles_helper.cpp#L1058)、[权重队列](https://github.com/ihhub/fheroes2/blob/3042b215fe1afd575508e601d9da34ce3dfdce5d/src/engine/rand.cpp#L121)。

**对本游戏的建议：** 所有配置同时输出“原始权重”和“归一化后的条件概率”。永久研究可考虑已解锁过滤、重复兑换或有限重选，但排除已拥有项目会加速全收集，必须作为经济变化计算。店内佣兵已经不重复，不需要重复实现这一能力。

### 6.6 OpenTTD：把收入和成本的增长分别建模

OpenTTD 分别维护收入与价格指数，使用不同增长率，并设年代窗口和上限；基础建设/运行费用又有分类和难度倍率。它展示了长期经济不能只看单笔交易，而要关注收入、成本随时间的相对斜率。来源：[economy.cpp](https://github.com/OpenTTD/OpenTTD/blob/d48d693d7ff09b643846e23a5f033cf3582ab27f/src/economy.cpp#L698)、[价格目录](https://github.com/OpenTTD/OpenTTD/blob/d48d693d7ff09b643846e23a5f033cf3582ab27f/src/table/pricebase.h)。

**对本游戏的建议：** 按完成区域和出征次数统计永久金币净流入、家园升级支出、研究支出与满级后的积压。当前不需要日历通胀，也不建议店价跟着玩家钱包自动上涨。只有数据证明长期金币失去用途，才讨论新的可选消耗；先解决现有高价商品不可购买的问题。

## 7. 商品出现阶段与概率：建议的设计方式

### 7.1 Pokémon Auto Chess：阶段表和例外规则分开

该项目把等级对应的五档商店权重写成显式矩阵。其第 1 级为 `[100,0,0,0,0]%`，第 5 级为 `[36,42,20,2,0]%`，第 8 级为 `[11,27,35,22,5]%`；另有特定角色的最低阶段与刷新次数增量。矩阵描述基础抽样权重，特殊角色、可用池和后续逻辑还会影响最终结果，不能把裸表当作任意角色的最终购买概率。来源：[shop.ts](https://github.com/keldaanCommunity/pokemonAutoChess/blob/8efd8319787047a5758e417dac9c0d3a5cffe778/app/config/game/shop.ts)。

**适用之处：** 把阶段、商品类别、基础概率与特殊例外集中成可审计数据。**不宜照搬之处：** 多人抢公共卡池、合星需求和频繁刷新，与本项目一轮约 8 次市场、临时佣兵不带回、永久研究解锁的节奏不同。该仓角色与美术基于 Pokémon IP，本文只借鉴代码中的概率组织方式。

### 7.2 先定义可进入池的商品，再讨论品质权重

建议商品生成顺序为：**机会预算 → 类别 → 阶段资格 → 品质 → 具体商品 → 去重/保底 → 保存结果**。每层都要有明确条件与空池回退，并输出最终边际概率用于审计。

| 条件 | 应解决的问题 |
| --- | --- |
| 最低地区深度／指定区域 | 让新机制随着旅程逐步出现 |
| 卡牌功能标签 | 当前牌组缺前排、治疗、攻城或防空时，提供一定构筑机会 |
| 战斗银币上限 | 买到后是否真的能够出牌；高费“未来投资”需明确提示 |
| 牌组占格及最低卡种 | 高价值卡是否因 2/3 格占用而无法合理加入 |
| 已解锁／待研究状态 | 避免永久商品反复占据稀有位置，或提供明确重复用途 |
| 钱包与阶段购买力 | 常规商品至少留有可负担选择；昂贵追求项允许存在 |
| 原有远征版本和种子 | 保存后稳定，不因查看、重进或升级配置刷新 |

这些条件不宜全部变成硬过滤。例如“必定只出克制下一场的卡”会让商店替玩家解题；更适合保留随机性，给基础功能设置软保底，并允许提前购买尚未能使用的追求项。

### 7.3 三格佣兵市场的候选试验表

下面是**用于首轮对照试验的建议参数，不是本游戏已采用数值，也不是从某个外部游戏照抄**。只使用当前临时佣兵池实际存在的三档品质。

| 地区深度 | 优秀 | 稀有 | 史诗 | 设计意图 |
| --- | ---: | ---: | ---: | --- |
| 0 | 80% | 20% | 0% | 建立基本角色与低费循环 |
| 1 | 60% | 35% | 5% | 引入构筑核心，允许小概率惊喜 |
| 2 | 40% | 45% | 15% | 强化协同与高费选择 |
| 3 | 25% | 50% | 25% | 提供后期完善阵容的机会 |

这张表是普通格的目标品质权重：先抽品质，再从该品质剩余合格角色中选择并移除；不要把表中同一个数赋给每名角色后全池抽取，否则候选更多的品质会被放大。空品质需要明确回退。若采用全池逐角色权重，应按该档剩余数量分配品质总权重并重新计算；加入“至少一格可负担”或“缺失功能保底”后，最终分布也会改变，应重新枚举/模拟，而不是仍宣称等于裸表。

若粗略按三格独立、有足够候选估计，5%/15%/25% 的单格史诗率对应一店至少一张约 14.26%/38.59%/57.81%；不放回、空池和保底后不能直接使用这个近似。保留优秀商品有实际意义：较低银币费用与占格可以支持循环，品质不应让旧卡自动报废。

### 7.4 永久解锁应有稳定进度渠道

建议把“核心玩法解锁”与“稀有惊喜”分开设计。冰冻、黑云、墙、治疗站等机制，如果长期不出现，玩家就无法形成完整构筑体验；高度随机适合奖励变化，不适合承担全部基础学习路径。

可选择以下一种主结构，再少量组合，避免多重保底互相叠加：

- **章节研究配额：** 每个区域两个市场共享一份研究机会袋，达到固定条件后至少提供一次合格研究选择；商品可以随机，基础功能不完全依赖低概率。
- **累计未见补偿：** 记录合格机会数而不是总游玩时间，连续未见提高出现率；计数在首次合法生成/访问时持久化，读档不推进。
- **研究点兑换：** 重复书或首领奖励提供可积累进度；指定高阶内容有较长但有限的兑换路径。需要新资源和界面，成本更高。
- **定向章节奖励：** 某些机制由明确遭遇/区域首次完成奖励开放，商店负责补充与提前获得。适合让新内容与关卡教学自然结合，而不新增独立教程文档。

必须分清“看见保底”和“获得保底”：看到了却买不起时，若立即清空保底，会让玩家感到进度落空；若必须购买才清空，又可能被反复用于刷高价值选择。规则应先确定，日志分别记录生成、展示、购买、带回、研究五个事件。

对于已生成的旧远征，沿用既有商品；新规则绑定新远征的 `GenerationVersion` 或等价版本字段。永久收藏过滤若依赖出发时状态，应使用快照，避免同一 RunId 因外部状态变化产生不同商品。当前佣兵洗牌、OfferId、法术书和图纸共用节点随机流，改佣兵抽取次数就可能改变后续研究结果；应显式选择生成器版本兼容或拆分随机流，不能依靠“种子没变”假定内容不变。

## 8. 难度曲线与战斗数值的建议

### 8.1 从面板分数转向场景中的有效能力

`DPS≈伤害/攻击间隔`、`EHP≈生命/实际承伤比例` 可以发现异常，但不足以把治疗、控制、召唤、范围伤害、驻守与只打建筑单位压成一个公平价格。建议建立小型对照场景矩阵：单挑、多人围攻、远程风筝、窄路、墙阵、高台、草丛/黑云、双治疗、召唤与死亡触发。

记录每单位/卡的有效伤害、有效治疗、控制覆盖、走路时间、存活时间、伤害浪费、银币消耗与目标类别。增援计整组容量；兵营计存活期间实际产兵；亡灵法师计转化收益和生命代价；窃贼/哥布林大盗计额外银币反馈。这样能区分“基础数值弱”“没有合适目标”“AI 使用不当”和“地形压制”。

品质用于呈现和出现阶段，费用用于战斗机会成本，占格用于牌组机会成本，永久获取难度用于进度节奏。四者可以关联，但不应互相替代；尤其不能用“很难抽到”来证明一张卡在战斗里可以无条件压倒全部替代项。

### 8.2 把遭遇预算拆成可观察的维度

建议每个工坊布局附带元数据：地区深度、遭遇类型、主要威胁、反制手段、推荐银币上限、预期持续时间、作者版本与验证记录。预算至少区分：初始单位、免费增援、付费牌组、资源产速/上限、地形优势、特殊规则。

同样的总生命，分 20 秒集中到场和分 180 秒零散到场，对玩家压力不同。当前普通/精英/首领增援与法阵节奏不同，单位容量上限和失败重试还会造成拥堵后的集中释放，因此不能只把所有波次数量求和。用滑动时间窗内的到场战力、玩家有效银币供给和英雄可承受压力共同分析。来源：[平衡规则](../Source/little_king/LKBalanceRules.cpp)、[波次执行](../Source/little_king/ALKOpponentBrain.cpp)。

**建议的章节节奏：** 安全展示一种机制 → 给资源选择 → 将两种已见机制组合 → 恢复与调整 → 首领检验。不要在一个普通节点同时首次引入高台、禁军之墙、强治疗和新控制。提高后期难度优先增加组合与空间决策，再校正属性；保留 300 秒后递增虚弱的收束机制，并观察超长战斗。当前 300 秒不是硬结束时间。

### 8.3 家园成长与远征难度要分层记录

同一遭遇至少分别观察新档、部分解锁、接近满成长三种状态。家园金库改变银币上限与产速，永久收藏改变可选牌组，其他建筑改变恢复或携带能力；只汇总一个总体胜率会把成长差异误判为玩家水平差异。

建议统计每一地区的到达率、条件胜率、英雄剩余生命、失能数、花费、休息选择、战斗时长分位数和退出率。整轮时长应按“各类节点数量×实测时长＋界面决策时间”估计，不能用少量历史战斗样本直接推断当前整轮体验。

自适应难度先用于开发分析或明确可选模式。若悄悄根据玩家牌组即时生成克制敌阵，会削弱构筑与路线选择的意义。较安全的做法是公开难度档、可选挑战词缀、已生成后不变的种子和区域预告。

## 9. 经济系统：把钱的用途和反馈回路画清楚

### 9.1 战斗银币与远征金币分开评估

战斗银币按时间生成、有上限、每房重建，用于当局出牌；远征金币用于市场消费和家园成长，可跨结局带回。基础银币每秒 1/3、上限 5，金库升级改变出发快照。成功/放弃带回剩余金币 100%，失败带回 80% 向下取整，通关另加 20 金币，已购研究材料全部带回。来源：[GDD](01-GDD.md)、[银币组件](../Source/little_king/ULKSilverComponent.cpp)、[远征结算](../Source/little_king/ULKRunSubsystem.cpp)。

拖延并持续出牌可以增加本场累计银币支出，但当前没有战斗时长/普通击杀转远征金币的通用入口；免费增援直接生兵，默认波次有限。300 秒后有递增虚弱压力，但仅凭静态源码不能证明所有治疗/召唤组合都必定迅速结束。应测“是否能拖延获利”，不应未经验证就把它写成无限刷金币漏洞。

另有一处值得后续核对的 UI 一致性问题：[家园提示](../Source/little_king/ALKHomeGameMode.cpp)仍包含失败全额带回的旧表述，与实际 80% 结算不一致。本文按结算源码分析，未修改该文案。

因此，自走棋的“存钱拿利息”若直接套在银币上，会与现有低上限、即时防守和循环手牌冲突；金币投资若又提高下一次赚钱效率，则可能形成永久滚雪球。应先列账本，再决定是否引入利息或新资源。

| 账本 | 主要流入 | 主要流出/约束 | 优先观察 |
| --- | --- | --- | --- |
| 战斗银币 | 时间、单位技能等 | 出牌、上限溢出、每房重建 | 满币闲置、每费有效收益、技能经济回本时间 |
| 远征钱包 | 出发本金、胜利奖励 | 市场购买、失败损失 | 到店购买力、放弃时机、无消费率 |
| 永久金币 | 结算带回 | 家园升级、下轮本金转移 | 解锁速度、升级顺序垄断、满级后积压 |
| 永久内容 | 买回研究材料、研究 | 一次解锁与重复材料处理 | 首次获得轮数、重复率、实际使用率 |

### 9.2 建议用“可购买次数”和“回本战斗数”设计价格

对每一类商品同时报告：到达该店时钱包 P10/P50/P90、价格/钱包比、看见后购买率、购买后入组率、使用率和完成远征率。当前佣兵商品价格为 `20+15×银币费用+10×品质枚举值`，研究书/图纸则使用独立定价表；这些初始化方式都不保证与某个阶段的真实购买力匹配。

永久升级可估计 `回本轮数≈升级成本/每轮新增净收益`，但对增加构筑选择、降低失败风险的升级，应另外看体验价值。不能只给所有升级一个金币收益指标，否则系统会倾向鼓励最赚钱的路线而压低其他玩法。

将“早退可以保留全部剩余金币”视为明确的风险选择，而不是先判定为漏洞。需要比较提前返回、继续推进和失败三条路径的单位时间净收益；若反复跑起点市场或低风险节点显著优于推进，优先调整奖励分布、首次/深度奖励或高阶内容获取条件，而非直接加重失败惩罚。

### 9.3 重点观察几条反馈回路

- 银币技能 → 更多单位/控制 → 更容易触发银币技能：用上限、可触发目标、频率和机会成本共同限制，不只看单次概率。
- 治疗/驻守 → 活得更久 → 兵营产更多兵 → 更安全：用实际持续收益评估建筑，不能只比较初始血量和费用。
- 首轮强解锁 → 后续胜率更高 → 更多永久金币：区分有意的成长体验与使新玩家长期受限的差距。
- 高价稀有品 → 长期攒钱 → 放弃普通构筑消费：若最优策略是一路不买，商店的阶段乐趣会消失；可用专门研究进度或价格结构减轻冲突。

这些是待测假设，不是已经证实的经济漏洞。先采集实际流量，避免为假设中的通胀增加复杂货币和惩罚。

## 10. 关卡、验证与中长期研究工具

### 10.1 路线生成：图结构不等于有意义的选择

`slay-the-spire-map-in-unity` 将地图配置、层、节点和路径生成分开，支持起点/首领前节点数量、额外路径及交叉连线处理；它解决的是可编辑的路线图，不负责证明奖励与难度公平。来源：[MapConfig](https://github.com/silverua/slay-the-spire-map-in-unity/blob/a60376a1e501db2c76f8b4a96a59ec4a0e18ce03/Assets/Scripts/MapConfig.cs)、[MapGenerator](https://github.com/silverua/slay-the-spire-map-in-unity/blob/a60376a1e501db2c76f8b4a96a59ec4a0e18ce03/Assets/Scripts/MapGenerator.cs)。

本项目已经有路线生成，建议借鉴约束表达：每段至少有可行通路、危险分支与收益匹配、必要恢复机会、避免连续重复遭遇、不同路线有不同取舍。将工坊布局自动分配到远征时，先按阶段与机制标签选候选，再检查路线整体。它是尚未实现的后续集成，不应写成现有功能。

可以给作者关卡建立“威胁—反制”标签：高台远程需要可获得的远程/法术反制，墙阵需要拆墙或绕路机会；不要求所有牌组轻松通过，但应避免在玩家尚无法获得反制时出现强制检查。

### 10.2 Optuna：离线寻找候选参数

Optuna 的多目标示例为不同目标指定最小化/最大化方向，并读取 Pareto 前沿；框架支持参数试验与约束。该示例优化机器学习模型，不能作为游戏平衡有效性的证据。来源：[多目标示例](https://github.com/optuna/optuna/blob/13a3ff1c78314b6a1117dcee9dbbe54ab66788f9/tutorial/20_recipes/002_multi_objective.py)、[TPE 实现](https://github.com/optuna/optuna/blob/13a3ff1c78314b6a1117dcee9dbbe54ab66788f9/optuna/samplers/_tpe/sampler.py)。

**适合本项目的用法：** 先固定基线玩家策略与敌方策略，在作者场景里调整少量参数，例如波次间隔、敌人生命曲线或商店阶段权重。目标可以是接近指定胜率区间、降低超长战斗比例、减少某类牌组的极端劣势；非法生成、存档不兼容和碰撞违规作为硬拒绝条件。

训练场景和留出场景分开，固定一组公开回归种子，再加入独立检验种子。每次只开放少数有解释的旋钮，保留多组取舍方案交给试玩，而不是把几十个数同时交给搜索后接受一个“最优解”。模拟玩家若策略单一，算法只会平衡对这个机器人的表现。

### 10.3 fast-check：让数据边界自动产生反例

fast-check 提供模型命令序列测试，并用 seed/path 复现缩减后的反例，适合具有很多合法组合和失败回滚条件的工具。来源：[ModelRunner](https://github.com/dubzzz/fast-check/blob/03d658ba016351599f43a085066dbb2b15f645ff/packages/fast-check/src/check/model/ModelRunner.ts)、[运行参数](https://github.com/dubzzz/fast-check/blob/03d658ba016351599f43a085066dbb2b15f645ff/packages/fast-check/src/check/runner/configuration/Parameters.ts)。

建议用于工坊 JSON 和经济状态机的性质验证：

- 导出—读取—再导出保持规范化语义；拒绝超量、非法几何和不存在的身份。
- 同一输入/版本/种子生成相同内容，已生成商品不会因重复调用改变。
- 购买失败时钱包、手牌、商品、材料不变；重复提交不重复扣费/加物品。
- 无论命令顺序如何，研究不会消耗不存在的材料、结算不会二次入账。
- 随机地图中起点可达终点，关键节点约束成立，遇到无解配置能明确拒绝。

现有网页校验比 C++ 更严格，不能直接断言双端完全一致。差分验证应先定义两端共同支持的子集与允许差异，再比较接受/拒绝和规范化结果。这项工作适合在后续行为或格式变更时引入，本次调研无需重跑整个游戏测试套件。

### 10.4 OpenSpiel：给远期策略研究留接口

OpenSpiel 将状态推进、合法动作、随机分支、观察/信息状态和克隆接口分开，并提供策略评估入口。来源：[spiel.h](https://github.com/google-deepmind/open_spiel/blob/48401890ee9857e611678302371378175a8e4c6b/open_spiel/spiel.h)、[evaluate_bots.cc](https://github.com/google-deepmind/open_spiel/blob/48401890ee9857e611678302371378175a8e4c6b/open_spiel/algorithms/evaluate_bots.cc)。

最有价值的近期借鉴是接口边界：策略只读观察、返回动作，再由规则系统执行。未来可在低维抽象战斗模型中研究搜索、自动构筑和关卡评估；直接复制 UE 世界来做海量搜索成本很高，连续时间、移动、范围技能也需要建模。强化学习、多智能体训练、完整自博弈暂列远期探索，不作为改进当前 AI 的前置条件。

### 10.5 其他值得保留的方向

| 方向 | 为什么相关 | 建议触发条件 |
| --- | --- | --- |
| 命令记录与战斗复现 | 平衡/AI 问题需要复盘；仅记录 seed 不足以消除帧率与随机调用顺序差异 | 先统一模拟步长、随机流和事件序列，再承诺重放 |
| 随机流隔离 | 商店、战斗、AI、美术特效不应相互改变随机结果 | 下次生成/AI 重构时按子系统固定流与稳定排序 |
| 可访问性 | 品质、阵营、预警、控制不能只靠颜色；动画效果要可关闭 | UI/表现打磨时加入图形与文字冗余、音量分类、震屏开关 |
| 内容身份与版本 | CardId/UnitId、研究材料、工坊目录、存档迁移容易互相影响 | 新增内容时统一 manifest 和校验，保留旧 ID 映射 |
| 阵容推荐与解释 | 可以帮助玩家理解失败原因和卡牌协同 | 先用明确规则解释“缺少可攻击高台的单位”等事实，避免编造确定结论 |
| 模组与创意内容 | 已有 JSON 工坊是合理入口 | 先完成 schema 版本、资源白名单和隔离校验，再考虑脚本扩展 |
| 每日种子/挑战词缀 | 可用同一场景比较策略并增加内容复用 | 完成确定性验证及版本绑定后再做，不急于联网排行榜 |

## 11. 建议推进顺序与验收方式

下面是供选择的路线，未安排为开发任务。成本只表示相对工程量；资源质量、UI 范围和旧档兼容要求会改变实际投入。

| 优先级 | 建议工作 | 相对成本 | 验收重点 |
| --- | --- | --- | --- |
| P0 | 确定研究商品体验目标，审计全链路概率与购买力 | 小—中 | 列出每个商品按阶段的出现率、可负担率、预计获得机会，检查重复和空池 |
| P0 | 统一 AI 观察，核查新增法术候选模型 | 中 | 固定观察、内部记忆和策略随机状态，仅改变未观测真实状态时决策相同；新增效果有明确支持或拒绝原因 |
| P1 | 战斗/经济结构化遥测与固定对照场景 | 中 | 胜率、时长、英雄损耗、每费收益、钱包流量可按版本/种子复核 |
| P1 | 步幅/帧时长/接触事件元数据与少数角色表现试验 | 中 | 命中时刻不漂移、停走不滑步、中断不重复事件、原资源回退正常 |
| P1 | 阶段商店与单一研究保底方案原型 | 中 | 存量远征不重掷，分布符合预期，失败回滚和存档兼容 |
| P2 | 小队意图、成员预约与效果效用评分 | 中 | 减少空放/无效围攻，保留指挥与嘲讽规则，不产生无限撤退 |
| P2 | 工坊布局分配到远征及遭遇预算 | 中—大 | 路线可达、阶段内容匹配、可解释反制、不会改写已有战前快照 |
| P2 | 导航请求预算或局部避让对照 | 中 | 先有性能/拥堵证据，再比较 P95/P99 与卡住时长，零穿墙 |
| P3 | Optuna 调参、自动构筑与抽象模拟 | 中—大 | 留出关卡有效、参数可解释、结果经过人工试玩 |
| P3 | 全面骨骼换装、流场替换、学习型 AI | 大 | 只有内容规模/性能/制作需求证明收益时再推进 |

建议首个小周期只回答三个问题：玩家能否稳定体验到新机制；AI 是否正确且公平地使用机制；失败原因是否看得懂。对应的最小成果是概率/购买力表、观察与决策日志、几组动画和战术对照场景。

### 验证指标的使用边界

胜率按新档/成长档、牌组、地形和遭遇类型分层，给出样本量与不确定区间，不把总体平均强行定为 50%。成对种子比较保留初始快照；固定步长和随机调用契约未完成前，不宣称完全确定性重放。

概率可以先精确枚举再做抽样检查。正常抽样很难验证万分之一以下的商品，应直接验证权重、资格和概率公式；玩家试玩用来判断是否有趣。动画/AI 需要实战观察，旧版归档通过数不能写成新方案的验收结果。

## 12. 采纳边界与后续阅读

优先读 PaperZD 的状态/通知、OpenRA 的小队、SNKRX 的商店、Shattered Pixel Dungeon 的资源生成和 Brogue 的累计预算，再看本报告的概率分析。需要解决明确技术瓶颈时，再读 Recast/RVO2、Optuna 与 OpenSpiel。

开源游戏的规则适合研究后按本项目接口独立实现。GPL/AGPL 游戏代码不作为直接并入当前 UE 游戏的依赖推荐；宽松许可库也仍有版本、依赖和构建适配成本。Spine 与 Aseprite 的源码可见不等于采用常见开源许可；第三方角色、美术、音效及旧商业游戏资源不能由代码仓库许可自动推定可用。具体来源保持在[素材来源文档](36-ArtAudioSources.md)登记。

本次新增一份调研报告及入口链接，没有改动玩法、概率配置、AI、资源或玩家存档。文中的外部机制是源码阅读结果，提议是针对《小小国王》的设计判断；实际采纳应在选定范围内做原型和验证。
