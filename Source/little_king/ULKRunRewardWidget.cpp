#include "ULKRunRewardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"

#include "ALKBattleGameMode.h"
#include "LKRunTypes.h"
#include "ULKBattleHUDWidget.h"
#include "ULKCardDefinition.h"
#include "ULKRunSubsystem.h"
#include "LKCardPresentation.h"
#include "LKCardRules.h"
#include "ULKSilverComponent.h"
#include "ULKHomeListButtonWidget.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Kismet/GameplayStatics.h"

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

	void AddVerticalSpace(UWidgetTree* Tree, UVerticalBox* Box, float Height)
	{
		USpacer* Spacer = Tree->ConstructWidget<USpacer>();
		Spacer->SetSize(FVector2D(1.f, Height));
		Box->AddChildToVerticalBox(Spacer);
	}

	FString Number(float Value)
	{
		return FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value), 0.05f)
			? FString::Printf(TEXT("%.0f"), Value)
			: FString::Printf(TEXT("%.1f"), Value);
	}
}

void ULKRunRewardWidget::InitializeReward(ULKBattleHUDWidget* InOwnerHUD)
{
	OwnerHUD = InOwnerHUD;
}

TSharedRef<SWidget> ULKRunRewardWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildNativeTree();
	}
	return Super::RebuildWidget();
}

void ULKRunRewardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	RefreshReward();
	if (OptionButtons.IsValidIndex(0) && OptionButtons[0]->GetVisibility() == ESlateVisibility::Visible)
	{
		OptionButtons[0]->SetKeyboardFocus();
	}
}

