#include "ULKRunResumeWidget.h"
#include "ULKJourneyPresentationSubsystem.h"
#include "LKPresentationStyle.h"

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
#include "LKHomeContent.h"

namespace
{
	const FLinearColor BackdropColor = LKPresentationStyle::Ink().CopyWithNewOpacity(.92f);
	const FLinearColor PanelColor = LKPresentationStyle::Panel();
	const FLinearColor GoldColor = LKPresentationStyle::Gold();
	const FLinearColor PaleColor = LKPresentationStyle::Paper();
	const FLinearColor MutedColor = LKPresentationStyle::Muted();
	const FLinearColor ButtonColor = LKPresentationStyle::Card();
	const FLinearColor ButtonHover = LKPresentationStyle::Hover();

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Center)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(LKPresentationStyle::Font(Font.Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void StyleButton(UButton* Button, const FLinearColor& Normal, const FLinearColor& Hovered)
	{
        LKPresentationStyle::StyleButton(Button);
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
	Super::NativeConstruct(); ULKJourneyPresentationSubsystem::Reveal(this);
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
		if (PrimaryLabel) { PrimaryLabel->SetText(FText::FromString(LKHomeContent::DoesMapExist(GM->GetHomeMapName()) ? TEXT("返回家园") : TEXT("开始新远征"))); }
	}
	else
	{
		if (TitleText) { TitleText->SetText(FText::FromString(TEXT("远征进行中"))); }
		if (SummaryText)
		{
			const ULKRunSubsystem* Run = OwnerHUD->GetWorld()
				? OwnerHUD->GetWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>() : nullptr;
			const int32 Room = Run ? Run->GetRunState().BattleHistory.Num() : 0;
			SummaryText->SetText(FText::FromString(FString::Printf(
				TEXT("已完成 %d 场战斗。继续后回到保存的奖励、地图或服务节点。"), Room)));
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
		// 正式终态返回家园；缺少家园地图的独立测试保留重开入口。
		ALKBattleGameMode* GM = OwnerHUD->GetBattleGameMode();
		if (LKHomeContent::DoesMapExist(GM->GetHomeMapName()) ? GM->ReturnToHome() : OwnerHUD->StartNewRunFromRecovery())
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	// 选择中断：关闭本面板，让 HUD 弹出奖励/节点选择面板。
	OwnerHUD->ContinueFromRecoveryJunction();
}
