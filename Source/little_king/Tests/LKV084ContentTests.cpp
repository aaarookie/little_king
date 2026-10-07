#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../LKV084Content.h"
#include "../LKUnitContent.h"
#include "../LKResearchContent.h"
#include "../LKCardRules.h"
#include "../LKCardPresentation.h"
#include "../LKHomeContent.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKProfileSubsystem.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
ULKCardDefinition* FindCard(ULKGameData& Data, FName Id)
{
    const auto* Entry = Data.CardLibrary.FindByPredicate([Id](const auto& C) { return C && C->CardId == Id; });
    return Entry ? Entry->Get() : nullptr;
}
struct FScopedProfilePersistence
{
    FScopedProfilePersistence() { ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true); }
    ~FScopedProfilePersistence() { ULKProfileSubsystem::ResetTestHooks(); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084BuildingCatalog, "LittleKing.V084.Content.BuildingCardsAndAliases", Flags)
bool FLKV084BuildingCatalog::RunTest(const FString&)
{
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
    TestEqual(TEXT("Seven new building units"), LKV084Content::Units().Num(), 7);
    TestEqual(TEXT("Seven new building cards"), LKV084Content::CardIds().Num(), 7);
    TSet<FName> Cards;
    for (const auto& C : Data->CardLibrary)
    { if (!TestNotNull(TEXT("No null definitions"), C.Get())) { return false; } TestFalse(TEXT("Unique card IDs"), Cards.Contains(C->CardId)); Cards.Add(C->CardId); }
    const int32 Count = Cards.Num(); Data->EnsureCardLibrary();
    TestEqual(TEXT("Repeated registration is idempotent"), Data->CardLibrary.Num(), Count);
    for (FName Id : LKV084Content::CardIds())
    {
        const auto* Card = FindCard(*Data, Id); const auto* Research = LKResearchContent::Find(Id);
        if (!TestNotNull(TEXT("Card registered"), Card) || !TestNotNull(TEXT("Blueprint registered"), Research)) { return false; }
        TestEqual(TEXT("Card uses building deployment"), Card->CardType, ELKCardType::Building);
        TestEqual(TEXT("Card resolves to explicit unit ID"), Card->BuildingUnitId, LKV084Content::UnitIdForCard(Id));
        TestEqual(TEXT("Building has a blueprint, not a book"), Research->Kind, ELKMarketOfferKind::BuildingBlueprint);
        TestEqual(TEXT("Deployment price agrees with catalog"), Card->Cost, Research->SilverCost);
        TestTrue(TEXT("Card alias works with rest upgrade building classification"), LKCardRules::IsBuilding(Id));
        TestTrue(TEXT("Unit and card resolve to the same stable row"), LKUnitContent::Find(Id) == LKUnitContent::Find(Card->BuildingUnitId));
        TestEqual(TEXT("Every new building consumes one deck slot"), LKCardRules::Slots(Id), 1);
        TestFalse(TEXT("No starter unlock"), LKHomeContent::DefaultUnlockedCards().Contains(Id));
        TestFalse(TEXT("No automatic insertion in starting deck"), Data->DefaultPlayerDeck.Contains(Id));
        TestFalse(TEXT("No temporary expedition reward"), LKCardRules::IsTemporaryMercenary(Id));
        TestFalse(TEXT("Permanent card survives expedition once researched"), Card->bExpeditionOnly);
        TestTrue(TEXT("Dedicated building illustration"), Card->Icon.ToSoftObjectPath().ToString().Contains(TEXT("/V084/Cards/T_Card_")));
    }
    TestTrue(TEXT("Unknown Card_ identities are not guessed as buildings"), LKV084Content::UnitIdForCard("Card_Unregistered").IsNone());
    TestNull(TEXT("Unknown alias remains unresolved"), LKUnitContent::Find("Card_Unregistered"));
    const auto* Cart = LKUnitContent::Find("Unit_SiegeCart");
    TestEqual(TEXT("Epic melee health baseline"), Cart->BaseHealth, 320.f);
    TestEqual(TEXT("Cart moves at one tenth of 300"), Cart->MoveSpeed, 30.f);
    TestEqual(TEXT("Cart attacks each three seconds"), Cart->AttackInterval, 3.f);
    TestTrue(TEXT("Cart closes to melee range instead of attacking across the map"), Cart->AttackRange < 200.f && Cart->AcquireRadius > 10000.f);
    for (FName Id : { FName("Unit_ElfPriest"), FName("Unit_AngelPriest_Rare"), FName("Unit_ChosenHighPriest") })
    { TestTrue(TEXT("Every shipped priest follows healing-only basic attack pipeline"), LKUnitContent::Find(Id)->bBasicAttackHeals); }
    for (FName Id : { FName("Hero_Mage"), FName("Hero_Necromancer"), FName("Unit_ApprenticeMage"), FName("Unit_TrollMage") })
    { TestTrue(TEXT("Every shipped mage is eligible for same-team mage tower selection"), LKUnitContent::Find(Id)->bIsMage); }
    TestFalse(TEXT("Elf priest is not also a mage"), LKUnitContent::Find("Unit_ElfPriest")->bIsMage);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084BuildingTuning, "LittleKing.V084.Content.AuthoredTuningAndStableIdentity", Flags)
