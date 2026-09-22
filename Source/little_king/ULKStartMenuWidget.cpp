#include "ULKStartMenuWidget.h"
#include "ULKJourneyPresentationSubsystem.h"
#include "LKPresentationStyle.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "LKHomeContent.h"
#include "ULKHomeListButtonWidget.h"
#include "ULKSaveSlotSubsystem.h"

namespace
{
	const FLinearColor Gold = LKPresentationStyle::Gold();
	const FLinearColor Muted = LKPresentationStyle::Muted();
	UTextBlock* Text(UWidgetTree* Tree, const FString& Copy, int32 Size, FLinearColor Color = LKPresentationStyle::Paper(), FName Name = NAME_None)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Result->GetFont(); Font.Size = Size;
		Result->SetFont(LKPresentationStyle::Font(Font.Size)); Result->SetText(FText::FromString(Copy));
		Result->SetColorAndOpacity(Color); Result->SetAutoWrapText(true);
		return Result;
	}
	UBorder* Panel(UWidgetTree* Tree, FLinearColor Color, float Padding = 20.f)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>();
		LKPresentationStyle::StylePanel(Result, Color); Result->SetPadding(FMargin(Padding));
		return Result;
	}
	FString RunDescription(const FLKSaveSlotSummary& Row)
	{
		if (!Row.bHasRun || Row.RunPhase == ELKRunPhase::Inactive) { return TEXT("家园"); }
		if (Row.RunPhase == ELKRunPhase::Completed || Row.RunPhase == ELKRunPhase::Failed || Row.RunPhase == ELKRunPhase::Abandoned) { return TEXT("远征已结束 · 返回家园"); }
		return TEXT("远征途中 · 继续冒险");
	}
}

