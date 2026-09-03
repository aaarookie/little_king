# Sprint 3 指南：英雄技能与对局反馈

**状态**：规划完成；**C++ 部分已实现 ✅（本轮）**，剩余你按教程 A~E 联调。
**配套**：[07-ArtStyleGuide.md](07-ArtStyleGuide.md)（美术风格，你已建）· [05-BugLog.md](05-BugLog.md)

---

## 一、Sprint 3 目标与分工总览

| # | 内容 | 谁做 | 难度 |
|---|---|---|---|
| 1 | **英雄主动技能**（GAS Ability：每个英雄 1 个自动施放的技能 + 冷却） | C++ 数据接入 + **你做技能蓝图资产** | ★★★ |
| 2 | **顶部双方英雄血条**（HUD 顶栏，直观看到"保英雄"博弈） | C++ 提供数据函数 + **你做 UI** | ★★ |
| 3 | **胜负/超时虚弱手感验证**（调数值，快速验证一局节奏） | 你为主（数值在数据表） | ★ |
| 4 | 收尾：BugLog + Git 提交（tag v0.3） | 你我共同 | ★ |

> Sprint 3 **不做**（避免范围膨胀）：DT_Skills 表驱动技能、法术卡 GA 化（原型法术继续走 C++ 结算）、英雄特性/羁绊、血条挂在每个单位头上（Sprint 5 表现打磨再做）。

---

## 二、我（C++）实现的内容（✅ 本轮已完成，含设计说明）

### 1. 技能配置入口：`ULKGameData::HeroAbilityMap`（✅ 已实现）

```cpp
USTRUCT(BlueprintType)
struct FLKHeroSkillEntry          // 包装结构：UHT 不支持 TMap 值直接嵌套 TArray<TSubclassOf>
{
    UPROPERTY(EditAnywhere)
    TArray<TSubclassOf<UGameplayAbility>> Abilities;
};

UPROPERTY(EditAnywhere, Category="Skills")
TMap<FName, FLKHeroSkillEntry> HeroAbilityMap;
// 例：Key=Hero_Knight → Abilities[0]=GA_KnightHeal；不配 = 该英雄无技能
```

- 为什么用 TMap 而不是每英雄建 BP 子类：单位由 C++ 直接生成（`SpawnUnitForTeam`），不是 BP 子类生成——技能必须**数据驱动**才能按英雄区分。这也为将来"地牢局内换技能"铺路。
- 编辑器操作：DA_GameData → 分类 **Skills** → **Hero Ability Map** 点 + 加键，每个键展开后在 **Abilities** 数组 + 并选 GA 资产（教程 B）。

### 2. 技能授予 + 简单冷却（✅ 已实现，`ALKUnitHero`）

- 授予时机：`BeginPlay` 先授予类默认 `Abilities`（BP 子类可配，预留）；随后 `InitUnit` 末尾的 `OnUnitInitialized` 钩子里，若 `Abilities` 为空则查 `HeroAbilityMap[UnitId]` 授予（英雄由 C++ 直接生成，主路径永远是数据驱动）
- **简单冷却（新手友好设计）**：`TryCastAbilities` 带冷却计时——
  - 冷却剩余 >0 → 不尝试；=0 → `TryActivateAbilitiesByTag(LK.Ability)`，**激活成功** → 进入冷却
  - 冷却时长：类默认 `SkillCooldownSeconds = 5` 秒；**DT_Units 英雄行 `SkillCooldown` 列 > 0 时以行值为准**（教程 D 填 8/6/5）
  - 为什么不用 GAS 标准 CooldownGameplayEffect：需要用户给每个技能配"冷却 GE 资产 + 冷却标签"，对新手是三重配置；C++ 计时一个字段搞定，效果等价（技能不重复施放），GAS 冷却留到 Sprint 4 内容阶段再正规化
- 顺带：`GetTarget()` 已标记 `BlueprintPure`（ALKUnitBase，Category=LK|Unit），技能 BP 能拿到当前攻击目标
- 兜底：英雄行 `UnitId` 字段为空时自动用行名兜底并打 Warning（技能映射按 UnitId 查，防止数据表漏填导致静默失效）

### 3. 蓝图函数库 `ULKGameplayLibrary`（✅ 已实现，BP 技能要用的"武器"）

技能蓝图最怕"伤害怎么打出去"。提供 5 个静态函数（蓝图里直接搜英文函数名调用）：

