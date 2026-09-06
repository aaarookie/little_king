#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LKTypes.h"
#include "LKDataTypes.h"
#include "ULKCardDefinition.h"
#include "ALKBattleGameMode.generated.h"

class ULKGameData;
class ULKDeckState;
class ULKSilverComponent;
class ULKBattleHUDWidget;
class ALKBattleGameState;
class ALKOpponentBrain;
class ALKUnitBase;
class ALKProjectile;
class ALKHeroCamp;
class ALKUnitHero;
class USoundBase;
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
	friend struct FLKSprint5TestAccess;

public:
	ALKBattleGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------- 流程 ----------
	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	void ForceStartBattle();
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool CanStartBattle() const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool HasValidDecks() const;
	/** 放置预览的基础射程；非攻击建筑返回 0，临时战斗增益不计入。 */
	UFUNCTION(BlueprintPure, Category = "LK|Battle") float GetBuildingPlacementAttackRange(FName CardId) const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") int32 GetRequiredHeroCount() const { return AvailableHeroes.Num(); }
	UFUNCTION(BlueprintPure, Category = "LK|Battle") int32 GetDeployedPlayerHeroCount() const { return DeployedHeroes[0].Num(); }
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool IsHeroDeployed(FName HeroId) const { return DeployedHeroes[0].Contains(HeroId); }
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool CanPlaceSpellAt(ELKTeam Team, FVector Location) const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool HasGlobalSpellPlacement(ELKTeam Team) const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") FLKMatchStats GetMatchStats() const { return MatchStats; }
	ELKPlayResult ValidateCardPlay(ELKTeam Team, int32 HandIndex, const FVector& Location) const;
	void BeginCombatBatch() { ++CombatBatchDepth; }
	void EndCombatBatch();
	void RecordCombatEvent(const FLKCombatEvent& Event);
	void NotifyFireballCast(const FVector& Location);
	FRandomStream& GetBattleRandom() const { return BattleRandom; }
	USoundBase* FindPreloadedSound(FName Id) const;
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLKCombatEvent, const FLKCombatEvent&, Event);
	UPROPERTY(BlueprintAssignable, Category = "LK|Battle") FOnLKCombatEvent OnCombatEvent;

	/** 强制结束对局（调试/剧情用） */
	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	void ForceEndMatch(ELKTeam Winner);

	/** 旧蓝图兼容事件，新规则不再广播；法术合法性由落点和英雄特性决定 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpellLockChanged, bool, bUnlocked);
	UPROPERTY(BlueprintAssignable, Category = "LK|Battle")
	FOnSpellLockChanged OnSpellLockChanged;

	// ---------- S5：伤害/治疗事件（飘字数据链） ----------
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLKDamageEvent, FVector, Location, float, Amount, bool, bIsHeal);
	UPROPERTY(BlueprintAssignable, Category = "LK|Battle")
	FOnLKDamageEvent OnDamageEvent;

	/** 播放一次性音效（SoundMap 查表；未配置静默跳过） */
	void PlayLKOneShot(FName SoundId, const FVector& Location, float VolumeScale = 1.f);

	// ---------- S5：弹道对象池 ----------
	/** 从池中取一条弹道并激活（池空则新建）；发射点/伤害/阵营/施法者/方向 */
	ALKProjectile* AcquireProjectile(const FVector& Location, float Damage, ELKTeam Team, AActor* InInstigator, const FVector& Direction);
	/** 弹道命中/超时后回收（隐藏停用，供复用） */
	void ReleaseProjectile(ALKProjectile* Projectile);
	/** 对局结束统一回收全部弹道（防池泄漏） */
	void ReleaseAllProjectiles();
	void GetProjectilePoolStats(int32& OutTotal, int32& OutActive) const;

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ELKGamePhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	ULKGameData* GetGameData() const { return GameData; }

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	float GetBattleElapsed() const { return BattleElapsed; }

	// ---------- 出牌 / 部署（玩家与敌方 AI 共用入口） ----------
	ELKPlayResult PlayCardForTeam(ELKTeam Team, int32 HandIndex, const FVector& Location);
	ELKPlayResult DeployHero(ELKTeam Team, FName HeroUnitId, const FVector& Location);
	ELKPlayResult ValidateHeroDeployment(ELKTeam Team, FName HeroUnitId, const FVector& Location, FVector* OutHeroPosition = nullptr) const;

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

	/** 弃用兼容入口：固定返回 0，新版使用单位头顶血条 */
	UFUNCTION(BlueprintPure, Category = "LK|Battle", meta = (DeprecatedFunction, DeprecationMessage = "团队血条已移除，请删除旧 HUD 绑定；新版由原生 HUD 绘制单位头顶血条"))
	float GetTeamHeroHealthRatio(ELKTeam Team) const;

	// ---------- S4：AI 战术辅助 ----------
	/** 阵营存活单位总数（英雄+佣兵+建筑；上限判定/爆发判定用） */
	int32 CountAliveUnits(ELKTeam Team) const;

	/** 阵营存活"战斗单位"中某攻击类型数量（排除建筑；AI 反制评估用） */
	int32 CountCombatUnitsOfAttackType(ELKTeam Team, ELKAttackType Type) const;

	/** 血量比例最低的存活英雄（AI 集火目标）；无英雄返回空 */
	ALKUnitBase* GetWeakestAliveHero(ELKTeam Team) const;

	/** 让某阵营所有存活单位（建筑除外）强制集火指定目标 Duration 秒（低于有效嘲讽） */
	void ForcedTargetAllUnits(ELKTeam Team, AActor* Target, float Duration);

	/**
	 * 法术智能目标：遍历敌方阵营每个存活单位，统计其半径内敌人数，
	 * 取"聚集度最高"的点作为落点（AI 火球不再乱扔）。找到返回 true 并输出落点。
	 */
	bool FindBestSpellTarget(ELKTeam CasterTeam, float Radius, FVector& OutLocation, ELKSpellEffect Effect = ELKSpellEffect::Damage) const;

	bool IsPlacementValid(const FVector& Location, ELKTeam Team, ELKCardType CardType,
		FName BuildingUnitId = NAME_None, int32 BuildingLimit = -2) const;
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
	mutable FRandomStream BattleRandom;
	FLKMatchStats MatchStats;
	int32 CombatBatchDepth = 0;
	bool bPendingVictoryCheck = false;
	bool bResolvingCard = false;
	ELKTeam SeedTieBreaker = ELKTeam::Player;
	TMap<FName, FLKUnitRow> FallbackUnitRows;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> PreloadedSounds;
	void CheckVictoryAfterBatch();

	int32 HeroCounts[2] = { 0, 0 };
	TArray<ALKUnitBase*> AliveHeroes[2];
	TMap<FName, int32> BuildingCounts[2];
	TArray<FName> DeployedHeroes[2];

	// S5 弹道对象池（BeginPlay 预生成 32 条，命中/超时回收复用）
	UPROPERTY(Transient)
	TArray<TObjectPtr<ALKProjectile>> ProjectilePool;

	/** S5 屏幕震动（幅度×DA_GameData CameraShakeScale；0 关闭） */
	void TriggerCameraShake(float BaseIntensity);

	void EnsureGameData();
	void InitProjectilePool();
	void TryInitPlayerState();
	void SetPhase(ELKGamePhase NewPhase);
	/** 统一开关所有单位的战斗状态（部署阶段冻结，开战/结算时更新） */
	void ApplyCombatEnabledToAllUnits(bool bEnabled);
	/** 仅自动布置敌方全部英雄与营地；玩家必须手动完成 */
	void AutoDeployDefaultHeroes(ELKTeam Team);
	void TickDeployment(float DeltaSeconds);
	void TickBattle(float DeltaSeconds);
	void TickOvertime(float DeltaSeconds);
	void TickPlayerSilver(float DeltaSeconds);
	void EndMatch(ELKTeam Winner);
	UFUNCTION()
	void HandleUnitDied(ALKUnitBase* Unit);
	bool ResolveCard(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location);
	bool CastSpell(ULKCardDefinition* Card, ELKTeam Team, const FVector& Location);
	void DrawFieldBounds() const;
};
