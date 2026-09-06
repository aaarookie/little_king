#include "ALKBattleGameMode.h"
#include "ALKHeroCamp.h"
#include "ALKUnitHero.h"
#include "ALKPresentationHUD.h"
#include "Sound/SoundBase.h"

#include "Blueprint/UserWidget.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

#include "ALKBattleGameState.h"
#include "ALKOpponentBrain.h"
#include "ALKPlayerController.h"
#include "ALKPlayerState.h"
#include "ALKProjectile.h"
#include "ALKUnitBase.h"
#include "ALKUnitBuilding.h"
#include "ALKUnitHero.h"
#include "LKDataTypes.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "ULKCardDefinition.h"
#include "ULKBattleHUDWidget.h"
#include "ULKDeckState.h"
#include "ULKGameData.h"
#include "ULKSilverComponent.h"

ALKBattleGameMode::ALKBattleGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	// 纯 UI 驱动战斗，不需要 Pawn
	DefaultPawnClass = nullptr;
    HUDClass = ALKPresentationHUD::StaticClass();
	GameStateClass = ALKBattleGameState::StaticClass();
	PlayerStateClass = ALKPlayerState::StaticClass();
	PlayerControllerClass = ALKPlayerController::StaticClass();
}

void ALKBattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureGameData();
	SetPhase(ELKGamePhase::Deployment);

	// 生成敌方 AI
	FActorSpawnParameters Params;
	Params.Owner = this;
	OpponentBrain = GetWorld()->SpawnActor<ALKOpponentBrain>(ALKOpponentBrain::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (OpponentBrain)
	{
		OpponentBrain->InitBrain(GameData, this);
	}

	// 敌方默认英雄就位（否则玩家没有可击败的目标，胜负永不触发）
	AutoDeployDefaultHeroes(ELKTeam::Enemy);

	// S5 弹道对象池预生成（命中/超时回收复用，避免每箭 New/Delete）
	InitProjectilePool();

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] GameMode 就绪，无限部署时间，全部英雄部署后由玩家开始"));
}

void ALKBattleGameMode::InitProjectilePool()
{
	UWorld* World = GetWorld();
	if (!World || ProjectilePool.Num() > 0)
	{
		return;
	}

	// 预生成 32 条，全部停用藏在战场外
	const int32 PreSpawnCount = 32;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	for (int32 i = 0; i < PreSpawnCount; ++i)
	{
		ALKProjectile* Projectile = World->SpawnActor<ALKProjectile>(
			ALKProjectile::StaticClass(), FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, Params);
		if (Projectile)
		{
			Projectile->DeactivateToPool();
			ProjectilePool.Add(Projectile);
		}
	}

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 弹道对象池就绪：%d 条"), ProjectilePool.Num());
}

ALKProjectile* ALKBattleGameMode::AcquireProjectile(const FVector& Location, float Damage, ELKTeam Team, AActor* InInstigator, const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 复用池中闲置弹道
	for (ALKProjectile* Projectile : ProjectilePool)
	{
		if (Projectile && !Projectile->IsPooledActive())
		{
			Projectile->ActivateFromPool(Location, Damage, Team, InInstigator, Direction);
			return Projectile;
		}
	}

	// 池耗尽（极端场面）：动态扩一条（保底可玩）
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	ALKProjectile* Projectile = World->SpawnActor<ALKProjectile>(
		ALKProjectile::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Projectile)
	{
		Projectile->ActivateFromPool(Location, Damage, Team, InInstigator, Direction);
		ProjectilePool.Add(Projectile);
		UE_LOG(LogLKBattle, Warning, TEXT("[Battle] 弹道池耗尽，动态扩容至 %d 条（预期内可忽略；若频繁出现说明场上远程单位过多）"),
			ProjectilePool.Num());
	}
	return Projectile;
}

void ALKBattleGameMode::ReleaseProjectile(ALKProjectile* Projectile)
{
	if (!Projectile)
	{
		return;
	}

	// 确认是本池管理的弹道（旧路径直生成的直接销毁）
	bool bInPool = false;
	for (const TObjectPtr<ALKProjectile>& P : ProjectilePool)
	{
		if (P == Projectile)
		{
			bInPool = true;
			break;
		}
	}

	if (!bInPool)
	{
		Projectile->Destroy();
		return;
	}

	Projectile->DeactivateToPool();
}

void ALKBattleGameMode::ReleaseAllProjectiles()
{
	for (ALKProjectile* Projectile : ProjectilePool)
	{
		if (Projectile)
		{
			Projectile->DeactivateToPool();
		}
	}
}

void ALKBattleGameMode::GetProjectilePoolStats(int32& OutTotal, int32& OutActive) const
{
	OutTotal = ProjectilePool.Num();
	OutActive = 0;
	for (const ALKProjectile* Projectile : ProjectilePool)
	{
		if (Projectile && Projectile->IsPooledActive())
		{
			++OutActive;
		}
	}
}

void ALKBattleGameMode::PlayLKOneShot(FName SoundId, const FVector& Location, float VolumeScale)
{
	LKGameplay::PlayOneShot(GetWorld(), GameData, SoundId, Location, VolumeScale);
}

void ALKBattleGameMode::TriggerCameraShake(float BaseIntensity)
{
	const float Scale = GameData ? GameData->CameraShakeScale : 1.f;
	if (Scale <= 0.f)
	{
		return; // 可配置关闭震屏
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ALKPlayerController* PC = Cast<ALKPlayerController>(World->GetFirstPlayerController()))
	{
		PC->RequestCameraShake(BaseIntensity * Scale);
	}
}

