# Sprint 5 · UE 5.8 新手迁移与验收教程

更新：2026-09-06。适用本项目 UE 5.8（本机实际引擎为 5.8.1）。先读 [15 变更记录](15-Sprint5ChangeLog.md)。本轮 C++ 提供血条、营地、选中范围、弹道与飘字的最低可用显示，不需要你另写这些系统。

用户已保存 HUD/数据资产并完成数次基础试玩；助手本轮未覆写二进制资产。A～F 是旧版本的迁移/复核教程，已完成的不用重做；本次新增检查集中在 C 的满手循环及下方 F.1 的射程/黄线。G 仅为开发用定向验证。音效、表现素材、打包和硬件测试全部后置，不作为进入地牢的要求。每次实际修改后记录资产名与结果到 15。

## A. 正确加载新 C++

1. 如果 UE 编辑器已经开着，保存当前工作后关闭。此次包含新 UCLASS、结构体和组件层级变化，使用完整构建再重开。
2. 在 Windows 文件资源管理器进入项目根目录 `E:\little_hero\little_king`，右键空白处打开终端，选择 PowerShell。
3. 执行以下命令；其他电脑把引擎和项目路径替换为自己的实际路径。

```powershell
& 'E:/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' little_kingEditor Win64 Development '-Project=E:/little_hero/little_king/little_king.uproject' -WaitMutex -NoHotReloadFromIDE
```

4. 看到 `Result: Succeeded` 后，双击 `little_king.uproject`。打开 Content/Maps/L_BattleTest。
5. 在内容抽屉（Content Drawer）打开 `Content/blueprint/BP_ALKBattleGameMode`。点击 Class Defaults（类默认值），在 Details（细节）搜索：

| 字段 | 应设置为 |
|---|---|
| Game Data | Content/Data/DA_GameData |
| HUD Widget Class | WBP_BattleHUD |
| HUD Class | LKPresentationHUD（原生 C++ 类，类名可能显示空格） |
| Player Controller Class | LKPlayerController |
| Available Heroes（如可编辑） | Hero_Knight、Hero_Mage、Hero_Ranger，各一次；空数组使用这三名默认英雄 |

`HUD Class` 和 `HUD Widget Class` 是两个字段：前者负责新头顶条/营地等显示，后者负责原来的 UMG 手牌和面板。如果 HUD Class 旁边有“重置到默认值”箭头，可以重置后核对是否为 LKPresentationHUD。不要把 WBP_BattleHUD 填到 HUD Class。

6. 点击 Compile（编译）和 Save（保存）。回到地图，打开 World Settings（世界设置），确认 GameMode Override 为 BP_ALKBattleGameMode。
7. 地图中的相机先保持正俯视：位置约 `(0, 0, 2000)`，旋转 Pitch=-90 / Yaw=0 / Roll=0，正交 Ortho Width 约 4800。不要在这轮迁移中同时改斜视。鼠标现在直接与 Z=0 求交，不受点到角色/地板表面的高度影响。

**看到什么算正确：** Play 后显示部署进度；营地及新条由代码自动出现；没有配置精灵的单位显示简单色块。若只有 UMG 而没有营地圈/头顶条，先检查 HUD Class。

## B. 删除旧团队血条和旧倒计时

1. 双击 `Content/blueprint/WBP_BattleHUD`，切到 Designer（设计器）。
2. 在 Hierarchy（层级）搜索 `PlayerHeroBar`、`EnemyHeroBar`，逐个选中并删除。删除只服务这两条的标签/背景容器，保留手牌、银币、部署和结算面板。
3. 切到 Graph（图表），按 Ctrl+F 搜索 `GetTeamHeroHealthRatio`，删除旧的百分比绑定函数和相关执行链。如果某个 Tick 还连接其他必要 UI 更新，只移除设置旧条的那段，不要整条删掉。
4. 再搜索 `PlayerHeroBar` / `EnemyHeroBar`，清理删除控件后变红的引用节点。Compile；出现这些名字的错误时双击错误定位，清掉剩余引用。
5. 删除部署倒计时文本和其更新逻辑。检查 `Delay`、定时器事件、`DeploymentTime`、`ForceStartBattle` 或 `RequestStartBattle` 的自动调用；部署开始后按时间开战的链路要移除。
6. 开始按钮 `Btn_Start` 的 OnClicked 保留：`Get LK Player Controller → Request Start Battle`。不要直接修改 GameState.Phase。
7. 所有英雄按钮仍调用 `Begin Hero Placement`，分别传 `Hero_Knight`、`Hero_Mage`、`Hero_Ranger`。代码拒绝同一英雄重复部署，并在全部部署前拒绝开始。
8. Compile 和 Save。

