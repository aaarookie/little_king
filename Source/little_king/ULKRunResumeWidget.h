#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ULKRunResumeWidget.generated.h"

class UButton;
class UTextBlock;
class ULKBattleHUDWidget;

/**
 * D5 远征终态/恢复面板：冷启动恢复时显示。
 * 场景 A：路线/奖励选择中断 → 面板给出入口按钮，点击后由 HUD 弹奖励或节点选择面板；
 * 场景 B：通关/失败终态 → 显示摘要并允许"开始新远征"。
 * 纯 C++ 原生树；可创建本类 Widget Blueprint 子类在 BattleHUD 上替换 ResumeWidgetClass 换肤。
 */
UCLASS(Blueprintable, BlueprintType)
class ULKRunResumeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeResume(ULKBattleHUDWidget* InOwnerHUD);
	/** 重读阶段与摘要文本并刷新（阶段可能已推进） */
	UFUNCTION(BlueprintCallable, Category = "LK|Run|UI")
	void RefreshResume();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** 自定义 Widget Blueprint 子类可在这里读取 OwnerHUD 并刷新自己的控件。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Run|UI")
	void OnResumeDataReadyBP(bool bTerminal);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "LK|Run|UI")
	TObjectPtr<ULKBattleHUDWidget> OwnerHUD;

private:
	void BuildNativeTree();

	UFUNCTION() void HandlePrimaryClicked();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SummaryText;
	UPROPERTY(Transient) TObjectPtr<UButton> PrimaryButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PrimaryLabel;
};