void ALKBattleGameMode::EnsureGameData()
{
	if (!GameData)
	{
		GameData = NewObject<ULKGameData>(this, TEXT("DA_GameData_Runtime"));
		UE_LOG(LogLKBattle, Log, TEXT("[Battle] 未配置 DA_GameData，已创建运行时默认配置"));
	}

	GameData->EnsureDefaultDecks();
    BattleRandom.Initialize(GameData->BattleSeed);
    SeedTieBreaker = BattleRandom.RandRange(0, 1) == 0 ? ELKTeam::Player : ELKTeam::Enemy;
    MatchStats.Seed = GameData->BattleSeed;
    for (const auto& Pair : GameData->SoundMap)
    {
        if (USoundBase* Sound = Pair.Value.LoadSynchronous()) { PreloadedSounds.Add(Pair.Key, Sound); }
    }
    auto AddUnit = [this](FName Id, ELKUnitClass Type, ELKAttackType Attack, float HP, float Range, float Damage)
    {
        FLKUnitRow& Row = FallbackUnitRows.FindOrAdd(Id);
        Row.UnitId = Id; Row.UnitClass = Type; Row.AttackType = Attack;
        Row.BaseHealth = HP; Row.AttackRange = Range; Row.AttackDamage = Damage;
    };
    AddUnit(TEXT("Hero_Knight"), ELKUnitClass::Hero, ELKAttackType::Melee, 800.f, 170.f, 35.f);
    AddUnit(TEXT("Hero_Mage"), ELKUnitClass::Hero, ELKAttackType::Ranged, 450.f, 700.f, 25.f);
    FallbackUnitRows[TEXT("Hero_Mage")].bIsMage = true;
    AddUnit(TEXT("Hero_Ranger"), ELKUnitClass::Hero, ELKAttackType::Ranged, 500.f, 850.f, 25.f);
    AddUnit(TEXT("Unit_Swordsman"), ELKUnitClass::Soldier, ELKAttackType::Melee, 140.f, 150.f, 15.f);
    AddUnit(TEXT("Unit_Archer"), ELKUnitClass::Soldier, ELKAttackType::Ranged, 100.f, 700.f, 18.f);
    AddUnit(TEXT("Unit_Shieldbearer"), ELKUnitClass::Soldier, ELKAttackType::Melee, 250.f, 150.f, 8.f);
    FallbackUnitRows[TEXT("Unit_Shieldbearer")].HeroTraits = { TEXT("Taunt") };
    AddUnit(TEXT("Building_ArrowTower"), ELKUnitClass::Building, ELKAttackType::Ranged, 400.f, 900.f, 20.f);
    FallbackUnitRows[TEXT("Building_ArrowTower")].BuildingBehavior = ELKBuildingBehavior::Turret;
    AddUnit(TEXT("Building_Barracks"), ELKUnitClass::Building, ELKAttackType::Melee, 450.f, 0.f, 0.f);
    FallbackUnitRows[TEXT("Building_Barracks")].BuildingBehavior = ELKBuildingBehavior::Barracks;
    FallbackUnitRows[TEXT("Building_Barracks")].SpawnUnitId = TEXT("Unit_Swordsman");

	UnitTableCached = GameData->UnitTable.LoadSynchronous();

	// 原型期内置卡牌库（编辑器配置 CardLibrary 后不再走这里）
	if (GameData->CardLibrary.Num() == 0)
	{
		auto AddCard = [this](FName Id, const TCHAR* Name, int32 Cost, ELKCardType Type,
			FName SpawnId, ELKSpellEffect SpellFx, float SpellValue, float SpellRadius)
		{
			ULKCardDefinition* Card = NewObject<ULKCardDefinition>(GameData, Id);
			Card->CardId = Id;
			Card->CardName = FText::FromString(Name);
			Card->Cost = Cost;
			Card->CardType = Type;
			Card->SpawnUnitId = SpawnId;
			Card->BuildingUnitId = SpawnId;
			Card->SpellEffect = SpellFx;
			Card->SpellValue = SpellValue;
			Card->SpellRadius = SpellRadius;
			GameData->CardLibrary.Add(Card);
		};

		AddCard(TEXT("Unit_Swordsman"),    TEXT("剑士"),   2, ELKCardType::Unit,     TEXT("Unit_Swordsman"),    ELKSpellEffect::None,   0.f,   0.f);
		AddCard(TEXT("Unit_Archer"),       TEXT("弓箭手"), 3, ELKCardType::Unit,     TEXT("Unit_Archer"),       ELKSpellEffect::None,   0.f,   0.f);
		AddCard(TEXT("Unit_Shieldbearer"), TEXT("盾卫"),   3, ELKCardType::Unit,     TEXT("Unit_Shieldbearer"), ELKSpellEffect::None,   0.f,   0.f);
		AddCard(TEXT("Spell_Fireball"),    TEXT("火球术"), 4, ELKCardType::Spell,    NAME_None,                 ELKSpellEffect::Damage, 60.f,  250.f);
		AddCard(TEXT("Spell_HealWave"),    TEXT("治疗波"), 3, ELKCardType::Spell,    NAME_None,                 ELKSpellEffect::Heal,   40.f,  300.f);
		AddCard(TEXT("Building_ArrowTower"), TEXT("箭塔"), 5, ELKCardType::Building, TEXT("Building_ArrowTower"), ELKSpellEffect::None, 0.f,   0.f);
		AddCard(TEXT("Building_Barracks"), TEXT("兵营"),   4, ELKCardType::Building, TEXT("Building_Barracks"), ELKSpellEffect::None,   0.f,   0.f);
	}

	if (AvailableHeroes.Num() == 0)
	{
		AvailableHeroes = { TEXT("Hero_Knight"), TEXT("Hero_Mage"), TEXT("Hero_Ranger") };
	}
}

void ALKBattleGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (Phase)
	{
	case ELKGamePhase::Deployment:
		TickDeployment(DeltaSeconds);
		break;

	case ELKGamePhase::Battle:
		TickBattle(DeltaSeconds);
		break;

	default:
		break;
	}

	if (Phase != ELKGamePhase::Result)
	{
		DrawFieldBounds();
	}
}

void ALKBattleGameMode::TickDeployment(float DeltaSeconds)
{
    TryInitPlayerState(); // 无限部署时间，只有玩家主动开始。
}

void ALKBattleGameMode::TryInitPlayerState()
{
	if (bPlayerStateReady)
	{
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	ALKPlayerState* PS = PC->GetPlayerState<ALKPlayerState>();
	if (!PS)
	{
		return;
	}

	PS->Silver->Init(GameData->SilverPerSecond, GameData->SilverCap);
	if (!PS->Deck->InitDeck(GameData->DefaultPlayerDeck, GameData->HandSize, GameData->BattleSeed + 1))
	{
		UE_LOG(LogLKBattle, Warning, TEXT("[Deck] 玩家牌库无效：去重后至少需要 HandSize + 1 种卡，禁止开战"));
	}
	PS->Deck->CostProvider = [this](FName CardId) { return GetCardCost(CardId); };
	bPlayerStateReady = true;

	// 创建战斗 HUD（BP_ALKBattleGameMode 类默认值里配置 HUDWidgetClass）
	if (HUDWidgetClass)
	{
		if (ULKBattleHUDWidget* HUD = CreateWidget<ULKBattleHUDWidget>(GetWorld(), HUDWidgetClass))
		{
			HUD->AddToViewport();
			UE_LOG(LogLKBattle, Log, TEXT("[Battle] 战斗 HUD 已创建：%s"), *HUDWidgetClass->GetName());
		}
		else
		{
			UE_LOG(LogLKBattle, Warning, TEXT("[Battle] 战斗 HUD 创建失败：%s"), *HUDWidgetClass->GetName());
		}
	}

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 玩家状态就绪：银币 %.1f/s（上限 %.1f），牌库 %d 张"),
		GameData->SilverPerSecond, GameData->SilverCap, GameData->DefaultPlayerDeck.Num());
}

void ALKBattleGameMode::TickBattle(float DeltaSeconds)
{
	BattleElapsed += DeltaSeconds;
	TickPlayerSilver(DeltaSeconds);

	if (OpponentBrain)
	{
		OpponentBrain->TickBrain(DeltaSeconds, BattleElapsed);
	}

	if (Phase != ELKGamePhase::Battle) { return; }

	if (!bOvertimeActive && BattleElapsed >= GameData->BattleTimeLimit)
	{
		bOvertimeActive = true;
		OvertimeElapsed = 0.f;
		OvertimeTickAccumulator = 0.f;
		if (ALKBattleGameState* GS = GetGameState<ALKBattleGameState>())
		{
			GS->bBattleOvertime = true;
		}
		UE_LOG(LogLKBattle, Log, TEXT("[Battle] 超时！双方英雄进入虚弱状态，强度随时间上升（无平局）"));
	}

	if (bOvertimeActive)
	{
		TickOvertime(FMath::Min(DeltaSeconds, FMath::Max(0.f, BattleElapsed - GameData->BattleTimeLimit)));
	}
}

