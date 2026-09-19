#include "ULKHomeHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/SpinBox.h"
#include "LKHomeUIStyle.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "ALKHomeGameMode.h"
#include "LKHomeContent.h"
#include "LKLog.h"
#include "ULKHomeListButtonWidget.h"
#include "ULKProfileSubsystem.h"

namespace
{
	const FLinearColor HomeBackdropColor(0.004f, 0.010f, 0.024f, 0.72f);
	const FLinearColor HomeTopBarColor(0.014f, 0.026f, 0.045f, 0.95f);
	const FLinearColor HomePanelColor(0.020f, 0.038f, 0.064f, 0.99f);
	const FLinearColor HomeGoldColor(0.96f, 0.69f, 0.22f, 1.f);
	const FLinearColor HomePaleColor(0.90f, 0.94f, 0.98f, 1.f);
	const FLinearColor HomeMutedColor(0.60f, 0.69f, 0.79f, 1.f);

	UTextBlock* MakeHomeText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, const FLinearColor& Color,
		ETextJustify::Type Justify = ETextJustify::Left)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justify);
		Text->SetAutoWrapText(true);
		return Text;
	}
}

void ULKHomeHUDWidget::InitializeHomeHUD(ALKHomeGameMode* InGameMode)
{
	HomeGameMode = InGameMode;
	ResetDraftFromSaved();
}

TSharedRef<SWidget> ULKHomeHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildNativeTree(); }
	return Super::RebuildWidget();
}

void ULKHomeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	if (!bBound && HomeGameMode)
	{
		HomeGameMode->OnHomeRefreshRequested.AddDynamic(this, &ULKHomeHUDWidget::HandleHomeRefreshRequested);
		if (ULKProfileSubsystem* Profile = HomeGameMode->GetProfileSubsystem())
		{
			Profile->OnProfileChanged.AddDynamic(this, &ULKHomeHUDWidget::HandleProfileChanged);
		}
		bBound = true;
	}
	Refresh();
}

void ULKHomeHUDWidget::NativeDestruct()
{
	if (bBound && HomeGameMode)
	{
		HomeGameMode->OnHomeRefreshRequested.RemoveDynamic(this, &ULKHomeHUDWidget::HandleHomeRefreshRequested);
		if (ULKProfileSubsystem* Profile = HomeGameMode->GetProfileSubsystem())
		{
			Profile->OnProfileChanged.RemoveDynamic(this, &ULKHomeHUDWidget::HandleProfileChanged);
		}
		bBound = false;
	}
	Super::NativeDestruct();
}

