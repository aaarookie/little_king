#include "ALKUnitBuilding.h"
#include "ALKUnitHero.h"
#include "ALKBattleGameMode.h"
#include "LKBuildingGeometry.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "ULKCardDefinition.h"
#include "ULKGameData.h"
#include "ULKPresentationSubsystem.h"
#include "ULKUnitAnimationComponent.h"
#include "ULKUnitMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"

ALKUnitBuilding::ALKUnitBuilding()
{
    if (MovementComponent) { MovementComponent->SetComponentTickEnabled(false); }
}

void ALKUnitBuilding::OnUnitInitialized(const FLKUnitRow& Row)
{
    BuildingBehavior = Row.BuildingBehavior;
    SpawnUnitId = Row.SpawnUnitId;
    SpawnInterval = FMath::Max(.1f, Row.SpawnInterval);
    ConstructionDuration = IsWall() ? FMath::Max(0.f, Row.ConstructionSeconds) : 0.f;
    ConstructionRemaining = ConstructionDuration;
    WallLength = FMath::Max(1.f, Row.WallLength);
    WallThickness = FMath::Clamp(Row.WallThickness, 1.f, WallLength);
    ReflectDamage = FMath::Max(0.f, Row.ReflectDamage);
    WallAuraRange = FMath::Max(0.f, Row.WallAuraRange);
    SpawnTimer = AuraHealTimer = ServiceRetryTimer = 0.f;
    ReflectionLastTime.Reset();
    MovementComponent->SetComponentTickEnabled(IsMobileBuilding());
    if (IsMobileBuilding()) { AcquireRadius = 100000.f; }
    Super::OnUnitInitialized(Row);
}

void ALKUnitBuilding::SetWallYaw(float Yaw)
{
    WallYaw = FMath::UnwindDegrees(Yaw);
    if (IsWall())
    {
        if (const UPaperSprite* Sprite=SpriteComponent->GetSprite())
        {
            const float Width=Sprite->GetRenderBounds().BoxExtent.X*2.f;
            if (Width>SMALL_NUMBER)
            { BaseSpriteLocalScale.X=WallLength/Width;SpriteComponent->SetRelativeScale3D(BaseSpriteLocalScale); }
        }
        const FVector Axis(FMath::Cos(FMath::DegreesToRadians(WallYaw)), FMath::Sin(FMath::DegreesToRadians(WallYaw)), 0.f);
        SpriteComponent->SetRelativeRotation(FRotationMatrix::MakeFromXY(Axis, FVector::UpVector).Rotator());
        GetAnimationComponent()->Initialize(); // Recompute visual foot depth after rotating the wall footprint.
        if (IsConstructionComplete()) { LKBuildingGeometry::DisplaceOverlappingUnits(this); }
    }
}

FVector ALKUnitBuilding::GetWallFront() const
{
    const float Radians = FMath::DegreesToRadians(WallYaw);
    return FVector(-FMath::Sin(Radians), FMath::Cos(Radians), 0.f);
}

void ALKUnitBuilding::Tick(float DeltaSeconds)
{
    const float OperationalSeconds = FMath::Max(0.f, DeltaSeconds - ConstructionRemaining);
    if (IsAlive() && IsCombatEnabled() && ConstructionRemaining > 0.f)
    {
        ConstructionRemaining = FMath::Max(0.f, ConstructionRemaining - DeltaSeconds);
        if (IsConstructionComplete())
        {
            LKBuildingGeometry::DisplaceOverlappingUnits(this);
            ULKPresentationSubsystem::Sound(GetWorld(), "WallBuild", GetActorLocation());
            ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Empower, GetActorLocation(), WallLength * .5f);
        }
    }
    Super::Tick(DeltaSeconds);
    if (IsAlive() && IsWall() && HitFlashRemaining<=0.f)
    { SpriteComponent->SetSpriteColor(IsConstructionComplete()?FLinearColor::White:FLinearColor(.62f,.59f,.48f,.75f)); }
    if (IsDead() || !IsCombatEnabled() || IsControlled() || !IsConstructionComplete()) { return; }
    if (BuildingBehavior == ELKBuildingBehavior::Barracks)
    {
        SpawnTimer -= DeltaSeconds;
        if (SpawnTimer <= 0.f) { SpawnTimer = SpawnInterval; TrySpawnUnit(); }
    }
    if (IsWall()) { TickWallAura(OperationalSeconds); }
    if (BuildingBehavior == ELKBuildingBehavior::MageTower || BuildingBehavior == ELKBuildingBehavior::HealingStation)
    { TickServices(DeltaSeconds); }
}

