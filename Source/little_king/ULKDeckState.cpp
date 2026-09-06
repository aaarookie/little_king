#include "ULKDeckState.h"

bool ULKDeckState::InitDeck(const TArray<FName>& DeckCards, int32 InHandSizeLimit, int32 Seed)
{
    const int32 Slots = FMath::Max(1, InHandSizeLimit);
    TArray<FName> Cards;
    for (FName Id : DeckCards) { if (!Id.IsNone()) { Cards.AddUnique(Id); } }
    if (Cards.Num() <= Slots) { return false; }

    FRandomStream Random(Seed);
    for (int32 i = Cards.Num() - 1; i > 0; --i) { Cards.Swap(i, Random.RandRange(0, i)); }
    HandSizeLimit = Slots;
    Hand.Reset(Slots);
    for (int32 i = 0; i < Slots; ++i) { Hand.Add(Cards[i]); }
    Cards.RemoveAt(0, Slots);
    DrawPile = MoveTemp(Cards);
    BroadcastHandChanged();
    return true;
}

bool ULKDeckState::DrawCard() { return false; }

FName ULKDeckState::PlayCard(int32 HandIndex)
{
    if (!IsReady() || !Hand.IsValidIndex(HandIndex)) { return NAME_None; }
    const FName Played = Hand[HandIndex];
    Hand[HandIndex] = DrawPile[0];
    DrawPile.RemoveAt(0);
    DrawPile.Add(Played);
    BroadcastHandChanged();
    return Played;
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
    if (!IsReady() || CardId.IsNone() || ContainsCard(CardId)) { return false; }
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
        Hand[Slot] = DrawPile[0];
        DrawPile.RemoveAt(0);
    }
    else { DrawPile.RemoveSingle(CardId); }
    BroadcastHandChanged();
    return true;
}

bool ULKDeckState::TransformCard(FName OldCardId, FName NewCardId)
{
    if (!IsReady() || NewCardId.IsNone() || ContainsCard(NewCardId)) { return false; }
    for (TArray<FName>* Pile : { &Hand, &DrawPile })
    {
        const int32 Index = Pile->IndexOfByKey(OldCardId);
        if (Index != INDEX_NONE)
        {
            (*Pile)[Index] = NewCardId;
            BroadcastHandChanged();
            return true;
        }
    }
    return false;
}

void ULKDeckState::BroadcastHandChanged() { OnHandChanged.Broadcast(); }
