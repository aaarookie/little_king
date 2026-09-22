#include "ULKUnitActiveComponent.h"
#include "ALKUnitBase.h"
#include "ALKBattleGameMode.h"
#include "LKGameplayHelpers.h"
#include "ULKCardDefinition.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"
#include "ULKUnitMovementComponent.h"
#include "ULKUnitStatusComponent.h"
#include "ULKPresentationSubsystem.h"
#include "ULKUnitAnimationComponent.h"
#include "EngineUtils.h"

namespace
{
TArray<ALKUnitBase*> LivingUnits(UWorld* World)
{
    TArray<ALKUnitBase*> Units;
    for (TActorIterator<ALKUnitBase> It(World); It; ++It) { if (It->IsTargetable()) { Units.Add(*It); } }
    Units.Sort([](const ALKUnitBase& A, const ALKUnitBase& B) { return A.GetFName().LexicalLess(B.GetFName()); });
    return Units;
}
float Positive(float Value, float Fallback)
{
    return FMath::IsFinite(Value) && Value > 0.f ? Value : Fallback;
}
}

ULKUnitActiveComponent::ULKUnitActiveComponent() { PrimaryComponentTick.bCanEverTick = false; }

void ULKUnitActiveComponent::Initialize(const FLKUnitRow& Row)
{
    Ability = Row.ActiveAbility;
    Cooldown = Positive(Row.SkillCooldown, 8.f);
    CooldownRemaining = Cooldown;
    DamageMultiplier = Positive(Row.SkillDamageMultiplier, 2.f);
    SilverChance = FMath::IsFinite(Row.SkillSilverChance) ? FMath::Clamp(Row.SkillSilverChance, 0.f, 1.f) : .3f;
    DashDistance = Positive(Row.SkillDashDistance, 450.f); DashSpeed = Positive(Row.SkillDashSpeed, 1800.f);
    HitRadius = Positive(Row.SkillHitRadius, 100.f); KnockbackDistance = Positive(Row.SkillKnockbackDistance, 120.f);
    Stop(); LastSpellId = NAME_None;
    EmpowerDuration = Positive(Row.EmpowerDuration, 8.f);
    EmpowerMove = Positive(Row.EmpowerMoveMultiplier, 1.3f);
    EmpowerInterval = Positive(Row.EmpowerIntervalMultiplier, .75f);
}

void ULKUnitActiveComponent::Stop()
{
    DashRemaining = 0.f; LockedTarget.Reset(); DashHits.Reset();
}

ALKUnitBase* ULKUnitActiveComponent::GetLockedTarget() const
{
    ALKUnitBase* Target = LockedTarget.Get();
    const ALKUnitBase* Unit = Cast<ALKUnitBase>(GetOwner());
    return Unit && Unit->IsAlive() && Target && Unit->CanPursueTarget(Target) ? Target : nullptr;
}

void ULKUnitActiveComponent::ObserveDefeat(const ALKUnitBase* Victim)
{
    if (Ability == ELKActiveAbility::Backstab && Victim && LockedTarget.Get() == Victim)
    {
        LockedTarget.Reset(); CooldownRemaining = 0.f;
    }
}

bool ULKUnitActiveComponent::TickAbility(float DeltaSeconds)
{
    ALKUnitBase* Unit = Cast<ALKUnitBase>(GetOwner());
    if (!Unit || !Unit->IsAlive() || !Unit->IsCombatEnabled() || Ability == ELKActiveAbility::None) { return false; }
    if (DeltaSeconds < 0.f || !FMath::IsFinite(DeltaSeconds)) { return IsDashing(); }
    CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaSeconds);
    if (Unit->IsControlled()) { InterruptMovement(); return false; }
    if (IsDashing()) { TickDash(DeltaSeconds); return true; }
    if (CooldownRemaining <= 0.f) { TryActivate(); }
    return IsDashing();
}