void ALKBattleGameMode::TickOvertime(float DeltaSeconds)
{
    OvertimeElapsed += DeltaSeconds;
    OvertimeTickAccumulator += DeltaSeconds;
    const float Interval = FMath::Max(1.f, GameData->OvertimeWeaknessTick);
    while (OvertimeTickAccumulator >= Interval && Phase == ELKGamePhase::Battle)
    {
        OvertimeTickAccumulator -= Interval;
        const float TickTime = OvertimeElapsed - OvertimeTickAccumulator;
        const float Pct = GameData->OvertimeWeaknessBasePct * (1.f + GameData->OvertimeWeaknessGrowth * TickTime / Interval);
        BeginCombatBatch();
        for (int32 TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
        {
            const TArray<ALKUnitBase*> Snapshot = AliveHeroes[TeamIdx];
            for (ALKUnitBase* Hero : Snapshot)
            {
                if (IsValid(Hero) && Hero->IsAlive()) { LKGameplay::ApplyMaxHealthPercentDamage(Hero, Pct, nullptr); }
            }
        }
        EndCombatBatch();
    }
}

void ALKBattleGameMode::TickPlayerSilver(float DeltaSeconds)
{
    if (bPlayerStateReady)
    {
        if (ULKSilverComponent* Silver = GetTeamSilver(ELKTeam::Player)) { Silver->TickSilver(DeltaSeconds); }
    }
}

void ALKBattleGameMode::SetPhase(ELKGamePhase NewPhase)
{
    if (Phase == NewPhase) { return; }
    if (NewPhase == ELKGamePhase::Battle && !CanStartBattle()) { return; }
    Phase = NewPhase;
    ApplyCombatEnabledToAllUnits(NewPhase == ELKGamePhase::Battle);
    if (NewPhase == ELKGamePhase::Battle) { PlayLKOneShot(TEXT("BattleStart"), FVector::ZeroVector); }
    if (ALKBattleGameState* GS = GetGameState<ALKBattleGameState>()) { GS->SetPhase(NewPhase); }
}

void ALKBattleGameMode::ApplyCombatEnabledToAllUnits(bool bEnabled)
{
	for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
	{
		(*It)->SetCombatEnabled(bEnabled && !(*It)->IsCamp());
	}
}

void ALKBattleGameMode::AutoDeployDefaultHeroes(ELKTeam Team)
{
    if (Team == ELKTeam::Player) { return; }
    for (int32 i = 0; i < AvailableHeroes.Num(); ++i)
    {
        const float X = (i - (AvailableHeroes.Num() - 1) * 0.5f) * GameData->FieldHalfWidth * 0.6f;
        const FVector Location(X, GameData->FieldHalfHeight * 0.65f, 0.f);
        const ELKPlayResult Result = DeployHero(Team, AvailableHeroes[i], Location);
        if (Result != ELKPlayResult::Success) { UE_LOG(LogLKBattle, Error, TEXT("[Deployment] 敌方英雄部署失败 %s: %d"), *AvailableHeroes[i].ToString(), int32(Result)); }
    }
}

bool ALKBattleGameMode::HasValidDecks() const
{
    for (ELKTeam Side : { ELKTeam::Player, ELKTeam::Enemy })
    {
        const ULKDeckState* Deck = GetTeamDeck(Side);
        if (!Deck || !Deck->IsReady()) { return false; }
        for (FName Id : Deck->GetAllCards()) { if (!FindCard(Id)) { return false; } }
    }
    return true;
}

float ALKBattleGameMode::GetBuildingPlacementAttackRange(FName CardId) const
{
    const ULKCardDefinition* Card = FindCard(CardId);
    if (!Card || Card->CardType != ELKCardType::Building) { return 0.f; }
    const FLKUnitRow* Row = GetUnitRow(Card->BuildingUnitId);
    return Row && Row->BuildingBehavior == ELKBuildingBehavior::Turret && FMath::IsFinite(Row->AttackRange)
        ? FMath::Max(0.f, Row->AttackRange) : 0.f;
}

bool ALKBattleGameMode::CanStartBattle() const
{
    if (Phase != ELKGamePhase::Deployment || !bPlayerStateReady || AvailableHeroes.IsEmpty() || !HasValidDecks()) { return false; }
    for (int32 Side = 0; Side < 2; ++Side)
    {
        for (FName Id : AvailableHeroes) { if (!DeployedHeroes[Side].Contains(Id)) { return false; } }
        if (HeroCounts[Side] != AvailableHeroes.Num()) { return false; }
    }
    return true;
}

void ALKBattleGameMode::ForceStartBattle()
{
    if (CanStartBattle()) { SetPhase(ELKGamePhase::Battle); }
}

void ALKBattleGameMode::ForceEndMatch(ELKTeam Winner)
{
	EndMatch(Winner);
}

void ALKBattleGameMode::EndMatch(ELKTeam Winner)
{
	if (Phase == ELKGamePhase::Result)
	{
		return;
	}

	MatchStats.Duration = BattleElapsed;
    MatchStats.bOvertime = bOvertimeActive;
    MatchStats.Winner = Winner;
    Phase = ELKGamePhase::Result;
    UE_LOG(LogLKBattle, Log, TEXT("[MatchStats] Seed=%d Winner=%d Duration=%.2f Kills=%d/%d Damage=%.1f/%.1f Healing=%.1f/%.1f Cards=%d/%d Overtime=%d Simultaneous=%d"),
        MatchStats.Seed, int32(Winner), BattleElapsed, MatchStats.Player.Kills, MatchStats.Enemy.Kills,
        MatchStats.Player.Damage, MatchStats.Enemy.Damage, MatchStats.Player.Healing, MatchStats.Enemy.Healing,
        MatchStats.Player.CardsPlayed, MatchStats.Enemy.CardsPlayed, int32(bOvertimeActive), int32(MatchStats.bSimultaneousElimination));

	// 对局结束，单位停止战斗
	ApplyCombatEnabledToAllUnits(false);

	// S5：回收全部弹道（防对象池泄漏）
	ReleaseAllProjectiles();

	// S5 音效：胜利/失败
	PlayLKOneShot(Winner == ELKTeam::Player ? TEXT("Victory") : TEXT("Defeat"), FVector::ZeroVector, 1.f);

	if (ALKBattleGameState* GS = GetGameState<ALKBattleGameState>())
	{
		GS->EndMatch(Winner);
	}
}

void ALKBattleGameMode::HandleUnitDied(ALKUnitBase* Unit)
{
    if (!Unit || Unit->IsCamp()) { return; }
    const int32 Side = int32(Unit->GetTeam());
    if (Unit->IsHero())
    {
        AliveHeroes[Side].Remove(Unit);
        HeroCounts[Side] = FMath::Max(0, HeroCounts[Side] - 1);
        bPendingVictoryCheck = true;
    }
    if (Unit->IsBuilding())
    {
        int32& Count = BuildingCounts[Side].FindOrAdd(Unit->GetUnitId());
        Count = FMath::Max(0, Count - 1);
    }
    if (CombatBatchDepth == 0) { CheckVictoryAfterBatch(); }
}

void ALKBattleGameMode::EndCombatBatch()
{
    check(CombatBatchDepth > 0);
    --CombatBatchDepth;
    if (CombatBatchDepth == 0) { CheckVictoryAfterBatch(); }
}

void ALKBattleGameMode::CheckVictoryAfterBatch()
{
    if (!bPendingVictoryCheck || Phase != ELKGamePhase::Battle) { return; }
    bPendingVictoryCheck = false;
    if (HeroCounts[0] > 0 && HeroCounts[1] > 0) { return; }
    if (HeroCounts[0] <= 0 && HeroCounts[1] <= 0)
    {
        MatchStats.bSimultaneousElimination = true;
        const float Difference = MatchStats.Player.Damage - MatchStats.Enemy.Damage;
        EndMatch(FMath::IsNearlyZero(Difference) ? SeedTieBreaker : (Difference > 0.f ? ELKTeam::Player : ELKTeam::Enemy));
    }
    else { EndMatch(HeroCounts[0] > 0 ? ELKTeam::Player : ELKTeam::Enemy); }
}

void ALKBattleGameMode::RecordCombatEvent(const FLKCombatEvent& Event)
{
    if (Phase == ELKGamePhase::Result) { return; }
    if (Event.Source.bHasTeam)
    {
        FLKTeamMatchStats& Stats = Event.Source.Team == ELKTeam::Player ? MatchStats.Player : MatchStats.Enemy;
        if (Event.bIsHeal) { Stats.Healing += Event.ActualAmount; }
        else if (Event.Source.Team != Event.TargetTeam)
        {
            Stats.Damage += Event.ActualAmount;
            if (Event.bKilled) { ++Stats.Kills; }
        }
    }
    OnCombatEvent.Broadcast(Event);
    if (Event.ActualAmount > 0.f) { OnDamageEvent.Broadcast(Event.Location, Event.ActualAmount, Event.bIsHeal); }
}

void ALKBattleGameMode::NotifyFireballCast(const FVector& Location)
{
    if (Phase == ELKGamePhase::Battle) { TriggerCameraShake(GameData->FireballShakeIntensity); }
}

USoundBase* ALKBattleGameMode::FindPreloadedSound(FName Id) const
{
    const TObjectPtr<USoundBase>* Sound = PreloadedSounds.Find(Id);
    return Sound ? Sound->Get() : nullptr;
}

ELKPlayResult ALKBattleGameMode::ValidateCardPlay(ELKTeam Team, int32 HandIndex, const FVector& Location) const
{
    if (Phase != ELKGamePhase::Battle) { return ELKPlayResult::WrongPhase; }
    const ULKDeckState* Deck = GetTeamDeck(Team);
    const ULKSilverComponent* Silver = GetTeamSilver(Team);
    if (!Deck || !Silver) { return ELKPlayResult::Unknown; }
    const FName Id = Deck->GetHandCard(HandIndex);
    if (Id.IsNone()) { return ELKPlayResult::HandEmpty; }
    const ULKCardDefinition* Card = FindCard(Id);
    if (!Card || Card->Cost < 0) { return ELKPlayResult::InvalidCardData; }
    if (Card->CardType == ELKCardType::Spell)
    {
        if (Card->SpellEffect == ELKSpellEffect::None || !FMath::IsFinite(Card->SpellValue) || Card->SpellValue <= 0.f
            || !FMath::IsFinite(Card->SpellRadius) || Card->SpellRadius <= 0.f) { return ELKPlayResult::InvalidCardData; }
        if (!IsInsideField(Location)) { return ELKPlayResult::InvalidLocation; }
        if (!CanPlaceSpellAt(Team, Location)) { return ELKPlayResult::SpellLocked; }
    }
    else
    {
        const FName UnitId = Card->CardType == ELKCardType::Building ? Card->BuildingUnitId : Card->SpawnUnitId;
        const FLKUnitRow* Row = GetUnitRow(UnitId);
        if (!Row || Row->UnitClass != (Card->CardType == ELKCardType::Building ? ELKUnitClass::Building : ELKUnitClass::Soldier)) { return ELKPlayResult::InvalidCardData; }
        // -2 继承全局，-1 不限，>=0 卡牌自己的上限。
        const int32 Limit = Card->BuildingTypeLimitOverride == -2 ? GameData->BuildingTypeLimit : Card->BuildingTypeLimitOverride;
        if (!IsPlacementValid(Location, Team, Card->CardType, UnitId, Limit)) { return ELKPlayResult::InvalidLocation; }
        if (GameData->MaxUnitsPerTeam > 0 && CountAliveUnits(Team) >= GameData->MaxUnitsPerTeam) { return ELKPlayResult::UnitLimitReached; }
    }
    return Silver->GetSilver() >= Card->Cost ? ELKPlayResult::Success : ELKPlayResult::NotEnoughSilver;
}

ELKPlayResult ALKBattleGameMode::PlayCardForTeam(ELKTeam Team, int32 HandIndex, const FVector& Location)
{
    if (bResolvingCard) { return ELKPlayResult::WrongPhase; }
    const ELKPlayResult Validation = ValidateCardPlay(Team, HandIndex, Location);
    if (Validation != ELKPlayResult::Success) { return Validation; }
    TGuardValue<bool> Guard(bResolvingCard, true);
    ULKDeckState* Deck = GetTeamDeck(Team);
    ULKSilverComponent* Silver = GetTeamSilver(Team);
    ULKCardDefinition* Card = FindCard(Deck->GetHandCard(HandIndex));
    BeginCombatBatch();
    const bool bPaid = Silver->TrySpend(Card->Cost);
    const bool bResolved = bPaid && Phase == ELKGamePhase::Battle && ResolveCard(Card, Team, Location);
    if (bResolved)
    {
        Deck->PlayCard(HandIndex);
        ++(Team == ELKTeam::Player ? MatchStats.Player : MatchStats.Enemy).CardsPlayed;
        PlayLKOneShot(TEXT("CardPlay"), Location, 0.8f);
    }
    else if (bPaid) { Silver->AddSilver(Card->Cost); }
    EndCombatBatch();
    return bResolved ? ELKPlayResult::Success : ELKPlayResult::InvalidCardData;
}

ELKPlayResult ALKBattleGameMode::ValidateHeroDeployment(ELKTeam Team, FName HeroUnitId, const FVector& Location, FVector* OutHeroPosition) const
{
    if (Phase != ELKGamePhase::Deployment) { return ELKPlayResult::WrongPhase; }
    const int32 Side = int32(Team);
    if (!AvailableHeroes.Contains(HeroUnitId)) { return ELKPlayResult::InvalidCardData; }
    if (DeployedHeroes[Side].Contains(HeroUnitId)) { return ELKPlayResult::AlreadyDeployed; }
    if (HeroCounts[Side] >= AvailableHeroes.Num()) { return ELKPlayResult::HeroLimitReached; }
    const FLKUnitRow* Row = GetUnitRow(HeroUnitId);
    if (!Row || Row->UnitClass != ELKUnitClass::Hero) { return ELKPlayResult::InvalidCardData; }
    const float CampRadius = GameData->HeroCampBodyRadius;
    if (!IsPlacementValid(Location, Team, ELKCardType::Building) ||
        FMath::Abs(Location.X) + CampRadius >= GameData->FieldHalfWidth ||
        FMath::Abs(Location.Y) + CampRadius >= GameData->FieldHalfHeight) { return ELKPlayResult::InvalidLocation; }
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if ((*It)->IsAlive() && FVector::Dist2D(Location, (*It)->GetActorLocation()) < CampRadius + (*It)->GetBodyRadius() + 2.f) { return ELKPlayResult::InvalidLocation; }
    }
    FVector HeroPosition = FVector::ZeroVector;
    bool bFound = false;
    for (int32 i = 0; i < 16; ++i)
    {
        const float Angle = (Team == ELKTeam::Player ? PI * 0.5f : -PI * 0.5f) + i * PI / 8.f;
        const FVector Candidate = Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (CampRadius + GameData->UnitBodyRadius + 8.f);
        if (IsPlacementValid(Candidate, Team, ELKCardType::Unit)) { HeroPosition = Candidate; bFound = true; break; }
    }
    if (!bFound) { return ELKPlayResult::InvalidLocation; }
    if (OutHeroPosition) { *OutHeroPosition = HeroPosition; }
    return ELKPlayResult::Success;
}