void ULKRunRewardWidget::BuildNativeTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RewardRoot"));
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
	PanelSize->SetWidthOverride(1080.f);
	PanelSize->SetHeightOverride(650.f);
	Scale->SetContent(PanelSize);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RewardPanel"));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(34.f, 26.f));
	PanelSize->SetContent(Panel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Panel->SetContent(Content);

	UTextBlock* Title = MakeText(WidgetTree, TEXT("Title"), 36, GoldColor);
	Title->SetText(FText::FromString(TEXT("选择战利品")));
	Content->AddChildToVerticalBox(Title);

	ProgressText = MakeText(WidgetTree, TEXT("Progress"), 18, MutedColor);
	Content->AddChildToVerticalBox(ProgressText);
	AddVerticalSpace(WidgetTree, Content, 20.f);

	Options = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Options"));
	if (UVerticalBoxSlot* LayoutSlot = Content->AddChildToVerticalBox(Options))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
			*FString::Printf(TEXT("Btn_Reward%d"), Index));
		StyleButton(Button, CardColor, FLinearColor(0.10f, 0.16f, 0.24f, 1.f));
		Button->SetToolTipText(FText::FromString(TEXT("选择此奖励；领取成功后本窗口关闭")));
		if (UHorizontalBoxSlot* LayoutSlot = Options->AddChildToHorizontalBox(Button))
		{
			LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LayoutSlot->SetPadding(FMargin(7.f, 0.f));
			LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
			LayoutSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>();
		Button->SetContent(Card);

		UTextBlock* Kind = MakeText(WidgetTree, *FString::Printf(TEXT("RewardKind%d"), Index), 15, GoldColor);
		if (UVerticalBoxSlot* LayoutSlot = Card->AddChildToVerticalBox(Kind)) { LayoutSlot->SetPadding(FMargin(12.f, 13.f, 12.f, 5.f)); }

		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
		IconSize->SetWidthOverride(54.f);
		IconSize->SetHeightOverride(54.f);
		if (UVerticalBoxSlot* LayoutSlot = Card->AddChildToVerticalBox(IconSize))
		{
			LayoutSlot->SetHorizontalAlignment(HAlign_Center);
			LayoutSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 8.f));
		}
		UBorder* IconPlate = WidgetTree->ConstructWidget<UBorder>();
		IconPlate->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.06f, 1.f));
		IconPlate->SetPadding(FMargin(8.f));
		IconSize->SetContent(IconPlate);
		UOverlay* IconOverlay = WidgetTree->ConstructWidget<UOverlay>();
		IconPlate->SetContent(IconOverlay);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>();
		if (UOverlaySlot* LayoutSlot = IconOverlay->AddChildToOverlay(Icon))
		{
			LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
			LayoutSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UTextBlock* Placeholder = MakeText(WidgetTree, *FString::Printf(TEXT("RewardGlyph%d"), Index), 26, GoldColor);
		if (UOverlaySlot* LayoutSlot = IconOverlay->AddChildToOverlay(Placeholder))
		{
			LayoutSlot->SetHorizontalAlignment(HAlign_Center);
			LayoutSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* CardTitle = MakeText(WidgetTree, *FString::Printf(TEXT("RewardTitle%d"), Index), 25, PaleColor);
		if (UVerticalBoxSlot* LayoutSlot = Card->AddChildToVerticalBox(CardTitle)) { LayoutSlot->SetPadding(FMargin(12.f, 0.f, 12.f, 8.f)); }

		UTextBlock* Detail = MakeText(WidgetTree, *FString::Printf(TEXT("RewardDetail%d"), Index), 17, MutedColor);
		Detail->SetLineHeightPercentage(1.15f);
        Detail->SetWrapTextAt(260.f);
        UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
        DetailScroll->AddChild(Detail);
		if (UVerticalBoxSlot* LayoutSlot = Card->AddChildToVerticalBox(DetailScroll))
		{
			LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LayoutSlot->SetPadding(FMargin(18.f, 2.f, 18.f, 14.f));
			LayoutSlot->SetVerticalAlignment(VAlign_Fill);
		}

		OptionButtons.Add(Button);
		OptionKindTexts.Add(Kind);
		OptionTitleTexts.Add(CardTitle);
		OptionDetailTexts.Add(Detail);
		OptionIcons.Add(Icon);
		OptionPlaceholderTexts.Add(Placeholder);
	}

	OptionButtons[0]->OnClicked.AddDynamic(this, &ULKRunRewardWidget::HandleOption0Clicked);
	OptionButtons[1]->OnClicked.AddDynamic(this, &ULKRunRewardWidget::HandleOption1Clicked);
	OptionButtons[2]->OnClicked.AddDynamic(this, &ULKRunRewardWidget::HandleOption2Clicked);

    ReplacementPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ReplacementPanel"));
    Content->AddChildToVerticalBox(ReplacementPanel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ReplacementPanel->SetVisibility(ESlateVisibility::Collapsed);
    ReplacementTitle = MakeText(WidgetTree, TEXT("ReplacementTitle"), 22, GoldColor);
    ReplacementPanel->AddChildToVerticalBox(ReplacementTitle);
    ReplacementList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ReplacementList"));
    ReplacementPanel->AddChildToVerticalBox(ReplacementList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ReplacementGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ReplacementGrid"));
    ReplacementGrid->SetSlotPadding(FMargin(4.f));
    ReplacementList->AddChild(ReplacementGrid);
    ReplacementBudget = MakeText(WidgetTree, TEXT("ReplacementBudget"), 15, MutedColor);
    ReplacementPanel->AddChildToVerticalBox(ReplacementBudget);
    ConfirmReplacementButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Btn_ConfirmReplacement"));
    StyleButton(ConfirmReplacementButton, FLinearColor(.08f,.20f,.17f), FLinearColor(.12f,.30f,.24f));
    UTextBlock* ConfirmText = MakeText(WidgetTree, TEXT("ConfirmReplacementLabel"), 17, PaleColor);
    ConfirmText->SetText(FText::FromString(TEXT("确认替换")));
    ConfirmReplacementButton->SetContent(ConfirmText);
    ConfirmReplacementButton->OnClicked.AddDynamic(this, &ULKRunRewardWidget::HandleConfirmReplacements);
    ReplacementPanel->AddChildToVerticalBox(ConfirmReplacementButton);

	AddVerticalSpace(WidgetTree, Content, 17.f);
	DeckSummaryText = MakeText(WidgetTree, TEXT("DeckSummary"), 15, MutedColor, ETextJustify::Left);
	if (UVerticalBoxSlot* LayoutSlot = Content->AddChildToVerticalBox(DeckSummaryText))
	{
		LayoutSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 10.f));
	}

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Footer"));
	Content->AddChildToVerticalBox(Footer);
	StatusText = MakeText(WidgetTree, TEXT("Status"), 15, MutedColor, ETextJustify::Left);
	StatusText->SetText(FText::FromString(TEXT("临时佣兵仅本次远征有效 · 滚轮查看技能详情")));
	if (UHorizontalBoxSlot* LayoutSlot = Footer->AddChildToHorizontalBox(StatusText))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}

	SkipButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Btn_SkipReward"));
	StyleButton(SkipButton, FLinearColor(0.13f, 0.16f, 0.20f, 1.f), FLinearColor(0.20f, 0.24f, 0.30f, 1.f));
	SkipButton->SetToolTipText(FText::FromString(TEXT("放弃本房奖励并解锁下一关")));
	SkipButton->OnClicked.AddDynamic(this, &ULKRunRewardWidget::HandleSkipClicked);
	if (UHorizontalBoxSlot* LayoutSlot = Footer->AddChildToHorizontalBox(SkipButton))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		LayoutSlot->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}
	SkipLabel = MakeText(WidgetTree, TEXT("SkipLabel"), 17, PaleColor);
    SkipLabel->SetAutoWrapText(false);
	SkipLabel->SetText(FText::FromString(TEXT("跳过奖励")));
	SkipButton->SetContent(SkipLabel);
	UButton* Home = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Btn_ReturnHome"));
	StyleButton(Home, FLinearColor(0.10f, 0.14f, 0.18f), FLinearColor(0.17f, 0.22f, 0.27f));
	Home->OnClicked.AddDynamic(this, &ULKRunRewardWidget::HandleReturnHomeClicked);
	Footer->AddChildToHorizontalBox(Home)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	UTextBlock* HomeText = MakeText(WidgetTree, TEXT("ReturnHomeText"), 17, PaleColor);
	HomeText->SetText(FText::FromString(TEXT("暂回家园")));
	Home->SetContent(HomeText);
	Home->SetToolTipText(FText::FromString(TEXT("保留当前奖励，继续远征时再选择")));
}

