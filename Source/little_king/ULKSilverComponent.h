#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ULKSilverComponent.generated.h"

/**
 * 银币组件：每秒产出 + 上限，驱动卡牌消耗。
 * 银币是经济资源，刻意不进 GAS（避免复杂度扩散）。
 */
UCLASS(ClassGroup = (LK), meta = (BlueprintSpawnableComponent))
class ULKSilverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSilverChanged, float, NewSilver, float, Delta);
	UPROPERTY(BlueprintAssignable, Category = "LK|Economy")
	FOnSilverChanged OnSilverChanged;

	void Init(float InPerSecond, float InCap);

	/** 每秒产出（由 GameMode 每帧调用） */
	void TickSilver(float DeltaTime);

	/** 尝试消费，成功扣款并返回 true */
	bool TrySpend(float Amount);

	void AddSilver(float Amount);

	UFUNCTION(BlueprintPure, Category = "LK|Economy")
	float GetSilver() const { return Silver; }

	UFUNCTION(BlueprintPure, Category = "LK|Economy")
	float GetCap() const { return Cap; }

	UFUNCTION(BlueprintPure, Category = "LK|Economy")
	float GetPerSecond() const { return PerSecond; }

	void SetPerSecond(float NewValue) { PerSecond = FMath::Max(0.f, NewValue); }
	void SetCap(float NewValue) { Cap = FMath::Max(1.f, NewValue); Silver = FMath::Min(Silver, Cap); }

private:
	float Silver = 0.f;
	float PerSecond = 1.5f;
	float Cap = 12.f;

	void Broadcast(float Delta) const;
};
