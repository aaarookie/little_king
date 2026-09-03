#include "ALKBattleGameMode.h"

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

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] GameMode 就绪，进入部署阶段（%.0f 秒）"), GameData->DeploymentTime);
}

void ALKBattleGameMode::EnsureGameData()
{
	if (!GameData)
	{
		GameData = NewObject<ULKGameData>(this, TEXT("DA_GameData_Runtime"));
		UE_LOG(LogLKBattle, Log, TEXT("[Battle] 未配置 DA_GameData，已创建运行时默认配置"));
	}

	GameData->EnsureDefaultDecks();

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
	TryInitPlayerState();

	PhaseElapsed += DeltaSeconds;
	if (PhaseElapsed >= GameData->DeploymentTime)
	{
		SetPhase(ELKGamePhase::Battle);
		UE_LOG(LogLKBattle, Log, TEXT("[Battle] 部署倒计时结束，自动开战"));
	}
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
	PS->Deck->InitDeck(GameData->DefaultPlayerDeck, GameData->HandSize);
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
		TickOvertime(DeltaSeconds);
	}
}

void ALKBattleGameMode::TickOvertime(float DeltaSeconds)
{
	OvertimeElapsed += DeltaSeconds;
	OvertimeTickAccumulator += DeltaSeconds;

	if (OvertimeTickAccumulator < GameData->OvertimeWeaknessTick)
	{
		return;
	}
	OvertimeTickAccumulator = 0.f;

	const float Pct = GameData->OvertimeWeaknessBasePct
		* (1.f + GameData->OvertimeWeaknessGrowth * (OvertimeElapsed / GameData->OvertimeWeaknessTick));

	for (int32 TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		// 遍历【快照副本】：ApplyDamage 可能把英雄打死 -> 同步回调 HandleUnitDied -> Remove 原数组，
		// 直接 range-for 原数组会在 checked 构建触发 "Array has changed during ranged-for iteration"
		const TArray<ALKUnitBase*> HeroesSnapshot = AliveHeroes[TeamIdx];
		for (ALKUnitBase* Hero : HeroesSnapshot)
		{
			if (Hero && Hero->IsAlive())
			{
				LKGameplay::ApplyMaxHealthPercentDamage(Hero, Pct, nullptr);
			}
		}
	}

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 虚弱结算：%.2f%% 最大生命"), Pct * 100.f);
}

void ALKBattleGameMode::TickPlayerSilver(float DeltaSeconds)
{
	if (!bPlayerStateReady)
	{
		return;
	}

	if (ALKPlayerState* PS = Cast<ALKPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState))
	{
		PS->Silver->TickSilver(DeltaSeconds);
	}
}

void ALKBattleGameMode::SetPhase(ELKGamePhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}

	// 开战时玩家一个英雄都没部署 -> 自动补位，保证对局必然能分出胜负
	if (NewPhase == ELKGamePhase::Battle && Phase == ELKGamePhase::Deployment && HeroCounts[0] == 0)
	{
		AutoDeployDefaultHeroes(ELKTeam::Player);
	}

	Phase = NewPhase;

	// 部署阶段冻结单位（不能移动/攻击/放技能），开战才解冻
	ApplyCombatEnabledToAllUnits(NewPhase == ELKGamePhase::Battle);

	// 开战瞬间广播法术锁定状态（HUD 刷新手牌）
	if (NewPhase == ELKGamePhase::Battle)
	{
		OnSpellLockChanged.Broadcast(CanCastSpell(ELKTeam::Player));
	}

	if (ALKBattleGameState* GS = GetGameState<ALKBattleGameState>())
	{
		GS->SetPhase(NewPhase);
	}
}

void ALKBattleGameMode::ApplyCombatEnabledToAllUnits(bool bEnabled)
{
	for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
	{
		(*It)->SetCombatEnabled(bEnabled);
	}
}

