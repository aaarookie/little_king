#include "LKEncounterContent.h"

#include "Engine/DataTable.h"
#include "LKBalanceRules.h"
#include "LKCardRules.h"
#include "LKUnitContent.h"
#include "ULKCardDefinition.h"
#include "ULKGameData.h"

namespace
{
	constexpr int32 MaxWaveCount = 64;
	constexpr int32 MaxUnitsPerWave = 50;

	FLKWaveEntry Wave(float Time, FName UnitId, int32 Count)
	{
		FLKWaveEntry Entry;
		Entry.Time = Time;
		Entry.UnitId = UnitId;
		Entry.Count = Count;
		return Entry;
	}

	FLKEncounterRow Encounter(FName Id, const TCHAR* Name, ELKEncounterRank Rank, int32 RewardTier,
		TArray<FName> Heroes, bool bFocus, float FocusMin, float FocusMax, float FocusDuration)
	{
		FLKEncounterRow Row;
		Row.EncounterId = Id;
		Row.DisplayName = FText::FromString(Name);
		Row.Rank = Rank;
		Row.RewardTier = RewardTier;
		Row.EnemyHeroIds = MoveTemp(Heroes);
		// 有限密集增援：初始 4 近战 + 2 远程，之后按遭遇强度每 8/7/6 秒一波，直到 176/175/180 秒。
		LKBalanceRules::BuildReinforcementWaves(Rank, Row.Waves);
		Row.bEnemyUsesCards = false;
		Row.AI.bEnableFocus = bFocus;
		Row.AI.bEnableCounter = false;
		Row.AI.bEnablePush = false;
		Row.AI.FocusIntervalMin = FocusMin;
		Row.AI.FocusIntervalMax = FocusMax;
		Row.AI.FocusDuration = FocusDuration;
		// 战术法术经济与波次预算分开：波次不扣银币，法术独立扣费。
		Row.EnemySilverPerSecond = LKBalanceRules::EnemySilverRate(Rank);
		Row.EnemySilverCap = 10.f;
		Row.EnemyStartingSilver = 0.f;
		Row.EnemySpell.Cooldown = LKBalanceRules::CircleCooldown(Rank);
		Row.EnemySpell.bEnabled = true;
		return Row;
	}

	bool IsFiniteNonNegative(float Value) { return FMath::IsFinite(Value) && Value >= 0.f; }
	bool IsFinitePositive(float Value) { return FMath::IsFinite(Value) && Value > 0.f; }
}

