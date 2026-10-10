#include "ALKOpponentBrain.h"
#include "EngineUtils.h"

#include "Engine/DataTable.h"

#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "LKDataTypes.h"
#include "LKLog.h"
#include "ULKCardDefinition.h"
#include "ULKDeckState.h"
#include "ULKGameData.h"
#include "ULKSilverComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
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
	ResetDecisions();

	Silver->Init(GameData->SilverPerSecond, GameData->SilverCap);
	if (bCanPlayCards && !Deck->InitEnemyDeck(GameData->DefaultEnemyDeck, GameData->HandSize, GameData->BattleSeed + 2))
	{
		UE_LOG(LogLKBattle, Warning, TEXT("[Deck] 敌方牌库无效：至少需要一张不同的有效卡牌，禁止开战"));
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
	FocusTimer = DecisionRandom.FRandRange(AISettings.FocusIntervalMin, AISettings.FocusIntervalMax);
	CounterTimer = AISettings.CounterCheckInterval;
	PushCooldownTimer = DecisionRandom.FRandRange(AISettings.PushCooldownMin, AISettings.PushCooldownMax);

	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 敌方 AI 就绪: 波次 %d, 牌库 %d 张（首轮集火 %.0f 秒后）"),
		Waves.Num(), bCanPlayCards ? Deck->GetDeckSize() + Deck->GetHandSize() : 0, FocusTimer);
}

void ALKOpponentBrain::ResetDecisions()
{
    DecisionRandom.Initialize(int32(uint32(GameData ? GameData->BattleSeed : 0) ^ 0x41494231u));
    Observation = FLKAIObservation(); ObservationTimer = 0.f; CurrentBattleTime = 0.f;
    Intent = ELKAIIntent::Hold; IntentTarget = NAME_None; IntentRemaining = 0.f;
    PlayerRangedCount = PlayerMeleeCount = 0;
    DecisionHistory.Reset(); DecisionCount = 0; CandidateCursors.Reset();
}

void ALKOpponentBrain::RefreshObservation()
{
    Observation = LKAIObservation::Capture(GameMode.Get(), ELKTeam::Enemy, CurrentBattleTime, Observation.Revision + 1);
    ObservationTimer = .25f;
}

void ALKOpponentBrain::UpdateIntent()
{
    // Two-second commitment prevents target oscillation. Missing/hidden/dead targets
    // cancel immediately; this minimum never overrides discovery or attack eligibility.
    if (Observation.FindEnemy(IntentTarget) && IntentRemaining > 0.f) { return; }
    IntentTarget = LKAIDecision::ChooseIntentTarget(Observation, Intent);
    IntentRemaining = IntentTarget.IsNone() ? 0.f : 2.f;
}

void ALKOpponentBrain::RecordDecision(FLKAIDecisionTrace Trace)
{
    Trace.ObservationRevision = Observation.Revision; Trace.BattleTime = CurrentBattleTime;
    Trace.Intent = Intent; Trace.IntentTarget = IntentTarget;
    DecisionHistory.Add(MoveTemp(Trace));
    if (DecisionHistory.Num() > LKAIDecision::MaxTraceHistory) { DecisionHistory.RemoveAt(0); }
    ++DecisionCount;
    const auto& Last = DecisionHistory.Last();
    UE_LOG(LogLKBattle, Verbose, TEXT("[AI] decision=%u observation=%u intent=%s card=%s score=%.2f checks=%d result=%s"),
        DecisionCount, Last.ObservationRevision, LKAIDecision::IntentName(Intent), *Last.CardId.ToString(),
        Last.Utility.Total(), Last.ValidationChecks, *Last.Result);
    // Overwrite one bounded artifact every 32 decisions and at battle end; never per frame.
    if (DecisionCount % 32 == 0) { ExportDecisionTelemetry(); }
}

