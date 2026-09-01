#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ULKDeckState.generated.h"

/**
 * 牌库状态：抽牌堆 / 手牌 / 弃牌堆（杀戮尖塔式循环）。
 * 牌库只存 CardId（FName），费用等通过 CostProvider 从 CardLibrary 解析。
 * 法术门（法师在场）由 GameMode 判定，本组件保持"哑状态"。
 */
UCLASS(ClassGroup = (LK), meta = (BlueprintSpawnableComponent))
class ULKDeckState : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHandChanged);
	UPROPERTY(BlueprintAssignable, Category = "LK|Deck")
	FOnHandChanged OnHandChanged;

	/** 费用解析器（GameMode 注入：CardId -> Cost） */
	TFunction<int32(FName)> CostProvider;

	void InitDeck(const TArray<FName>& DeckCards, int32 InHandSizeLimit);

	/** 抽一张牌补手（牌库空则洗回弃牌堆）；手牌满则不抽 */
	bool DrawCard();

	/** 打出第 Index 张手牌：移除 -> 入弃牌堆 -> 补抽；返回 CardId（无效返回 NAME_None） */
	FName PlayCard(int32 HandIndex);

	// ---------- 查询 ----------
	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	FName GetHandCard(int32 HandIndex) const;

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetHandSize() const;

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetDeckSize() const { return DrawPile.Num(); }

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetDiscardSize() const { return DiscardPile.Num(); }

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetHandCost(int32 HandIndex) const;

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	TArray<FName> GetHand() const { return Hand; }

	// ---------- 地牢局内构筑接口（S2 接入） ----------
	void AddCardToDeck(FName CardId) { DrawPile.Add(CardId); }
	void RemoveCardFromDeck(FName CardId);
	void TransformCard(FName OldCardId, FName NewCardId);

private:
	TArray<FName> DrawPile;
	TArray<FName> Hand;
	TArray<FName> DiscardPile;
	int32 HandSizeLimit = 4;

	void RefillDrawFromDiscard();
	void Shuffle(TArray<FName>& Cards);
	void BroadcastHandChanged();
};