void ALKBattleGameMode::AutoDeployDefaultHeroes(ELKTeam Team)
{
	const int32 TeamIdx = (int32)Team;
	const int32 ToDeploy = FMath::Min(AvailableHeroes.Num(), GameData->MaxHeroesPerTeam);
	const float Sign = (Team == ELKTeam::Player) ? -1.f : 1.f;

	for (int32 i = 0; i < ToDeploy; ++i)
	{
		// 左右分界在 Y 轴：玩家 Y<0（屏幕左），敌方 Y>0（屏幕右）
		const FVector Loc(
			FMath::FRandRange(-0.6f, 0.6f) * GameData->FieldHalfWidth,
			Sign * FMath::FRandRange(0.3f, 0.7f) * GameData->FieldHalfHeight,
			0.f);

		if (ALKUnitBase* Hero = SpawnUnitForTeam(AvailableHeroes[i], Team, Loc, ELKUnitClass::Hero))
		{
			DeployedHeroes[TeamIdx].Add(AvailableHeroes[i]);
		}
	}

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 自动部署默认英雄 x%d（%s）"), ToDeploy, TeamIdx == 0 ? TEXT("玩家") : TEXT("敌方"));
}

void ALKBattleGameMode::ForceStartBattle()
{
	if (Phase == ELKGamePhase::Deployment)
	{
		SetPhase(ELKGamePhase::Battle);
		UE_LOG(LogLKBattle, Log, TEXT("[Battle] 手动开战"));
	}
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

	Phase = ELKGamePhase::Result;

	// 对局结束，单位停止战斗
	ApplyCombatEnabledToAllUnits(false);

	if (ALKBattleGameState* GS = GetGameState<ALKBattleGameState>())
	{
		GS->EndMatch(Winner);
	}
}

void ALKBattleGameMode::HandleUnitDied(ALKUnitBase* Unit)
{
	if (!Unit)
	{
		return;
	}

	const int32 TeamIdx = (int32)Unit->GetTeam();

	if (Unit->IsHero())
	{
		AliveHeroes[TeamIdx].Remove(Unit);
		HeroCounts[TeamIdx] = FMath::Max(0, HeroCounts[TeamIdx] - 1);
		UE_LOG(LogLKBattle, Log, TEXT("[Battle] 英雄阵亡 %s，%s 剩余英雄 %d"),
			*Unit->GetUnitId().ToString(), TeamIdx == 0 ? TEXT("玩家") : TEXT("敌方"), HeroCounts[TeamIdx]);

		if (HeroCounts[TeamIdx] <= 0)
		{
			const ELKTeam Winner = (TeamIdx == 0) ? ELKTeam::Enemy : ELKTeam::Player;
			UE_LOG(LogLKBattle, Log, TEXT("[Battle] 一方英雄全灭，胜者: %d"), (int32)Winner);
			EndMatch(Winner);
		}

		// 法师英雄阵亡 -> 法术可能被锁定，通知 HUD 刷新
		if (Unit->IsMage())
		{
			OnSpellLockChanged.Broadcast(HasMage(ELKTeam::Player));
		}
	}

	if (Unit->IsBuilding())
	{
		if (int32* Count = BuildingCounts[TeamIdx].Find(Unit->GetUnitId()))
		{
			*Count = FMath::Max(0, *Count - 1);
		}
	}
}