bool ULKUnitActiveComponent::TryActivate()
{
    ALKUnitBase* Unit = Cast<ALKUnitBase>(GetOwner());
    ALKBattleGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
    if (!Unit || !Unit->IsAlive() || !Unit->IsCombatEnabled() || !GM || GM->GetPhase() != ELKGamePhase::Battle
        || Unit->IsManualMoving() || Unit->IsControlled() || CooldownRemaining > 0.f || IsDashing() || Ability == ELKActiveAbility::None) { return false; }
    // Set before damage: a kill from the skill itself can reset this cooldown synchronously.
    CooldownRemaining = Cooldown;
    bool bCast = false;
    switch (Ability)
    {
    case ELKActiveAbility::Backstab: bCast = Backstab(); break;
    case ELKActiveAbility::MimicSpell: bCast = MimicSpell(); break;
    case ELKActiveAbility::MakeWay: bCast = StartDash(); break;
    case ELKActiveAbility::TrollEmpower: bCast = TrollEmpower(); break;
    default: break;
    }
    if (!bCast) { CooldownRemaining = .25f; }
    else { Unit->GetAnimationComponent()->Attack(); }
    return bCast;
}

bool ULKUnitActiveComponent::Backstab()
{
    ALKUnitBase* Unit = CastChecked<ALKUnitBase>(GetOwner());
    ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    ALKUnitBase* Target = nullptr;
    double BestDistance = -1;
    for (ALKUnitBase* Candidate : LivingUnits(GetWorld()))
    {
        const double Distance = FVector::DistSquared2D(Unit->GetActorLocation(), Candidate->GetActorLocation());
        if (Unit->CanPursueTarget(Candidate) && Distance > BestDistance) { BestDistance = Distance; Target = Candidate; }
    }
    if (!Target) { return false; }
    FVector Facing = Target->GetTeam() == ELKTeam::Player ? FVector(0,1,0) : FVector(0,-1,0);
    if (AActor* Opponent = Target->GetTarget())
    {
        const FVector ToOpponent = (Opponent->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
        if (!ToOpponent.IsNearlyZero()) { Facing = ToOpponent; }
    }
    bool bMoved = false;
    const FVector BeforeTeleport = Unit->GetActorLocation();
    const float Minimum = Target->GetBodyRadius() + Unit->GetBodyRadius() + 4.f;
    for (float Radius : { Minimum, Minimum + 40.f, Minimum + 80.f })
    {
        for (float Angle : { 0.f, 25.f, -25.f, 50.f, -50.f, 75.f, -75.f })
        {
            const FVector Position = Target->GetActorLocation() - Facing.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
            if (Unit->GetMovementComponent()->TeleportToFreePoint(Position)) { bMoved = true; break; }
        }
        if (bMoved) { break; }
    }
    if (!bMoved) { return false; }
    ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Backstab, Unit->GetActorLocation(), 60.f, BeforeTeleport);
    ULKPresentationSubsystem::Sound(GetWorld(), "Backstab", Unit->GetActorLocation());
    Unit->CancelAttackWindup();
    LockedTarget = Target; Unit->SetTarget(Target);
    const FLKCombatSource Source = LKGameplay::MakeSource(Unit, ELKCombatSourceKind::Skill, "Skill_Backstab");
    GM->BeginCombatBatch();
    LKGameplay::ApplyDamage(Target, Unit->GetAttackDamage() * DamageMultiplier, Unit, false, &Source);
    if (GM->GetBattleRandom().FRand() < SilverChance)
    { if (ULKSilverComponent* Silver = GM->GetTeamSilver(Unit->GetTeam()))
        { const float Before = Silver->GetSilver(); Silver->AddSilver(1.f);
          if (Silver->GetSilver() > Before) { ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Coin, Unit->GetActorLocation()); ULKPresentationSubsystem::Sound(GetWorld(), "Coin", Unit->GetActorLocation()); } } }
    GM->EndCombatBatch();
    return true;
}

bool ULKUnitActiveComponent::TrollEmpower()
{
    ALKUnitBase* Unit = CastChecked<ALKUnitBase>(GetOwner());
    ALKUnitBase* Target = nullptr;
    double BestDistance = TNumericLimits<double>::Max();
    bool bFoundKing = false;
    for (ALKUnitBase* Other : LivingUnits(GetWorld()))
    {
        if (Other == Unit || Other->GetTeam() != Unit->GetTeam() || Other->GetRace() != ELKRace::Troll) { continue; }
        const bool bKing = Other->GetUnitId() == "Unit_TrollKing";
        const double Distance = FVector::DistSquared2D(Unit->GetActorLocation(), Other->GetActorLocation());
        if ((bKing && !bFoundKing) || (bKing == bFoundKing && Distance < BestDistance))
        { Target = Other; BestDistance = Distance; bFoundKing = bKing; }
    }
    Unit->CancelAttackWindup();
    Unit->GetStatusComponent()->Empower(EmpowerDuration, EmpowerMove, EmpowerInterval);
    if (Target) { Target->GetStatusComponent()->Empower(EmpowerDuration, EmpowerMove, EmpowerInterval); }
    return true;
}