ELKPlayResult ALKBattleGameMode::DeployHero(ELKTeam Team, FName HeroUnitId, const FVector& Location)
{
    FVector HeroPosition = FVector::ZeroVector;
    const ELKPlayResult Validation = ValidateHeroDeployment(Team, HeroUnitId, Location, &HeroPosition);
    if (Validation != ELKPlayResult::Success) { return Validation; }
    const int32 Side = int32(Team);
    ALKUnitHero* Hero = Cast<ALKUnitHero>(SpawnUnitForTeam(HeroUnitId, Team, HeroPosition, ELKUnitClass::Hero));
    if (!Hero) { return ELKPlayResult::InvalidCardData; }
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ALKHeroCamp* Camp = GetWorld()->SpawnActor<ALKHeroCamp>(ALKHeroCamp::StaticClass(), FVector(Location.X, Location.Y, 0.f), FRotator::ZeroRotator, Params);
    if (!Camp)
    {
        AliveHeroes[Side].Remove(Hero); --HeroCounts[Side]; Hero->Destroy();
        return ELKPlayResult::Unknown;
    }
    Camp->InitializeCamp(Hero, GameData);
    DeployedHeroes[Side].Add(HeroUnitId);
    return ELKPlayResult::Success;
}

ALKUnitBase* ALKBattleGameMode::SpawnUnitForTeam(FName UnitId, ELKTeam Team, const FVector& Location, ELKUnitClass FallbackClass)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 战斗期单位总数上限（波次出兵/兵营出兵同样受控；部署期英雄不受限）
	if (Phase == ELKGamePhase::Battle && GameData->MaxUnitsPerTeam > 0
		&& CountAliveUnits(Team) >= GameData->MaxUnitsPerTeam)
	{
		UE_LOG(LogLKUnit, Warning, TEXT("[Unit] %s 单位已达上限 %d，拒绝生成 %s"),
			Team == ELKTeam::Player ? TEXT("玩家") : TEXT("敌方"), GameData->MaxUnitsPerTeam, *UnitId.ToString());
		return nullptr;
	}

    const FLKUnitRow* Row = GetUnitRow(UnitId);
    if (!Row || !IsInsideField(Location)) { UE_LOG(LogLKUnit, Warning, TEXT("[Spawn] 无效单位或位置 %s"), *UnitId.ToString()); return nullptr; }
    const FLKUnitRow& Data = *Row;
    if (Data.BaseHealth <= 0.f || !FMath::IsFinite(Data.BaseHealth)
        || !FMath::IsFinite(Data.MoveSpeed) || Data.MoveSpeed < 0.f
        || !FMath::IsFinite(Data.AttackRange) || Data.AttackRange < 0.f
        || !FMath::IsFinite(Data.AttackDamage) || Data.AttackDamage < 0.f
        || !FMath::IsFinite(Data.AttackInterval) || Data.AttackInterval <= 0.f
        || FMath::Abs(Location.X) + GameData->UnitBodyRadius >= GameData->FieldHalfWidth
        || FMath::Abs(Location.Y) + GameData->UnitBodyRadius >= GameData->FieldHalfHeight) { return nullptr; }
    for (TActorIterator<ALKUnitBase> It(World); It; ++It)
    {
        if ((*It)->IsAlive() && (*It)->IsBuilding() && FVector::Dist2D(Location, (*It)->GetActorLocation()) < GameData->UnitBodyRadius + (*It)->GetBodyRadius() + 2.f) { return nullptr; }
    }

	UClass* ClassToSpawn = ALKUnitBase::StaticClass();
	switch (Data.UnitClass)
	{
	case ELKUnitClass::Hero:     ClassToSpawn = ALKUnitHero::StaticClass();     break;
	case ELKUnitClass::Building: ClassToSpawn = ALKUnitBuilding::StaticClass(); break;
	default: break;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	ALKUnitBase* Unit = World->SpawnActor<ALKUnitBase>(ClassToSpawn,
		FVector(Location.X, Location.Y, 0.f), FRotator::ZeroRotator, Params);
	if (!Unit)
	{
		UE_LOG(LogLKUnit, Warning, TEXT("[Unit] 生成失败: %s"), *UnitId.ToString());
		return nullptr;
	}

	Unit->SetTeam(Team);
	Unit->InitUnit(Data, GameData, UnitId);
	Unit->SetCombatEnabled(Phase == ELKGamePhase::Battle);
	Unit->OnUnitDied.AddDynamic(this, &ALKBattleGameMode::HandleUnitDied);

	const int32 TeamIdx = (int32)Team;

	if (Unit->IsHero())
	{
		++HeroCounts[TeamIdx];
		AliveHeroes[TeamIdx].Add(Unit);
	}

	if (Unit->IsBuilding())
	{
		if (ALKUnitBuilding* Building = Cast<ALKUnitBuilding>(Unit))
		{
			Building->BuildingBehavior = Data.BuildingBehavior;
			Building->SpawnUnitId = Data.SpawnUnitId;
			Building->SpawnInterval = Data.SpawnInterval;
		}
		BuildingCounts[TeamIdx].FindOrAdd(UnitId) += 1;
	}

	return Unit;
}