namespace LKEncounterContent
{
const TMap<FName, FLKEncounterRow>& BuiltInEncounters()
{
	static const TMap<FName, FLKEncounterRow> Rows = []
	{
		TMap<FName, FLKEncounterRow> Result;
		FLKEncounterRow Patrol = Encounter("Encounter_UndeadPatrol", TEXT("亡灵巡逻"), ELKEncounterRank::Normal, 1,
			{ "Hero_Necromancer" },
			false, 35.f, 42.f, 5.f);
		Result.Add(Patrol.EncounterId, MoveTemp(Patrol));

		FLKEncounterRow Elite = Encounter("Encounter_UndeadElite", TEXT("巨骨卫队"), ELKEncounterRank::Elite, 2,
			{ "Hero_Necromancer", "Hero_SkeletonGiant" },
			true, 28.f, 36.f, 6.f);
		Elite.AI.FocusWarningSeconds = 2.5f;
		Result.Add(Elite.EncounterId, MoveTemp(Elite));

		FLKEncounterRow Boss = Encounter("Encounter_SkeletonKing", TEXT("骷髅王座"), ELKEncounterRank::Boss, 3,
			{ "Boss_SkeletonKing", "Hero_Necromancer" },
			true, 22.f, 30.f, 7.f);
		Boss.AI.FocusWarningSeconds = 2.f;
		Result.Add(Boss.EncounterId, MoveTemp(Boss));
		return Result;
	}();
	return Rows;
}

FName CanonicalId(FName EncounterId)
{
	if (EncounterId == "Patrol") { return "Encounter_UndeadPatrol"; }
	if (EncounterId == "Elite") { return "Encounter_UndeadElite"; }
	if (EncounterId == "Boss") { return "Encounter_SkeletonKing"; }
	return EncounterId;
}

const FLKEncounterRow* FindBuiltIn(FName EncounterId)
{
	return BuiltInEncounters().Find(CanonicalId(EncounterId));
}

bool NormalizeAndValidate(FName RowName, const FLKEncounterRow& Source, FLKEncounterRow& OutRow, FString& OutError)
{
	OutError.Reset();
	OutRow = Source;
	if (!RowName.IsNone())
	{
		if (!OutRow.EncounterId.IsNone() && OutRow.EncounterId != RowName)
		{
			OutError = FString::Printf(TEXT("行 %s 的 EncounterId=%s 与行名不一致"), *RowName.ToString(), *OutRow.EncounterId.ToString());
			return false;
		}
		OutRow.EncounterId = RowName;
	}
	OutRow.EncounterId = CanonicalId(OutRow.EncounterId);
	if (OutRow.EncounterId.IsNone()) { OutError = TEXT("EncounterId 不能为空"); return false; }
	if (OutRow.RewardTier < 1 || OutRow.RewardTier > 100) { OutError = TEXT("RewardTier 必须在 1～100"); return false; }
	if (OutRow.EnemyHeroIds.IsEmpty()) { OutError = TEXT("EnemyHeroIds 至少需要一个英雄或首领"); return false; }
	TSet<FName> Heroes;
	for (FName HeroId : OutRow.EnemyHeroIds)
	{
		if (HeroId.IsNone() || Heroes.Contains(HeroId)) { OutError = TEXT("EnemyHeroIds 包含空值或重复 ID"); return false; }
		Heroes.Add(HeroId);
	}
	if (OutRow.Waves.IsEmpty() || OutRow.Waves.Num() > MaxWaveCount) { OutError = TEXT("Waves 数量必须在 1～64"); return false; }
	for (const FLKWaveEntry& Entry : OutRow.Waves)
	{
		if (!IsFiniteNonNegative(Entry.Time) || Entry.UnitId.IsNone() || Entry.Count < 1 || Entry.Count > MaxUnitsPerWave)
		{ OutError = TEXT("Waves 含非法时间、单位 ID 或数量（单波 1～50）"); return false; }
	}
	OutRow.Waves.StableSort([](const FLKWaveEntry& A, const FLKWaveEntry& B) { return A.Time < B.Time; });
	TSet<FName> Cards;
	for (FName CardId : OutRow.EnemyCards)
	{
		if (CardId.IsNone() || Cards.Contains(CardId)) { OutError = TEXT("EnemyCards 包含空值或重复 CardId"); return false; }
		Cards.Add(CardId);
	}
	if (OutRow.bEnemyUsesCards && OutRow.EnemyCards.IsEmpty()) { OutError = TEXT("启用敌方出牌时 EnemyCards 不能为空"); return false; }
	if (!IsFiniteNonNegative(OutRow.EnemySilverPerSecond) || !IsFinitePositive(OutRow.EnemySilverCap)
		|| !IsFiniteNonNegative(OutRow.EnemyStartingSilver) || OutRow.EnemyStartingSilver > OutRow.EnemySilverCap)
	{ OutError = TEXT("敌方银币初值、产速或上限非法"); return false; }

	const FLKEncounterAISettings& AI = OutRow.AI;
	if (!IsFiniteNonNegative(AI.FocusWarningSeconds) || !IsFinitePositive(AI.FocusIntervalMin)
		|| !IsFinitePositive(AI.FocusIntervalMax) || AI.FocusIntervalMin > AI.FocusIntervalMax
		|| !IsFinitePositive(AI.FocusDuration) || !IsFinitePositive(AI.CounterCheckInterval)
		|| !IsFinitePositive(AI.PushSilverThreshold) || AI.PushMaxCards < 1 || AI.PushMaxCards > 5
		|| !IsFinitePositive(AI.PushCooldownMin) || !IsFinitePositive(AI.PushCooldownMax)
		|| AI.PushCooldownMin > AI.PushCooldownMax || !IsFiniteNonNegative(AI.PushReserveMaxSeconds))
	{ OutError = TEXT("AI 参数含非有限值、非正值或 Min > Max"); return false; }

	// 敌方战术法术（独立通道）：参数必须有限且在合理范围；卡牌解析由 BuildValidatedCatalog 负责。
	const FLKEnemySpellSettings& Spell = OutRow.EnemySpell;
	if (!IsFiniteNonNegative(Spell.FirstCastTime) || !IsFinitePositive(Spell.CheckInterval)
		|| !IsFiniteNonNegative(Spell.Cooldown) || Spell.MaxActive < 0 || Spell.MaxActive > 4
		|| Spell.PreferredTargets < 1 || Spell.PreferredTargets > 10 || !IsFinitePositive(Spell.StopTime))
	{ OutError = TEXT("敌方战术法术参数非法"); return false; }
	if (Spell.bEnabled && Spell.SpellId.IsNone()) { OutError = TEXT("启用敌方战术法术时 SpellId 不能为空"); return false; }

	// 平衡快照：缺失（BalanceVersion=0）时按中性深度补齐；已写入的快照必须自洽。
	if (OutRow.BalanceVersion < 0 || OutRow.BalanceVersion > LKBalanceRules::CurrentBalanceVersion)
	{ OutError = TEXT("不支持的平衡快照版本"); return false; }
	if (OutRow.BalanceVersion == 0) { LKBalanceRules::EnsureSnapshot(OutRow); }
	if (!LKBalanceRules::ValidateSnapshot(OutRow, OutError)) { return false; }
	return true;
}

bool BuildCatalog(const UDataTable* Table, TArray<FLKEncounterRow>& OutRows, FString& OutError)
{
	OutRows.Reset();
	OutError.Reset();
	if (!Table)
	{
		for (const FName Id : { FName("Encounter_UndeadPatrol"), FName("Encounter_UndeadElite"), FName("Encounter_SkeletonKing") })
		{ OutRows.Add(BuiltInEncounters().FindChecked(Id)); }
		return true;
	}
	if (Table->GetRowStruct() != FLKEncounterRow::StaticStruct())
	{
		OutError = TEXT("DT_Encounters 必须使用 LKEncounterRow");
		return false;
	}
	TSet<FName> Seen;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		FLKEncounterRow Row;
		if (!NormalizeAndValidate(Pair.Key, *reinterpret_cast<const FLKEncounterRow*>(Pair.Value), Row, OutError)) { return false; }
		if (Seen.Contains(Row.EncounterId)) { OutError = FString::Printf(TEXT("重复 EncounterId %s"), *Row.EncounterId.ToString()); return false; }
		Seen.Add(Row.EncounterId);
		OutRows.Add(MoveTemp(Row));
	}
	for (const FName Required : { FName("Encounter_UndeadPatrol"), FName("Encounter_UndeadElite"), FName("Encounter_SkeletonKing") })
	{
		if (!Seen.Contains(Required)) { OutError = FString::Printf(TEXT("DT_Encounters 缺少固定路线行 %s"), *Required.ToString()); OutRows.Reset(); return false; }
	}
	OutRows.StableSort([](const FLKEncounterRow& A, const FLKEncounterRow& B) { return A.EncounterId.LexicalLess(B.EncounterId); });
	return true;
}