void ULKHomeHUDWidget::BuildNativeTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HomeRoot"));
	WidgetTree->RootWidget = Root;

	// ---------- 顶部状态栏 ----------
	TopBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopBar"));
	TopBar->SetBrushColor(HomeTopBarColor);
	TopBar->SetPadding(FMargin(24.f, 10.f));
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(TopBar))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
		LayoutSlot->SetVerticalAlignment(VAlign_Top);
	}

	UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopRow"));
	TopBar->SetContent(TopRow);

	TitleText = MakeHomeText(WidgetTree, TEXT("TitleText"), 24, HomeGoldColor);
	TitleText->SetText(FText::FromString(TEXT("家园")));
	if (UHorizontalBoxSlot* LayoutSlot = TopRow->AddChildToHorizontalBox(TitleText))
	{
		LayoutSlot->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}
	GoldText = MakeHomeText(WidgetTree, TEXT("GoldText"), 20, HomePaleColor);
	if (UHorizontalBoxSlot* LayoutSlot = TopRow->AddChildToHorizontalBox(GoldText))
	{
		LayoutSlot->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}
	ExpeditionText = MakeHomeText(WidgetTree, TEXT("ExpeditionText"), 17, HomeMutedColor);
	if (UHorizontalBoxSlot* LayoutSlot = TopRow->AddChildToHorizontalBox(ExpeditionText))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}
	ProfileText = MakeHomeText(WidgetTree, TEXT("ProfileText"), 15, HomeMutedColor, ETextJustify::Right);
	if (UHorizontalBoxSlot* LayoutSlot = TopRow->AddChildToHorizontalBox(ProfileText))
	{
		LayoutSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ---------- 遮罩（面板打开时挡住场景点击） ----------
	Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(HomeBackdropColor);
	Backdrop->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(Backdrop))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
		LayoutSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// ---------- 建筑快捷栏 ----------
	UHorizontalBox* BuildingBarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildingBarRow"));
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(BuildingBarRow))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Center);
		LayoutSlot->SetVerticalAlignment(VAlign_Bottom);
		LayoutSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
	}

	BuildingBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildingBar"));
	if (UHorizontalBoxSlot* LayoutSlot = BuildingBarRow->AddChildToHorizontalBox(BuildingBar))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	const TArray<FLKBuildingDefinition>& Definitions = LKHomeContent::Buildings();
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		ULKHomeListButtonWidget* Button = CreateWidget<ULKHomeListButtonWidget>(this, ULKHomeListButtonWidget::StaticClass());
		if (!Button) { continue; }
		FLKHomePanelAction Action;
		Action.Label = FText::FromString(FString::Printf(TEXT("%d %s"), Index + 1,
			Index == 0 ? TEXT("神像") : *Definitions[Index].DisplayName.ToString()));
		Action.ActionId = Definitions[Index].BuildingId;
		Action.bEnabled = true;
		Button->SetupAction(Index, Action);
		Button->OnHomeListClicked.BindLambda([this](int32 InIndex) { HandleBuildingBarClicked(InIndex); });
		if (UHorizontalBoxSlot* LayoutSlot = BuildingBar->AddChildToHorizontalBox(Button))
		{
			LayoutSlot->SetPadding(FMargin(4.f, 0.f));
		}
		BuildingBarWidgets.Add(Button);
	}

	// ---------- 面板 ----------
	UScaleBox* PanelScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("PanelScale"));
	PanelScale->SetStretch(EStretch::ScaleToFit);
	PanelScale->SetStretchDirection(EStretchDirection::Both);
	UOverlaySlot* ScaleSlot = Root->AddChildToOverlay(PanelScale);
	ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
	ScaleSlot->SetVerticalAlignment(VAlign_Fill);
	ScaleSlot->SetPadding(FMargin(24.f, 66.f, 24.f, 92.f));
	PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(1120.f);
	PanelSize->SetHeightOverride(620.f);
	PanelScale->SetContent(PanelSize);
	PanelSize->SetVisibility(ESlateVisibility::Collapsed);

	PanelHost = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelHost"));
	PanelHost->SetBrushColor(HomePanelColor);
	PanelHost->SetPadding(FMargin(30.f, 22.f));
	PanelHost->SetVisibility(ESlateVisibility::Collapsed);
	PanelSize->SetContent(PanelHost);

	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelContent"));
	PanelHost->SetContent(PanelContent);

	PanelTitleText = MakeHomeText(WidgetTree, TEXT("PanelTitle"), 30, HomeGoldColor);
	PanelContent->AddChildToVerticalBox(PanelTitleText);

	PanelSubtitleText = MakeHomeText(WidgetTree, TEXT("PanelSubtitle"), 16, HomeMutedColor);
	if (UVerticalBoxSlot* LayoutSlot = PanelContent->AddChildToVerticalBox(PanelSubtitleText))
	{
		LayoutSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
	}

	TabsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TabsBox"));
	PanelContent->AddChildToVerticalBox(TabsBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* LayoutSlot = PanelContent->AddChildToVerticalBox(Body))
	{
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LayoutSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	RowsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RowsScroll"));
	UHorizontalBoxSlot* ListSlot = Body->AddChildToHorizontalBox(RowsScroll);
	ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListSlot->SetPadding(FMargin(0.f, 0.f, 22.f, 0.f));
	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RowsBox"));
	RowsScroll->AddChild(RowsBox);

	DetailHost = WidgetTree->ConstructWidget<UBorder>();
	DetailHost->SetBrushColor(FLinearColor(0.035f, 0.060f, 0.085f));
	DetailHost->SetPadding(FMargin(24.f));
	UHorizontalBoxSlot* DetailSlot = Body->AddChildToHorizontalBox(DetailHost);
	DetailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
	DetailHost->SetContent(DetailScroll);
	UVerticalBox* Detail = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailScroll->AddChild(Detail);
	USizeBox* BadgeSize = WidgetTree->ConstructWidget<USizeBox>();
	BadgeSize->SetWidthOverride(64.f);
	BadgeSize->SetHeightOverride(64.f);
	Detail->AddChildToVerticalBox(BadgeSize)->SetHorizontalAlignment(HAlign_Left);
	DetailBadge = WidgetTree->ConstructWidget<UBorder>();
	DetailBadge->SetVerticalAlignment(VAlign_Center);
	BadgeSize->SetContent(DetailBadge);
	DetailGlyph = MakeHomeText(WidgetTree, TEXT("DetailGlyph"), 28, HomePaleColor, ETextJustify::Center);
	DetailBadge->SetContent(DetailGlyph);
	DetailTitle = MakeHomeText(WidgetTree, TEXT("DetailTitle"), 22, HomePaleColor);
	Detail->AddChildToVerticalBox(DetailTitle)->SetPadding(FMargin(0.f, 10.f, 0.f, 10.f));
	DetailCopy = MakeHomeText(WidgetTree, TEXT("DetailCopy"), 16, HomeMutedColor);
	DetailCopy->SetLineHeightPercentage(1.15f);
	Detail->AddChildToVerticalBox(DetailCopy);

	DepartureGoldRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	PanelContent->AddChildToVerticalBox(DepartureGoldRow)->SetPadding(FMargin(0, 4));
	UTextBlock* CarryLabel = MakeHomeText(WidgetTree, TEXT("CarryGoldLabel"), 18, HomeGoldColor);
	CarryLabel->SetText(FText::FromString(TEXT("出征携带金币  ")));
	DepartureGoldRow->AddChildToHorizontalBox(CarryLabel)->SetVerticalAlignment(VAlign_Center);
	DepartureGoldInput = WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), TEXT("DepartureGoldInput"));
	DepartureGoldInput->SetMinValue(0); DepartureGoldInput->SetMinSliderValue(0);
	DepartureGoldInput->SetDelta(1); DepartureGoldInput->SetMinFractionalDigits(0); DepartureGoldInput->SetMaxFractionalDigits(0);
	DepartureGoldInput->SetMinDesiredWidth(180);
	DepartureGoldInput->OnValueChanged.AddDynamic(this, &ULKHomeHUDWidget::HandleDepartureGoldChanged);
	DepartureGoldRow->AddChildToHorizontalBox(DepartureGoldInput);
	PanelStatusText = MakeHomeText(WidgetTree, TEXT("PanelStatus"), 16, HomePaleColor);
	if (UVerticalBoxSlot* LayoutSlot = PanelContent->AddChildToVerticalBox(PanelStatusText))
	{
		LayoutSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));
	}

	ActionsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionsBox"));
	PanelContent->AddChildToVerticalBox(ActionsBox);

	HintText = MakeHomeText(WidgetTree, TEXT("HintText"), 14, HomeMutedColor, ETextJustify::Center);
	if (UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(HintText))
	{
		LayoutSlot->SetHorizontalAlignment(HAlign_Center);
		LayoutSlot->SetVerticalAlignment(VAlign_Bottom);
		LayoutSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 86.f));
	}
	HintText->SetText(FText::FromString(TEXT("点击建筑打开面板（也可按 1~7 快捷打开）· Esc 关闭")));
	HintText->SetAutoWrapText(false);
}