| 函数 | 作用 | 技能场景 |
|---|---|---|
| `LK_ApplyDamageInRadius(中心Actor, 半径, 伤害, 施法者)` | 对**敌方**单位造成范围伤害；返回命中数 | 法师火环、游侠箭雨 |
| `LK_ApplyHealInRadius(中心Actor, 半径, 治疗量, 施法者)` | 治疗**友方**单位（含自己）；返回命中数 | 骑士鼓舞 |
| `LK_GetNearestEnemy(单位)` | 找最近敌人（返回 Actor） | 单体技能索敌 |
| `LK_GetUnitHealth(单位)` | 读当前生命 | 判定类技能 |
| `LK_GetUnitMaxHealth(单位)` | 读最大生命 | 判定类技能 |

> 施法者不是单位（非 ALKUnitBase）时会打 Warning 提示——技能蓝图里 Caster 引脚务必接英雄自身。

### 4. 顶部英雄血条数据：`GameMode::GetTeamHeroHealthRatio(ELKTeam)`（✅ 已实现）

- 返回存活英雄**当前血量合计 / 满血合计**（0~1）；无英雄时返回 0
- HUD 蓝图 Event Tick 每帧读它刷新 ProgressBar 即可，无需事件系统

> ✅ 函数签名已定稿（与上方一致）；本文件教程 A~E 均已按实际实现校对。

### 5.（S3 追加）通用"无敌"状态（✅ 已实现，为后续技能/法术预留）

- 接口（蓝图节点直接可调）：`SetInvulnerable(秒数)` / `IsInvulnerable`——英雄、佣兵、建筑通用
  - 秒数 >0 = 持续 N 秒；=0 = 立即解除；<0 = 永久（直到再调解除）
- 规则：**免疫所有普通伤害**（敌方近战/弹道/法术/技能全部挡下——所有伤害都汇聚在 `LKGameplay::ApplyDamage`，此处拦截一处生效全局）；**不免疫"真伤"**：超时虚弱以真伤形式穿透无敌（保证无平局规则不被无敌破坏）
- 视觉：无敌期间脚下阵营环/占位框变**金色**（平时绿=玩家/红=敌方）
- 调试命令：`InvulnerableHeroes 秒数`（己方在场英雄无敌，详见教程 F）

---

## 三、你要做的（新手级教程，UE5.8 中文版）

### 教程 A：创建英雄技能蓝图资产（GA_）

**前置**：C++ 库函数已就绪（上节 3），可以一路连到底。

**A1. 创建 3 个技能资产**（每个英雄一个，先建"骑士治疗"练手）
```
内容浏览器 → 右键 → 蓝图类（Blueprint Class）
→ "选择父类"对话框 → 搜索框输入：GameplayAbility
→ 选中它（UGameplayAbility，分类在"游戏玩法/Gameplay"下；注意别选成 AbilityTask 之类的）
→ 命名：GA_KnightHeal（再做 GA_MageNova、GA_RangerShot）
```
> ⚠️ 搜"GameplayAbility"可能出现多个：选**类名开头带 U、描述是 GameplayAbility** 的那个（通常结果列表第一个）。

**A2. 打上"激活标签"（最关键的一步，漏了技能永不触发）**
```
双击打开 GA_KnightHeal → 左上"类默认值"（Class Defaults）
→ 细节面板搜索：Asset Tags（UE5.8 里属性显示名是 AssetTags (Default AbilityTags)，搜 Asset Tags 或 Ability Tags 都能找到）
→ 点 + 号 → 输入：LK.Ability
```
- 标签 `LK.Ability` 已由 C++ 在模块启动时**原生注册**（无需去 Project Settings 配置），标签选择器里直接能搜到；输入时**大小写敏感**（L 大写 K 大写，中间一个点），不配 = 英雄 AI 找不到你的技能（先查这里）。
- 实例化策略（Instancing Policy）保持默认即可（5.8 默认 InstancedPerExecution，蓝图层无需改动）。