bool ALKOpponentBrain::ExportDecisionTelemetry() const
{
    if (DecisionHistory.IsEmpty()) { return true; }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("format"), TEXT("little-king.opponent-decisions.v1"));
    Root->SetNumberField(TEXT("seed"), GameData ? GameData->BattleSeed : 0);
    Root->SetNumberField(TEXT("totalDecisions"), DecisionCount);
    Root->SetNumberField(TEXT("retainedLimit"), LKAIDecision::MaxTraceHistory);
    auto ScoreJson = [](const FLKAIUtility& S)
    {
        TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("benefit"), S.Benefit); J->SetNumberField(TEXT("synergy"), S.Synergy);
        J->SetNumberField(TEXT("cost"), S.Cost); J->SetNumberField(TEXT("risk"), S.Risk); J->SetNumberField(TEXT("total"), S.Total());
        return J;
    };
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const auto& T : DecisionHistory)
    {
        TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("observation"), T.ObservationRevision); J->SetNumberField(TEXT("time"), T.BattleTime);
        J->SetStringField(TEXT("intent"), LKAIDecision::IntentName(T.Intent)); J->SetStringField(TEXT("target"), T.IntentTarget.ToString());
        J->SetStringField(TEXT("card"), T.CardId.ToString()); J->SetStringField(TEXT("result"), T.Result);
        J->SetNumberField(TEXT("x"), T.Location.X); J->SetNumberField(TEXT("y"), T.Location.Y);
        J->SetObjectField(TEXT("utility"), ScoreJson(T.Utility)); J->SetNumberField(TEXT("candidates"), T.Candidates);
        J->SetNumberField(TEXT("validationChecks"), T.ValidationChecks);
        J->SetNumberField(TEXT("silverBefore"), T.SilverBefore); J->SetNumberField(TEXT("silverAfter"), T.SilverAfter);
        TArray<TSharedPtr<FJsonValue>> Top;
        for (const auto& C : T.Shortlist)
        {
            TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
            A->SetStringField(TEXT("card"), C.CardId.ToString()); A->SetNumberField(TEXT("x"), C.Location.X);
            A->SetNumberField(TEXT("y"), C.Location.Y); A->SetObjectField(TEXT("utility"), ScoreJson(C.Utility));
            Top.Add(MakeShared<FJsonValueObject>(A));
        }
        J->SetArrayField(TEXT("top"), Top); Rows.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("decisions"), Rows);
    FString Json; const auto Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Telemetry"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    // Automated worlds must not replace the player's last diagnostic history.
    const TCHAR* Filename = GIsAutomationTesting ? TEXT("OpponentAI-Automation.json") : TEXT("OpponentAI.json");
    return FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, Filename), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void ALKOpponentBrain::LoadWaves()
{
	Waves.Reset();
	ResetWaveRuntime();
	PendingReinforcements.Reset();
	DroppedReinforcements = 0;

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
					Entry.Trigger = Row->Trigger;
					Entry.TriggerTargetTag = Row->TriggerTargetTag;
					Entry.HealthThreshold = Row->HealthThreshold;
					Entry.bUseSpawnLocation = Row->bUseSpawnLocation;
					Entry.SpawnLocation = Row->SpawnLocation;
					Entry.SpawnRadius = Row->SpawnRadius;
					Waves.Add(Entry);
				}
			}
			Waves.Sort([](const FLKWaveEntry& A, const FLKWaveEntry& B) { return A.Time < B.Time; });
			ResetWaveRuntime();
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
	ResetWaveRuntime();
}

void ALKOpponentBrain::ResetBattleState()
{
	ExportDecisionTelemetry();
	ResetDecisions();
	Waves.Reset();
	ResetWaveRuntime();
	PendingReinforcements.Reset();
	PendingFocusTarget.Reset();
	FocusWarningTimer = 0.f;
	bReserving = false;
	ReserveRemaining = 0.f;
	PushRemainingCards = 0;
	SpellCheckTimer = 0.f;
	SpellCooldownRemaining = 0.f;
}

void ALKOpponentBrain::SetScriptedWaves(const TArray<FLKWaveEntry>& InWaves)
{
    Waves = InWaves;
    Waves.StableSort([](const FLKWaveEntry& A, const FLKWaveEntry& B) { return A.Time < B.Time; });
	ResetWaveRuntime(); bCanPlayCards = false;
	PendingReinforcements.Reset();
	DroppedReinforcements = 0;
	// 脚本波次夹具不启用战术法术：避免给旧的固定节奏测试引入额外敌人。
	SpellSettings.bEnabled = false;
	SpellCheckTimer = 0.f;
	SpellCooldownRemaining = 0.f;
    PendingFocusTarget.Reset(); FocusWarningTimer = 0.f;
}

