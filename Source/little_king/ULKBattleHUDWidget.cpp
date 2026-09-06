#include "ULKBattleHUDWidget.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "ULKGameData.h"

#include "ALKBattleGameMode.h"
#include "ALKBattleGameState.h"
#include "ALKPlayerController.h"
#include "LKLog.h"
#include "ULKCardDefinition.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"

void ULKBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindEvents();
	PushInitialState();
}

void ULKBattleHUDWidget::NativeDestruct()
{
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
}

void ULKBattleHUDWidget::HandlePhaseChanged(ELKGamePhase NewPhase)
{
	OnPhaseChanged(NewPhase);
	HandleHandChanged();
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
}
