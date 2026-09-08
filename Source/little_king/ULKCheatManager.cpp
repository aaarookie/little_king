#include "ULKCheatManager.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"

#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "LKLog.h"
#include "LKGameplayHelpers.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"
#include "ULKRunSubsystem.h"

ALKBattleGameMode* ULKCheatManager::GetGameMode() const
{
	UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
}

void ULKCheatManager::UndeadEncounter(const FString& Preset)
{
    ALKBattleGameMode* GM = GetGameMode();
    UE_LOG(LogLK, Log, TEXT("[Cheat] UndeadEncounter %s: %s"), *Preset,
        GM && GM->ConfigureEnemyEncounter(FName(*Preset)) ? TEXT("已切换") : TEXT("仅部署阶段可用，参数 Patrol / Elite / Boss"));
}

void ULKCheatManager::RunHeroMaxHealth(const FString& HeroId, float NewBaseMaxHealth)
{
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	ULKRunSubsystem* Run = Instance ? Instance->GetSubsystem<ULKRunSubsystem>() : nullptr;
	const bool bChanged = Run && Run->SetHeroBaseMaxHealth(FName(*HeroId), NewBaseMaxHealth);
	if (bChanged) { UE_LOG(LogLK, Log, TEXT("[Cheat] RunHeroMaxHealth %s %.1f：已写入下一房永久状态"), *HeroId, NewBaseMaxHealth); }
	else { UE_LOG(LogLK, Warning, TEXT("[Cheat] RunHeroMaxHealth 失败：仅能在前两房胜利结算后使用，且英雄/数值必须有效")); }
}

void ULKCheatManager::RunHeroTrait(const FString& HeroId, const FString& TraitId, int32 Enabled)
{
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	ULKRunSubsystem* Run = Instance ? Instance->GetSubsystem<ULKRunSubsystem>() : nullptr;
	const bool bChanged = Run && (Enabled != 0
		? Run->AddHeroTrait(FName(*HeroId), FName(*TraitId))
		: Run->RemoveHeroTrait(FName(*HeroId), FName(*TraitId)));
	if (bChanged) { UE_LOG(LogLK, Log, TEXT("[Cheat] RunHeroTrait %s %s=%d：已写入下一房永久状态"), *HeroId, *TraitId, Enabled); }
	else { UE_LOG(LogLK, Warning, TEXT("[Cheat] RunHeroTrait 失败：阶段、英雄、特性或重复状态不合法")); }
}

void ULKCheatManager::ListRunState()
{
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const ULKRunSubsystem* Run = Instance ? Instance->GetSubsystem<ULKRunSubsystem>() : nullptr;
	if (!Run || !Run->HasRun()) { UE_LOG(LogLK, Warning, TEXT("[Cheat] 当前没有远征")); return; }
	const FLKRunState State = Run->GetRunState();
	const FLKEncounterRow* Encounter = State.Encounters.FindByPredicate(
		[&State](const FLKEncounterRow& Row) { return Row.EncounterId == State.PendingBattle.EncounterId; });
	UE_LOG(LogLK, Log, TEXT("[Cheat] Run=%s Phase=%d Node=%s Encounter=%s RewardTier=%d History=%d"),
		*State.RunId.ToString(), int32(State.Phase), *State.CurrentNodeId.ToString(), *State.PendingBattle.EncounterId.ToString(),
		Encounter ? Encounter->RewardTier : 0, State.BattleHistory.Num());
	for (const FLKRunHeroState& Hero : State.Heroes)
	{
		FString Traits;
		for (FName Trait : Hero.Traits) { if (!Traits.IsEmpty()) { Traits += TEXT(","); } Traits += Trait.ToString(); }
		UE_LOG(LogLK, Log, TEXT("[Cheat] %s HP=%.1f/%.1f BaseMax=%.1f Traits=[%s]"),
			*Hero.HeroId.ToString(), Hero.Health, Hero.MaxHealth, Hero.BaseMaxHealth, *Traits);
	}
}

void ULKCheatManager::AddSilver(float Amount)
{
	ALKBattleGameMode* GM = GetGameMode();
	if (!GM)
	{
		return;
	}

	if (ULKSilverComponent* Silver = GM->GetTeamSilver(ELKTeam::Player))
	{
		Silver->AddSilver(Amount);
		UE_LOG(LogLK, Log, TEXT("[Cheat] AddSilver %.1f -> 当前 %.1f"), Amount, Silver->GetSilver());
	}
}

void ULKCheatManager::DrawCard()
{
	ALKBattleGameMode* GM = GetGameMode();
	if (!GM)
	{
		return;
	}

	if (ULKDeckState* Deck = GM->GetTeamDeck(ELKTeam::Player))
	{
		Deck->DrawCard();
		UE_LOG(LogLK, Log, TEXT("[Cheat] DrawCard -> 手牌 %d 张"), Deck->GetHandSize());
	}
}

void ULKCheatManager::SpawnUnit(const FString& UnitId, int32 TeamIdx, float X, float Y)
{
	ALKBattleGameMode* GM = GetGameMode();
	if (!GM)
	{
		return;
	}

	const ELKTeam Team = (TeamIdx == 0) ? ELKTeam::Player : ELKTeam::Enemy;

	// 半场约定：玩家 Y<=0（左），敌方 Y>=0（右）。记不清正负时随便填——
	// 坐标落在对方半场会自动镜像到本阵营半场并提示。
	float SpawnY = Y;
	if (Team == ELKTeam::Player && SpawnY > 0.f)
	{
		SpawnY = -SpawnY;
		UE_LOG(LogLK, Warning, TEXT("[Cheat] 玩家阵营坐标 Y=%.1f 落在敌方半场，已镜像为 %.1f"), Y, SpawnY);
	}
	else if (Team == ELKTeam::Enemy && SpawnY < 0.f)
	{
		SpawnY = -SpawnY;
		UE_LOG(LogLK, Warning, TEXT("[Cheat] 敌方阵营坐标 Y=%.1f 落在玩家半场，已镜像为 %.1f"), Y, SpawnY);
	}

	GM->SpawnUnitForTeam(FName(*UnitId), Team, FVector(X, SpawnY, 0.f));
}