bool ULKUnitActiveComponent::MimicSpell()
{
    ALKUnitBase* Unit = CastChecked<ALKUnitBase>(GetOwner());
    ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    TArray<const ULKCardDefinition*> Spells;
    if (const ULKDeckState* Deck = GM->GetTeamDeck(Unit->GetTeam()))
    {
        TArray<FName> Ids = Deck->GetAllCards(); Ids.Sort(FNameLexicalLess());
        for (FName Id : Ids)
        {
            const ULKCardDefinition* Card = GM->FindCard(Id);
            if (Card && Card->CardType == ELKCardType::Spell && Card->SpellGrade <= ELKSpellGrade::Novice3
                && Card->SpellEffect != ELKSpellEffect::None && Card->SpellValue > 0.f && FMath::IsFinite(Card->SpellValue)
                && Card->SpellRadius > 0.f && FMath::IsFinite(Card->SpellRadius)) { Spells.Add(Card); }
        }
    }
    const ULKCardDefinition* Chosen = Spells.IsEmpty() ? nullptr : Spells[GM->GetBattleRandom().RandRange(0, Spells.Num() - 1)];
    if (!Chosen)
    {
        const ULKCardDefinition* Fireball = GM->FindCard("Spell_Fireball");
        if (Fireball && Fireball->SpellGrade == ELKSpellGrade::Novice1 && Fireball->SpellEffect == ELKSpellEffect::Damage
            && FMath::IsFinite(Fireball->SpellValue) && Fireball->SpellValue > 0.f && FMath::IsFinite(Fireball->SpellRadius) && Fireball->SpellRadius > 0.f) { Chosen = Fireball; }
    }
    const ELKSpellEffect Effect = Chosen ? Chosen->SpellEffect : ELKSpellEffect::Damage;
    const float Value = Chosen ? Chosen->SpellValue : 60.f, Radius = Chosen ? Chosen->SpellRadius : 250.f;
    const bool bHeal = Effect == ELKSpellEffect::Heal;
    const TArray<ALKUnitBase*> Units = LivingUnits(GetWorld());
    FVector Center = Unit->GetActorLocation();
    float BestScore = -1.f;
    bool bHasTarget = false;
    for (ALKUnitBase* Candidate : Units)
    {
        if ((Candidate->GetTeam() == Unit->GetTeam()) != bHeal) { continue; }
        float Score = 0.f;
        for (ALKUnitBase* Other : Units)
        {
            if ((Other->GetTeam() == Unit->GetTeam()) == bHeal && FVector::Dist2D(Candidate->GetActorLocation(), Other->GetActorLocation()) <= Radius)
            { Score += bHeal ? FMath::Min(Value, Other->GetMaxHealth() - Other->GetHealth()) : (Other->IsHero() ? 2.f : 1.f); }
        }
        if (Score > BestScore) { BestScore = Score; Center = Candidate->GetActorLocation(); bHasTarget = true; }
    }
    if (!bHasTarget) { return false; }
    Unit->CancelAttackWindup();
    LastSpellId = Chosen ? Chosen->CardId : FName("Spell_Fireball");
    ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Mimic, Unit->GetActorLocation());
    ULKPresentationSubsystem::Sound(GetWorld(), "Mimic", Unit->GetActorLocation());
    const FLKCombatSource Source = LKGameplay::MakeSource(Unit, ELKCombatSourceKind::Skill, LastSpellId);
    GM->BeginCombatBatch();
    if (LastSpellId == "Spell_Fireball") { GM->NotifyFireballCast(Center, Radius); }
    for (ALKUnitBase* Other : Units)
    {
        if (FVector::Dist2D(Center, Other->GetActorLocation()) > Radius) { continue; }
        if (bHeal && Other->GetTeam() == Unit->GetTeam()) { LKGameplay::ApplyHeal(Other, Value, Unit, &Source); }
        else if (!bHeal && Other->GetTeam() != Unit->GetTeam()) { LKGameplay::ApplyDamage(Other, Value, Unit, false, &Source); }
    }
    GM->EndCombatBatch();
    return true;
}

