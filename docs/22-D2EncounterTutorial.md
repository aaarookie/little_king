# D2 · UE 5.8 遭遇配置与英雄状态新手教程

适用版本：UE 5.8.1，D2。源码、`DT_Encounters` 和三房默认数据已经完成；不要求你再创建资产。本教程说明如何安全调遭遇、验证跨房英雄状态，以及配置错误时如何定位问题。

## 1. 编译并确认资产

1. 关闭 Unreal Editor，避免 Live Coding 锁住外部构建。
2. 在 PowerShell 进入项目目录，执行：

```powershell
& 'E:/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' little_kingEditor Win64 Development '-Project=E:/little_hero/little_king/little_king.uproject' -WaitMutex -NoHotReloadFromIDE
```

3. 看到 `Result: Succeeded` 后打开 `little_king.uproject`。
4. 在内容浏览器打开 `Content/Data/DT_Encounters`。右上或行结构处应显示 `LKEncounterRow`，并有以下三行：

| 行名 | 强度 | 奖励档 |
|---|---|---:|
| `Encounter_UndeadPatrol` | Normal | 1 |
| `Encounter_UndeadElite` | Elite | 2 |
| `Encounter_SkeletonKing` | Boss | 3 |

5. 打开 `Content/Data/DA_GameData`，在 Data 分类确认 Encounter Table 指向 `DT_Encounters`。该属性在 C++ 有默认软引用，旧 DA 没有显式保存它也能正确加载。

## 2. 每一列控制什么

| 字段 | 用途 | 约束 |
|---|---|---|
| EncounterId | 稳定定义 ID | 必须与行名相同 |
| DisplayName | 编辑器和后续 UI 显示名 | 可自由改，不参与逻辑 |
| Rank | Normal / Elite / Boss | 描述强度，不代替路线节点类型 |
| RewardTier | 后续 D3 奖励强度 | 1～100；当前三房为 1/2/3 |
| EnemyHeroIds | 开局自动部署的敌方英雄/首领 | 至少一个，不得重复；必须是 Hero 或 Boss 单位行 |
| Waves | 开战后按时间生成的敌方单位 | 至少一项；Time ≥ 0，Count 1～50，只能引用佣兵或建筑 |
| bEnemyUsesCards | 是否启用敌方过牌 AI | 关闭时仍可运行波次和集火 |
| EnemyCards | 敌方牌组 | 开启出牌时至少 5 个不同的有效 CardId；当前手牌为 4 槽 |
| EnemySilverPerSecond / Cap / StartingSilver | 敌方单房经济 | 初值不能大于上限，全部必须是有限非负值，上限必须大于 0 |
| AI | 本房集火、反制和爆发节奏 | 所有 Min ≤ Max；反制和爆发只有在出牌开启时运行 |

固定路线仍引用上面的三个稳定行名。可以添加其他合法行供控制台测试和以后扩展路线，但不能删除或重命名固定三行。行内 EncounterId 与行名冲突、英雄/卡牌/单位不存在、数组重复、波次非法或 AI 最小值大于最大值都会在远征开始前报错。

## 3. 当前三房参数

| 房间 | 英雄 | 波次 | 战术 |
|---|---|---|---|
| 巡逻 | 死灵法师 | 0 秒 3 兵+1 射手；28 秒 2 兵；55 秒 1 射手 | 不集火、不出牌 |
| 精英 | 死灵法师、骷髅巨人 | 0 秒 3 兵+2 射手；32 秒 2 兵；60 秒 2 射手 | 28～36 秒一次集火，持续 6 秒 |
| 首领 | 骷髅王、死灵法师 | 0 秒 4 兵+2 射手；30 秒 2 兵；60 秒 2 射手 | 22～30 秒一次集火，持续 7 秒 |

这些数值是三连战的第一版低压曲线。骷髅死亡还会触发召唤、巨骨和朽骨再生，因此不能只按表面波次数量比较难度。一次只改一个维度并完整走三房：先调开局数量，再调后续时间，最后才调英雄数值。

## 4. 安全修改和恢复遭遇表

直接在 `DT_Encounters` 修改并保存即可。修改只影响之后新建的远征；已经开始的远征持有自己的目录副本，不会在第二房突然换参数。要立即检查新值，结束 PIE 后重新 Play，确保创建新的 GameInstance 和 RunState。

仓库同时保存可审查源文件 `Content/DataSources/DT_Encounters.json`。如果表被误删或希望按源文件重建：

1. 关闭编辑器并先完成 C++ 构建。
2. 在 PowerShell 执行：

```powershell
& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -run=pythonscript '-script=E:/little_hero/little_king/Scripts/CreateD2EncounterTable.py' -unattended -nop4 -nosplash -NullRHI -EnablePlugin=PythonScriptPlugin -EnablePlugin=EditorScriptingUtilities
```

