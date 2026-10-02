#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../LKV083Content.h"
#include "../LKUnitContent.h"
#include "../LKResearchContent.h"
#include "../LKCardRules.h"
#include "../LKCardPresentation.h"
#include "../LKHomeContent.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKProfileSubsystem.h"

namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FScopedProfilePersistence
{
    FScopedProfilePersistence() { ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true); }
    ~FScopedProfilePersistence() { ULKProfileSubsystem::ResetTestHooks(); }
};
ULKCardDefinition* FindCard(ULKGameData& Data, FName Id)
{
    const auto* Entry = Data.CardLibrary.FindByPredicate([Id](const auto& Card) { return Card && Card->CardId == Id; });
    return Entry ? Entry->Get() : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083Catalog, "LittleKing.V083.Content.SummonedRolesAndSpellDefinitions", Flags)
bool FLKV083Catalog::RunTest(const FString&)
{
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
    TestEqual(TEXT("Eleven spell-only summoned roles"), LKV083Content::Units().Num(), 11);
    TestEqual(TEXT("Nine new spell identities"), LKV083Content::SpellIds().Num(), 9);
    TSet<FName> Cards;
    for (const auto& Card : Data->CardLibrary)
    { if (!TestNotNull(TEXT("No null cards"), Card.Get())) { return false; } TestFalse(TEXT("Unique card identity"), Cards.Contains(Card->CardId)); Cards.Add(Card->CardId); }
    const int32 CardCount = Data->CardLibrary.Num();
    Data->EnsureCardLibrary(); TestEqual(TEXT("Registration remains idempotent"), Data->CardLibrary.Num(), CardCount);
    for (const auto& Pair : LKV083Content::Units())
    {
        const auto* Row = LKUnitContent::Find(Pair.Key);
        if (!TestNotNull(TEXT("Summoned unit resolves natively"), Row)) { return false; }
        TestEqual(TEXT("Summoned angel is not a hero"), Row->UnitClass, ELKUnitClass::Soldier);
        TestEqual(TEXT("Angel race"), Row->Race, ELKRace::Angel);
        TestEqual(TEXT("No passive ability"), Row->PassiveAbility, ELKPassiveAbility::None);
        TestEqual(TEXT("No active ability"), Row->ActiveAbility, ELKActiveAbility::None);
        TestTrue(TEXT("No traits"), Row->HeroTraits.IsEmpty());
        TestNull(TEXT("No independently obtainable unit card"), FindCard(*Data, Pair.Key));
        TestFalse(TEXT("No expedition mercenary reward identity"), LKCardRules::IsTemporaryMercenary(Pair.Key));
        TestFalse(TEXT("No default permanent unlock"), LKHomeContent::DefaultUnlockedCards().Contains(Pair.Key));
    }
    for (FName Id : LKV083Content::SpellIds())
    {
        const auto* Card = FindCard(*Data, Id);
        const auto* Research = LKResearchContent::Find(Id);
        if (!TestNotNull(TEXT("Spell has a library definition"), Card) || !TestNotNull(TEXT("Spell requires a book"), Research)) { return false; }
        TestEqual(TEXT("Research grade matches runtime"), Card->SpellGrade, Research->Grade);
        TestEqual(TEXT("Research silver price matches runtime default"), Card->Cost, Research->SilverCost);
        TestEqual(TEXT("Research effect matches runtime"), Card->SpellEffect, Research->Effect);
        TestFalse(TEXT("Spell is a permanent unlock rather than a temporary card"), Card->bExpeditionOnly);
        TestFalse(TEXT("Spell is initially locked"), LKHomeContent::DefaultUnlockedCards().Contains(Id));
        TestFalse(TEXT("Spell is absent from the default deck"), Data->DefaultPlayerDeck.Contains(Id));
        TestTrue(TEXT("Card icon belongs to the V083 asset collection"), Card->Icon.ToSoftObjectPath().ToString().Contains(TEXT("/V083/Cards/")));
        for (FName Summoned : Card->SummonedUnitIds) { TestNotNull(TEXT("Every summon ID resolves"), LKUnitContent::Find(Summoned)); }
    }
    TestEqual(TEXT("Novice reinforcement summons three"), FindCard(*Data, "Spell_MariaNovice")->SummonedUnitIds.Num(), 3);
    TestEqual(TEXT("Divine reinforcement summons two"), FindCard(*Data, "Spell_MariaDivine")->SummonedUnitIds.Num(), 2);
    TestTrue(TEXT("All reinforcements permit battlefield-wide deployment"), FindCard(*Data, "Spell_MariaNovice")->bGlobalPlacement && FindCard(*Data, "Spell_MariaDivine")->bGlobalPlacement);
    TestTrue(TEXT("Priest basic action is healing, without a passive trait"), LKUnitContent::Find("Unit_AngelPriest_Uncommon")->bBasicAttackHeals);
    TestFalse(TEXT("Archer basic action deals damage"), LKUnitContent::Find("Unit_AngelArcher_Uncommon")->bBasicAttackHeals);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083Tuning, "LittleKing.V083.Content.StableIdentityAndAuthoredTuning", Flags)