void ULKHomeHUDWidget::HandleDepartureGoldChanged(float Value)
{
	if (HomeGameMode && FMath::IsFinite(Value)) { HomeGameMode->SetDepartureGold(FMath::RoundToInt(Value)); }
}

void ULKHomeHUDWidget::Refresh()
{
	RefreshTopBar();
	RebuildPanel();
}

void ULKHomeHUDWidget::RefreshTopBar()
{
	if (!HomeGameMode) { return; }
	if (GoldText) { GoldText->SetText(GetGoldText()); }
	if (ExpeditionText) { ExpeditionText->SetText(GetExpeditionStatusText()); }
	if (ProfileText) { ProfileText->SetText(HomeGameMode->GetProfileStatusText()); }
	OnHomeTopBarChangedBP(GetGoldText(), GetExpeditionStatusText());
}

FText ULKHomeHUDWidget::GetGoldText() const
{
	const int32 Gold = HomeGameMode ? HomeGameMode->GetGold() : 0;
	return FText::FromString(FString::Printf(TEXT("金币 %d"), Gold));
}

FText ULKHomeHUDWidget::GetExpeditionStatusText() const
{
	return HomeGameMode ? HomeGameMode->GetExpeditionStatusText() : FText::FromString(TEXT("远征状态未知"));
}

