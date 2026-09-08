#include "ULKBattleHUDWidget.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "ULKGameData.h"

#include "ALKBattleGameMode.h"
#include "ALKBattleGameState.h"
#include "ALKPlayerController.h"
#include "LKLog.h"
#include "ULKCardDefinition.h"
#include "ULKDeckState.h"
#include "ULKRunNodeSelectWidget.h"
#include "ULKRunResumeWidget.h"
#include "ULKRunRewardWidget.h"
#include "ULKSilverComponent.h"

namespace
{
	UTextBlock* FindFirstTextBlock(UWidget* Widget)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			return Text;
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (UTextBlock* Text = FindFirstTextBlock(Panel->GetChildAt(Index)))
				{
					return Text;
				}
			}
		}
		return nullptr;
	}
}

void ULKBattleHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindResultActionButton();
}

void ULKBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindEvents();
	// Widget 从视口移除后再次加入时 NativeOnInitialized 不会重跑，因此在这里幂等重绑。
	BindResultActionButton();
	PushInitialState();
	// D5：冷启动恢复到结算/终态时，部署阶段直接弹对应面板。
	TryShowRecoveryPanels();
}

void ULKBattleHUDWidget::NativeDestruct()
{
	CloseRewardPanel();
	CloseNodeSelectPanel();
	CloseResumePanel();
	if (ResultActionButton)
	{
		ResultActionButton->OnClicked.RemoveDynamic(this, &ULKBattleHUDWidget::HandleResultActionClicked);
		ResultActionButton = nullptr;
	}
	UnbindEvents();
	Super::NativeDestruct();
}

void ULKBattleHUDWidget::BindEvents()
{
	if (ALKBattleGameState* GS = GetBattleGameState())
	{
		GS->OnPhaseChanged.AddDynamic(this, &ULKBattleHUDWidget::HandlePhaseChanged);
		GS->OnMatchEnded.AddDynamic(this, &ULKBattleHUDWidget::HandleMatchEnded);
	}

	if (ALKPlayerController* PC = GetLKPlayerController())
	{
		PC->OnPlacementStateChanged.AddDynamic(this, &ULKBattleHUDWidget::HandlePlacementStateChanged);
		PC->OnPlayResult.AddDynamic(this, &ULKBattleHUDWidget::HandlePlayResult);
	}

	if (ULKDeckState* Deck = GetDeck())
	{
		Deck->OnHandChanged.AddDynamic(this, &ULKBattleHUDWidget::HandleHandChanged);
	}

	// 战斗表现事件：法师死亡不会改变手牌可选状态。
	if (ALKBattleGameMode* GM = GetBattleGameMode())
	{


		// S5 飘字数据链：伤害/治疗事件
		GM->OnDamageEvent.AddDynamic(this, &ULKBattleHUDWidget::HandleDamageEvent);
	}

	if (ULKSilverComponent* Silver = GetSilverComp())
	{
		Silver->OnSilverChanged.AddDynamic(this, &ULKBattleHUDWidget::HandleSilverChanged);
	}
}

void ULKBattleHUDWidget::UnbindEvents()
{
	if (ALKBattleGameState* GS = GetBattleGameState())
	{
		GS->OnPhaseChanged.RemoveDynamic(this, &ULKBattleHUDWidget::HandlePhaseChanged);
		GS->OnMatchEnded.RemoveDynamic(this, &ULKBattleHUDWidget::HandleMatchEnded);
	}

	if (ALKPlayerController* PC = GetLKPlayerController())
	{
		PC->OnPlacementStateChanged.RemoveDynamic(this, &ULKBattleHUDWidget::HandlePlacementStateChanged);
		PC->OnPlayResult.RemoveDynamic(this, &ULKBattleHUDWidget::HandlePlayResult);
	}

	if (ULKDeckState* Deck = GetDeck())
	{
		Deck->OnHandChanged.RemoveDynamic(this, &ULKBattleHUDWidget::HandleHandChanged);
	}

	if (ALKBattleGameMode* GM = GetBattleGameMode())
	{

		GM->OnDamageEvent.RemoveDynamic(this, &ULKBattleHUDWidget::HandleDamageEvent);
	}

	if (ULKSilverComponent* Silver = GetSilverComp())
	{
		Silver->OnSilverChanged.RemoveDynamic(this, &ULKBattleHUDWidget::HandleSilverChanged);
	}
}

