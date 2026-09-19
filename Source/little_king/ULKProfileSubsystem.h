#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LKHomeTypes.h"
#include "ULKProfileSubsystem.generated.h"

class ULKGameData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHomeProfileChanged);

/**
 * 家园永久进度（H1/H2/H4）。持有 FLKProfileState：金币、建筑等级、永久解锁、已保存战备、已处理结算。
 *
 * 关键约定：
 *  - 双槽 A/B + 递增 Revision：候选写入 → 读回校验 → 才发布；两槽都不可读时保留原文件并禁用永久写操作。
 *  - 永久扣费/入账是单次提交：先校验（等级、费用、余额、预期等级），写盘成功后才对外发布新状态。
 *  - 不控制战斗单位、不累计当场银币；战斗内数值由 ULKRunSubsystem 的出征快照决定。
 */
UCLASS()
class ULKProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---------- 槽名 ----------
	static FString GetSlotNameA();
	static FString GetSlotNameB();
	/** 旧/兼容槽名（历史保留，不参与 A/B 轮换） */
	static FString GetLegacySlotName();
	/**
	 * 仅自动化测试：改写永久档槽名前缀（默认 LittleKing_Profile）。
	 * 让测试写自己的槽，绝不触碰玩家真实永久档；传空串恢复默认。
	 */
	static void SetSlotNameOverrideForTest(const FString& InBaseName);
	/** 仅自动化测试：关闭永久档读写（战斗/地牢回归不需要也不应改动真实永久档） */
	static void SetPersistentProfileEnabledForTest(bool bEnabled);
	/** 仅自动化测试：恢复默认槽名与开关（测试夹具析构时调用） */
	static void ResetTestHooks();
	/** 实例级存储路由；不同 GameInstance/PIE 互不共享当前存档选择。 */
	void ConfigureStorage(const FString& BaseName);
	FString GetStorageSlot(bool bSlotB) const;
	bool ReadExistingProfile(FLKProfileState& OutState, FString& OutError) const;
	bool TouchSelectedProfile();
	int32 GetDepartureGoldCap() const;
	bool ReserveDepartureGold(FGuid RunId, int32 Amount);
	/** 仅在已成功读取/确认没有 Run 后调用；未知/损坏 Run 不触发退款。 */
	bool ReconcileDepartureGold(FGuid StoredRunId);

	// ---------- 读取 / 创建 ----------
	/** 已有内存档直接返回；否则读 A/B 最高有效修订；都没有则创建默认档（写盘失败只标记不可写） */
	bool EnsureProfile();
	bool HasProfile() const { return State.ProfileId.IsValid(); }
	/** 永久写操作是否可用（存档读写正常） */
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool IsProfileUsable() const { return bProfileUsable; }
	/** 最近一次错误说明（供 UI 诊断） */
	const FString& GetLastError() const { return LastError; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") FLKProfileState GetProfile() const { return State; }

	// ---------- 查询 ----------
	UFUNCTION(BlueprintPure, Category = "LK|Home") int32 GetGold() const { return State.Gold; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") int32 GetBuildingLevel(FName BuildingId) const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") float GetStatueRecoveryPercent() const;
	UFUNCTION(BlueprintPure, Category = "LK|Home") FLKExpeditionLoadout GetSavedLoadout() const { return State.SavedLoadout; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") FName GetLastSelectedRegionId() const { return State.LastSelectedRegionId; }
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool IsHeroUnlocked(FName HeroId) const { return State.UnlockedHeroIds.Contains(HeroId); }
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool IsCardUnlocked(FName CardId) const { return State.UnlockedCardIds.Contains(CardId); }
	UFUNCTION(BlueprintPure, Category = "LK|Home") bool IsRegionUnlocked(FName RegionId) const { return State.UnlockedRegionIds.Contains(RegionId); }

	/** 用当前永久进度生成出征加成快照（创建 RunId 时冻结；基础值来自 GameData） */
	FLKMetaBonusSnapshot BuildBonusSnapshot(const ULKGameData* Data, FName RegionId) const;

	// ---------- 事务：建筑升级 ----------
	/**
	 * 单次提交：校验建筑/预期等级/满级/余额 → 生成新副本 → 写盘并读回校验 → 发布。
	 * 失败保留旧状态（不扣费），可安全重试；RequestId 仅用于日志追踪。
	 */
	ELKUpgradeResult UpgradeBuilding(FName BuildingId, int32 ExpectedLevel, FGuid RequestId);

	// ---------- 事务：战备 ----------
	ELKLoadoutResult ValidateLoadout(const FLKExpeditionLoadout& Loadout, const ULKGameData* Data) const;
	ELKLoadoutResult SaveLoadout(const FLKExpeditionLoadout& Loadout, const ULKGameData* Data);

	// ---------- 事务：金币入账 ----------
	/** 幂等入账：同一 SettlementId 只加一次；成功后记录到 ProcessedSettlementIds */
	ELKSettlementResult ApplySettlement(const FLKSettlementReceipt& Receipt);
	bool HasProcessedSettlement(const FGuid& SettlementId) const;

	/** 记录最近选择的区域（不参与任何数值计算） */
	bool SetLastSelectedRegionId(FName RegionId);

	// ---------- 调试 ----------
	UFUNCTION(BlueprintCallable, Category = "LK|Home|Debug") bool AddGold(int32 Amount);
	UFUNCTION(BlueprintCallable, Category = "LK|Home|Debug") bool ResetProfile();

	/** 自动化测试关闭落盘（避免并行写同一槽） */
	void SetAutoSaveEnabled(bool bEnabled) { bAutoSaveEnabled = bEnabled; }
	/** 测试用：用内存状态直接覆盖（不写盘、不做校验），随后由调用方 SaveForTest 落盘 */
	void SetProfileForTest(const FLKProfileState& InState) { State = InState; bProfileUsable = InState.ProfileId.IsValid(); }
	bool SaveForTest() { FLKProfileState Candidate = State; return CommitProfile(Candidate); }

	UPROPERTY(BlueprintAssignable, Category = "LK|Home")
	FOnHomeProfileChanged OnProfileChanged;

private:
	/** 候选写入 → 读回校验 → 发布；成功才把 Candidate 发布为 State（失败保留旧状态） */
	bool CommitProfile(FLKProfileState Candidate);
	bool LoadBestValid(FLKProfileState& OutState, int32& OutSlotIndex, FString& OutError) const;
	static bool ValidateProfile(const FLKProfileState& Profile, FString& OutError);
	static FLKProfileState MakeDefaultProfile();

	UPROPERTY(Transient) FLKProfileState State;
	FString StorageBase;
	bool bProfileUsable = false;
	bool bAutoSaveEnabled = true;
	/** 最近一次成功写入/读取的槽下标（0 = A，1 = B）；-1 表示未知 */
	int32 LastSlotIndex = -1;
	FString LastError;
};