bool ALKOpponentBrain::ConfigureEncounter(const FLKEncounterRow& Encounter)
{
	if (!GameData || !GameMode.IsValid() || Encounter.EncounterId.IsNone()) { return false; }
	ResetDecisions();
	Waves = Encounter.Waves;
	Waves.StableSort([](const FLKWaveEntry& A, const FLKWaveEntry& B) { return A.Time < B.Time; });
	ResetWaveRuntime();
	PendingReinforcements.Reset();
	DroppedReinforcements = 0;
	bCanPlayCards = Encounter.bEnemyUsesCards;
	AISettings = Encounter.AI;
	PendingFocusTarget.Reset();
	FocusWarningTimer = 0.f;
	bReserving = false;
	ReserveRemaining = 0.f;
	PushRemainingCards = 0;
	ThinkTimer = 0.f;
	// 战术法术通道：独立于波次与牌组，共用敌方银币组件。
	SpellSettings = Encounter.EnemySpell;
	// The same spell in an authored deck uses the normal paid-card transaction only.
	if (bCanPlayCards && Encounter.EnemyCards.Contains(SpellSettings.SpellId)) { SpellSettings.bEnabled = false; }
	SpellFirstCastAt = SpellSettings.FirstCastTime;
	SpellCheckTimer = 0.f;
	SpellCooldownRemaining = 0.f;
	TacticalSpellCasts = 0;
	Silver->Init(Encounter.EnemySilverPerSecond, Encounter.EnemySilverCap);
	Silver->AddSilver(Encounter.EnemyStartingSilver);
	if (bCanPlayCards && !Deck->InitEnemyDeck(Encounter.EnemyCards, GameData->HandSize, GameData->BattleSeed + 2))
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Encounter] %s 敌方牌组无效"), *Encounter.EncounterId.ToString());
		bCanPlayCards = false;
		return false;
	}
	FocusTimer = DecisionRandom.FRandRange(AISettings.FocusIntervalMin, AISettings.FocusIntervalMax);
	CounterTimer = AISettings.CounterCheckInterval;
	PushCooldownTimer = DecisionRandom.FRandRange(AISettings.PushCooldownMin, AISettings.PushCooldownMax);
	UE_LOG(LogLKBattle, Log, TEXT("[Encounter] AI %s：波次%d 出牌=%d 集火=%d 反制=%d 爆发=%d 奖励档=%d 战术法术=%d(%s 冷却%.0f 首放%.0f 停%.0f)"),
		*Encounter.EncounterId.ToString(), Waves.Num(), int32(bCanPlayCards), int32(AISettings.bEnableFocus),
		int32(AISettings.bEnableCounter), int32(AISettings.bEnablePush), Encounter.RewardTier,
		int32(SpellSettings.bEnabled), *SpellSettings.SpellId.ToString(), SpellSettings.Cooldown,
		SpellSettings.FirstCastTime, SpellSettings.StopTime);
	return true;
}

bool ALKOpponentBrain::IsFocusEnabled() const
{
	return AISettings.bEnableFocus;
}

void ALKOpponentBrain::TickBrain(float DeltaTime, float BattleElapsed)
{
	if (!GameData || !GameMode.IsValid() || GameMode->GetPhase() != ELKGamePhase::Battle
		|| !FMath::IsFinite(DeltaTime) || DeltaTime < 0.f || !FMath::IsFinite(BattleElapsed))
	{
		return;
	}
	CurrentBattleTime = BattleElapsed;
	ObservationTimer -= DeltaTime;
	IntentRemaining = FMath::Max(0.f, IntentRemaining - DeltaTime);
	if (ObservationTimer <= 0.f) { RefreshObservation(); UpdateIntent(); }

	if (bCanPlayCards) { Silver->TickSilver(DeltaTime); }
	// 战术法术经济独立于出牌：即使 bEnemyUsesCards=false 也照常产银币并施放。
	else if (SpellSettings.bEnabled) { Silver->TickSilver(DeltaTime); }

	ProcessWaves(BattleElapsed);
	TickTacticalSpell(DeltaTime, BattleElapsed);
	// 波次、集火和出牌是独立开关；无牌遭遇仍可拥有精英/首领集火节奏。
    if (bReserving) { ReserveRemaining -= DeltaTime; }
    if (FocusWarningTimer > 0.f)
    {
        FocusWarningTimer -= DeltaTime;
        if (FocusWarningTimer <= 0.f)
        {
            ResolveFocus();
        }
    }

	// ---------- S4 战术节奏 ----------
	if (AISettings.bEnableFocus) { FocusTimer -= DeltaTime; }
	if (AISettings.bEnableFocus && FocusTimer <= 0.f)
	{
		DoFocus();
		FocusTimer = DecisionRandom.FRandRange(AISettings.FocusIntervalMin, AISettings.FocusIntervalMax);
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
		// At most one paid-card decision per update, even after a long frame.
		ThinkTimer = FMath::Max(.25f, ThinkInterval);
		ThinkAndPlay();
	}
}

void ALKOpponentBrain::ResetWaveRuntime()
{
    WaveRuntime.SetNum(Waves.Num());
    for (FWaveRuntime& Runtime : WaveRuntime) { Runtime = FWaveRuntime(); }
}

