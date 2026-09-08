# D0 与亡灵敌人 · UE 5.8 新手操作教程

更新：2026-09-07，适用 UE 5.8，本机验证版本 5.8.1。改动台账见 [18](18-DungeonChangeLog.md)，数值与规则见 [19](19-ContentCatalog.md)。

五种新单位、三个被动、献祭/减伤特性、战后恢复与方框显示均已由 C++ 实现，**没有必须手工创建的新蓝图、精灵或数据表**。本教程的第 1～3 节用来加载并试玩；第 4～5 节是可选配置。第 6 节记录本轮发现的两处旧 GA 资产问题及编辑器修复步骤，由维护这两份蓝图的协作者操作；本轮未替其保存资产。当前无法通过本次命令行检查确认真实鼠标与画面效果，短 PIE 复核也列在下方。

## 1. 加载新代码

1. 保存已有工作并关闭 UE 编辑器。这次增加了反射结构、枚举和组件，请完整编译后重开，不只靠 Live Coding 更新结构布局。
2. 在项目目录打开 PowerShell，执行下面命令。此电脑本轮已经构建通过，只有重新拉取源码或再改 C++ 后才需要重编译。

```powershell
& 'E:/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' little_kingEditor Win64 Development '-Project=E:/little_hero/little_king/little_king.uproject' -WaitMutex -NoHotReloadFromIDE
```

3. 等待 `Result: Succeeded`，再打开 `little_king.uproject`。其他电脑替换引擎/项目路径。
4. 内容浏览器打开 `Content/Maps/L_BattleTest`。World Settings → GameMode Override 应为现有 `BP_ALKBattleGameMode`。现有 HUD 保持 `WBP_BattleHUD`；原生 HUD Class 继承 `LKPresentationHUD`，如曾手动覆盖，按 16 的迁移方法恢复。

## 2. 不改资产，立即试玩亡灵

1. 点击工具栏 Play。先留在部署阶段。
2. 点击游戏画面获得输入焦点，按键盘 Esc 下方的反引号/波浪号键打开控制台。切到英文输入法，输入一条命令并回车：

```text
UndeadEncounter Patrol
```

3. 关闭控制台。原来的敌方三英雄会替换为一名紫色死灵法师和其营地，己方已放英雄不受影响。
4. 分别选骑士、法师、游侠并在己方半场布置。三名全部就位后点击开始；没有部署时间限制。
5. 开战后应出现米灰骷髅兵、青蓝骷髅射手。单位头顶有简易血条，营地没有；黄线仍分隔左右半场。
6. 停止 PIE 后再 Play，部署阶段分别输入下列命令，试玩其余两种组合：

```text
UndeadEncounter Elite
UndeadEncounter Boss
```

每轮只选一个；部署期间也可再次切换预设。Elite 有死灵法师和黄绿色巨人，Boss 有金色骷髅王和死灵法师。王的血条旁显示 `当前点数/复活门槛`。开战后切换会被拒绝；停止重开即可重新选。命令仅改本次运行，不保存 DA_GameData。

这是保留给单场调试的遭遇切换命令。D1 正常 Play 已按 Patrol → Elite → Boss 串成三房；完整操作见 [21](21-D1ExpeditionTutorial.md)。

## 3. 用控制台定向检查

阵营 `0=玩家、1=敌方`。以下命令每行输入一次；应在开始战斗后使用。`DamageUnit` 只找第一个同 ID、同阵营、可攻击的单位，造成无来源且穿透无敌的伤害；它不模拟远程伤害，不适合验证王的远程减伤。

### 3.1 失能与战后恢复

选择任一遭遇，布置三英雄并开始。为了减少敌军干扰，可先让玩家英雄无敌，再测试调试伤害（该命令仍能穿透）：

```text
InvulnerableHeroes 300
DamageUnit Hero_Knight 0 360
DamageUnit Hero_Mage 0 99999
ListUnits
WinMatch 0
ListUnits
```

在英雄尚未受其他伤害/治疗的情况下：当前骑士最大生命 450，扣 360 后剩 90，结束后变成 270；法师失能时显示灰色方框与“失能”，结束后回到 120/300。战斗期间不能再指挥其营地、普通治疗不能救起；结束后恢复但不重新攻击。若战斗中已发生治疗或伤害，按结束时生命 +40% 最大生命计算，不能强求仍是 270。

蓝图若要显示准确结算值：`Get Game Mode` → `Cast To LKBattleGameMode` → `Get Battle Outcome` → `Break LKBattleOutcome` → `Player Heroes`；每项有 `bWasIncapacitated`、`HealthBeforeRecovery` 和 `RecoveredState`。后者包含恢复后的 Health/MaxHealth。不要用旧团队总血条接口，也不要在显示时再加一次 40%。

### 3.2 亡灵召唤与献祭