void ALKUnitBuilding::UpdateStateMachine(float DeltaSeconds)
{
    if (!IsConstructionComplete()) { State = ELKUnitState::Idle; return; }
    if (IsMobileBuilding()) { Super::UpdateStateMachine(DeltaSeconds); return; }
    if (BuildingBehavior != ELKBuildingBehavior::Turret) { return; }
    TargetRetryTimer -= DeltaSeconds;
    if (TargetRetryTimer <= 0.f) { AcquireTarget(); TargetRetryTimer = .5f; }
    ALKUnitBase* TargetUnit = Cast<ALKUnitBase>(TargetActor.Get());
    if (!TargetUnit || !CanPursueTarget(TargetUnit) || DistanceTo2D(TargetUnit) > GetAttackRange())
    { ChangeTarget(nullptr); State = ELKUnitState::Idle; return; }
    State = ELKUnitState::Attacking;
    TryAttack(DeltaSeconds);
}

void ALKUnitBuilding::PerformAttack(AActor* Target)
{
    if (!IsMobileBuilding()) { Super::PerformAttack(Target); return; }
    ALKUnitBase* Victim = Cast<ALKUnitBase>(Target);
    if (!CanPursueTarget(Victim) || IsControlled() || !IsAlive() || !IsCombatEnabled()) { return; }
    const ALKUnitBuilding* Building = Cast<ALKUnitBuilding>(Victim);
    const float Percent = Building && Building->IsWall() ? .2f : .1f;
    const FLKCombatSource Source = LKGameplay::MakeSource(this, ELKCombatSourceKind::Attack, "SiegeRam");
    GetAnimationComponent()->Impact();
    LKGameplay::ApplyDamage(Victim, Victim->GetMaxHealth() * Percent, this, false, &Source);
    ULKPresentationSubsystem::Sound(GetWorld(), "SiegeRam", Victim->GetActorLocation());
    ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::HeavyHit, Victim->GetActorLocation(), 100.f);
}

