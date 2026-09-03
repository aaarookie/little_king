#include "ULKBattleHUDWidget.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"

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

	// 法术门：法师英雄阵亡/开战时刷新（锁定的法术卡显示为不可打出）
	if (ALKBattleGameMode* GM = GetBattleGameMode())
	{
		GM->OnSpellLockChanged.AddDynamic(this, &ULKBattleHUDWidget::HandleSpellLockChanged);
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
		GM->OnSpellLockChanged.RemoveDynamic(this, &ULKBattleHUDWidget::HandleSpellLockChanged);
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
	TArray<bool> bPlayable;

	if (ULKDeckState* Deck = GetDeck())
	{
		ALKBattleGameMode* GM = GetBattleGameMode();

		Hand = Deck->GetHand();
		for (int32 i = 0; i < Hand.Num(); ++i)
		{
			Costs.Add(Deck->GetHandCost(i));

			// 可打出判定：法术门锁定（无法师在场）时法术卡不可打出
			bool Playable = true;
			if (GM)
			{
				if (const ULKCardDefinition* Card = GM->FindCard(Hand[i]))
				{
					if (Card->CardType == ELKCardType::Spell && !GM->CanCastSpell(ELKTeam::Player))
					{
						Playable = false;
					}
				}
			}
			bPlayable.Add(Playable);
		}
	}

	OnHandChanged(Hand, Costs, bPlayable);
}

void ULKBattleHUDWidget::HandleSpellLockChanged(bool bUnlocked)
{
	// 法术锁定状态变化 -> 重推手牌（刷新可打出标记）
	HandleHandChanged();
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
