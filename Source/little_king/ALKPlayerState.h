#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ALKPlayerState.generated.h"

class ULKSilverComponent;
class ULKDeckState;

/**
 * 玩家侧状态：银币 + 牌库（敌方由 AOpponentBrain 持有对称组件）。
 */
UCLASS()
class ALKPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ALKPlayerState();

	UPROPERTY(BlueprintReadOnly, Category = "LK")
	TObjectPtr<ULKSilverComponent> Silver;

	UPROPERTY(BlueprintReadOnly, Category = "LK")
	TObjectPtr<ULKDeckState> Deck;
};
