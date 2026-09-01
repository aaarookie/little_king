#include "ULKUnitAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "ALKUnitBase.h"
#include "LKLog.h"

class ALKUnitBase;

void ULKUnitAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float MaxHp = FMath::Max(1.f, GetMaxHealth());
		Health.SetCurrentValue(FMath::Clamp(GetHealth(), 0.f, MaxHp));

		AActor* OwningActor = GetOwningActor();
		if (ALKUnitBase* Unit = Cast<ALKUnitBase>(OwningActor))
		{
			Unit->OnHealthChanged(GetHealth(), MaxHp);
			if (GetHealth() <= 0.f && !Unit->IsDead())
			{
				Unit->Die();
			}
		}
	}
}
