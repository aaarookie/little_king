#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ULKDeckState.generated.h"

/**
 * 牌库状态：固定手牌 + 先进先出的待抽队列。
 * 牌库只存 CardId（FName），费用等通过 CostProvider 从 CardLibrary 解析。
 * CardId 全副牌唯一；有效牌组至少为手牌槽数 + 1。出牌后原槽立即补满。
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

	/** 去重后仅开局洗牌一次；不足槽数 + 1 时返回 false，保留原状态。 */
	bool InitDeck(const TArray<FName>& DeckCards, int32 InHandSizeLimit, int32 Seed = 1);

	/** 兼容旧调试调用；有效牌组始终满手，额外抽牌不改变队列。 */
	bool DrawCard();

	/** 队首换入原槽，打出的牌排至队尾；只广播一次完整手牌。 */
	FName PlayCard(int32 HandIndex);

	// ---------- 查询 ----------
	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	FName GetHandCard(int32 HandIndex) const;

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetHandSize() const;

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetDeckSize() const { return DrawPile.Num(); }

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetDiscardSize() const { return 0; } // 兼容旧查询，不再使用弃牌堆。

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	FName GetNextCard() const { return DrawPile.IsEmpty() ? NAME_None : DrawPile[0]; }

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	bool IsReady() const { return Hand.Num() == HandSizeLimit && !DrawPile.IsEmpty(); }

	TArray<FName> GetAllCards() const;

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	int32 GetHandCost(int32 HandIndex) const;

	UFUNCTION(BlueprintPure, Category = "LK|Deck")
	TArray<FName> GetHand() const { return Hand; }

	// ---------- 地牢局内构筑接口（S2 接入） ----------
	/** 重复添加、删到不足槽数 + 1、变成已有 CardId 均拒绝并保留原状态。 */
	bool AddCardToDeck(FName CardId);
	bool RemoveCardFromDeck(FName CardId);
	bool TransformCard(FName OldCardId, FName NewCardId);

private:
	TArray<FName> DrawPile;
	TArray<FName> Hand;
	int32 HandSizeLimit = 4;
	bool ContainsCard(FName CardId) const { return Hand.Contains(CardId) || DrawPile.Contains(CardId); }
	void BroadcastHandChanged();
};
