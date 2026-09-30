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
    /**
     * 区域减速（骷髅法阵）：按来源登记，多个来源取最强、不叠乘成定身。
     * MoveMultiplier < 1 减速；AttackSpeedMultiplier < 1 变慢（内部换算为攻击间隔倍率 1/值）。
     * 离开区域或法阵到期立即解除；Clear() 会清空全部来源（控制/死亡/换房）。
     */
    void ApplyAreaSlow(FName SourceId, float MoveMultiplier, float AttackSpeedMultiplier, float Seconds);
    void RemoveAreaSlow(FName SourceId);
    bool HasAreaSlow(FName SourceId) const { return AreaSlowRemaining.Contains(SourceId); }
    /** 供法阵每帧刷新存活来源；过期来源自动清理。 */
    void TickAreaSlowRemaining(float DeltaSeconds);
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
    /** 强化与区域减速相乘：控场既不叠加成定身，也不互相抹掉。 */
    float MoveMultiplier() const { return (IsEmpowered() ? EmpowerMove : 1.f) * AreaSlowMoveMultiplier(); }
    float IntervalMultiplier() const { return (IsEmpowered() ? EmpowerInterval : 1.f) * AreaSlowIntervalMultiplier(); }
    float DamageTakenMultiplier() const { return IsEmpowered() ? .8f : 1.f; }
    float AttackMultiplier() const { return bSpearSupport ? 1.2f : 1.f; }
    /** 当前生效的区域减速分量（无来源时为 1）。 */
    float AreaSlowMoveMultiplier() const;
    float AreaSlowIntervalMultiplier() const;
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
    /** 区域减速来源：SourceId -> 最大移动倍率 / 最大攻击间隔倍率 / 剩余时间。 */
    struct FAreaSlowEntry
    {
        float MoveMultiplier = 1.f;
        float IntervalMultiplier = 1.f;
        float Remaining = 0.f;
    };
    TMap<FName, FAreaSlowEntry> AreaSlowRemaining;
    bool bSpearSupport = false;
    bool bWarriorSupport = false;
    ELKBreathHead LastBreath = ELKBreathHead::None;
    FLKCombatSource BurnSource;
    TWeakObjectPtr<AActor> BurnInstigator;
    FActiveGameplayEffectHandle WarriorHandle;
    UPROPERTY(Transient) TObjectPtr<UGameplayEffect> WarriorEffect;
};
