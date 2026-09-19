#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LKDataTypes.h"
#include "ULKUnitPassiveComponent.generated.h"

class ALKUnitBase;

/** 事件驱动被动技能，与可增删的英雄特性独立；由 GameMode 在伤害批次后调度。 */
UCLASS(ClassGroup = (LK))
class ULKUnitPassiveComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULKUnitPassiveComponent();
    void Initialize(const FLKUnitRow& Row);
    bool CanSummonFrom(const ALKUnitBase* Victim) const;
    bool TrySummonFrom(ALKUnitBase* Victim);
    void ObserveDefeat(const ALKUnitBase* Victim, bool bOwnerDefeatedInBatch);
    void ObserveCombatEvent(const FLKCombatEvent& Event);
    bool TryRevive();
    UFUNCTION(BlueprintPure, Category = "LK|Passive") ELKPassiveAbility GetAbility() const { return Ability; }
    UFUNCTION(BlueprintPure, Category = "LK|Passive") int32 GetBoneCount() const { return BoneCount; }
    UFUNCTION(BlueprintPure, Category = "LK|Passive") int32 GetRevivalThreshold() const { return RevivalThreshold; }
    UFUNCTION(BlueprintPure, Category = "LK|Passive") int32 GetRevivalCount() const { return RevivalCount; }
private:
    ELKPassiveAbility Ability = ELKPassiveAbility::None;
    float HealPercent = 0.03f;
    int32 BoneCount = 0;
    int32 RevivalThreshold = 10;
    int32 ThresholdStep = 5;
    int32 RevivalCount = 0;
};
