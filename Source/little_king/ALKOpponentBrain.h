#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LKTypes.h"
#include "ALKOpponentBrain.generated.h"

class ULKSilverComponent;
class ULKDeckState;
class ULKGameData;
class ULKCardDefinition;
class ALKBattleGameMode;

/**
 * 敌方 AI：脚本波次（DT_Waves）+ 简单打牌规则（银币够就按最便宜的打）。
 * S4 起升级：集火英雄指令、反制策略。
 */
UCLASS()
class ALKOpponentBrain : public AActor
{
	GENERATED_BODY()

public:
	ALKOpponentBrain();

	void InitBrain(ULKGameData* InGameData, ALKBattleGameMode* InGameMode);

	/** 由 GameMode 每帧驱动 */
	void TickBrain(float DeltaTime, float BattleElapsed);

	ULKSilverComponent* GetSilver() const { return Silver; }
	ULKDeckState* GetDeck() const { return Deck; }

private:
	UPROPERTY()
	TObjectPtr<ULKSilverComponent> Silver;

	UPROPERTY()
	TObjectPtr<ULKDeckState> Deck;

	UPROPERTY()
	TObjectPtr<ULKGameData> GameData;

	TWeakObjectPtr<ALKBattleGameMode> GameMode;

	TArray<struct FLKWaveEntry> Waves;
	int32 WaveIndex = 0;

	float ThinkTimer = 0.f;
	float ThinkInterval = 1.f;

	void LoadWaves();
	void ProcessWaves(float BattleElapsed);
	void ThinkAndPlay();
	bool PickTargetLocation(FName CardId, FVector& OutLocation) const;
};
