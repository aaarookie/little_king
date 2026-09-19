#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "LKTypes.h"
#include "ULKUnitStatusComponent.generated.h"
class ALKUnitBase;
class UGameplayEffect;

/** Per-battle status and target-owned shared attack history. Never persisted across rooms. */
UCLASS(ClassGroup = (LK))
class ULKUnitStatusComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULKUnitStatusComponent();
    void TickStatus(float DeltaSeconds);
    void Clear();
    bool Stun(float Seconds);
    bool Freeze(float Seconds);
    void Empower(float Seconds, float MoveMultiplier, float IntervalMultiplier);
    bool BeginAttack();
    void AfterAttack();
    float ReceiveBreath(ELKBreathHead Head, float Damage, AActor* Instigator, const FLKCombatSource& Source);
    void Ignite(const FLKCombatSource& Source, AActor* Instigator);
    void RefreshTrollSupport();
    static void RefreshTeamSupport(UWorld* World);
    bool IsControlled() const { return StunRemaining > 0.f || FreezeRemaining > 0.f; }
    bool IsStunned() const { return StunRemaining > 0.f; }
    bool IsFrozen() const { return FreezeRemaining > 0.f; }
    bool IsEmpowered() const { return EmpowerRemaining > 0.f; }
    float GetStunMeter() const { return StunMeter; }
    int32 GetBurnStacks() const { return BurnStacks; }
    int32 GetAttackCount() const { return AttackCount; }
    ELKBreathHead GetLastBreath() const { return LastBreath; }
    float MoveMultiplier() const { return IsEmpowered() ? EmpowerMove : 1.f; }
    float IntervalMultiplier() const { return IsEmpowered() ? EmpowerInterval : 1.f; }
    float DamageTakenMultiplier() const { return IsEmpowered() ? .8f : 1.f; }
    float AttackMultiplier() const { return bSpearSupport ? 1.2f : 1.f; }
private:
    ALKUnitBase* Unit() const;
    void InterruptActions();
    void SetWarriorSupport(bool bEnabled);
    bool IsKing() const;
    float StunRemaining = 0.f;
    float FreezeRemaining = 0.f;
    float StunMeter = 0.f;
    float EmpowerRemaining = 0.f;
    float EmpowerMove = 1.3f;
    float EmpowerInterval = .75f;
    float BurnRemaining = 0.f;
    float BurnTick = 0.f;
    int32 BurnStacks = 0;
    int32 AttackCount = 0;
    bool bSpearSupport = false;
    bool bWarriorSupport = false;
    ELKBreathHead LastBreath = ELKBreathHead::None;
    FLKCombatSource BurnSource;
    TWeakObjectPtr<AActor> BurnInstigator;
    FActiveGameplayEffectHandle WarriorHandle;
    UPROPERTY(Transient) TObjectPtr<UGameplayEffect> WarriorEffect;
};