bool FLKV083Tuning::RunTest(const FString&)
{
    ULKGameData* Data = NewObject<ULKGameData>();
    ULKCardDefinition* Shared = NewObject<ULKCardDefinition>();
    Shared->CardId = "Spell_Freeze"; Shared->Cost = 6; Shared->EffectDuration = 4.5f; Shared->SpellRadius = 415.f;
    Shared->SpellEffect = ELKSpellEffect::Damage; Shared->CardType = ELKCardType::Unit;
    Data->CardLibrary.Add(Shared); Data->EnsureCardLibrary();
    const auto* Runtime = FindCard(*Data, "Spell_Freeze");
    if (!TestNotNull(TEXT("Authored spell receives a private runtime definition"), Runtime)) { return false; }
    TestTrue(TEXT("Shared asset was not mutated"), Runtime != Shared && Shared->CardType == ELKCardType::Unit && Shared->SpellEffect == ELKSpellEffect::Damage);
    TestEqual(TEXT("Canonical effect restored"), Runtime->SpellEffect, ELKSpellEffect::Freeze);
    TestEqual(TEXT("Canonical card type restored"), Runtime->CardType, ELKCardType::Spell);
    TestEqual(TEXT("Author controls duration"), Runtime->EffectDuration, 4.5f);
    TestEqual(TEXT("Author controls radius"), Runtime->SpellRadius, 415.f);
    TestEqual(TEXT("Author controls silver cost"), Runtime->Cost, 6);
    FLKUnitRow Authored = *LKUnitContent::Find("Unit_AngelPriest_Rare");
    Authored.bBasicAttackHeals = false; Authored.Race = ELKRace::Undead; Authored.Quality = ELKQuality::Common;
    Authored.BaseHealth = 777.f; Authored.AttackDamage = 55.f;
    const auto Merged = LKUnitContent::MergeAuthoredTuning(*LKUnitContent::Find("Unit_AngelPriest_Rare"), &Authored);
    TestTrue(TEXT("Priest profession is canonical"), Merged.bBasicAttackHeals);
    TestEqual(TEXT("Race is canonical"), Merged.Race, ELKRace::Angel);
    TestEqual(TEXT("Quality is canonical"), Merged.Quality, ELKQuality::Rare);
    TestEqual(TEXT("Health remains tunable"), Merged.BaseHealth, 777.f);
    TestEqual(TEXT("Healing remains tunable"), Merged.AttackDamage, 55.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083Research, "LittleKing.V083.Research.MaterialConsumptionAndPermanentUnlock", Flags)
bool FLKV083Research::RunTest(const FString&)
{
    // Explicitly opt into profile operations. A unique storage base and disabled autosave
    // keep this fixture in memory without reading or writing the player's profile.
    FScopedProfilePersistence Persistence;
    UGameInstance* Instance = NewObject<UGameInstance>(); auto* Profile = NewObject<ULKProfileSubsystem>(Instance);
    Profile->ConfigureStorage(TEXT("LittleKing_V083_Content_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    Profile->SetAutoSaveEnabled(false);
    if (!TestTrue(TEXT("Create isolated in-memory profile"), Profile->EnsureProfile())) { return false; }
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
    auto Loadout = LKHomeContent::DefaultLoadout(); Loadout.CardIds[0] = "Spell_Freeze";
    TestEqual(TEXT("New spell cannot depart before research"), Profile->SaveLoadout(Loadout, Data), ELKLoadoutResult::LockedCard);
    FString Error;
    TestFalse(TEXT("Research requires a purchased book"), Profile->ResearchCard("Spell_Freeze", Error));
    auto State = Profile->GetProfile();
    for (FName Id : LKV083Content::SpellIds()) { State.ResearchMaterials.Add(Id, 1); }
    Profile->SetProfileForTest(State);
    for (FName Id : LKV083Content::SpellIds())
    {
        TestTrue(TEXT("Research consumes the matching book"), Profile->ResearchCard(Id, Error));
        TestTrue(TEXT("Researched spell is permanently unlocked"), Profile->IsCardUnlocked(Id));
        TestFalse(TEXT("The consumed material is removed"), Profile->GetProfile().ResearchMaterials.Contains(Id));
        TestFalse(TEXT("Repeated research is rejected"), Profile->ResearchCard(Id, Error));
    }
    TestEqual(TEXT("Researched spell is valid for departure"), Profile->SaveLoadout(Loadout, Data), ELKLoadoutResult::Success);
    Loadout.CardIds[0] = "Unit_AngelWarrior_Uncommon";
    TestEqual(TEXT("Summoned warrior cannot become a card in the war room"), Profile->SaveLoadout(Loadout, Data), ELKLoadoutResult::LockedCard);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083MarketAndUI, "LittleKing.V083.Content.MarketDepthAndRulePresentation", Flags)
bool FLKV083MarketAndUI::RunTest(const FString&)
{
    TestEqual(TEXT("Catalog no longer places a blueprint at the end"), LKResearchContent::All().Last().Kind, ELKMarketOfferKind::SpellBook);
    int32 NewBooks = 0, Blueprints = 0;
    for (int32 Seed = 0; Seed < 20000; ++Seed)
    {
        FLKDungeonNode Node; Node.NodeId = "V083_Market"; Node.Type = ELKDungeonNodeType::Market;
        const int32 Depth = Seed % 4;
        LKResearchContent::GenerateMarket(Node, Seed, Depth);
        for (const auto& Offer : Node.MarketOffers)
        {
            if (Offer.Kind == ELKMarketOfferKind::Mercenary)
            { TestFalse(TEXT("Market does not sell angels"), LKV083Content::Units().Contains(Offer.CardId)); continue; }
            const auto* R = LKResearchContent::Find(Offer.CardId);
            if (!TestNotNull(TEXT("Every material has a definition"), R)) { return false; }
            TestEqual(TEXT("Material kind follows its definition, rather than row position"), Offer.Kind, R->Kind);
            TestTrue(TEXT("Book geography gate applies"), Depth >= R->MinimumRegionDepth);
            TestEqual(TEXT("Configured material price"), Offer.Price, R->Price);
            if (Offer.Kind == ELKMarketOfferKind::BuildingBlueprint) { ++Blueprints; }
            else if (LKV083Content::SpellIds().Contains(Offer.CardId)) { ++NewBooks; }
        }
    }
    TestTrue(TEXT("New books enter the market pool"), NewBooks > 0);
    TestTrue(TEXT("Blueprints continue to appear after inserting spells later in the catalog"), Blueprints > 0);
    const auto* Novice = LKResearchContent::Find("Spell_MariaNovice"); const auto* Divine = LKResearchContent::Find("Spell_MariaDivine");
    TestTrue(TEXT("Divine books are more expensive, rarer and require a later region"), Divine->Price > Novice->Price && Divine->Weight < Novice->Weight && Divine->MinimumRegionDepth > Novice->MinimumRegionDepth);
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureCardLibrary();
    TestTrue(TEXT("Freeze UI shows scaled duration"), LKCardPresentation::Detail(*FindCard(*Data, "Spell_Freeze"), nullptr, 1).ToString().Contains(TEXT("3.3 秒")));
    TestTrue(TEXT("Cloud UI explains both-team concealment"), LKCardPresentation::Detail(*FindCard(*Data, "Spell_BlackCloud")).ToString().Contains(TEXT("双方区域隐匿")));
    TestTrue(TEXT("Lightning UI shows current-health ordering"), LKCardPresentation::Detail(*FindCard(*Data, "Spell_Lightning")).ToString().Contains(TEXT("当前生命最高")));
    TestTrue(TEXT("Hurricane UI shows rectangle dimensions"), LKCardPresentation::Detail(*FindCard(*Data, "Spell_Hurricane")).ToString().Contains(TEXT("1300 × 700")));
    TestTrue(TEXT("Reinforcement UI distinguishes a healing profession"), LKCardPresentation::Detail(*FindCard(*Data, "Spell_MariaNovice")).ToString().Contains(TEXT("治疗 16")));
    TestTrue(TEXT("Blessing UI shows draw cooldown"), LKCardPresentation::Detail(*FindCard(*Data, "Spell_DivineBlessing")).ToString().Contains(TEXT("抽牌冷却 150 秒")));
    TestEqual(TEXT("Angel race is displayed"), LKCardPresentation::RaceName(ELKRace::Angel).ToString(), FString(TEXT("天使")));
    AddInfo(FString::Printf(TEXT("20,000 markets: %d new books, %d blueprints"), NewBooks, Blueprints));
    return true;
}
#endif
