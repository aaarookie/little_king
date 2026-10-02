#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../LKHomeContent.h"
#include "../LKUndeadContent.h"
#include "../LKExpeditionMercenaryContent.h"
#include "../LKResearchContent.h"
#include "../LKV083Content.h"

namespace LKContentTest
{
/** Expected IDs come from each contributing catalog, so new rows do not require literal-count edits. */
inline TSet<FName> UnitIds()
{
    TSet<FName> Ids = { "Hero_Knight", "Hero_Mage", "Hero_Ranger", "Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer", "Building_ArrowTower", "Building_Barracks" };
    for (const auto& Pair : LKUndeadContent::Units()) { Ids.Add(Pair.Key); }
    for (const auto& D : LKExpeditionMercenaryContent::All()) { Ids.Add(D.Unit.UnitId); }
    for (const auto& Pair : LKV083Content::Units()) { Ids.Add(Pair.Key); }
    return Ids;
}
inline TSet<FName> CardIds()
{
    TSet<FName> Ids = { "Unit_Skeleton", "Unit_SkeletonArcher", "Spell_SkeletonCircle" };
    for (FName Id : LKHomeContent::DefaultUnlockedCards()) { Ids.Add(Id); }
    for (const auto& D : LKExpeditionMercenaryContent::All()) { Ids.Add(D.Unit.UnitId); }
    for (const auto& R : LKResearchContent::All()) { Ids.Add(R.CardId); }
    return Ids;
}
}
#endif