bool ULKUnitActiveComponent::StartDash()
{
    ALKUnitBase* Unit = CastChecked<ALKUnitBase>(GetOwner());
    ALKUnitBase* Target = Cast<ALKUnitBase>(Unit->GetTarget());
    if (!Unit->CanPursueTarget(Target))
    {
        float Nearest = TNumericLimits<float>::Max();
        for (ALKUnitBase* Other : LivingUnits(GetWorld()))
        {
            const float Distance = FVector::DistSquared2D(Unit->GetActorLocation(), Other->GetActorLocation());
            if (Unit->CanPursueTarget(Other) && Distance < Nearest) { Target = Other; Nearest = Distance; }
        }
    }
    if (!Target) { return false; }
    Unit->SetTarget(Target); // Existing taunt priority still applies to this mercenary.
    Target = Cast<ALKUnitBase>(Unit->GetTarget());
    if (!Target) { return false; }
    DashDirection = (Target->GetActorLocation() - Unit->GetActorLocation()).GetSafeNormal2D();
    if (DashDirection.IsNearlyZero()) { return false; }
    Unit->CancelAttackWindup(); Unit->GetMovementComponent()->Stop();
    DashRemaining = DashDistance; DashHits.Reset();
    ULKPresentationSubsystem::Sound(GetWorld(), "Dash", Unit->GetActorLocation());
    return true;
}

void ULKUnitActiveComponent::TickDash(float DeltaSeconds)
{
    ALKUnitBase* Unit = CastChecked<ALKUnitBase>(GetOwner());
    ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    const FVector Start = Unit->GetActorLocation();
    const float Step = FMath::Min(DashRemaining, DashSpeed * DeltaSeconds);
    const FVector End = Unit->GetMovementComponent()->MoveSkillDelta(DashDirection * Step);
    if (FVector::DistSquared2D(Start, End) > 1.f) { ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Dash, End, 40.f, Start); }
    const FVector Side(-DashDirection.Y, DashDirection.X, 0.f);
    const FLKCombatSource Source = LKGameplay::MakeSource(Unit, ELKCombatSourceKind::Skill, "Skill_MakeWay");
    GM->BeginCombatBatch();
    for (ALKUnitBase* Target : LivingUnits(GetWorld()))
    {
        if (Target->GetTeam() == Unit->GetTeam() || DashHits.Contains(Target)) { continue; }
        const FVector Closest = FMath::ClosestPointOnSegment(Target->GetActorLocation(), Start, End);
        if (FVector::DistSquared2D(Closest, Target->GetActorLocation()) > FMath::Square(HitRadius + Target->GetBodyRadius())) { continue; }
        // The hit radius must not reach through a different building or camp.
        bool bBlocked = false;
        for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
        {
            if (*It == Target || *It == Unit || !It->IsAlive() || !It->IsBuilding()) { continue; }
            const FVector Projection = FMath::ClosestPointOnSegment(It->GetActorLocation(), Closest, Target->GetActorLocation());
            if (FVector::DistSquared2D(Projection, It->GetActorLocation()) < FMath::Square(It->GetBodyRadius())) { bBlocked = true; break; }
        }
        if (bBlocked) { continue; }
        DashHits.Add(Target);
        LKGameplay::ApplyDamage(Target, Unit->GetAttackDamage() * DamageMultiplier, Unit, false, &Source);
        if (Target->IsAlive() && !Target->IsBuilding())
        {
            const double Offset = FVector::DotProduct(Target->GetActorLocation() - Start, Side);
            const float Sign = FMath::IsNearlyZero(Offset) ? (DashHits.Num() % 2 ? 1.f : -1.f) : (Offset > 0 ? 1.f : -1.f);
            Target->CancelAttackWindup();
            Target->GetMovementComponent()->MoveSkillDelta(Side * Sign * KnockbackDistance);
        }
    }
    GM->EndCombatBatch();
    if (!Unit->IsAlive() || !Unit->IsCombatEnabled() || GM->GetPhase() != ELKGamePhase::Battle) { Stop(); return; }
    DashRemaining = FVector::Dist2D(Start, End) + .5f < Step ? 0.f : FMath::Max(0.f, DashRemaining - Step);
}
