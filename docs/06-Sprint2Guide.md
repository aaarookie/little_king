# Sprint 2 指南：经济与卡牌完整化

**本文档结构**：① 本次 C++ 已完成的工作 → ② 你需要做的 4 个教程（A 卡牌图标 / B 单位精灵 / C 法术门锁定 UI / D 敌方波次表）→ ③ 测试清单

---

## ① 本次 C++ 已完成（已编译通过）

### 1. 调试命令（CheatManager）——PIE 里按 `~` 打开控制台输入

| 命令 | 示例 | 说明 |
|---|---|---|
| `AddSilver` | `AddSilver 10` | 给玩家加银币（调试经济节奏必备） |
| `DrawCard` | `DrawCard` | 玩家抽一张牌 |
| `SpawnUnit` | `SpawnUnit Unit_Swordsman 1 0 300` | 生成单位：ID + 阵营(0玩家/1敌方) + X Y。**半场约定：玩家 Y<0（左）、敌方 Y>0（右）**，坐标写反会自动镜像到本阵营半场并提示 |
| `KillAll` | `KillAll 1` | 处决某阵营全部单位（0玩家/1敌方） |
| `WinMatch` | `WinMatch 0` | 直接结束对局并指定胜者 |
| `StartBattle` | `StartBattle` | 跳过部署直接开战 |
| `ListUnits` | `ListUnits` | 列出场上所有单位（ID/阵营/生命/状态/坐标） |

技术要点（面试可讲）：`ULKCheatManager : UCheatManager` + `UFUNCTION(Exec)`；**UE5.8 中 `CheatClass` 属性在 `APlayerController` 上（旧版在 GameMode）**，已在 `ALKPlayerController` 构造函数里指定。

### 2. 法术门锁定支持（HUD 侧数据已就绪）

- `GameMode` 新增事件 **`OnSpellLockChanged`**：开战瞬间和**法师英雄阵亡**时广播（玩家法师死后，法术卡自动变不可用）
- HUD 的 **`OnHandChanged` 签名升级为 3 个参数**：`Hand`（卡ID）、`Costs`（费用）、**`bPlayable`（能否打出）**——法术卡在"无法师在场"时自动标记为不可打出
- 新增查询函数 **`GetCardDefinition(卡ID)`**（HUD 基类，BlueprintPure）——按卡 ID 拿到卡牌资产（图标/名称），显示卡面用

### 3. 其他

- `GameMode::ForceEndMatch(胜者)`（蓝图可调，WinMatch 命令的底层）
- `GameMode::FindCard` 变为蓝图可调（BlueprintPure）

### ⚠️ 重要：WBP_BattleHUD 需要重连一处

`OnHandChanged` 现在有 **3 个参数**（多了 `bPlayable` 数组）。打开 WBP_BattleHUD → 找到 OnHandChanged 事件 → **删除旧事件节点，重新添加**（覆写下拉里点它）→ 重连卡槽（新参数 `bPlayable[0]~[3]` 正好用于教程 C 的变灰）。

---

## ② 教程 A：卡牌图标（你正在做的部分）

### A1. AI 生成图标（建议用 ChatGPT image）

每张卡一个图标，**统一提示词模板**保证风格一致。推荐模板：

```
2D mobile game card icon, [主题], [背景色] background, thick dark outline,
flat cartoon style, centered, square composition, no text
```

对照表（背景色区分类型：角色=蓝、法术=紫、建筑=棕）：

| CardId | 主题 | 提示词关键词 |
|---|---|---|
| Unit_Swordsman | 剑士 | knight with sword |
| Unit_Archer | 弓箭手 | archer with bow |
| Unit_Shieldbearer | 盾卫 | shield warrior |
| Spell_Fireball | 火球术 | fireball explosion |
| Spell_HealWave | 治疗波 | green healing wave |
| Building_ArrowTower | 箭塔 | arrow tower |
| Building_Barracks | 兵营 | barracks building |

### A2. 导入引擎

1. 内容浏览器 → 新建文件夹 `Content/Icons`
2. 把 7 张 PNG 拖进 `Content/Icons`（自动导入为 Texture2D）
3. 双击任意一张检查：勾选 **sRGB 关闭**（图标不需要色彩校正，可选）、压缩设置默认即可

### A3. 创建 7 张卡牌资产（ULKCardDefinition）