bool FLKV084BuildingTuning::RunTest(const FString&)
{
    ULKCardDefinition* Shared = NewObject<ULKCardDefinition>();
    Shared->CardId = "Card_WallEpic"; Shared->CardType = ELKCardType::Spell; Shared->Cost = 9;
    Shared->BuildingTypeLimitOverride = 12; Shared->BuildingUnitId = "Unit_Swordsman";
    ULKGameData* Data = NewObject<ULKGameData>(); Data->CardLibrary.Add(Shared); Data->EnsureCardLibrary();
    const auto* Card = FindCard(*Data, Shared->CardId);
    if (!TestNotNull(TEXT("Private runtime card"), Card)) { return false; }
    TestTrue(TEXT("Shared authored asset unchanged"), Card != Shared && Shared->CardType == ELKCardType::Spell && Shared->BuildingUnitId == "Unit_Swordsman");
    TestEqual(TEXT("Building identity corrected"), Card->BuildingUnitId, FName("Unit_WallEpic"));
    TestEqual(TEXT("Custom silver cost preserved"), Card->Cost, 9);
    TestEqual(TEXT("Custom per-type count preserved"), Card->BuildingTypeLimitOverride, 12);
    const auto& Canonical = *LKUnitContent::Find("Unit_WallEpic");
    FLKUnitRow Authored = Canonical;
    Authored.Quality = ELKQuality::Common; Authored.BuildingBehavior = ELKBuildingBehavior::Turret;
    Authored.BaseHealth = 12345.f; Authored.ConstructionSeconds = 7.f; Authored.WallLength = 450.f;
    Authored.WallThickness = 48.f; Authored.ReflectDamage = 18.f; Authored.WallAuraRange = 420.f;
    auto Merged = LKUnitContent::MergeAuthoredTuning(Canonical, &Authored);
    TestEqual(TEXT("Wall behavior cannot drift"), Merged.BuildingBehavior, ELKBuildingBehavior::Wall);
    TestEqual(TEXT("Repair tier follows canonical rarity"), Merged.Quality, ELKQuality::Epic);
    TestEqual(TEXT("Author can tune health"), Merged.BaseHealth, 12345.f);
    TestEqual(TEXT("Author can tune duration"), Merged.ConstructionSeconds, 7.f);
    TestEqual(TEXT("Author can tune physical length"), Merged.WallLength, 450.f);
    TestEqual(TEXT("Author can tune physical thickness"), Merged.WallThickness, 48.f);
    TestEqual(TEXT("Author can tune reflection"), Merged.ReflectDamage, 18.f);
    TestEqual(TEXT("Author can tune aura range"), Merged.WallAuraRange, 420.f);
    Authored.WallLength = 0.f; Authored.WallThickness = -1.f; Authored.ConstructionSeconds = std::numeric_limits<float>::quiet_NaN();
    Authored.ReflectDamage = std::numeric_limits<float>::infinity(); Authored.WallAuraRange = 0.f;
    Merged = LKUnitContent::MergeAuthoredTuning(Canonical, &Authored);
    TestEqual(TEXT("Zero length falls back to canonical wall"), Merged.WallLength, Canonical.WallLength);
    TestEqual(TEXT("Negative thickness falls back to canonical wall"), Merged.WallThickness, Canonical.WallThickness);
    TestEqual(TEXT("Invalid duration cannot stall construction"), Merged.ConstructionSeconds, Canonical.ConstructionSeconds);
    TestEqual(TEXT("Infinite reflection cannot enter combat"), Merged.ReflectDamage, Canonical.ReflectDamage);
    TestEqual(TEXT("Missing range receives canonical value"), Merged.WallAuraRange, Canonical.WallAuraRange);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084BlueprintResearch, "LittleKing.V084.Research.PermanentUnlockAndRestClassification", Flags)
