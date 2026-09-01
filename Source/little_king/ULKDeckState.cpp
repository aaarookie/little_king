#include "ULKDeckState.h"
#include "LKLog.h"

void ULKDeckState::InitDeck(const TArray<FName>& DeckCards, int32 InHandSizeLimit)
{
	HandSizeLimit = FMath::Max(1, InHandSizeLimit);
	DrawPile = DeckCards;
	DiscardPile.Reset();
	Hand.Reset();

	Shuffle(DrawPile);

	// 开局抽满手牌
	int32 Safety = 0;
	while (Hand.Num() < HandSizeLimit && DrawPile.Num() > 0 && Safety < 64)
	{
		DrawCard();
		++Safety;
	}

	UE_LOG(LogLKEconomy, Log, TEXT("[Deck] Init: 牌库 %d 张, 手牌 %d 张"), DrawPile.Num(), Hand.Num());
	BroadcastHandChanged();
}

bool ULKDeckState::DrawCard()
{
	if (Hand.Num() >= HandSizeLimit)
	{
		return false;
	}

	if (DrawPile.Num() == 0)
	{
		RefillDrawFromDiscard();
	}

	if (DrawPile.Num() == 0)
	{
		return false;
	}

	Hand.Add(DrawPile[0]);
	DrawPile.RemoveAt(0);
	BroadcastHandChanged();
	return true;
}

FName ULKDeckState::PlayCard(int32 HandIndex)
{
	if (!Hand.IsValidIndex(HandIndex))
	{
		return NAME_None;
	}

	const FName CardId = Hand[HandIndex];
	Hand.RemoveAt(HandIndex);
	DiscardPile.Add(CardId);

	// 补抽一张（手牌满则放弃）
	DrawCard();

	UE_LOG(LogLKEconomy, Log, TEXT("[Deck] Play %s, 手牌剩余 %d"), *CardId.ToString(), Hand.Num());
	return CardId;
}

FName ULKDeckState::GetHandCard(int32 HandIndex) const
{
	return Hand.IsValidIndex(HandIndex) ? Hand[HandIndex] : NAME_None;
}

int32 ULKDeckState::GetHandSize() const
{
	return Hand.Num();
}

int32 ULKDeckState::GetHandCost(int32 HandIndex) const
{
	const FName CardId = GetHandCard(HandIndex);
	if (CardId.IsNone())
	{
		return -1;
	}
	return CostProvider ? CostProvider(CardId) : 2;
}

void ULKDeckState::RemoveCardFromDeck(FName CardId)
{
	DrawPile.Remove(CardId);
	Hand.Remove(CardId);
	DiscardPile.Remove(CardId);
	BroadcastHandChanged();
}

void ULKDeckState::TransformCard(FName OldCardId, FName NewCardId)
{
	for (FName& Id : DrawPile) { if (Id == OldCardId) { Id = NewCardId; } }
	for (FName& Id : Hand)    { if (Id == OldCardId) { Id = NewCardId; } }
	for (FName& Id : DiscardPile) { if (Id == OldCardId) { Id = NewCardId; } }
	BroadcastHandChanged();
}

void ULKDeckState::RefillDrawFromDiscard()
{
	if (DiscardPile.Num() == 0)
	{
		return;
	}

	DrawPile = DiscardPile;
	DiscardPile.Reset();
	Shuffle(DrawPile);
	UE_LOG(LogLKEconomy, Log, TEXT("[Deck] 洗回弃牌堆: %d 张"), DrawPile.Num());
}

void ULKDeckState::Shuffle(TArray<FName>& Cards)
{
	if (Cards.Num() < 2)
	{
		return;
	}

	for (int32 i = Cards.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Cards.Swap(i, j);
	}
}

void ULKDeckState::BroadcastHandChanged()
{
	OnHandChanged.Broadcast();
}