void ALKUnitBuilding::OnDamageReceived(AActor* DamageInstigator, float ActualAmount, const FLKCombatSource& Source)
{
    if (!IsWall() || !IsConstructionComplete() || ReflectDamage <= 0.f || ActualAmount <= 0.f
        || Source.Kind == ELKCombatSourceKind::HealthCost || Source.Kind == ELKCombatSourceKind::Overtime
        || Source.ActionId == "WallReflect") { return; }
    ALKUnitBase* Attacker = Cast<ALKUnitBase>(DamageInstigator);
    if (!Attacker || Attacker == this || !Attacker->IsTargetable() || Attacker->GetTeam() == GetTeam()) { return; }
    const float Now = GetWorld()->GetTimeSeconds();
    if (const float* Last = ReflectionLastTime.Find(Attacker); Last && Now - *Last < .5f - KINDA_SMALL_NUMBER) { return; }
    for (auto It = ReflectionLastTime.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
    ReflectionLastTime.Add(Attacker, Now);
    const FLKCombatSource Reflected = LKGameplay::MakeSource(this, ELKCombatSourceKind::Skill, "WallReflect");
    LKGameplay::ApplyDamage(Attacker, ReflectDamage, this, false, &Reflected);
    ULKPresentationSubsystem::Sound(GetWorld(), "WallReflect", GetActorLocation());
}

bool ALKUnitBuilding::IsInWallAura(const ALKUnitBase* Unit, bool bFront) const
{
    if (!IsWall() || GetQuality() != ELKQuality::Legendary || !IsAlive() || !IsCombatEnabled()
        || IsControlled() || !IsConstructionComplete() || !Unit || Unit == this || !Unit->IsTargetable()
        || Unit->GetTeam() != GetTeam()) { return false; }
    const FVector Offset = Unit->GetActorLocation() - GetActorLocation();
    const FVector Front = GetWallFront();
    const FVector Axis(Front.Y, -Front.X, 0.f);
    const float SignedDepth = FVector::DotProduct(Offset, Front) * (bFront ? 1.f : -1.f);
    return SignedDepth > 0.f && SignedDepth <= WallAuraRange + WallThickness * .5f
        && FMath::Abs(FVector::DotProduct(Offset, Axis)) <= WallLength * .5f;
}

float ALKUnitBuilding::GetFrontAttackBonus(const ALKUnitBase* Unit) const { return IsInWallAura(Unit, true) ? .1f : 0.f; }

void ALKUnitBuilding::TickWallAura(float DeltaSeconds)
{
    if (GetQuality() != ELKQuality::Legendary) { return; }
    AuraHealTimer += DeltaSeconds;
    if (AuraHealTimer < 1.f) { return; }
    const int32 Pulses = FMath::FloorToInt(AuraHealTimer);
    AuraHealTimer -= Pulses;
    const FLKCombatSource Source = LKGameplay::MakeSource(this, ELKCombatSourceKind::Skill, "WallSanctuary");
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    { if (IsInWallAura(*It, false)) { LKGameplay::ApplyHeal(*It, It->GetMaxHealth() * .01f * Pulses, this, &Source); } }
}

bool ALKUnitBuilding::FindServiceApproach(ALKUnitBase* Unit, FVector& Result) const
{
    if (!Unit || !Unit->GetMovementComponent()) { return false; }
    const float Radius = GetBodyRadius() + Unit->GetBodyRadius() + 8.f;
    const FVector ToUnit = (Unit->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(SMALL_NUMBER, FVector(0.f, -1.f, 0.f));
    for (float Degrees : { 0.f, 30.f, -30.f, 60.f, -60.f, 90.f, -90.f, 120.f, -120.f, 150.f, -150.f, 180.f })
    {
        const FVector Candidate = GetActorLocation() + ToUnit.RotateAngleAxis(Degrees, FVector::UpVector) * Radius;
        if (Unit->GetMovementComponent()->CanStandAt(Candidate, true)
            && Unit->GetMovementComponent()->CanReach(Candidate)) { Result = Candidate; return true; }
    }
    return false;
}

void ALKUnitBuilding::SelectOccupant()
{
    struct FCandidate { ALKUnitBase* Unit; FVector Approach; int32 Priority; int32 Cost; };
    TArray<FCandidate> Candidates;
    int32 BestPriority = -1, BestCost = -1;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Unit = *It;
        if (!Unit->IsAlive() || Unit->GetTeam() != GetTeam() || (!Unit->IsHero() && !Unit->IsSoldier())
            || Unit->GetServiceBuilding() || Unit->GetPatientStation()) { continue; }
        if (BuildingBehavior == ELKBuildingBehavior::MageTower ? !Unit->IsMage() : !Unit->BasicAttackHeals()) { continue; }
        FVector Approach;
        if (!FindServiceApproach(Unit, Approach)) { continue; }
        const int32 Priority = Unit->IsHero() ? 100 : static_cast<int32>(Unit->GetQuality());
        int32 Cost = 0;
        if (!Unit->IsHero() && GameDataCached)
        { for (const ULKCardDefinition* Card : GameDataCached->CardLibrary)
            { if (Card && Card->SpawnUnitId == Unit->GetUnitId()) { Cost = FMath::Max(Cost, Card->Cost); } } }
        if (Priority > BestPriority || (Priority == BestPriority && Cost > BestCost))
        { Candidates.Reset(); BestPriority = Priority; BestCost = Cost; }
        if (Priority == BestPriority && Cost == BestCost) { Candidates.Add({ Unit, Approach, Priority, Cost }); }
    }
    if (Candidates.IsEmpty()) { return; }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Unit->GetFName().LexicalLess(B.Unit->GetFName()); });
    ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    const FCandidate& Chosen = Candidates[GM ? GM->GetBattleRandom().RandRange(0, Candidates.Num()-1) : 0];
    if (Chosen.Unit->ReserveBuildingService(this, false, Chosen.Approach)) { Occupant = Chosen.Unit; }
}

bool ALKUnitBuilding::IsServiceAvailable() const
{
    return IsAlive() && IsCombatEnabled() && IsConstructionComplete() && !IsControlled()
        && Occupant.IsValid() && Occupant->IsAlive() && Occupant->GetShelterBuilding() == this && !Occupant->IsControlled();
}

