#include "ALKUnitBase.h"
#include "ALKUnitBuilding.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "PaperSpriteComponent.h"
#include "ULKUnitActiveComponent.h"
#include "ULKUnitMovementComponent.h"
#include "ULKPresentationSubsystem.h"

ALKUnitBuilding* ALKUnitBase::GetServiceBuilding() const { return ServiceBuilding.Get(); }
ALKUnitBuilding* ALKUnitBase::GetPatientStation() const { return PatientStation.Get(); }
ALKUnitBuilding* ALKUnitBase::GetShelterBuilding() const
{
    ALKUnitBuilding* Building = ServiceBuilding.Get();
    return bInsideShelter && IsAlive() && Building && Building->IsAlive() ? Building : nullptr;
}
bool ALKUnitBase::IsSheltered() const { return GetShelterBuilding() != nullptr; }
bool ALKUnitBase::IsBuildingServiceMoving() const
{
    return PatientStation.IsValid() || (ServiceBuilding.IsValid() && !IsSheltered());
}
float ALKUnitBase::GetSkillCooldownRate() const
{
    const ALKUnitBuilding* Building = GetShelterBuilding();
    return Building && Building->BuildingBehavior == ELKBuildingBehavior::MageTower && Building->IsCombatEnabled()
        && Building->IsConstructionComplete() ? 2.f : 1.f;
}
float ALKUnitBase::GetStationHealingMultiplier(const ALKUnitBase* Target) const
{
    const ALKUnitBuilding* Building = GetShelterBuilding();
    return Building && Building->BuildingBehavior == ELKBuildingBehavior::HealingStation
        && Building->GetPatient() == Target && Building->IsPatientReady() ? 1.5f : 1.f;
}

bool ALKUnitBase::ReserveBuildingService(ALKUnitBuilding* Building, bool bAsPatient, const FVector& Approach)
{
    if (!Building || !Building->IsAlive() || !IsAlive() || IsBuilding() || IsHighlandResident() || ServiceBuilding.IsValid() || PatientStation.IsValid()) { return false; }
    CancelManualMove();
    CancelAttackWindup();
    ChangeTarget(nullptr);
    AbilitySystem->CancelAllAbilities();
    ActiveComponent->Stop();
    if (bAsPatient) { PatientStation = Building; } else { ServiceBuilding = Building; }
    ServiceApproach = Approach;
    ServicePathCheckTimer = 0.f;
    MovementComponent->MoveToward(Approach, GetMoveSpeed());
    State = ELKUnitState::Moving;
    return true;
}

void ALKUnitBase::EnterShelter(ALKUnitBuilding* Building)
{
    if (ServiceBuilding.Get() != Building || !Building || !Building->IsAlive() || !IsAlive()) { return; }
    bInsideShelter = true;
    CancelAttackWindup();
    ChangeTarget(nullptr);
    MovementComponent->Stop();
    MovementComponent->ResetVisualTravel();
    SetActorLocation(Building->GetActorLocation());
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpriteComponent->SetVisibility(false);
    ULKPresentationSubsystem::Sound(GetWorld(), "TowerShelter", Building->GetActorLocation());
}

void ALKUnitBase::ReleaseBuildingService(ALKUnitBuilding* Building)
{
    const bool bWasOccupant = ServiceBuilding.Get() == Building;
    if (PatientStation.Get() == Building) { PatientStation.Reset(); }
    if (bWasOccupant)
    {
        ServiceBuilding.Reset();
        const bool bWasInside = bInsideShelter;
        bInsideShelter = false;
        SpriteComponent->SetVisibility(true);
        if (bWasInside && IsAlive())
        {
            FVector Exit = ServiceApproach;
            bool bFound = MovementComponent->CanStandAt(Exit, true);
            if (!bFound && MovementComponent->CanStandAt(GetActorLocation(), true)) { Exit = GetActorLocation(); bFound = true; }
            if (!bFound)
            {
                const FVector Center = GetActorLocation();
                for (float Radius : { 110.f, 150.f, 220.f, 300.f })
                {
                    for (int32 I = 0; I < 16; ++I)
                    {
                        const float R = 2.f * PI * I / 16.f;
                        const FVector Candidate = Center + FVector(FMath::Cos(R), FMath::Sin(R), 0.f) * Radius;
                        if (MovementComponent->CanStandAt(Candidate, true)) { Exit = Candidate; bFound = true; break; }
                    }
                    if (bFound) { break; }
                }
            }
            if (bFound) { SetActorLocation(Exit); }
            BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
    }
    CancelAttackWindup();
    MovementComponent->Stop();
    MovementComponent->ResetVisualTravel();
    ChangeTarget(nullptr);
    TargetRetryTimer = 0.f;
}

void ALKUnitBase::ClearBuildingService()
{
    if (ALKUnitBuilding* Building = PatientStation.Get()) { Building->ReleaseClient(this); }
    if (ALKUnitBuilding* Building = ServiceBuilding.Get()) { Building->ReleaseClient(this); }
    PatientStation.Reset(); ServiceBuilding.Reset(); bInsideShelter = false;
    if (SpriteComponent) { SpriteComponent->SetVisibility(true); }
}

bool ALKUnitBase::TickBuildingService(float DeltaSeconds)
{
    ALKUnitBuilding* Building = PatientStation.IsValid() ? PatientStation.Get() : ServiceBuilding.Get();
    if (!Building) { return false; }
    if (!Building->IsAlive() || !Building->IsCombatEnabled()) { Building->ReleaseClient(this); return false; }
    if (IsSheltered())
    {
        SetActorLocation(Building->GetActorLocation());
        MovementComponent->Stop();
        if (Building->BuildingBehavior == ELKBuildingBehavior::HealingStation)
        {
            ALKUnitBase* Patient = Building->IsPatientReady() ? Building->GetPatient() : nullptr;
            ChangeTarget(Patient);
            State = Patient ? ELKUnitState::Attacking : ELKUnitState::Idle;
            if (Patient) { TryAttack(DeltaSeconds); }
            return true;
        }
        return false;
    }
    // Check crowds as well as static navigation. A legal path can still end at a point occupied by a friendly unit.
    // Arrival also rechecks immediately, so reaching a reserved point never permits entering through another body.
    ServicePathCheckTimer -= DeltaSeconds;
    if (ServicePathCheckTimer <= 0.f || FVector::Dist2D(GetActorLocation(), ServiceApproach) <= 18.f)
    {
        ServicePathCheckTimer = .5f;
        if (!MovementComponent->CanStandAt(ServiceApproach, true) || !MovementComponent->CanReach(ServiceApproach))
        {
            FVector Alternative;
            if (!Building->FindServiceApproach(this, Alternative)) { Building->ReleaseClient(this); return false; }
            ServiceApproach = Alternative;
        }
    }
    if (FVector::Dist2D(GetActorLocation(), ServiceApproach) <= 18.f)
    {
        ChangeTarget(nullptr);
        MovementComponent->Stop();
        State = ELKUnitState::Idle;
        if (ServiceBuilding.IsValid()) { EnterShelter(Building); }
        return true;
    }
    CancelAttackWindup();
    ChangeTarget(nullptr);
    State = ELKUnitState::Moving;
    MovementComponent->MoveToward(ServiceApproach, GetMoveSpeed());
    return true;
}
