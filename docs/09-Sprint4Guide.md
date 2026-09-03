# Sprint 4 指南：敌方 AI 升级与内容平衡

**状态**：规划文档（执行时细化，涉及接口以实现为准）。
**前置**：Sprint 3 完成（英雄技能 + 血条可用）。
**上一篇**：[08-Sprint3Guide.md](08-Sprint3Guide.md)

---

## 一、目标与分工总览

| # | 内容 | 谁做 | 难度 |
|---|---|---|---|
| 1 | **AI 战术升级**：集火英雄、反制玩家兵种、爆发时机、法术智能目标 | C++ 为主（我） | ★★★ |
| 2 | **特性系统**：`DT_Traits` + 光环/嘲讽（英雄特性真正生效） | C++ 运行时 + **你建表** | ★★★ |
| 3 | **内容与首次平衡**：3 英雄/6 佣兵/2 法术/2 建筑数值整理 + 单位数量上限 | 你填表调数 + 我加上限字段 | ★★ |
| 4 | 收尾：BugLog + 复盘 + Git tag v0.4 | 共同 | ★ |

> 不做：新单位种类扩编（阶段 2 地牢再做）、AI 难度分档（暂缓）、联机。

---

## 二、我（C++）实现内容

### 1. AI 战术升级（`ALKOpponentBrain` + `ALKBattleGameMode` + `ALKUnitBase`）

**① 集火英雄指令（TargetOverride）**
```
ALKUnitBase 新增：
  void SetForcedTarget(AActor*, float Duration);   // 优先级最高的目标
  AcquireTarget() 逻辑改为：有 ForcedTarget 且存活 → 用它；到期自动清除
GameMode/敌方脑：
  每 25~35s（可配）对玩家"血量最低的英雄"发起 8s 集火：
  ForcedTargetAllEnemies(目标英雄) → 到期清除
```
玩家对策空间：集火期用建筑/佣兵挡路、治疗波抬血——这就是"保英雄"博弈的节奏点。

**② 反制玩家兵种（牌序偏好）**
```
Brain 每 5s 评估一次玩家场上构成（GameMode 提供：
  CountUnitsOfClass(Team, 兵种类别)、CountUnits(Team)）
→ 生成"偏好分"：玩家远程多 → 我方偏好盾卫/剑士冲锋；
  玩家近战多 → 偏好弓箭手/箭塔。
ThinkAndPlay 从"最便宜优先"改为"得分 = 费用权重 + 反制偏好"选牌。
```

**③ 爆发时机（push）**
```
银币 ≥ 8 且 我方可部署单位优势（或波次节点）→ 连续打出 2~3 张牌（一波流），
打破"永远均匀出牌"的机器人感。
```

**④ 法术智能目标**
```
GameMode 提供 FindBestSpellTarget(施法阵营, 半径)：
遍历敌方每个单位，统计其半径内敌人数，取聚集度最高点 → AI 火球不再乱扔。
```

### 2. 特性系统（DT_Traits 真正生效）

```
运行时（生成单位后按 HeroTraits 查 TraitTable）：
① Self 修饰符（AuraRadius=0）：ApplyAttributeModifier(属性, 基础值 × Value, 无限)
② 光环（AuraRadius>0）：新组件 ULKTraitAuraComponent——每 0.5s
   给范围内友军施加无限时长 GE（记录 handle），离开范围移除
③ 特殊 TraitId="Taunt"：标记为嘲讽单位 → 敌方索敌优先攻击嘲讽者
   （给坦克类佣兵用，保护后排的机制起点）
```
> FLKTraitRow / FLKTraitModifier 结构已在 `LKDataTypes.h` 就绪，本轮补运行时。

### 3. 内容上限与辅助

- `ULKGameData::MaxUnitsPerTeam`（默认 20）：超限时打牌返回失败并提示（防后期单位海卡顿）
- 平衡期辅助：`ListUnits` 已够用，视需要加 `ClearField 阵营`（清场重测）

---

## 三、你要做的（教程）

### 教程 A：建 DT_Traits 特性表

**A1. 创建表**
```
内容浏览器 → 添加/导入 → 杂项 → 数据表 → 行结构搜：FLKTraitRow → 命名 DT_Traits
```

