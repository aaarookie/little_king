# Sprint 4 指南：敌方 AI 升级与内容平衡

**状态**：规划完成；**C++ 部分已实现 ✅（本轮）**，剩余你按教程 A~D 联调（建 DT_Traits → AI 验证 → 首次平衡）。
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

## 二、我（C++）实现内容（✅ 本轮全部完成）

> 实现后校对：AI 参数全部集中在 `DA_GameData → AI` 分类（集火间隔/时长、反制评估间隔、爆发门槛/张数/冷却），上限在 `DA_GameData → Field → MaxUnitsPerTeam`（默认 20，-1=不限）。日志前缀 `[Brain]` / `[Trait]` / `[Battle] 集火指令`。

### 1. AI 战术升级（✅ `ALKOpponentBrain` + `ALKBattleGameMode` + `ALKUnitBase`）

**① 集火英雄指令（SetForcedTarget）✅**
```
ALKUnitBase::SetForcedTarget(AActor*, float Duration)   // 优先级最高的目标，到期自动清除
索敌优先级（AcquireTarget）：ForcedTarget > 嘲讽者 > 最近敌人
GameMode::ForcedTargetAllUnits(阵营, 目标, 秒)          // 全体（建筑除外）转火
敌方脑：每 AIFocusIntervalMin~Max 秒（默认 25~35s）对玩家血量最低英雄
        （GetWeakestAliveHero）发起 AIFocusDuration 秒（默认 8s）集火
调试：集火窗口内敌方单位画【青色】集火线（bDrawDebugShapes 开启时）
```
玩家对策空间：集火期用建筑/佣兵挡路、治疗波抬血——这就是"保英雄"博弈的节奏点。

**② 反制玩家兵种（牌序偏好）✅**
```
敌方脑每 AICounterCheckInterval 秒（默认 5s）评估一次玩家场上构成：
  GameMode::CountCombatUnitsOfAttackType(阵营, 近战/远程)   // 排除建筑
选牌改为"得分 = 费用权重（便宜优先）+ 反制偏好分"：
  玩家远程 ≥ 3 → 偏好盾卫/剑士（贴脸冲锋克制远程）
  玩家近战 ≥ 3 → 偏好弓箭手/箭塔（放风筝克制近战）
日志每轮评估输出：[Brain] 反制评估：玩家 近战 xN / 远程 xM
```

**③ 爆发时机（push）✅**
```
银币 ≥ AIPushSilverThreshold（默认 8）且己方存活单位数 ≥ 玩家
→ 一波连打 AIPushMaxCards 张（默认 3），随后进入 AIPushCooldownMin~Max 秒冷却
日志：[Brain] 爆发！银币 xx 兵力占优，一波连打 N 张
```

**④ 法术智能目标 ✅**
```
GameMode::FindBestSpellTarget(施法阵营, 半径, OutLocation)
→ 遍历敌方每个存活单位，统计其半径内敌人数（聚集度），取最高点
AI 火球不再乱扔；找不到聚集点时回退"随机玩家英雄 ±100"。
日志：[Battle] 法术目标：xxx 处聚集 N 个单位
```

### 2. 特性系统（✅ DT_Traits 运行时生效）

```
单位生成后按 HeroTraits 查 TraitTable（DA_GameData → Data → TraitTable）：
① 自身修饰（AuraRadius=0）：无限时长 GE，数值 = 该属性【基础值】× Value
   （多个加成线性叠加、不滚雪球）；日志 [Trait] xxx 自身特性 …
② 光环（AuraRadius>0）：新组件 ULKTraitAuraComponent（挂在每个单位上，有光环才启用）
   ——每 0.5s 给范围内友军施加无限 GE（记录 handle），离开范围/死亡按 handle 移除
   日志：[Trait] 光环：半径 N 内友军 … / 光环生效 / 光环移除（进出圈都可见）
③ 嘲讽：行 TraitId 或行名 = "Taunt" → 标记 bTaunting
   ——敌方索敌优先级高于最近敌人；ListUnits 显示"嘲讽中"
```
> 数值规则：Value 是百分比（0.15 = +15% 基础值）；StatName 必须精确匹配
> `Health / MaxHealth / MoveSpeed / AttackRange / AttackDamage / AttackInterval`，拼错会打 Warning。

### 3. 内容上限与辅助（✅）

- `ULKGameData::MaxUnitsPerTeam`（默认 20，Field 分类）：出牌时超限返回 `UnitLimitReached`
  （打牌失败提示）；战斗期生成（波次/兵营）同样拒绝并打 Warning——部署期英雄不受限
- 打牌新增失败码 `UnitLimitReached`（HUD 的 OnPlayResult 可直接提示）
- `ListUnits` 升级：显示 英雄/嘲讽中/无敌中 标记

---

## 三、你要做的（教程）

### 教程 A：建 DT_Traits 特性表

**A1. 创建表**
```
内容浏览器 → 添加/导入 → 杂项 → 数据表 → 行结构搜：FLKTraitRow → 命名 DT_Traits
```

**A2. 填 3 个示例特性**（行名 = 特性 ID；**DT_Units 的 HeroTraits 数组里填的就是行名**）

| 行名 | TraitName | Modifiers（数组，点 + 加元素） |
|---|---|---|
| Trait_KnightAura | 骑士光环 | StatName=`AttackDamage`，Value=`0.15`，AuraRadius=`400`（范围友军 +15% 攻击） |
| Trait_MageMight | 法师威能 | StatName=`AttackRange`，Value=`0.10`，AuraRadius=`0`（自身射程 +10%） |
| **Taunt** | 嘲讽 | **Modifiers 留空即可**（嘲讽是纯标记，不需要属性修饰；行名就叫 `Taunt`，TraitId 可留空） |

> ⚠️ **引用规则（最容易踩的坑）**：DT_Units → HeroTraits 数组填的是 DT_Traits 的【**行名**】。
> 例：盾卫行 HeroTraits = `[Taunt]` → DT_Traits 必须有一行**行名就叫 Taunt**，否则报
> `[Trait] xxx 找不到特性行 'Taunt'`。运行时的嘲讽判定 = 行名 == Taunt **或** 行内 TraitId == Taunt（双保险），
> 所以行名直接叫 Taunt 最省事。改了行名后要重开数据表编辑器刷新。

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
| HeroTraits 引用错 | HeroTraits 数组填的是 DT_Traits 的【行名】不是 TraitId；行名与引用不一致 → 日志 `找不到特性行 xxx` |
| 光环 GE 移除 | 离圈者要按 handle 移除 GE，否则 buff 永久生效——实现时重点测"进出圈" |
| 嘲讽与集火叠加 | 索敌优先级顺序定为：ForcedTarget > 嘲讽者 > 最近敌人，测试三者并存 |
| 数据表嵌套数组 | Modifiers 是数组里套结构，行编辑器里要点开"三角"逐层展开 |
| 平衡改太多 | 一次只动一个变量，用记录表对照，否则不知道是谁改好的 |

---

## 五、下轮顺序（执行时细化）

✅ 特性运行时 → ✅ AI 升级（集火→反制→爆发→法术目标）→ ✅ 上限字段 → ✅ 编译通过（UBT little_kingEditor Win64 Development）
⏳ **等你**：教程 A（建 DT_Traits + 接线）→ 教程 B/C（AI 手感验证 + 首次平衡）→ 验收 + BugLog + Git tag v0.4