**A3. 事件图连线（C++ 库函数已就绪，直接按下面连）**
```
事件图表添加：Event ActivateAbility（右键搜索 ActivateAbility，或点左上 Functions 的覆写列表选它）
→ 拖出执行线 → 搜 LK_ApplyHealInRadius（中文编辑器里节点名仍显示英文函数名，一定能搜到）
→ 参数：
   中心Actor ← GetAvatarActorFromActorInfo（Ability 自带的纯函数，右键搜英文名）
   半径 300、治疗量 150、施法者(Caster) ← 也接 GetAvatarActorFromActorInfo
→ 执行线继续 → 搜 "End Ability"（函数名 K2_EndAbility，节点显示"结束能力"）→ 调用它
→ 编译 + 保存
```
> 技能施放点（事件/时机）由 C++ 英雄 AI 统一管理，你不需要写任何触发逻辑。
> ⚠️ **End Ability 别漏**：漏了技能会一直处于"激活中"，冷却永远无法开始（只放一次/不触发）。
>
> 💡 **中心 Actor 选谁**（决定技能罩在哪里，三个技能不一样）：
> | 技能 | 中心 Actor | 原因 |
> |---|---|---|
> | 骑士·治疗鼓舞 | 自己（Avatar） | 以自己为圆心的光环，奶身边友军 |
> | 法师·火环 | 自己（Avatar） | "火环"= 以法师自身为中心爆开一圈火，贴身才挨打；若实战总命中 0，可把中心换成 `LK_GetNearestEnemy`（火环落到敌人身上） |
> | 游侠·箭雨 | `LK_GetNearestEnemy`(自己) | 游侠是远程后排，以自己为中心 350 半径往往罩不到敌人；箭雨要"落在敌人那边" |
>
> 法师/游侠的 GA：把 `LK_ApplyHealInRadius` 换成 `LK_ApplyDamageInRadius` 即可；Caster（施法者）始终接自己，**中心 Actor 按上表选**。
> 游侠连线：从"Get Avatar Actor From Actor Info"拖出执行线/引脚 → 搜 `LK_GetNearestEnemy`（纯函数直接吃 AActor，**无需类型转换**）→ 返回值接 `LK_ApplyDamageInRadius` 的中心 Actor。
> 找不到敌人时 `LK_GetNearestEnemy` 返回空 → 伤害函数安全跳过（日志 `命中 0 个`），不会报错。

### 教程 B：把技能挂给英雄（DA_GameData）

```
打开 DA_GameData → 细节搜索：Hero Ability Map（C++ 新增字段，分类在 Skills）
→ 点 + 添加 3 个键值对（键直接打文字即可）：
   Key = Hero_Knight → 展开该条目 → Abilities 数组点 + → 下拉选 GA_KnightHeal
   Key = Hero_Mage   → Abilities[0] = GA_MageNova
   Key = Hero_Ranger → Abilities[0] = GA_RangerShot
→ 保存
```
不配 = 英雄无技能（不影响其他功能），可以先只配骑士验证全流程。
> 提示：Abilities 下拉列表选的是**蓝图技能资产**（GA_*）；确认选择器里能看到它们（若看不到，先编译保存 GA 蓝图）。

### 教程 C：顶部双方英雄血条（HUD）

**C1. Designer 加控件**
```
打开 WBP_BattleHUD → Designer
→ 面板搜"进度条"（Progress Bar）拖入根 Canvas
→ 玩家条：命名 PlayerHeroBar，锚点选"左上"，对齐 (0,0)，位置留边距
→ 敌方条：EnemyHeroBar，锚点选"右上"，对齐 (1,0)
→ 各自子文本（Text）"英雄"或留空
```

**C2. 颜色**
```
选中进度条 → 细节 → 外观（Appearance）→ 填充颜色（Fill Color）：
   玩家条 → 绿色；敌方条 → 红色
```

**C3. 蓝图刷新（每帧）**
```
事件图表 → 右键搜 Event Tick → 添加
→ 从执行线拖出：GetBattleGameMode（HUD 基类自带函数）
→ 返回值拖出 → 搜 GetTeamHeroHealthRatio（C++ 已实现，Category=LK|Battle）
   → 阵营参数：玩家条选 Player，敌方条选 Enemy（下拉枚举）
→ 结果（0~1 比例）→ 接进度条的"设置百分比"（Set Percent）节点
   Target ← PlayerHeroBar / EnemyHeroBar
→ 编译保存
```
> 坑提醒：进度条 Percent 范围 0~1；函数已保证返回 0~1，直接接即可。进度条默认从左到右填充，放哪边都自然。

### 教程 D：数值建议 + 手感验证

**D1. 填冷却数值（DT_Units 新增列）**
```
S3 给单位表加了一列 SkillCooldown（英雄技能冷却，0 = 默认 5 秒）：
打开 DT_Units → 找到 3 个英雄行（Hero_Knight / Hero_Mage / Hero_Ranger）
→ 若列没显示，重新打开数据表即可看到新列
→ 填：骑士 8、法师 6、游侠 5 → 保存
```

**D2. 技能效果数值初版建议**（之后在 GA 蓝图里调）：

| 英雄 | 技能 | 效果建议 | 冷却 |
|---|---|---|---|
| 骑士 | 治疗鼓舞 | 半径 300 内友军 +150 血 | 8s |
| 法师 | 火环 | 半径 250 内敌人 -120 | 6s |
| 游侠 | 箭雨 | 半径 350 内敌人 -90 | 5s |