FText ULKHomeHUDWidget::GetPanelStatusText() const
{
	return PanelStatusText ? PanelStatusText->GetText() : FText::GetEmpty();
}

void ULKHomeHUDWidget::OpenPanelForBuilding(FName BuildingId)
{
	const FLKBuildingDefinition* Definition = LKHomeContent::FindBuilding(BuildingId);
	if (!Definition) { return; }
	OpenPanel(LKHomeContent::PanelForBuilding(LKHomeContent::BuildingFromId(BuildingId)));
}

void ULKHomeHUDWidget::OpenPanel(ELKHomePanel Panel)
{
	if (Panel == ELKHomePanel::None || Panel == CurrentPanel) { return; }
	// 切换到别的建筑时，战备草稿未保存先确认（不静默丢弃）。
	if (CurrentPanel == ELKHomePanel::WarRoom && Panel != ELKHomePanel::WarRoom && bDraftDirty)
	{
		PendingPanel = Panel;
		bConfirmDiscard = true;
		RebuildPanel();
		return;
	}

	CurrentPanel = Panel;
	SelectedRowId = NAME_None;
	bConfirmDiscard = false;
	bConfirmAbandon = false;
	LastMessage = FText::GetEmpty();
	if (Panel == ELKHomePanel::WarRoom) { ResetDraftFromSaved(); }
	if (Panel == ELKHomePanel::Gate)
	{
		SelectedRowId = HomeGameMode ? HomeGameMode->GetSelectedRegionId() : LKHomeContent::DefaultRegionId();
	}
	RebuildPanel();
}

void ULKHomeHUDWidget::RequestClosePanel()
{
	if (CurrentPanel == ELKHomePanel::None) { return; }
	if (bConfirmAbandon) { bConfirmAbandon = false; RebuildPanel(); return; }
	if (bConfirmDiscard) { bConfirmDiscard = false; RebuildPanel(); return; }
	PendingPanel = ELKHomePanel::None;
	if (CurrentPanel == ELKHomePanel::WarRoom && bDraftDirty)
	{
		bConfirmDiscard = true;
		RebuildPanel();
		return;
	}
	CurrentPanel = ELKHomePanel::None;
	SelectedRowId = NAME_None;
	bConfirmDiscard = false;
	LastMessage = FText::GetEmpty();
	RebuildPanel();
}

