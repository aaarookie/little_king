#include "ULKUnitStatusComponent.h"
#include "ALKUnitBase.h"
#include "ULKUnitPassiveComponent.h"
#include "ULKUnitActiveComponent.h"
#include "ULKUnitMovementComponent.h"
#include "ULKUnitAttributeSet.h"
#include "LKGameplayHelpers.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "EngineUtils.h"
#include "ULKPresentationSubsystem.h"

ULKUnitStatusComponent::ULKUnitStatusComponent() { PrimaryComponentTick.bCanEverTick = false; }
ALKUnitBase* ULKUnitStatusComponent::Unit() const { return Cast<ALKUnitBase>(GetOwner()); }
bool ULKUnitStatusComponent::IsKing() const
{ return Unit() && Unit()->GetPassiveComponent()->GetAbility() == ELKPassiveAbility::TrollKing; }

void ULKUnitStatusComponent::InterruptActions()
{
    ALKUnitBase* Owner = Unit();
    if (!Owner) { return; }
    Owner->CancelAttackWindup(); Owner->GetMovementComponent()->Stop();
    Owner->GetActiveComponent()->InterruptMovement();
    Owner->GetAbilitySystemComponent()->CancelAllAbilities();
}
bool ULKUnitStatusComponent::Stun(float Seconds)
{
    if (!Unit() || !Unit()->IsAlive() || !Unit()->IsCombatEnabled() || !FMath::IsFinite(Seconds) || Seconds <= 0.f) { return false; }
    StunMeter = 0.f;
    if (IsKing()) { return false; }
    StunRemaining = FMath::Max(StunRemaining, Seconds); InterruptActions();
    ULKPresentationSubsystem::Sound(GetWorld(), "Stun", Unit()->GetActorLocation()); return true;
}
bool ULKUnitStatusComponent::Freeze(float Seconds)
{
    if (!Unit() || !Unit()->IsAlive() || !Unit()->IsCombatEnabled() || !FMath::IsFinite(Seconds) || Seconds <= 0.f) { return false; }
    FreezeRemaining = FMath::Max(FreezeRemaining, Seconds); InterruptActions();
    ULKPresentationSubsystem::Sound(GetWorld(), "Freeze", Unit()->GetActorLocation()); return true;
}
void ULKUnitStatusComponent::Empower(float Seconds, float Move, float Interval)
{
    if (!Unit() || !Unit()->IsAlive() || !Unit()->IsCombatEnabled() || !FMath::IsFinite(Seconds) || Seconds <= 0.f) { return; }
    EmpowerRemaining = Seconds;
    ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Empower, Unit()->GetActorLocation());
    ULKPresentationSubsystem::Sound(GetWorld(), "Empower", Unit()->GetActorLocation());
    EmpowerMove = FMath::IsFinite(Move) ? FMath::Max(1.f, Move) : 1.3f;
    EmpowerInterval = FMath::IsFinite(Interval) ? FMath::Clamp(Interval, .1f, 1.f) : .75f;
}
bool ULKUnitStatusComponent::BeginAttack()
{
    if (!IsKing()) { return false; }
    if (++AttackCount < 6) { return false; }
    AttackCount = 0; return true;
}
void ULKUnitStatusComponent::AfterAttack()
{
    if (!IsEmpowered() || !Unit() || !Unit()->IsAlive()) { return; }
    if (IsKing()) { StunMeter = 0.f; return; }
    StunMeter += .25f;
    if (StunMeter >= 1.f) { Stun(2.f); }
}
void ULKUnitStatusComponent::Ignite(const FLKCombatSource& Source, AActor* Instigator)
{
    if (!Unit() || !Unit()->IsAlive() || !Unit()->IsCombatEnabled()) { return; }
    if (BurnStacks == 0) { BurnTick = 0.f; }
    ++BurnStacks; BurnRemaining = 3.f;
    ULKPresentationSubsystem::Sound(GetWorld(), "Burn", Unit()->GetActorLocation());
    BurnSource = Source; BurnSource.Kind = ELKCombatSourceKind::Skill; BurnSource.ActionId = "Status_DragonBurn";
    BurnInstigator = Instigator;
}
float ULKUnitStatusComponent::ReceiveBreath(ELKBreathHead Head, float Damage, AActor* Instigator, const FLKCombatSource& Source)
{
    if (!Unit() || Head == ELKBreathHead::None) { return 0.f; }
    const ELKBreathHead Previous = LastBreath;
    const float Multiplier = Previous != ELKBreathHead::None && Previous != Head ? 1.2f : 1.f;
    const float Actual = LKGameplay::ApplyDamage(Unit(), Damage * Multiplier, Instigator, false, &Source);
    if (Actual <= 0.f || !Unit()->IsAlive()) { return Actual; }
    LastBreath = Head;
    if (Previous == Head)
    {
        if (Head == ELKBreathHead::Ice) { Freeze(1.f); }
        else { Ignite(Source, Instigator); }
    }
    return Actual;
}
void ULKUnitStatusComponent::TickStatus(float DeltaSeconds)
{
    if (!Unit() || !Unit()->IsAlive() || !Unit()->IsCombatEnabled() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) { return; }
    RefreshTrollSupport();
    StunRemaining = FMath::Max(0.f, StunRemaining - DeltaSeconds);
    FreezeRemaining = FMath::Max(0.f, FreezeRemaining - DeltaSeconds);
    EmpowerRemaining = FMath::Max(0.f, EmpowerRemaining - DeltaSeconds);
    if (!IsEmpowered()) { StunMeter = 0.f; }
    if (BurnStacks > 0)
    {
        BurnTick += FMath::Min(DeltaSeconds, BurnRemaining);
        BurnRemaining = FMath::Max(0.f, BurnRemaining - DeltaSeconds);
        while (BurnTick + KINDA_SMALL_NUMBER >= 1.f && BurnStacks > 0 && Unit()->IsAlive() && Unit()->IsCombatEnabled())
        {
            BurnTick = FMath::Max(0.f, BurnTick - 1.f);
            LKGameplay::ApplyDamage(Unit(), Unit()->GetMaxHealth() * .01f * BurnStacks, BurnInstigator.Get(), false, &BurnSource);
        }
        if (BurnRemaining <= 0.f) { BurnStacks = 0; BurnTick = 0.f; BurnInstigator.Reset(); }
    }
}
void ULKUnitStatusComponent::SetWarriorSupport(bool bEnabled)
{
    ALKUnitBase* Owner = Unit();
    if (!Owner || bEnabled == bWarriorSupport) { return; }
    const float Fraction = Owner->GetHealth() / FMath::Max(1.f, Owner->GetMaxHealth());
    UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
    if (WarriorHandle.IsValid()) { ASC->RemoveActiveGameplayEffect(WarriorHandle); WarriorHandle.Invalidate(); }
    bWarriorSupport = bEnabled;
    if (bEnabled)
    {
        if (!WarriorEffect)
        {
            WarriorEffect = NewObject<UGameplayEffect>(this);
            WarriorEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
            FGameplayModifierInfo& Mod = WarriorEffect->Modifiers.AddDefaulted_GetRef();
            Mod.Attribute = ULKUnitAttributeSet::GetMaxHealthAttribute();
            Mod.ModifierOp = EGameplayModOp::Multiplicitive;
            Mod.ModifierMagnitude = FScalableFloat(1.2f);
        }
        FGameplayEffectSpec Spec(WarriorEffect, ASC->MakeEffectContext(), 1.f);
        WarriorHandle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
    }
    // Attribute rescaling is neither damage nor healing: no combat event and no revival.
    ASC->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), FMath::Clamp(Fraction, 0.f, 1.f) * Owner->GetMaxHealth());
    Owner->OnHealthChanged(Owner->GetHealth(), Owner->GetMaxHealth());
}
void ULKUnitStatusComponent::RefreshTrollSupport()
{
    if (!IsKing() || !Unit()->IsAlive()) { return; }
    bool bWarrior = false, bSpear = false;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if (!It->IsTargetable() || It->IsActorBeingDestroyed() || It->GetTeam() != Unit()->GetTeam()) { continue; }
        bWarrior |= It->GetUnitId() == "Unit_TrollWarrior";
        bSpear |= It->GetUnitId() == "Unit_TrollSpearman";
    }
    SetWarriorSupport(bWarrior); bSpearSupport = bSpear;
}
void ULKUnitStatusComponent::RefreshTeamSupport(UWorld* World)
{
    if (!World) { return; }
    for (TActorIterator<ALKUnitBase> It(World); It; ++It)
    { if (It->IsAlive() && !It->IsActorBeingDestroyed()) { It->GetStatusComponent()->RefreshTrollSupport(); } }
}
void ULKUnitStatusComponent::Clear()
{
    StunRemaining = FreezeRemaining = StunMeter = EmpowerRemaining = BurnRemaining = BurnTick = 0.f;
    BurnStacks = AttackCount = 0; LastBreath = ELKBreathHead::None; BurnSource = FLKCombatSource(); BurnInstigator.Reset();
    SetWarriorSupport(false); bSpearSupport = false;
}
