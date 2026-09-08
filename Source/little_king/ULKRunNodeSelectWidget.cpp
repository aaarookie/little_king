#include "ULKRunNodeSelectWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "ALKBattleGameMode.h"
#include "LKRunTypes.h"
#include "ULKBattleHUDWidget.h"

namespace
{
	const FLinearColor BackdropColor(0.006f, 0.012f, 0.027f, 0.86f);
	const FLinearColor PanelColor(0.025f, 0.045f, 0.075f, 0.99f);
	const FLinearColor CardColor(0.055f, 0.085f, 0.13f, 1.f);
	const FLinearColor GoldColor(0.96f, 0.69f, 0.22f, 1.f);
	const FLinearColor PaleColor(0.88f, 0.92f, 0.96f, 1.f);
	const FLinearColor MutedColor(0.58f, 0.67f, 0.76f, 1.f);

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
		Style.Disabled.TintColor = FSlateColor(FLinearColor(Normal.R, Normal.G, Normal.B, 0.42f));
		Button->SetStyle(Style);
	}
}

void ULKRunNodeSelectWidget::InitializeNodeSelect(ULKBattleHUDWidget* InOwnerHUD)
{
	OwnerHUD = InOwnerHUD;
}

TSharedRef<SWidget> ULKRunNodeSelectWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildNativeTree();
	}
	return Super::RebuildWidget();
}

void ULKRunNodeSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	RefreshNodeSelect();
	if (OptionButtons.IsValidIndex(0) && OptionButtons[0]->GetVisibility() == ESlateVisibility::Visible)
	{
		OptionButtons[0]->SetKeyboardFocus();
	}
}

void ULKRunNodeSelectWidget::BuildNativeTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("NodeRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(BackdropColor);
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(Backdrop))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
		LayoutSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ResponsiveScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(Scale))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Center);
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
		LayoutSlot->SetPadding(FMargin(28.f));
	}

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(860.f);
	PanelSize->SetHeightOverride(420.f);
	Scale->SetContent(PanelSize);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NodePanel"));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(34.f, 26.f));
	PanelSize->SetContent(Panel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Panel->SetContent(Content);

	UTextBlock* Title = MakeText(WidgetTree, TEXT("Title"), 34, GoldColor);
	Title->SetText(FText::FromString(TEXT("选择下一站")));
	Content->AddChildToVerticalBox(Title);

	UTextBlock* SubTitle = MakeText(WidgetTree, TEXT("SubTitle"), 16, MutedColor);
	SubTitle->SetText(FText::FromString(TEXT("只能看到下一排节点；休息营地可恢复 30% 最大生命")));
	Content->AddChildToVerticalBox(SubTitle);

	UHorizontalBox* Options = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Options"));
	Content->AddChildToVerticalBox(Options);

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
			*FString::Printf(TEXT("Btn_Node%d"), Index));
		StyleButton(Button, CardColor, FLinearColor(0.10f, 0.16f, 0.24f, 1.f));
		if (UHorizontalBoxSlot* LayoutSlot = Options->AddChildToHorizontalBox(Button))
		{
			LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LayoutSlot->SetPadding(FMargin(12.f, 18.f));
			LayoutSlot->SetVerticalAlignment(VAlign_Center);
		}

		UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>();
		Button->SetContent(Card);

		UTextBlock* NodeTitle = MakeText(WidgetTree, *FString::Printf(TEXT("NodeTitle%d"), Index), 26, PaleColor);
		if (UVerticalBoxSlot* LayoutSlot = Card->AddChildToVerticalBox(NodeTitle)) { LayoutSlot->SetPadding(FMargin(14.f, 16.f, 14.f, 6.f)); }

		UTextBlock* Subtitle = MakeText(WidgetTree, *FString::Printf(TEXT("NodeSubtitle%d"), Index), 17, MutedColor, ETextJustify::Center);
		Subtitle->SetLineHeightPercentage(1.2f);
		if (UVerticalBoxSlot* LayoutSlot = Card->AddChildToVerticalBox(Subtitle))
		{
			LayoutSlot->SetPadding(FMargin(18.f, 2.f, 18.f, 18.f));
		}

		OptionButtons.Add(Button);
		OptionTitleTexts.Add(NodeTitle);
		OptionSubtitleTexts.Add(Subtitle);
	}

	OptionButtons[0]->OnClicked.AddDynamic(this, &ULKRunNodeSelectWidget::HandleOption0Clicked);
	OptionButtons[1]->OnClicked.AddDynamic(this, &ULKRunNodeSelectWidget::HandleOption1Clicked);

	StatusText = MakeText(WidgetTree, TEXT("Status"), 16, MutedColor, ETextJustify::Left);
	Content->AddChildToVerticalBox(StatusText);
}

void ULKRunNodeSelectWidget::RefreshNodeSelect()
{
	if (!OwnerHUD) { return; }
	const int32 Count = OwnerHUD->GetNextNodeCount();
	DisplayedNodeCount = FMath::Clamp(Count, 0, 2);
	if (OptionButtons.Num() != 2)
	{
		OnNodeDataReadyBP(DisplayedNodeCount);
		return;
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const bool bVisible = Index < DisplayedNodeCount;
		OptionButtons[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bVisible) { continue; }
		OptionTitleTexts[Index]->SetText(OwnerHUD->GetNextNodeTitle(Index));
		OptionSubtitleTexts[Index]->SetText(OwnerHUD->GetNextNodeSubtitle(Index));
	}

	if (StatusText && Count == 0)
	{
		StatusText->SetText(FText::FromString(TEXT("没有可选节点，请返回结果界面操作")));
	}
	SetInteractionEnabled(Count > 0);
	OnNodeDataReadyBP(DisplayedNodeCount);
}

void ULKRunNodeSelectWidget::SetInteractionEnabled(bool bEnabled)
{
	bInteractionLocked = !bEnabled;
	for (UButton* Button : OptionButtons)
	{
		if (Button) { Button->SetIsEnabled(bEnabled); }
	}
}

void ULKRunNodeSelectWidget::TrySelect(int32 Index)
{
	if (bInteractionLocked || !OwnerHUD || Index < 0 || Index >= DisplayedNodeCount) { return; }
	SetInteractionEnabled(false);
	const ELKNodeSelectionResult Result = OwnerHUD->SelectNextNode(Index);
	if (Result == ELKNodeSelectionResult::RestResolved)
	{
		// 休息完成：回血已应用，继续显示再下一排。
		if (StatusText) { StatusText->SetText(FText::FromString(TEXT("已休息：全体英雄恢复 30% 生命，可继续选择下一排"))); }
		RefreshNodeSelect();
	}
	else if (Result == ELKNodeSelectionResult::BattleEntered)
	{
		// 进入战斗：世界即将重载，本面板随旧世界销毁。
	}
	else
	{
		if (StatusText) { StatusText->SetText(FText::FromString(TEXT("该节点不可选（状态已变化），请重新选择"))); }
		RefreshNodeSelect();
	}
}

void ULKRunNodeSelectWidget::HandleOption0Clicked() { TrySelect(0); }
void ULKRunNodeSelectWidget::HandleOption1Clicked() { TrySelect(1); }