void ULKBattleHUDWidget::PushInitialState()
{
	if (ALKBattleGameState* GS = GetBattleGameState())
	{
		HandlePhaseChanged(GS->Phase);
	}

	HandleHandChanged();

	if (ULKSilverComponent* Silver = GetSilverComp())
	{
		HandleSilverChanged(Silver->GetSilver(), 0.f);
	}
	RefreshResultActionButton();
	RefreshRewardPanel();
}

void ULKBattleHUDWidget::HandlePhaseChanged(ELKGamePhase NewPhase)
{
	OnPhaseChanged(NewPhase);
	HandleHandChanged();
	RefreshResultActionButton();
}

void ULKBattleHUDWidget::HandleHandChanged()
{
    TArray<FName> Hand;
    TArray<int32> Costs;
    TArray<bool> Playable;
    if (ULKDeckState* Deck = GetDeck())
    {
        ALKBattleGameMode* GM = GetBattleGameMode();
        Hand = Deck->GetHand();
        for (int32 Index = 0; Index < Hand.Num(); ++Index)
        {
            const int32 Cost = Deck->GetHandCost(Index);
            Costs.Add(Cost);
            Playable.Add(GM && GM->GetPhase() == ELKGamePhase::Battle && GM->FindCard(Hand[Index])
                && GetSilverComp() && GetSilverComp()->GetSilver() >= Cost);
        }
    }
    OnHandChanged(Hand, Costs, Playable);
}

void ULKBattleHUDWidget::HandleSpellLockChanged(bool bUnlocked)
{
	// 兼容旧绑定，仅刷新费用/空槽/阶段，不锁定法术。
	HandleHandChanged();
}

void ULKBattleHUDWidget::HandleSilverChanged(float NewSilver, float Delta)
{
	const float Cap = GetSilverComp() ? GetSilverComp()->GetCap() : 0.f;
	OnSilverChanged(NewSilver, Cap, Delta);
    const int32 WholeSilver = FMath::FloorToInt(NewSilver);
    if (WholeSilver != LastWholeSilver) { LastWholeSilver = WholeSilver; HandleHandChanged(); }
}

void ULKBattleHUDWidget::HandleMatchEnded(ELKTeam Winner)
{
	OnMatchEnded(Winner);
	SyncResultPanels();

	// D3/D4：原生面板生命周期由 C++ 负责；事件仅作为蓝图追加表现通知。
	ALKBattleGameMode* GM = GetBattleGameMode();
	if (GM)
	{
		if (GM->HasPendingRewardChoice()) { OnRunRewardReadyBP(GM->GetPendingRewardCount()); }
		else if (GM->CanSelectNextNode()) { OnNodeSelectReadyBP(GM->GetNextNodeCount()); }
	}
}

void ULKBattleHUDWidget::BindResultActionButton()
{
	ResultActionButton = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Restart")));
	if (!ResultActionButton)
	{
		ResultActionButton = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Next")));
	}
	if (!ResultActionButton)
	{
		UE_LOG(LogLKBattle, Warning, TEXT("[Run] HUD 未找到 Btn_Restart/Btn_Next，结算流程按钮无法绑定"));
		return;
	}

	// 旧 WBP 的点击事件曾直接 OpenLevel 且已失效；D1 统一交给远征状态机处理。
	ResultActionButton->OnClicked.Clear();
	ResultActionButton->OnClicked.AddDynamic(this, &ULKBattleHUDWidget::HandleResultActionClicked);
	RefreshResultActionButton();
}

void ULKBattleHUDWidget::RefreshResultActionButton()
{
	if (!ResultActionButton) { return; }
	const ALKBattleGameMode* GM = GetBattleGameMode();
	// D3/D4：待领奖励或存在下一排可选节点时禁用结算按钮（由对应面板驱动）。
	ResultActionButton->SetIsEnabled(GM && GM->GetPhase() == ELKGamePhase::Result
		&& !GM->HasPendingRewardChoice() && !GM->CanSelectNextNode());
	if (UTextBlock* Label = FindFirstTextBlock(ResultActionButton))
	{
		Label->SetText(GetResultActionLabel());
	}
}

void ULKBattleHUDWidget::HandleResultActionClicked()
{
	if (!ResultActionButton) { return; }
	ResultActionButton->SetIsEnabled(false);
	if (!RequestResultAction())
	{
		RefreshResultActionButton();
	}
}

bool ULKBattleHUDWidget::RequestResultAction()
{
	ALKBattleGameMode* GM = GetBattleGameMode();
	return GM && GM->RequestResultAction();
}

