#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKTypes.h"
#include "ULKBattleHUDWidget.generated.h"

class ALKPlayerController;
class ALKBattleGameState;
class ALKBattleGameMode;
class ULKCardDefinition;
class ULKDeckState;
class ULKSilverComponent;
class ULKRunRewardWidget;
class ULKRunNodeSelectWidget;
class ULKRunResumeWidget;
class UButton;

/**
 * 战斗 HUD 基类：C++ 逻辑 + 蓝图表现（行业标准分工）。
 * C++ 自动绑定银币/手牌/阶段/胜负/放置状态事件，蓝图只需"覆写"下方事件刷新界面。
 *
 * 用法：
 *  1) 创建 Widget Blueprint，父类选 ULKBattleHUDWidget
 *  2) BP_ALKBattleGameMode 类默认值 -> HUDWidgetClass = 该 Widget
 */
UCLASS(Blueprintable, BlueprintType)
class ULKBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	int32 LastWholeSilver = -1;

	// ---------- 蓝图实现事件（BP 里覆写，刷新界面） ----------
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnPhaseChanged(ELKGamePhase NewPhase);

	/** bPlayable[i] = false 表示该卡当前不可打出（空槽/银币不足/阶段错误，不因法师死亡灰卡） */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnHandChanged(const TArray<FName>& Hand, const TArray<int32>& Costs, const TArray<bool>& bPlayable);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnSilverChanged(float Silver, float Cap, float Delta);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnMatchEnded(ELKTeam Winner);

	/** D3 房间胜利奖励通知：原生面板已由 C++ 创建；蓝图可在这里追加表现（OptionCount = 可选数量）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Run")
	void OnRunRewardReadyBP(int32 OptionCount);

	/** D4 节点选择通知：打完并领完奖励后触发（NodeCount = 下一排可选节点数，1~2） */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Run")
	void OnNodeSelectReadyBP(int32 NodeCount);

	/** S5 飘字数据链：任何伤害/治疗生效时触发（Amount 恒为正；bIsHeal 区分伤害/治疗） */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnDamageEventBP(FVector WorldLocation, float Amount, bool bIsHeal);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnPlacementStateChanged(bool bPlacing, ELKPlacementMode Mode, int32 HandIndex, FName ItemId);

	UFUNCTION(BlueprintImplementableEvent, Category = "LK|HUD")
	void OnPlayResult(ELKPlayResult Result);

	// ---------- 便捷查询（蓝图里直接用，省得 cast 链） ----------
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ALKPlayerController* GetLKPlayerController() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ULKDeckState* GetDeck() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ULKSilverComponent* GetSilverComp() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ALKBattleGameState* GetBattleGameState() const;

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ALKBattleGameMode* GetBattleGameMode() const;

	/** D1 结算页唯一操作：前两房胜利进入下一关，其余情况从头开始。 */
	UFUNCTION(BlueprintCallable, Category = "LK|HUD")
	bool RequestResultAction();

	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	FText GetResultActionLabel() const;

	/** 按 CardId 查卡牌定义（图标/名称/费用），HUD 显示卡面用 */
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	ULKCardDefinition* GetCardDefinition(FName CardId) const;

	/** 按 CardId 直接取卡牌图标（已加载的 Texture2D；无卡/无图标返回空）——免去蓝图软引用加载 */
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	class UTexture2D* GetCardIcon(FName CardId) const;

	/** S5 飘字定位：世界坐标 -> 视口坐标（失败返回 false，通常不会） */
	UFUNCTION(BlueprintPure, Category = "LK|HUD")
	bool WorldToScreen(FVector WorldLocation, FVector2D& OutScreenLocation) const;

	// ---------- D3 奖励便捷入口（BP 奖励面板直接调用） ----------
	UFUNCTION(BlueprintPure, Category = "LK|Run")
	bool HasPendingRewardChoice() const;

	UFUNCTION(BlueprintPure, Category = "LK|Run")
	int32 GetPendingRewardCount() const;

	/** 第 Index 个选项的按钮文案（如：升级「剑士」→ Lv2：攻击/生命 +10%；获得新卡「骷髅兵」（1 费）） */
	UFUNCTION(BlueprintPure, Category = "LK|Run")
	FText GetRewardOptionText(int32 Index) const;

	/** 原子领取第 Index 个奖励；成功后自动刷新结算按钮（"下一关"恢复可用） */
	UFUNCTION(BlueprintCallable, Category = "LK|Run")
	bool ChooseRunReward(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "LK|Run")
	bool SkipRunReward();

	/** 默认原生奖励面板是否正在显示；自动化与蓝图定制都可查询。 */
	UFUNCTION(BlueprintPure, Category = "LK|Run|UI")
	bool IsRewardPanelOpen() const;

	/** 幂等同步待选奖励与面板；HUD 晚创建/重新加入视口时也可主动恢复。 */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void RefreshRewardPanel();

	// ---------- D4 节点选择便捷入口（原生面板与蓝图共用） ----------
	UFUNCTION(BlueprintPure, Category = "LK|Run")
	bool CanSelectNextNode() const;

	UFUNCTION(BlueprintPure, Category = "LK|Run")
	int32 GetNextNodeCount() const;

	UFUNCTION(BlueprintPure, Category = "LK|Run")
	FName GetNextNodeId(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "LK|Run")
	FText GetNextNodeTitle(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "LK|Run")
	FText GetNextNodeSubtitle(int32 Index) const;

	/** 选择第 Index 个下一排节点；战斗类进入该房（自动重载），休息类回血后返回 RestResolved */
	UFUNCTION(BlueprintCallable, Category = "LK|Run")
	ELKNodeSelectionResult SelectNextNode(int32 Index);

	UFUNCTION(BlueprintPure, Category = "LK|Run|UI")
	bool IsNodeSelectPanelOpen() const;

	/** 幂等同步节点选择面板：有可选节点则显示，否则关闭 */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void RefreshNodeSelectPanel();

	// ---------- D5 恢复/终态入口 ----------
	/** 冷启动恢复时 HUD 显示对应面板（奖励/节点选择/终态摘要） */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void TryShowRecoveryPanels();

	/** 选择中断恢复：关闭摘要面板并弹出奖励/节点选择（幂等） */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void ContinueFromRecoveryJunction();

	/** 终态摘要后开始新远征（RestartRun + 重载） */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	bool StartNewRunFromRecovery();

	UFUNCTION(BlueprintPure, Category = "LK|Run|UI")
	bool IsResumePanelOpen() const;

	/** 幂等同步恢复/终态摘要面板 */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void RefreshResumePanel();