void ULKCheatManager::KillAll(int32 TeamIdx)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Killed = 0;
	TArray<ALKUnitBase*> Victims;
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		ALKUnitBase* Unit = *It;
		if ((int32)Unit->GetTeam() == TeamIdx && Unit->IsTargetable())
		{
			Victims.Add(Unit);
		}
	}
	ALKBattleGameMode* GM = GetGameMode();
	if (GM) { GM->BeginCombatBatch(); }
	for (ALKUnitBase* Unit : Victims) { Unit->Die(); ++Killed; }
	if (GM) { GM->EndCombatBatch(); }
	UE_LOG(LogLK, Log, TEXT("[Cheat] KillAll 阵营%d：处决 %d 个单位"), TeamIdx, Killed);
}

void ULKCheatManager::WinMatch(int32 TeamIdx)
{
	ALKBattleGameMode* GM = GetGameMode();
	if (!GM)
	{
		return;
	}

	const ELKTeam Winner = (TeamIdx == 0) ? ELKTeam::Player : ELKTeam::Enemy;
	GM->ForceEndMatch(Winner);
	UE_LOG(LogLK, Log, TEXT("[Cheat] WinMatch -> 胜者 %d"), TeamIdx);
}

void ULKCheatManager::StartBattle()
{
	ALKBattleGameMode* GM = GetGameMode();
	if (!GM)
	{
		return;
	}

	GM->ForceStartBattle();
	UE_LOG(LogLK, Log, TEXT("[Cheat] StartBattle: %s"), GM->GetPhase() == ELKGamePhase::Battle ? TEXT("已开战") : TEXT("尚未全部部署"));
}

void ULKCheatManager::ListUnits()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogLK, Log, TEXT("[Cheat] === 场上单位列表 ==="));
	int32 Count = 0;
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		const ALKUnitBase* Unit = *It;
		UE_LOG(LogLK, Log, TEXT("[Cheat] %s | 阵营%d | %s%s%s | 生命 %.0f/%.0f | 状态%d | @ %s"),
			*Unit->GetUnitId().ToString(), (int32)Unit->GetTeam(),
			Unit->IsHero() ? TEXT("英雄 ") : TEXT(""),
			Unit->IsTaunting() ? TEXT("嘲讽中 ") : TEXT(""),
			Unit->IsInvulnerable() ? TEXT("无敌中 ") : TEXT(""),
			Unit->GetHealth(), Unit->GetMaxHealth(), (int32)Unit->GetState(),
			*Unit->GetActorLocation().ToString());
		++Count;
	}
	UE_LOG(LogLK, Log, TEXT("[Cheat] === 共 %d 个单位 ==="), Count);
}

void ULKCheatManager::InvulnerableHeroes(float Seconds)
{
	if (Seconds <= 0.f)
	{
		Seconds = 10.f; // 省略参数 = 默认 10 秒
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Affected = 0;
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		ALKUnitBase* Unit = *It;
		if (Unit->GetTeam() == ELKTeam::Player && Unit->IsHero() && Unit->IsAlive())
		{
			Unit->SetInvulnerable(Seconds);
			++Affected;
		}
	}
	UE_LOG(LogLK, Log, TEXT("[Cheat] InvulnerableHeroes %.1f 秒：己方 %d 个在场英雄进入无敌（虚弱仍会扣血）"),
		Seconds, Affected);
}

void ULKCheatManager::ProjectilePool()
{
	ALKBattleGameMode* GM = GetGameMode();
	if (!GM)
	{
		return;
	}

	int32 Total = 0;
	int32 Active = 0;
	GM->GetProjectilePoolStats(Total, Active);
	UE_LOG(LogLK, Log, TEXT("[Cheat] 弹道池：总量 %d，在飞 %d（对局结束应归 0；3 分钟无泄漏 = 池工作正常）"),
		Total, Active);
}

void ULKCheatManager::HeroTrait(const FString& HeroId, const FString& TraitId, int32 Enabled)
{
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if (It->GetTeam() == ELKTeam::Player && It->IsHero() && It->IsAlive() && It->GetUnitId() == FName(*HeroId))
        {
            const bool Changed = Enabled != 0 ? It->AddTrait(FName(*TraitId)) : It->RemoveTrait(FName(*TraitId));
            UE_LOG(LogLK, Log, TEXT("[Cheat] HeroTrait %s %s = %d, changed=%d"), *HeroId, *TraitId, Enabled, int32(Changed));
            return;
        }
    }
    UE_LOG(LogLK, Warning, TEXT("[Cheat] 未找到存活的己方英雄 %s"), *HeroId);
}

void ULKCheatManager::DamageUnit(const FString& UnitId, int32 TeamIdx, float Amount)
{
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if (int32(It->GetTeam()) == TeamIdx && It->IsTargetable() && It->GetUnitId() == FName(*UnitId))
        {
            const float Actual = LKGameplay::ApplyDamage(*It, Amount, nullptr, true);
            UE_LOG(LogLK, Log, TEXT("[Cheat] DamageUnit %s actual=%.1f"), *UnitId, Actual);
            return;
        }
    }
    UE_LOG(LogLK, Warning, TEXT("[Cheat] 未找到目标单位 %s 阵营%d"), *UnitId, TeamIdx);
}