const FLKUnitRow* ALKBattleGameMode::GetUnitRow(FName UnitId) const
{
    if (!GameData) { return nullptr; }
    if (!GameData->UnitTable.IsNull())
    {
        return UnitTableCached && UnitTableCached->GetRowStruct() == FLKUnitRow::StaticStruct()
            ? UnitTableCached->FindRow<FLKUnitRow>(UnitId, TEXT("Units"), false) : nullptr;
    }
    return FallbackUnitRows.Find(UnitId);
}

ULKCardDefinition* ALKBattleGameMode::FindCard(FName CardId) const
{
	if (!GameData)
	{
		return nullptr;
	}

	for (ULKCardDefinition* Card : GameData->CardLibrary)
	{
		if (Card && Card->CardId == CardId)
		{
			return Card;
		}
	}
	return nullptr;
}

int32 ALKBattleGameMode::GetCardCost(FName CardId) const
{
	const ULKCardDefinition* Card = FindCard(CardId);
	return Card ? Card->Cost : 2;
}

bool ALKBattleGameMode::HasMage(ELKTeam Team) const
{
	for (const ALKUnitBase* Hero : AliveHeroes[(int32)Team])
	{
		if (Hero && Hero->IsAlive() && Hero->IsMage())
		{
			return true;
		}
	}
	return false;
}