FText ULKBattleHUDWidget::GetResultActionLabel() const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->GetResultActionLabel() : FText::FromString(TEXT("从头开始"));
}

// ---------- D3 奖励便捷入口 ----------
bool ULKBattleHUDWidget::HasPendingRewardChoice() const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM && GM->HasPendingRewardChoice();
}

int32 ULKBattleHUDWidget::GetPendingRewardCount() const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->GetPendingRewardCount() : 0;
}

FText ULKBattleHUDWidget::GetRewardOptionText(int32 Index) const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	if (!GM) { return FText::GetEmpty(); }
	const FLKRunRewardOffer Offer = GM->GetRunRewardOffer(Index);
	if (Offer.CardId.IsNone()) { return FText::GetEmpty(); }

	const ULKCardDefinition* Def = GetCardDefinition(Offer.CardId);
	const FString CardName = Def ? Def->CardName.ToString() : Offer.CardId.ToString();

	if (Offer.Kind == ELKRunRewardKind::AddCard)
	{
		const int32 Cost = Def ? Def->Cost : 0;
		return FText::FromString(FString::Printf(TEXT("获得新卡「%s」（%d 费）"), *CardName, Cost));
	}
	if (Offer.LevelBefore > 0)
	{
		return FText::FromString(FString::Printf(
			TEXT("升级「%s」Lv%d → Lv%d：攻击/生命 +10%%"), *CardName, Offer.LevelBefore, Offer.LevelAfter));
	}
	return FText::FromString(FString::Printf(
		TEXT("升级「%s」→ Lv%d：攻击/生命 +10%%"), *CardName, Offer.LevelAfter));
}

bool ULKBattleHUDWidget::ChooseRunReward(int32 Index)
{
	ALKBattleGameMode* GM = GetBattleGameMode();
	const bool bChosen = GM && GM->ChooseRunReward(Index);
	if (bChosen) { CloseRewardPanel(); SyncResultPanels(); }
	return bChosen;
}

bool ULKBattleHUDWidget::SkipRunReward()
{
	ALKBattleGameMode* GM = GetBattleGameMode();
	const bool bSkipped = GM && GM->SkipRunReward();
	if (bSkipped) { CloseRewardPanel(); SyncResultPanels(); }
	return bSkipped;
}

bool ULKBattleHUDWidget::IsRewardPanelOpen() const
{
	return IsValid(RewardWidget);
}

void ULKBattleHUDWidget::RefreshRewardPanel()
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	// D5：恢复场景在部署阶段也会弹出奖励面板（战斗结算仍是 Result 阶段）。
	const bool bPanelPhase = GM && (GM->GetPhase() == ELKGamePhase::Result || GM->GetPhase() == ELKGamePhase::Deployment);
	const bool bShouldShow = bPanelPhase && GM->HasPendingRewardChoice();
	if (bShouldShow)
	{
		if (!RewardWidget) { ShowRewardPanel(); }
	}
	else if (RewardWidget)
	{
		CloseRewardPanel();
	}
}

void ULKBattleHUDWidget::ShowRewardPanel()
{
	if (RewardWidget || !GetBattleGameMode() || !GetBattleGameMode()->HasPendingRewardChoice()) { return; }
	TSubclassOf<ULKRunRewardWidget> Class = RewardWidgetClass;
	if (!Class) { Class = ULKRunRewardWidget::StaticClass(); }
	RewardWidget = CreateWidget<ULKRunRewardWidget>(GetWorld(), Class);
	if (!RewardWidget)
	{
		UE_LOG(LogLKBattle, Error, TEXT("[RunUI] 奖励面板创建失败"));
		return;
	}
	RewardWidget->InitializeReward(this);
	RewardWidget->AddToViewport(100);
	UE_LOG(LogLKBattle, Log, TEXT("[RunUI] 奖励面板已显示：%d 项"), GetPendingRewardCount());
}

void ULKBattleHUDWidget::CloseRewardPanel()
{
	if (!RewardWidget) { return; }
	RewardWidget->RemoveFromParent();
	RewardWidget = nullptr;
}

// ---------- D4 节点选择 ----------
bool ULKBattleHUDWidget::CanSelectNextNode() const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM && GM->CanSelectNextNode();
}

int32 ULKBattleHUDWidget::GetNextNodeCount() const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->GetNextNodeCount() : 0;
}

FName ULKBattleHUDWidget::GetNextNodeId(int32 Index) const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->GetNextNodeId(Index) : NAME_None;
}