void ULKRunRewardWidget::RefreshReward()
{
	if (!OwnerHUD) { return; }
    const ULKRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULKRunSubsystem>() : nullptr;
    bReducingLegacyDeck = Run && Run->NeedsDeckReduction();
    if (Options && IsChoosingReplacement()) { ShowReplacementChoices(); return; }
	DisplayedOptionCount = FMath::Clamp(OwnerHUD->GetPendingRewardCount(), 0, 3);
	if (OptionButtons.Num() != 3)
	{
		OnRewardDataReadyBP(DisplayedOptionCount);
		return;
	}
	const ALKBattleGameMode* GM = OwnerHUD->GetBattleGameMode();
	if (ProgressText)
	{
		const int32 Room = Run ? Run->GetRunState().BattleHistory.Num() : 0;
		const int32 Tier = GM ? GM->GetCurrentEncounter().RewardTier : 0;
		ProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("已完成 %d 场战斗  ·  奖励档 %d  ·  选择一项继续"), Room, Tier)));
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bVisible = Index < DisplayedOptionCount;
		OptionButtons[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bVisible || !GM) { continue; }

		const FLKRunRewardOffer Offer = GM->GetRunRewardOffer(Index);
		const ULKCardDefinition* Definition = OwnerHUD->GetCardDefinition(Offer.CardId);
		const FString CardName = Definition ? Definition->CardName.ToString() : Offer.CardId.ToString();
		OptionKindTexts[Index]->SetText(FText::FromString(
			Offer.Kind == ELKRunRewardKind::AddCard ? TEXT("新卡入队") : TEXT("卡牌强化")));
		OptionTitleTexts[Index]->SetText(FText::FromString(CardName));
        if (Definition)
        {
            OptionTitleTexts[Index]->SetColorAndOpacity(LKCardPresentation::Color(*Definition));
            OptionPlaceholderTexts[Index]->SetColorAndOpacity(LKCardPresentation::Color(*Definition));
            OptionKindTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"),
                *LKCardPresentation::Classification(*Definition).ToString(), Offer.Kind == ELKRunRewardKind::AddCard ? TEXT("新卡入队") : TEXT("卡牌强化"))));
            OptionButtons[Index]->SetToolTipText(LKCardPresentation::Detail(*Definition));
        }
		OptionDetailTexts[Index]->SetText(BuildOptionDetail(Index));

		UTexture2D* Icon = OwnerHUD->GetCardIcon(Offer.CardId);
		OptionIcons[Index]->SetBrushFromTexture(Icon, true);
		OptionIcons[Index]->SetVisibility(Icon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		OptionPlaceholderTexts[Index]->SetVisibility(Icon ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		OptionPlaceholderTexts[Index]->SetText(FText::FromString(
			Offer.Kind == ELKRunRewardKind::AddCard ? TEXT("新") : TEXT("↑")));
	}
	if (DeckSummaryText) { DeckSummaryText->SetText(BuildDeckSummary()); }
	SetInteractionEnabled(true);
	OnRewardDataReadyBP(DisplayedOptionCount);
}