bool FLKV084BlueprintResearch::RunTest(const FString&)
{
    FScopedProfilePersistence Persistence;
    UGameInstance* Instance = NewObject<UGameInstance>(); auto* Profile = NewObject<ULKProfileSubsystem>(Instance);
    Profile->ConfigureStorage(TEXT("LittleKing_V084_Content_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    Profile->SetAutoSaveEnabled(false);
    if (!TestTrue(TEXT("Isolated in-memory profile"), Profile->EnsureProfile())) { return false; }
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
    auto Loadout = LKHomeContent::DefaultLoadout(); Loadout.CardIds[0] = "Card_WallCommon";
    TestEqual(TEXT("Cannot equip unresearched wall"), Profile->SaveLoadout(Loadout, Data), ELKLoadoutResult::LockedCard);
    FString Error;
    TestFalse(TEXT("Blueprint required before research"), Profile->ResearchCard("Card_WallCommon", Error));
    auto State = Profile->GetProfile();
    for (FName Id : LKV084Content::CardIds()) { State.ResearchMaterials.Add(Id, 1); }
    Profile->SetProfileForTest(State);
    for (FName Id : LKV084Content::CardIds())
    {
        TestTrue(TEXT("Research accepts new blueprint"), Profile->ResearchCard(Id, Error));
        TestTrue(TEXT("Research creates permanent unlock"), Profile->IsCardUnlocked(Id));
        TestFalse(TEXT("Blueprint consumed exactly once"), Profile->GetProfile().ResearchMaterials.Contains(Id));
        TestFalse(TEXT("Duplicate research rejected"), Profile->ResearchCard(Id, Error));
        Loadout.CardIds[0] = Id;
        TestEqual(TEXT("Researched building can join departure deck"), Profile->SaveLoadout(Loadout, Data), ELKLoadoutResult::Success);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084BuildingMarketAndUI, "LittleKing.V084.Content.BlueprintMarketsAndCardRules", Flags)
bool FLKV084BuildingMarketAndUI::RunTest(const FString&)
{
    int32 NewBlueprints = 0, EarlyWalls = 0;
    for (int32 Seed = 0; Seed < 20000; ++Seed)
    {
        FLKDungeonNode Node; Node.NodeId = "V084_Market"; Node.Type = ELKDungeonNodeType::Market;
        const int32 Depth = Seed % 5; LKResearchContent::GenerateMarket(Node, Seed, Depth);
        const auto Frozen = Node.MarketOffers; LKResearchContent::GenerateMarket(Node, Seed + 17, 4);
        TestEqual(TEXT("Reopening market cannot refresh the offer list"), Node.MarketOffers.Num(), Frozen.Num());
        for (int32 I = 0; I < Frozen.Num(); ++I)
        {
            const auto& Offer = Node.MarketOffers[I];
            TestEqual(TEXT("Existing offer identity frozen"), Offer.OfferId, Frozen[I].OfferId);
            if (Offer.Kind == ELKMarketOfferKind::Mercenary)
            { TestFalse(TEXT("Building never sold as temporary mercenary"), LKV084Content::CardIds().Contains(Offer.CardId)); continue; }
            const auto* R = LKResearchContent::Find(Offer.CardId);
            if (!TestNotNull(TEXT("Market material has research definition"), R)) { return false; }
            TestTrue(TEXT("Blueprint region gate respected"), Depth >= R->MinimumRegionDepth);
            if (LKV084Content::CardIds().Contains(Offer.CardId))
            { ++NewBlueprints; TestEqual(TEXT("New building material is a blueprint"), Offer.Kind, ELKMarketOfferKind::BuildingBlueprint); if (Depth == 0) { ++EarlyWalls; TestEqual(TEXT("Only common wall can appear in first region"), Offer.CardId, FName("Card_WallCommon")); } }
        }
    }
    TestTrue(TEXT("New blueprints enter very rare market pool"), NewBlueprints > 0 && NewBlueprints < 200);
    TestTrue(TEXT("First region can offer basic wall blueprint"), EarlyWalls > 0);
    const auto* Common = LKResearchContent::Find("Card_WallCommon"); const auto* Imperial = LKResearchContent::Find("Card_WallImperial");
    TestTrue(TEXT("Legendary blueprint requires later region, higher price, lower weight"), Imperial->MinimumRegionDepth > Common->MinimumRegionDepth && Imperial->Price > Common->Price && Imperial->Weight < Common->Weight);
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureCardLibrary();
    auto Detail = [Data](FName Id, int32 Level = 0) { return LKCardPresentation::Detail(*FindCard(*Data, Id), nullptr, Level).ToString(); };
    TestTrue(TEXT("Wall shows scaled HP"), Detail("Card_WallCommon", 1).Contains(TEXT("1100")));
    TestTrue(TEXT("Wall shows construction duration"), Detail("Card_WallCommon").Contains(TEXT("施工 10 秒")));
    TestTrue(TEXT("Epic wall explains per-source reflection cooldown"), Detail("Card_WallEpic").Contains(TEXT("每个来源冷却 0.5 秒")));
    TestTrue(TEXT("Cart explains double damage to walls"), Detail("Card_SiegeCart").Contains(TEXT("防御墙 20%")));
    TestFalse(TEXT("Cart does not claim battlefield-wide attacks"), Detail("Card_SiegeCart").Contains(TEXT("全图")));
    TestTrue(TEXT("Mage tower explains skill cooldown benefit"), Detail("Card_MageTower").Contains(TEXT("冷却缩短 50%")));
    TestTrue(TEXT("Station explains full-health release"), Detail("Card_HealingStation").Contains(TEXT("满血归队")));
    TestTrue(TEXT("Elf priest UI explicitly excludes damaging enemies"), Detail("Unit_ElfPriest").Contains(TEXT("不攻击敌人")));
    AddInfo(FString::Printf(TEXT("20,000 frozen markets: %d new blueprints, %d basic wall offers in first region"), NewBlueprints, EarlyWalls));
    return true;
}
#endif