void ULKStartMenuWidget::InitializeMenu(ULKSaveSlotSubsystem* InSaves) { Saves = InSaves; }
TSharedRef<SWidget> ULKStartMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildTree(); }
	return Super::RebuildWidget();
}
void ULKStartMenuWidget::NativeConstruct() { Super::NativeConstruct(); ULKJourneyPresentationSubsystem::Reveal(this); Refresh(); }
void ULKStartMenuWidget::BuildTree()
{
	UBorder* Background = Panel(WidgetTree, LKPresentationStyle::Ink(), 32.f);
	WidgetTree->RootWidget = Background;
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit); Background->SetContent(Scale);
	MenuFrame = WidgetTree->ConstructWidget<USizeBox>();
	MenuFrame->SetWidthOverride(1100); MenuFrame->SetHeightOverride(670); Scale->SetContent(MenuFrame);
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(); MenuFrame->SetContent(Layout);
	Layout->AddChildToVerticalBox(Text(WidgetTree, TEXT("L I T T L E   K I N G   /   小小国王"), 16, Gold))->SetPadding(FMargin(0, 0, 0, 14));
	PageTitle = Text(WidgetTree, TEXT(""), 34, FLinearColor::White, TEXT("PageTitle"));
	Layout->AddChildToVerticalBox(PageTitle);
	PageSubtitle = Text(WidgetTree, TEXT(""), 16, Muted);
	Layout->AddChildToVerticalBox(PageSubtitle)->SetPadding(FMargin(0, 8, 0, 22));
	Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBody"));
	Layout->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Status = Text(WidgetTree, TEXT(""), 16, Gold, TEXT("MenuStatus"));
	USizeBox* StatusSize = WidgetTree->ConstructWidget<USizeBox>(); StatusSize->SetHeightOverride(50); StatusSize->SetContent(Status);
	Layout->AddChildToVerticalBox(StatusSize)->SetPadding(FMargin(0, 14, 0, 0));
    Layout->AddChildToVerticalBox(Text(WidgetTree, TEXT("音乐：MiniMax-Music3 · AI 生成    字体：Noto Sans / Serif CJK"), 13, Muted));
}
void ULKStartMenuWidget::AddButton(UPanelWidget* Parent, const TCHAR* Name, const FString& Label, int32 Action, bool bEnabled, bool bPrimary)
{
	ULKHomeListButtonWidget* Entry = CreateWidget<ULKHomeListButtonWidget>(this, ULKHomeListButtonWidget::StaticClass(), FName(Name));
	FLKHomePanelAction Model; Model.Label = FText::FromString(Label); Model.bEnabled = bEnabled;
	Model.ActionId = bPrimary ? TEXT("Start") : TEXT("Menu");
	Entry->SetupAction(Action, Model);
	Entry->OnHomeListClicked.BindUObject(this, &ULKStartMenuWidget::HandleAction);
	if (UVerticalBox* Column = Cast<UVerticalBox>(Parent)) { Column->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0, 5)); }
	else if (UHorizontalBox* Row = Cast<UHorizontalBox>(Parent))
	{
        Entry->SetCompactRow(); // Eight-slot load grid must fit the Chinese font's line metrics.
		UHorizontalBoxSlot* LayoutSlot = Row->AddChildToHorizontalBox(Entry);
		LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LayoutSlot->SetPadding(FMargin(4, 2));
	}
}
void ULKStartMenuWidget::ShowMainMenu() { bLoadMenu = false; PendingDelete = INDEX_NONE; Refresh(); SetStatus(TEXT("")); }
void ULKStartMenuWidget::ShowLoadMenu() { bLoadMenu = true; PendingDelete = INDEX_NONE; Refresh(); SetStatus(TEXT("")); }
FString ULKStartMenuWidget::GetStatusText() const { return Status ? Status->GetText().ToString() : FString(); }
void ULKStartMenuWidget::SetStatus(const FString& Message) { if (Status) { Status->SetText(FText::FromString(Message)); } }
void ULKStartMenuWidget::Refresh()
{
	if (!Body || !Saves) { return; }
	Body->ClearChildren();
	MenuFrame->SetHeightOverride(bLoadMenu && PendingDelete == INDEX_NONE ? 840.f : 670.f);
	const TArray<FLKSaveSlotSummary> Rows = Saves->ListSlots();
	const int32 Used = Rows.FilterByPredicate([](const FLKSaveSlotSummary& Row) { return Row.bExists; }).Num();
	if (PendingDelete != INDEX_NONE)
	{
		PageTitle->SetText(FText::FromString(FString::Printf(TEXT("删除存档 %02d？"), PendingDelete + 1)));
		PageSubtitle->SetText(FText::FromString(TEXT("此操作无法撤销，请确认要删除的存档编号。")));
		UBorder* Card = Panel(WidgetTree, FLinearColor(0.09f, 0.035f, 0.038f), 32);
		UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Copy);
		const FLKSaveSlotSummary& Row = Rows[PendingDelete];
		Copy->AddChildToVerticalBox(Text(WidgetTree, FString::Printf(TEXT("存档 %02d    /    %d 金币"), PendingDelete + 1, Row.Gold), 26, Gold));
		Copy->AddChildToVerticalBox(Text(WidgetTree, TEXT("将永久删除这个存档的家园、解锁内容、出征配置与远征进度。\n其他存档会保留。"), 20))->SetPadding(FMargin(0, 26));
		Body->AddChildToVerticalBox(Card);
		AddButton(Body, TEXT("CancelDeleteButton"), TEXT("取消 · 保留存档"), 4, true, true);
		AddButton(Body, TEXT("ConfirmDeleteButton"), TEXT("确认删除"), 5);
		return;
	}
	if (!bLoadMenu)
	{
		PageTitle->SetText(FText::FromString(TEXT("从家园，走向未知")));
		PageSubtitle->SetText(FText::FromString(TEXT("集结英雄，筹备远征。每一次归来，都是新的开始。")));
		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UBorder* Scene = Panel(WidgetTree, LKPresentationStyle::Panel(), 30);
		UHorizontalBoxSlot* SceneSlot = Columns->AddChildToHorizontalBox(Scene); SceneSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); SceneSlot->SetPadding(FMargin(0, 0, 32, 0));
		UVerticalBox* SceneCopy = WidgetTree->ConstructWidget<UVerticalBox>(); Scene->SetContent(SceneCopy);
		SceneCopy->AddChildToVerticalBox(Text(WidgetTree, TEXT("H O M E   &   E X P E D I T I O N"), 14, Muted));
        UScaleBox* ArtScale = WidgetTree->ConstructWidget<UScaleBox>();
        ArtScale->SetStretch(EStretch::ScaleToFit);
        SceneCopy->AddChildToVerticalBox(ArtScale)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("KingdomIllustration"));
        Art->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/Art/StorybookV1/Textures/T_MenuKingdom.T_MenuKingdom")), true);
        Art->SetVisibility(ESlateVisibility::HitTestInvisible);
        ArtScale->SetContent(Art);
		USizeBox* MenuSize = WidgetTree->ConstructWidget<USizeBox>(); MenuSize->SetWidthOverride(375);
		Columns->AddChildToHorizontalBox(MenuSize)->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Actions = WidgetTree->ConstructWidget<UVerticalBox>(); MenuSize->SetContent(Actions);
		const int32 Recent = Saves->GetMostRecentSlot();
		const FString Hint = Recent != INDEX_NONE ? FString::Printf(TEXT("最近游戏 · 存档 %02d"), Recent + 1) : TEXT("暂无可继续的存档");
		Actions->AddChildToVerticalBox(Text(WidgetTree, Hint, 16, Muted))->SetPadding(FMargin(0, 0, 0, 12));
		AddButton(Actions, TEXT("ContinueButton"), TEXT("继续游戏"), 0, Recent != INDEX_NONE, true);
		AddButton(Actions, TEXT("NewGameButton"), Used == 8 ? TEXT("新的游戏 · 存档已满") : TEXT("新的游戏"), 1, Used < 8);
		AddButton(Actions, TEXT("LoadMenuButton"), FString::Printf(TEXT("读取存档    %d / 8"), Used), 2);
		AddButton(Actions, TEXT("SettingsButton"), TEXT("设置"), 3);
		Actions->AddChildToVerticalBox(Text(WidgetTree, Used == 8 ? TEXT("可在读取存档中删除旧档，腾出空位。") : TEXT("进度自动保存 · 最多 8 个独立存档"), 14, Muted))->SetPadding(FMargin(0, 14, 0, 0));
		return;
	}
	PageTitle->SetText(FText::FromString(TEXT("读取存档")));
	PageSubtitle->SetText(FText::FromString(FString::Printf(TEXT("已使用 %d / 8 个位置 · 每个存档拥有独立的家园与远征进度"), Used)));
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(); Grid->SetSlotPadding(FMargin(0, 0, 12, 8));
	Body->AddChildToVerticalBox(Grid)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	for (const FLKSaveSlotSummary& Row : Rows)
	{
		UBorder* Card = Panel(WidgetTree, LKPresentationStyle::Panel(), 12);
		UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Copy);
		FString Label = FString::Printf(TEXT("%02d    %s"), Row.Index + 1, Row.bExists ? *FString::Printf(TEXT("%d 金币   /   %s"), Row.Gold, *RunDescription(Row)) : TEXT("空存档"));
		Copy->AddChildToVerticalBox(Text(WidgetTree, Label, 17, Row.bExists ? FLinearColor::White : Muted));
		FString Detail = Row.bExists ? (Row.bCanLoad ? TEXT("最近保存  ") + Row.SavedAtUtc.ToIso8601().Left(19).Replace(TEXT("T"), TEXT(" ")) + TEXT(" UTC") : Row.Error) : TEXT("选择「新的游戏」时使用空位");
		Copy->AddChildToVerticalBox(Text(WidgetTree, Detail, 13, Row.bCanLoad || !Row.bExists ? Muted : Gold));
		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(); Copy->AddChildToVerticalBox(Buttons);
		AddButton(Buttons, *FString::Printf(TEXT("LoadSlot%dButton"), Row.Index), TEXT("读取"), 100 + Row.Index, Row.bCanLoad, true);
		AddButton(Buttons, *FString::Printf(TEXT("DeleteSlot%dButton"), Row.Index), TEXT("删除"), 200 + Row.Index, Row.bExists);
		UUniformGridSlot* Cell = Grid->AddChildToUniformGrid(Card, Row.Index / 2, Row.Index % 2);
		Cell->SetHorizontalAlignment(HAlign_Fill); Cell->SetVerticalAlignment(VAlign_Fill);
	}
	AddButton(Body, TEXT("BackButton"), TEXT("返回开始界面"), 4);
}
void ULKStartMenuWidget::RequestDelete(int32 SlotIndex)
{
	if (!Saves || bTravelQueued || !Saves->InspectSlot(SlotIndex).bExists) { return; }
	PendingDelete = SlotIndex; Refresh(); SetStatus(TEXT(""));
}
void ULKStartMenuWidget::EnterGame(int32 SlotIndex, bool bNew)
{
	if (!Saves || bTravelQueued) { return; }
	// 在创建/选择前先验证地图，避免缺失资产时留下无法继续的会话状态。
	if (!LKHomeContent::DoesMapExist(TEXT("L_Home")) || !LKHomeContent::DoesMapExist(TEXT("L_BattleTest")))
	{ SetStatus(TEXT("无法进入游戏：缺少家园或战斗地图，请检查项目内容。")); return; }
	if (!(bNew ? Saves->CreateNewGame() : Saves->LoadSlot(SlotIndex))) { SetStatus(Saves->GetLastError()); return; }
	SetStatus(TEXT("正在进入游戏…"));
	if (!bSuppressTravel)
	{
		bTravelQueued = true; SetIsEnabled(false);
		ULKJourneyPresentationSubsystem::Travel(this, Saves->GetEntryMap());
	}
}
void ULKStartMenuWidget::HandleAction(int32 Action)
{
	if (!Saves || bTravelQueued) { return; }
	if (Action >= 200 && Action < 208) { RequestDelete(Action - 200); return; }
	if (Action >= 100 && Action < 108) { EnterGame(Action - 100, false); return; }
	switch (Action)
	{
	case 0: EnterGame(Saves->GetMostRecentSlot(), false); break;
	case 1: EnterGame(INDEX_NONE, true); break;
	case 2: ShowLoadMenu(); break;
	case 3: OnSettingsRequested(); SetStatus(TEXT("设置暂未开放，后续版本加入。")); break;
	case 4: if (PendingDelete != INDEX_NONE) { ShowLoadMenu(); } else { ShowMainMenu(); } break;
	case 5:
		if (PendingDelete != INDEX_NONE)
		{
			if (Saves->DeleteSlot(PendingDelete)) { ShowLoadMenu(); SetStatus(TEXT("存档已删除，空位可用于新的游戏。")); }
			else { SetStatus(Saves->GetLastError()); }
		}
		break;
	default: break;
	}
}