用 Patrol 开战，保持死灵法师可行动：

```text
SpawnUnit Unit_Swordsman 0 0 -300
DamageUnit Unit_Swordsman 0 99999
SpawnUnit Unit_Archer 0 400 -300
DamageUnit Unit_Archer 0 99999
ListUnits
```

剑士死亡位置出现敌方骷髅兵，弓箭手死亡位置出现敌方骷髅射手。它们可以出现在玩家半场，因为这是原地转换，不是玩家手牌部署。每次成功召唤死灵法师扣 14.4（180×8%）生命；生成失败不扣。输出日志中可搜索 `[Passive]` 或“亡灵召唤”。注意场上若已有同类玩家佣兵，DamageUnit 可能命中更早的那一名，其实际死亡位置才是召唤点。

### 3.3 巨骨

用 Elite 开战，给巨人制造伤口，再生成并击倒同阵营骷髅：

```text
DamageUnit Hero_SkeletonGiant 1 100
SpawnUnit Unit_Skeleton 1 500 300
DamageUnit Unit_Skeleton 1 99999
ListUnits
```

巨人应额外回复 8.4（280×3%），封顶 280。敌方骷髅数量多时调试伤害会命中第一个同类对象，但同阵营任意一名骷髅兵死亡都能触发。其他战斗事件可能同时改变生命，可在 Output Log 和头顶实际治疗数字中确认。

### 3.4 朽骨再生

用 Boss 开战。下列步骤人工提供两次骷髅英雄失能，每次 +5；为了稳定操作，尽量在其他单位还没大量死亡时执行：

```text
SpawnUnit Hero_SkeletonGiant 1 700 300
DamageUnit Hero_SkeletonGiant 1 99999
SpawnUnit Hero_SkeletonGiant 1 700 300
DamageUnit Hero_SkeletonGiant 1 99999
DamageUnit Boss_SkeletonKing 1 99999
```

王到 10/10 后被击倒，应立即满血站起，计数变成 0/15。战斗不会因为这一次倒下就结束；再次攒满 15 后倒下会变成 0/20。未满额倒下时保持失能；若死灵法师仍在场，战斗继续，否则结算并执行全员战后恢复。每次新建的英雄会计入该方英雄数量，所以用完本组测试后停止 PIE，避免调试生成的英雄影响下一项检查。

远程减伤推荐观察真实弓箭/箭塔/火球命中：未加其他修饰时火球 60 应扣王 42。无敌/虚弱、其他单位同时攻击会干扰手工读数；精确的远程来源、建筑、技能和弹道来源销毁用例已经纳入自动化。

## 4. 可选：让地图默认使用亡灵遭遇

1. 先停止 PIE，在内容浏览器打开 `Content/Data/DA_GameData`。
2. 细节面板搜索 `Enemy Encounter Id`，填完整 ID：`Encounter_UndeadPatrol`、`Encounter_UndeadElite` 或 `Encounter_SkeletonKing`。
3. 搜索 `Hero Post Battle Recovery`，默认是 **0.4**，不是 40。无需改动即可使用 40% 规则。
4. 保存该资产，再 Play。敌方应直接使用所选遭遇。恢复原镜像对手时将 Enemy Encounter Id 清空为 None。
5. 在 18 记录自己保存过 DA_GameData，避免另一位协作者覆盖。

只想试玩不需要保存以上配置，使用第 2 节命令即可。亡灵遭遇不要求给敌方凑五种卡，也不要往玩家默认卡组塞两张同类骷髅卡。

## 5. 可选：把新敌人的参数交给数据表

当前即使 DT_Units 没有新行，五个新 ID 也可以直接使用。只有希望在编辑器调参时才做此节。

1. 停止 PIE，打开 `Content/Data/DT_Units`；右上角可搜索字段。新增一行，行名严格使用 19 中的 UnitId。也把该行的 `Unit Id` 填成相同值。
2. 若复制旧行起步，先清空 Sprite、Hero Traits、Spawn Unit Id，并将 Building Behavior 设为 None；新敌人不要沿用旧法师专属标记/额外技能绑定。
3. 按 19 的表填 `Unit Class`、`Attack Type`、`Base Health`、`Attack Damage`、`Attack Range`、`Attack Interval`、`Move Speed`，Attack Windup=0.15。新单位无需往 Hero Ability Map 添加 GA。
4. 按下表填本轮新字段，Placeholder Color 选相应色且 Alpha=1。Sprite 保持 None。

| 行名 | Unit Class | Skeleton | Passive Ability | Hero Traits |
|---|---|---|---|---|
| Unit_Skeleton | Soldier | true | None | 空 |
| Unit_SkeletonArcher | Soldier | true | None | 空 |
| Hero_Necromancer | Hero | false | UndeadSummoning | Trait_Sacrifice |
| Hero_SkeletonGiant | Hero | true | GiantBones | 空 |
| Boss_SkeletonKing | Boss | true | BoneRegeneration | Trait_FaceFear |