每张卡一个数据资产：
```
添加/导入 → 杂项 → 数据资产 → 类型搜索 ULKCardDefinition
→ 命名：C_Swordsman / C_Archer / C_Shieldbearer / C_Fireball / C_HealWave / C_ArrowTower / C_Barracks
```
每个资产填写（双击打开 → 细节面板）：
- **CardId**：必须与上表一致（`Unit_Swordsman` 等，**行名/ID 严格等于代码里的名字**）
- **CardName / Description**：卡名/描述
- **Cost**：剑士2 / 弓箭手3 / 盾卫3 / 火球4 / 治疗波3 / 箭塔5 / 兵营4
- **CardType**：角色卡选"角色"，法术选"法术"，建筑选"建筑"
- **SpawnUnitId**（角色卡）：`Unit_Swordsman` 等；**BuildingUnitId**（建筑卡）：`Building_ArrowTower` 等
- **SpellEffect / SpellValue / SpellRadius**（法术卡）：火球=范围伤害 60 / 半径 250；治疗波=范围治疗 40 / 半径 300
- **Icon**：选对应导入的图标贴图

> ⚠️ **要么 7 张全建，要么全不建**：代码只在 CardLibrary 为空时自动生成默认卡。只建一部分会导致缺失的卡打不出来（日志报"不在 CardLibrary 中"）。

### A4. 填进 DA_GameData

1. 打开 `DA_GameData` → **Cards（卡牌）分类 → CardLibrary**
2. 点 + 号添加 7 个元素，每个选一张卡资产（C_Swordsman 等）

### A5. HUD 卡槽显示图标（WBP_BattleHUD 蓝图）

> ✅ **2025-09 简化**：C++ 新增 `GetCardIcon(卡ID)` 直接返回已加载的图片，不再需要蓝图里的软引用加载（Load Soft Object 在 5.8 右键菜单不稳定，已弃用）。卡槽结构：`Button → Overlay → [CardImg(Image 铺满, 自命中测试不可见) + 费用 Text(右下角)]`，CardImg 勾选"是变量"。

在 `OnHandChanged` 事件里给每个卡槽设图标（**每卡槽 3 个节点**）：
```
① 取卡ID：从 Hand 引脚拖出 → "Get（复制）" → 索引 0 → Hand[0]
② 取图标：空白处右键搜 GetCardIcon → CardId ← 接 Hand[0] → 输出 Texture2D
③ 设画刷：空白处右键搜"设置画刷"(Set Brush From Texture)
   → Target ← CardImg_0 引用（搜变量名）
   → Texture ← 接 ② 的输出
其余 3 个卡槽同理：索引改 1/2/3，Target 改 CardImg_1/2/3；费用文本照旧用 Costs 数组
```

---

## ② 教程 B：单位精灵（角色外观）

> 注意：ChatGPT image **无法生成真正的透明 PNG**；UE 也没有内置一键抠图（"从图集提取"是切图集用的，不是去背景）。正确流程 = 外部抠图出透明 PNG → 导入 → 创建 Sprite。

1. AI 生成角色图：提示词要求**纯白背景 + 深色描边**（如 `plain solid white background, game sprite, thick dark outline`）——抠图干净、边缘少白边
2. 去背景（三选一）：**Windows 11 画图"移除背景"按钮**（免费内置）／ remove.bg 在线抠图 ／ Krita·GIMP 魔棒工具 → 导出透明 PNG
3. 导入 `Content/Sprites/`（透明 PNG 直接拖入即可）
4. 右键贴图 → **Sprite 操作（Sprite Actions）→ 创建 Paper Sprite（Create Sprite）**——透明区域自动镂空（精灵编辑器里应是灰白棋盘格背景）
5. 打开 `DT_Units` → 对应行 → **Sprite 字段**选刚创建的 Paper Sprite；**SpriteScale** 按需缩放（默认 1,1）
6. 填了精灵的单位不再显示色块（调试框自动隐藏）；没填的继续显示色块——**可以分批替换**

---

## ② 教程 C：法术门锁定 UI（卡槽变灰）

**目标**：场上没有法师英雄时，法术卡槽变半透明（看起来"锁住"），法师在场/阵亡自动切换。

**原理**：C++ 在 `OnHandChanged(Hand, Costs, bPlayable)` 里已经算好了每张卡能否打出（`bPlayable[i]=false` = 法术门锁定/无法打出）。蓝图只需**照抄 bPlayable 到卡槽透明度**。

### 前提（重要）

`OnHandChanged` 必须是 **3 参数版**：打开 WBP_BattleHUD → 事件图表 → 若旧事件节点还在（2 参数）→ 删除 → 工具栏"覆写"下拉重新添加 OnHandChanged。

### 逐步操作（以 CardSlot_0 为例，中文节点名对照）

**① 取出 bPlayable[0]**
```
从 OnHandChanged 事件的 bPlayable 引脚（数组）拖出
→ 松开后搜索 "Get" → 选"Get（复制）"
→ 节点出现"索引"引脚 → 填 0
→ 输出：bPlayable[0]（布尔值）
```