void ALKOpponentBrain::NotifyUnitHealthEvent(ALKUnitBase* Unit, float NewHealth, float MaxHealth, float BattleElapsed)
{
    if (!Unit || !Unit->IsHero() || !FMath::IsFinite(NewHealth) || !FMath::IsFinite(MaxHealth)
        || MaxHealth <= 0.f || !FMath::IsFinite(BattleElapsed) || BattleElapsed < 0.f) { return; }
    for (int32 Index = 0; Index < Waves.Num(); ++Index)
    {
        const FLKWaveEntry& Entry = Waves[Index];
        if (Entry.Trigger == ELKWaveTrigger::HeroHealthBelow && WaveRuntime.IsValidIndex(Index)
            && WaveRuntime[Index].TriggeredAt < 0.f && Unit->ActorHasTag(Entry.TriggerTargetTag)
            && NewHealth / MaxHealth < Entry.HealthThreshold)
        { WaveRuntime[Index].TriggeredAt = BattleElapsed; }
    }
}

void ALKOpponentBrain::NotifyUnitFirstDeath(ALKUnitBase* Unit, float BattleElapsed)
{
    if (!Unit || !Unit->IsHero() || !FMath::IsFinite(BattleElapsed) || BattleElapsed < 0.f) { return; }
    for (int32 Index = 0; Index < Waves.Num(); ++Index)
    {
        const FLKWaveEntry& Entry = Waves[Index];
        if (Entry.Trigger == ELKWaveTrigger::HeroFirstDeath && WaveRuntime.IsValidIndex(Index)
            && WaveRuntime[Index].TriggeredAt < 0.f && Unit->ActorHasTag(Entry.TriggerTargetTag))
        { WaveRuntime[Index].TriggeredAt = BattleElapsed; }
    }
}

void ALKOpponentBrain::ResolvePendingScenarioEvents(float BattleElapsed)
{
    ProcessWaves(BattleElapsed);
}

bool ALKOpponentBrain::HasPendingScenarioReinforcements() const
{
    for (const FPendingReinforcement& Pending : PendingReinforcements)
    { if (Pending.bFromCondition) { return true; } }
    for (const FWaveRuntime& Runtime : WaveRuntime)
    {
        if (Runtime.TriggeredAt >= 0.f && !Runtime.bQueued) { return true; }
    }
    return false;
}

bool ALKOpponentBrain::HasOutstandingScenarioWaves() const
{
    if (!PendingReinforcements.IsEmpty() || HasPendingScenarioReinforcements()) { return true; }
    for (int32 Index = 0; Index < Waves.Num(); ++Index)
    {
        if (Waves[Index].Trigger == ELKWaveTrigger::Time && WaveRuntime.IsValidIndex(Index)
            && !WaveRuntime[Index].bQueued) { return true; }
    }
    return false;
}

void ALKOpponentBrain::ProcessWaves(float BattleElapsed)
{
    if (!GameMode.IsValid() || !GameData || bResolvingWaves || !FMath::IsFinite(BattleElapsed) || BattleElapsed < 0.f) { return; }
    TGuardValue<bool> Guard(bResolvingWaves, true);
    for (int32 Index = 0; Index < Waves.Num(); ++Index)
    {
        if (!WaveRuntime.IsValidIndex(Index)) { continue; }
        const FLKWaveEntry& Entry = Waves[Index];
        FWaveRuntime& Runtime = WaveRuntime[Index];
        if (Runtime.bQueued) { continue; }
        const float Due = Entry.Trigger == ELKWaveTrigger::Time ? Entry.Time
            : (Runtime.TriggeredAt >= 0.f ? Runtime.TriggeredAt + Entry.Time : -1.f);
        if (Due < 0.f || Due > BattleElapsed) { continue; }
        Runtime.bQueued = true;
        QueueReinforcement(Entry, Due);
        UE_LOG(LogLKBattle, Log, TEXT("[Brain] 波次触发 t=%.1f: %s x%d (trigger=%d)"),
            Due, *Entry.UnitId.ToString(), Entry.Count, int32(Entry.Trigger));
    }

    // Bounded retry keeps a full battlefield from accumulating a later unbounded burst.
    for (int32 Index = PendingReinforcements.Num() - 1; Index >= 0; --Index)
    {
        FPendingReinforcement& Pending = PendingReinforcements[Index];
        if (BattleElapsed > Pending.Deadline)
        {
            DroppedReinforcements += Pending.Remaining;
            UE_LOG(LogLKBattle, Log, TEXT("[Brain] 增援 %s x%d 超过延后窗口丢弃（累计 %d）"),
                *Pending.UnitId.ToString(), Pending.Remaining, DroppedReinforcements);
            PendingReinforcements.RemoveAt(Index);
            continue;
        }
        while (Pending.Remaining > 0)
        {
            if (GameData->MaxUnitsPerTeam > 0 && GameMode->CountAliveUnits(ELKTeam::Enemy) >= GameData->MaxUnitsPerTeam) { break; }
            const FVector Preferred = Pending.bUseSpawnLocation ? Pending.SpawnLocation : FVector(
                GameMode->GetBattleRandom().FRandRange(-0.8f, 0.8f) * GameData->FieldHalfWidth,
                GameMode->GetBattleRandom().FRandRange(0.15f, 0.45f) * GameData->FieldHalfHeight, 0.f);
            const float Radius = Pending.bUseSpawnLocation ? Pending.SpawnRadius : GameData->UnitBodyRadius * 4.f;
            FVector Location = Preferred;
            const bool bFound = Radius <= 0.f
                ? GameMode->IsSpawnPointFree(Preferred, GameData->UnitBodyRadius, Pending.bUseSpawnLocation)
                : GameMode->FindFreeSpawnLocation(Preferred, Radius, Location, GameMode->GetBattleRandom(), 12, Pending.bUseSpawnLocation);
            if (!bFound || !GameMode->SpawnUnitForTeam(Pending.UnitId, ELKTeam::Enemy, Location,
                ELKUnitClass::Soldier, NAME_None, 0.f, nullptr, Pending.bUseSpawnLocation)) { break; }
            --Pending.Remaining;
        }
        if (Pending.Remaining <= 0) { PendingReinforcements.RemoveAt(Index); }
    }
}