FText ULKBattleHUDWidget::GetNextNodeTitle(int32 Index) const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->GetNextNodeTitle(Index) : FText::GetEmpty();
}

FText ULKBattleHUDWidget::GetNextNodeSubtitle(int32 Index) const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->GetNextNodeSubtitle(Index) : FText::GetEmpty();
}

ELKNodeSelectionResult ULKBattleHUDWidget::SelectNextNode(int32 Index)
{
	ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->SelectNextNode(Index) : ELKNodeSelectionResult::Rejected;
}

bool ULKBattleHUDWidget::IsNodeSelectPanelOpen() const
{
	return IsValid(NodeSelectWidget);
}

void ULKBattleHUDWidget::RefreshNodeSelectPanel()
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	// D5：恢复场景在部署阶段也会弹出节点选择面板。
	const bool bPanelPhase = GM && (GM->GetPhase() == ELKGamePhase::Result || GM->GetPhase() == ELKGamePhase::Deployment);
	const bool bShouldShow = bPanelPhase
		&& !GM->HasPendingRewardChoice() && GM->CanSelectNextNode();
	if (bShouldShow)
	{
		if (!NodeSelectWidget) { ShowNodeSelectPanel(); }
		else { NodeSelectWidget->RefreshNodeSelect(); }
	}
	else if (NodeSelectWidget)
	{
		CloseNodeSelectPanel();
	}
}

void ULKBattleHUDWidget::ShowNodeSelectPanel()
{
	if (NodeSelectWidget || !GetBattleGameMode() || !GetBattleGameMode()->CanSelectNextNode()
		|| GetBattleGameMode()->HasPendingRewardChoice())
	{
		return;
	}
	TSubclassOf<ULKRunNodeSelectWidget> Class = NodeSelectWidgetClass;
	if (!Class) { Class = ULKRunNodeSelectWidget::StaticClass(); }
	NodeSelectWidget = CreateWidget<ULKRunNodeSelectWidget>(GetWorld(), Class);
	if (!NodeSelectWidget)
	{
		UE_LOG(LogLKBattle, Error, TEXT("[RunUI] 节点选择面板创建失败"));
		return;
	}
	NodeSelectWidget->InitializeNodeSelect(this);
	NodeSelectWidget->AddToViewport(101);
	UE_LOG(LogLKBattle, Log, TEXT("[RunUI] 节点选择面板已显示：%d 个选项"), GetNextNodeCount());
}

void ULKBattleHUDWidget::CloseNodeSelectPanel()
{
	if (!NodeSelectWidget) { return; }
	NodeSelectWidget->RemoveFromParent();
	NodeSelectWidget = nullptr;
}

void ULKBattleHUDWidget::SyncResultPanels()
{
	// 顺序：奖励优先（若有）→ 否则有可选节点则显示节点选择 → 最后刷新结算按钮。
	RefreshRewardPanel();
	RefreshNodeSelectPanel();
	RefreshResultActionButton();
}

// ---------- D5 恢复/终态入口 ----------
void ULKBattleHUDWidget::TryShowRecoveryPanels()
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	if (!GM || GM->GetPhase() != ELKGamePhase::Deployment) { return; }
	if (GM->IsRecoveryTerminal())
	{
		ShowResumePanel();
		return;
	}
	if (GM->IsRecoveryJunction())
	{
		// 选择中断：优先奖励 → 节点选择；两者都无可用时给摘要入口。
		RefreshRewardPanel();
		RefreshNodeSelectPanel();
		if (!GM->HasPendingRewardChoice() && !GM->CanSelectNextNode()) { ShowResumePanel(); }
	}
}

void ULKBattleHUDWidget::ContinueFromRecoveryJunction()
{
	CloseResumePanel();
	SyncResultPanels();
}

bool ULKBattleHUDWidget::StartNewRunFromRecovery()
{
	ALKBattleGameMode* GM = GetBattleGameMode();
	return GM && GM->StartNewRunFromRecovery();
}

bool ULKBattleHUDWidget::IsResumePanelOpen() const
{
	return IsValid(ResumeWidget);
}

void ULKBattleHUDWidget::RefreshResumePanel()
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	const bool bShouldShow = GM && (GM->IsRecoveryTerminal()
		|| (GM->IsRecoveryJunction() && !GM->HasPendingRewardChoice() && !GM->CanSelectNextNode()));
	if (bShouldShow)
	{
		if (!ResumeWidget) { ShowResumePanel(); }
		else { ResumeWidget->RefreshResume(); }
	}
	else if (ResumeWidget)
	{
		CloseResumePanel();
	}
}

