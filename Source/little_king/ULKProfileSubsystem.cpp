#include "ULKProfileSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#include "LKHomeContent.h"
#include "LKCardRules.h"
#include "LKLog.h"
#include "ULKGameData.h"
#include "ULKProfileSaveGame.h"

namespace
{
	/** 自动化测试钩子：槽名前缀与永久档开关（默认前缀 + 启用） */
	FString GProfileSlotBase = TEXT("LittleKing_Profile");
	bool GProfilePersistentEnabled = true;
	/** 测试显式接管永久档（LKHomeTests）时为 true，此时不再自动屏蔽 */
	bool GProfileAutomationOverride = false;

	/**
	 * 是否允许读写真实永久档。
	 * 自动化测试默认屏蔽（战斗/地牢回归不应创建或给玩家永久档加金币）；
	 * LKHomeTests 通过 SetPersistentProfileEnabledForTest 显式接管，并把槽名换成测试槽。
	 */
	bool IsProfilePersistenceAllowed()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (FAutomationTestFramework::Get().GetCurrentTest() && !GProfileAutomationOverride) { return false; }
#endif
		return GProfilePersistentEnabled;
	}

	/** 战备的结构合法性（数量/唯一性）；解锁资格由 LKHomeContent::ValidateLoadout 严格校验 */
	bool IsLoadoutWellFormed(const FLKExpeditionLoadout& Loadout, FString& OutError)
	{
		if (Loadout.HeroIds.Num() != 3) { OutError = TEXT("英雄数量不是 3"); return false; }
		if (Loadout.HeroIds.Contains(NAME_None)) { OutError = TEXT("英雄含空 ID"); return false; }
		if (Loadout.CardIds.Num() < LKHomeContent::MinStartingDeck()
			|| LKCardRules::Used(Loadout.CardIds) > LKCardRules::Capacity)
		{ OutError = TEXT("初始牌组至少 5 种卡，部队容量不超过 8 格"); return false; }
		if (Loadout.CardIds.Contains(NAME_None)) { OutError = TEXT("卡牌含空 ID"); return false; }
		TSet<FName> Heroes;
		for (FName HeroId : Loadout.HeroIds) { if (Heroes.Contains(HeroId)) { OutError = TEXT("英雄重复"); return false; } Heroes.Add(HeroId); }
		TSet<FName> Cards;
		for (FName CardId : Loadout.CardIds) { if (Cards.Contains(CardId)) { OutError = TEXT("卡牌重复"); return false; } Cards.Add(CardId); }
		return true;
	}
}

FString ULKProfileSubsystem::GetSlotNameA() { return FString::Printf(TEXT("%s_A"), *GProfileSlotBase); }
FString ULKProfileSubsystem::GetSlotNameB() { return FString::Printf(TEXT("%s_B"), *GProfileSlotBase); }
FString ULKProfileSubsystem::GetLegacySlotName() { return GProfileSlotBase; }

void ULKProfileSubsystem::SetSlotNameOverrideForTest(const FString& InBaseName)
{
	GProfileSlotBase = InBaseName.IsEmpty() ? FString(TEXT("LittleKing_Profile")) : InBaseName;
}

void ULKProfileSubsystem::SetPersistentProfileEnabledForTest(bool bEnabled)
{
	GProfilePersistentEnabled = bEnabled;
	GProfileAutomationOverride = true;
}

void ULKProfileSubsystem::ResetTestHooks()
{
	GProfileSlotBase = TEXT("LittleKing_Profile");
	GProfilePersistentEnabled = true;
	GProfileAutomationOverride = false;
}

void ULKProfileSubsystem::ConfigureStorage(const FString& BaseName)
{
	StorageBase = BaseName;
	State = FLKProfileState();
	bProfileUsable = false;
	LastSlotIndex = -1;
	LastError.Reset();
}

FString ULKProfileSubsystem::GetStorageSlot(bool bSlotB) const
{
	return StorageBase.IsEmpty() ? (bSlotB ? GetSlotNameB() : GetSlotNameA()) : StorageBase + (bSlotB ? TEXT("_B") : TEXT("_A"));
}

