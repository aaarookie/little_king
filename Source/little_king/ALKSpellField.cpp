#include "ALKSpellField.h"
#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "ULKCardDefinition.h"
#include "ULKUnitStatusComponent.h"
#include "ULKPresentationSubsystem.h"
#include "LKGameplayHelpers.h"
#include "LKSpellExecutor.h"
#include "LKWorldArt.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

ALKSpellField::ALKSpellField()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    SetActorEnableCollision(false);
}
bool ALKSpellField::Initialize(ALKBattleGameMode* GM, const ULKCardDefinition* Card, ELKTeam Team, FVector P, float Scale, const FLKCombatSource& InSource)
{
    if (!GM || !Card || GameMode || !FMath::IsFinite(Scale) || Scale <= 0.f || LKSpellExecutor::Validate(GM,Card,Team,P,true) != ELKPlayResult::Success) { return false; }
    Effect = Card->SpellEffect;
    if (Effect != ELKSpellEffect::BlackCloud && Effect != ELKSpellEffect::Hurricane && Effect != ELKSpellEffect::Lightning) { return false; }
    GameMode = GM; CasterTeam = Team; Center = FVector(P.X, P.Y, 0.f); Source = InSource;
    Radius = Card->SpellRadius; HalfExtents = Card->SpellHalfExtents;
    Duration = Remaining = Effect == ELKSpellEffect::Lightning ? Card->SecondaryDelay : Card->EffectDuration * (Effect == ELKSpellEffect::BlackCloud ? Scale : 1.f);
    Speed = Card->ForceMoveSpeed * Scale; SecondDamage = Card->SecondarySpellValue * Scale;
    if (!FMath::IsFinite(Remaining) || Remaining <= 0.f || !FMath::IsFinite(Speed) || !FMath::IsFinite(SecondDamage)) { return false; }
    SetActorLocation(Center);
    if (Effect == ELKSpellEffect::BlackCloud) { CloudTexture=LKWorldArt::EffectTexture("FX_Dust"); RefreshCloud(); }
    if (Effect == ELKSpellEffect::Hurricane)
    {
        for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
        { const FVector Offset=It->GetActorLocation()-Center;
          if (It->IsTargetable() && !It->IsBuilding() && FMath::Abs(Offset.X)<=HalfExtents.X && FMath::Abs(Offset.Y)<=HalfExtents.Y)
          { HiddenUnits.Add(*It); It->GetStatusComponent()->ApplyWind(GetFName(),Remaining+.1f); } }
    }
    ULKPresentationSubsystem::Sound(GetWorld(), Effect == ELKSpellEffect::BlackCloud ? "BlackCloud" : Effect == ELKSpellEffect::Hurricane ? "Hurricane" : "Lightning", Center);
    return true;
}
void ALKSpellField::RefreshCloud()
{
    TSet<TWeakObjectPtr<ALKUnitBase>> Inside;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        if (It->IsTargetable() && FVector::DistSquared2D(Center, It->GetActorLocation()) <= FMath::Square(Radius))
        { Inside.Add(*It); It->GetStatusComponent()->ApplyConcealment(GetFName(), Remaining + .1f); }
    }
    for (const auto& Unit : HiddenUnits)
    { if (Unit.IsValid() && !Inside.Contains(Unit)) { Unit->GetStatusComponent()->RemoveConcealment(GetFName()); } }
    HiddenUnits = MoveTemp(Inside);
}
void ALKSpellField::ClearCloud()
{
    for (const auto& Unit : HiddenUnits)
    { if (Unit.IsValid()) { Unit->GetStatusComponent()->RemoveConcealment(GetFName()); Unit->GetStatusComponent()->RemoveWind(GetFName()); } }
    HiddenUnits.Reset();
}
void ALKSpellField::StrikeLowest()
{
    ALKUnitBase* Lowest = nullptr;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Unit = *It;
        if (!Unit->IsTargetable() || Unit->IsHero() || Unit->GetTeam() == CasterTeam || FVector::DistSquared2D(Center, Unit->GetActorLocation()) > FMath::Square(Radius)) { continue; }
        if (!Lowest || Unit->GetHealth() < Lowest->GetHealth() || (Unit->GetHealth() == Lowest->GetHealth() && Unit->GetFName().LexicalLess(Lowest->GetFName()))) { Lowest = Unit; }
    }
    if (Lowest)
    {
        GameMode->BeginCombatBatch();
        LKGameplay::ApplyDamage(Lowest, SecondDamage, nullptr, false, &Source);
        ULKPresentationSubsystem::Emit(GetWorld(), ELKVisualCue::Lightning, Lowest->GetActorLocation());
        GameMode->EndCombatBatch();
    }
}
void ALKSpellField::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GameMode || GameMode->GetPhase() != ELKGamePhase::Battle) { ClearCloud(); Destroy(); return; }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
    const float Step = FMath::Min(DeltaSeconds, Remaining);
    if (Effect == ELKSpellEffect::Hurricane)
    {
        // Short steps ensure an actor that exits the rectangle stops being driven,
        // and a long frame cannot push through a building or beyond the field.
        const int32 Steps = FMath::Clamp(FMath::CeilToInt(Step / .025f), 1, 256);
        const float Dt = Step / Steps;
        TSet<TWeakObjectPtr<ALKUnitBase>> Inside;
        for (int32 I = 0; I < Steps; ++I)
        {
            Inside.Reset();
            for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
            {
                const FVector Offset = It->GetActorLocation() - Center;
                if (!It->IsTargetable() || It->IsBuilding() || FMath::Abs(Offset.X) > HalfExtents.X || FMath::Abs(Offset.Y) > HalfExtents.Y) { continue; }
                Inside.Add(*It); It->GetStatusComponent()->ApplyWind(GetFName(), Remaining + .1f);
                It->ApplyWindDisplacement(FVector(0.f, Speed * Dt, 0.f));
                const FVector After=It->GetActorLocation()-Center;
                if (FMath::Abs(After.X)>HalfExtents.X || FMath::Abs(After.Y)>HalfExtents.Y)
                { It->GetStatusComponent()->RemoveWind(GetFName()); Inside.Remove(*It); }
            }
            for (const auto& Unit : HiddenUnits) { if (Unit.IsValid() && !Inside.Contains(Unit)) { Unit->GetStatusComponent()->RemoveWind(GetFName()); } }
            HiddenUnits = Inside;
        }
    }
    Remaining = FMath::Max(0.f, Remaining - Step);
    if (Remaining <= 0.f)
    {
        if (Effect == ELKSpellEffect::Lightning) { StrikeLowest(); }
        ClearCloud(); Destroy(); return;
    }
    if (Effect == ELKSpellEffect::BlackCloud) { RefreshCloud(); }
}
void ALKSpellField::EndPlay(const EEndPlayReason::Type Reason) { ClearCloud(); Super::EndPlay(Reason); }
void ALKSpellField::Draw(AHUD* HUD) const
{
    if (!HUD || !HUD->GetOwningPlayerController() || !GameMode || GameMode->GetPhase() != ELKGamePhase::Battle) { return; }
    auto Line = [HUD](FVector A, FVector B, FLinearColor C, float Width)
    {
        FVector2D P,Q; auto* PC = HUD->GetOwningPlayerController();
        if (PC->ProjectWorldLocationToScreen(A + FVector(0,0,20),P,true) && PC->ProjectWorldLocationToScreen(B + FVector(0,0,20),Q,true)) { HUD->DrawLine(P.X,P.Y,Q.X,Q.Y,C,Width); }
    };
    const float T = Duration - Remaining;
    if (Effect == ELKSpellEffect::Hurricane)
    {
        const FLinearColor C(.65f,.82f,.75f,.65f);
        const FVector Corners[] = {Center+FVector(-HalfExtents.X,-HalfExtents.Y,0),Center+FVector(HalfExtents.X,-HalfExtents.Y,0),Center+FVector(HalfExtents.X,HalfExtents.Y,0),Center+FVector(-HalfExtents.X,HalfExtents.Y,0)};
        for (int I=0;I<4;++I) { Line(Corners[I],Corners[(I+1)%4],C,1.5f); }
        for (int I=0;I<16;++I)
        {
            const float Y = -HalfExtents.Y + FMath::Fmod(T*Speed+I*77.f,2.f*HalfExtents.Y);
            const FVector P = Center + FVector(-HalfExtents.X + (I%5+.5f)*(2.f*HalfExtents.X/5.f),Y,0);
            const FVector Tip = P+FVector(0,70,0);
            Line(P,Tip,C,2.f); Line(Tip,Tip+FVector(16,-20,0),C,2.f); Line(Tip,Tip+FVector(-16,-20,0),C,2.f);
        }
    }
    else if (Effect == ELKSpellEffect::BlackCloud)
    {
        if (CloudTexture)
        {
            auto* PC=HUD->GetOwningPlayerController();
            const float Fade=FMath::Min(FMath::Clamp(T/.2f,0.f,1.f),FMath::Clamp(Remaining/.4f,0.f,1.f));
            for (int I=0;I<4;++I)
            {
                const float Angle=I*2.f*PI/4+T*.07f;
                const FVector P=Center+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius*.18f;
                FVector2D A,B;
                if (PC->ProjectWorldLocationToScreen(P,A,true)&&PC->ProjectWorldLocationToScreen(P+FVector(0,Radius,0),B,true))
                {
                    const float Width=FMath::Max(2.f,float((B-A).Size()*2.4f));
                    HUD->DrawTexture(CloudTexture,A.X-Width*.5f,A.Y-Width*.5f,Width,Width,0,0,1,1,FLinearColor(.06f,.10f,.085f,.5f*Fade),BLEND_Translucent,1,false,I*85.f+T*3.f,FVector2D(.5,.5));
                }
            }
        }
        for (int I=0;I<48;++I)
        {
            const float A=I*2.f*PI/48, B=(I+1)*2.f*PI/48;
            Line(Center+FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius,Center+FVector(FMath::Cos(B),FMath::Sin(B),0)*Radius,FLinearColor(.22f,.30f,.27f,.5f),1.5f);
        }
    }
}
