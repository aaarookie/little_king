# v0.8.5 · 战场工坊、地形与遭遇

发布日期：2026-10-09；工程版本 `0.8.5`。本页合并本阶段的设计接口、集成记录与发布验证。现行通用玩法见 [游戏设计](01-GDD.md)，协作入口见 [项目交接](64-ProjectHandoff.md)。

## 交付范围

离线[战场工坊](../Tools/BattleDesigner/index.html)提供 32 种积木、旋转、复制、缩放、撤销重做、本地草稿、图片导出和 JSON 回读。目录含 46 种 UnitId 与 48 张 CardId；两者不可互换。本版没有新增可收集卡牌，研究目录仍为 16 种法术、8 种建筑图纸。

设计 JSON 由 `Scripts/PlayBattleLayout.ps1` 交给 UE 独立战场，生成地形与单位并安装波次、部署区和敌方经济。脚本参数 `-Layout` 指定文件，`-EngineRoot` 指定引擎，`-AutoStart` 用于测试自动部署。此入口不写入家园或远征存档；尚未自动分配到远征节点，也不导出 `.umap`。

## 地形与部署规则

| 地形 | 行为 |
| --- | --- |
| 草丛 | 相接或重叠区域组成连通片；片内互见，对片外敌人隐藏。向片外攻击时，自身及周围 2 米内己方草丛单位暴露 3 秒，再次攻击延续暴露 |
| 沼泽 | 移速乘 0.7，离开恢复，重叠不叠乘，不影响攻速；单位接口 `SetSwampMovementMultiplier` 预留免疫或加速 |
| 军营篝火 | 无血量、不可攻击的实体障碍，参与绕行、出生和放置检查 |
| 高山 | 不可通行的固定高台；驻军不移动、不受强制位移，地面近战不能伤害高台单位，远程与法术沿用现有命中规则 |

草丛隐藏与黑云独立管理，草丛暴露不会清除黑云；不可见敌人不能继续被自动锁定。高山没有上下山路径，也不能被摧毁。

草地、道路、水域、花丛仅为装饰；树木和岩石不可破坏且阻挡。我方部署区约束英雄、佣兵、建筑，并保留己方半场和碰撞检查；法术仍用原有施法范围。敌方出现区与文字只是标注。地图尺寸同步战场边界和镜头，角色图标的设计尺寸不改变单位数值或碰撞体。

## 波次与敌方经济

免费事件增援与消耗银币的敌方卡牌并行；关闭主动出牌不影响波次。敌方牌组限定可用卡，出牌顺序仍由现有策略决定，支持单卡循环。玩家四槽唯一手牌规则见 [游戏设计](01-GDD.md)。

| 波次字段 | 约定 |
| --- | --- |
| `id` / `label` | 稳定 ID；名称 1～100 字符。对象与波次共用 ID 唯一空间，按 ASCII 大小写不敏感比较 |
| `trigger` | `time`、`hero-health-below`、`hero-first-death` |
| `time` | 0～3,600 秒；定时为开战后秒数，条件触发为首次满足后的延迟 |
| `targetId` | 条件波次指向开局出现、数量为 1 的具体英雄或首领对象，引用按大小写不敏感匹配；定时波次必须为空字符串 |
| `healthThreshold` | 0.01～0.99，血量触发使用严格小于比较 |
| `unitId` / `count` | 规范 UnitId；每条 1～50 个 |
| `x` / `y` / `radius` | 地图内米坐标；搜索半径 0～10 米，为 0 时只尝试锚点 |

所有波次以正式开战为计时起点，每条只登记、执行一次。首次低血量后回血再下降、英雄复活后再次倒下均不会重复触发。条件按对象原始 ID 绑定 Actor Tag，不会把同名兵种当成同一实例；不支持未来波次中的英雄实例或复杂多条件组合。

场上人数满员或空间不足时最多重试 3 秒，随后丢弃剩余数量并记录日志。自定义关卡胜利必须清敌并处理未来定时、已触发延迟及等待落点的波次；已不可能触发的条件不会阻止结束。

| 敌方配置 | 默认值 | 范围 |
| --- | --- | --- |
| `usesCards` | false | 布尔值 |
| `cards` | 骷髅兵、骷髅射手、骷髅法阵 | 已登记 CardId，不重复；启用时至少一张 |
| `silverPerSecond` | 1 | 0～100 |
| `silverCap` | 20 | 1～1,000 |
| `startingSilver` | 5 | 0～上限 |

银币产速为 0 时只消耗初始资金；上限低于费用的卡无法支付。免费波次不扣款、不充值，也不要求牌组包含同名卡。

## 文件格式与边界

格式为 `little-king.battle-layout` v2；包含 `meta`、`field`、`settings`、`objects`、`enemy`、`waves`。上限 2 MiB UTF-8、500 个对象、64 条波次；单个地图群组最多 20 个单位。旧对象的 `spawnAtSeconds` 范围为 0～300 秒，转换后也计入波次上限；显式波次仍允许 0～3,600 秒。

网页模型与 C++ 解析器都拒绝根对象及各层配置中的未知字段、缺失字段和非法类型；字段名及枚举使用规范大小写。标量必须保持声明的 JSON 类型，例如宽度 `"40"`、延时 `true`、标题 `123`、出牌开关 `"false"` 均非法，不能通过引擎的自动类型转换读取。地图、对象、波次的稳定 ID 长度为 1～80，以 ASCII 字母或数字开头，后续只允许字母、数字、下划线、点、连字符，且不能为大小写任意组合的 `None`。ModuleId 使用规范小写名称，CardId 与 UnitId 使用各自目录的规范大小写。`createdAt` / `updatedAt` 必须是有效日历日期的完整 ISO 日期时间，含 `Z` 或 `±hh:mm` 时区，秒的小数可省略或为 1～9 位。

