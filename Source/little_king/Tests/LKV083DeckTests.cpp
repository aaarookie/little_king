#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../ULKDeckState.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags V083DeckFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
bool CheckFullUnique(FAutomationTestBase& Test, const ULKDeckState* Deck, int32 Total)
{
    bool bOkay = Test.TestEqual(TEXT("Four slots remain full"), Deck->GetHandSize(), 4);
    TSet<FName> UniqueHand;
    for (FName Id : Deck->GetHand())
    {
        bOkay &= Test.TestFalse(TEXT("No hand slot is empty"), Id.IsNone());
        bOkay &= Test.TestFalse(TEXT("No cooling card enters the hand"), Deck->GetCooldownRemaining(Id) > 0.f);
        UniqueHand.Add(Id);
    }
    bOkay &= Test.TestEqual(TEXT("Hand cards remain unique"), UniqueHand.Num(), 4);
    TSet<FName> UniqueDeck;
    for (FName Id : Deck->GetAllCards()) { UniqueDeck.Add(Id); }
    bOkay &= Test.TestEqual(TEXT("No card is lost or duplicated"), UniqueDeck.Num(), Total);
    bOkay &= Test.TestEqual(TEXT("AllCards includes cooling cards"), Deck->GetAllCards().Num(), Total);
    return bOkay;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083DrawCooldownBoundary,
    "LittleKing.V083.Deck.Cooldown150BoundaryAndFIFO", V083DeckFlags)
bool FLKV083DrawCooldownBoundary::RunTest(const FString&)
{
    ULKDeckState* Deck = NewObject<ULKDeckState>();
    TestTrue(TEXT("Seven-card deck initializes"), Deck->InitDeck({"A", "B", "C", "D", "E", "F", "G"}, 4, 83));
    const FName Cooling = Deck->GetHandCard(0);
    const FName OriginalNext = Deck->GetNextCard();
    TestTrue(TEXT("First cooldown cast can replenish the hand"), Deck->CanPlayWithCooldown(0, 150.f));
    TestEqual(TEXT("Cooldown play returns the cast card"), Deck->PlayCard(0, 150.f), Cooling);
    TestEqual(TEXT("The current queue head fills the cast slot"), Deck->GetHandCard(0), OriginalNext);
    TestEqual(TEXT("The full 150 seconds starts on successful play"), Deck->GetCooldownRemaining(Cooling), 150.f);

    for (int32 I = 0; I < 80; ++I)
    {
        const TArray<FName> All = Deck->GetAllCards();
        FName ExpectedNext = NAME_None;
        for (int32 J = 4; J < All.Num(); ++J)
        {
            if (All[J] != Cooling) { ExpectedNext = All[J]; break; }
        }
        TestEqual(TEXT("FIFO selects the first eligible card without deleting skipped cards"), Deck->GetNextCard(), ExpectedNext);
        const int32 Slot = I % 4;
        const FName Played = Deck->GetHandCard(Slot);
        TestEqual(TEXT("Repeated play succeeds while the spell cools"), Deck->PlayCard(Slot), Played);
        TestEqual(TEXT("Eligible FIFO head fills the selected slot"), Deck->GetHandCard(Slot), ExpectedNext);
        if (!CheckFullUnique(*this, Deck, 7)) { return false; }
    }
    Deck->AdvanceCooldowns(149.75f);
    TestEqual(TEXT("The final quarter second remains unavailable"), Deck->GetCooldownRemaining(Cooling), .25f);
    TestFalse(TEXT("The spell cannot be drawn before its deadline"), Deck->GetNextCard() == Cooling);
    const TArray<FName> BeforeExpiry = Deck->GetHand();
    Deck->AdvanceCooldowns(.25f);
    TestEqual(TEXT("Exactly 150 seconds releases the spell"), Deck->GetCooldownRemaining(Cooling), 0.f);
    TestTrue(TEXT("Expiry does not automatically insert a card"), Deck->GetHand() == BeforeExpiry);
    TestEqual(TEXT("The older skipped FIFO card resumes first"), Deck->GetNextCard(), Cooling);
    Deck->PlayCard(0);
    TestEqual(TEXT("Normal subsequent play draws the released spell"), Deck->GetHandCard(0), Cooling);
    return CheckFullUnique(*this, Deck, 7);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083FiveCardFallback,
    "LittleKing.V083.Deck.FiveCardFallbackAndAtomicRejection", V083DeckFlags)
