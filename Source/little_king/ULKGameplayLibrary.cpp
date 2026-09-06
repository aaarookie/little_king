#include "ULKGameplayLibrary.h"
#include "ALKUnitBase.h"
#include "ALKBattleGameMode.h"
#include "LKGameplayHelpers.h"
#include "ULKUnitAttributeSet.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    int32 ApplyArea(AActor* CenterActor, float Radius, float Amount, AActor* Caster, bool bHeal)
    {
        ALKUnitBase* SourceUnit = Cast<ALKUnitBase>(Caster);
        if (!IsValid(CenterActor) || !SourceUnit || !SourceUnit->IsTargetable() || SourceUnit->IsManualMoving()
            || !FMath::IsFinite(Radius) || Radius <= 0.f || !FMath::IsFinite(Amount) || Amount <= 0.f) { return 0; }
        UWorld* World = CenterActor->GetWorld();
        ALKBattleGameMode* GM = World->GetAuthGameMode<ALKBattleGameMode>();
        if (GM && GM->GetPhase() != ELKGamePhase::Battle) { return 0; }
        FLKCombatSource Source = LKGameplay::MakeSource(Caster, ELKCombatSourceKind::Skill, SourceUnit->GetUnitId());
        TArray<ALKUnitBase*> Targets;
        for (TActorIterator<ALKUnitBase> It(World); It; ++It)
        {
            ALKUnitBase* Unit = *It;
            if (Unit->IsTargetable() && ((Unit->GetTeam() == SourceUnit->GetTeam()) == bHeal)
                && FVector::Dist2D(CenterActor->GetActorLocation(), Unit->GetActorLocation()) <= Radius) { Targets.Add(Unit); }
        }
        if (GM) { GM->BeginCombatBatch(); }
        int32 Count = 0;
        for (ALKUnitBase* Unit : Targets)
        {
            const float Actual = bHeal ? LKGameplay::ApplyHeal(Unit, Amount, Caster, &Source)
                : LKGameplay::ApplyDamage(Unit, Amount, Caster, false, &Source);
            if (Actual > 0.f) { ++Count; }
        }
        if (GM) { GM->EndCombatBatch(); }
        return Count;
    }
}

int32 ULKGameplayLibrary::LK_ApplyDamageInRadius(AActor* CenterActor, float Radius, float Damage, AActor* Caster)
{
    return ApplyArea(CenterActor, Radius, Damage, Caster, false);
}
int32 ULKGameplayLibrary::LK_ApplyHealInRadius(AActor* CenterActor, float Radius, float HealAmount, AActor* Caster)
{
    return ApplyArea(CenterActor, Radius, HealAmount, Caster, true);
}
AActor* ULKGameplayLibrary::LK_GetNearestEnemy(AActor* Unit)
{
    ALKUnitBase* Self = Cast<ALKUnitBase>(Unit);
    if (!Self || !Self->IsTargetable() || Self->IsManualMoving()) { return nullptr; }
    if (AActor* Taunter = Self->FindNearestEnemy(true)) { return Taunter; }
    if (ALKUnitBase* Forced = Cast<ALKUnitBase>(Self->ForcedTargetActor.Get()))
    { if (Self->CanPursueTarget(Forced)) { return Forced; } }
    return Self->FindNearestEnemy();
}
float ULKGameplayLibrary::LK_GetUnitHealth(AActor* Unit) { return LKGameplay::GetAttributeValue(Unit, ULKUnitAttributeSet::GetHealthAttribute(), 0.f); }
float ULKGameplayLibrary::LK_GetUnitMaxHealth(AActor* Unit) { return LKGameplay::GetAttributeValue(Unit, ULKUnitAttributeSet::GetMaxHealthAttribute(), 0.f); }
