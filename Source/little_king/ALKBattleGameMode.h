#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LKTypes.h"
#include "LKDataTypes.h"
#include "LKRunTypes.h"
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
class ULKRunSubsystem;
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
	friend struct FLKD0TestAccess;
	friend struct FLKD1TestAccess;
	friend struct FLKD2TestAccess;

public:
	ALKBattleGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------- 流程 ----------
	UFUNCTION(BlueprintCallable, Category = "LK|Battle")
	void ForceStartBattle();
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool CanStartBattle() const;
	/** 无法开战时逐条打印判据（部署/牌库/阶段/名单），供 PIE 排障 */
	void LogStartBattleBlockers() const;
	/** 是否至少有一个场上单位处于可战斗状态（HUD 兜底提示用） */
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool IsAnyUnitCombatEnabled() const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool HasValidDecks() const;
	/** 放置预览的基础射程；非攻击建筑返回 0，临时战斗增益不计入。 */
	UFUNCTION(BlueprintPure, Category = "LK|Battle") float GetBuildingPlacementAttackRange(FName CardId) const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") int32 GetRequiredHeroCount() const { return AvailableHeroes.Num(); }
	UFUNCTION(BlueprintPure, Category = "LK|Battle") int32 GetDeployedPlayerHeroCount() const { return DeployedHeroes[0].Num(); }
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool IsHeroDeployed(FName HeroId) const { return DeployedHeroes[0].Contains(HeroId); }
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool CanPlaceSpellAt(ELKTeam Team, FVector Location) const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") bool HasGlobalSpellPlacement(ELKTeam Team) const;
	UFUNCTION(BlueprintPure, Category = "LK|Battle") FLKMatchStats GetMatchStats() const { return MatchStats; }
	UFUNCTION(BlueprintPure, Category = "LK|Battle") FLKBattleOutcome GetBattleOutcome() const { return BattleOutcome; }
	UFUNCTION(BlueprintPure, Category = "LK|Run") bool IsExpeditionBattle() const { return bExpeditionBattle; }
	UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetExpeditionRoomIndex() const;
	UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetExpeditionRoomCount() const;
	UFUNCTION(BlueprintPure, Category = "LK|Run") bool IsResultActionNext() const;
	UFUNCTION(BlueprintPure, Category = "LK|Run") FText GetResultActionLabel() const;
	/** 结算按钮入口：推进/重开状态后重载当前战斗地图。 */
	UFUNCTION(BlueprintCallable, Category = "LK|Run") bool RequestResultAction();
	UFUNCTION(BlueprintPure, Category = "LK|Home") FName GetHomeMapName() const;
	/** 保存奖励/路线安全点或终态，然后返回家园；失败留在当前界面。 */
	UFUNCTION(BlueprintCallable, Category = "LK|Home") bool ReturnToHome();

	// ---------- D3 房间胜利奖励（三选一/跳过；经 RunSubsystem 原子落地） ----------
	/** 是否存在待领取的奖励批次（有奖励时"下一关"不可点，必须先选或跳过） */
	UFUNCTION(BlueprintPure, Category = "LK|Run") bool HasPendingRewardChoice() const;
	UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetPendingRewardCount() const;
	/** 第 Index 个候选（展示用；越界返回空结构） */
	UFUNCTION(BlueprintPure, Category = "LK|Run") FLKRunRewardOffer GetRunRewardOffer(int32 Index) const;
	UFUNCTION(BlueprintCallable, Category = "LK|Run") bool ChooseRunReward(int32 Index);
    UFUNCTION(BlueprintCallable, Category = "LK|Run") bool ChooseRunRewardReplacing(int32 Index, FName ReplacedCardId);
    UFUNCTION(BlueprintCallable, Category = "LK|Run") bool ChooseRunRewardReplacingCards(int32 Index, const TArray<FName>& ReplacedCardIds);
	UFUNCTION(BlueprintCallable, Category = "LK|Run") bool SkipRunReward();

	// ---------- D4 节点选择（打完并领完奖励后出现；只显示下一排） ----------
	UFUNCTION(BlueprintPure, Category = "LK|Run") bool CanSelectNextNode() const;
	UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetNextNodeCount() const;
	UFUNCTION(BlueprintPure, Category = "LK|Run") FName GetNextNodeId(int32 Index) const;
	UFUNCTION(BlueprintPure, Category = "LK|Run") ELKDungeonNodeType GetNextNodeType(int32 Index) const;
	/** 按钮标题：普通战 / 精英战 / 首领战 / 休息营地 */
	UFUNCTION(BlueprintPure, Category = "LK|Run") FText GetNextNodeTitle(int32 Index) const;
	/** 按钮副标题：敌方英雄名单，或"恢复 30% 最大生命" */
	UFUNCTION(BlueprintPure, Category = "LK|Run") FText GetNextNodeSubtitle(int32 Index) const;
	/** 选择第 Index 个下一排节点：战斗类进入该房（自动重载战场）；休息类回血并继续选择 */
	UFUNCTION(BlueprintCallable, Category = "LK|Run") ELKNodeSelectionResult SelectNextNode(int32 Index);

	// ---------- D5 恢复与终态 ----------
	/** 冷启动恢复到"路线/奖励选择中"（本世界只弹面板，不进入战斗部署） */
	UFUNCTION(BlueprintPure, Category = "LK|Run") bool IsRecoveryJunction() const { return bRecoveredJunction; }
	/** 冷启动恢复到终态（通关/失败摘要，可从此开始新远征） */
	UFUNCTION(BlueprintPure, Category = "LK|Run") bool IsRecoveryTerminal() const { return bRecoveredTerminal; }
	/** 终态摘要文本（通关/失败 + 战斗数与胜负统计） */
	UFUNCTION(BlueprintPure, Category = "LK|Run") FText GetRunSummaryText() const;
	/** 终态摘要后开始新远征（RestartRun + 重载战场） */
	UFUNCTION(BlueprintCallable, Category = "LK|Run") bool StartNewRunFromRecovery();
	/** 仅部署阶段切换测试遭遇；不需要创建亡灵资产。 */
	UFUNCTION(BlueprintCallable, Category = "LK|Encounter") bool ConfigureEnemyEncounter(FName EncounterId);
	UFUNCTION(BlueprintPure, Category = "LK|Encounter") TArray<FName> GetEnemyHeroIds() const { return EnemyHeroIds; }
	UFUNCTION(BlueprintPure, Category = "LK|Encounter") FLKEncounterRow GetCurrentEncounter() const { return CurrentEncounter; }
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
	UFUNCTION(BlueprintPure, Category = "LK|UI") ULKBattleHUDWidget* GetBattleHUDWidget() const { return BattleHUDWidget; }

	UFUNCTION(BlueprintPure, Category = "LK|Battle")
	float GetBattleElapsed() const { return BattleElapsed; }

	// ---------- 出牌 / 部署（玩家与敌方 AI 共用入口） ----------
	ELKPlayResult PlayCardForTeam(ELKTeam Team, int32 HandIndex, const FVector& Location);
	ELKPlayResult DeployHero(ELKTeam Team, FName HeroUnitId, const FVector& Location);
	ELKPlayResult ValidateHeroDeployment(ELKTeam Team, FName HeroUnitId, const FVector& Location, FVector* OutHeroPosition = nullptr) const;

	// ---------- 单位 ----------
	/** SourceCardId：玩家出牌生成单位时的来源卡（D3 卡升级放大依据）；波次/兵营/部署/召唤不传 */
	ALKUnitBase* SpawnUnitForTeam(FName UnitId, ELKTeam Team, const FVector& Location, ELKUnitClass FallbackClass = ELKUnitClass::Soldier, FName SourceCardId = NAME_None);
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
	bool bProcessingDefeats = false;
	bool bEnemyUsesCards = true;
	UPROPERTY(Transient) TArray<TObjectPtr<ALKUnitBase>> PendingDefeats;
	UPROPERTY(Transient) TArray<FName> EnemyHeroIds;
	UPROPERTY(Transient) FLKEncounterRow CurrentEncounter;
	UPROPERTY(Transient) FLKBattleContext BattleContext;
	UPROPERTY(Transient) FLKBattleOutcome BattleOutcome;
	UPROPERTY(Transient) TObjectPtr<ULKBattleHUDWidget> BattleHUDWidget;
	bool bExpeditionBattle = false;
	bool bResultActionInProgress = false;
	/** D5：冷启动恢复标记（本世界不进入战斗部署，只展示恢复面板） */
	bool bRecoveredJunction = false;
	bool bRecoveredTerminal = false;
	void ProcessDefeats();
	void FinalizeHeroRecovery();
	bool InitializeExpeditionContext(bool bAutoStart);
	bool ApplyExpeditionContext(const FLKBattleContext& Context);
	/** D3：按本房奖励档与确定性种子生成 2~3 个候选（升级已有单位卡/骷髅新卡）；无候选返回 false */
	bool BuildRunRewardOffers(TArray<FLKRunRewardOffer>& OutOffers) const;
	/** D4：重载当前战场地图（进入选定战斗房间）；成功前不置位进行中标记 */
	bool ReloadBattleLevel();
	TArray<FLKRunHeroState> BuildInitialRunHeroes() const;
	TArray<FLKRunCardState> BuildInitialRunCards() const;
	bool BuildEncounterCatalog(TArray<FLKEncounterRow>& OutCatalog, FString& OutError) const;
	bool ApplyEnemyEncounter(const FLKEncounterRow& Encounter);
	ULKRunSubsystem* GetRunSubsystem() const;
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
	/** BUG-017：代码身份特性表（DefaultHeroTraits + 单位行内代码特性），用于载入旧档后补全英雄特性。 */
	TMap<FName, TArray<FName>> CollectIdentityTraits() const;
	/** H3：用永久档的已保存战备组装出征输入；没有永久档时返回 false（退回默认名单/独立测试） */
	bool TryBuildProfileStartRequest(FLKExpeditionStartRequest& OutRequest) const;
	/** 终态（通关/失败/放弃）判定：结果按钮在终态改为"返回家园" */
	bool IsTerminalRun() const;
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
