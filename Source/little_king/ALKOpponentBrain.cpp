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

void ALKOpponentBrain::InitBrain(ULKGameData* InGameData, ALKBattleGameMode* InGameMode, bool bInCanPlayCards)
{
	GameData = InGameData;
	GameMode = InGameMode;
	bCanPlayCards = bInCanPlayCards;

	if (!GameData || !GameMode.IsValid())
	{
		return;
	}

	Silver->Init(GameData->SilverPerSecond, GameData->SilverCap);
	if (bCanPlayCards && !Deck->InitDeck(GameData->DefaultEnemyDeck, GameData->HandSize, GameData->BattleSeed + 2))
	{
		UE_LOG(LogLKBattle, Warning, TEXT("[Deck] 敌方牌库无效：去重后至少需要 HandSize + 1 种卡，禁止开战"));
		bCanPlayCards = false;
	}
	Deck->CostProvider = [this](FName CardId) { return GameMode.IsValid() ? GameMode->GetCardCost(CardId) : 2; };

	LoadWaves();

	AISettings.bEnableFocus = true;
	AISettings.bEnableCounter = true;
	AISettings.bEnablePush = true;
	AISettings.FocusWarningSeconds = GameData->AIFocusWarningSeconds;
	AISettings.FocusIntervalMin = GameData->AIFocusIntervalMin;
	AISettings.FocusIntervalMax = GameData->AIFocusIntervalMax;
	AISettings.FocusDuration = GameData->AIFocusDuration;
	AISettings.CounterCheckInterval = GameData->AICounterCheckInterval;
	AISettings.PushSilverThreshold = GameData->AIPushSilverThreshold;
	AISettings.PushMaxCards = GameData->AIPushMaxCards;
	AISettings.PushCooldownMin = GameData->AIPushCooldownMin;
	AISettings.PushCooldownMax = GameData->AIPushCooldownMax;
	AISettings.PushReserveMaxSeconds = GameData->AIPushReserveMaxSeconds;

	// 集火节奏：首轮随机落在配置区间内
	FocusTimer = GameMode->GetBattleRandom().FRandRange(AISettings.FocusIntervalMin, AISettings.FocusIntervalMax);
	CounterTimer = AISettings.CounterCheckInterval;
	PushCooldownTimer = GameMode->GetBattleRandom().FRandRange(AISettings.PushCooldownMin, AISettings.PushCooldownMax);

	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 敌方 AI 就绪: 波次 %d, 牌库 %d 张（首轮集火 %.0f 秒后）"),
		Waves.Num(), bCanPlayCards ? Deck->GetDeckSize() + Deck->GetHandSize() : 0, FocusTimer);
}

