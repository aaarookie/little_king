#include "ALKBattleGameMode.h"
#include "ULKUnitPassiveComponent.h"
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
#include "Kismet/GameplayStatics.h"

#include "ALKBattleGameState.h"
#include "ALKOpponentBrain.h"
#include "ALKPlayerController.h"
#include "ALKPlayerState.h"
#include "ALKProjectile.h"
#include "ALKUnitBase.h"
#include "ULKUnitStatusComponent.h"
#include "ALKUnitBuilding.h"
#include "ALKUnitHero.h"
#include "LKDataTypes.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "LKUnitContent.h"
#include "ULKCardDefinition.h"
#include "ULKBattleHUDWidget.h"
#include "ULKDeckState.h"
#include "ULKGameData.h"
#include "ULKSilverComponent.h"
#include "ULKRunSubsystem.h"
#include "ULKSaveSlotSubsystem.h"
#include "LKBattleArtPreview.h"
#include "ULKPresentationSubsystem.h"

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
#if WITH_EDITOR
    if (LKBattleArtPreview::Enabled()) { LKBattleArtPreview::Prepare(this, GameData); }
    else
#endif
	if (ULKSaveSlotSubsystem::RouteInitialPlayToMenu(GetWorld())) { SetActorTickEnabled(false); return; }

	const bool bHasAuthoredGameData = IsValid(GameData);
	EnsureGameData();
	if (!InitializeExpeditionContext(bHasAuthoredGameData && GameData->bEnableExpeditionFlow))
	{
		// H3 修复（用户报告"继续远征后双方面对面不攻击"）：
		// 远征还在进行中、但当前不是开战点（例如安全点停在奖励面板时点"继续"，或恢复中枢）——
		// 这时不能静默退回独立单场（那会让这一房变成没有远征身份的假战斗，双方站着不动也推不动远征），
		// 而是进入"恢复中枢"：只显示奖励/路线面板，由玩家处理完再进下一房。
		ULKRunSubsystem* Run = GetRunSubsystem();
		if (bHasAuthoredGameData && GameData->bEnableExpeditionFlow && Run && Run->HasRun() && !Run->IsTerminal())
		{
			bRecoveredJunction = true;
			bExpeditionBattle = true;
			UE_LOG(LogLKBattle, Log, TEXT("[Run] 远征进行中但当前不是开战点（Phase=%d）：进入恢复中枢显示奖励/路线面板，不创建假战斗"),
				int32(Run->GetRunPhase()));
		}
	}
	SetPhase(ELKGamePhase::Deployment);
    GetWorld()->GetSubsystem<ULKPresentationSubsystem>()->SetupBattlefield(this);

	// 生成敌方 AI
	FActorSpawnParameters Params;
	Params.Owner = this;
	OpponentBrain = GetWorld()->SpawnActor<ALKOpponentBrain>(ALKOpponentBrain::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (OpponentBrain)
	{
		OpponentBrain->InitBrain(GameData, this, bEnemyUsesCards);
	}

	// 敌方默认英雄就位（否则玩家没有可击败的目标，胜负永不触发）。
	// D5：恢复中枢/终态世界不部署敌人、不进入战斗准备（只展示恢复面板）。
	if (!bRecoveredJunction && !bRecoveredTerminal)
	{
		if (GameData->EnemyEncounterId.IsNone()) { AutoDeployDefaultHeroes(ELKTeam::Enemy); }
		else if (!(bExpeditionBattle ? ApplyEnemyEncounter(CurrentEncounter) : ConfigureEnemyEncounter(GameData->EnemyEncounterId)))
		{
			UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 无效 EnemyEncounterId %s"), *GameData->EnemyEncounterId.ToString());
		}
	}

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
	if (GameData && GameData->GetOuter() != this) { GameData = DuplicateObject<ULKGameData>(GameData, this); }
	if (!GameData)
	{
		GameData = NewObject<ULKGameData>(this, TEXT("DA_GameData_Runtime"));
		UE_LOG(LogLKBattle, Log, TEXT("[Battle] 未配置 DA_GameData，已创建运行时默认配置"));
	}

	GameData->EnsureDefaultDecks();
    BattleRandom.Initialize(GameData->BattleSeed);
    SeedTieBreaker = BattleRandom.RandRange(0, 1) == 0 ? ELKTeam::Player : ELKTeam::Enemy;
    MatchStats.Seed = GameData->BattleSeed;
    BattleContext.AttemptId = FGuid::NewGuid();
    BattleContext.Seed = GameData->BattleSeed;
    GameData->EnsurePresentationDefaults();
    for (const auto& Pair : GameData->SoundMap)
    {
        if (USoundBase* Sound = Pair.Value.LoadSynchronous()) { PreloadedSounds.Add(Pair.Key, Sound); }
    }
	UnitTableCached = GameData->UnitTable.LoadSynchronous();

	// 所有内置单位统一采用“代码规则 + 表内调参”：表行不能意外改变玩法身份，扩展 ID 不受限制。
	FallbackUnitRows.Reset();
	for (const TPair<FName, FLKUnitRow>& Pair : LKUnitContent::Units())
	{
		const FLKUnitRow* Authored = nullptr;
		if (UnitTableCached && UnitTableCached->GetRowStruct() == FLKUnitRow::StaticStruct())
		{
			Authored = UnitTableCached->FindRow<FLKUnitRow>(Pair.Key, TEXT("CoreUnitMerge"), false);
		}
		FallbackUnitRows.Add(Pair.Key, LKUnitContent::MergeAuthoredTuning(Pair.Value, Authored));
	}

	// 运行时卡牌目录（内置卡 + D3 奖励卡）；编辑器配置了 CardLibrary 时只补奖励卡。
	GameData->EnsureCardLibrary();

	if (AvailableHeroes.Num() == 0)
	{
		AvailableHeroes = { TEXT("Hero_Knight"), TEXT("Hero_Mage"), TEXT("Hero_Ranger") };
	}
	EnemyHeroIds = AvailableHeroes;
}

void ALKBattleGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if WITH_EDITOR
    if (LKBattleArtPreview::Enabled())
    { TryInitPlayerState(); LKBattleArtPreview::Tick(this); return; }
#endif

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

	// H3：远征使用本轮冻结的家园加成（金库）；独立单场沿用 DA_GameData 基础值。
	// 只覆盖玩家经济，敌方银币仍由遭遇配置负责。
	const FLKMetaBonusSnapshot& Bonus = BattleContext.BonusSnapshot;
	const float PlayerSilverPerSecond = (bExpeditionBattle && FMath::IsFinite(Bonus.PlayerSilverPerSecond) && Bonus.PlayerSilverPerSecond > 0.f)
		? Bonus.PlayerSilverPerSecond : GameData->SilverPerSecond;
	const float PlayerSilverCap = (bExpeditionBattle && FMath::IsFinite(Bonus.PlayerSilverCap) && Bonus.PlayerSilverCap > 0.f)
		? Bonus.PlayerSilverCap : GameData->SilverCap;

	PS->Silver->Init(PlayerSilverPerSecond, PlayerSilverCap);
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
			BattleHUDWidget = HUD;
			HUD->AddToViewport();
			UE_LOG(LogLKBattle, Log, TEXT("[Battle] 战斗 HUD 已创建：%s"), *HUDWidgetClass->GetName());
		}
		else
		{
			UE_LOG(LogLKBattle, Warning, TEXT("[Battle] 战斗 HUD 创建失败：%s"), *HUDWidgetClass->GetName());
		}
	}

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 玩家状态就绪：银币 %.3f/s（上限 %.1f），牌库 %d 张"),
		PlayerSilverPerSecond, PlayerSilverCap, GameData->DefaultPlayerDeck.Num());
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
    if (NewPhase == ELKGamePhase::Battle && !CanStartBattle())
    {
        // 拒绝时必须可诊断：以前这里静默返回，画面上表现为"双方面对面却不攻击"，很难定位。
        LogStartBattleBlockers();
        return;
    }
    Phase = NewPhase;
    ApplyCombatEnabledToAllUnits(NewPhase == ELKGamePhase::Battle);
    if (NewPhase == ELKGamePhase::Battle) { PlayLKOneShot(TEXT("BattleStart"), FVector::ZeroVector); }
    if (ALKBattleGameState* GS = GetGameState<ALKBattleGameState>()) { GS->SetPhase(NewPhase); }
}

