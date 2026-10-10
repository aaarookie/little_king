#include "LKAIDecision.h"
#include "LKDataTypes.h"
#include "ULKCardDefinition.h"

namespace
{
float Distance(const FVector& A, const FVector& B) { return float(FVector::Dist2D(A, B)); }
float Missing(const FLKAIUnitObservation& U) { return FMath::Max(0.f, U.MaxHealth - U.Health); }
bool Within(const FLKAIUnitObservation& U, const FVector& P, float Radius) { return Distance(U.Position, P) <= Radius; }
float Nearest(const FVector& P, const TArray<FLKAIUnitObservation>& Units)
{
    float Best = 10000.f;
    for (const auto& U : Units) { Best = FMath::Min(Best, Distance(P, U.Position)); }
    return Best;
}
bool Better(const FLKAICandidate& A, const FLKAICandidate& B)
{
    if (A.Utility.Total() != B.Utility.Total()) { return A.Utility.Total() > B.Utility.Total(); }
    if (A.CardId != B.CardId) { return A.CardId.LexicalLess(B.CardId); }
    if (A.Location.X != B.Location.X) { return A.Location.X < B.Location.X; }
    return A.Location.Y < B.Location.Y;
}
}

const TCHAR* LKAIDecision::IntentName(ELKAIIntent Intent)
{
    switch (Intent)
    {
    case ELKAIIntent::Pressure: return TEXT("Pressure");
    case ELKAIIntent::Protect: return TEXT("Protect");
    case ELKAIIntent::Breach: return TEXT("Breach");
    default: return TEXT("Hold");
    }
}

FName LKAIDecision::ChooseIntentTarget(const FLKAIObservation& O, ELKAIIntent& OutIntent)
{
    OutIntent = ELKAIIntent::Hold;
    FName Best; float BestScore = 0.f;
    for (const auto& Enemy : O.Enemies)
    {
        ELKAIIntent Mode = Enemy.bWall ? ELKAIIntent::Breach : ELKAIIntent::Pressure;
        float Score = (Enemy.bHero ? 3.f : 1.f) + (Enemy.bHealer ? 2.f : 0.f) + (1.f - Enemy.HealthRatio());
        for (const auto& Ally : O.Allies)
        {
            if ((Ally.bHealer || (Ally.bHero && Ally.HealthRatio() < .45f))
                && !Enemy.bHealer && Distance(Ally.Position, Enemy.Position) < FMath::Max(300.f, Enemy.Range))
            { Score += 5.f; Mode = ELKAIIntent::Protect; break; }
        }
        // Walls only take priority when actually between a nearby ally and the battle.
        if (Enemy.bWall) { Score += Nearest(Enemy.Position, O.Allies) < 500.f ? 3.f : -2.f; }
        if (Score > BestScore) { BestScore = Score; Best = Enemy.Id; OutIntent = Mode; }
    }
    return Best;
}