ELKPlayResult ALKBattleGameMode::PlayCardForTeam(ELKTeam Team, int32 HandIndex, const FVector& Location)
{
	if (Phase != ELKGamePhase::Battle)
	{
		return ELKPlayResult::WrongPhase;
	}

	ULKDeckState* Deck = GetTeamDeck(Team);
	ULKSilverComponent* Silver = GetTeamSilver(Team);
	if (!Deck || !Silver)
	{
		return ELKPlayResult::Unknown;
	}

	const FName CardId = Deck->GetHandCard(HandIndex);
	if (CardId.IsNone())
	{
		return ELKPlayResult::HandEmpty;
	}

	ULKCardDefinition* Card = FindCard(CardId);
	if (!Card)
	{
		UE_LOG(LogLKBattle, Warning, TEXT("[Battle] 卡牌 %s 不在 CardLibrary 中"), *CardId.ToString());
		return ELKPlayResult::Unknown;
	}

	const int32 Cost = Card->Cost;
	if (Silver->GetSilver() < Cost)
	{
		return ELKPlayResult::NotEnoughSilver;
	}

	if (Card->CardType == ELKCardType::Spell)
	{
		if (!CanCastSpell(Team))
		{
			return ELKPlayResult::SpellLocked;
		}
		if (!IsInsideField(Location))
		{
			return ELKPlayResult::InvalidLocation;
		}
	}
	else
	{
		const int32 BuildingLimit = (Card->CardType == ELKCardType::Building && Card->BuildingTypeLimitOverride >= 0)
			? Card->BuildingTypeLimitOverride
			: GameData->BuildingTypeLimit;
		if (!IsPlacementValid(Location, Team, Card->CardType, Card->BuildingUnitId, BuildingLimit))
		{
			return ELKPlayResult::InvalidLocation;
		}
	}

	if (!Silver->TrySpend(Cost))
	{
		return ELKPlayResult::NotEnoughSilver;
	}

	Deck->PlayCard(HandIndex);
	ResolveCard(Card, Team, Location);
	return ELKPlayResult::Success;
}

ELKPlayResult ALKBattleGameMode::DeployHero(ELKTeam Team, FName HeroUnitId, const FVector& Location)
{
	if (Phase != ELKGamePhase::Deployment)
	{
		return ELKPlayResult::WrongPhase;
	}

	const int32 TeamIdx = (int32)Team;
	if (HeroCounts[TeamIdx] >= GameData->MaxHeroesPerTeam)
	{
		return ELKPlayResult::HeroLimitReached;
	}
	if (DeployedHeroes[TeamIdx].Contains(HeroUnitId))
	{
		return ELKPlayResult::AlreadyDeployed;
	}
	if (!IsPlacementValid(Location, Team, ELKCardType::Unit))
	{
		return ELKPlayResult::InvalidLocation;
	}

	ALKUnitBase* Hero = SpawnUnitForTeam(HeroUnitId, Team, Location, ELKUnitClass::Hero);
	if (!Hero)
	{
		return ELKPlayResult::Unknown;
	}

	DeployedHeroes[TeamIdx].Add(HeroUnitId);
	UE_LOG(LogLKBattle, Log, TEXT("[Battle] %s 部署英雄 %s @ %s"),
		TeamIdx == 0 ? TEXT("玩家") : TEXT("敌方"), *HeroUnitId.ToString(), *Location.ToString());
	return ELKPlayResult::Success;
}

ALKUnitBase* ALKBattleGameMode::SpawnUnitForTeam(FName UnitId, ELKTeam Team, const FVector& Location, ELKUnitClass FallbackClass)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FLKUnitRow* Row = GetUnitRow(UnitId);
	if (!Row)
	{
		// 自诊断：行找不到（DT_Units 无此行 / 行名与引用不一致）→ 使用默认值生成，并明确告警
		UE_LOG(LogLKUnit, Warning,
			TEXT("[Unit] 单位行 '%s' 未找到（检查 DT_Units 行名，或卡牌 SpawnUnitId/波次 UnitId 是否一致），已用默认值生成"),
			*UnitId.ToString());
	}

	FLKUnitRow Fallback;
	Fallback.UnitId = UnitId;
	Fallback.UnitClass = FallbackClass;

	const FLKUnitRow& Data = Row ? *Row : Fallback;

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
	if (!UnitTableCached)
	{
		return nullptr;
	}
	return UnitTableCached->FindRow<FLKUnitRow>(UnitId, TEXT(""), false);
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
	return Heroes[FMath::RandRange(0, Heroes.Num() - 1)];
}

bool ALKBattleGameMode::CanCastSpell(ELKTeam Team) const
{
	return !GameData->bRequireMageForSpells || HasMage(Team);
}