void ALKOpponentBrain::QueueReinforcement(const FLKWaveEntry& Entry, float DueTime)
{
    if (Entry.UnitId.IsNone() || Entry.Count <= 0) { return; }
    FPendingReinforcement Pending;
    Pending.UnitId = Entry.UnitId;
    Pending.Remaining = Entry.Count;
    Pending.Deadline = DueTime + MaxReinforcementDelay;
    Pending.bFromCondition = Entry.Trigger != ELKWaveTrigger::Time;
    Pending.bUseSpawnLocation = Entry.bUseSpawnLocation;
    Pending.SpawnLocation = Entry.SpawnLocation;
    Pending.SpawnRadius = Entry.SpawnRadius;
    PendingReinforcements.Add(Pending);
}

void ALKOpponentBrain::TickTacticalSpell(float DeltaTime, float BattleElapsed)
{
	if (!SpellSettings.bEnabled || !GameMode.IsValid()) { return; }
	SpellCooldownRemaining = FMath::Max(0.f, SpellCooldownRemaining - DeltaTime);
	SpellCheckTimer -= DeltaTime;
	if (SpellCheckTimer > 0.f) { return; }
	SpellCheckTimer = FMath::Max(0.1f, SpellSettings.CheckInterval);
	TryCastTacticalSpell(BattleElapsed);
}

bool ALKOpponentBrain::TryCastTacticalSpell(float BattleElapsed)
{
	ALKBattleGameMode* GM = GameMode.Get();
	if (!GM || GM->GetPhase() != ELKGamePhase::Battle) { return false; }
	// 第一次最早在开战 FirstCastTime 之后，且余额够 5；180 秒后停止新增法阵以保留残局收束期。
	if (BattleElapsed < SpellFirstCastAt || BattleElapsed > SpellSettings.StopTime) { return false; }
	if (SpellCooldownRemaining > 0.f) { return false; }
	if (SpellSettings.MaxActive <= 0 || GM->GetActiveTacticalCircleCount() >= SpellSettings.MaxActive) { return false; }
	const ULKCardDefinition* Card = GM->FindCard(SpellSettings.SpellId);
	if (!Card || Card->CardType != ELKCardType::Spell || Card->SpellEffect != ELKSpellEffect::SummonZone) { return false; }
	if (!Silver || Silver->GetSilver() < Card->Cost) { return false; } // 余额不足：不扣费、不空放。
	RefreshObservation(); UpdateIntent();

	FVector Location = FVector::ZeroVector;
	if (!FindTacticalSpellLocation(Card->Circle.Radius, SpellSettings.PreferredTargets, Location)) { return false; }
	// 扣费与落点在 GameMode 事务里再次校验；失败不扣费、不进冷却。
	const float Before = Silver->GetSilver();
	if (!GM->CastEnemyTacticalSpell(SpellSettings.SpellId, Location)) { return false; }
	SpellCooldownRemaining = FMath::Max(0.f, SpellSettings.Cooldown);
	++TacticalSpellCasts;
	FLKAIDecisionTrace Trace; Trace.CardId = SpellSettings.SpellId; Trace.Location = Location;
	Trace.SilverBefore = Before; Trace.SilverAfter = Silver->GetSilver(); Trace.Result = TEXT("TacticalSpellSucceeded");
	RecordDecision(MoveTemp(Trace));
	UE_LOG(LogLKBattle, Log, TEXT("[Brain] 战术法术 %s 于 t=%.1f 释放（第 %d 次，银币 %.1f，冷却 %.0f）"),
		*SpellSettings.SpellId.ToString(), BattleElapsed, TacticalSpellCasts, Silver->GetSilver(), SpellSettings.Cooldown);
	return true;
}