const FLKEncounterRow* Find(const TArray<FLKEncounterRow>& Rows, FName EncounterId)
{
	const FName Canonical = CanonicalId(EncounterId);
	return Rows.FindByPredicate([Canonical](const FLKEncounterRow& Row) { return Row.EncounterId == Canonical; });
}

bool BuildValidatedCatalog(const ULKGameData* Data,
	TFunctionRef<const FLKUnitRow*(FName)> ResolveUnitRow,
	TFunctionRef<const ULKCardDefinition*(FName)> ResolveCard,
	TArray<FLKEncounterRow>& OutRows, FString& OutError)
{
	OutRows.Reset();
	OutError.Reset();
	if (!Data) { OutError = TEXT("GameData 为空"); return false; }

	UDataTable* Table = nullptr;
	if (!Data->EncounterTable.IsNull())
	{
		Table = Data->EncounterTable.LoadSynchronous();
		if (!Table) { OutError = TEXT("EncounterTable 资源加载失败"); return false; }
	}
	if (!BuildCatalog(Table, OutRows, OutError)) { return false; }

	for (const FLKEncounterRow& Encounter : OutRows)
	{
		for (FName HeroId : Encounter.EnemyHeroIds)
		{
			const FLKUnitRow* Row = ResolveUnitRow(HeroId);
			if (!Row || (Row->UnitClass != ELKUnitClass::Hero && Row->UnitClass != ELKUnitClass::Boss))
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的英雄 %s 不存在或类别错误"),
					*Encounter.EncounterId.ToString(), *HeroId.ToString());
				OutRows.Reset();
				return false;
			}
		}
		for (const FLKWaveEntry& Wave : Encounter.Waves)
		{
			const FLKUnitRow* Row = ResolveUnitRow(Wave.UnitId);
			if (!Row || Row->UnitClass == ELKUnitClass::Hero || Row->UnitClass == ELKUnitClass::Boss)
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的波次单位 %s 不存在或类别错误"),
					*Encounter.EncounterId.ToString(), *Wave.UnitId.ToString());
				OutRows.Reset();
				return false;
			}
		}
		if (Encounter.bEnemyUsesCards && Encounter.EnemyCards.Num() < Data->HandSize + 1)
		{
			OutError = FString::Printf(TEXT("遭遇 %s 的敌方牌组少于 HandSize + 1"), *Encounter.EncounterId.ToString());
			OutRows.Reset();
			return false;
		}
		for (FName CardId : Encounter.EnemyCards)
		{
			if (!ResolveCard(CardId))
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的敌方卡牌 %s 不存在"),
					*Encounter.EncounterId.ToString(), *CardId.ToString());
				OutRows.Reset();
				return false;
			}
		}
		// 敌方战术法术必须解析到"敌方专属 + 区域召唤"的卡牌定义。
		if (Encounter.EnemySpell.bEnabled)
		{
			const ULKCardDefinition* Spell = ResolveCard(Encounter.EnemySpell.SpellId);
			if (!Spell || Spell->CardType != ELKCardType::Spell || Spell->SpellEffect != ELKSpellEffect::SummonZone)
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的战术法术 %s 缺失或不是区域召唤法术"),
					*Encounter.EncounterId.ToString(), *Encounter.EnemySpell.SpellId.ToString());
				OutRows.Reset();
				return false;
			}
			if (LKCardRules::IsPlayerObtainable(Encounter.EnemySpell.SpellId))
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的战术法术 %s 不是敌方专属（阵营权限不一致）"),
					*Encounter.EncounterId.ToString(), *Encounter.EnemySpell.SpellId.ToString());
				OutRows.Reset();
				return false;
			}
		}
	}
	return true;
}