**② 生成分支（判断真假）**
```
从 bPlayable[0] 的布尔输出引脚【直接拖出】
→ 右键菜单会出现"分支"（Branch）——布尔引脚拖出会自动推荐它，点选
→ 节点有两个执行出口：True / False
```
> 小技巧：从布尔引脚拖出直接选 Branch，是最快的生成方式，不用先放节点再接。

**③ 设置透明度（True/False 两条路）**
```
在 True 出口后面接：右键搜"设置渲染不透明度"（Set Render Opacity）
  → Target（目标）← CardSlot_0（搜变量名，需按钮已勾选"是变量"；没勾选就先去层级面板勾上）
  → 不透明度（In Render Opacity）填 1.0     ← 正常不透明
在 False 出口后面接：同样的节点
  → 不透明度填 0.3                            ← 半透明（变灰效果）
```
> ⚠️ 透明度范围是 0（全透明）~ 1（不透明）。**1.0 是正常态，不是 0**——别填反了。
> 目标选**按钮本体**（CardSlot_0 而不是里面的 Image）——按钮透明度会连带子内容一起变灰，一步到位。

**④ 补上白色执行线**
```
True → 设置渲染不透明度(1.0)；False → 设置渲染不透明度(0.3)
两路的白色执行线分别连好，不能漏
```

**⑤ 复制到另外 3 个卡槽**
```
对 CardSlot_1~3 重复 ①~④，每遍改两处：
  · "Get（复制）"的索引：1 / 2 / 3
  · 设置渲染不透明度的 Target：CardSlot_1 / 2 / 3
偷懒法：框选 CardSlot_0 的三个节点 → Ctrl+C → 空白处右键粘贴 → 改索引和 Target
```

### 完整节点预览（CardSlot_0）

```
OnHandChanged (Hand, Costs, bPlayable)
   │
   ├─ bPlayable → Get(0) ──→ Branch ──True──→ Set Render Opacity (CardSlot_0, 1.0)
   │                                  └─False─→ Set Render Opacity (CardSlot_0, 0.3)
   └─ Hand → Get(0) → GetCardIcon → Set Brush From Texture (CardImg_0)   ← 图标（教程A5）
   └─ Costs → Get(0) → Set Text (费用文字)                                ← 费用（已有）
```

### 测试

1. PIE 开战、**不部署法师** → 火球卡槽半透明；点它也打不出（顶部提示"需要法师英雄在场"——C++ 已拦截）
2. 部署法师 → 火球槽恢复不透明
3. 法师阵亡 → 火球槽**自动**变灰（C++ 的 `OnSpellLockChanged` 事件触发重推，无需你写任何刷新逻辑）

### 可选进阶（做完基础版再考虑）

- **锁定时禁点**：False 分支里顺便接"设置是否可用"（Set Is Enabled）→ false，卡槽变灰且不可点击（注意：禁用后鼠标悬停效果也消失）
- **效果提示**：False 时把卡槽文字换成"需法师"或加个锁图标（静态 Image 子控件切可见性）

---

## ② 教程 D：敌方波次表（DT_Waves）

1. 创建数据表：添加/导入 → 杂项 → 数据表 → 行结构 **FLKWaveRow** → 命名 `DT_Waves`
2. 添加以下行（**行名随意**，字段生效）：

| Time | UnitId | Count |
|---|---|---|
| 5 | Unit_Swordsman | 1 |
| 25 | Unit_Swordsman | 2 |
| 50 | Unit_Archer | 1 |
| 80 | Unit_Shieldbearer | 1 |
| 120 | Unit_Swordsman | 2 |
| 160 | Unit_Archer | 2 |
| 210 | Unit_Shieldbearer | 2 |
| 280 | Building_ArrowTower | 1 |
| 360 | Unit_Swordsman | 3 |

3. 打开 `DA_GameData` → **Data → WaveTable** → 选 `DT_Waves`
4. 没有该表时游戏用代码内置的同款波次（两者行为一致，填表是为了可调）

---

## ③ 测试清单（全部完成后）

- [ ] `~` 控制台 `AddSilver 50` → 银币立刻增加
- [ ] `SpawnUnit Unit_Swordsman 1 0 -300` → 敌方半场出现剑士
- [ ] `KillAll 1` → 敌方单位全灭（英雄阵亡触发结算）
- [ ] `WinMatch 0` → 直接胜利画面
- [ ] `ListUnits` → 日志列出场上单位
- [ ] 手牌显示图标 + 费用（不再是纯文字）
- [ ] 不部署法师时火球卡变灰不可打；部署法师后恢复；法师阵亡再变灰
- [ ] 敌方按 DT_Waves 波次出兵（日志"波次触发"时间与表一致）

## 收尾

完成后提交 Git（`git add -A && git commit -m "Sprint 2: ..." && git push`，代理配置已写好），然后进入 **Sprint 3：英雄技能（GAS Ability）**。