bool ALKOpponentBrain::FindTacticalSpellLocation(float Radius, int32 PreferredTargets, FVector& OutLocation) const
{
    if (!FMath::IsFinite(Radius) || Radius <= 0.f) { return false; }
    int32 BestCount = 0; FVector Best = FVector::ZeroVector;
    for (const auto& Candidate : Observation.Enemies)
    {
        int32 Count = 0;
        for (const auto& Unit : Observation.Enemies)
        { if (FVector::DistSquared2D(Candidate.Position, Unit.Position) <= FMath::Square(Radius)) { ++Count; } }
        if (Count > BestCount) { BestCount = Count; Best = Candidate.Position; }
    }
    if (BestCount >= FMath::Max(1, PreferredTargets)) { OutLocation = Best; return true; }
    for (const auto& Unit : Observation.Enemies)
    { if (Unit.bHero) { OutLocation = Unit.Position; return true; } }
    return false;
}

void ALKOpponentBrain::DoFocus()
{
    RefreshObservation(); UpdateIntent();
    const FLKAIUnitObservation* Target = Observation.FindEnemy(IntentTarget);
    if (!Target || !Target->Actor.IsValid()) { return; }
    PendingFocusTarget = Target->Actor;
    FocusWarningTimer = FMath::Max(.01f, AISettings.FocusWarningSeconds);
    PendingFocusTarget->SetFocusWarning(FocusWarningTimer);
}

void ALKOpponentBrain::ResolveFocus()
{
    ALKUnitBase* Target = PendingFocusTarget.Get();
    PendingFocusTarget.Reset(); FocusWarningTimer = 0.f;
    // Cached coordinates are never upgraded to hidden real coordinates at execution.
    RefreshObservation(); UpdateIntent();
    FLKAIDecisionTrace Trace; Trace.SilverBefore = Trace.SilverAfter = Silver ? Silver->GetSilver() : 0.f;
    const auto* Seen = Target ? Observation.FindEnemy(Target->GetFName()) : nullptr;
    if (!Seen || !LKAIObservation::CanTeamDiscover(GameMode.Get(), ELKTeam::Enemy, Target))
    {
        if (IsValid(Target)) { Target->SetFocusWarning(0.f); }
        Trace.Result = TEXT("FocusCancelled:UndiscoveredOrDead"); RecordDecision(MoveTemp(Trace)); return;
    }
    struct FMember { ALKUnitBase* Unit; float Distance; };
    TArray<FMember> Members;
    for (const auto& Ally : Observation.Allies)
    {
        ALKUnitBase* Unit = Ally.Actor.Get();
        if (!Unit || Ally.bHealer || Ally.bBuilding || Ally.bControlled || Ally.bBusyWithService
            || !Unit->CanPursueTarget(Target)) { continue; }
        Members.Add({ Unit, float(FVector::DistSquared2D(Ally.Position, Seen->Position)) });
    }
    Members.StableSort([](const FMember& A, const FMember& B) { return A.Distance < B.Distance; });
    // Only this enemy intent is limited; player focus and unit taunt/backstab priorities
    // are unchanged. Three attackers avoid ordering the whole army into one bottleneck.
    const int32 Count = FMath::Min(3, Members.Num());
    for (int32 I=0; I<Count; ++I) { Members[I].Unit->SetForcedTarget(Target, FMath::Min(4.f, AISettings.FocusDuration)); }
    Target->SetFocusWarning(0.f);
    Trace.Location = Seen->Position; Trace.Result = FString::Printf(TEXT("FocusSucceeded:Members=%d"), Count);
    RecordDecision(MoveTemp(Trace));
}

void ALKOpponentBrain::RefreshCounter()
{
    RefreshObservation();
    PlayerRangedCount = PlayerMeleeCount = 0;
    for (const auto& Unit : Observation.Enemies)
    {
        if (Unit.bBuilding) { continue; }
        if (Unit.AttackType == ELKAttackType::Ranged) { ++PlayerRangedCount; }
        else { ++PlayerMeleeCount; }
    }
}