**A2. 填 3 个示例特性**（行名 = 特性 ID，英雄行 HeroTraits 里填的就是它）

| 行名 | TraitName | Modifiers（数组，点 + 加元素） |
|---|---|---|
| Trait_KnightAura | 骑士光环 | StatName=`AttackDamage`，Value=`0.15`，AuraRadius=`400`（范围友军 +15% 攻击） |
| Trait_MageMight | 法师威能 | StatName=`AttackRange`，Value=`0.10`，AuraRadius=`0`（自身射程 +10%） |
| Trait_Taunt | 嘲讽 | StatName=`Health`，Value=`0`，AuraRadius=`0`（**TraitId 填 Taunt 才触发嘲讽逻辑**，建议单独建行名 `Taunt`） |

> 嵌套数组编辑：数据表里点开 Modifiers 行 → 点 + 号添加元素 → 展开元素填 StatName/Value/AuraRadius。StatName 必须是：`Health / MaxHealth / MoveSpeed / AttackRange / AttackDamage / AttackInterval`（与代码属性同名）。

**A3. 接线**
```
打开 DA_GameData → Data → TraitTable = DT_Traits
打开 DT_Units → 对应行的 HeroTraits 数组填特性行名
  例：Hero_Knight → HeroTraits = [Trait_KnightAura]
      Unit_Shieldbearer → HeroTraits = [Taunt]
```

### 教程 B：AI 手感验证

测试剧本（PIE + `~` 命令）：
1. **集火**：只派 1 个残血英雄 → 观察敌方是否在集火窗口集体转向它（索敌线会帮你看见）
2. **反制**：连续打 5 张弓箭手 → 下一波敌方是否变多盾卫（日志看波次/出牌）
3. **爆发**：用 `AddSilver 50` 观察 AI 是否攒够一波连续出牌
4. 全程 `ListUnits` 辅助观察

### 教程 C：首次平衡（数值整理）

1. 打开 `DT_Units` + 7 张卡资产，逐行核对"费用/属性是否符合设计意图"（对照表见 02 技术设计）
2. **平衡杠杆速查**（每次只改一个变量）：
   | 想解决 | 改哪里 |
   |---|---|
   | 佣兵太脆 | BaseHealth ↑ 或费用 ↓ |
   | 弓箭手太强 | AttackInterval ↑（攻速变慢）或 AttackDamage ↓ |
   | 法术一炸清场 | SpellValue ↓ / SpellRadius ↓ / Cost ↑ |
   | 对局太长 | BattleTimeLimit ↓（但不低于 240，虚弱机制才是收尾） |
3. 记录格式建议（每局一行，写进 BugLog 或单独 `BalanceNotes.md`）：
   `日期 | 谁赢 | 时长 | 我方击杀 | 敌方击杀 | 观感(快/慢/胶着) | 本局改了什么`

### 教程 D：验收清单

- [ ] 敌方会周期性集火玩家最低血量英雄，且集火会结束
- [ ] 玩家兵种构成会影响 AI 出牌（反制生效）
- [ ] AI 有"攒一波"的爆发行为
- [ ] 骑士光环/法师威能/盾卫嘲讽实际生效（开战前后属性对比可见）
- [ ] 单位总数达到 20/方后无法继续出牌并有提示
- [ ] 平衡调整 3 轮，每轮有记录
- [ ] BugLog 更新 + Git 提交 tag v0.4

---

## 四、风险与坑

| 坑 | 说明 |
|---|---|
| 光环 GE 移除 | 离圈者要按 handle 移除 GE，否则 buff 永久生效——实现时重点测"进出圈" |
| 嘲讽与集火叠加 | 索敌优先级顺序定为：ForcedTarget > 嘲讽者 > 最近敌人，测试三者并存 |
| 数据表嵌套数组 | Modifiers 是数组里套结构，行编辑器里要点开"三角"逐层展开 |
| 平衡改太多 | 一次只动一个变量，用记录表对照，否则不知道是谁改好的 |

---

## 五、下轮顺序（执行时细化）

特性运行时 → AI 升级（集火→反制→爆发→法术目标）→ 上限字段 → 编译 → 你按教程 A~D 联调。
