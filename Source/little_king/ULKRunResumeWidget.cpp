#include "ULKRunResumeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

#include "ALKBattleGameMode.h"
#include "ULKBattleHUDWidget.h"
#include "ULKRunSubsystem.h"

namespace
{
	const FLinearColor BackdropColor(0.006f, 0.012f, 0.027f, 0.88f);
	const FLinearColor PanelColor(0.025f, 0.045f, 0.075f, 0.99f);
	const FLinearColor GoldColor(0.96f, 0.69f, 0.22f, 1.f);
	const FLinearColor PaleColor(0.88f, 0.92f, 0.96f, 1.f);
	const FLinearColor MutedColor(0.58f, 0.67f, 0.76f, 1.f);
	const FLinearColor ButtonColor(0.13f, 0.16f, 0.20f, 1.f);
	const FLinearColor ButtonHover(0.20f, 0.24f, 0.30f, 1.f);

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Center)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void StyleButton(UButton* Button, const FLinearColor& Normal, const FLinearColor& Hovered)
	{
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(Normal);
		Style.Hovered.TintColor = FSlateColor(Hovered);
		Style.Pressed.TintColor = FSlateColor(Hovered * 0.82f);
		Button->SetStyle(Style);
	}
}

void ULKRunResumeWidget::InitializeResume(ULKBattleHUDWidget* InOwnerHUD)
{
	OwnerHUD = InOwnerHUD;
}

TSharedRef<SWidget> ULKRunResumeWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildNativeTree();
	}
	return Super::RebuildWidget();
}

void ULKRunResumeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	RefreshResume();
}

void ULKRunResumeWidget::BuildNativeTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ResumeRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(BackdropColor);
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(Backdrop))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
		LayoutSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(Scale))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Center);
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
		LayoutSlot->SetPadding(FMargin(28.f));
	}

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelSize->SetWidthOverride(720.f);
	PanelSize->SetHeightOverride(360.f);
	Scale->SetContent(PanelSize);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResumePanel"));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(36.f, 30.f));
	PanelSize->SetContent(Panel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Content);

	TitleText = MakeText(WidgetTree, TEXT("Title"), 38, GoldColor);
	Content->AddChildToVerticalBox(TitleText);

	SummaryText = MakeText(WidgetTree, TEXT("Summary"), 20, PaleColor);
	if (UVerticalBoxSlot* LayoutSlot = Content->AddChildToVerticalBox(SummaryText))
	{
		LayoutSlot->SetPadding(FMargin(0.f, 22.f, 0.f, 26.f));
	}

	PrimaryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Btn_Primary"));
	StyleButton(PrimaryButton, ButtonColor, ButtonHover);
	PrimaryButton->OnClicked.AddDynamic(this, &ULKRunResumeWidget::HandlePrimaryClicked);
	Content->AddChildToVerticalBox(PrimaryButton);
	PrimaryLabel = MakeText(WidgetTree, TEXT("PrimaryLabel"), 20, PaleColor);
	PrimaryButton->SetContent(PrimaryLabel);
}

void ULKRunResumeWidget::RefreshResume()
{
	const ALKBattleGameMode* GM = OwnerHUD ? OwnerHUD->GetBattleGameMode() : nullptr;
	if (!GM) { return; }

	bool bTerminal = false;
	if (GM->IsRecoveryTerminal())
	{
		bTerminal = true;
		if (TitleText) { TitleText->SetText(FText::FromString(TEXT("远征总结"))); }
		if (SummaryText) { SummaryText->SetText(GM->GetRunSummaryText()); }
		if (PrimaryLabel) { PrimaryLabel->SetText(FText::FromString(TEXT("开始新远征"))); }
	}
	else
	{
		if (TitleText) { TitleText->SetText(FText::FromString(TEXT("远征进行中"))); }
		if (SummaryText)
		{
			const ULKRunSubsystem* Run = OwnerHUD->GetWorld()
				? OwnerHUD->GetWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>() : nullptr;
			const int32 Room = Run ? Run->GetCurrentRoomIndex() : 0;
			SummaryText->SetText(FText::FromString(FString::Printf(
				TEXT("上次退出于第 %d 战结算后。继续后领取待选奖励并选择下一排节点。"), Room)));
		}
		if (PrimaryLabel) { PrimaryLabel->SetText(FText::FromString(TEXT("继续远征"))); }
	}
	OnResumeDataReadyBP(bTerminal);
}

void ULKRunResumeWidget::HandlePrimaryClicked()
{
	if (!OwnerHUD) { return; }
	if (OwnerHUD->GetBattleGameMode() && OwnerHUD->GetBattleGameMode()->IsRecoveryTerminal())
	{
		// 终态：开始新远征（RestartRun + 重载战场）。
		if (OwnerHUD->StartNewRunFromRecovery())
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	// 选择中断：关闭本面板，让 HUD 弹出奖励/节点选择面板。
	OwnerHUD->ContinueFromRecoveryJunction();
}