bool ALKOpponentBrain::TryEnterPush()
{
    if (PushCooldownTimer > 0.f || !Silver || !AISettings.bEnablePush
        || Silver->GetSilver() < FMath::Min(AISettings.PushSilverThreshold, Silver->GetCap())) { return false; }
    RefreshObservation();
    if (Observation.Enemies.IsEmpty()) { return false; }
    auto Power = [](const TArray<FLKAIUnitObservation>& Units)
    {
        float Total = 0.f;
        for (const auto& U : Units) { Total += FMath::Sqrt(FMath::Max(0.f,U.Health)) * FMath::Sqrt(FMath::Max(1.f,U.DPS())); }
        return Total;
    };
    if (Power(Observation.Allies) < Power(Observation.Enemies)) { return false; }
    PushRemainingCards = FMath::Clamp(AISettings.PushMaxCards, 1, 3);
    PushCooldownTimer = DecisionRandom.FRandRange(AISettings.PushCooldownMin, AISettings.PushCooldownMax);
    return true;
}

void ALKOpponentBrain::ThinkAndPlay()
{
    if (AISettings.bEnablePush && PushCooldownTimer <= 0.f && !bReserving && PushRemainingCards <= 0)
    { bReserving = true; ReserveRemaining = FMath::Min(3.f, AISettings.PushReserveMaxSeconds); }
    if (bReserving)
    {
        if (TryEnterPush()) { bReserving = false; }
        else if (ReserveRemaining > 0.f && Intent != ELKAIIntent::Protect
            && Silver->GetSilver() < FMath::Min(AISettings.PushSilverThreshold, Silver->GetCap()))
        {
            FLKAIDecisionTrace T; T.Result = TEXT("Wait:Reserve");
            T.SilverBefore = T.SilverAfter = Silver->GetSilver(); RecordDecision(MoveTemp(T)); return;
        }
        else { bReserving = false; PushCooldownTimer = AISettings.PushCooldownMin; }
    }
    const bool bPlayed = TryPlayOneCard();
    // Bursts are paced across decisions; no multiple expensive spawn transactions per frame.
    if (PushRemainingCards > 0)
    {
        if (!bPlayed) { PushRemainingCards = 0; }
        else { --PushRemainingCards; ThinkTimer = .25f; }
    }
}

int32 ALKOpponentBrain::GetCounterBonus(const ULKCardDefinition* Def) const
{
    if (!AISettings.bEnableCounter || !Def || !GameMode.IsValid() || Def->CardType == ELKCardType::Spell) { return 0; }
    const auto* Row = GameMode->GetUnitRow(Def->CardType == ELKCardType::Building ? Def->BuildingUnitId : Def->SpawnUnitId);
    if (!Row || Row->bBasicAttackHeals) { return 0; }
    if (PlayerRangedCount >= 3 && Row->AttackType == ELKAttackType::Melee && Row->MoveSpeed > 0.f) { return 1; }
    if (PlayerMeleeCount >= 3 && Row->AttackType == ELKAttackType::Ranged) { return 1; }
    return 0;
}

