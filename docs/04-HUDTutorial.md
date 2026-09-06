# 战斗 HUD 搭建教程（Sprint 1）

> **Sprint 5 修订提示（2026-09-06）**：本文保留历史教程/素材参考。当前规则以 [01-GDD](01-GDD.md) 为准，先执行 [16 迁移教程](16-Sprint5MigrationTutorial.md)。旧倒计时开战、法术门/灰卡、团队血条、死亡震动、集火高于嘲讽的说明已失效；当前相机仍按正俯视验证，斜视为未验收方案。资产清单中的“完成”不代表本轮资产接通或运行验收完成。

**前置**：C++ 地基已编译（`ALKPlayerController` 放置状态机 + `ULKBattleHUDWidget` 事件基类 + GameMode 自动创建 HUD）。

**原理**：C++ 负责逻辑并广播事件 → Widget 蓝图（继承 `ULKBattleHUDWidget`）只需"覆写"事件刷新界面。

---

## 第 1 步：创建 WBP_BattleHUD（Widget 蓝图）

1. 内容浏览器 → **添加/导入 → 用户界面（User Interface）→ Widget 蓝图（Widget Blueprint）**
2. 弹出"选择父类"对话框 → 搜索框输入：**LKBattleHUDWidget** → 选中 → 创建
3. 重命名：`WBP_BattleHUD`

## 第 2 步：搭界面结构（Designer 面板）

右侧"层级"面板从零搭（删掉默认的 Canvas Panel 再重建也行），层级如下：

```
WBP_BattleHUD（根：画布面板 Canvas Panel）
├── DeploymentPanel（垂直框 VerticalBox，锚点居中，标题"部署英雄"）
│   ├── Title（文本："部署英雄"）
│   ├── Btn_Knight（按钮，文字"骑士"）
│   ├── Btn_Mage（按钮，文字"法师"）
│   ├── Btn_Ranger（按钮，文字"游侠"）
│   └── Btn_Start（按钮，文字"开始战斗"）
├── HUDMain（水平框 HorizontalBox，锚点底部居中）
│   ├── SilverText（文本："银币 0/12"）
│   ├── CardSlot_0（按钮 + 子文本"费用"）
│   ├── CardSlot_1（按钮 + 子文本）
│   ├── CardSlot_2（按钮 + 子文本）
│   └── CardSlot_3（按钮 + 子文本）
├── ResultOverlay（画布，全屏，默认"折叠"Collapsed）
│   ├── ResultText（文本，居中："胜利！"）
│   └── Btn_Restart（按钮："再来一局"）
└── TipText（文本，顶部居中，默认"折叠"）—— 出牌提示（"银币不足"等）
```

**关键设置**：
- 根 Canvas Panel 的**可见性（Visibility）选 "Not Hit-Testable (Self Only)"** ← 必须！这样空白区域点击能穿透到战场（否则 UI 挡住鼠标，无法放置单位）；子按钮仍然可点。注意：**5.8 中文版未翻译此枚举**，下拉框显示英文原文，别找中文名（同组的 "Not Hit-Testable (Self & All Children)" 会让按钮也点不了，别选错）
- 每个卡槽按钮记得加一个子 Text 显示费用（或直接按钮文字 = 费用）

## 第 3 步：按钮接线（Event Graph 事件图表）

### 蓝图操作入门（先看这个，5 分钟学会）

**创建节点的两种方式**（本质相同，都是"搜索框"）：
- 从已有节点的**白色执行引脚**（右侧的小三角箭头）按住左键拖出 → 松手 → 弹出搜索框 → 输入函数名 → 点结果
- 在图表**空白处右键** → 同样弹出搜索框 → 输入函数名

**函数节点长什么样**：函数节点左侧有白色**执行引脚**（接事件流）和一个**"目标（Target）"引脚**（这个函数属于哪个对象）——你调用的 `BeginHeroPlacement` 是控制器的方法，所以必须把控制器引用连到 Target 上。

**参数怎么填（字面量技巧）**：从参数引脚（如 HeroUnitId）拖出 → 搜索框输入带引号的字符串如 `"Hero_Knight"` → 回车，自动生成常量并连上。

### 逐个接线（对照表）

| 按钮 | 连线 |
|---|---|
| Btn_Knight | **GetLKPlayerController**（搜索 GetLK 即可）→ 返回值连到 **BeginHeroPlacement 的 Target**；HeroUnitId 引脚填 `"Hero_Knight"`；事件执行线 → BeginHeroPlacement |
| Btn_Mage | 同上，HeroUnitId 填 `"Hero_Mage"` |
| Btn_Ranger | 同上，HeroUnitId 填 `"Hero_Ranger"` |
| Btn_Start | GetLKPlayerController → **RequestStartBattle 的 Target**；事件执行线 → RequestStartBattle（无参数） |
| CardSlot_0~3 | GetLKPlayerController → **BeginCardPlacement 的 Target**；HandIndex 引脚填数字 `0`（搜 "0" 出整数字面量），四个卡槽分别 0/1/2/3 |
| Btn_Restart | **打开关卡（Open Level）**（引擎节点，搜 Open Level 或中文"打开关卡"）；Level Name 填 `"L_BattleTest"`（**没有 Target 引脚**，它是全局函数） |