该脚本会创建或覆盖 `DT_Encounters` 的三行内容。你在编辑器里调出满意数值后，也要同步 JSON 和 [19 内容图鉴](19-ContentCatalog.md)，否则下次运行脚本会恢复 JSON 中的值。

## 5. 检查英雄永久最大生命

正常完成第一房并停留在“下一关”结算界面。按 `~` 打开控制台，输入：

```text
RunHeroMaxHealth Hero_Knight 600
ListRunState
```

日志应出现骑士 `BaseMax=600`。点击“下一关”，重新部署三名英雄，再输入：

```text
ListUnits
```

骑士最大生命应为 600，当前生命保留上一房恢复后的绝对值并钳制在新上限内。`RunHeroMaxHealth` 只能在前两房胜利后的房间间阶段使用；部署中、战斗中、失败后或第三房终态调用会拒绝。

降低最大生命也必须安全。例如第一房结算后输入 `RunHeroMaxHealth Hero_Knight 200`，下一房骑士生命不能超过 200。新一轮“从头开始”重新读取 `DT_Units` 初始值，不继承上一轮的 600 或 200。

## 6. 检查永久特性增加与删除

仍在第一房胜利结算界面时输入：

```text
RunHeroTrait Hero_Mage Trait_MageSpellReach 0
RunHeroTrait Hero_Ranger Trait_FaceFear 1
ListRunState
```

点击“下一关”后：

- 法师不再提供全场施法，火球在敌方半场落点会提示无法释放；
- 游侠拥有直面恐惧，受到远程来源伤害减少 30%；
- 重复添加同一特性、重复删除不存在的特性或添加未知 ID 会被拒绝。

战斗中现有 `HeroTrait` 命令仍直接改场上英雄。该变化会随合法 BattleOutcome 进入下一房；房间间 `RunHeroTrait` 直接修改 RunState，适合以后 D3 奖励使用。

## 7. 确认临时状态没有串房

第一房里让敌人触发一次集火，或让英雄处于技能冷却/手动移动中，然后结束战斗并进入下一房。新房部署后应满足：

- 没有上一房的黄色集火预警；
- 单位不会继续攻击上一世界的目标；
- 骑士光环重新按新位置计算，不保留旧 Aura Source；
- 英雄技能冷却为 0，可以按本房条件重新释放；
- 手动移动、攻击前摇和普攻冷却均从新房状态开始。

永久特性 ID 会继承，临时 GE 句柄和 Actor 引用不会写入 RunState。

## 8. 常见错误

| 日志或现象 | 修复方法 |
|---|---|
| `DT_Encounters 必须使用 LKEncounterRow` | 重新创建 DataTable，Row Structure 选择 LKEncounterRow，再导入 JSON |
| `缺少固定路线行` | 补齐三个精确行名，不要用 Patrol / Elite / Boss 作为正式行名 |
| `敌方牌组少于 HandSize + 1` | HandSize=4 时至少填 5 个不同且存在于 CardLibrary 的 CardId |
| `波次单位不存在或类别错误` | Waves 只能填佣兵/建筑 UnitId；英雄放 EnemyHeroIds |
| 改表后当前远征没变化 | 这是目录快照的预期行为；结束 PIE 后重新 Play |
| 删除法师特性后下一房又出现 | 不要在 BeginPlay 蓝图额外添加默认特性；远征部署会用 RunHeroState 精确替换 |
| 下一房生命超过新最大值 | 检查是否由其他蓝图在部署后直接写 Health；C++ 恢复入口会自动钳制 |

## 9. 自动验证

关闭编辑器后执行完整逻辑测试：

```powershell
& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests LittleKing' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/little_hero/little_king/Saved/Automation/D2' '-abslog=E:/little_hero/little_king/Saved/Logs/D2Automation.log'
```

再只读编译五个项目蓝图：

```powershell
& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -run=CompileAllBlueprints '-AllowListFile=Saved/Sprint5BlueprintAllowList.txt' -unattended -nop4 -nosplash -NullRHI '-abslog=E:/little_hero/little_king/Saved/Logs/D2BlueprintCompile.log'
```

以自动化报告中的 `failed=0`、`notRun=0` 和每项 `state=Success` 为准。NullRHI 不验证真实文字、鼠标命中或画面层级，最后按第 3、5、6、7 节做一次短 PIE 即可；本阶段不做打包、硬件、音效或正式美术检查。

## 10. D2 边界

D2 当时只把 RewardTier 写入遭遇和 BattleContext；D3 现已完成候选生成、三选一/跳过、升级、原子领取和原生界面，见 [23](23-D3RewardTutorial.md)。D4 才加入分支路线，D5 才加入安全节点存档。RewardTier 当前影响奖励池中新卡数量上限，不直接改变战斗数值。