void ULKHomeHUDWidget::RebuildPanel()
{
	if (!PanelHost || !HomeGameMode) { return; }

	const bool bOpen = CurrentPanel != ELKHomePanel::None;
	PanelSize->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PanelHost->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Backdrop->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	HintText->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (!bOpen)
	{
		OnHomePanelDataReadyBP(FLKHomePanelModel());
		return;
	}

	FLKHomePanelModel Model;
	if (bConfirmAbandon)
	{
		Model.Panel = CurrentPanel;
		Model.Title = FText::FromString(TEXT("放弃当前远征？"));
		Model.Subtitle = FText::FromString(TEXT("本轮路线与临时强化将结束，剩余远征金币全额带回家园。家园和已保存战备保留。"));
		for (const bool bConfirm : {false, true})
		{
			FLKHomePanelAction Action;
			Action.ActionId = bConfirm ? TEXT("ConfirmAbandon") : TEXT("CancelAbandon");
			Action.Label = FText::FromString(bConfirm ? TEXT("确认放弃") : TEXT("保留远征"));
			Model.Actions.Add(Action);
		}
	}
	else if (bConfirmDiscard)
	{
		Model.Panel = CurrentPanel;
		Model.Title = FText::FromString(TEXT("战备草稿未保存"));
		Model.Subtitle = FText::FromString(TEXT("离开战备处会丢弃这次修改，是否保存？"));
		Model.Status = FText::FromString(TEXT("保存后下次新远征生效；进行中的远征不受影响"));
		FLKHomePanelAction Save;
		Save.Label = FText::FromString(TEXT("保存并离开"));
		Save.ActionId = TEXT("ConfirmSave");
		Save.bEnabled = HomeGameMode->BuildPanelModel(ELKHomePanel::WarRoom, DraftLoadout, SelectedRowId, BarracksTab).Actions.ContainsByPredicate([](const FLKHomePanelAction& A) { return A.ActionId == "SaveLoadout" && A.bEnabled; });
		Model.Actions.Add(Save);
		FLKHomePanelAction Discard;
		Discard.Label = FText::FromString(TEXT("丢弃修改并离开"));
		Discard.ActionId = TEXT("ConfirmDiscard");
		Discard.bEnabled = true;
		Model.Actions.Add(Discard);
		FLKHomePanelAction Cancel;
		Cancel.Label = FText::FromString(TEXT("继续编辑"));
		Cancel.ActionId = TEXT("CancelClose");
		Cancel.bEnabled = true;
		Model.Actions.Add(Cancel);
	}
	else
	{
		Model = HomeGameMode->BuildPanelModel(CurrentPanel, DraftLoadout, SelectedRowId, BarracksTab);
	}
	if (CurrentPanel == ELKHomePanel::WarRoom && !bConfirmDiscard)
	{
		Model.Rows.RemoveAll([this](const FLKHomePanelRow& Row)
		{
			return Row.RowId.IsNone() || (bWarRoomHeroes ? Row.bToggleable : !Row.bToggleable);
		});
	}

	if (SelectedRowId.IsNone())
	{
		for (const FLKHomePanelRow& Row : Model.Rows)
		{
			if ((Row.bSelectable || Row.bToggleable) && Row.HeroSlotIndex == INDEX_NONE) { SelectedRowId = Row.RowId; break; }
		}
	}
	const bool bCarryEditable = CurrentPanel == ELKHomePanel::Gate && !bConfirmAbandon && !bConfirmDiscard && !HomeGameMode->HasActiveExpedition();
	DepartureGoldRow->SetVisibility(bCarryEditable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCarryEditable)
	{
		const int32 Amount = HomeGameMode->GetDepartureGold();
		DepartureGoldInput->SetMaxValue(HomeGameMode->GetDepartureGoldLimit());
		DepartureGoldInput->SetMaxSliderValue(HomeGameMode->GetDepartureGoldLimit());
		DepartureGoldInput->SetValue(Amount);
	}
	DisplayedModel = Model;
	PanelTitleText->SetText(Model.Title);
	PanelSubtitleText->SetText(Model.Subtitle);
	const bool bCollection = CurrentPanel == ELKHomePanel::Library || CurrentPanel == ELKHomePanel::HeroHouse || CurrentPanel == ELKHomePanel::Barracks;
	PanelStatusText->SetText(!LastMessage.IsEmpty() ? LastMessage : bCollection ? FText::FromString(TEXT("点击左侧条目查看详情")) : Model.Status);
	UpdateDetail(Model);
	TabsBox->ClearChildren();
	TabsBox->SetVisibility(CurrentPanel == ELKHomePanel::WarRoom && !bConfirmDiscard ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (CurrentPanel == ELKHomePanel::WarRoom && !bConfirmDiscard)
	{
		for (int32 Tab = 0; Tab < 2; ++Tab)
		{
			ULKHomeListButtonWidget* Widget = CreateWidget<ULKHomeListButtonWidget>(this, ULKHomeListButtonWidget::StaticClass());
			FLKHomePanelAction Action;
			Action.Label = FText::FromString(Tab == 0 ? TEXT("出征牌组") : TEXT("出征英雄"));
			Action.bEnabled = bWarRoomHeroes != (Tab == 1);
			Widget->SetupAction(Tab, Action);
			Widget->OnHomeListClicked.BindLambda([this](int32 Index)
			{
				bWarRoomHeroes = Index == 1;
				SelectedRowId = NAME_None;
				LastMessage = FText::GetEmpty();
				RebuildPanel();
				RowsScroll->ScrollToStart();
			});
			TabsBox->AddChildToHorizontalBox(Widget)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}
	}

	// 行
	RowsBox->ClearChildren();
	RowWidgets.Reset();
	for (int32 Index = 0; Index < Model.Rows.Num(); ++Index)
	{
		const FLKHomePanelRow& Row = Model.Rows[Index];
		ULKHomeListButtonWidget* Widget = CreateWidget<ULKHomeListButtonWidget>(this, ULKHomeListButtonWidget::StaticClass());
		if (!Widget) { continue; }
		Widget->SetupRow(Index, Row, Row.HeroSlotIndex != INDEX_NONE ? Row.HeroSlotIndex == SelectedHeroSlot : !Row.RowId.IsNone() && Row.RowId == SelectedRowId);
		Widget->OnHomeListClicked.BindLambda([this](int32 InIndex) { HandleRowActivated(InIndex); });
		if (UVerticalBoxSlot* LayoutSlot = RowsBox->AddChildToVerticalBox(Widget))
		{
			LayoutSlot->SetPadding(FMargin(0.f, 3.f));
		}
		RowWidgets.Add(Widget);
	}

	// 操作
	ActionsBox->ClearChildren();
	ActionWidgets.Reset();
	for (int32 Index = 0; Index < Model.Actions.Num(); ++Index)
	{
		ULKHomeListButtonWidget* Widget = CreateWidget<ULKHomeListButtonWidget>(this, ULKHomeListButtonWidget::StaticClass());
		if (!Widget) { continue; }
		Widget->SetupAction(Index, Model.Actions[Index]);
		Widget->OnHomeListClicked.BindLambda([this](int32 InIndex) { HandleActionActivated(InIndex); });
		if (UHorizontalBoxSlot* LayoutSlot = ActionsBox->AddChildToHorizontalBox(Widget))
		{
			LayoutSlot->SetPadding(FMargin(6.f, 0.f));
			LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		ActionWidgets.Add(Widget);
	}

	OnHomePanelDataReadyBP(Model);
}

void ULKHomeHUDWidget::ApplyMessage(const FText& Message)
{
	LastMessage = Message;
	if (PanelStatusText) { PanelStatusText->SetText(Message); }
}

void ULKHomeHUDWidget::HandleBuildingBarClicked(int32 Index)
{
	const TArray<FLKBuildingDefinition>& Definitions = LKHomeContent::Buildings();
	if (!Definitions.IsValidIndex(Index)) { return; }
	OpenPanelForBuilding(Definitions[Index].BuildingId);
}

void ULKHomeHUDWidget::HandleRowActivated(int32 Index)
{
	if (!HomeGameMode || bConfirmDiscard || bConfirmAbandon) { return; }
	if (!DisplayedModel.Rows.IsValidIndex(Index)) { return; }
	const FLKHomePanelRow Row = DisplayedModel.Rows[Index];
	if (!Row.bEnabled) { return; }
	LastMessage = FText::GetEmpty();
	if (CurrentPanel == ELKHomePanel::WarRoom && Row.bSelectable)
	{
		if (Row.HeroSlotIndex != INDEX_NONE) { SelectedHeroSlot = Row.HeroSlotIndex; }
		else if (DraftLoadout.HeroIds.IsValidIndex(SelectedHeroSlot))
		{
			const int32 Other = DraftLoadout.HeroIds.IndexOfByKey(Row.RowId);
			if (Other != INDEX_NONE) { DraftLoadout.HeroIds.Swap(SelectedHeroSlot, Other); }
			else { DraftLoadout.HeroIds[SelectedHeroSlot] = Row.RowId; }
			bDraftDirty = true;
		}
		SelectedRowId = Row.RowId;
		RebuildPanel();
		return;
	}

	if (Row.bToggleable && CurrentPanel == ELKHomePanel::WarRoom)
	{
		ToggleDraftCard(Row.RowId);
		return;
	}
	if (Row.bSelectable)
	{
		SelectedRowId = Row.RowId;
		RebuildPanel();
	}
}

void ULKHomeHUDWidget::HandleActionActivated(int32 Index)
{
	if (!HomeGameMode || !DisplayedModel.Actions.IsValidIndex(Index) || !DisplayedModel.Actions[Index].bEnabled) { return; }
	const FName ActionId = DisplayedModel.Actions[Index].ActionId;
	for (ULKHomeListButtonWidget* Widget : ActionWidgets) { Widget->SetIsEnabled(false); }
	ExecuteAction(ActionId);
}

FName ULKHomeHUDWidget::GetCurrentUpgradeBuildingId() const
{
	if (CurrentPanel == ELKHomePanel::Statue) { return LKHomeContent::BuildingId(ELKHomeBuilding::Statue); }
	if (CurrentPanel == ELKHomePanel::Treasury) { return LKHomeContent::BuildingId(ELKHomeBuilding::Treasury); }
	return NAME_None;
}

void ULKHomeHUDWidget::ToggleDraftCard(FName CardId)
{
	if (CardId.IsNone()) { return; }
	SelectedRowId = CardId;
	if (DraftLoadout.CardIds.Contains(CardId))
	{
		DraftLoadout.CardIds.Remove(CardId);
	}
	else if (DraftLoadout.CardIds.Num() < LKHomeContent::MaxStartingDeck())
	{
		DraftLoadout.CardIds.Add(CardId);
	}
	else
	{
		ApplyMessage(FText::FromString(FString::Printf(TEXT("初始牌组最多 %d 张，请先取消一张"), LKHomeContent::MaxStartingDeck())));
		return;
	}
	bDraftDirty = true;
	RebuildPanel();
}

void ULKHomeHUDWidget::ResetDraftFromSaved()
{
	const ULKProfileSubsystem* Profile = HomeGameMode ? HomeGameMode->GetProfileSubsystem() : nullptr;
	DraftLoadout = Profile ? Profile->GetSavedLoadout() : LKHomeContent::DefaultLoadout();
	bDraftDirty = false;
}

void ULKHomeHUDWidget::ExecuteAction(FName ActionId)
{
	if (!HomeGameMode) { return; }

	if (ActionId == TEXT("Abandon") || ActionId == TEXT("CancelAbandon"))
	{
		bConfirmAbandon = ActionId == TEXT("Abandon");
		LastMessage = FText::GetEmpty();
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("ConfirmAbandon"))
	{
		const bool bSuccess = HomeGameMode->RequestAbandonExpedition();
		bConfirmAbandon = !bSuccess;
		ApplyMessage(FText::FromString(bSuccess ? TEXT("已放弃远征，可以重新出征") : TEXT("放弃未完成，请检查存档状态后重试")));
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("Close"))
	{
		RequestClosePanel();
		return;
	}
	if (ActionId == TEXT("OpenWarRoom"))
	{
		OpenPanel(ELKHomePanel::WarRoom);
		return;
	}
	if (ActionId == TEXT("Tab0") || ActionId == TEXT("Tab1"))
	{
		BarracksTab = ActionId == TEXT("Tab0") ? 0 : 1;
		SelectedRowId = NAME_None;
		LastMessage = FText::GetEmpty();
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("Upgrade"))
	{
		const FName BuildingId = GetCurrentUpgradeBuildingId();
		if (BuildingId.IsNone()) { return; }
		const int32 Level = HomeGameMode->GetBuildingLevel(BuildingId);
		const ELKUpgradeResult Result = HomeGameMode->RequestUpgrade(BuildingId, Level);
		switch (Result)
		{
		case ELKUpgradeResult::Success:
			ApplyMessage(FText::FromString(FString::Printf(TEXT("升级成功：%s 已到 Lv%d"),
				*LKHomeContent::BuildingDisplayName(BuildingId).ToString(), HomeGameMode->GetBuildingLevel(BuildingId))));
			break;
		case ELKUpgradeResult::InsufficientGold: ApplyMessage(FText::FromString(TEXT("金币不足"))); break;
		case ELKUpgradeResult::MaxLevel: ApplyMessage(FText::FromString(TEXT("已满级"))); break;
		case ELKUpgradeResult::LevelMismatch: ApplyMessage(FText::FromString(TEXT("等级已变化，请重新打开面板"))); break;
		case ELKUpgradeResult::SaveFailed: ApplyMessage(FText::FromString(TEXT("存档写入失败：本次未扣金币，可重试"))); break;
		default: ApplyMessage(FText::FromString(TEXT("该建筑当前不可升级"))); break;
		}
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("SaveLoadout"))
	{
		const ELKLoadoutResult Result = HomeGameMode->RequestSaveLoadout(DraftLoadout);
		if (Result == ELKLoadoutResult::Success) { bDraftDirty = false; }
		ApplyMessage(LKHomeContent::LoadoutResultText(Result));
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("ResetDraft"))
	{
		ResetDraftFromSaved();
		ApplyMessage(FText::FromString(TEXT("已恢复为保存的战备")));
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("Start"))
	{
		const ELKExpeditionStartResult Result = HomeGameMode->RequestStartExpedition(LKHomeContent::DefaultRegionId());
		switch (Result)
		{
		case ELKExpeditionStartResult::Success: return; // 即将切图
		case ELKExpeditionStartResult::RunInProgress: ApplyMessage(FText::FromString(TEXT("已有远征进行中，请先继续或结束它"))); break;
		case ELKExpeditionStartResult::NoLoadout: ApplyMessage(FText::FromString(TEXT("战备无效，请先到战备处保存合法配置"))); break;
		case ELKExpeditionStartResult::RegionLocked: ApplyMessage(FText::FromString(TEXT("该区域未解锁"))); break;
		case ELKExpeditionStartResult::MapLoadFailed: ApplyMessage(FText::FromString(TEXT("地图加载失败：请检查战斗地图是否存在，可重试"))); break;
		case ELKExpeditionStartResult::SaveFailed: ApplyMessage(FText::FromString(TEXT("存档写入失败：本次未创建远征，可重试"))); break;
		default: ApplyMessage(FText::FromString(TEXT("无法出征"))); break;
		}
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("Continue"))
	{
		const ELKExpeditionStartResult Result = HomeGameMode->RequestContinueExpedition();
		if (Result == ELKExpeditionStartResult::Success) { return; }
		ApplyMessage(FText::FromString(TEXT("继续远征失败：请检查战斗地图是否存在")));
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("ConfirmSave"))
	{
		const ELKLoadoutResult Result = HomeGameMode->RequestSaveLoadout(DraftLoadout);
		ApplyMessage(LKHomeContent::LoadoutResultText(Result));
		if (Result == ELKLoadoutResult::Success)
		{
			bDraftDirty = false;
			bConfirmDiscard = false;
			CompleteDraftExit();
		}
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("ConfirmDiscard"))
	{
		ResetDraftFromSaved();
		bConfirmDiscard = false;
		CompleteDraftExit();
		ApplyMessage(FText::FromString(TEXT("已丢弃战备草稿")));
		RebuildPanel();
		return;
	}
	if (ActionId == TEXT("CancelClose"))
	{
		bConfirmDiscard = false;
		RebuildPanel();
		return;
	}
}

void ULKHomeHUDWidget::HandleHomeRefreshRequested() { Refresh(); }
void ULKHomeHUDWidget::HandleProfileChanged() { Refresh(); }

void ULKHomeHUDWidget::CompleteDraftExit()
{
    const ELKHomePanel Destination = PendingPanel;
    PendingPanel = ELKHomePanel::None;
    CurrentPanel = ELKHomePanel::None;
    bConfirmDiscard = false;
    if (Destination != ELKHomePanel::None) { OpenPanel(Destination); }
}

void ULKHomeHUDWidget::UpdateDetail(const FLKHomePanelModel& Model)
{
    if (!DetailHost) { return; }
    DetailHost->SetVisibility(bConfirmDiscard || bConfirmAbandon ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (bConfirmDiscard || bConfirmAbandon) { return; }
    const FLKHomePanelRow* Selected = Model.Rows.FindByPredicate([this](const FLKHomePanelRow& Row)
        { return !SelectedRowId.IsNone() && Row.RowId == SelectedRowId && Row.HeroSlotIndex == INDEX_NONE; });
    FText Heading = Selected ? Selected->Label : Model.Title;
    FText Copy = Selected && !Selected->Detail.IsEmpty() ? Selected->Detail : Model.Subtitle;
    FName Id = Selected ? Selected->RowId : GetCurrentUpgradeBuildingId();
    if (CurrentPanel == ELKHomePanel::Statue || CurrentPanel == ELKHomePanel::Treasury)
    {
        FString Values;
        for (const FLKHomePanelRow& Row : Model.Rows)
        {
            if (Row.Label.ToString().Contains(TEXT("当前")) || Row.Label.ToString().Contains(TEXT("升级后")))
            { Values += Row.Label.ToString() + TEXT("：") + Row.Value.ToString() + TEXT("\n"); }
        }
        Copy = FText::FromString(Values + TEXT("升级将在下一次新远征中生效。"));
    }
    if (CurrentPanel == ELKHomePanel::WarRoom)
    {
        Copy = FText::FromString((bWarRoomHeroes
            ? FString::Printf(TEXT("选中英雄槽 %d，点击下方英雄替换或交换。\n\n"), SelectedHeroSlot + 1)
            : FString(TEXT("点击卡牌切换携带，勾选表示已加入牌组。\n\n"))) + Copy.ToString());
    }
    DetailTitle->SetText(Heading);
    DetailCopy->SetText(Copy);
    DetailBadge->SetBrushColor(LKHomeUIStyle::Accent(Id) * 0.7f);
    DetailGlyph->SetText(FText::FromString(Heading.ToString().Left(1)));
}