int32 ALKBattleGameMode::GetHeroCount(ELKTeam Team) const
{
	return HeroCounts[(int32)Team];
}

ALKUnitBase* ALKBattleGameMode::GetRandomAliveHero(ELKTeam Team) const
{
	const TArray<ALKUnitBase*>& Heroes = AliveHeroes[(int32)Team];
	if (Heroes.Num() == 0)
	{
		return nullptr;
	}
	return Heroes[BattleRandom.RandRange(0, Heroes.Num() - 1)];
}

bool ALKBattleGameMode::CanCastSpell(ELKTeam Team) const { return true; }

bool ALKBattleGameMode::HasGlobalSpellPlacement(ELKTeam Team) const
{
    for (const ALKUnitBase* Hero : AliveHeroes[int32(Team)])
    {
        if (IsValid(Hero) && Hero->HasTraitEffect(ELKTraitEffect::GlobalSpellPlacement)) { return true; }
    }
    return false;
}

bool ALKBattleGameMode::CanPlaceSpellAt(ELKTeam Team, FVector Location) const
{
    return IsInsideField(Location) && (HasGlobalSpellPlacement(Team)
        || (Team == ELKTeam::Player ? Location.Y <= 0.f : Location.Y >= 0.f));
}

float ALKBattleGameMode::GetTeamHeroHealthRatio(ELKTeam Team) const { return 0.f; }

int32 ALKBattleGameMode::CountAliveUnits(ELKTeam Team) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		if ((*It)->GetTeam() == Team && (*It)->IsTargetable())
		{
			++Count;
		}
	}
	return Count;
}

int32 ALKBattleGameMode::CountCombatUnitsOfAttackType(ELKTeam Team, ELKAttackType Type) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		const ALKUnitBase* Unit = *It;
		if (Unit->GetTeam() == Team && !Unit->IsDead() && !Unit->IsBuilding()
			&& Unit->GetAttackType() == Type)
		{
			++Count;
		}
	}
	return Count;
}

ALKUnitBase* ALKBattleGameMode::GetWeakestAliveHero(ELKTeam Team) const
{
	const TArray<ALKUnitBase*>& Heroes = AliveHeroes[(int32)Team];
	ALKUnitBase* Weakest = nullptr;
	float WorstRatio = TNumericLimits<float>::Max();

	for (ALKUnitBase* Hero : Heroes)
	{
		if (!Hero || Hero->IsDead())
		{
			continue;
		}
		const float MaxHp = Hero->GetMaxHealth();
		const float Ratio = (MaxHp > 0.f) ? (Hero->GetHealth() / MaxHp) : 0.f;
		if (Ratio < WorstRatio)
		{
			WorstRatio = Ratio;
			Weakest = Hero;
		}
	}
	return Weakest;
}

void ALKBattleGameMode::ForcedTargetAllUnits(ELKTeam Team, AActor* Target, float Duration)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Target) || Duration <= 0.f)
	{
		return;
	}

	int32 Affected = 0;
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		ALKUnitBase* Unit = *It;
		if (Unit->GetTeam() == Team && !Unit->IsDead() && !Unit->IsBuilding())
		{
			Unit->SetForcedTarget(Target, Duration);
			++Affected;
		}
	}
	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 集火指令：%s 全体 %d 个单位 -> %s（%.0f 秒）"),
		Team == ELKTeam::Player ? TEXT("玩家") : TEXT("敌方"), Affected, *Target->GetName(), Duration);
}