默认战场 40×24 米，网格可选 0.25、0.5、1、2 米。左上角为原点，x 向右、y 向下，对象位置表示中心：

```text
UE.X = (field.height / 2 - y) × 100
UE.Y = (x - field.width / 2) × 100
UE.Z = 0
UE.Yaw = 90 + rotation
```

旋转为屏幕顺时针角度，整个旋转轮廓须在边界内；网页与解析器以原始米坐标和双精度计算，容差为 `1e-7` 米。波次搜索圆可以部分越界，但实际出生仍须合法。`settings.encounterId` 取 Patrol、Elite、Boss，或对应规范别名 Encounter_UndeadPatrol、Encounter_UndeadElite、Encounter_SkeletonKing，提供基础难度快照，不承担远征节点分配。

默认每阵营最多 20 个战斗单位，计入英雄、佣兵、建筑。玩家三英雄和正常出牌的同类建筑两座限制，不等于自定义敌阵的统一数量上限；布局仍检查空间、容量与初始英雄类型唯一性。可启动布局至少有一名开局敌方英雄或首领，英雄对象数量固定为 1，开局类型不重复；延时对象仅支持敌方。

v1 迁移保留原对象与延时，补入默认关闭的敌方经济及空波次，再保存为 v2；v1 文档不能夹带 v2 的 `enemy` / `waves` 字段。迁移不自动修复违反新规则的设计；导入失败不会覆盖原文件或当前布局，损坏草稿保留并提示。

schema 只描述 v2 的结构约束，不能独立保证跨字段关系、大小写不敏感的唯一性、英雄引用、日历有效性或旋转占地，必须搭配网页模型或 C++ 校验。字符串长度上限在网页/C++ 中按 UTF-16 单元计数，JSON Schema 的 `maxLength` 按 Unicode 码点计数。编辑器允许保存缺少开局敌方英雄的草稿并提示，UE 启动则拒绝；实际出生碰撞与单位容量仍由运行时检查。

以上一致性修正属于 v0.8.6 的[优化实施](OptimizationImplementation.md#关卡校验)。本轮 Node 模型测试为 37/37，通过；下方 v0.8.5 发布证据保留当时的 35/35，不计作本轮 UE 验证。

## 实现与素材

| 入口 | 职责 |
| --- | --- |
| `Tools/BattleDesigner/model.js` / `schema.json` | 模块、格式、迁移、几何和跨字段校验 |
| `catalog.js` / `units.js` | CardId / UnitId 目录；注册内容变更时同步 |
| `app.js` / `index.html` / `style.css` | SVG、编辑、撤销、草稿及导入导出 |
| `Source/little_king/LKBattleLayout.*` | JSON 解析、坐标转换与遭遇配置 |
| `ALKBattleGameMode_Layout.cpp` | 部署区、初始生成及独立入口 |
| `ALKBattleTerrain.*` | 几何、连通草丛、视野、占位与表现 |
| `ALKOpponentBrain.*` / `ULKDeckState::InitEnemyDeck` | 事件调度、容量等待和敌方付费出牌 |
| `Tests/LKTerrainTests.cpp` / `LKScenarioWaveTests.cpp` / `LKBattleLayoutTests.cpp` | 地形、波次、导入及胜利判断 |
| `Tools/BattleDesigner/model.test.cjs` | 格式、目录、迁移、坏输入和坐标回归 |

示例：[林间关隘](../Tools/BattleDesigner/examples/forest-pass.json)保留 v1 迁移覆盖；[高地伏击](../Tools/BattleDesigner/examples/terrain-ambush.json)包含四地形、三类波次与并行经济。[草图模板](../Tools/BattleDesigner/paper-template.html)作为工具资源保留。

工坊使用 SVG，UE 使用代码几何及已有材质、字体；未新增第三方下载或生成图片/音频。运行时修改私有副本，未写回共享地图、蓝图、数据资产。

## 发布与验证证据

本轮开发验证完成于 2026-10-08；10-09 发布整理沿用这些结果，没有重复执行全量测试。提交 `f256338e8d95731b0f46cd728d5dd7a48f530792`，由前版 `3c26688` 演进，发布时 main 与注释标签 v0.8.5 已推送并验证一致。

| 检查 | 已记录结果 |
| --- | --- |
| UE 5.8.1 / Win64 Development Editor | 编译成功 |
| 全量 LittleKing 自动化 | 169/169：90 项无警告、79 项带警告；0 失败、0 未运行 |
| 最终 Scenario 回归 | 8/8 |
| Node 模型 / app.js 语法 | 35/35，通过 |
| 浏览器 | 波次编辑/撤销、牌组/经济、非法精度拒绝、JSON 下载回读 |
| UE 实际画面 | 1280×720 部署及开战 18 秒；草丛隐藏、高台驻军、15 秒增援和付费出牌 |
| 存档 | 六个既有 .sav 的 SHA-256 与数量不变 |

完整证据：[发布汇总](validation/v0.8.5-Release.json)、[开发验证](validation/TerrainDesigner-Validation.json)。截图：[工坊](images/BattleDesignerV2.jpg)、[部署](images/TerrainDeployment.png)、[战斗](images/TerrainBattle.png)。`Saved/Logs` 与 `Saved/Automation` 原始日志仅为本机证据。

浏览器交互通过本地 HTTP 检查，`file:` 双击仅做静态依赖检查；草图纸未实测打印。短局截图及自动化不替代长期难度、打包或硬件验收。