void CollectRosterPools(const TArray<FLKEncounterRow>& Catalog, TArray<FName>& OutHeroPool, TArray<FName>& OutBossPool)
{
	OutHeroPool.Reset();
	OutBossPool.Reset();
	TSet<FName> Heroes;
	TSet<FName> Bosses;
	for (const FLKEncounterRow& Row : Catalog)
	{
		for (int32 Index = 0; Index < Row.EnemyHeroIds.Num(); ++Index)
		{
			const FName HeroId = Row.EnemyHeroIds[Index];
			if (HeroId.IsNone()) { continue; }
			// 首领判定：单位注册表类别为 Boss，或（自定义单位）担任 Boss 行首位。
			const FLKUnitRow* Unit = LKUnitContent::Find(HeroId);
			const bool bIsBossUnit = Unit && Unit->UnitClass == ELKUnitClass::Boss;
			const bool bLeadsBossRow = Row.Rank == ELKEncounterRank::Boss && Index == 0;
			if (bIsBossUnit || bLeadsBossRow) { Bosses.Add(HeroId); }
			else { Heroes.Add(HeroId); }
		}
	}
	OutHeroPool = Heroes.Array();
	OutHeroPool.Sort(FNameLexicalLess());
	OutBossPool = Bosses.Array();
	OutBossPool.Sort(FNameLexicalLess());
}

bool MakeDynamicEncounter(FName NewEncounterId, const FLKEncounterRow& Template, ELKEncounterRank Rank,
	const TArray<FName>& HeroPool, const TArray<FName>& BossPool,
	FRandomStream& Stream, FLKEncounterRow& OutRow, FString& OutError)
{
	OutError.Reset();
	OutRow = Template;
	OutRow.EncounterId = NewEncounterId;
	OutRow.Rank = Rank;

	// 英雄池洗牌后取前 N（确定性：同一 Stream 同结果）
	TArray<FName> Heroes = HeroPool;
	for (int32 i = Heroes.Num() - 1; i > 0; --i) { Heroes.Swap(i, Stream.RandRange(0, i)); }
	TArray<FName> Bosses = BossPool;
	for (int32 i = Bosses.Num() - 1; i > 0; --i) { Bosses.Swap(i, Stream.RandRange(0, i)); }

	TArray<FName> Roster;
	switch (Rank)
	{
	case ELKEncounterRank::Normal:
		Roster.Add(Heroes.Num() > 0 ? Heroes[0] : NAME_None);
		break;
	case ELKEncounterRank::Elite:
		for (int32 i = 0; i < FMath::Min(2, Heroes.Num()); ++i) { Roster.Add(Heroes[i]); }
		break;
	case ELKEncounterRank::Boss:
		if (Bosses.Num() > 0) { Roster.Add(Bosses[0]); }
		for (int32 i = 0; i < FMath::Min(2, Heroes.Num()); ++i) { Roster.Add(Heroes[i]); }
		break;
	default:
		OutError = TEXT("未知遭遇强度");
		return false;
	}

	Roster.Remove(NAME_None);
	// 首领与英雄池理论上不重叠；防御性去重（Boss 行若也把 Boss 放进了英雄池）
	TSet<FName> Seen;
	TArray<FName> Unique;
	for (FName Id : Roster)
	{
		if (Id.IsNone() || Seen.Contains(Id)) { continue; }
		Seen.Add(Id);
		Unique.Add(Id);
	}
	OutRow.EnemyHeroIds = MoveTemp(Unique);

	if (OutRow.EnemyHeroIds.IsEmpty())
	{
		OutError = TEXT("敌阵容池为空，无法生成动态遭遇");
		return false;
	}
	// A generated roster needs a fresh budget; persisted snapshots are validated without repair.
	LKBalanceRules::ApplySnapshot(OutRow, OutRow.Depth);
	return NormalizeAndValidate(NewEncounterId, OutRow, OutRow, OutError);
}
}
