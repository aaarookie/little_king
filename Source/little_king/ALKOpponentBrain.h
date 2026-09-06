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
class ALKUnitBase;

/**
 * 敌方 AI：脚本波次（DT_Waves）+ 打牌脑。
 * S4 战术升级：
 *  ① 集火：周期（AIFocusIntervalMin~Max）对玩家血量最低英雄发起 AIFocusDuration 秒集火；
 *  ② 反制：每 AICounterCheckInterval 评估玩家近战/远程构成，出牌给"克制卡"加分；
 *  ③ 爆发：银币达标 + 兵力不劣时，一波连打 AIPushMaxCards 张；
 *  ④ 法术：用 GameMode 的聚集度评估找最优落点，不再乱扔。
 * 索敌优先级（单位侧）：ForcedTarget > 嘲讽者 > 最近敌人。
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

	// ---------- S4 战术状态 ----------
	float FocusTimer = 20.f;			// 距下次集火的倒计时（InitBrain 里按配置随机初值）
	float CounterTimer = 0.f;			// 距下次反制评估的倒计时
	float PushCooldownTimer = 0.f;		// 爆发冷却（归零才可再次爆发）
	int32 PushRemainingCards = 0;		// 爆发窗口中剩余连打张数（>0 时 ThinkAndPlay 连打）
	int32 PlayerRangedCount = 0;		// 最近一次评估：玩家远程战斗单位数
	int32 PlayerMeleeCount = 0;			// 最近一次评估：玩家近战战斗单位数
	TWeakObjectPtr<ALKUnitBase> PendingFocusTarget;
	float FocusWarningTimer = 0.f;
	bool bReserving = false;
	float ReserveRemaining = 0.f;

	void LoadWaves();
	void ProcessWaves(float BattleElapsed);
	void ThinkAndPlay();

	// S4：集火 / 反制 / 爆发 / 单张出牌
	void DoFocus();
	void RefreshCounter();
	bool TryEnterPush();
	bool TryPlayOneCard();
	int32 GetCounterBonus(const ULKCardDefinition* Def) const;

	bool PickTargetLocation(FName CardId, FVector& OutLocation) const;
};
