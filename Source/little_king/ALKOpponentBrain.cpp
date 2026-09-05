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

	// 集火节奏：首轮随机落在配置区间内
	FocusTimer = FMath::FRandRange(GameData->AIFocusIntervalMin, GameData->AIFocusIntervalMax);
	CounterTimer = GameData->AICounterCheckInterval;
	PushCooldownTimer = FMath::FRandRange(GameData->AIPushCooldownMin, GameData->AIPushCooldownMax);

	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 敌方 AI 就绪: 波次 %d, 牌库 %d 张（首轮集火 %.0f 秒后）"),
		Waves.Num(), Deck->GetDeckSize() + Deck->GetHandSize(), FocusTimer);
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

	// ---------- S4 战术节奏 ----------
	FocusTimer -= DeltaTime;
	if (FocusTimer <= 0.f)
	{
		DoFocus();
		FocusTimer = FMath::FRandRange(GameData->AIFocusIntervalMin, GameData->AIFocusIntervalMax);
	}

	CounterTimer -= DeltaTime;
	if (CounterTimer <= 0.f)
	{
		RefreshCounter();
		CounterTimer = GameData->AICounterCheckInterval;
	}

	PushCooldownTimer = FMath::Max(0.f, PushCooldownTimer - DeltaTime);

	// 思考出牌
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

void ALKOpponentBrain::DoFocus()
{
	ALKBattleGameMode* GM = GameMode.Get();
	if (!GM)
	{
		return;
	}

	// 目标：玩家血量比例最低的存活英雄
	ALKUnitBase* Weakest = GM->GetWeakestAliveHero(ELKTeam::Player);
	if (!Weakest)
	{
		return; // 玩家已无英雄（对局即将结束），跳过
	}

	GM->ForcedTargetAllUnits(ELKTeam::Enemy, Weakest, GameData->AIFocusDuration);
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 集火！敌方全体转火玩家英雄 %s（%.0f 秒）"),
		*Weakest->GetUnitId().ToString(), GameData->AIFocusDuration);
}

void ALKOpponentBrain::RefreshCounter()
{
	ALKBattleGameMode* GM = GameMode.Get();
	if (!GM)
	{
		return;
	}

	PlayerRangedCount = GM->CountCombatUnitsOfAttackType(ELKTeam::Player, ELKAttackType::Ranged);
	PlayerMeleeCount = GM->CountCombatUnitsOfAttackType(ELKTeam::Player, ELKAttackType::Melee);
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 反制评估：玩家 近战 x%d / 远程 x%d"),
		PlayerMeleeCount, PlayerRangedCount);
}

bool ALKOpponentBrain::TryEnterPush()
{
	if (PushCooldownTimer > 0.f || !Silver || !GameMode.IsValid())
	{
		return false;
	}

	// 银币达标 + 己方兵力不劣于玩家 → 一波流
	if (Silver->GetSilver() < GameData->AIPushSilverThreshold)
	{
		return false;
	}
	if (GameMode->CountAliveUnits(ELKTeam::Enemy) < GameMode->CountAliveUnits(ELKTeam::Player))
	{
		return false;
	}

	PushRemainingCards = GameData->AIPushMaxCards;
	PushCooldownTimer = FMath::FRandRange(GameData->AIPushCooldownMin, GameData->AIPushCooldownMax);
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 爆发！银币 %.1f 兵力占优，一波连打 %d 张"),
		Silver->GetSilver(), PushRemainingCards);
	return true;
}

void ALKOpponentBrain::ThinkAndPlay()
{
	// 爆发窗口：一口气连打（最多 AIPushMaxCards 张，一张失败就停）
	if (TryEnterPush())
	{
		while (PushRemainingCards > 0)
		{
			if (!TryPlayOneCard())
			{
				break;
			}
			--PushRemainingCards;
		}
		PushRemainingCards = 0;
		return;
	}

	// 常规节奏：每思考周期一张
	TryPlayOneCard();
}

int32 ALKOpponentBrain::GetCounterBonus(const ULKCardDefinition* Def) const
{
	if (!Def)
	{
		return 0;
	}

	const FName& CardId = Def->CardId;
	int32 Bonus = 0;

	// 玩家远程多 → 盾卫/剑士冲锋贴脸（克制远程）
	if (PlayerRangedCount >= 3
		&& (CardId == TEXT("Unit_Shieldbearer") || CardId == TEXT("Unit_Swordsman")))
	{
		Bonus += 3;
	}

	// 玩家近战多 → 弓箭手/箭塔放风筝（克制近战）
	if (PlayerMeleeCount >= 3)
	{
		if (CardId == TEXT("Unit_Archer") || CardId == TEXT("Building_ArrowTower"))
		{
			Bonus += 3;
		}
	}

	return Bonus;
}

bool ALKOpponentBrain::TryPlayOneCard()
{
	ULKDeckState* DeckState = Deck;
	ULKSilverComponent* SilverComp = Silver;
	ALKBattleGameMode* GM = GameMode.Get();

	if (!DeckState || !SilverComp || !GM)
	{
		return false;
	}

	// 选牌：得分 = 费用权重（便宜优先）+ 反制偏好分
	int32 BestIndex = -1;
	int32 BestScore = TNumericLimits<int32>::Min();

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
		if (SilverComp->GetSilver() < Def->Cost)
		{
			continue; // 买不起的不参与评分
		}

		const int32 Score = (100 - Def->Cost * 10) + GetCounterBonus(Def);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = i;
		}
	}

	if (BestIndex < 0)
	{
		return false;
	}

	const FName CardId = DeckState->GetHandCard(BestIndex);
	FVector Location;
	if (!PickTargetLocation(CardId, Location))
	{
		return false;
	}

	const ELKPlayResult Result = GM->PlayCardForTeam(ELKTeam::Enemy, BestIndex, Location);
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 打出 %s @ %s -> %d%s"),
		*CardId.ToString(), *Location.ToString(), (int32)Result,
		Result == ELKPlayResult::Success ? TEXT("") : TEXT("（失败）"));
	return Result == ELKPlayResult::Success;
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

	// 法术：智能落点（聚集度最高的敌人堆）；找不到就随机瞄准玩家英雄
	if (Def && Def->CardType == ELKCardType::Spell)
	{
		if (GM->FindBestSpellTarget(ELKTeam::Enemy, Def->SpellRadius, OutLocation))
		{
			return true;
		}

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