bool ULKProfileSubsystem::ReadExistingProfile(FLKProfileState& OutState, FString& OutError) const
{
	int32 Index = -1;
	return LoadBestValid(OutState, Index, OutError);
}

bool ULKProfileSubsystem::TouchSelectedProfile()
{
	return HasProfile() && bProfileUsable && CommitProfile(State);
}

FLKProfileState ULKProfileSubsystem::MakeDefaultProfile()
{
	FLKProfileState Profile;
	Profile.SchemaVersion = 1;
	Profile.ProfileId = FGuid::NewGuid();
	Profile.Revision = 0;
	Profile.Gold = 0;
	for (const FLKBuildingDefinition& Definition : LKHomeContent::Buildings())
	{
		Profile.BuildingLevels.Add(Definition.BuildingId, 1);
	}
	Profile.UnlockedHeroIds = LKHomeContent::DefaultUnlockedHeroes();
	Profile.UnlockedCardIds = LKHomeContent::DefaultUnlockedCards();
	Profile.UnlockedRegionIds = LKHomeContent::DefaultUnlockedRegions();
	Profile.SavedLoadout = LKHomeContent::DefaultLoadout();
	Profile.LastSelectedRegionId = LKHomeContent::DefaultRegionId();
	return Profile;
}

bool ULKProfileSubsystem::ValidateProfile(const FLKProfileState& Profile, FString& OutError)
{
	OutError.Reset();
	if (Profile.SchemaVersion < 1 || Profile.SchemaVersion > 1)
	{
		OutError = FString::Printf(TEXT("永久档结构版本 %d 不受支持"), Profile.SchemaVersion);
		return false;
	}
	if (!Profile.ProfileId.IsValid()) { OutError = TEXT("ProfileId 无效"); return false; }
	if (Profile.Revision < 0) { OutError = TEXT("Revision 非法"); return false; }
	if (Profile.Gold < 0) { OutError = TEXT("金币为负"); return false; }
	if (Profile.FundedRunGold < 0 || (!Profile.FundedRunId.IsValid() && Profile.FundedRunGold != 0)) { OutError = TEXT("出发资金凭据无效"); return false; }
	if (Profile.UnlockedHeroIds.IsEmpty() || Profile.UnlockedCardIds.IsEmpty() || Profile.UnlockedRegionIds.IsEmpty())
	{
		OutError = TEXT("永久解锁集为空");
		return false;
	}
	for (const TPair<FName, int32>& Pair : Profile.BuildingLevels)
	{
		const FLKBuildingDefinition* Definition = LKHomeContent::FindBuilding(Pair.Key);
		if (!Definition) { OutError = FString::Printf(TEXT("未知建筑 %s"), *Pair.Key.ToString()); return false; }
		if (Pair.Value < 1 || Pair.Value > Definition->MaxLevel)
		{
			OutError = FString::Printf(TEXT("建筑 %s 等级 %d 越界（1~%d）"), *Pair.Key.ToString(), Pair.Value, Definition->MaxLevel);
			return false;
		}
	}
	FString LoadoutError;
	if (!IsLoadoutWellFormed(Profile.SavedLoadout, LoadoutError))
	{
		OutError = FString::Printf(TEXT("已保存战备非法：%s"), *LoadoutError);
		return false;
	}
	return true;
}

bool ULKProfileSubsystem::LoadBestValid(FLKProfileState& OutState, int32& OutSlotIndex, FString& OutError) const
{
	OutError.Reset();
	bool bFound = false;
	int32 BestRevision = -1;
	FString BestError;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FString Slot = Index == 0 ? GetStorageSlot(false) : GetStorageSlot(true);
		if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) { continue; }
		ULKProfileSaveGame* Save = Cast<ULKProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!Save)
		{
			BestError = FString::Printf(TEXT("槽 %s 读取失败"), *Slot);
			continue;
		}
		if (Save->SaveVersion < 1 || Save->SaveVersion > 1)
		{
			BestError = FString::Printf(TEXT("槽 %s 文件版本 %d 不受支持"), *Slot, Save->SaveVersion);
			continue;
		}
		FString ValidationError;
		if (!ValidateProfile(Save->Profile, ValidationError))
		{
			BestError = FString::Printf(TEXT("槽 %s 内容非法：%s"), *Slot, *ValidationError);
			continue;
		}
		if (Save->Profile.Revision > BestRevision)
		{
			BestRevision = Save->Profile.Revision;
			OutState = Save->Profile;
			OutSlotIndex = Index;
			bFound = true;
		}
	}
	if (!bFound) { OutError = BestError.IsEmpty() ? TEXT("没有永久档") : BestError; }
	return bFound;
}