FLKAIUtility LKAIDecision::Score(const ULKCardDefinition& C, const FLKUnitRow* Row, const FLKAIObservation& O,
    const FVector& P, float Silver, ELKAIIntent Intent, float Scale)
{
    FLKAIUtility S;
    // One point is roughly 60 effective damage/healing. Cost is an opportunity cost,
    // not a blanket preference for cheap cards; at low reserves it grows modestly.
    S.Cost = FMath::Max(0, C.Cost) * (Silver < C.Cost + 2.f ? .55f : .4f);
    const bool bFull = O.UnitLimit > 0 && O.Allies.Num() >= O.UnitLimit;
    if (C.CardType == ELKCardType::Spell)
    {
        switch (C.SpellEffect)
        {
        case ELKSpellEffect::Damage:
            for (const auto& U : O.Enemies) { if (Within(U, P, C.SpellRadius)) { S.Benefit += FMath::Min(U.Health, C.SpellValue * Scale) / 60.f; } }
            break;
        case ELKSpellEffect::Heal: case ELKSpellEffect::DivineBlessing:
            for (const auto& U : O.Allies)
            {
                if (C.SpellEffect != ELKSpellEffect::DivineBlessing && !Within(U, P, C.SpellRadius)) { continue; }
                const float Healing = C.SpellEffect == ELKSpellEffect::DivineBlessing
                    ? U.MaxHealth * (U.bHero ? FMath::Min(1.f, C.HeroHealPercent * Scale) : 1.f)
                    : (C.SpellValue + (U.bHero ? U.MaxHealth * C.HeroHealPercent : 0.f)) * Scale;
                S.Benefit += FMath::Min(Missing(U), Healing) / 60.f * (U.bHero ? 1.3f : 1.f);
            }
            break;
        case ELKSpellEffect::Freeze:
            for (const auto& U : O.Enemies)
            { if (!U.bControlled && Within(U, P, C.SpellRadius)) { S.Benefit += U.DPS() * FMath::Min(4.f, C.EffectDuration * Scale) / 60.f + (U.bHero ? .6f : .2f); } }
            break;
        case ELKSpellEffect::Lightning:
        {
            const FLKAIUnitObservation* Highest = nullptr; const FLKAIUnitObservation* Lowest = nullptr;
            for (const auto& U : O.Enemies)
            {
                if (U.bHero || !Within(U, P, C.SpellRadius)) { continue; }
                if (!Highest || U.Health > Highest->Health) { Highest = &U; }
                if (!Lowest || U.Health < Lowest->Health) { Lowest = &U; }
            }
            if (Highest)
            {
                const float First = FMath::Min(Highest->Health, FMath::Min(Highest->MaxHealth * C.MaxHealthDamageFraction, C.SpellValue * Scale));
                const float SecondRemaining = Lowest == Highest ? FMath::Max(0.f, Lowest->Health - First) : Lowest->Health;
                S.Benefit = (First + FMath::Min(SecondRemaining, C.SecondarySpellValue * Scale)) / 60.f;
            }
            break;
        }
        case ELKSpellEffect::SummonZone:
            if (!bFull)
            { for (const auto& U : O.Enemies) { if (Within(U, P, C.Circle.Radius)) { S.Benefit += U.bHero ? 3.5f : 2.5f; } } }
            break;
        case ELKSpellEffect::Reinforcements:
            if (!C.SummonedUnitIds.IsEmpty() && (O.UnitLimit <= 0 || O.Allies.Num() + C.SummonedUnitIds.Num() <= O.UnitLimit))
            {
                S.Benefit = C.SummonedUnitIds.Num() * 3.2f;
                S.Risk = Nearest(P, O.Enemies) < 180.f ? 2.f : 0.f;
                S.Synergy = Intent == ELKAIIntent::Protect ? 1.f : 0.f;
            }
            break;
        case ELKSpellEffect::BlackCloud:
            // Cloud conceals BOTH teams but does not silence attackers. Protect injured
            // friends currently under pressure; never reward hiding enemy damage dealers.
            for (const auto& U : O.Allies)
            {
                if (!U.bSheltered && !U.bConcealed && Within(U, P, C.SpellRadius))
                {
                    if (Nearest(U.Position, O.Enemies) < 650.f) { S.Benefit += (1.f - U.HealthRatio()) * (U.bHero ? 6.f : 3.f); }
                    S.Risk += U.bHealer ? 1.f : .15f;
                }
            }
            for (const auto& U : O.Enemies) { if (Within(U, P, C.SpellRadius)) { S.Risk += 1.25f + (U.bHealer ? 1.f : 0.f); } }
            break;
        case ELKSpellEffect::Hurricane:
        {
            // The executor moves every affected mobile unit in WORLD +Y (both teams).
            // Value actual separation, not an imaginary caster-relative knockback.
            const FVector Shift(0.f, FMath::Min(400.f, C.ForceMoveSpeed * Scale * C.EffectDuration), 0.f);
            for (const auto& U : O.Allies)
            {
                const FVector D = U.Position - P;
                if (!U.bMobile || U.bSheltered || U.bHighland || FMath::Abs(D.X) > C.SpellHalfExtents.X || FMath::Abs(D.Y) > C.SpellHalfExtents.Y) { continue; }
                const float Before = Nearest(U.Position, O.Enemies);
                const float Gain = Nearest(U.Position + Shift, O.Enemies) - Before;
                if (Before < 650.f && Gain > 30.f && (U.HealthRatio() < .6f || U.bHealer))
                { S.Benefit += Gain / 160.f * (U.bHero ? 1.5f : 1.f); }
                else { S.Risk += .5f; }
            }
            for (const auto& U : O.Enemies)
            {
                const FVector D = U.Position - P;
                if (U.bMobile && !U.bSheltered && !U.bHighland && FMath::Abs(D.X) <= C.SpellHalfExtents.X && FMath::Abs(D.Y) <= C.SpellHalfExtents.Y)
                { S.Risk += FMath::Max(0.f, Nearest(U.Position, O.Allies) - Nearest(U.Position + Shift, O.Allies)) / 180.f; }
            }
            break;
        }
        default: break;
        }
        return S;
    }
    if (!Row || bFull) { return S; }
    S.Benefit = FMath::Min(9.f, Row->BaseHealth / 130.f + Row->AttackDamage / FMath::Max(.1f, Row->AttackInterval) / 12.f);
    if (Row->bBasicAttackHeals)
    {
        float Hurt = 0.f; for (const auto& U : O.Allies) { Hurt += Missing(U); }
        S.Synergy += FMath::Min(3.f, Hurt / 120.f);
    }
    if (Row->bTargetsBuildingsOnly)
    {
        const bool bTarget = O.Enemies.ContainsByPredicate([](const auto& U) { return U.bBuilding; });
        if (!bTarget) { S.Benefit = 0.f; }
        else { S.Synergy += 2.f; }
    }
    if (Row->BuildingBehavior == ELKBuildingBehavior::Barracks) { S.Benefit += 3.f; }
    if (Row->BuildingBehavior == ELKBuildingBehavior::MageTower || Row->BuildingBehavior == ELKBuildingBehavior::HealingStation)
    {
        const bool bMage = Row->BuildingBehavior == ELKBuildingBehavior::MageTower;
        const bool bOperator = O.Allies.ContainsByPredicate([bMage, P](const auto& U)
        { return !U.bBusyWithService && !U.bSheltered && !U.bHighland && (bMage ? U.bMage : U.bHealer) && Distance(P, U.Position) < 900.f; });
        S.Benefit = bOperator ? 4.5f : 0.f;
    }
    if (Row->BuildingBehavior == ELKBuildingBehavior::Wall)
    {
        S.Benefit = O.Enemies.IsEmpty() ? 0.f : 3.f;
        S.Synergy = Intent == ELKAIIntent::Protect ? 2.f : 0.f;
        // Existing friendly walls are deliberate repair candidates, preserving their yaw.
        for (const auto& U : O.Allies) { if (U.bWall && Distance(P, U.Position) < 10.f) { S.Benefit = Missing(U) / 100.f; break; } }
    }
    const float ThreatDistance = Nearest(P, O.Enemies);
    if (ThreatDistance < 180.f && (Row->AttackType == ELKAttackType::Ranged || Row->BuildingBehavior != ELKBuildingBehavior::None)) { S.Risk += 2.f; }
    if (O.Allies.Num() > 12) { S.Risk += (O.Allies.Num() - 12) * .18f; }
    if (Intent == ELKAIIntent::Protect && Row->AttackType == ELKAttackType::Melee && !Row->bBasicAttackHeals) { S.Synergy += .75f; }
    return S;
}

