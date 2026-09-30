#include "LKCardRules.h"
#include "LKUnitContent.h"
#include "LKRunTypes.h"
#include "ULKCardDefinition.h"
#include "LKExpeditionMercenaryContent.h"

namespace
{
	struct FCanonicalFaction
	{
		FName CardId;
		ELKCardFaction Faction;
	};

	/** 规范身份表：与 docs/48 第 7 节一致；新增敌方专属卡只在此登记。 */
	const TArray<FCanonicalFaction>& CanonicalFactions()
	{
		static const TArray<FCanonicalFaction> Rows = {
			{ "Unit_Skeleton", ELKCardFaction::EnemyOnly },
			{ "Unit_SkeletonArcher", ELKCardFaction::EnemyOnly },
			{ "Spell_SkeletonCircle", ELKCardFaction::EnemyOnly },
		};
		return Rows;
	}
}

namespace LKCardRules
{
int32 Slots(FName CardId)
{
    const FLKUnitRow* Row = LKUnitContent::Find(CardId);
    return Row ? FMath::Clamp(Row->DeckSlots, 1, 64) : 1;
}
int32 Used(const TArray<FName>& Cards)
{
    int32 Result = 0;
    for (FName Id : Cards) { Result += Slots(Id); }
    return Result;
}
int32 Used(const TArray<FLKRunCardState>& Cards)
{
    int32 Result = 0;
    for (const FLKRunCardState& Card : Cards) { Result += Slots(Card.CardId); }
    return Result;
}

float UpgradeMultiplier(int32 Level) { return FMath::Pow(1.1f, FMath::Clamp(Level,0,50)); }
bool IsSpell(FName Id) { return Id.ToString().StartsWith(TEXT("Spell_")); }
bool IsBuilding(FName Id) { const FLKUnitRow* R=LKUnitContent::Find(Id); return R && R->UnitClass==ELKUnitClass::Building; }
bool IsTemporaryMercenary(FName Id)
{
    for (const auto& D : LKExpeditionMercenaryContent::All())
    { if (D.Unit.UnitId==Id && D.Unit.UnitClass==ELKUnitClass::Soldier) { return true; } }
    return false;
}
bool ValidateReplacement(const TArray<FLKRunCardState>& Cards, FName Incoming,
    const TArray<FName>& Removed, int32 MinSlots, int32 MaxSlots, FString& Error)
{
    Error.Reset();
    if (MinSlots<1 || MaxSlots<MinSlots || MaxSlots>64) { Error=TEXT("容量规则无效"); return false; }
    TSet<FName> Seen;
    int32 Released=0;
    for (FName Id : Removed)
    {
        if (Id.IsNone() || Seen.Contains(Id) || !Cards.ContainsByPredicate([Id](const auto& C){return C.CardId==Id;}))
        { Error=TEXT("弃牌不在当前牌组中或重复"); return false; }
        Seen.Add(Id); Released+=Slots(Id);
    }
    const int32 Added=Incoming.IsNone()?0:1;
    const int32 Total=Used(Cards)+(Added?Slots(Incoming):0);
    if (Cards.Num()+Added-Removed.Num()<MinimumCards) { Error=TEXT("循环牌组至少保留五种卡"); return false; }
    if (MinSlots<MaxSlots)
    {
        if (Total-Released<MinSlots || Total-Released>MaxSlots) { Error=TEXT("替换后容量必须位于当前合法区间"); return false; }
        return true;
    }
    const int32 Required=FMath::Max(0,Total-MaxSlots);
    // Bounded subset DP; keep the minimum discarded-card count for each released budget.
    const int32 Budget=Used(Cards), MaxDiscard=Cards.Num()+Added-MinimumCards;
    TArray<int32> Best; Best.Init(MAX_int32,Budget+1); Best[0]=0;
    for (const auto& C : Cards)
    {
        const int32 W=Slots(C.CardId);
        for (int32 S=Budget; S>=W; --S)
        { if (Best[S-W]!=MAX_int32) { Best[S]=FMath::Min(Best[S],Best[S-W]+1); } }
    }
    for (int32 S=Required; S<=Budget; ++S)
    {
        if (Best[S]<=MaxDiscard)
        {
            if (Released==S && Removed.Num()==Best[S]) { return true; }
            Error=FString::Printf(TEXT("本次只需弃 %d 格、%d 张；请选择最少必要弃牌"),S,Best[S]); return false;
        }
    }
    Error=TEXT("无法同时满足容量与五种循环卡要求"); return false;
}

ELKCardFaction FactionOf(FName CardId)
{
    for (const FCanonicalFaction& Row : CanonicalFactions())
    {
        if (Row.CardId == CardId) { return Row.Faction; }
    }
    return ELKCardFaction::Both;
}

bool IsEnemyOnly(FName CardId) { return FactionOf(CardId) == ELKCardFaction::EnemyOnly; }
bool IsPlayerObtainable(FName CardId) { return FactionOf(CardId) != ELKCardFaction::EnemyOnly; }
bool IsEnemyUsable(FName CardId) { return FactionOf(CardId) != ELKCardFaction::PlayerOnly; }

const TArray<FName>& EnemyOnlyCardIds()
{
    static const TArray<FName> Ids = [] {
        TArray<FName> Result;
        for (const FCanonicalFaction& Row : CanonicalFactions())
        {
            if (Row.Faction == ELKCardFaction::EnemyOnly) { Result.Add(Row.CardId); }
        }
        return Result;
    }();
    return Ids;
}

const TArray<FName>& ReplacementCardIds()
{
    static const TArray<FName> Ids = {
        "Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer",
        "Spell_Fireball", "Spell_HealWave",
        "Building_ArrowTower", "Building_Barracks"
    };
    return Ids;
}

void ApplyCanonicalFaction(ULKCardDefinition& Card)
{
    for (const FCanonicalFaction& Row : CanonicalFactions())
    {
        if (Card.CardId == Row.CardId) { Card.Faction = Row.Faction; return; }
    }
}

void FilterPlayerObtainable(const TArray<FName>& In, TArray<FName>& Out)
{
    Out.Reset();
    TSet<FName> Seen;
    for (FName Id : In)
    {
        if (Id.IsNone() || Seen.Contains(Id) || !IsPlayerObtainable(Id)) { continue; }
        Seen.Add(Id);
        Out.Add(Id);
    }
}
}