bool ULKProfileSubsystem::CommitProfile(FLKProfileState Candidate)
{
	FString Error;
	if (!ValidateProfile(Candidate, Error))
	{
		LastError = FString::Printf(TEXT("拒绝写入非法永久档：%s"), *Error);
		UE_LOG(LogLK, Warning, TEXT("[Home] %s"), *LastError);
		return false;
	}
	++Candidate.Revision;
	if (!IsProfilePersistenceAllowed())
	{
		LastError = TEXT("永久档读写已禁用（测试模式）");
		return false;
	}
	if (!bAutoSaveEnabled)
	{
		State = Candidate;
		bProfileUsable = true;
		LastError.Reset();
		return true;
	}

	// 轮换写入另一槽：上一版仍保留在旧槽，读档失败时可回退。
	const int32 WriteIndex = (LastSlotIndex == 0) ? 1 : 0;
	const FString Slot = WriteIndex == 0 ? GetStorageSlot(false) : GetStorageSlot(true);
	ULKProfileSaveGame* Save = NewObject<ULKProfileSaveGame>();
	Save->SaveVersion = 1;
	Save->Profile = Candidate;
	Save->SavedAtUtc = FDateTime::UtcNow();
	if (!UGameplayStatics::SaveGameToSlot(Save, Slot, 0))
	{
		LastError = FString::Printf(TEXT("永久档写入失败（槽 %s）：本次操作未生效"), *Slot);
		UE_LOG(LogLK, Warning, TEXT("[Home] %s"), *LastError);
		return false;
	}

	// 读回校验：确认落盘内容与候选一致后才发布（避免"显示成功但实际没存"）。
	ULKProfileSaveGame* ReadBack = Cast<ULKProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	FString ReadError;
	if (!ReadBack || !ValidateProfile(ReadBack->Profile, ReadError)
		|| ReadBack->Profile.Revision != Candidate.Revision
		|| ReadBack->Profile.ProfileId != Candidate.ProfileId
		|| ReadBack->Profile.Gold != Candidate.Gold)
	{
		LastError = FString::Printf(TEXT("永久档读回校验失败（槽 %s）：本次操作未生效"), *Slot);
		UE_LOG(LogLK, Warning, TEXT("[Home] %s"), *LastError);
		return false;
	}

	State = Candidate;
	LastSlotIndex = WriteIndex;
	bProfileUsable = true;
	LastError.Reset();
	UE_LOG(LogLK, Log, TEXT("[Home] 永久档已保存：槽 %s Revision=%d 金币=%d"),
		*Slot, State.Revision, State.Gold);
	return true;
}

