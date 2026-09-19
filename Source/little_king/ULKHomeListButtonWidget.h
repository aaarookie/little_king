#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LKHomeTypes.h"
#include "ULKHomeListButtonWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class USizeBox;

/**
 * 家园面板里的通用列表按钮（H1/H2）：一行 = 一个按钮。
 * 面板行与操作按钮共用同一个控件，避免为每一行写一套 UFUNCTION 回调。
 */
UCLASS()
class ULKHomeListButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 点击回调（参数 = 行/操作在模型里的下标） */
	DECLARE_DELEGATE_OneParam(FOnHomeListClicked, int32);
	FOnHomeListClicked OnHomeListClicked;

	void SetupRow(int32 InIndex, const FLKHomePanelRow& Row, bool bSelected);
	void SetupAction(int32 InIndex, const FLKHomePanelAction& Action);
    void SetCompactRow();
	int32 GetModelIndex() const { return Index; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION() void HandleClicked();

private:
	void BuildNativeTree();

	UPROPERTY(Transient) TObjectPtr<UButton> Button;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LabelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ValueText;
	UPROPERTY(Transient) TObjectPtr<USizeBox> BadgeSize;
	UPROPERTY(Transient) TObjectPtr<UBorder> Badge;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BadgeText;
	int32 Index = INDEX_NONE;
	bool bBuilt = false;
};
