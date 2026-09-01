#include "ALKOpponentBrain.h"

#include "Engine/DataTable.h"

#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "LKDataTypes.h"
#include "LKLog.h"
#include "ULKCardDefinition.h"
#include "ULKDeckState.h"
#include "ULKGameData.h"
#include "ULKSilverComponent.h"

ALKOpponentBrain::ALKOpponentBrain()
{
	PrimaryActorTick.bCanEverTick = false;

	Silver = CreateDefaultSubobject<ULKSilverComponent>(TEXT("Silver"));
	Deck = CreateDefaultSubobject<ULKDeckState>(TEXT("Deck"));
}

void ALKOpponentBrain::InitBrain(ULKGameData* InGameData, ALKBattleGameMode* InGameMode)
{
	GameData = InGameData;
	GameMode = InGameMode;

	if (!GameData || !GameMode.IsValid())
	{
		return;
	}

	Silver->Init(GameData->SilverPerSecond, GameData->SilverCap);
	Deck->InitDeck(GameData->DefaultEnemyDeck, GameData->HandSize);
	Deck->CostProvider = [this](FName CardId) { return GameMode.IsValid() ? GameMode->GetCardCost(CardId) : 2; };

	LoadWaves();

	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 敌方 AI 就绪: 波次 %d, 牌库 %d 张"), Waves.Num(), Deck->GetDeckSize() + Deck->GetHandSize());
}

void ALKOpponentBrain::LoadWaves()
{
	Waves.Reset();
	WaveIndex = 0;

	if (GameData && !GameData->WaveTable.IsNull())
	{
		if (UDataTable* Table = GameData->WaveTable.LoadSynchronous())
		{
			for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
			{
				if (const FLKWaveRow* Row = reinterpret_cast<const FLKWaveRow*>(Pair.Value))
				{
					FLKWaveEntry Entry;
					Entry.Time = Row->Time;
					Entry.UnitId = Row->UnitId;
					Entry.Count = Row->Count;
					Waves.Add(Entry);
				}
			}
			Waves.Sort([](const FLKWaveEntry& A, const FLKWaveEntry& B) { return A.Time < B.Time; });
			return;
		}
	}

	// 内置示例波次（无 DT_Waves 资产时兜底）
	Waves = {
		{ 5.f,   TEXT("Unit_Swordsman"),    1 },
		{ 25.f,  TEXT("Unit_Swordsman"),    2 },
		{ 50.f,  TEXT("Unit_Archer"),       1 },
		{ 80.f,  TEXT("Unit_Shieldbearer"), 1 },
		{ 120.f, TEXT("Unit_Swordsman"),    2 },
		{ 160.f, TEXT("Unit_Archer"),       2 },
		{ 210.f, TEXT("Unit_Shieldbearer"), 2 },
		{ 280.f, TEXT("Building_ArrowTower"), 1 },
		{ 360.f, TEXT("Unit_Swordsman"),    3 },
	};
}

void ALKOpponentBrain::TickBrain(float DeltaTime, float BattleElapsed)
{
	if (!GameData || !GameMode.IsValid())
	{
		return;
	}

	Silver->TickSilver(DeltaTime);

	ProcessWaves(BattleElapsed);

	ThinkTimer -= DeltaTime;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = ThinkInterval;
		ThinkAndPlay();
	}
}

void ALKOpponentBrain::ProcessWaves(float BattleElapsed)
{
	if (!GameMode.IsValid())
	{
		return;
	}

	while (WaveIndex < Waves.Num() && Waves[WaveIndex].Time <= BattleElapsed)
	{
		const FLKWaveEntry& Entry = Waves[WaveIndex];
		for (int32 i = 0; i < Entry.Count; ++i)
		{
			const float HalfW = GameData->FieldHalfWidth;
			const float HalfH = GameData->FieldHalfHeight;
			// 敌方右半侧（Y>0），前中场位置
			const FVector Loc(
				FMath::FRandRange(-0.8f, 0.8f) * HalfW,
				FMath::FRandRange(0.15f, 0.45f) * HalfH,
				0.f);
			GameMode->SpawnUnitForTeam(Entry.UnitId, ELKTeam::Enemy, Loc);
		}
		UE_LOG(LogLKBattle, Log, TEXT("[Brain] 波次触发 t=%.0f: %s x%d"), Entry.Time, *Entry.UnitId.ToString(), Entry.Count);
		++WaveIndex;
	}
}

void ALKOpponentBrain::ThinkAndPlay()
{
	ULKDeckState* DeckState = Deck;
	ULKSilverComponent* SilverComp = Silver;
	ALKBattleGameMode* GM = GameMode.Get();

	if (!DeckState || !SilverComp || !GM)
	{
		return;
	}

	// 找最便宜的可打手牌
	int32 BestIndex = -1;
	int32 BestCost = TNumericLimits<int32>::Max();

	for (int32 i = 0; i < DeckState->GetHandSize(); ++i)
	{
		const FName CardId = DeckState->GetHandCard(i);
		const ULKCardDefinition* Def = GM->FindCard(CardId);
		if (!Def)
		{
			continue;
		}
		if (Def->CardType == ELKCardType::Spell && !GM->CanCastSpell(ELKTeam::Enemy))
		{
			continue; // 法术门：敌方同样需要法师在场
		}
		if (Def->Cost < BestCost)
		{
			BestCost = Def->Cost;
			BestIndex = i;
		}
	}

	if (BestIndex < 0 || SilverComp->GetSilver() < BestCost)
	{
		return;
	}

	FVector Location;
	if (!PickTargetLocation(DeckState->GetHandCard(BestIndex), Location))
	{
		return;
	}

	const ELKPlayResult Result = GM->PlayCardForTeam(ELKTeam::Enemy, BestIndex, Location);
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 打出 %s @ %s -> %d"),
		*DeckState->GetHandCard(BestIndex).ToString(), *Location.ToString(), (int32)Result);
}

bool ALKOpponentBrain::PickTargetLocation(FName CardId, FVector& OutLocation) const
{
	ALKBattleGameMode* GM = GameMode.Get();
	if (!GM || !GameData)
	{
		return false;
	}

	const ULKCardDefinition* Def = GM->FindCard(CardId);
	const float HalfW = GameData->FieldHalfWidth;
	const float HalfH = GameData->FieldHalfHeight;

	// 法术：瞄准玩家随机英雄
	if (Def && Def->CardType == ELKCardType::Spell)
	{
		const ALKUnitBase* Hero = GM->GetRandomAliveHero(ELKTeam::Player);
		if (!Hero)
		{
			return false;
		}
		OutLocation = Hero->GetActorLocation() + FVector(FMath::FRandRange(-100.f, 100.f), FMath::FRandRange(-100.f, 100.f), 0.f);
		return true;
	}

	// 建筑：后场（Y 深处）；角色：前中场 —— 敌方右半侧（Y>0）
	if (Def && Def->CardType == ELKCardType::Building)
	{
		OutLocation = FVector(FMath::FRandRange(-0.7f, 0.7f) * HalfW, FMath::FRandRange(0.65f, 0.9f) * HalfH, 0.f);
	}
	else
	{
		OutLocation = FVector(FMath::FRandRange(-0.8f, 0.8f) * HalfW, FMath::FRandRange(0.1f, 0.35f) * HalfH, 0.f);
	}
	return true;
}