bool ALKBattleGameMode::FindBestSpellTarget(ELKTeam CasterTeam, float Radius, FVector& OutLocation, ELKSpellEffect Effect) const
{
    if (!GetWorld() || Radius <= 0.f) { return false; }
    TArray<ALKUnitBase*> Candidates;
    const bool bHeal = Effect == ELKSpellEffect::Heal;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Unit = *It;
        if (Unit->IsTargetable() && ((Unit->GetTeam() == CasterTeam) == bHeal)) { Candidates.Add(Unit); }
    }
    float BestScore = 0.f;
    for (ALKUnitBase* Candidate : Candidates)
    {
        FVector Location = Candidate->GetActorLocation();
        if (!CanPlaceSpellAt(CasterTeam, Location)) { continue; }
        float Score = 0.f;
        for (ALKUnitBase* Other : Candidates)
        {
            if (FVector::Dist2D(Location, Other->GetActorLocation()) <= Radius)
            {
                Score += bHeal ? FMath::Max(0.f, Other->GetMaxHealth() - Other->GetHealth()) : (Other->IsHero() ? 2.f : 1.f);
            }
        }
        if (Score > BestScore) { BestScore = Score; OutLocation = Location; }
    }
    return BestScore > 0.f;
}

bool ALKBattleGameMode::IsPlacementValid(const FVector& Location, ELKTeam Team, ELKCardType CardType, FName BuildingUnitId, int32 BuildingLimit) const
{
    if (!IsInsideField(Location) || (Team == ELKTeam::Player ? Location.Y > 0.f : Location.Y < 0.f)) { return false; }
    const float Radius = GameData->UnitBodyRadius;
    if (FMath::Abs(Location.X) + Radius >= GameData->FieldHalfWidth || FMath::Abs(Location.Y) + Radius >= GameData->FieldHalfHeight) { return false; }
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if ((*It)->IsAlive() && FVector::Dist2D(Location, (*It)->GetActorLocation()) < Radius + (*It)->GetBodyRadius() + 2.f) { return false; }
    }
    const int32 Limit = BuildingLimit == -2 ? GameData->BuildingTypeLimit : BuildingLimit;
    return CardType != ELKCardType::Building || Limit < 0 || BuildingUnitId.IsNone()
        || BuildingCounts[int32(Team)].FindRef(BuildingUnitId) < Limit;
}

bool ALKBattleGameMode::IsInsideField(const FVector& Location) const
{
	return GameData && !Location.ContainsNaN() && FMath::Abs(Location.X) <= GameData->FieldHalfWidth
		&& FMath::Abs(Location.Y) <= GameData->FieldHalfHeight;
}

ULKSilverComponent* ALKBattleGameMode::GetTeamSilver(ELKTeam Team) const
{
	if (Team == ELKTeam::Player)
	{
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const ALKPlayerState* PS = PC ? PC->GetPlayerState<ALKPlayerState>() : nullptr;
		return PS ? PS->Silver : nullptr;
	}

	return OpponentBrain ? OpponentBrain->GetSilver() : nullptr;
}

ULKDeckState* ALKBattleGameMode::GetTeamDeck(ELKTeam Team) const
{
	if (Team == ELKTeam::Player)
	{
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const ALKPlayerState* PS = PC ? PC->GetPlayerState<ALKPlayerState>() : nullptr;
		return PS ? PS->Deck : nullptr;
	}

	return OpponentBrain ? OpponentBrain->GetDeck() : nullptr;
}

bool ALKBattleGameMode::ResolveCard(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location)
{
    if (!Card) { return false; }
    switch (Card->CardType)
    {
    case ELKCardType::Unit: return SpawnUnitForTeam(Card->SpawnUnitId, Team, Location) != nullptr;
    case ELKCardType::Building: return SpawnUnitForTeam(Card->BuildingUnitId, Team, Location, ELKUnitClass::Building) != nullptr;
    case ELKCardType::Spell: return CastSpell(Card, Team, Location);
    }
    return false;
}

bool ALKBattleGameMode::CastSpell(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location)
{
    if (!GetWorld() || !Card || Card->SpellEffect == ELKSpellEffect::None || !CanPlaceSpellAt(Team, Location)) { return false; }
    FLKCombatSource Source;
    Source.bHasTeam = true; Source.Team = Team; Source.ActionId = Card->CardId; Source.Kind = ELKCombatSourceKind::Spell;
    TArray<ALKUnitBase*> Targets;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if ((*It)->IsTargetable() && FVector::Dist2D(Location, (*It)->GetActorLocation()) <= Card->SpellRadius) { Targets.Add(*It); }
    }
    BeginCombatBatch();
    if (Card->CardId == TEXT("Spell_Fireball")) { NotifyFireballCast(Location); }
    for (ALKUnitBase* Unit : Targets)
    {
        if (Card->SpellEffect == ELKSpellEffect::Damage && Unit->GetTeam() != Team) { LKGameplay::ApplyDamage(Unit, Card->SpellValue, nullptr, false, &Source); }
        else if (Card->SpellEffect == ELKSpellEffect::Heal && Unit->GetTeam() == Team) { LKGameplay::ApplyHeal(Unit, Card->SpellValue, nullptr, &Source); }
    }
    EndCombatBatch();
    return true; // 合法空放是玩家选择，也会消耗卡牌和银币。
}

void ALKBattleGameMode::DrawFieldBounds() const
{
	if (!GameData || !GameData->bDrawFieldBounds)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center(0.f, 0.f, 0.f);
	const FVector Extent(GameData->FieldHalfWidth, GameData->FieldHalfHeight, 5.f);
	DrawDebugBox(World, Center, Extent, FColor::White, false, -1.f, 0, 1.f);

	// 正式中线由 LKPresentationHUD 绘制，不依赖调试开关或地面深度。
}