bool ULKProfileSubsystem::EnsureProfile()
{
	if (HasProfile()) { return true; }
	if (!IsProfilePersistenceAllowed())
	{
		// 自动化/独立调试：不读写真实永久档，永久操作保持禁用。
		LastError = TEXT("永久档读写已禁用（测试模式）");
		bProfileUsable = false;
		return false;
	}

	FLKProfileState Loaded;
	int32 SlotIndex = -1;
	FString Error;
	if (LoadBestValid(Loaded, SlotIndex, Error))
	{
		State = Loaded;
		LastSlotIndex = SlotIndex;
		bProfileUsable = true;
		LastError.Reset();
		UE_LOG(LogLK, Log, TEXT("[Home] 已读取永久档 %s（Revision=%d，金币=%d，槽 %s）"),
			*State.ProfileId.ToString(), State.Revision, State.Gold, SlotIndex == 0 ? TEXT("A") : TEXT("B"));
		OnProfileChanged.Broadcast();
		return true;
	}

	const bool bAnyFileExists = UGameplayStatics::DoesSaveGameExist(GetStorageSlot(false), 0)
		|| UGameplayStatics::DoesSaveGameExist(GetStorageSlot(true), 0);
	if (bAnyFileExists)
	{
		// 两个槽都存在但都不可用：保留原文件，禁用永久写操作（不静默新建覆盖）。
		// 用 Warning 而非 Error：这是"可预期的拒绝"（自动化框架会把 Error 日志判为测试失败）。
		LastError = FString::Printf(TEXT("永久档不可用：%s（原文件保留，未覆盖）"), *Error);
		UE_LOG(LogLK, Warning, TEXT("[Home] %s"), *LastError);
		bProfileUsable = false;
		return false;
	}

	// 完全新玩家：创建默认档并先写盘成功，再允许永久操作。
	FLKProfileState Fresh = MakeDefaultProfile();
	if (!CommitProfile(Fresh))
	{
		// 内存里保留默认档供浏览，但永久写操作禁用（下次启动重试创建）。
		State = Fresh;
		State.Revision = 0;
		bProfileUsable = false;
		UE_LOG(LogLK, Warning, TEXT("[Home] 新永久档创建失败：本次会话只读，永久操作已禁用"));
		OnProfileChanged.Broadcast();
		return true;
	}
	UE_LOG(LogLK, Log, TEXT("[Home] 已创建新永久档 %s（神像/金库 1 级，金币 0）"), *State.ProfileId.ToString());
	OnProfileChanged.Broadcast();
	return true;
}

int32 ULKProfileSubsystem::GetBuildingLevel(FName BuildingId) const
{
	const int32* Found = State.BuildingLevels.Find(BuildingId);
	const FLKBuildingDefinition* Definition = LKHomeContent::FindBuilding(BuildingId);
	const int32 MaxLevel = Definition ? Definition->MaxLevel : 1;
	return FMath::Clamp(Found ? *Found : 1, 1, FMath::Max(1, MaxLevel));
}

float ULKProfileSubsystem::GetStatueRecoveryPercent() const
{
	return LKHomeContent::StatueRecoveryPercent(GetBuildingLevel(LKHomeContent::BuildingId(ELKHomeBuilding::Statue)));
}

FLKMetaBonusSnapshot ULKProfileSubsystem::BuildBonusSnapshot(const ULKGameData* Data, FName RegionId) const
{
	FLKMetaBonusSnapshot Snapshot;
	Snapshot.RegionId = RegionId;
	Snapshot.StatueLevel = GetBuildingLevel(LKHomeContent::BuildingId(ELKHomeBuilding::Statue));
	Snapshot.TreasuryLevel = GetBuildingLevel(LKHomeContent::BuildingId(ELKHomeBuilding::Treasury));
	Snapshot.HeroRecoveryPercent = LKHomeContent::StatueRecoveryPercent(Snapshot.StatueLevel);

	const float BaseSpeed = (Data && FMath::IsFinite(Data->SilverPerSecond) && Data->SilverPerSecond > 0.f)
		? Data->SilverPerSecond : (1.f / 3.f);
	const float BaseCap = (Data && FMath::IsFinite(Data->SilverCap) && Data->SilverCap > 0.f)
		? Data->SilverCap : 5.f;
	Snapshot.PlayerSilverPerSecond = BaseSpeed * LKHomeContent::TreasurySpeedMultiplier(Snapshot.TreasuryLevel);
	Snapshot.PlayerSilverCap = BaseCap + float(LKHomeContent::TreasuryCapBonus(Snapshot.TreasuryLevel));
	Snapshot.RuleVersion = 1;
	return Snapshot;
}