bool FLKV083FiveCardFallback::RunTest(const FString&)
{
    ULKDeckState* Deck = NewObject<ULKDeckState>();
    Deck->InitDeck({"A", "B", "C", "D", "E"}, 4, 3);
    const FName Cooling = Deck->GetHandCard(0);
    Deck->PlayCard(0, 150.f);
    TestTrue(TEXT("A ready deck remains ready when its queue is cooling"), Deck->IsReady());
    TestTrue(TEXT("No cooled spell is advertised as the next card"), Deck->GetNextCard().IsNone());
    const TArray<FName> FullHand = Deck->GetHand();
    const TArray<FName> AllBefore = Deck->GetAllCards();
    for (int32 I = 0; I < 40; ++I)
    {
        const int32 Slot = I % 4;
        const FName Played = Deck->GetHandCard(Slot);
        TestTrue(TEXT("A non-cooldown card can always be played"), Deck->CanPlayWithCooldown(Slot, 0.f));
        TestEqual(TEXT("The only legal replacement is the just-played card"), Deck->PlayCard(Slot), Played);
        TestTrue(TEXT("Fallback keeps every unique slot full"), Deck->GetHand() == FullHand);
        if (!CheckFullUnique(*this, Deck, 5)) { return false; }
    }
    TestFalse(TEXT("Cooling a second card would leave fewer than four usable cards"), Deck->CanPlayWithCooldown(1, 10.f));
    TestTrue(TEXT("Impossible second cooldown play is rejected"), Deck->PlayCard(1, 10.f).IsNone());
    TestTrue(TEXT("Rejected play preserves hand and queue"), Deck->GetAllCards() == AllBefore);
    TestEqual(TEXT("Rejected play does not start a second cooldown"), Deck->GetCooldownRemaining(Deck->GetHandCard(1)), 0.f);
    TestEqual(TEXT("Rejected play leaves the existing cooldown untouched"), Deck->GetCooldownRemaining(Cooling), 150.f);

    for (float Invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        TestFalse(TEXT("Non-finite and negative cooldowns are rejected before casting"), Deck->CanPlayWithCooldown(0, Invalid));
        TestTrue(TEXT("Invalid cooldown cannot mutate the deck"), Deck->PlayCard(0, Invalid).IsNone());
    }
    TestTrue(TEXT("Invalid slots are rejected"), Deck->PlayCard(-1).IsNone() && Deck->PlayCard(4).IsNone());
    Deck->AdvanceCooldowns(-1.f);
    Deck->AdvanceCooldowns(std::numeric_limits<float>::quiet_NaN());
    Deck->AdvanceCooldowns(std::numeric_limits<float>::infinity());
    TestEqual(TEXT("Invalid clock advancement cannot bypass cooldown"), Deck->GetCooldownRemaining(Cooling), 150.f);
    Deck->AdvanceCooldowns(150.f);
    TestEqual(TEXT("The sole queued spell is drawable on expiry"), Deck->GetNextCard(), Cooling);
    Deck->PlayCard(2);
    TestEqual(TEXT("The released spell draws into the played slot"), Deck->GetHandCard(2), Cooling);
    return CheckFullUnique(*this, Deck, 5);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083MultipleCooldownAndReset,
    "LittleKing.V083.Deck.MultipleCooldownMutationAndBattleReset", V083DeckFlags)
bool FLKV083MultipleCooldownAndReset::RunTest(const FString&)
{
    ULKDeckState* Deck = NewObject<ULKDeckState>();
    Deck->InitDeck({"A", "B", "C", "D", "E", "F"}, 4, 4);
    const FName First = Deck->GetHandCard(0);
    const FName Second = Deck->GetHandCard(1);
    Deck->PlayCard(0, 150.f);
    TestTrue(TEXT("A six-card deck permits two simultaneous cooldowns"), Deck->CanPlayWithCooldown(1, 30.f));
    Deck->PlayCard(1, 30.f);
    TestFalse(TEXT("A third simultaneous cooldown is rejected"), Deck->CanPlayWithCooldown(2, 20.f));
    CheckFullUnique(*this, Deck, 6);
    const TArray<FName> Snapshot = Deck->GetAllCards();
    TestFalse(TEXT("Removing a hand card cannot draw a cooling replacement"), Deck->RemoveCardFromDeck(Deck->GetHandCard(0)));
    TestTrue(TEXT("Failed removal leaves all piles unchanged"), Snapshot == Deck->GetAllCards());
    TestFalse(TEXT("Invalid initialization preserves the active battle deck"), Deck->InitDeck({"A", "B", "C", "D"}, 4));
    TestTrue(TEXT("Invalid initialization preserves queue order"), Snapshot == Deck->GetAllCards());
    TestEqual(TEXT("Invalid initialization preserves the first cooldown"), Deck->GetCooldownRemaining(First), 150.f);
    TestEqual(TEXT("Invalid initialization preserves the second cooldown"), Deck->GetCooldownRemaining(Second), 30.f);

    TestTrue(TEXT("Transforming a cooling card preserves its slot"), Deck->TransformCard(Second, "Upgrade"));
    TestEqual(TEXT("Transform cannot bypass the remaining cooldown"), Deck->GetCooldownRemaining("Upgrade"), 30.f);
    TestEqual(TEXT("Transform removes the obsolete cooldown identity"), Deck->GetCooldownRemaining(Second), 0.f);
    Deck->AdvanceCooldowns(30.f);
    TestEqual(TEXT("Independent cooldowns progress by the same battle clock"), Deck->GetCooldownRemaining(First), 120.f);
    TestEqual(TEXT("The shorter cooldown becomes the next eligible FIFO card"), Deck->GetNextCard(), FName("Upgrade"));
    TestTrue(TEXT("A queued cooling card may be removed without emptying the hand"), Deck->RemoveCardFromDeck(First));
    TestEqual(TEXT("Removing a card clears its obsolete cooldown"), Deck->GetCooldownRemaining(First), 0.f);
    CheckFullUnique(*this, Deck, 5);

    const FName ResetCooling = Deck->GetHandCard(0);
    Deck->PlayCard(0, 150.f);
    TestEqual(TEXT("The old battle still has an active cooldown before reset"), Deck->GetCooldownRemaining(ResetCooling), 150.f);
    TestTrue(TEXT("A fresh battle initializes successfully"), Deck->InitDeck({"A", "B", "C", "D", "E", "F"}, 4, 4));
    TestEqual(TEXT("Fresh battle removes the prior battle cooldown"), Deck->GetCooldownRemaining(ResetCooling), 0.f);
    for (FName Id : Deck->GetAllCards())
    { TestEqual(TEXT("Room reset clears every draw cooldown"), Deck->GetCooldownRemaining(Id), 0.f); }
    return CheckFullUnique(*this, Deck, 6);
}
#endif
