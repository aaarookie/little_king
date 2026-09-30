#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LKDataTypes.h"
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
 * 索敌优先级（单位侧）：嘲讽者 > ForcedTarget > 最近敌人。
 */
UCLASS()
class ALKOpponentBrain : public AActor
{
	GENERATED_BODY()

public:
	ALKOpponentBrain();

	void InitBrain(ULKGameData* InGameData, ALKBattleGameMode* InGameMode, bool bInCanPlayCards = true);

	/** 由 GameMode 每帧驱动 */
	void TickBrain(float DeltaTime, float BattleElapsed);
	void SetScriptedWaves(const TArray<struct FLKWaveEntry>& InWaves);
	/** 应用已校验的 D2 遭遇快照，并重建该房的敌方经济、牌组、波次和战术计时。 */
	bool ConfigureEncounter(const FLKEncounterRow& Encounter);
	bool UsesCards() const { return bCanPlayCards; }
	bool IsFocusEnabled() const;
	const TArray<struct FLKWaveEntry>& GetWaves() const { return Waves; }
	/** 结束战斗/换房：清空波次队列、重试队列与战术法术计时。 */
	void ResetBattleState();
	/** 统计：因满员/落点失败超过延后窗口而丢弃的增援数；已成功施放的战术法术数。 */
	int32 GetDroppedReinforcements() const { return DroppedReinforcements; }
	int32 GetTacticalSpellCasts() const { return TacticalSpellCasts; }
	const FLKEnemySpellSettings& GetSpellSettings() const { return SpellSettings; }

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
	bool bCanPlayCards = true;
	FLKEncounterAISettings AISettings;

	/** 延迟重试的增援（满员/落点失败）：最多延后 3 秒，过期丢弃并计数，不累计到腾空后爆发。 */
	static constexpr float MaxReinforcementDelay = 3.f;
	struct FPendingReinforcement
	{
		FName UnitId;
		int32 Remaining = 0;
		float Deadline = 0.f;
	};
	TArray<FPendingReinforcement> PendingReinforcements;
	int32 DroppedReinforcements = 0;

	// ---------- 敌方战术法术（独立通道，不用五张假手牌） ----------
	FLKEnemySpellSettings SpellSettings;
	float SpellCheckTimer = 0.f;
	float SpellCooldownRemaining = 0.f;
	float SpellFirstCastAt = 12.f;
	int32 TacticalSpellCasts = 0;

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
	/** 把一条到期波次排入重试队列（首次尝试失败时）。 */
	void QueueReinforcement(FName UnitId, int32 Count, float BattleElapsed);
	void ThinkAndPlay();
	/** 独立敌方战术施法通道：与有限波次并行，共用敌方银币组件（波次不扣费）。 */
	void TickTacticalSpell(float DeltaTime, float BattleElapsed);
	bool TryCastTacticalSpell(float BattleElapsed);
	/** 找一个能覆盖优先数量玩家战斗单位的落点；必要时允许只覆盖单英雄。 */
	bool FindTacticalSpellLocation(float Radius, int32 PreferredTargets, FVector& OutLocation) const;

	// S4：集火 / 反制 / 爆发 / 单张出牌
	void DoFocus();
	void RefreshCounter();
	bool TryEnterPush();
	bool TryPlayOneCard();
	int32 GetCounterBonus(const ULKCardDefinition* Def) const;

	bool PickTargetLocation(FName CardId, FVector& OutLocation) const;
};
