#include "ULKCheatManager.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "LKLog.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"

ALKBattleGameMode* ULKCheatManager::GetGameMode() const
{
	UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
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
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		ALKUnitBase* Unit = *It;
		if ((int32)Unit->GetTeam() == TeamIdx && Unit->IsAlive())
		{
			Unit->Die();
			++Killed;
		}
	}
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
	UE_LOG(LogLK, Log, TEXT("[Cheat] StartBattle"));
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