void ALKOpponentBrain::LoadWaves()
{
	Waves.Reset();
	WaveIndex = 0;

	if (GameData && !GameData->WaveTable.IsNull())
	{
		if (UDataTable* Table = GameData->WaveTable.LoadSynchronous())
		{
			if (Table->GetRowStruct() != FLKWaveRow::StaticStruct()) { UE_LOG(LogLKBattle, Error, TEXT("[Brain] WaveTable 必须使用 LKWaveRow")); return; }
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

void ALKOpponentBrain::SetScriptedWaves(const TArray<FLKWaveEntry>& InWaves)
{
    Waves = InWaves;
    Waves.StableSort([](const FLKWaveEntry& A, const FLKWaveEntry& B) { return A.Time < B.Time; });
	WaveIndex = 0; bCanPlayCards = false;
    PendingFocusTarget.Reset(); FocusWarningTimer = 0.f;
}

bool ALKOpponentBrain::ConfigureEncounter(const FLKEncounterRow& Encounter)
{
	if (!GameData || !GameMode.IsValid() || Encounter.EncounterId.IsNone()) { return false; }
	Waves = Encounter.Waves;
	Waves.StableSort([](const FLKWaveEntry& A, const FLKWaveEntry& B) { return A.Time < B.Time; });
	WaveIndex = 0;
	bCanPlayCards = Encounter.bEnemyUsesCards;
	AISettings = Encounter.AI;
	PendingFocusTarget.Reset();
	FocusWarningTimer = 0.f;
	bReserving = false;
	ReserveRemaining = 0.f;
	PushRemainingCards = 0;
	ThinkTimer = 0.f;
	Silver->Init(Encounter.EnemySilverPerSecond, Encounter.EnemySilverCap);
	Silver->AddSilver(Encounter.EnemyStartingSilver);
	if (bCanPlayCards && !Deck->InitDeck(Encounter.EnemyCards, GameData->HandSize, GameData->BattleSeed + 2))
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Encounter] %s 敌方牌组无效"), *Encounter.EncounterId.ToString());
		bCanPlayCards = false;
		return false;
	}
	FocusTimer = GameMode->GetBattleRandom().FRandRange(AISettings.FocusIntervalMin, AISettings.FocusIntervalMax);
	CounterTimer = AISettings.CounterCheckInterval;
	PushCooldownTimer = GameMode->GetBattleRandom().FRandRange(AISettings.PushCooldownMin, AISettings.PushCooldownMax);
	UE_LOG(LogLKBattle, Log, TEXT("[Encounter] AI %s：波次%d 出牌=%d 集火=%d 反制=%d 爆发=%d 奖励档=%d"),
		*Encounter.EncounterId.ToString(), Waves.Num(), int32(bCanPlayCards), int32(AISettings.bEnableFocus),
		int32(AISettings.bEnableCounter), int32(AISettings.bEnablePush), Encounter.RewardTier);
	return true;
}

bool ALKOpponentBrain::IsFocusEnabled() const
{
	return AISettings.bEnableFocus;
}

void ALKOpponentBrain::TickBrain(float DeltaTime, float BattleElapsed)
{
	if (!GameData || !GameMode.IsValid())
	{
		return;
	}

	if (bCanPlayCards) { Silver->TickSilver(DeltaTime); }

	ProcessWaves(BattleElapsed);
	// 波次、集火和出牌是独立开关；无牌遭遇仍可拥有精英/首领集火节奏。
    if (bReserving) { ReserveRemaining -= DeltaTime; }
    if (FocusWarningTimer > 0.f)
    {
        FocusWarningTimer -= DeltaTime;
        if (FocusWarningTimer <= 0.f)
        {
            if (PendingFocusTarget.IsValid() && PendingFocusTarget->IsTargetable())
            {
				GameMode->ForcedTargetAllUnits(ELKTeam::Enemy, PendingFocusTarget.Get(), AISettings.FocusDuration);
            }
            PendingFocusTarget = nullptr;
        }
    }

	// ---------- S4 战术节奏 ----------
	if (AISettings.bEnableFocus) { FocusTimer -= DeltaTime; }
	if (AISettings.bEnableFocus && FocusTimer <= 0.f)
	{
		DoFocus();
		FocusTimer = GameMode->GetBattleRandom().FRandRange(AISettings.FocusIntervalMin, AISettings.FocusIntervalMax);
	}

	if (bCanPlayCards && AISettings.bEnableCounter) { CounterTimer -= DeltaTime; }
	if (bCanPlayCards && AISettings.bEnableCounter && CounterTimer <= 0.f)
	{
		RefreshCounter();
		CounterTimer = AISettings.CounterCheckInterval;
	}

	PushCooldownTimer = FMath::Max(0.f, PushCooldownTimer - DeltaTime);

	// 思考出牌
	if (bCanPlayCards) { ThinkTimer -= DeltaTime; }
	if (bCanPlayCards && ThinkTimer <= 0.f)
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
				GameMode->GetBattleRandom().FRandRange(-0.8f, 0.8f) * HalfW,
				GameMode->GetBattleRandom().FRandRange(0.15f, 0.45f) * HalfH,
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

	PendingFocusTarget = Weakest;
	FocusWarningTimer = FMath::Max(0.01f, AISettings.FocusWarningSeconds);
    Weakest->SetFocusWarning(FocusWarningTimer);
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 集火预警：即将转火玩家英雄 %s（集火持续 %.0f 秒）"),
		*Weakest->GetUnitId().ToString(), AISettings.FocusDuration);
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
	if (!AISettings.bEnablePush || Silver->GetSilver() < FMath::Min(AISettings.PushSilverThreshold, Silver->GetCap()))
	{
		return false;
	}
	if (GameMode->CountAliveUnits(ELKTeam::Enemy) < GameMode->CountAliveUnits(ELKTeam::Player))
	{
		return false;
	}

	PushRemainingCards = AISettings.PushMaxCards;
	PushCooldownTimer = GameMode->GetBattleRandom().FRandRange(AISettings.PushCooldownMin, AISettings.PushCooldownMax);
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 爆发！银币 %.1f 兵力占优，一波连打 %d 张"),
		Silver->GetSilver(), PushRemainingCards);
	return true;
}