**临时兼容：** C++ 会每帧折叠标准名字的两条旧血条、控制 Btn_Start 是否可按；这只为旧资产能继续加载，不能代替上述删除。重命名过的旧条必须由你在层级里找到并移除。请勿继续按 Sprint 3 教程创建团队血条。

## C. 更新手牌和法术 UI

1. 在 WBP_BattleHUD 的 Graph 找 `Event On Hand Changed`。它输出 Hand、Costs、bPlayable 三个数组；每个索引 0~3 对应固定槽位。
2. 有效牌组初始化后 Hand 长度恒为 4，每次成功出牌也保持四槽非 None。不要再设计“等待过其他牌才补空槽”的分支。可保留 Name=None 的防御性显示处理（只针对未初始化/错误配置），不显示 Costs=-1；它不属于正常战斗循环。
3. 若不是 None：按 CardId 更新图标与名字，Costs[Index] 显示费用，按钮可用性取 bPlayable[Index]。银币不足和错误阶段可以使按钮不可用；法师死亡不能再单独使法术卡变灰。
4. Ctrl+F 搜索 `CanCastSpell`、`HasMage`、`SpellLock`、`法师`、`锁定`。移除“没有法师 → 所有法术按钮变灰/禁用”的分支，包括自建的法术锁图层。
5. 搜索 `Event On Play Result`。原来的“法师阵亡，法术无法释放”提示链删除。新版原生 HUD 会在无全场施法特性且点击敌方半场时显示正确提示；C++ 不再把这个结果送给旧蓝图错误提示分支。
6. 若之前在 OnPlayResult 中为其他错误做了 TipText，可保留，但会与原生提示重复。推荐移除该事件中的 TipText 显示链，由原生 HUD 统一提示。TipText 如仅作错误提示，可折叠或删除，随后清理引用。
7. 若旧 `Event On Placement State Changed` 用 Switch 区分 Card / Hero，新增的 HeroMove 走“营地指挥”分支，或不执行旧选卡效果。不要把未知分支默认当作 Hero 并读取空 HeroId。
8. 如果全屏背景吃掉战场点击：在 Designer 选根 Canvas/纯装饰背景，Visibility 选择 `Not Hit-Testable (Self Only)`（自身不可命中测试，子控件仍可交互）。实际 Button 保持 Visible。不要给包含按钮的父面板设置 `Self & All Children`。
9. Compile 和 Save，然后逐个点击四个卡槽检查图标/费用是否随原槽更新。

**预期：** 四个槽始终有牌且不同。开局只洗牌一次，出牌后队首补到原槽、刚打出的牌排队尾。例如手牌 A/B/C/D、队列 E/F/G，出 A 后为 E/B/C/D，再出 B 后为 E/F/C/D，再出 C 后为 E/F/G/D，继续出 D 才轮到 A 回手。费用不足仍可能禁用按钮，不代表空槽。法师死后仍照常轮转。

如想制作“下一张”小预览，可在 Get Deck State 上调用 `Get Next Card`，再用 Get Card Definition / Get Card Icon 查显示；这是可选 UI，本轮未新增对应 UMG 控件。不要调用 Draw Card 来填满，它现在是兼容空操作。

## D. 更新 DA_GameData 和卡牌数据

1. 打开 `Content/Data/DA_GameData`。在 Details 搜索 `Default Player Deck`，展开数组，用右侧加号添加条目、条目菜单删除多余项，将七个 Name 值分别填为下表。对 `Default Enemy Deck` 同样处理。

| CardId（必须精确） | CardLibrary 对应已有资产 |
|---|---|
| Unit_Swordsman | C_Swordsman |
| Unit_Archer | C_Archer |
| Unit_Shieldbearer | C_Shieldbearer |
| Spell_Fireball | C_Fireball |
| Spell_HealWave | C_HealWave |
| Building_ArrowTower | C_ArrowTower |
| Building_Barracks | C_Barracks |

2. 搜索 Card Library，确认七个资产都在数组中。打开每个 C_* 检查 CardId，资产名 `C_Fireball` 与运行时 CardId `Spell_Fireball` 是不同用途，不能混填。
3. `Hand Size=4`，每方去重后至少五种卡；建议保留上述七种。重复 ID 会归并，不能用多副本凑数量。少于五种或 CardLibrary 缺卡会显示配置提示并阻止开战，请在资产修正后重新 Play。保留当前已调好的 Silver Per Second / Silver Cap；源码默认分别为约 0.333333 和 5。不要因为旧文档写 10~20 就顺手改经济。
4. 检查 C_ArrowTower 和 C_Barracks 的 `Building Type Limit Override`。要继承全局 2 个上限，填 **-2**；-1 明确表示不限。已有资产保存的 -1 不会自动变成 -2。
5. 检查两张法术：Fireball 的 Spell Effect=Damage，HealWave=Heal，Spell Value 和 Spell Radius 必须大于 0。法术效果为 None 属于配置错误，出牌将被拒绝。
6. 回到 DA_GameData，按以下字段核对并保存：

