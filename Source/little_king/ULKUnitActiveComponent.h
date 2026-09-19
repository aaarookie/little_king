#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LKDataTypes.h"
#include "ULKUnitActiveComponent.generated.h"
class ALKUnitBase;
class ULKCardDefinition;

/** Native mercenary active skills. Ticked by the unit only while combat is enabled. */
UCLASS(ClassGroup = (LK))
class ULKUnitActiveComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULKUnitActiveComponent();
    void Initialize(const FLKUnitRow& Row);
    bool TickAbility(float DeltaSeconds);
    bool TryActivate();
    void ObserveDefeat(const ALKUnitBase* Victim);
    void Stop();
    void InterruptMovement() { DashRemaining = 0.f; DashHits.Reset(); }
    ALKUnitBase* GetLockedTarget() const;
    bool IsDashing() const { return DashRemaining > 0.f; }
    float GetCooldownRemaining() const { return CooldownRemaining; }
    FName GetLastSpellId() const { return LastSpellId; }
    ELKActiveAbility GetAbility() const { return Ability; }
private:
    bool Backstab();
    bool MimicSpell();
    bool StartDash();
    bool TrollEmpower();
    float EmpowerDuration = 8.f;
    float EmpowerMove = 1.3f;
    float EmpowerInterval = .75f;
    void TickDash(float DeltaSeconds);
    ELKActiveAbility Ability = ELKActiveAbility::None;
    float Cooldown = 8.f;
    float CooldownRemaining = 8.f;
    float DamageMultiplier = 2.f;
    float SilverChance = .3f;
    float DashDistance = 450.f;
    float DashSpeed = 1800.f;
    float HitRadius = 100.f;
    float KnockbackDistance = 120.f;
    float DashRemaining = 0.f;
    FVector DashDirection = FVector::ZeroVector;
    TWeakObjectPtr<ALKUnitBase> LockedTarget;
    TSet<TWeakObjectPtr<ALKUnitBase>> DashHits;
    FName LastSpellId;
};
