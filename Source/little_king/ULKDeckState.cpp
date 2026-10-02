#include "ULKDeckState.h"
#include "LKCardRules.h"

bool ULKDeckState::InitDeck(const TArray<FName>& DeckCards, int32 InHandSizeLimit, int32 Seed)
{
    const int32 Slots = FMath::Max(1, InHandSizeLimit);
    TArray<FName> Cards;
    for (FName Id : DeckCards)
    {
        if (CardAllowed && !CardAllowed(Id)) { return false; }
        if (!Id.IsNone()) { Cards.AddUnique(Id); }
    }
    if (Cards.Num() <= Slots) { return false; }

    FRandomStream Random(Seed);
    for (int32 i = Cards.Num() - 1; i > 0; --i) { Cards.Swap(i, Random.RandRange(0, i)); }
    HandSizeLimit = Slots;
    Hand.Reset(Slots);
    for (int32 i = 0; i < Slots; ++i) { Hand.Add(Cards[i]); }
    Cards.RemoveAt(0, Slots);
    DrawPile = MoveTemp(Cards);
    DrawCooldownRemaining.Reset();
    BroadcastHandChanged();
    return true;
}

bool ULKDeckState::DrawCard() { return false; }

FName ULKDeckState::PlayCard(int32 HandIndex, float DrawCooldownSeconds)
{
    if (!CanPlayWithCooldown(HandIndex, DrawCooldownSeconds)) { return NAME_None; }
    const FName Played = Hand[HandIndex];
    const int32 DrawIndex = FindDrawableIndex();
    if (DrawCooldownSeconds > 0.f) { DrawCooldownRemaining.Add(Played, DrawCooldownSeconds); }
    if (DrawIndex != INDEX_NONE)
    {
        Hand[HandIndex] = DrawPile[DrawIndex];
        DrawPile.RemoveAt(DrawIndex);
        DrawPile.Add(Played);
    }
    // Minimum compatibility: a five-card deck with one cooling card has exactly
    // four usable kinds. Re-draw the just-played non-cooldown card, never an empty
    // slot or an additional copy. Cooldown cards stay in their original queue order.
    BroadcastHandChanged();
    return Played;
}

bool ULKDeckState::CanPlayWithCooldown(int32 HandIndex, float DrawCooldownSeconds) const
{
    if (!IsReady() || !Hand.IsValidIndex(HandIndex) || !FMath::IsFinite(DrawCooldownSeconds)
        || DrawCooldownSeconds < 0.f || GetCooldownRemaining(Hand[HandIndex]) > 0.f) { return false; }
    int32 UsableKinds = 0;
    const FName Played = Hand[HandIndex];
    for (FName CardId : GetAllCards())
    {
        if (GetCooldownRemaining(CardId) <= 0.f && (CardId != Played || DrawCooldownSeconds <= 0.f)) { ++UsableKinds; }
    }
    return UsableKinds >= HandSizeLimit && (FindDrawableIndex() != INDEX_NONE || DrawCooldownSeconds <= 0.f);
}

int32 ULKDeckState::FindDrawableIndex() const
{
    for (int32 Index = 0; Index < DrawPile.Num(); ++Index)
    {
        if (GetCooldownRemaining(DrawPile[Index]) <= 0.f) { return Index; }
    }
    return INDEX_NONE;
}

FName ULKDeckState::GetNextCard() const
{
    const int32 Index = FindDrawableIndex();
    return DrawPile.IsValidIndex(Index) ? DrawPile[Index] : NAME_None;
}

float ULKDeckState::GetCooldownRemaining(FName CardId) const
{
    const float* Remaining = DrawCooldownRemaining.Find(CardId);
    return Remaining ? FMath::Max(0.f, *Remaining) : 0.f;
}

void ULKDeckState::AdvanceCooldowns(float DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
    bool bExpired = false;
    for (auto It = DrawCooldownRemaining.CreateIterator(); It; ++It)
    {
        It.Value() = FMath::Max(0.f, It.Value() - DeltaSeconds);
        if (It.Value() <= 0.f) { It.RemoveCurrent(); bExpired = true; }
    }
    if (bExpired) { BroadcastHandChanged(); }
}

FName ULKDeckState::GetHandCard(int32 HandIndex) const
{
    return Hand.IsValidIndex(HandIndex) ? Hand[HandIndex] : NAME_None;
}

int32 ULKDeckState::GetHandSize() const { return Hand.Num(); }

int32 ULKDeckState::GetHandCost(int32 HandIndex) const
{
    const FName Id = GetHandCard(HandIndex);
    return Id.IsNone() ? -1 : (CostProvider ? CostProvider(Id) : 2);
}

TArray<FName> ULKDeckState::GetAllCards() const
{
    TArray<FName> Cards = Hand;
    Cards.Append(DrawPile);
    return Cards;
}

bool ULKDeckState::AddCardToDeck(FName CardId)
{
    if (CardAllowed && !CardAllowed(CardId)) { return false; }
    if (!IsReady() || LKCardRules::Used(GetAllCards()) + LKCardRules::Slots(CardId) > LKCardRules::Capacity || CardId.IsNone() || ContainsCard(CardId)) { return false; }
    DrawPile.Add(CardId);
    BroadcastHandChanged();
    return true;
}

bool ULKDeckState::RemoveCardFromDeck(FName CardId)
{
    if (!IsReady() || DrawPile.Num() <= 1 || !ContainsCard(CardId)) { return false; }
    const int32 Slot = Hand.IndexOfByKey(CardId);
    if (Slot != INDEX_NONE)
    {
        const int32 DrawIndex = FindDrawableIndex();
        if (DrawIndex == INDEX_NONE) { return false; }
        Hand[Slot] = DrawPile[DrawIndex];
        DrawPile.RemoveAt(DrawIndex);
    }
    else { DrawPile.RemoveSingle(CardId); }
    DrawCooldownRemaining.Remove(CardId);
    BroadcastHandChanged();
    return true;
}

bool ULKDeckState::TransformCard(FName OldCardId, FName NewCardId)
{
    if (CardAllowed && !CardAllowed(NewCardId)) { return false; }
    if (!IsReady() || NewCardId.IsNone() || ContainsCard(NewCardId)
        || LKCardRules::Used(GetAllCards()) - LKCardRules::Slots(OldCardId) + LKCardRules::Slots(NewCardId) > LKCardRules::Capacity) { return false; }
    for (TArray<FName>* Pile : { &Hand, &DrawPile })
    {
        const int32 Index = Pile->IndexOfByKey(OldCardId);
        if (Index != INDEX_NONE)
        {
            (*Pile)[Index] = NewCardId;
            if (const float Remaining = GetCooldownRemaining(OldCardId); Remaining > 0.f)
            { DrawCooldownRemaining.Remove(OldCardId); DrawCooldownRemaining.Add(NewCardId, Remaining); }
            BroadcastHandChanged();
            return true;
        }
    }
    return false;
}

void ULKDeckState::BroadcastHandChanged() { OnHandChanged.Broadcast(); }
