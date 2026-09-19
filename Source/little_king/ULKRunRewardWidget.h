#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ULKRunRewardWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UTextBlock;
class ULKBattleHUDWidget;
class UVerticalBox;
class UScrollBox;
class UUniformGridPanel;
class ULKHomeListButtonWidget;

/**
 * D3 默认奖励界面。纯 C++ 构建完整可用的 UMG 树，零额外资产也能显示；
 * 美术协作者可创建本类的 Widget Blueprint 子类并在 BattleHUD 上替换 RewardWidgetClass。
 */
UCLASS(Blueprintable, BlueprintType)
class ULKRunRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeReward(ULKBattleHUDWidget* InOwnerHUD);
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void RefreshReward();
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	bool ChooseOption(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	bool SkipReward();
    UFUNCTION(BlueprintCallable, Category = "LK|Run|UI") bool ChooseReplacement(int32 CardIndex);
    UFUNCTION(BlueprintCallable, Category = "LK|Run|UI") bool ConfirmReplacements();
    UFUNCTION(BlueprintPure, Category = "LK|Run|UI") bool IsChoosingReplacement() const { return PendingOptionIndex != INDEX_NONE || bReducingLegacyDeck; }

	UFUNCTION(BlueprintPure, Category = "LK|Run|UI")
	int32 GetDisplayedOptionCount() const { return DisplayedOptionCount; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** 自定义 Widget Blueprint 子类可在这里读取 OwnerHUD 并刷新自己的控件。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Run|UI")
	void OnRewardDataReadyBP(int32 OptionCount);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "LK|Run|UI")
	TObjectPtr<ULKBattleHUDWidget> OwnerHUD;

private:
	void BuildNativeTree();
	void TryChoose(int32 Index);
	void SetInteractionEnabled(bool bEnabled);
	FText BuildOptionDetail(int32 Index) const;
	FText BuildDeckSummary() const;
    void ShowReplacementChoices();
    UFUNCTION() void HandleConfirmReplacements();
    UPROPERTY(Transient) TObjectPtr<UButton> ConfirmReplacementButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ReplacementBudget;
    TArray<FName> SelectedReplacementIds;

	UFUNCTION() void HandleOption0Clicked();
	UFUNCTION() void HandleOption1Clicked();
	UFUNCTION() void HandleOption2Clicked();
	UFUNCTION() void HandleSkipClicked();
	UFUNCTION() void HandleReturnHomeClicked();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> ProgressText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DeckSummaryText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> SkipButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SkipLabel;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> Options;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> ReplacementPanel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ReplacementTitle;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> ReplacementList;
    UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> ReplacementGrid;
    TArray<FName> ReplacementCardIds;
    int32 PendingOptionIndex = INDEX_NONE;
    bool bReducingLegacyDeck = false;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> OptionButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OptionKindTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OptionTitleTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OptionDetailTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> OptionIcons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OptionPlaceholderTexts;

	int32 DisplayedOptionCount = 0;
	bool bInteractionLocked = false;
};