protected:
	// ---------- C++ 事件转发（绑定引擎/游戏事件 -> 调用蓝图事件） ----------
	UFUNCTION()
	void HandlePhaseChanged(ELKGamePhase NewPhase);

	UFUNCTION()
	void HandleHandChanged();

	UFUNCTION()
	void HandleSpellLockChanged(bool bUnlocked);

	UFUNCTION()
	void HandleSilverChanged(float NewSilver, float Delta);

	UFUNCTION()
	void HandleMatchEnded(ELKTeam Winner);

	UFUNCTION()
	void HandleDamageEvent(FVector WorldLocation, float Amount, bool bIsHeal);

	UFUNCTION()
	void HandlePlacementStateChanged(bool bPlacing, ELKPlacementMode Mode, int32 HandIndex, FName ItemId);

	UFUNCTION()
	void HandlePlayResult(ELKPlayResult Result);

	UFUNCTION()
	void HandleResultActionClicked();

	void BindEvents();
	void UnbindEvents();
	void PushInitialState();
	void BindResultActionButton();
	void RefreshResultActionButton();
	void ShowRewardPanel();
	void CloseRewardPanel();
	/** 结果流程总同步：奖励面板 → 节点选择面板 → 结算按钮（顺序敏感） */
	void SyncResultPanels();
	void ShowNodeSelectPanel();
	void CloseNodeSelectPanel();
	void ShowResumePanel();
	void CloseResumePanel();

	/** 兼容现有 WBP_BattleHUD 中名为 Btn_Restart 的结算按钮。 */
	UPROPERTY(Transient)
	TObjectPtr<UButton> ResultActionButton;

	/** 可选的 ULKRunRewardWidget 蓝图子类；未设置时使用本轮提供的原生完整界面。 */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Run|UI")
	TSubclassOf<ULKRunRewardWidget> RewardWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULKRunRewardWidget> RewardWidget;

	/** 可选的 ULKRunNodeSelectWidget 蓝图子类；未设置时使用原生界面。 */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Run|UI")
	TSubclassOf<ULKRunNodeSelectWidget> NodeSelectWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULKRunNodeSelectWidget> NodeSelectWidget;

	/** 可选的 ULKRunResumeWidget 蓝图子类；未设置时使用原生摘要界面。 */
	UPROPERTY(EditDefaultsOnly, Category = "LK|Run|UI")
	TSubclassOf<ULKRunResumeWidget> ResumeWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULKRunResumeWidget> ResumeWidget;
};
