#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LKTypes.h"
#include "ALKBattleGameState.generated.h"

/**
 * 对局状态：阶段 / 胜者 / 超时标记，负责向 UI 广播。
 */
UCLASS()
class ALKBattleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, ELKGamePhase, NewPhase);
	UPROPERTY(BlueprintAssignable, Category = "LK|Battle")
	FOnPhaseChanged OnPhaseChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchEnded, ELKTeam, Winner);
	UPROPERTY(BlueprintAssignable, Category = "LK|Battle")
	FOnMatchEnded OnMatchEnded;

	UPROPERTY(BlueprintReadOnly, Category = "LK|Battle")
	ELKGamePhase Phase = ELKGamePhase::Deployment;

	UPROPERTY(BlueprintReadOnly, Category = "LK|Battle")
	ELKTeam Winner = ELKTeam::Player;

	/** 是否已进入超时（虚弱）阶段 */
	UPROPERTY(BlueprintReadOnly, Category = "LK|Battle")
	bool bBattleOvertime = false;

	void SetPhase(ELKGamePhase NewPhase);
	void EndMatch(ELKTeam InWinner);
};