void ALKBattleGameMode::LogStartBattleBlockers() const
{
    UE_LOG(LogLKBattle, Warning,
        TEXT("[Battle] 无法进入战斗阶段（单位保持冻结、面对面也不会攻击）：阶段=%d 玩家状态就绪=%d 玩家牌库有效=%d 敌方用牌=%d"),
        int32(Phase), int32(bPlayerStateReady), int32(HasValidDecks()), int32(bEnemyUsesCards));
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const TArray<FName>& Roster = Side == 0 ? AvailableHeroes : EnemyHeroIds;
        FString Deployed;
        for (FName Id : Roster)
        {
            Deployed += FString::Printf(TEXT("%s%s "), *Id.ToString(),
                DeployedHeroes[Side].Contains(Id) ? TEXT("已部署") : TEXT("未部署"));
        }
        UE_LOG(LogLKBattle, Warning, TEXT("[Battle]   阵营%d：名单 %d 名［%s］；场上英雄 %d 名，登记部署 %d 名"),
            Side, Roster.Num(), *Deployed, HeroCounts[Side], DeployedHeroes[Side].Num());
    }
}

bool ALKBattleGameMode::IsAnyUnitCombatEnabled() const
{
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if (!It->IsCamp() && It->IsCombatEnabled()) { return true; }
    }
    return false;
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
    for (int32 i = 0; i < EnemyHeroIds.Num(); ++i)
    {
        const float X = (i - (EnemyHeroIds.Num() - 1) * 0.5f) * GameData->FieldHalfWidth * 0.6f;
        const FVector Location(X, GameData->FieldHalfHeight * 0.65f, 0.f);
        const ELKPlayResult Result = DeployHero(Team, EnemyHeroIds[i], Location);
        if (Result != ELKPlayResult::Success) { UE_LOG(LogLKBattle, Error, TEXT("[Deployment] 敌方英雄部署失败 %s: %d"), *EnemyHeroIds[i].ToString(), int32(Result)); }
    }
    // BUG-015 排查埋点：敌方英雄每房必须按遭遇配置满血开局；若此处 HP 非满血，说明生成链路被外部改动。
    for (const ALKUnitBase* Hero : AliveHeroes[1])
    {
        if (Hero)
        {
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 敌方自动部署：%s HP %.0f/%.0f（BaseMax %.0f）"),
                *Hero->GetUnitId().ToString(), Hero->GetHealth(), Hero->GetMaxHealth(), Hero->GetBaseMaxHealth());
        }
    }
}

