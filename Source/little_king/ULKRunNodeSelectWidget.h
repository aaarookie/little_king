#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKTypes.h"
#include "ULKRunNodeSelectWidget.generated.h"

class UButton;
class UTextBlock;
class ULKBattleHUDWidget;

/**
 * D4 默认节点选择界面（打完并领完奖励后出现）。
 * 纯 C++ 构建完整 UMG 树，零额外资产可用；只展示"下一排"可选节点，看不到后续路线。
 * 美术协作者可创建本类的 Widget Blueprint 子类并在 BattleHUD 上替换 NodeSelectWidgetClass。
 */
UCLASS(Blueprintable, BlueprintType)
class ULKRunNodeSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeNodeSelect(ULKBattleHUDWidget* InOwnerHUD);
	/** 重读下一排选项与状态（休息结算后调用，显示再下一排） */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void RefreshNodeSelect();

	UFUNCTION(BlueprintPure, Category = "LK|Run|UI")
	int32 GetDisplayedNodeCount() const { return DisplayedNodeCount; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** 自定义 Widget Blueprint 子类可在这里读取 OwnerHUD 并刷新自己的控件。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Run|UI")
	void OnNodeDataReadyBP(int32 NodeCount);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "LK|Run|UI")
	TObjectPtr<ULKBattleHUDWidget> OwnerHUD;

private:
	void BuildNativeTree();
	void TrySelect(int32 Index);
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION() void HandleOption0Clicked();
	UFUNCTION() void HandleOption1Clicked();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> OptionButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OptionTitleTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OptionSubtitleTexts;

	int32 DisplayedNodeCount = 0;
	bool bInteractionLocked = false;
};
