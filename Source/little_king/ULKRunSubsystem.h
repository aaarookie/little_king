#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LKRunTypes.h"
#include "ULKRunSubsystem.generated.h"

class ULKProfileSubsystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnServiceNodeEntered, FName, NodeId, ELKDungeonNodeType, NodeType);

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
	UFUNCTION(BlueprintPure, Category="LK|Run") bool HasServiceNode() const { return State.Phase == ELKRunPhase::ResolvingNode; }
	UFUNCTION(BlueprintCallable, Category="LK|Run") bool ResolveServiceNode(FName ActionId);
	UPROPERTY(BlueprintAssignable, Category="LK|Run") FOnServiceNodeEntered OnServiceNodeEntered;
	UFUNCTION(BlueprintPure, Category="LK|Run") int32 GetWalletGold() const { return State.WalletGold; }
	/** 节点奖励/未来商品共用幂等钱包接口；只有安全节点阶段允许交易。 */
	bool ChangeWalletGold(int32 Delta, FGuid TransactionId);
	/** 读档/进入家园后对齐出发资金；不向不存在的 Profile 借钱。 */
	bool ReconcileDepartureFunding();
    /** 新建远征；已有未结束远征时拒绝覆盖。 */
    bool StartNewRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed);
	bool StartNewRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed,
		const TArray<FLKEncounterRow>& Encounters);
    /** 用户明确点击“从头开始”时重建远征，可覆盖当前/终态。 */
    bool RestartRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed);
	bool RestartRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed,
		const TArray<FLKEncounterRow>& Encounters);

    // ---------- H3：家园出征（区域 + 战备 + 加成快照一次写入） ----------
    /** 家园大门组装的完整出征输入；所有字段在第一次 AutoSave 之前就位 */
    bool StartNewRun(const FLKExpeditionStartRequest& Request);
    /** 终态后重新开始（家园"开始新远征"）；进行中的远征不允许覆盖 */
    bool RestartRun(const FLKExpeditionStartRequest& Request);

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
    bool ChooseReward(int32 Index, FName ReplacedCardId = NAME_None);
    bool ChooseRewardReplacingCards(int32 Index, const TArray<FName>& ReplacedCardIds);
    UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetDeckCapacityUsed() const;
    /** 旧版本超额卡组先由玩家裁减，不在读档时丢弃卡牌。 */
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool NeedsDeckReduction() const { return !IsTerminal() && GetDeckCapacityUsed() > 8; }
    bool DiscardExcessCard(FName CardId);
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
    /** 仅自动化测试：改写远征槽名（避免测试覆盖玩家真实远征档）；传空串恢复默认 */
    static void SetRunSlotNameOverrideForTest(const FString& InSlotName);
    void ConfigureStorage(const FString& SlotName);
    FString GetStorageSlot() const;
    /** 仅自动化测试：迁移旧档时用它作为关联的 ProfileId（测试实例没有注册的 Profile 子系统） */
    void SetProfileIdOverrideForTest(const FGuid& InProfileId) { ProfileIdOverrideForTest = InProfileId; }
    /** 仅自动化测试：入账交接时使用指定永久档（测试的两个子系统不共享 GameInstance） */
    void SetProfileSubsystemOverrideForTest(ULKProfileSubsystem* InProfile);
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

    // ---------- H4：金币结算（远征终态 → 家园永久档，幂等交接） ----------
    /** 本轮已暂存金币（每房胜利累加；终态按规则折算） */
    UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetPendingGold() const { return State.PendingGold; }
    /** 终态冻结的结算回执（未终态时 SettlementId 无效） */
    UFUNCTION(BlueprintPure, Category = "LK|Run") FLKSettlementReceipt GetPendingSettlement() const { return State.PendingSettlement; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool HasPendingSettlement() const { return State.PendingSettlement.SettlementId.IsValid(); }
    /** 标记 Run 侧交接完成（Profile 已入账）；写盘失败返回 false，可重试 */
    bool MarkPendingSettlementApplied();
    /** 让 Profile 幂等入账并标记交接；返回是否已完成交接（无 Profile/不参与结算时返回 false） */
    bool ApplyPendingSettlementToProfile();
    /** 明确结束当前远征：终态 Abandoned，全额带回钱包剩余金币 */
    bool AbandonCurrentRun();

    UFUNCTION(BlueprintPure, Category = "LK|Run") FLKRunState GetRunState() const { return State; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") ELKRunPhase GetRunPhase() const { return State.Phase; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool HasRun() const { return State.RunId.IsValid(); }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool HasRunInProgress() const;
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool CanAdvance() const { return State.Phase == ELKRunPhase::ChoosingNode; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool IsTerminal() const;
    UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetCurrentRoomIndex() const;
    /** 已完成战斗 + 从当前节点出发最多还需的战斗数；选路后可能减少。 */
    UFUNCTION(BlueprintPure, Category = "LK|Run") int32 GetTotalRoomCount() const;
    UFUNCTION(BlueprintPure, Category = "LK|Run") FName GetRegionId() const { return State.RegionId; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") FLKMetaBonusSnapshot GetBonusSnapshot() const { return State.BonusSnapshot; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") FLKHomeRewardRules GetRewardRules() const { return State.RewardRules; }
    UFUNCTION(BlueprintPure, Category = "LK|Run") bool IsHomeRewardEligible() const { return State.bHomeRewardEligible; }

    /** Stage 3 content/capacity contract; version 6 routes and wallets are preserved. */
    static constexpr int32 CurrentSchemaVersion = 8;

private:
    UPROPERTY(Transient) FLKRunState State;
    FString StorageSlot;
    bool bAutoSaveEnabled = true;
    /** 仅自动化测试：迁移旧档时的 ProfileId 关联值 */
    FGuid ProfileIdOverrideForTest;
    /** 仅自动化测试：入账交接使用的永久档 */
    TWeakObjectPtr<ULKProfileSubsystem> ProfileSubsystemOverrideForTest;
    /** 安全点自动保存（成功路径末尾调用；写失败仅 Warning，不影响内存状态） */
    void AutoSave();

    /** 家园出征/重开的公共实现（bRestart = 允许覆盖终态） */
    bool StartRunInternal(const FLKExpeditionStartRequest& Request, bool bRestart);
    /** 旧签名（独立测试/调试）构造的出征输入：不关联 Profile、不参与金币结算 */
    FLKExpeditionStartRequest MakeStandaloneRequest(const TArray<FLKRunHeroState>& Heroes,
        const TArray<FLKRunCardState>& Cards, int32 Seed, const TArray<FLKEncounterRow>& Encounters) const;
    /** 终态时冻结金币结算回执（同一轮只冻结一次） */
    void FreezeTerminalSettlement();
    /** v0.6 档迁移到 Schema 5：补默认加成/区域/战备并标记旧轮不参与金币结算 */
    void MigrateStoredRun(FLKRunState& InOutState) const;

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