bool ALKOpponentBrain::TryPlayOneCard()
{
    ALKBattleGameMode* GM = GameMode.Get();
    if (!GM || !Deck || !Silver || GM->GetPhase() != ELKGamePhase::Battle) { return false; }
    RefreshObservation(); UpdateIntent();
    // Counter composition is from this same current snapshot, never stale hidden units.
    PlayerRangedCount = PlayerMeleeCount = 0;
    for (const auto& U : Observation.Enemies)
    { if (!U.bBuilding) { U.AttackType == ELKAttackType::Ranged ? ++PlayerRangedCount : ++PlayerMeleeCount; } }
    FLKAIDecisionTrace Trace; Trace.SilverBefore = Silver->GetSilver();
    TArray<FLKAICandidate> Candidates;
    for (int32 I=0; I<Deck->GetHandSize(); ++I)
    {
        const auto* Card = GM->FindCard(Deck->GetHandCard(I));
        if (!Card || Card->Cost < 0 || Card->Cost > Silver->GetSilver() || !Deck->CanPlayWithCooldown(I, Card->DrawCooldown)) { continue; }
        if (Card->CardType == ELKCardType::Spell && Card->SpellEffect == ELKSpellEffect::SummonZone
            && GM->GetActiveTacticalCircleCount() >= 1) { continue; }
        const auto* Row = Card->CardType == ELKCardType::Spell ? nullptr
            : GM->GetUnitRow(Card->CardType == ELKCardType::Building ? Card->BuildingUnitId : Card->SpawnUnitId);
        const int32 Start = Candidates.Num();
        LKAIDecision::Candidates(*Card, Row, Observation, Silver->GetSilver(), Intent, I, Candidates, GM->GetCardUpgradeScale(Card->CardId, ELKTeam::Enemy));
        for (int32 J=Start; J<Candidates.Num(); ++J) { Candidates[J].Utility.Synergy += GetCounterBonus(Card); }
    }
    Candidates.StableSort([](const auto& A, const auto& B)
    {
        if (A.Utility.Total() != B.Utility.Total()) { return A.Utility.Total() > B.Utility.Total(); }
        if (A.HandIndex != B.HandIndex) { return A.HandIndex < B.HandIndex; }
        if (A.Location.X != B.Location.X) { return A.Location.X < B.Location.X; }
        return A.Location.Y < B.Location.Y;
    });
    Trace.Candidates = Candidates.Num();
    for (int32 I=0; I<FMath::Min(3,Candidates.Num()); ++I) { Trace.Shortlist.Add(Candidates[I]); }
    Trace.Result = TEXT("Wait:NoUsefulAffordableCandidate");
    TMap<int32,int32> ChecksPerCard, CandidateCounts, CandidateRanks, CursorStarts;
    for (const auto& C : Candidates) { ++CandidateCounts.FindOrAdd(C.HandIndex); }
    for (const auto& C : Candidates)
    { CursorStarts.FindOrAdd(C.HandIndex) = CandidateCursors.FindRef(C.CardId) % CandidateCounts.FindRef(C.HandIndex); }
    for (const auto& C : Candidates)
    {
        // A blocked high-scoring card cannot consume the entire budget and starve the
        // other hand slots. Continue its two-candidate window next decision instead of
        // retrying the same blocked points forever. Recompute candidates from fresh
        // observation; the cursor stores only an index, never a hidden/stale target.
        const int32 Count = CandidateCounts.FindRef(C.HandIndex);
        const int32 Rank = CandidateRanks.FindOrAdd(C.HandIndex)++;
        const int32 Start = CursorStarts.FindRef(C.HandIndex);
        // Do not wrap inside this decision: even if the global budget stops after one
        // check, the next cursor follows the exact rank that was actually examined.
        if (Rank < Start || Rank >= Start + 2) { continue; }
        if (ChecksPerCard.FindRef(C.HandIndex) >= 2) { continue; }
        if (Trace.ValidationChecks >= LKAIDecision::MaxValidationChecks) { Trace.Result = TEXT("Wait:ValidationBudget"); break; }
        ++ChecksPerCard.FindOrAdd(C.HandIndex);
        CandidateCursors.FindOrAdd(C.CardId) = (Start + ChecksPerCard.FindRef(C.HandIndex)) % Count;
        ++Trace.ValidationChecks;
        const ELKPlayResult Valid = GM->ValidateCardPlay(ELKTeam::Enemy, C.HandIndex, C.Location, C.Yaw);
        if (Valid != ELKPlayResult::Success)
        { Trace.Result = FString::Printf(TEXT("Rejected:%d"), int32(Valid)); continue; }
        Trace.CardId = C.CardId; Trace.Location = C.Location; Trace.Utility = C.Utility;
        // Fresh snapshot and synchronous transaction: no deferred payment or stale hand
        // index/target transform crosses this boundary. Only the authority spends silver.
        const ELKPlayResult Played = GM->PlayCardForTeam(ELKTeam::Enemy, C.HandIndex, C.Location, C.Yaw);
        if (Played == ELKPlayResult::Success) { CandidateCursors.Remove(C.CardId); }
        Trace.Result = Played == ELKPlayResult::Success ? TEXT("Succeeded") : FString::Printf(TEXT("Cancelled:%d"),int32(Played));
        Trace.SilverAfter = Silver->GetSilver(); RecordDecision(MoveTemp(Trace));
        return Played == ELKPlayResult::Success;
    }
    Trace.SilverAfter = Silver->GetSilver(); RecordDecision(MoveTemp(Trace)); return false;
}

bool ALKOpponentBrain::PickTargetLocation(FName CardId, FVector& OutLocation) const
{
    const ALKBattleGameMode* GM = GameMode.Get();
    const auto* Card = GM ? GM->FindCard(CardId) : nullptr;
    if (!Card || !Silver) { return false; }
    const auto* Row = Card->CardType == ELKCardType::Spell ? nullptr
        : GM->GetUnitRow(Card->CardType == ELKCardType::Building ? Card->BuildingUnitId : Card->SpawnUnitId);
    TArray<FLKAICandidate> Candidates;
    LKAIDecision::Candidates(*Card,Row,Observation,Silver->GetSilver(),Intent,INDEX_NONE,Candidates,GM->GetCardUpgradeScale(CardId,ELKTeam::Enemy));
    if (Candidates.IsEmpty()) { return false; }
    OutLocation = Candidates[0].Location; return true;
}