void ALKOpponentBrain::ThinkAndPlay()
{
    if (PushCooldownTimer <= 0.f && !bReserving)
    {
        bReserving = true;
		ReserveRemaining = AISettings.PushReserveMaxSeconds;
    }
    if (bReserving)
    {
        if (TryEnterPush())
        {
            bReserving = false;
            while (PushRemainingCards-- > 0) { if (!TryPlayOneCard()) { break; } }
            PushRemainingCards = 0;
            return;
        }
		if (ReserveRemaining > 0.f && Silver->GetSilver() < FMath::Min(AISettings.PushSilverThreshold, Silver->GetCap())) { return; }
        bReserving = false;
		PushCooldownTimer = AISettings.PushCooldownMin;
    }
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
		Bonus += 35;
	}

	// 玩家近战多 → 弓箭手/箭塔放风筝（克制近战）
	if (PlayerMeleeCount >= 3)
	{
		if (CardId == TEXT("Unit_Archer") || CardId == TEXT("Building_ArrowTower"))
		{
			Bonus += 35;
		}
	}

	return Bonus;
}

bool ALKOpponentBrain::TryPlayOneCard()
{
    ALKBattleGameMode* GM = GameMode.Get();
    if (!GM || !Deck || !Silver || GM->GetPhase() != ELKGamePhase::Battle) { return false; }
    TArray<int32> Candidates;
    for (int32 i = 0; i < Deck->GetHandSize(); ++i)
    {
        const ULKCardDefinition* Card = GM->FindCard(Deck->GetHandCard(i));
        if (Card && Card->Cost <= Silver->GetSilver()) { Candidates.Add(i); }
    }
    Candidates.StableSort([this, GM](int32 A, int32 B)
    {
        const ULKCardDefinition* Left = GM->FindCard(Deck->GetHandCard(A));
        const ULKCardDefinition* Right = GM->FindCard(Deck->GetHandCard(B));
        return (100 - Left->Cost * 10 + GetCounterBonus(Left)) > (100 - Right->Cost * 10 + GetCounterBonus(Right));
    });
    for (int32 Index : Candidates)
    {
        for (int32 Attempt = 0; Attempt < 8; ++Attempt)
        {
            FVector Location;
            if (!PickTargetLocation(Deck->GetHandCard(Index), Location)) { break; }
            if (GM->ValidateCardPlay(ELKTeam::Enemy, Index, Location) != ELKPlayResult::Success) { continue; }
            if (GM->PlayCardForTeam(ELKTeam::Enemy, Index, Location) == ELKPlayResult::Success) { return true; }
        }
    }
    return false;
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
        return GM->FindBestSpellTarget(ELKTeam::Enemy, Def->SpellRadius, OutLocation, Def->SpellEffect);
    }

	// 建筑：后场（Y 深处）；角色：前中场 —— 敌方右半侧（Y>0）
	if (Def && Def->CardType == ELKCardType::Building)
	{
		OutLocation = FVector(GameMode->GetBattleRandom().FRandRange(-0.7f, 0.7f) * HalfW, GameMode->GetBattleRandom().FRandRange(0.65f, 0.9f) * HalfH, 0.f);
	}
	else
	{
		OutLocation = FVector(GameMode->GetBattleRandom().FRandRange(-0.8f, 0.8f) * HalfW, GameMode->GetBattleRandom().FRandRange(0.1f, 0.35f) * HalfH, 0.f);
	}
	return true;
}