| 搜索字段 | 本轮建议初始值 |
|---|---|
| Hero Camp Move Radius / Hero Camp Body Radius | 850 / 65 |
| Unit Body Radius | 50 |
| Knight Taunt Aura Radius / Taunt Acquire Radius | 400 / 500 |
| Fireball Shake Intensity / Camera Shake Scale | 10 / 1（Scale=0 可关闭） |
| Fireball Skill Hero Ids | 数组只放 Hero_Mage |
| AI Focus Warning Seconds / AI Push Reserve Max Seconds | 2.5 / 15 |
| Battle Seed | 12345，复测时固定 |
| Native Damage Text | 勾选 |

营地移动半径太小时，代码会抬到足够容纳营地和英雄出生体积的最小值。开始用上述默认值，稳定后一次只改一个范围。

旧 Deployment Time、Require Mage For Spells、Max Heroes Per Team 等兼容字段不再控制本轮的新规则。默认三英雄以 AvailableHeroes 为准，本轮保持三名。

## E. 英雄特性和技能

1. 在 DA_GameData 搜索 `Default Hero Traits`。展开 Map（映射）的每个元素，Key 为英雄 UnitId，Value 内有 Traits 数组。确认：

| Key | Value → Traits |
|---|---|
| Hero_Mage | Trait_MageSpellReach |
| Hero_Knight | Trait_KnightTauntAura |

2. Hero_Ranger 可不填此映射，或 Traits 为空。不要为游侠添加文档中的候选特性，它尚未实现。
3. 打开 DT_Units，逐个选中三个 Hero_* 行，检查 HeroTraits 列。删去旧 `Trait_MageMight`、`Trait_KnightAura`；代码也会剔除这两个旧默认特性，资产清理有助协作者看懂实际规则。已有盾卫 `Taunt` 可以保留。
4. 上述三个内置特性 ID 不要求在 DT_Traits 中补行。要新建自定义特性：打开 DT_Traits，添加新行，填 TraitId/名字，Effect 选择 `GlobalSpellPlacement`、`Taunt`、`MeleeSoldierTauntAura` 或 `Attributes`。光环半径填 EffectRadius；属性加成在 Modifiers 中配置。
5. 在游戏蓝图中，拿到英雄 Actor 引用后调用 `Add Trait` / `Remove Trait` 并填写 TraitId；返回 bool 表示是否发生修改。只读 `Has Trait` / `Has Trait Effect` 可做 UI。不要修改“法师是否在场”来模拟特性。
6. 检查 DA_GameData 的 Hero Ability Map：当前法师可继续使用 Content/blueprint/GA_MageNova，骑士使用 GA_KnightHeal，游侠使用 GA_RangerShot。技能与本节特性是独立配置。
7. 本轮把 Hero_Mage 成功激活的当前技能视为一次火球释放，小震由 C++ 触发。若你在 GA_MageNova 中另加过 Camera Shake，删除那一条，避免一次技能震两次。英雄普通远程攻击不触发火球震动。
8. 技能效果应调用 `LK Apply Damage In Radius` / `LK Apply Heal In Radius`，保留正确 Caster，并在瞬时效果完成后 `End Ability`。不要直接写 Health 或另走不带来源的伤害链。新增 Delay/异步技能时还需接取消事件，避免英雄移动后旧技能继续施放。

**验证增删特性：** Play、三英雄部署并开战，按 `~` 打开游戏控制台，逐行输入：

```text
HeroTrait Hero_Mage Trait_MageSpellReach 0
HeroTrait Hero_Mage Trait_MageSpellReach 1
HeroTrait Hero_Knight Trait_KnightTauntAura 0
HeroTrait Hero_Knight Trait_KnightTauntAura 1
```

最后的 0 表示删除，1 表示添加，只针对己方存活英雄。法师去掉特性时只限制敌方半场，恢复后再次允许全场；骑士的近战佣兵嘲讽应相应消失/恢复。

## F. 营地、血条、法师死亡和火球验收