FText ULKRunRewardWidget::BuildOptionDetail(int32 Index) const
{
	const ALKBattleGameMode* GM = OwnerHUD ? OwnerHUD->GetBattleGameMode() : nullptr;
	if (!GM) { return FText::GetEmpty(); }
	const FLKRunRewardOffer Offer = GM->GetRunRewardOffer(Index);
	const ULKCardDefinition* Definition = OwnerHUD->GetCardDefinition(Offer.CardId);
	if (!Definition) { return OwnerHUD->GetRewardOptionText(Index); }

	if (Offer.Kind == ELKRunRewardKind::AddCard)
	{
        FString Detail = LKCardPresentation::Detail(*Definition, GM->GetUnitRow(Definition->CardType == ELKCardType::Building ? Definition->BuildingUnitId : Definition->SpawnUnitId)).ToString();
        const ULKSilverComponent* Silver = GM->GetTeamSilver(ELKTeam::Player);
        if (Silver && Definition->Cost > Silver->GetCap())
        { Detail += FString::Printf(TEXT("\n\n本轮银币上限 %.0f，当前无法部署此卡。金库升级对下次新远征生效。"), Silver->GetCap()); }
        return FText::FromString(Detail);
	}

	const FName UnitId = Definition->CardType == ELKCardType::Building ? Definition->BuildingUnitId : Definition->SpawnUnitId;
	const FLKUnitRow* Unit = GM->GetUnitRow(UnitId);
	if (!Unit) { return OwnerHUD->GetRewardOptionText(Index); }
	const float BeforeScale = FMath::Pow(1.1f, FMath::Max(0, Offer.LevelBefore));
	const float AfterScale = FMath::Pow(1.1f, FMath::Max(0, Offer.LevelAfter));
	return FText::FromString(FString::Printf(TEXT("Lv%d  →  Lv%d\n生命 %s  →  %s\n攻击 %s  →  %s"),
		Offer.LevelBefore, Offer.LevelAfter,
		*Number(Unit->BaseHealth * BeforeScale), *Number(Unit->BaseHealth * AfterScale),
		*Number(Unit->AttackDamage * BeforeScale), *Number(Unit->AttackDamage * AfterScale)));
}

FText ULKRunRewardWidget::BuildDeckSummary() const
{
	if (!OwnerHUD || !OwnerHUD->GetWorld() || !OwnerHUD->GetWorld()->GetGameInstance())
	{
		return FText::GetEmpty();
	}
	const ULKRunSubsystem* Run = OwnerHUD->GetWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
	if (!Run) { return FText::GetEmpty(); }
	TArray<FString> Cards;
	for (const FLKRunCardState& State : Run->GetRunState().Cards)
	{
		const ULKCardDefinition* Definition = OwnerHUD->GetCardDefinition(State.CardId);
		FString Label = Definition ? Definition->CardName.ToString() : State.CardId.ToString();
		if (State.UpgradeLevel > 0) { Label += FString::Printf(TEXT(" Lv%d"), State.UpgradeLevel); }
		Cards.Add(MoveTemp(Label));
	}
	return FText::FromString(FString::Printf(TEXT("容量 %d / 8 · %d 张：%s"), Run->GetDeckCapacityUsed(), Cards.Num(), *FString::Join(Cards, TEXT("  ·  "))));
}

void ULKRunRewardWidget::SetInteractionEnabled(bool bEnabled)
{
	bInteractionLocked = !bEnabled;
	for (UButton* Button : OptionButtons) { if (Button) { Button->SetIsEnabled(bEnabled); } }
	if (SkipButton) { SkipButton->SetIsEnabled(bEnabled); }
}