ELKUpgradeResult ULKProfileSubsystem::UpgradeBuilding(FName BuildingId, int32 ExpectedLevel, FGuid RequestId)
{
	if (!EnsureProfile() || !HasProfile()) { return ELKUpgradeResult::NoProfile; }
	const FLKBuildingDefinition* Definition = LKHomeContent::FindBuilding(BuildingId);
	if (!Definition) { return ELKUpgradeResult::UnknownBuilding; }
	if (!Definition->bUpgradable) { return ELKUpgradeResult::NotUpgradable; }

	const int32 CurrentLevel = GetBuildingLevel(BuildingId);
	if (CurrentLevel >= Definition->MaxLevel) { return ELKUpgradeResult::MaxLevel; }
	if (ExpectedLevel > 0 && ExpectedLevel != CurrentLevel) { return ELKUpgradeResult::LevelMismatch; }

	const int32 Cost = LKHomeContent::UpgradeCost(BuildingId, CurrentLevel);
	if (Cost < 0) { return ELKUpgradeResult::MaxLevel; }
	if (State.Gold < Cost) { return ELKUpgradeResult::InsufficientGold; }
	if (!bProfileUsable) { return ELKUpgradeResult::SaveFailed; }

	FLKProfileState Candidate = State;
	Candidate.Gold -= Cost;
	Candidate.BuildingLevels.Add(BuildingId, CurrentLevel + 1);
	if (!CommitProfile(MoveTemp(Candidate))) { return ELKUpgradeResult::SaveFailed; }

	UE_LOG(LogLK, Log, TEXT("[Home] 升级 %s：%d -> %d（花费 %d 金币，剩余 %d，请求 %s）"),
		*BuildingId.ToString(), CurrentLevel, CurrentLevel + 1, Cost, State.Gold, *RequestId.ToString());
	OnProfileChanged.Broadcast();
	return ELKUpgradeResult::Success;
}

ELKLoadoutResult ULKProfileSubsystem::ValidateLoadout(const FLKExpeditionLoadout& Loadout, const ULKGameData* Data) const
{
	FString Error;
	return LKHomeContent::ValidateLoadout(Loadout, Data, Error);
}

ELKLoadoutResult ULKProfileSubsystem::SaveLoadout(const FLKExpeditionLoadout& Loadout, const ULKGameData* Data)
{
	if (!EnsureProfile() || !HasProfile()) { return ELKLoadoutResult::NoProfile; }
	FString Error;
	const ELKLoadoutResult Validation = LKHomeContent::ValidateLoadout(Loadout, Data, Error);
	if (Validation != ELKLoadoutResult::Success)
	{
		UE_LOG(LogLK, Warning, TEXT("[Home] 拒绝保存战备：%s"), *Error);
		return Validation;
	}
	if (!bProfileUsable) { return ELKLoadoutResult::SaveFailed; }

	FLKProfileState Candidate = State;
	Candidate.SavedLoadout = Loadout;
	if (!CommitProfile(MoveTemp(Candidate))) { return ELKLoadoutResult::SaveFailed; }
	UE_LOG(LogLK, Log, TEXT("[Home] 战备已保存：英雄 %d 名、初始牌组 %d 张"), State.SavedLoadout.HeroIds.Num(), State.SavedLoadout.CardIds.Num());
	OnProfileChanged.Broadcast();
	return ELKLoadoutResult::Success;
}

bool ULKProfileSubsystem::HasProcessedSettlement(const FGuid& SettlementId) const
{
	return SettlementId.IsValid() && State.ProcessedSettlementIds.Contains(SettlementId);
}

ELKSettlementResult ULKProfileSubsystem::ApplySettlement(const FLKSettlementReceipt& Receipt)
{
	if (!EnsureProfile() || !HasProfile()) { return ELKSettlementResult::NoProfile; }
	if (!Receipt.SettlementId.IsValid() || Receipt.GoldAmount < 0) { return ELKSettlementResult::IdentityMismatch; }
	if (Receipt.ProfileId.IsValid() && Receipt.ProfileId != State.ProfileId) { return ELKSettlementResult::IdentityMismatch; }
	if (State.ProcessedSettlementIds.Contains(Receipt.SettlementId)) { return ELKSettlementResult::AlreadyApplied; }
	if (!bProfileUsable) { return ELKSettlementResult::SaveFailed; }

	FLKProfileState Candidate = State;
	Candidate.Gold = int32(FMath::Min<int64>(MAX_int32, int64(Candidate.Gold) + Receipt.GoldAmount));
	if (Candidate.FundedRunId == Receipt.RunId) { Candidate.FundedRunId.Invalidate(); Candidate.FundedRunGold = 0; }
	Candidate.ProcessedSettlementIds.Add(Receipt.SettlementId);
	if (!CommitProfile(MoveTemp(Candidate))) { return ELKSettlementResult::SaveFailed; }
	UE_LOG(LogLK, Log, TEXT("[Home] 远征结算入账：+%d 金币（结算 %s，终态 %d，余额 %d）"),
		Receipt.GoldAmount, *Receipt.SettlementId.ToString(), int32(Receipt.TerminalPhase), State.Gold);
	OnProfileChanged.Broadcast();
	return ELKSettlementResult::Success;
}

