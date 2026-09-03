#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LKTypes.h"
#include "ULKCardDefinition.h"
#include "ALKBattleGameMode.generated.h"

class ULKGameData;
class ULKDeckState;
class ULKSilverComponent;
class ULKBattleHUDWidget;
class ALKBattleGameState;
class ALKOpponentBrain;
class ALKUnitBase;
class UDataTable;
struct FLKUnitRow;

/**
 * 对局流程权威（单机 PvE，GameMode 即权威）：
 * 阶段机（部署 -> 战斗 -> 结算）、银币结算、胜负判定、超时虚弱、单位生成与放置校验。
 */
UCLASS()
class ALKBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALKBattleGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------- 流程 ----------
	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	void ForceStartBattle();

	/** 强制结束对局（调试/剧情用） */
	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	void ForceEndMatch(ELKTeam Winner);

	/** 法术锁定状态变化（开战/法师英雄阵亡时广播，HUD 据此刷新手牌锁定态） */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpellLockChanged, bool, bUnlocked);
	UPROPERTY(BlueprintAssignable, Category = "LK|Battle")
	FOnSpellLockChanged OnSpellLockChanged;

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ELKGamePhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ULKGameData* GetGameData() const { return GameData; }

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	float GetBattleElapsed() const { return BattleElapsed; }

	// ---------- 出牌 / 部署（玩家与敌方 AI 共用入口） ----------
	ELKPlayResult PlayCardForTeam(ELKTeam Team, int32 HandIndex, const FVector& Location);
	ELKPlayResult DeployHero(ELKTeam Team, FName HeroUnitId, const FVector& Location);

	// ---------- 单位 ----------
	ALKUnitBase* SpawnUnitForTeam(FName UnitId, ELKTeam Team, const FVector& Location, ELKUnitClass FallbackClass = ELKUnitClass::Soldier);
	const FLKUnitRow* GetUnitRow(FName UnitId) const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ULKCardDefinition* FindCard(FName CardId) const;
	int32 GetCardCost(FName CardId) const;
	bool HasMage(ELKTeam Team) const;
	int32 GetHeroCount(ELKTeam Team) const;
	ALKUnitBase* GetRandomAliveHero(ELKTeam Team) const;
	bool CanCastSpell(ELKTeam Team) const;

	/** 顶部英雄血条：存活英雄当前血量合计 / 满血合计（0~1）；无存活英雄返回 0 */
	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	float GetTeamHeroHealthRatio(ELKTeam Team) const;
	bool IsPlacementValid(const FVector& Location, ELKTeam Team, ELKCardType CardType,
		FName BuildingUnitId = NAME_None, int32 BuildingLimit = -1) const;
	bool IsInsideField(const FVector& Location) const;
	ULKSilverComponent* GetTeamSilver(ELKTeam Team) const;
	ULKDeckState* GetTeamDeck(ELKTeam Team) const;

	/** 部署界面可选英雄（S3 UI 使用） */
	UPROPERTY(BlueprintReadOnly, Category = "LK|Config")
	TArray<FName> AvailableHeroes;

	/** 战斗 HUD（在 BP_ALKBattleGameMode 类默认值里指定 WBP_BattleHUD） */
	UPROPERTY(EditDefaultsOnly, Category = "LK|UI")
	TSubclassOf<ULKBattleHUDWidget> HUDWidgetClass;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "LK|Config")
	TObjectPtr<ULKGameData> GameData;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> UnitTableCached;

	UPROPERTY()
	TObjectPtr<ALKOpponentBrain> OpponentBrain;

	ELKGamePhase Phase = ELKGamePhase::Deployment;
	float PhaseElapsed = 0.f;
	float BattleElapsed = 0.f;
	float OvertimeElapsed = 0.f;
	float OvertimeTickAccumulator = 0.f;
	bool bPlayerStateReady = false;
	bool bOvertimeActive = false;

	int32 HeroCounts[2] = { 0, 0 };
	TArray<ALKUnitBase*> AliveHeroes[2];
	TMap<FName, int32> BuildingCounts[2];
	TArray<FName> DeployedHeroes[2];

	void EnsureGameData();
	void TryInitPlayerState();
	void SetPhase(ELKGamePhase NewPhase);
	/** 统一开关所有单位的战斗状态（部署阶段冻结，开战/结算时更新） */
	void ApplyCombatEnabledToAllUnits(bool bEnabled);
	/** 未部署任何英雄的一方自动补位默认英雄（防软锁；敌方开战即补位） */
	void AutoDeployDefaultHeroes(ELKTeam Team);
	void TickDeployment(float DeltaSeconds);
	void TickBattle(float DeltaSeconds);
	void TickOvertime(float DeltaSeconds);
	void TickPlayerSilver(float DeltaSeconds);
	void EndMatch(ELKTeam Winner);
	UFUNCTION()
	void HandleUnitDied(ALKUnitBase* Unit);
	void ResolveCard(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location);
	void CastSpell(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location);
	void DrawFieldBounds() const;
};