5. 巨人的 `Passive Heal Percent` = 0.03；王的 `Revival Initial Threshold` = 10、`Revival Threshold Step` = 5。完整数值见图鉴。
6. 保存 DT_Units，再 Play 验证。同名表行的名称与战斗数值会被采用；UnitId、兵种、攻击类型、骷髅身份、被动、配套特性与占位颜色会由代码按五个稳定 ID 校正，避免复制旧行时漏填关键规则。表里仍建议填完整，便于协作者阅读。

献祭和直面恐惧的内置 ID 无需 DT_Traits 新建行。若以后需要数据驱动的不同百分比，可以在 DT_Traits 新增自定义 ID，Effect 分别选择 `SummoningHealthCost` / `RangedDamageReduction`，Effect Value 填 0～1，Modifiers 留空，然后在单位 Hero Traits 中替换成该 ID。不要重复挂两个同效果特性，除非确实希望它们相加；同类效果总值钳制在 0～1。

## 6. 两处旧技能蓝图的修复教程

这两份资产不属于新亡灵被动实现，但会影响玩家英雄技能与连战平衡。用户已于 D1 前按本节修正并保存；本节保留为复查和重建教程。自动图检查已确认接线语义，仍建议在 PIE 观察一次实际命中。

### 6.1 法师 GA_MageNova：给取敌函数传入自己

1. 停止 PIE，双击打开 `Content/blueprint/GA_MageNova`，打开 Event Graph。
2. 找到 `Get Avatar Actor From Actor Info` 与 `LK_GetNearestEnemy` 节点。前者的 Return Value 是施法英雄，后者的 Unit 输入目前没有接线。
3. 从前者 **Return Value** 蓝色引脚拖一根线到后者 **Unit**。一根输出可以分给多个输入，不必断开已有的 Caster 连接。
4. 确认 `LK_GetNearestEnemy.Return Value` 接 `LK_ApplyDamageInRadius.Center Actor`；Caster 接自己的 Avatar；Radius=250，Damage=120。
5. 白色执行线为 `Event Activate Ability → LK_ApplyDamageInRadius → End Ability`。保持原技能标签 `LK.Ability` 和现有冷却配置。
6. 点击 Compile，再 Save。重新 Play，在敌人进入可追击范围时观察技能是否造成范围伤害。取敌函数会遵守嘲讽优先级。

### 6.2 游侠 GA_RangerShot：把治疗改成伤害

1. 打开 `Content/blueprint/GA_RangerShot` 的 Event Graph。按 6.1 的方法把 Avatar.Return Value 接入 `LK_GetNearestEnemy.Unit`。
2. 找到旧 `LK_ApplyHealInRadius` 节点。先记住它前后的白色执行线，然后选中该节点按 Delete。
3. 在空白处右键搜索 `LK_ApplyDamageInRadius`，创建伤害节点。若搜不到，确认新 C++ 已加载，必要时取消 Context Sensitive 再搜索 `ApplyDamageInRadius`。
4. 连接 `Event Activate Ability → 伤害节点 → End Ability` 的白线。连接取敌 Return Value → Center Actor，Avatar.Return Value → Caster。
5. 设置 Radius=350、Damage=90。保留 `LK.Ability` 标签；DT_Units 中游侠 Skill Cooldown 为 5。
6. Compile、Save，再 Play。箭雨应伤害敌军，而非尝试治疗敌军位置附近的己方单位。击中王时按远程技能减伤，基础 90 应为 63。

如果只编译却不保存，下次打开仍是旧连线。完成后记录两个资产路径、操作人、编译结果和一次实际技能命中结果；这些操作无需新建精灵或音效。

## 7. D0 验证与历史说明

本轮已实际完成 Editor 构建、16 组自动化、五个项目蓝图编译。以后涉及这些逻辑才重跑：

```powershell
& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests LittleKing' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/little_hero/little_king/Saved/Automation/D0Undead' '-abslog=E:/little_hero/little_king/Saved/Logs/D0UndeadAutomation.log'
```

以报告 `index.json` 中 failed=0、notRun=0、每个测试 state=Success 为准，不能只看进程退出码。测试世界无相机和专门触发的单位上限警告不等于断言失败。NullRHI 没有实际画面：进入 Boss 一次，检查彩色方框、名称、头顶血条、王计数、黄线和箭塔放置射程即可，不需要进行打包或硬件专项。

D1 已完成：RunSubsystem 会注入 BattleContext，并携带 GetBattleOutcome.RecoveredState 进入下一房。当前仍不会发奖励或保存远征；这些属于 D3/D5。继续开发时不要为了让第二房满血而额外加恢复，也不要沿用旧“英雄销毁就从队伍移除”的规则。
