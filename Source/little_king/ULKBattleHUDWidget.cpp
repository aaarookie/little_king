#include "ULKBattleHUDWidget.h"

#include "Engine/World.h"

#include "ALKBattleGameMode.h"
#include "ALKBattleGameState.h"
#include "ALKPlayerController.h"
#include "LKLog.h"
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
}

void ULKBattleHUDWidget::HandleHandChanged()
{
	TArray<FName> Hand;
	TArray<int32> Costs;

	if (ULKDeckState* Deck = GetDeck())
	{
		Hand = Deck->GetHand();
		for (int32 i = 0; i < Hand.Num(); ++i)
		{
			Costs.Add(Deck->GetHandCost(i));
		}
	}

	OnHandChanged(Hand, Costs);
}

void ULKBattleHUDWidget::HandleSilverChanged(float NewSilver, float Delta)
{
	const float Cap = GetSilverComp() ? GetSilverComp()->GetCap() : 0.f;
	OnSilverChanged(NewSilver, Cap, Delta);
}

void ULKBattleHUDWidget::HandleMatchEnded(ELKTeam Winner)
{
	OnMatchEnded(Winner);
}

void ULKBattleHUDWidget::HandlePlacementStateChanged(bool bPlacing, ELKPlacementMode Mode, int32 HandIndex, FName ItemId)
{
	OnPlacementStateChanged(bPlacing, Mode, HandIndex, ItemId);
}

void ULKBattleHUDWidget::HandlePlayResult(ELKPlayResult Result)
{
	OnPlayResult(Result);
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