bool ALKBattleGameMode::HasValidDecks() const
{
    for (ELKTeam Side : { ELKTeam::Player, ELKTeam::Enemy })
    {
        if (Side == ELKTeam::Enemy && !bEnemyUsesCards) { continue; }
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
    if (const ULKRunSubsystem* Run = GetRunSubsystem(); Run && Run->NeedsDeckReduction()) { return false; }
    if (Phase != ELKGamePhase::Deployment || !bPlayerStateReady || AvailableHeroes.IsEmpty() || !HasValidDecks()) { return false; }
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const TArray<FName>& Roster = Side == 0 ? AvailableHeroes : EnemyHeroIds;
        if (Roster.IsEmpty()) { return false; }
        for (FName Id : Roster) { if (!DeployedHeroes[Side].Contains(Id)) { return false; } }
        if (HeroCounts[Side] != Roster.Num()) { return false; }
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
    GetWorld()->GetSubsystem<ULKPresentationSubsystem>()->ClearEffects();
    UE_LOG(LogLKBattle, Log, TEXT("[MatchStats] Seed=%d Winner=%d Duration=%.2f Kills=%d/%d Damage=%.1f/%.1f Healing=%.1f/%.1f Cards=%d/%d Overtime=%d Simultaneous=%d"),
        MatchStats.Seed, int32(Winner), BattleElapsed, MatchStats.Player.Kills, MatchStats.Enemy.Kills,
        MatchStats.Player.Damage, MatchStats.Enemy.Damage, MatchStats.Player.Healing, MatchStats.Enemy.Healing,
        MatchStats.Player.CardsPlayed, MatchStats.Enemy.CardsPlayed, int32(bOvertimeActive), int32(MatchStats.bSimultaneousElimination));

	// 对局结束，单位停止战斗
	ApplyCombatEnabledToAllUnits(false);

	// S5：回收全部弹道（防对象池泄漏）
	ReleaseAllProjectiles();

	// S5 音效：胜利/失败
	FinalizeHeroRecovery();
	if (bExpeditionBattle)
	{
		ULKRunSubsystem* Run = GetRunSubsystem();
		if (!Run || !Run->SubmitBattleOutcome(BattleOutcome))
		{
			UE_LOG(LogLKBattle, Error, TEXT("[Run] 拒绝战斗结果：身份不匹配、快照不完整或已重复处理"));
		}
		else if (Winner == ELKTeam::Player && Run->CanAdvance())
		{
			// D3：胜利且有下一间 → 按本房奖励档生成三选一候选并进入 ChoosingReward。
			// 候选生成一次存定（确定性种子），UI 只读；无候选则保持 ChoosingNode 直接可进下一关。
			TArray<FLKRunRewardOffer> Offers;
			if (BuildRunRewardOffers(Offers) && !Run->OfferRewardBatch(Offers))
			{
				UE_LOG(LogLKBattle, Error, TEXT("[Run] 奖励批次提交失败（状态窗口不符或候选非法）"));
			}
		}
	}
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
    PendingDefeats.Add(Unit);
    if (CombatBatchDepth == 0 && !bProcessingDefeats) { ProcessDefeats(); CheckVictoryAfterBatch(); }
}

void ALKBattleGameMode::EndCombatBatch()
{
    check(CombatBatchDepth > 0);
    --CombatBatchDepth;
    if (CombatBatchDepth == 0 && !bProcessingDefeats) { ProcessDefeats(); CheckVictoryAfterBatch(); }
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
    if (Event.ActualAmount > 0.f)
    {
        if (Event.bIsHeal)
        {
            ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Heal, Event.Location);
            ULKPresentationSubsystem::Sound(GetWorld(), "Heal", Event.Location);
        }
        else if (Event.Source.ActionId == "Attack_Siege")
        {
            ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::SiegeImpact, Event.Location, 160.f);
            ULKPresentationSubsystem::Sound(GetWorld(), "SiegeImpact", Event.Location);
        }
        else if (Event.Source.Kind == ELKCombatSourceKind::Projectile)
        { ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Impact, Event.Location, 35.f); }
    }
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
    // Snapshot and stable order: nested healing must not invalidate iteration or reorder equal-health picks.
    if (Event.ActualAmount > 0.f)
    {
        TArray<ALKUnitBase*> Listeners;
        for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It) { if (It->IsAlive()) { Listeners.Add(*It); } }
        Listeners.Sort([](const ALKUnitBase& A, const ALKUnitBase& B) { return A.GetFName().LexicalLess(B.GetFName()); });
        for (ALKUnitBase* Unit : Listeners) { if (IsValid(Unit)) { Unit->GetPassiveComponent()->ObserveCombatEvent(Event); } }
    }
    if (Event.ActualAmount > 0.f) { OnDamageEvent.Broadcast(Event.Location, Event.ActualAmount, Event.bIsHeal); }
}