void ULKBattleHUDWidget::ShowResumePanel()
{
	if (ResumeWidget) { return; }
	TSubclassOf<ULKRunResumeWidget> Class = ResumeWidgetClass;
	if (!Class) { Class = ULKRunResumeWidget::StaticClass(); }
	ResumeWidget = CreateWidget<ULKRunResumeWidget>(GetWorld(), Class);
	if (!ResumeWidget)
	{
		UE_LOG(LogLKBattle, Error, TEXT("[RunUI] 恢复面板创建失败"));
		return;
	}
	ResumeWidget->InitializeResume(this);
	ResumeWidget->AddToViewport(102);
	UE_LOG(LogLKBattle, Log, TEXT("[RunUI] 恢复/终态面板已显示"));
}

void ULKBattleHUDWidget::CloseResumePanel()
{
	if (!ResumeWidget) { return; }
	ResumeWidget->RemoveFromParent();
	ResumeWidget = nullptr;
}

void ULKBattleHUDWidget::HandleDamageEvent(FVector WorldLocation, float Amount, bool bIsHeal)
{
    const ALKBattleGameMode* GM = GetBattleGameMode();
    if (!GM || !GM->GetGameData() || !GM->GetGameData()->bNativeDamageText)
    { OnDamageEventBP(WorldLocation, Amount, bIsHeal); }
}

bool ULKBattleHUDWidget::WorldToScreen(FVector WorldLocation, FVector2D& OutScreenLocation) const
{
	ALKPlayerController* PC = GetLKPlayerController();
	if (!PC)
	{
		return false;
	}

	FVector2D ScreenPos;
	if (!PC->ProjectWorldLocationToScreen(WorldLocation, ScreenPos))
	{
		return false;
	}

	// 视口坐标的原点在左上角，与锚点对齐
	OutScreenLocation = ScreenPos;
	return true;
}

void ULKBattleHUDWidget::HandlePlacementStateChanged(bool bPlacing, ELKPlacementMode Mode, int32 HandIndex, FName ItemId)
{
	OnPlacementStateChanged(bPlacing, Mode, HandIndex, ItemId);
}

void ULKBattleHUDWidget::HandlePlayResult(ELKPlayResult Result)
{
    // The native presentation owns the updated territory message; old BP branches say "spell locked".
    if (Result != ELKPlayResult::SpellLocked) { OnPlayResult(Result); }
}

ALKPlayerController* ULKBattleHUDWidget::GetLKPlayerController() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	return Cast<ALKPlayerController>(PC);
}

ULKDeckState* ULKBattleHUDWidget::GetDeck() const
{
	const ALKPlayerController* PC = GetLKPlayerController();
	return PC ? PC->GetDeckState() : nullptr;
}

ULKSilverComponent* ULKBattleHUDWidget::GetSilverComp() const
{
	const ALKPlayerController* PC = GetLKPlayerController();
	return PC ? PC->GetSilverComponent() : nullptr;
}

ALKBattleGameState* ULKBattleHUDWidget::GetBattleGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<ALKBattleGameState>() : nullptr;
}

ALKBattleGameMode* ULKBattleHUDWidget::GetBattleGameMode() const
{
	UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
}

ULKCardDefinition* ULKBattleHUDWidget::GetCardDefinition(FName CardId) const
{
	const ALKBattleGameMode* GM = GetBattleGameMode();
	return GM ? GM->FindCard(CardId) : nullptr;
}

UTexture2D* ULKBattleHUDWidget::GetCardIcon(FName CardId) const
{
	const ULKCardDefinition* Card = GetCardDefinition(CardId);
	return Card ? Card->Icon.LoadSynchronous() : nullptr;
}

void ULKBattleHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    // Migration bridge until the old aggregate widgets are deleted in the editor.
    for (FName Name : {FName("PlayerHeroBar"), FName("EnemyHeroBar")})
    { if (UWidget* Widget = GetWidgetFromName(Name)) { Widget->SetVisibility(ESlateVisibility::Collapsed); } }
	if (UButton* Start = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Start"))))
	{ Start->SetIsEnabled(GetBattleGameMode() && GetBattleGameMode()->CanStartBattle()); }
	// 幂等恢复各结果/恢复面板（含 HUD 晚创建或蓝图重新加入视口的情况）。
	SyncResultPanels();
	TryShowRecoveryPanels();
}