void ULKRunRewardWidget::TryChoose(int32 Index)
{
	if (bInteractionLocked || !OwnerHUD || Index < 0 || Index >= DisplayedOptionCount) { return; }
	SetInteractionEnabled(false);
	if (!ChooseOption(Index))
	{
		if (StatusText) { StatusText->SetText(FText::FromString(TEXT("奖励状态已变化，请重新选择或跳过"))); }
		RefreshReward();
	}
}

void ULKRunRewardWidget::HandleOption0Clicked() { TryChoose(0); }
void ULKRunRewardWidget::HandleOption1Clicked() { TryChoose(1); }
void ULKRunRewardWidget::HandleOption2Clicked() { TryChoose(2); }

void ULKRunRewardWidget::HandleSkipClicked()
{
    if (bReducingLegacyDeck) { return; }
    if (PendingOptionIndex != INDEX_NONE)
    {
        PendingOptionIndex = INDEX_NONE;
        SelectedReplacementIds.Reset();
        ReplacementPanel->SetVisibility(ESlateVisibility::Collapsed);
        Options->SetVisibility(ESlateVisibility::Visible);
        SkipLabel->SetText(FText::FromString(TEXT("跳过奖励")));
        StatusText->SetText(FText::FromString(TEXT("临时佣兵仅本次远征有效 · 滚轮查看技能详情")));
        RefreshReward();
        return;
    }
	if (bInteractionLocked || !OwnerHUD) { return; }
	SetInteractionEnabled(false);
	if (!SkipReward())
	{
		if (StatusText) { StatusText->SetText(FText::FromString(TEXT("无法跳过当前奖励，请重试"))); }
		RefreshReward();
	}
}

bool ULKRunRewardWidget::ChooseOption(int32 Index)
{
    if (!OwnerHUD) { return false; }
    const ULKRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULKRunSubsystem>() : nullptr;
    if (Run && Index >= 0 && Index < Run->GetPendingRewardCount()
        && Run->GetPendingRewardOffer(Index).Kind == ELKRunRewardKind::AddCard
        && Run->GetDeckCapacityUsed() + LKCardRules::Slots(Run->GetPendingRewardOffer(Index).CardId) > LKCardRules::Capacity)
    {
        PendingOptionIndex = Index;
        SelectedReplacementIds.Reset();
        ShowReplacementChoices();
        return true;
    }
	return OwnerHUD && OwnerHUD->ChooseRunReward(Index);
}