void ALKBattleGameMode::NotifyFireballCast(const FVector& Location, float Radius, bool bPresent)
{
    if (Phase == ELKGamePhase::Battle)
    {
        TriggerCameraShake(GameData->FireballShakeIntensity);
        if (bPresent) { ULKPresentationSubsystem::Fireball(GetWorld(), Location, Radius); }
    }
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
        if (!CanPlaceSpellAt(Team, Location))
        {
            // BUG-017 排查埋点：全场施法只取决于"存活英雄是否带 GlobalSpellPlacement"。
            // 把当时的存活英雄和各自特性写进日志，避免"法师明明在场却被拦"无从定位。
            FString AliveInfo;
            for (const ALKUnitBase* Hero : AliveHeroes[int32(Team)])
            {
                if (!IsValid(Hero)) { continue; }
                const TArray<FName> Traits = Hero->GetTraits();
                const FString TraitText = Traits.Num() > 0
                    ? FString::JoinBy(Traits, TEXT(","), [](const FName& Id) { return Id.ToString(); })
                    : FString(TEXT("无特性"));
                AliveInfo += FString::Printf(TEXT("%s[%s] "), *Hero->GetUnitId().ToString(), *TraitText);
            }
            const FString HeroesText = AliveInfo.IsEmpty() ? FString(TEXT("无")) : AliveInfo;
            UE_LOG(LogLKBattle, Warning,
                TEXT("[Spell] %s 在敌方半场 %s 施法被拒绝：存活英雄 %s（需要 Trait_MageSpellReach 全场施法）"),
                Team == ELKTeam::Player ? TEXT("玩家") : TEXT("敌方"), *Location.ToCompactString(), *HeroesText);
            return ELKPlayResult::SpellLocked;
        }
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
    // 恢复中枢/终态世界不是战斗场景：不允许部署（否则会出现"能摆兵但永远开不了战"的假战斗）。
    if (bRecoveredJunction || bRecoveredTerminal) { return ELKPlayResult::WrongPhase; }
    const int32 Side = int32(Team);
    const TArray<FName>& Roster = Team == ELKTeam::Player ? AvailableHeroes : EnemyHeroIds;
    if (!Roster.Contains(HeroUnitId)) { return ELKPlayResult::InvalidCardData; }
    if (DeployedHeroes[Side].Contains(HeroUnitId)) { return ELKPlayResult::AlreadyDeployed; }
    if (HeroCounts[Side] >= Roster.Num()) { return ELKPlayResult::HeroLimitReached; }
    const FLKUnitRow* Row = GetUnitRow(HeroUnitId);
    if (!Row || (Row->UnitClass != ELKUnitClass::Hero && Row->UnitClass != ELKUnitClass::Boss)) { return ELKPlayResult::InvalidCardData; }
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
    if (bExpeditionBattle && Team == ELKTeam::Player)
    {
        // 跨房继承仅限玩家英雄：按 HeroId 精确匹配远征状态；敌方英雄绝不走此分支
        // （敌方每房按遭遇配置满血重建，见 AutoDeployDefaultHeroes / BUG-015）。
        const FLKRunHeroState* RunHero = BattleContext.PlayerHeroes.FindByPredicate(
            [HeroUnitId](const FLKRunHeroState& State) { return State.HeroId == HeroUnitId; });
        if (!RunHero || !Hero->ApplyRunHeroState(*RunHero))
        {
            AliveHeroes[Side].Remove(Hero); HeroCounts[Side] = AliveHeroes[Side].Num();
            Hero->OnUnitDied.RemoveDynamic(this, &ALKBattleGameMode::HandleUnitDied);
            Hero->Destroy();
            UE_LOG(LogLKBattle, Error, TEXT("[Run] 英雄 %s 缺少有效跨房间状态"), *HeroUnitId.ToString());
            return ELKPlayResult::InvalidCardData;
        }
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 玩家英雄 %s 部署：继承 HP %.0f/%.0f（BaseMax %.0f）"),
            *HeroUnitId.ToString(), Hero->GetHealth(), Hero->GetMaxHealth(), Hero->GetBaseMaxHealth());
    }
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

