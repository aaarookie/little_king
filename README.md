# 小小国王 (Little King)

**UE5.8 · 2D 自动战斗肉鸽 · 单人开发**

> 你是小小国王，坐镇家园。派出英雄进入随机地牢，用银币雇佣佣兵、指挥法术与建筑，打一场"保英雄"的自动战斗；凯旋后建设家园、强化英雄，向更深的黑暗前进。
> 核心乐趣 = 银币经济的临场决策 × 保护英雄的攻防博弈 × 肉鸽构筑的局局不同

## 🏷 当前版本：v0.1（战斗原型可玩闭环）

**本版本已完成：**

- ✅ **战斗原型可玩闭环**：部署英雄 → 打牌（角色/法术/建筑）→ 自动战斗 → 胜负结算 → 重开
- ✅ **单位系统**：佣兵/英雄/建筑（哨塔·兵营）/远程弹道，自研轻量 FSM（移动·索敌·攻击·死亡）+ 软碰撞
- ✅ **GAS 数据管线**：属性集 + 运行时 GE 伤害管线（SetByCaller，为增伤/减伤乘区铺路）
- ✅ **经济与卡牌**：银币每秒产出 + 牌库/手牌/弃牌堆循环（杀戮尖塔式）
- ✅ **对战规则**：部署阶段冻结、战场左右分（玩家左/敌方右）、**法术门**（需法师英雄在场）、同类建筑 ≤2、**无平局**（超时虚弱机制）
- ✅ **敌方 AI**：脚本波次 + 最便宜打牌规则（受法术门约束）
- ✅ **HUD 交互**：部署面板 / 手牌 4 格 / 银币 / 结算，鼠标放置（左键放置·右键取消）
- ✅ **数据驱动**：`DT_Units` 数据表 + `DA_GameData` 全局配置 + `BP_ALKBattleGameMode`
- ✅ **质量**：9 项 Bug 修复全部记录（含哨塔发呆、射程抖动等 FSM 边界问题）

**技术栈**：UE 5.8 / C++（核心）/ Paper2D / GAS / UMG / DataTable / DataAsset / 蓝图（UI 表现）

## 🚀 运行方法

1. 引擎：UE 5.8（本项目在 `E:\epic\UE_5.8` 开发）
2. 用 VS 打开 `little_king.sln` 编译运行，或直接双击 `little_king.uproject`（首次加载自动编译）
3. PIE 打开 `Content/Maps/L_BattleTest`（相机规范见 docs/03-TaskList.md）
4. 部署阶段点英雄 → 点己方半场放置；开战后点手牌 → 点战场出牌；右键取消

## 📚 文档索引（docs/）

| 文档 | 内容 |
|---|---|
| [01-GDD.md](docs/01-GDD.md) | 游戏设计文档 v0.2（规则/支柱/MVP） |
| [02-BattlePrototypeDesign.md](docs/02-BattlePrototypeDesign.md) | 战斗原型技术设计（架构/模块/数据表） |
| [03-TaskList.md](docs/03-TaskList.md) | 任务清单（Sprint 0~6 + 阶段路线图） |
| [04-HUDTutorial.md](docs/04-HUDTutorial.md) | HUD 搭建教程（UMG 逐步） |
| [05-BugLog.md](docs/05-BugLog.md) | Bug 修复记录（现象/原因/修复/验证） |
| [06-Sprint2Guide.md](docs/06-Sprint2Guide.md) | Sprint 2 指南（卡牌图标/精灵/法术门/波次表） |
| [07-ArtStyleGuide.md](docs/07-ArtStyleGuide.md) | AI 美术风格指南 |
| [08-Sprint3Guide.md](docs/08-Sprint3Guide.md) | Sprint 3 指南（英雄技能 GAS/顶部血条） |
| [09-Sprint4Guide.md](docs/09-Sprint4Guide.md) | Sprint 4 指南（AI 集火/反制/特性系统/平衡） |
| [10-Sprint5Guide.md](docs/10-Sprint5Guide.md) | Sprint 5 指南（打击感/飘字/音效/对象池） |
| [11-Sprint6Guide.md](docs/11-Sprint6Guide.md) | Sprint 6 指南（统计/平衡验收/打包/复盘） |

## 🗺 路线图

- ✅ **Sprint 1~2 完成**（tag v0.1/v0.2）：可玩闭环 + 经济卡牌 + 调试命令 + 精灵链路
- **Sprint 3**（C++ 完成 ✅，联调中）：英雄技能（GAS Ability + 冷却）、英雄血条 UI、胜负手感
- **Sprint 4**（规划）：敌方 AI 升级（集火/反制）、特性系统、内容扩充与首次平衡
- **Sprint 5**（规划）：打击感、飘字、音效接入、HUD 美化、弹道对象池
- **Sprint 6**（规划）：对局统计、3 连测平衡、稳定性走查、Windows 打包（阶段 1 验收）
- **阶段 2~6**：地牢局内循环 → 家园局外循环 → 内容扩充 → 美术音乐替换 → 发布

## ⚠️ 已知事项

- 音乐/部分音效待接入（MiniMax Music 3 + 免费素材库），占位期以调试反馈为主
- 项目暂未启用 Git LFS（当前资产体积小；资产变大后建议启用）
- 单机 PvE 架构（无联机），PvP 为远期话题