int32 ULKProfileSubsystem::GetDepartureGoldCap() const
{
	return LKHomeContent::TreasuryDepartureGoldCap(GetBuildingLevel(LKHomeContent::BuildingId(ELKHomeBuilding::Treasury)));
}

bool ULKProfileSubsystem::ReserveDepartureGold(FGuid RunId, int32 Amount)
{
	if (!RunId.IsValid() || Amount < 0 || !HasProfile() || !bProfileUsable) { return false; }
	if (State.FundedRunId == RunId) { return State.FundedRunGold == Amount; }
	if (State.FundedRunId.IsValid() || Amount > State.Gold || Amount > GetDepartureGoldCap()) { return false; }
	FLKProfileState Candidate = State;
	Candidate.Gold -= Amount; Candidate.FundedRunId = RunId; Candidate.FundedRunGold = Amount;
	if (!CommitProfile(Candidate)) { return false; }
	OnProfileChanged.Broadcast(); return true;
}

bool ULKProfileSubsystem::ReconcileDepartureGold(FGuid StoredRunId)
{
	if (!State.FundedRunId.IsValid() || State.FundedRunId == StoredRunId) { return true; }
	FLKProfileState Candidate = State;
	Candidate.Gold = int32(FMath::Min<int64>(MAX_int32, int64(Candidate.Gold) + Candidate.FundedRunGold));
	Candidate.FundedRunId.Invalidate(); Candidate.FundedRunGold = 0;
	if (!CommitProfile(Candidate)) { return false; }
	OnProfileChanged.Broadcast(); return true;
}

bool ULKProfileSubsystem::SetLastSelectedRegionId(FName RegionId)
{
	if (!EnsureProfile() || !HasProfile()) { return false; }
	if (State.LastSelectedRegionId == RegionId) { return true; }
	FLKProfileState Candidate = State;
	Candidate.LastSelectedRegionId = RegionId;
	return CommitProfile(MoveTemp(Candidate));
}

bool ULKProfileSubsystem::AddGold(int32 Amount)
{
	if (!EnsureProfile() || !HasProfile()) { return false; }
	if (!bProfileUsable) { return false; }
	FLKProfileState Candidate = State;
	Candidate.Gold = int32(FMath::Clamp<int64>(int64(Candidate.Gold) + Amount, 0, MAX_int32));
	if (!CommitProfile(MoveTemp(Candidate))) { return false; }
	UE_LOG(LogLK, Log, TEXT("[Home] 调试金币 %+d -> %d"), Amount, State.Gold);
	OnProfileChanged.Broadcast();
	return true;
}

bool ULKProfileSubsystem::ResetProfile()
{
	// 坏档保护：两个槽都存在但都不可读时拒绝覆盖（保留原文件，避免静默丢弃玩家进度）。
	const bool bAnyFileExists = UGameplayStatics::DoesSaveGameExist(GetStorageSlot(false), 0)
		|| UGameplayStatics::DoesSaveGameExist(GetStorageSlot(true), 0);
	if (bAnyFileExists && !bProfileUsable)
	{
		LastError = TEXT("永久档不可读：拒绝覆盖（原文件保留）");
		UE_LOG(LogLK, Warning, TEXT("[Home] %s"), *LastError);
		return false;
	}
	FLKProfileState Fresh = MakeDefaultProfile();
	if (!CommitProfile(MoveTemp(Fresh))) { return false; }
	UE_LOG(LogLK, Log, TEXT("[Home] 永久档已重置为默认（%s）"), *State.ProfileId.ToString());
	OnProfileChanged.Broadcast();
	return true;
}
