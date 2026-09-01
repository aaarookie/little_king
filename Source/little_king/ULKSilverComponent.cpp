#include "ULKSilverComponent.h"
#include "LKLog.h"

void ULKSilverComponent::Init(float InPerSecond, float InCap)
{
	PerSecond = FMath::Max(0.f, InPerSecond);
	Cap = FMath::Max(1.f, InCap);
	Silver = 0.f;
	Broadcast(0.f);
	UE_LOG(LogLKEconomy, Log, TEXT("[Silver] Init: %.1f/s, cap %.1f"), PerSecond, Cap);
}

void ULKSilverComponent::TickSilver(float DeltaTime)
{
	if (DeltaTime <= 0.f)
	{
		return;
	}

	const float Before = Silver;
	Silver = FMath::Min(Cap, Silver + PerSecond * DeltaTime);
	if (!FMath::IsNearlyEqual(Before, Silver))
	{
		Broadcast(Silver - Before);
	}
}

bool ULKSilverComponent::TrySpend(float Amount)
{
	if (Amount <= 0.f || Silver < Amount)
	{
		return false;
	}

	Silver -= Amount;
	Broadcast(-Amount);
	UE_LOG(LogLKEconomy, Log, TEXT("[Silver] Spend %.1f -> %.1f"), Amount, Silver);
	return true;
}

void ULKSilverComponent::AddSilver(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	const float Before = Silver;
	Silver = FMath::Min(Cap, Silver + Amount);
	Broadcast(Silver - Before);
}

void ULKSilverComponent::Broadcast(float Delta) const
{
	// const_cast：动态委托 Broadcast 非 const；此处为只读广播的便捷封装
	const_cast<ULKSilverComponent*>(this)->OnSilverChanged.Broadcast(Silver, Delta);
}
