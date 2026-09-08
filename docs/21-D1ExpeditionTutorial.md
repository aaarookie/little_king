# D1 · UE 5.8 三房连续远征新手教程

适用版本：UE 5.8.1，D1。D1 已由 C++ 完成，不要求新建地图、Widget 或其他二进制资产。本教程用于在编辑器中检查现有 `L_BattleTest`、`BP_ALKBattleGameMode` 和 `WBP_BattleHUD` 的连战行为。

## 1. 编译与打开

1. 先关闭 Unreal Editor，避免 Live Coding 锁住外部构建。
2. 在 PowerShell 进入项目目录，执行：

```powershell
& 'E:/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' little_kingEditor Win64 Development '-Project=E:/little_hero/little_king/little_king.uproject' -WaitMutex -NoHotReloadFromIDE
```

3. 构建出现 `Result: Succeeded` 后打开 `little_king.uproject`。
4. 打开 `Content/Maps/L_BattleTest`。World Settings 的 GameMode Override 应为 `BP_ALKBattleGameMode`。
5. 打开 `BP_ALKBattleGameMode` 的 Class Defaults，确认 Game Data 指向 `DA_GameData`。`Enable Expedition Flow` 默认开启；只有需要调试旧独立单场时才临时关闭，检查结束后恢复开启。

## 2. 结算按钮无需重做蓝图

现有 `WBP_BattleHUD` 只需保留名为 **`Btn_Restart`** 的按钮。C++ 在运行时会：

- 清除按钮旧的无效 OnClicked / OpenLevel 委托；
- 绑定远征命令；
- 找到按钮内部第一个 TextBlock 并写入“下一关”或“从头开始”；
- 点击后先更新 RunSubsystem，再重载当前战斗地图；
- 首次点击后禁用按钮，防止双击提交。

不要在蓝图里再连接 `Open Level`。如果希望清理旧图，打开 WBP 的 Graph，删除 `Btn_Restart → OnClicked → OpenLevel` 那条旧链即可；不清理也不会影响运行，因为原生初始化会清空旧点击委托。不要改按钮名，除非改成代码兼容的备用名 `Btn_Next`。

## 3. 完整走一轮三房

点击 Play。每一房都是全新的战场世界，所以都要重新布置骑士、法师、游侠三个英雄；部署不限时，三名全部就位后才能开始。

| 房间 | 敌方英雄 | 预期结算按钮 |
|---|---|---|
| 1 / 3 巡逻战 | 死灵法师 | 胜利：“下一关”；失败：“从头开始” |
| 2 / 3 精英战 | 死灵法师、骷髅巨人 | 胜利：“下一关”；失败：“从头开始” |
| 3 / 3 首领战 | 骷髅王、死灵法师 | 胜负都为“从头开始” |

正常击败敌方即可验证，也可以在战斗开始后按 `~` 打开控制台，用以下命令缩短检查：

```text
WinMatch 0
```

其中 `0` 表示玩家胜利。第一房点击“下一关”，等待同一地图重载，重新部署三英雄；第二房重复；第三房结束后点击“从头开始”，应回到第一房且英雄满血、历史清空。不要在部署阶段直接输入 WinMatch；先部署三英雄并开始，才能同时检查每房初始化是否完整。

## 4. 检查生命继承

在第一房开战后，让一名英雄受伤，再结束战斗。也可以先令玩家英雄无敌，定向扣骑士生命：

```text
InvulnerableHeroes 300
DamageUnit Hero_Knight 0 360
ListUnits
WinMatch 0
```

结算时按 `min(当前生命 + 最大生命 × 40%, 最大生命)` 恢复。点击“下一关”后重新部署骑士，他应从这个**恢复后生命**开始，不会满血重置，也不会在进入房间时再加一次 40%。如果第一房骑士结束前为 20%，第二房起始应为 60%；失能英雄第二房起始为 40%。

每房会重置银币、四张手牌的位置、抽牌队列、营地、佣兵、建筑、波次、弹道、计时与冷却。远征只继承玩家三英雄的生命/最大生命/特性和持有卡组。当前 D1 没有奖励，所以三房持有卡组相同，但每房用各自种子重新洗牌。

## 5. 检查失败与重开

在第一房或第二房开始战斗后输入：

```text
WinMatch 1
```

`1` 表示敌方胜利。结算按钮必须显示“从头开始”，点击后回到第一房，新 RunId、生满血英雄、空战斗历史。失败不能进入下一关。

第三房无论胜败都显示“从头开始”：胜利代表本轮 Completed，失败代表 Failed。D1 暂无远征总结界面，所以两种终态复用同一个重开动作。

## 6. 日志和排障

Output Log 搜索 `[Run]`，正常顺序类似：

```text
[Run] 新远征 ...，进入 1/3 Node_Battle01
[Run] 加载房间 1/3 Encounter_UndeadPatrol ...
[Run] 房间 Node_Battle01 胜利，可进入下一关
[Run] 结算按钮：下一关，重载 L_BattleTest
[Run] 下一关 2/3 Node_Battle02
```

常见问题：

| 现象 | 检查方法 |
|---|---|
| 仍显示“再来一局” | 确认按钮名为 Btn_Restart，文字是该按钮的子控件；关闭 PIE 后重新编译 C++，不要只热重载 |
| 点击无反应 | 确认阶段已经进入 Result；搜索日志是否有“HUD 未找到 Btn_Restart/Btn_Next” |
| 第一房不是死灵法师 | BP GameMode 必须配置 DA_GameData，且 Enable Expedition Flow 开启 |
| 第二房英雄满血 | 先确认上一房结算前确实低于 60%；不要在蓝图 OnMatchEnded 或 BeginPlay 额外回血 |
| 第二房仍是第一房 | 确认使用结算按钮；日志应先有“下一关 2/3”，再有“加载房间 2/3” |
| 核心单位表行漏填后行为异常 | 十三个稳定 ID 会由代码校正身份、被动、特性和建筑行为；数值仍来自 DT_Units，先检查行名和数值是否合法 |

## 7. 自动验证

关闭编辑器后可运行完整逻辑队列：

```powershell
& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests LittleKing' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/little_hero/little_king/Saved/Automation/D1' '-abslog=E:/little_hero/little_king/Saved/Logs/D1Automation.log'
```

再只读编译五个项目蓝图：

```powershell
& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -run=CompileAllBlueprints '-AllowListFile=Saved/Sprint5BlueprintAllowList.txt' -unattended -nop4 -nosplash -NullRHI '-abslog=E:/little_hero/little_king/Saved/Logs/D1BlueprintCompile.log'
```

以自动化 `index.json` 中每个 test 的 `state=Success`、`failed=0`、`notRun=0` 为准。命令行返回码不能代替报告检查。NullRHI 没有真实 GameViewport，因此最终仍需做第 3～5 节的一次短 PIE；不需要打包或做硬件专项。

## 8. 当前边界与协作

D1 当时交付的是固定线性三房。D2 后续完成遭遇数据化和英雄永久状态，D3 已加入三选一奖励及原生界面，操作见 [22](22-D2EncounterTutorial.md) / [23](23-D3RewardTutorial.md)。当前仍没有选路、休息、事件、远征总结或存档；D4 扩成分支路线，D5 增加安全节点存档与总结。

用户当前维护 `Content/Data/DT_Units.uasset`、`DT_Traits.uasset`、`Content/blueprint/GA_MageNova.uasset`、`GA_RangerShot.uasset`。助手的 D1 改动没有重存这些二进制资产。另一位协作者要改同一资产前，先在 [18](18-DungeonChangeLog.md) 登记归属，避免 `.uasset` 无法文本合并。