void ULKRunRewardWidget::ShowReplacementChoices()
{
    ULKRunSubsystem* Run = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULKRunSubsystem>() : nullptr;
    if (!Run || !ReplacementPanel || !OwnerHUD) { return; }
    Options->SetVisibility(ESlateVisibility::Collapsed);
    ReplacementPanel->SetVisibility(ESlateVisibility::Visible);
    ReplacementGrid->ClearChildren();
    ReplacementCardIds.Reset();
    const ULKCardDefinition* Incoming = OwnerHUD->GetCardDefinition(Run->GetPendingRewardOffer(PendingOptionIndex).CardId);
    ReplacementTitle->SetText(FText::FromString(bReducingLegacyDeck
        ? TEXT("旧档容量超额：移除卡牌至不超过 8 格")
        : FString::Printf(TEXT("获取「%s」：勾选要替换的旧卡"), Incoming ? *Incoming->CardName.ToString() : TEXT("新卡"))));
    StatusText->SetText(FText::FromString(bReducingLegacyDeck ? TEXT("至少保留 5 种卡牌") : TEXT("可多选；确认前保留原卡，强化不转移")));
    SkipLabel->SetText(FText::FromString(TEXT("返回奖励选择")));
    SetInteractionEnabled(true);
    SkipButton->SetIsEnabled(!bReducingLegacyDeck);
    const FLKRunState State = Run->GetRunState();
    const int32 Released = LKCardRules::Used(SelectedReplacementIds);
    const int32 AfterSlots = Run->GetDeckCapacityUsed() - Released + (Incoming ? LKCardRules::Slots(Incoming->CardId) : 0);
    const int32 AfterCount = State.Cards.Num() - SelectedReplacementIds.Num() + 1;
    ReplacementBudget->SetText(FText::FromString(FString::Printf(TEXT("已选 %d 张 / 释放 %d 格 · 替换后 %d/8 格、%d 张（至少 5 张）"),
        SelectedReplacementIds.Num(), Released, AfterSlots, AfterCount)));
    ReplacementBudget->SetVisibility(bReducingLegacyDeck ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    ConfirmReplacementButton->SetVisibility(bReducingLegacyDeck ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    ConfirmReplacementButton->SetIsEnabled(!SelectedReplacementIds.IsEmpty() && AfterSlots <= LKCardRules::Capacity && AfterCount >= LKCardRules::MinimumCards);
    for (int32 Index = 0; Index < State.Cards.Num(); ++Index)
    {
        const FLKRunCardState& CardState = State.Cards[Index];
        ReplacementCardIds.Add(CardState.CardId);
        const ULKCardDefinition* Card = OwnerHUD->GetCardDefinition(CardState.CardId);
        FLKHomePanelRow Row;
        Row.Label = Card ? LKCardPresentation::Label(*Card) : FText::FromName(CardState.CardId);
        Row.Value = FText::FromString(FString::Printf(TEXT("%d 格 · Lv%d · %d 银币"), LKCardRules::Slots(CardState.CardId), CardState.UpgradeLevel, Card ? Card->Cost : 0));
        Row.Detail = Card ? LKCardPresentation::Detail(*Card) : FText::GetEmpty();
        Row.RowId = CardState.CardId;
        Row.bSelectable = true;
        Row.bToggleable = !bReducingLegacyDeck;
        Row.bChecked = SelectedReplacementIds.Contains(CardState.CardId);
        ULKHomeListButtonWidget* Button = CreateWidget<ULKHomeListButtonWidget>(GetWorld());
        ReplacementGrid->AddChildToUniformGrid(Button, Index / 2, Index % 2)->SetHorizontalAlignment(HAlign_Fill);
        Button->SetupRow(Index, Row, false);
        Button->SetCompactRow();
        Button->SetToolTipText(Row.Detail);
        Button->OnHomeListClicked.BindLambda([this](int32 CardIndex) { ChooseReplacement(CardIndex); });
    }
    DeckSummaryText->SetText(BuildDeckSummary());
}

bool ULKRunRewardWidget::ChooseReplacement(int32 CardIndex)
{
    if (bInteractionLocked || !ReplacementCardIds.IsValidIndex(CardIndex) || !OwnerHUD) { return false; }
    ULKRunSubsystem* Run = GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    if (!Run) { return false; }
    const FName CardId = ReplacementCardIds[CardIndex];
    if (bReducingLegacyDeck)
    {
        if (!Run->DiscardExcessCard(CardId)) { StatusText->SetText(FText::FromString(TEXT("保存失败，卡组保持原状，请重试"))); return false; }
        bReducingLegacyDeck = Run->NeedsDeckReduction();
        if (bReducingLegacyDeck) { ShowReplacementChoices(); }
        else { UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true))); }
        return true;
    }
    if (SelectedReplacementIds.Contains(CardId)) { SelectedReplacementIds.Remove(CardId); }
    else { SelectedReplacementIds.Add(CardId); }
    ShowReplacementChoices();
    return true;
}

void ULKRunRewardWidget::HandleConfirmReplacements() { ConfirmReplacements(); }

bool ULKRunRewardWidget::ConfirmReplacements()
{
    if (bInteractionLocked || !OwnerHUD || bReducingLegacyDeck || PendingOptionIndex == INDEX_NONE || SelectedReplacementIds.IsEmpty()) { return false; }
    SetInteractionEnabled(false);
    ConfirmReplacementButton->SetIsEnabled(false);
    if (OwnerHUD->ChooseRunRewardReplacingCards(PendingOptionIndex, SelectedReplacementIds))
    { PendingOptionIndex = INDEX_NONE; SelectedReplacementIds.Reset(); return true; }
    SetInteractionEnabled(true);
    ShowReplacementChoices();
    StatusText->SetText(FText::FromString(TEXT("容量/张数不符或保存失败；原卡和奖励均已保留")));
    return false;
}

bool ULKRunRewardWidget::SkipReward()
{
	return OwnerHUD && OwnerHUD->SkipRunReward();
}

void ULKRunRewardWidget::HandleReturnHomeClicked()
{
	ALKBattleGameMode* GM = OwnerHUD ? OwnerHUD->GetBattleGameMode() : nullptr;
	if (!GM || !GM->ReturnToHome())
	{
		if (StatusText) { StatusText->SetText(FText::FromString(TEXT("暂时无法返回，请检查存档并重试"))); }
	}
}