float ALKBattleGameMode::GetTeamHeroHealthRatio(ELKTeam Team) const
{
	const TArray<ALKUnitBase*>& Heroes = AliveHeroes[(int32)Team];
	if (Heroes.Num() == 0)
	{
		return 0.f;
	}

	float SumHealth = 0.f;
	float SumMaxHealth = 0.f;
	for (const ALKUnitBase* Hero : Heroes)
	{
		if (!Hero || Hero->IsDead())
		{
			continue;
		}
		SumHealth += Hero->GetHealth();
		SumMaxHealth += Hero->GetMaxHealth();
	}

	if (SumMaxHealth <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(SumHealth / SumMaxHealth, 0.f, 1.f);
}

bool ALKBattleGameMode::IsPlacementValid(const FVector& Location, ELKTeam Team, ELKCardType CardType,
	FName BuildingUnitId, int32 BuildingLimit) const
{
	if (!IsInsideField(Location))
	{
		return false;
	}

	// 玩家左半侧（Y<=0）/ 敌方右半侧（Y>=0）——配合 Pitch=-90 相机，屏幕左右即世界 Y
	if (Team == ELKTeam::Player ? Location.Y > 0.f : Location.Y < 0.f)
	{
		return false;
	}

	// 不与已有单位重叠
	UWorld* World = GetWorld();
	if (World)
	{
		TArray<FOverlapResult> Overlaps;
		const FCollisionShape Shape = FCollisionShape::MakeSphere(GameData->UnitBodyRadius * 0.8f);
		FCollisionQueryParams Params;
		if (World->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (Overlap.GetActor() && Overlap.GetActor()->IsA<ALKUnitBase>())
				{
					return false;
				}
			}
		}
	}

	// 同类建筑数量上限（种类无上限，见 GDD）
	if (CardType == ELKCardType::Building)
	{
		const int32 Limit = (BuildingLimit >= 0) ? BuildingLimit : GameData->BuildingTypeLimit;
		if (Limit >= 0 && !BuildingUnitId.IsNone())
		{
			const int32 Current = BuildingCounts[(int32)Team].FindRef(BuildingUnitId);
			if (Current >= Limit)
			{
				return false;
			}
		}
	}

	return true;
}

bool ALKBattleGameMode::IsInsideField(const FVector& Location) const
{
	return FMath::Abs(Location.X) <= GameData->FieldHalfWidth
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

void ALKBattleGameMode::ResolveCard(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location)
{
	if (!Card)
	{
		return;
	}

	switch (Card->CardType)
	{
	case ELKCardType::Unit:
		SpawnUnitForTeam(Card->SpawnUnitId, Team, Location);
		break;

	case ELKCardType::Building:
		SpawnUnitForTeam(Card->BuildingUnitId, Team, Location, ELKUnitClass::Building);
		break;

	case ELKCardType::Spell:
		CastSpell(Card, Team, Location);
		break;
	}
}

void ALKBattleGameMode::CastSpell(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || !Card || Card->SpellEffect == ELKSpellEffect::None)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Card->SpellRadius);
	FCollisionQueryParams Params;
	World->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_Pawn, Shape, Params);

	int32 HitCount = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ALKUnitBase* Unit = Cast<ALKUnitBase>(Overlap.GetActor());
		if (!Unit || Unit->IsDead())
		{
			continue;
		}

		if (Card->SpellEffect == ELKSpellEffect::Damage && Unit->GetTeam() != Team)
		{
			LKGameplay::ApplyDamage(Unit, Card->SpellValue, nullptr);
			++HitCount;
		}
		else if (Card->SpellEffect == ELKSpellEffect::Heal && Unit->GetTeam() == Team)
		{
			LKGameplay::ApplyHeal(Unit, Card->SpellValue, nullptr);
			++HitCount;
		}
	}

	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 法术 %s @ %s，命中 %d 个单位"),
		*Card->CardId.ToString(), *Location.ToString(), HitCount);
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

	// 中线（玩家左半 / 敌方右半）：沿 X 方向、位于 Y=0 —— 屏幕上呈现为左右分界竖线
	DrawDebugLine(World, FVector(-GameData->FieldHalfWidth, 0.f, 0.f), FVector(GameData->FieldHalfWidth, 0.f, 0.f),
		FColor::Yellow, false, -1.f, 0, 1.f);
}
