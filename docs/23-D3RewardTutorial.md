# D3 · 房间奖励界面与验收（UE 5.8）

适用版本：UE 5.8.1，D3。奖励逻辑和界面均已完成，打开项目即可使用；不需要创建 `WBP_RunReward`，也不需要重存现有 `WBP_BattleHUD`。

## 1. 玩家看到的流程

第一、二房胜利后，结算界面上方自动弹出全屏奖励层：

- 顶部显示已完成房间、总房间数和本房奖励档；
- 中间显示实际存在的 1～3 个候选，通常为 2～3 个；
- 已有卡升级会显示等级以及生命、攻击的真实升级前后值；
- 新卡显示费用、近战/远程定位和唯一卡规则；
- 有卡牌图标时显示图标，没有资源的骷髅卡使用“新”字占位；
- 底部显示本次远征当前牌组和升级等级，并提供“跳过奖励”。

遮罩会拦截战场误点。“下一关”在奖励未处理时保持禁用；选择或跳过成功后，奖励层关闭，按钮恢复可用。第三房胜利、任意房失败和没有有效候选时不弹面板。

## 2. 当前奖励规则

| 类型 | 内容 | 后续房间效果 |
|---|---|---|
| 卡牌强化 | 从已持有的单位/建筑卡中升级一张 | 该卡出牌生成单位的生命与攻击乘以 `1.1^等级` |
| 新卡入队 | 未持有时可获得骷髅兵或骷髅射手 | 加入本轮唯一牌组，费用 1，无美术时使用文字卡 |
| 跳过 | 消费本房奖励批次，不改变牌组 | 立即解锁下一关，不能回头领取 |

法术暂不进入数值升级池。升级只作用于远征玩家通过该卡生成的单位；敌方、波次、召唤、兵营产兵和共享数据资产不会被修改。相同 `CardId` 永远只有一张，获得过的骷髅卡不会再次作为“新卡”候选。

## 3. 快速 PIE 验收

1. 关闭编辑器后构建 `little_kingEditor / Win64 / Development`，再打开 `Content/Maps/L_BattleTest`。
2. Play，部署骑士、法师和游侠并开始战斗。
3. 战斗开始后按 `~`，输入 `WinMatch 0`。
4. 确认奖励层自动出现，显示 2～3 张非空奖励卡；背景战场不能被点击，“下一关”禁用。
5. 选择一张升级卡。面板应关闭，“下一关”恢复可用。进入下一房并打出该卡，日志应出现 `[Run] 卡升级应用`，生命和攻击与面板预览一致。
6. 第二房胜利后领取骷髅新卡。进入第三房后通过过牌可抽到 1 费文字卡，并正常部署对应单位。
7. 另开一轮，在第一房点“跳过奖励”，确认牌组不变且仍可进入下一关。
8. 第三房胜利确认不再弹奖励，结算按钮显示“从头开始”。

候选使用独立确定性种子。关闭再打开界面、分辨率变化或 UI 刷新都不会重抽候选。

## 4. 界面适配

默认面板的设计尺寸为 1080×650，并通过 `ScaleBox` 仅向下缩放；720p 与 1080p 都能完整容纳。三张奖励卡等宽排列，缺少候选时相应卡位折叠。颜色之外始终有“卡牌强化 / 新卡入队”、卡名和完整数值文字，因此不会只依赖颜色传达含义。

奖励按钮在点击瞬间全部禁用，RunSubsystem 原子消费成功后才关闭；失败会恢复按钮并显示提示。HUD 晚创建、重新加入视口或错过结算广播时，`RefreshRewardPanel` 会从仍然存在的 PendingRewardBatch 恢复同一面板。

## 5. 后续用 Widget Blueprint 换肤

当前阶段不需要做这一步。需要正式美术时可以保留现有逻辑，只替换布局：

1. 新建 Widget Blueprint，父类选择 `ULKRunRewardWidget`。
2. 在 `WBP_BattleHUD` 的 Class Defaults 中，把 `Reward Widget Class` 指向该子类。
3. 如果子类保持空 Designer，会继续使用原生布局；如果建立自定义根控件，在 `OnRewardDataReadyBP` 中通过 `OwnerHUD` 读取候选数量、`GetRewardOptionText`、`GetCardDefinition` 和 `GetCardIcon`。
4. 自定义奖励按钮调用 `ChooseOption(Index)`，跳过按钮调用 `SkipReward()`；返回成功后父 HUD 会关闭面板并解锁下一关。

`WBP_BattleHUD.OnRunRewardReadyBP` 仍作为奖励出现后的附加表现通知保留，可用于播放动画或埋点。不要再在该事件中创建第二个奖励面板。

## 6. 常见问题

| 现象 | 检查方法 |
|---|---|
| 胜利后没有奖励层 | 必须是远征第一或第二房、玩家胜利且存在候选；检查日志是否有“生成奖励”和 `[RunUI] 奖励面板已显示` |
| 奖励层出现但下一关还能点击 | 确认使用现有 `Btn_Restart` / `Btn_Next`，HUD 会按 `HasPendingRewardChoice` 禁用它 |
| 骷髅卡没有图片 | 当前阶段预期使用“新”字和文字说明，不需要补美术 |
| 升级值与进场单位不一致 | 确认该单位由玩家打出的目标卡生成；波次、召唤和兵营产兵不吃卡牌等级 |
| 自定义蓝图显示空白 | 自建 Designer 后必须实现 `OnRewardDataReadyBP`；也可以删除自定义根控件继续使用原生布局 |
| 点击两次得到两份奖励 | 属于错误；UI 会先锁按钮，RunSubsystem 还会用批次 GUID 和阶段做第二层拒绝 |

## 7. 自动验证

关闭编辑器后运行：

```powershell
& 'E:/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' little_kingEditor Win64 Development '-Project=E:/little_hero/little_king/little_king.uproject' -WaitMutex -NoHotReloadFromIDE

& 'E:/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/little_hero/little_king/little_king.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests LittleKing' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/little_hero/little_king/Saved/Automation/D3UIFinal' '-abslog=E:/little_hero/little_king/Saved/Logs/D3UIAutomation.log'
```

自动化覆盖奖励生成/消费、升级与加牌跨房、无重复、Boss 不发奖，以及真实项目 HUD 的面板打开、跳过后关闭和下一关解锁。NullRHI 不评价字体与最终像素观感，第 3 节保留为一次短 PIE 视觉验收；不要求打包、硬件、音效或正式美术检查。