void LKAIDecision::Candidates(const ULKCardDefinition& Card, const FLKUnitRow* Row, const FLKAIObservation& O,
    float Silver, ELKAIIntent Intent, int32 HandIndex, TArray<FLKAICandidate>& Out, float Scale)
{
    TArray<FVector> Points;
    const float Side = O.Team == ELKTeam::Enemy ? 1.f : -1.f;
    auto Add = [&](FVector P)
    {
        P.Z = 0.f;
        if (FMath::Abs(P.X) > O.FieldHalfSize.X || FMath::Abs(P.Y) > O.FieldHalfSize.Y) { return; }
        if (Card.CardType == ELKCardType::Spell && Card.SpellEffect != ELKSpellEffect::SummonZone
            && !Card.bGlobalPlacement && !O.bGlobalSpellPlacement && P.Y * Side < 0.f) { return; }
        if (Card.CardType != ELKCardType::Spell && P.Y * Side < 0.f) { return; }
        if (!Points.ContainsByPredicate([&](const FVector& Q) { return FVector::DistSquared2D(P,Q) < 1.f; })) { Points.Add(P); }
    };
    if (Card.CardType == ELKCardType::Spell)
    {
        if (Card.SpellEffect == ELKSpellEffect::DivineBlessing) { Add(FVector(0.f, Side * O.FieldHalfSize.Y * .5f, 0.f)); }
        else
        {
            const bool bFriends = Card.SpellEffect == ELKSpellEffect::Heal || Card.SpellEffect == ELKSpellEffect::BlackCloud || Card.SpellEffect == ELKSpellEffect::Hurricane;
            const auto& Targets = bFriends ? O.Allies : O.Enemies;
            for (const auto& U : Targets) { Add(U.Position); }
            // Adjacent stable-order pairs add cluster centers without an O(n^3) search.
            for (int32 I=1; I<Targets.Num(); ++I) { Add((Targets[I-1].Position + Targets[I].Position) * .5f); }
        }
    }
    if (Card.CardType != ELKCardType::Spell || Card.SpellEffect == ELKSpellEffect::Reinforcements)
    {
        const float Depth = Card.CardType == ELKCardType::Building ? .6f : .28f;
        for (float Y : {Depth, Depth + .17f, .12f})
        { for (float X : {-.65f,-.22f,.22f,.65f}) { Add(FVector(X * O.FieldHalfSize.X, Side * Y * O.FieldHalfSize.Y, 0.f)); } }
        if (Row && Row->BuildingBehavior == ELKBuildingBehavior::Wall)
        { for (const auto& U : O.Allies) { if (U.bWall) { Add(U.Position); } } }
    }
    TArray<FLKAICandidate> Local;
    for (const FVector& P : Points)
    {
        FLKAICandidate C; C.CardId = Card.CardId; C.HandIndex = HandIndex; C.Location = P;
        C.Utility = Score(Card, Row, O, P, Silver, Intent, Scale);
        // Wall front is +Y at zero yaw; enemy front faces toward the player (-Y).
        if (Row && Row->BuildingBehavior == ELKBuildingBehavior::Wall)
        {
            C.Yaw = O.Team == ELKTeam::Enemy ? 180.f : 0.f;
            for (const auto& U : O.Allies) { if (U.bWall && Distance(P,U.Position)<10.f) { C.Yaw = U.WallYaw; break; } }
        }
        if (C.Utility.Benefit > 0.f && C.Utility.Total() > .1f && FMath::IsFinite(C.Utility.Total())) { Local.Add(C); }
    }
    Local.Sort(Better);
    if (Local.Num() > MaxCandidatesPerCard) { Local.SetNum(MaxCandidatesPerCard); }
    Out.Append(Local);
}