> 不需要 Cast：`GetLKPlayerController` 已经返回正确的控制器类型。
> 搜索技巧：C++ 函数在蓝图搜索框里输**函数名的一部分**就能命中，如 "HeroPlacement"、"CardPlacement"、"LKPlayer"。

## 第 4 步：实现 C++ 事件刷新界面（核心）

> **注意（5.8）**：UE 5.8 **已删除蓝图工具栏的"覆写（Override）"下拉按钮**。替代入口有两个，任选其一：
> - **方式一（推荐）**：事件图表标签页 → 左侧**"我的蓝图（My Blueprint）"面板 → "函数（Functions）"分类** → 找到父类 C++ 的可覆写函数（如 OnPhaseChanged，带可覆写图标）→ **右键 → "实现事件（Implement Event）"** → 图表中出现事件节点
> - **方式二**：在事件图表**空白处右键** → 搜索框输入事件名（如 `OnPhaseChanged`）→ 点搜索结果 → 创建事件节点

**逐个实现以下事件**（每实现一个，从它的执行引脚拖出开始连线）：

**① OnPhaseChanged（NewPhase: ELKGamePhase）** — 控制面板显隐
```
分支（Branch）判断 NewPhase：
  == 部署(Deployment) → DeploymentPanel 可见；HUDMain 折叠；ResultOverlay 折叠
  == 战斗(Battle)     → DeploymentPanel 折叠；HUDMain 可见
  == 结算(Result)     → 全部折叠
```
（用"设置可见性 Set Visibility"节点，可见性值选"可见 Visible"或"折叠 Collapsed"）

**② OnHandChanged（Hand: 卡名数组，Costs: 费用数组）** — 刷新 4 个卡槽
```
CardSlot_0 子文本 → 设置文本 Set Text = Costs[0] 转成文本（"Get (ref)"取数组元素）
CardSlot_1 → Costs[1] ... CardSlot_3 → Costs[3]
```
（原型期只显示费用即可；显示卡名可加：`Hand[0]` 转字符串）

**③ OnSilverChanged（Silver, Cap, Delta）** — 刷新银币文本
```
SilverText → 设置文本 = Silver 四舍五入转文本 + " / " + Cap 转文本
```
（用"追加 Append"或"格式化文本 Format Text"节点）

**④ OnMatchEnded（Winner: ELKTeam）** — 显示胜负
```
分支：Winner == 玩家(Player) → ResultText 文本 = "胜利！"；否则 = "失败…"
ResultOverlay → 设置可见性 = 可见
```

**⑤ OnPlacementStateChanged（bPlacing, Mode, HandIndex, ItemId）** — 可选：选中的卡槽高亮
```
原型期可留空不实现（不点选它即可）
```

**⑥ OnPlayResult（Result: ELKPlayResult）** — 出牌失败提示
```
分支：Result == 银币不足 → TipText 文本 = "银币不足"
      == 位置无效 → "不能放在这里"
      == 法术未解锁 → "需要法师英雄在场"
TipText → 可见；再接一个 Delay 1.5 秒 → 折叠
```

> 第 5、6 步可选，先做 ①~④ 就能玩。

## 第 5 步：让游戏创建它

1. 打开 `BP_ALKBattleGameMode` → 左上角**"类默认值"（Class Defaults）** → 细节面板搜索 **HUDWidgetClass** → 下拉选择 **WBP_BattleHUD**
2. Ctrl+S 保存

## 第 6 步：测试清单

- [ ] PIE 后：部署面板出现（标题 + 3 个英雄按钮 + 开始按钮）
- [ ] 点"骑士"→ 鼠标变放置状态 → 左键点己方半场 → 绿块英雄出现（再点一次同英雄会提示已部署）
- [ ] 点"开始战斗"（或等 20 秒倒计时）→ 部署面板消失，底部出现银币 + 手牌
- [ ] 银币数字每秒增长；点手牌卡 → 左键点己方半场 → 佣兵出现，银币减少
- [ ] 右键取消放置；银币不足时点卡 → 顶部提示"银币不足"
- [ ] 一方英雄全灭 → 结算界面出现"胜利！/失败…"，点"再来一局"重开
- [ ] 日志无报错

## 常见问题

| 现象 | 原因/处理 |
|---|---|
| 点战场没反应（放置不了） | 根 Canvas 的可见性没设成 "Not Hit-Testable (Self Only)"；或按钮事件没接上 |
| HUD 没出现 | 检查 BP_ALKBattleGameMode 的 HUDWidgetClass 是否已设置并保存 |
| 事件覆写找不到 | 确认 WBP_BattleHUD 的父类是 LKBattleHUDWidget（类默认值面板顶部可见） |
| 卡槽没文字 | OnHandChanged 没实现，或数组元素节点连错（用 Costs 不是 Hand） |