1. Play 后先只部署一名英雄，等待至少半分钟，尝试开始。应该仍停在部署阶段，不能因旧 20 秒计时自动开始。
2. 继续部署另外两名。应生成三个独立营地，英雄站在各自营地旁；再次点同一英雄部署应提示已部署。
3. 点击己方营地，看见移动圈；点击圈内空地，看英雄走过去。试一条横穿营地的路线，应绕行；点营地内部或圈外，应拒绝移动。右键取消选中。
4. 开战后再次下移动指令：移动中停止普攻和技能，到达恢复自动攻击。连续指定新地点应立即接受，没有次数/冷却提示。若目的地被其他建筑封住，会拒绝或停下等待可用路径；换一个可达地点。
5. 按 `~` 输入 `AddSilver 10`，关闭控制台，选择火球卡，记住当前银币和手牌。法师活着时敌方半场可落点，只有释放瞬间一次轻微震动；点治疗卡不会震。
6. 再开控制台输入 `DamageUnit Hero_Mage 0 99999`，单独杀死己方法师。此调试伤害穿透无敌且不计入任一方输出；其他两英雄应仍存活，法师营地留在原地变灰，没有血条也不能指挥。
7. 再次补足银币并选择法术。在己方半场释放应成功；尝试敌方半场只出现范围限制提示，不扣费、不换牌。法术卡不能因法师死亡而额外变灰。
8. 输入 `DamageUnit Hero_Knight 0 50`，只伤骑士：它的头顶条下降；屏幕不震。英雄死亡、普通单位死亡同样不震。
9. 骑士存活时在旁边放己方剑士，观察剑士嘲讽圈；弓箭手、其他英雄和建筑不应因该光环获得嘲讽。移开骑士后至多约半秒应撤销。用 E 节命令删除特性可检查立即撤销。
10. 正常战斗中观察敌方集火前的 `!`；有附近嘲讽者时，敌人仍优先攻击嘲讽者。自动测试另有精确优先级覆盖，手工测试主要确认可读性。
11. 输入 `WinMatch 0` 结束对局，再输入 `ProjectilePool`。在飞数应为 0，单位停止行动；点重新开始后回到新的部署阶段。

**卡住时先检查：** 营地点击是否被全屏 UMG 背景截获、是否仍处于卡牌放置模式（右键取消后再点营地）、营地英雄是否已死、目的地是否在可达范围内。单位过度拥挤可能暂时影响到达，当前没有复杂 NavMesh 人群避让。

## F.1 本次新增：建筑射程与黄色中线（UE 5.8）

1. 用 A 节命令构建后打开 L_BattleTest，确认 BP_ALKBattleGameMode 的 HUD Class 是 LKPresentationHUD。无需新建蓝图范围圈、贴花或黄线资产。
2. Play，在部署阶段检查屏幕中间有黄色分界线。若要确认它不再依赖调试显示：停止 Play，打开 DA_GameData，取消 `Draw Field Bounds` 和 `Draw Debug Shapes`，保存并再次 Play；白色调试边框可消失，黄色中线应保留。
3. 部署全部三英雄并开战。按 `~` 输入 `AddSilver 10`，关闭控制台；出其他牌直到箭塔到手，点击箭塔卡。
4. 鼠标在己方半场移动：落点附近有占地小圈，外面有金色基础射程大圈；非法落点变红。移到敌方半场可检查拒绝，右键取消后两个预览圈都消失。
5. 停止 Play，打开 DT_Units，选择 Building_ArrowTower 行，`Building Behavior=Turret`，查看 `Attack Range`。大圈使用这个值，不使用卡牌 Spell Radius。兵营应为 Barracks，不显示攻击大圈。无需改当前已调好的射程。
6. 如果没有圈/线：检查 HUD Class；WBP 全屏纯背景不要画不透明色挡住战场；确保仍处于放置模式。当前预览显示基础射程，不计落地后获得的临时战斗增益。

## G. 开发用自动化与蓝图检查（可复现命令）

关闭编辑器后，在 PowerShell 中运行：

```powershell
& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests LittleKing.Sprint5' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/little_hero/little_king/Saved/Automation/Sprint5Queue' '-abslog=E:/little_hero/little_king/Saved/Logs/Sprint5QueueAutomation.log'
```

报告看 `Saved/Automation/Sprint5Queue/index.json` 的 succeeded/failed；日志搜索 `Test Completed`。不要只看进程退出码，测试失败时 `TestExit` 也可能返回 0。NullRHI 不打开渲染画面，只证明相应逻辑测试结果。

然后在编辑器里逐个 Compile WBP_BattleHUD、BP_ALKBattleGameMode 和三个 GA，确认没有红色编译错误。弃用旧血条函数的警告也应在 B 节清理后消失。

本次保存的 DA_GameData 双方牌组和原生 HUDClass 已通过只读自动化检查；五个指定项目蓝图编译为 0 错误、0 警告。NullRHI 不检查真实画面像素和鼠标遮挡，新增画面按 F.1 做短检查即可。

音效接口和原生最低反馈可以保留，当前无需导入 Audio、制作动画、打包 Windows 或安排硬件测试。下一阶段地牢任务见 [17](17-DungeonDevelopmentPlan.md)。