ALKUnitBase* ALKBattleGameMode::SpawnUnitForTeam(FName UnitId, ELKTeam Team, const FVector& Location, ELKUnitClass FallbackClass, FName SourceCardId)
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
    FLKUnitRow Data = *Row; // 本地副本：D3 卡升级在副本上放大数值，不改共享行/资产
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

    // D3 卡牌升级：仅远征中的玩家"出牌生成"携带来源卡（波次/兵营产兵/部署/召唤不升级）。
    // 数值 = 基础行 × 1.1^Lv（攻击与生命同步），只作用于本次生成实例。
    if (bExpeditionBattle && Team == ELKTeam::Player && !SourceCardId.IsNone())
    {
        const FLKRunCardState* CardState = BattleContext.PlayerCards.FindByPredicate(
            [&SourceCardId](const FLKRunCardState& Item) { return Item.CardId == SourceCardId; });
        if (CardState && CardState->UpgradeLevel > 0)
        {
            const float Scale = FMath::Pow(1.1f, CardState->UpgradeLevel);
            Data.BaseHealth *= Scale;
            Data.AttackDamage *= Scale;
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 卡升级应用：%s Lv%d -> %s（生命 %.1f / 攻击 %.1f）"),
                *SourceCardId.ToString(), CardState->UpgradeLevel, *UnitId.ToString(), Data.BaseHealth, Data.AttackDamage);
        }
    }

	UClass* ClassToSpawn = ALKUnitBase::StaticClass();
	switch (Data.UnitClass)
	{
	case ELKUnitClass::Hero:     ClassToSpawn = ALKUnitHero::StaticClass();     break;
	case ELKUnitClass::Boss:     ClassToSpawn = ALKUnitHero::StaticClass();     break;
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
    ULKUnitStatusComponent::RefreshTeamSupport(World);

	const int32 TeamIdx = (int32)Team;

	if (Unit->IsHero())
	{
		++HeroCounts[TeamIdx];
		AliveHeroes[TeamIdx].Add(Unit);
		// BUG-015 排查埋点：任何英雄生成（部署/远征/召唤）都记录初始血量，
		// 若发现敌方英雄生成即非满血，可据此区分"生成链路问题"与"开战后被改血"。
		UE_LOG(LogLKBattle, Log, TEXT("[Unit] 英雄生成：%s 阵营%d HP %.0f/%.0f（BaseMax %.0f）"),
			*Unit->GetUnitId().ToString(), TeamIdx, Unit->GetHealth(), Unit->GetMaxHealth(), Unit->GetBaseMaxHealth());
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
	if (LKUnitContent::Find(UnitId))
	{
		return FallbackUnitRows.Find(UnitId);
	}
    if (!GameData->UnitTable.IsNull())
    {
		if (!UnitTableCached || UnitTableCached->GetRowStruct() != FLKUnitRow::StaticStruct()) { return nullptr; }
		if (const FLKUnitRow* Row = UnitTableCached->FindRow<FLKUnitRow>(UnitId, TEXT("Units"), false)) { return Row; }
		return nullptr;
    }
    if (const FLKUnitRow* Row = FallbackUnitRows.Find(UnitId)) { return Row; }
	return nullptr;
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
    // D3：出牌生成时携带来源卡 ID（玩家远征卡升级按 1.1^Level 放大攻击/生命；敌方/独立单场不受影响）
    case ELKCardType::Unit: return SpawnUnitForTeam(Card->SpawnUnitId, Team, Location, ELKUnitClass::Soldier, Card->CardId) != nullptr;
    case ELKCardType::Building: return SpawnUnitForTeam(Card->BuildingUnitId, Team, Location, ELKUnitClass::Building, Card->CardId) != nullptr;
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
    if (Card->CardId == TEXT("Spell_Fireball")) { NotifyFireballCast(Location, Card->SpellRadius); }
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