bool ALKUnitBuilding::IsPatientReady() const
{
    return IsServiceAvailable() && Patient.IsValid() && Patient->IsAlive() && Patient->GetPatientStation() == this
        && FVector::Dist2D(Patient->GetActorLocation(), GetActorLocation()) <= GetBodyRadius() + Patient->GetBodyRadius() + 28.f;
}

void ALKUnitBuilding::SelectPatient()
{
    if (!IsServiceAvailable()) { return; }
    ALKUnitBase* Best = nullptr; FVector BestApproach;
    float Lowest = .5f;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Unit = *It;
        if (!Unit->IsAlive() || Unit->GetTeam() != GetTeam() || (!Unit->IsHero() && !Unit->IsSoldier())
            || Unit->GetServiceBuilding() || Unit->GetPatientStation()) { continue; }
        const float Ratio = Unit->GetHealth() / FMath::Max(.01f, Unit->GetMaxHealth());
        if (Ratio >= Lowest) { continue; }
        FVector Approach;
        if (!FindServiceApproach(Unit, Approach)) { continue; }
        Best = Unit; BestApproach = Approach; Lowest = Ratio;
    }
    if (Best && Best->ReserveBuildingService(this, true, BestApproach)) { Patient = Best; }
}

void ALKUnitBuilding::TickServices(float DeltaSeconds)
{
    if (Occupant.IsValid() && (!Occupant->IsAlive() || Occupant->GetServiceBuilding() != this)) { ReleaseClient(Occupant.Get()); }
    if (Patient.IsValid() && (!Patient->IsAlive() || Patient->GetPatientStation() != this
        || !IsServiceAvailable() || Patient->GetHealth() >= Patient->GetMaxHealth() - .01f)) { ReleaseClient(Patient.Get()); }
    ServiceRetryTimer -= DeltaSeconds;
    if (ServiceRetryTimer > 0.f) { return; }
    ServiceRetryTimer = .5f;
    if (!Occupant.IsValid()) { SelectOccupant(); }
    if (BuildingBehavior == ELKBuildingBehavior::HealingStation && !Patient.IsValid()) { SelectPatient(); }
}

void ALKUnitBuilding::ReleaseClient(ALKUnitBase* Unit)
{
    if (!Unit) { return; }
    if (Occupant.Get() == Unit)
    {
        Occupant.Reset();
        // Losing the priest invalidates its appointment immediately, without waiting for the next station tick.
        if (ALKUnitBase* Waiting = Patient.Get()) { ReleaseClient(Waiting); }
    }
    if (Patient.Get() == Unit) { Patient.Reset(); }
    Unit->ReleaseBuildingService(this);
    ServiceRetryTimer = 0.f;
}

void ALKUnitBuilding::ReleaseAllClients()
{
    if (ALKUnitBase* Unit = Patient.Get()) { ReleaseClient(Unit); }
    if (ALKUnitBase* Unit = Occupant.Get()) { ReleaseClient(Unit); }
}

void ALKUnitBuilding::SetCombatEnabled(bool bEnabled)
{
    Super::SetCombatEnabled(bEnabled);
    if (!bEnabled) { ReleaseAllClients(); }
}

void ALKUnitBuilding::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseAllClients();
    Super::EndPlay(Reason);
}

void ALKUnitBuilding::TrySpawnUnit()
{
    ALKBattleGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
    if (!GM || !GM->GetGameData() || SpawnUnitId.IsNone()) { return; }
    if (GM->GetGameData()->MaxUnitsPerTeam > 0 && GM->CountAliveUnits(Team) >= GM->GetGameData()->MaxUnitsPerTeam) { return; }
    const float Distance = GetBodyRadius() + GM->GetGameData()->UnitBodyRadius + 8.f;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Angle = (Team == ELKTeam::Player ? PI * .5f : -PI * .5f) + Index * PI * .25f;
        const FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
        if (GM->SpawnUnitForTeam(SpawnUnitId, Team, Location))
        {
            GetAnimationComponent()->Attack();
            UE_LOG(LogLKUnit, Log, TEXT("[Building] %s 出兵 %s"), *UnitId.ToString(), *SpawnUnitId.ToString());
            return;
        }
    }
}
