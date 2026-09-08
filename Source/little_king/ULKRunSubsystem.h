#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LKRunTypes.h"
#include "ULKRunSubsystem.generated.h"

/**
 * D2 远征状态机。它保存英雄永久状态和开局遭遇快照；
 * 战场 Actor、临时光环、强制目标、银币、手牌位置、弹道与技能冷却均由每个世界重建。
 * 路线仍固定为普通战 -> 精英战 -> 首领战，不包含奖励 UI、随机路线或存档。
 */
UCLASS()
class ULKRunSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** 新建远征；已有未结束远征时拒绝覆盖。 */
    bool StartNewRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed);
	bool StartNewRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed,
		const TArray<FLKEncounterRow>& Encounters);
    /** 用户明确点击“从头开始”时重建远征，可覆盖当前/终态。 */
    bool RestartRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed);
	bool RestartRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed,
		const TArray<FLKEncounterRow>& Encounters);
    /** 当前世界领取待进入的房间上下文；重复加载同一战斗返回同一个 AttemptId。 */
    bool BeginCurrentBattle(FLKBattleContext& OutContext);
    /** 只接受身份完整且未处理过的当前结果；成功后保存恢复后英雄状态。 */
    bool SubmitBattleOutcome(const FLKBattleOutcome& Outcome);
    /** D4：选择"下一排"中的一个节点。战斗类进入该房（EnteringBattle）；休息类立即回血并继续选择。 */
    ELKNodeSelectionResult SelectNode(FName NodeId);
    /** D1~D3 兼容入口：自动选择下一排第一个战斗节点（无战斗节点时失败）。 */
    bool AdvanceToNextBattle();

    // ---------- D4 节点选择查询 ----------
    /** 是否处于"可看到下一排并选择"的状态（打完并领完奖励后） */
    UFUNCTION(BlueprintPure, Category = "LK|Run")
    bool CanSelectNextNode() const { return State.Phase == ELKRunPhase::ChoosingNode && GetNextNodeCount() > 0; }
    UFUNCTION(BlueprintPure, Category = "LK|Run")
    int32 GetNextNodeCount() const;
    /** 下一排节点（UI 只读这一排，看不到后续——可见性由该列表天然保证） */
    UFUNCTION(BlueprintPure, Category = "LK|Run")
    TArray<FName> GetNextNodeIds() const;
    UFUNCTION(BlueprintPure, Category = "LK|Run")
    FLKDungeonNode GetNode(FName NodeId) const;

    // ---------- D3 房间胜利奖励（三选一/跳过；状态机原子入口） ----------
    /** 房间胜利且有下一间时，由 GameMode 生成候选并提交（一次）。成功后进入 ChoosingReward。 */
    bool OfferRewardBatch(const TArray<FLKRunRewardOffer>& Offers);
    /** 是否处于"待领取奖励"状态（结果界面先处理奖励，再允许"下一关"） */
    UFUNCTION(BlueprintPure, Category = "LK|Run")
    bool HasPendingRewardChoice() const { return State.Phase == ELKRunPhase::ChoosingReward && State.PendingRewardOffers.Num() > 0; }
    UFUNCTION(BlueprintPure, Category = "LK|Run")
    int32 GetPendingRewardCount() const { return HasPendingRewardChoice() ? State.PendingRewardOffers.Num() : 0; }
    UFUNCTION(BlueprintPure, Category = "LK|Run")
    FLKRunRewardOffer GetPendingRewardOffer(int32 Index) const;
    /** 原子领取第 Index 个选项：只允许一次，成功后回到 ChoosingNode（可点"下一关"） */
    bool ChooseReward(int32 Index);
    /** 原子跳过本批奖励：消费批次并回到 ChoosingNode；不可回头补领 */
    bool SkipReward();
	/** 供后续奖励/事件使用：只允许在两场之间修改永久基础最大生命。 */
	bool SetHeroBaseMaxHealth(FName HeroId, float NewBaseMaxHealth, bool bPreserveHealthRatio = false);
	/** 永久特性变更只允许在两场之间；战内 AddTrait/RemoveTrait 则由合法 Outcome 带回。 */
	bool AddHeroTrait(FName HeroId, FName TraitId);
	bool RemoveHeroTrait(FName HeroId, FName TraitId);
    void AbandonRun();

    /**
     * BUG-017：载入存档后按"代码身份特性"补全英雄（如法师的 Trait_MageSpellReach、骑士的嘲讽光环）。
     * 身份特性属于英雄身份，不能因为旧档创建时还没写进快照就永久丢失；
     * 房间间调试删除（RunHeroTrait ... 0）仍在本次会话内生效，只有重新载入才会补回。
     * 返回补回的条目数（0 = 无需修复）；有变化时按安全点自动保存。
     */
    int32 RestoreHeroIdentityTraits(const TMap<FName, TArray<FName>>& IdentityTraits);

    // ---------- D5 安全节点存档 ----------
    /** 固定远征存档槽名 */
    static FString GetRunSlotName();
    /** 载入前校验：版本/数值/结构/引用；未知版本与损坏内容明确拒绝 */
    static bool ValidateStoredRun(const FLKRunState& State, FString& OutError);
    /** 写盘到指定槽（测试可指定唯一槽）；失败返回 false 且不改内存 */
    bool SaveExpeditionToSlot(const FString& SlotName) const;
    /** 从指定槽载入；bAllowExistingRun=false 时拒绝覆盖进行中的内存远征 */
    bool LoadExpeditionFromSlot(const FString& SlotName, bool bAllowExistingRun = false);
    /** 固定槽自动保存（安全点由状态机内部调用；写失败仅记日志） */
    bool SaveExpedition();
    /** 固定槽载入（进程冷启动恢复入口） */
    bool LoadExpedition();
    bool HasSavedExpedition() const;
    void ClearSavedExpedition();
    /** 自动化测试关闭自动落盘，避免并行写同一槽 */
    void SetAutoSaveEnabled(bool bEnabled) { bAutoSaveEnabled = bEnabled; }

    UFUNCTION(BlueprintPure, Category = "LK|Run") FLKRunState GetRunState() const { return State; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") ELKRunPhase GetRunPhase() const { return State.Phase; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool HasRun() const { return State.RunId.IsValid(); }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool HasRunInProgress() const;
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool CanAdvance() const { return State.Phase == ELKRunPhase::ChoosingNode; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool IsTerminal() const;
    UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetCurrentRoomIndex() const;
    UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetTotalRoomCount() const { return 4; }

private:
    UPROPERTY(Transient) FLKRunState State;
    bool bAutoSaveEnabled = true;
    /** 安全点自动保存（成功路径末尾调用；写失败仅 Warning，不影响内存状态） */
    void AutoSave();

    bool ValidateStartingParty(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards) const;
	bool ValidateEncounterCatalog(const TArray<FLKEncounterRow>& Encounters) const;
	bool CanEditPermanentState() const;
    bool BuildPendingBattle();
    /** D4：生成分层节点图与随机敌阵容动态遭遇（同种子同结果）。 */
    bool GenerateGraphAndEncounters(FRandomStream& Stream, FString& OutError);
    static ELKDungeonNodeType NodeTypeForRank(ELKEncounterRank Rank);
    FLKDungeonNode* FindNode(FName NodeId);
    const FLKDungeonNode* FindNode(FName NodeId) const;
};