**D3. 节奏手感验证脚本**：
1. 正常打一局（3 分钟档）确认胜负结算正常、技能自动释放（看输出日志 `[Hero] xxx 施放技能` + 观察血条）
2. **快速验证超时虚弱**：把 `DA_GameData → Flow → BattleTimeLimit` 临时改成 60 → 打一局拖到 60 秒 → 观察英雄逐渐掉血直到分出胜负 → 调回 480
3. 验证"保英雄"博弈：只派佣兵不保法师 → 法师被集火阵亡 → 顶部血条见底 + 火球卡变灰（Sprint 2 的法术门联动）
4. 排障对照：技能没放出来 → 依次查 ① 输出日志有没有 `[Hero] xxx 授予技能(数据驱动)`（没有=HeroAbilityMap 没配上/Key 与 DT_Units 行名不一致）② GA 蓝图 AbilityTags 是否 = LK.Ability ③ 技能蓝图有没有调 End Ability ④ 范围函数命中日志 `[Skill] 范围治疗/伤害…命中 N 个`（没有=半径太小或打错阵营）

### 教程 E：验收清单

- [ ] 3 个英雄各有 1 个技能，部署后开战自动施放，冷却期间不重复施放
- [ ] 技能伤害/治疗正确作用到敌我（日志或血量变化可见）
- [ ] 顶部两个英雄血条随战斗实时变化，一方归零即结算
- [ ] BattleTimeLimit=60 时超时虚弱可见、无平局
- [ ] 无技能的英雄（如有）不影响战斗
- [ ] BugLog 记录新问题 + Git 提交 tag v0.3

### 教程 F（S3 追加，可选）：无敌调试命令——不用拖 8 分钟也能验证虚弱

**用途**：给己方英雄挂无敌 → 敌方怎么打都打不死 → 把 `BattleTimeLimit` 临时改成 60 → 拖到超时，就能快速看到"虚弱穿透无敌"的完整效果（不用真等 8 分钟）。

**控制台（PIE 按 `~` 输入）**：
```
InvulnerableHeroes 15     → 己方在场英雄无敌 15 秒（英雄脚下变金色）
InvulnerableHeroes        → 省略秒数 = 默认 10 秒
```

**验证清单**：
- [ ] 输入命令后己方英雄变**金色**，敌方攻击打不动（血量不掉）
- [ ] 超时后（BattleTimeLimit 改成 60 秒）虚弱照常每 5 秒扣血——确认"真伤穿透无敌"
- [ ] 佣兵/建筑也可挂：`ListUnits` 会显示"无敌中"标记；日志 `[Unit] xxx 进入无敌`
- [ ] 想取消：命令里秒数不支持 0（会被当默认 10 秒）；重开一局即可

**以后做技能/法术**：蓝图里直接搜 **Set Invulnerable**（秒数随意填），就能做"圣盾 X 秒"之类的技能；想永久无敌填 -1。

---

## 四、风险与坑（提前预警）

| 坑 | 说明 |
|---|---|
| AbilityTags 写错 | 属性在 5.8 显示名为 **AssetTags (Default AbilityTags)**；值必须精确 `LK.Ability`（大小写敏感），漏配=技能静默不触发（先查这个） |
| GA 父类选错 | 蓝图类对话框搜 "GameplayAbility" 选 UGameplayAbility，别选成 AbilityTask 相关类 |
| 忘了 End Ability | 技能卡在"激活中"，冷却永不开始 → 只放一次或干脆不放（教程 A3 有提醒） |
| HeroAbilityMap Key 对不上 | Key 必须 = DT_Units 英雄行名（Hero_Knight…）；配错看日志 `[Hero] xxx 未配置技能` |
| 范围函数没命中 | 看日志 `[Skill] 范围治疗/伤害…命中 N 个`；N=0 → 半径太小/中心 Actor 接错/阵营接反 |
| 激活者上下文 | 英雄在部署阶段生成、ASC 已初始化，技能能正常激活；若激活报错先看日志有没有 GAS 警告 |
| 血条百分比反向 | ProgressBar 默认左→右填充，敌方条视觉上"从左减少"也符合直觉，无需镜像 |
| 节点中文搜不到 | 蓝图搜索一律用英文函数名（如 Set Percent、Event Tick、End Ability） |

---

## 五、C++ 开工顺序（✅ 本轮已全部完成）

1. ✅ `ULKGameplayLibrary`（蓝图函数库，技能 BP 的武器）
2. ✅ `ULKGameData::HeroAbilityMap`（含 FLKHeroSkillEntry 包装结构）+ `FLKUnitRow::SkillCooldown` + `ALKUnitHero` 授予/冷却
3. ✅ `GameMode::GetTeamHeroHealthRatio` + BP 暴露 `GetTarget` + 原生注册标签 `LK.Ability`
4. ✅ 编译通过（UBT little_kingEditor Win64 Development）
5. ⏳ **等你**：按教程 A~D 走 → 联调验收（教程 E）→ BugLog + Git tag v0.3
