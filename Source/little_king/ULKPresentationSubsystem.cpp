#include "ULKPresentationSubsystem.h"
#include "LKWorldArt.h"
#include "LKGameplayHelpers.h"
#include "ALKBattleGameMode.h"
#include "ULKGameData.h"
#include "ULKRunSubsystem.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/Texture2D.h"

void ULKPresentationSubsystem::Emit(UWorld* World, ELKVisualCue Type, FVector Location, float Radius, FVector Origin)
{
    if (!World || Location.ContainsNaN() || Origin.ContainsNaN()) { return; }
    ULKPresentationSubsystem* P = World->GetSubsystem<ULKPresentationSubsystem>();
    if (!P) { return; }
    const double Now = World->GetTimeSeconds();
    P->Prune(Now);
    // Continuous dash segments and crowded on-hit heals are coalesced spatially.
    for (const FLKVisualCue& Cue : P->Effects)
    { if (Cue.Type == Type && Now - Cue.Started < .075 && FVector::DistSquared2D(Cue.Location, Location) < 25.f * 25.f) { return; } }
    if (P->Effects.Num() >= MaxEffects) { P->Effects.RemoveAt(0); }
    const float Duration = Type == ELKVisualCue::Revive ? 1.1f : Type == ELKVisualCue::Slash || Type == ELKVisualCue::Impact ? .22f : .65f;
    P->Effects.Add({Type, Location, Origin, Now, Duration, FMath::Clamp(FMath::IsFinite(Radius) ? Radius : 70.f, 10.f, 600.f)});
}
bool ULKPresentationSubsystem::AdmitSound(FName Id, double Now)
{
    if (!LKWorldArt::SoundIds().Contains(Id) || !FMath::IsFinite(Now)) { return false; }
    if (const double* Previous = LastSound.Find(Id))
    { if (Now >= *Previous && Now - *Previous < LKWorldArt::SoundInterval(Id)) { return false; } }
    LastSound.Add(Id, Now); return true;
}
void ULKPresentationSubsystem::Fireball(UWorld* World, FVector Location, float Radius)
{
    Emit(World, ELKVisualCue::Fireball, Location, Radius);
    Sound(World, "FireballCast", Location);
    Sound(World, "FireballImpact", Location);
}
void ULKPresentationSubsystem::Sound(UWorld* World, FName Id, FVector Location, bool bUI)
{
    if (!World) { return; }
    ULKPresentationSubsystem* P = World->GetSubsystem<ULKPresentationSubsystem>();
    if (!P || !P->AdmitSound(Id, World->GetTimeSeconds())) { return; }
    if (const ALKBattleGameMode* GM = World->GetAuthGameMode<ALKBattleGameMode>())
    {
        // Battle cues obey explicit SoundMap overrides, silence and the default-set switch.
        if (bUI)
        {
            if (USoundBase* Cue = GM->FindPreloadedSound(Id)) { UGameplayStatics::PlaySound2D(World, Cue); }
        }
        else { LKGameplay::PlayOneShot(World, GM->GetGameData(), Id, Location, .8f); }
    }
    else if (bUI)
    {
        TObjectPtr<USoundBase>& Cue = P->UISounds.FindOrAdd(Id);
        if (!Cue) { Cue = LoadObject<USoundBase>(nullptr, *LKWorldArt::SoundPath(Id), nullptr, LOAD_NoWarn); }
        if (Cue) { UGameplayStatics::PlaySound2D(World, Cue); }
    }
}
void ULKPresentationSubsystem::Prune(double Now)
{ Effects.RemoveAll([Now](const FLKVisualCue& C) { return Now < C.Started || Now - C.Started >= C.Duration; }); }
void ULKPresentationSubsystem::ClearEffects() { Effects.Reset(); }

void ULKPresentationSubsystem::SetupBattlefield(ALKBattleGameMode* GM)
{
    if (!GM || !GM->GetGameData() || Ground) { return; }
    // Painted unlit sprites need stable exposure when dark/light region surfaces change.
    if (APostProcessVolume* Palette = GetWorld()->SpawnActor<APostProcessVolume>())
    {
        Palette->bUnbound = true;
        Palette->Priority = 1000.f;
        auto& Settings = Palette->Settings;
        Settings.bOverride_AutoExposureMethod = true;
        Settings.AutoExposureMethod = AEM_Manual;
        Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        Settings.AutoExposureApplyPhysicalCameraExposure = false;
        Settings.bOverride_AutoExposureBias = true;
        Settings.AutoExposureBias = 0.f;
        Settings.bOverride_BloomIntensity = true;
        Settings.BloomIntensity = 0.f;
    }
    // GameMode actors are hidden by default. A dedicated visible owner is required.
    AActor* Surface = GetWorld()->SpawnActor<AActor>();
    if (!Surface) { return; }
    Ground = NewObject<UPaperSpriteComponent>(Surface, TEXT("StorybookGround"));
    Surface->SetRootComponent(Ground);
    Surface->AddInstanceComponent(Ground);
    Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Ground->SetGenerateOverlapEvents(false);
    Ground->SetCastShadow(false);
    Ground->RegisterComponent();
    Ground->SetWorldLocation(FVector(0, 0, 2));
    Ground->SetWorldRotation(FRotationMatrix::MakeFromXY(FVector(0,1,0), FVector(0,0,1)).Rotator());
    const ULKGameData* Data = GM->GetGameData();
    Ground->SetWorldScale3D(FVector(Data->FieldHalfHeight * 2.f / 100.f, 1.f, Data->FieldHalfWidth * 2.f / 100.f));
    Backdrop = NewObject<UPaperSpriteComponent>(Surface,TEXT("StorybookSurround"));
    Surface->AddInstanceComponent(Backdrop);
    Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Backdrop->SetGenerateOverlapEvents(false); Backdrop->SetCastShadow(false);
    Backdrop->RegisterComponent();
    Backdrop->SetWorldLocation(FVector(0,0,1));
    Backdrop->SetWorldRotation(Ground->GetComponentRotation());
    Backdrop->SetWorldScale3D(Ground->GetComponentScale()*10.f);
    Backdrop->SetSpriteColor(FLinearColor(.025f,.035f,.026f));
    for (FName Id : {FName("FX_EmberBurst"),FName("FX_Leaf"),FName("FX_Dust")})
    { EffectTextures.Add(Id,LKWorldArt::EffectTexture(Id)); }
    FName Region;
    ELKDungeonNodeType Type = ELKDungeonNodeType::Battle;
    if (const UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        if (const ULKRunSubsystem* Run = GI->GetSubsystem<ULKRunSubsystem>(); Run && Run->HasRun())
        {
            const FLKRunState State = Run->GetRunState();
            Region = State.RegionId;
            Type = Run->GetNode(State.CurrentNodeId).Type;
        }
    }
    SetRegion(Region, Type);
}
void ULKPresentationSubsystem::SetRegion(FName RegionId, ELKDungeonNodeType Type)
{
    BattleType = Type;
    if (Ground)
    {
        Ground->SetSprite(LKWorldArt::GroundSprite(RegionId));
        // Gentle atmosphere only; units and health bars keep their normal colours.
        Ground->SetSpriteColor(Type == ELKDungeonNodeType::Boss ? FLinearColor(.55f,.49f,.53f)
            : Type == ELKDungeonNodeType::Elite ? FLinearColor(.63f,.61f,.53f) : FLinearColor(.68f,.74f,.63f));
        if (Backdrop) { Backdrop->SetSprite(Ground->GetSprite()); }
    }
}

void ULKPresentationSubsystem::Draw(AHUD* HUD)
{
    if (!HUD || !HUD->GetOwningPlayerController()) { return; }
    const double Now = GetWorld()->GetTimeSeconds(); Prune(Now);
    APlayerController* PC = HUD->GetOwningPlayerController();
    auto Line = [HUD,PC](FVector A, FVector B, FLinearColor C, float Width = 2.f)
    {
        FVector2D P,Q;
        if (PC->ProjectWorldLocationToScreen(A,P,true) && PC->ProjectWorldLocationToScreen(B,Q,true))
        { HUD->DrawLine(P.X,P.Y,Q.X,Q.Y,C,Width); }
    };
    auto Stamp = [this,HUD,PC](FName Id,FVector Center,float Diameter,float Alpha,float Angle=0.f)
    {
        const TObjectPtr<UTexture2D>* Texture=EffectTextures.Find(Id);
        FVector2D A,B;
        if (!Texture || !*Texture || !PC->ProjectWorldLocationToScreen(Center,A,true)
            || !PC->ProjectWorldLocationToScreen(Center+FVector(0,Diameter*.5f,0),B,true)) { return; }
        const float Width=FMath::Max(2.f,float((B-A).Size()*2));
        HUD->DrawTexture(*Texture,A.X-Width*.5f,A.Y-Width*.5f,Width,Width,0,0,1,1,
            FLinearColor(1,1,1,Alpha),BLEND_Translucent,1,false,Angle,FVector2D(.5,.5));
    };
    for (const FLKVisualCue& Cue : Effects)
    {
        const float T = FMath::Clamp(float(Now - Cue.Started) / Cue.Duration, 0.f, 1.f);
        FLinearColor C(.93f,.69f,.27f, 1.f-T);
        const bool Green = Cue.Type == ELKVisualCue::Heal || Cue.Type == ELKVisualCue::HealLink || Cue.Type == ELKVisualCue::Empower;
        if (Green) { C = FLinearColor(.40f,.85f,.46f,1.f-T); }
        if (Cue.Type == ELKVisualCue::Summon || Cue.Type == ELKVisualCue::Revive) { C = FLinearColor(.55f,.83f,.69f,1.f-T); }
        if (Cue.Type == ELKVisualCue::Sacrifice || Cue.Type == ELKVisualCue::Backstab) { C = FLinearColor(.70f,.30f,.42f,1.f-T); }
        if (Cue.Type == ELKVisualCue::Fireball) { C = FLinearColor(1.f,.34f,.08f,1.f-T); }
        if (Cue.Type == ELKVisualCue::Death || Cue.Type == ELKVisualCue::SiegeImpact) { C = FLinearColor(.72f,.65f,.48f,1.f-T); }
        const FVector Center = Cue.Location + FVector(0,0,25);
        if (Cue.Type==ELKVisualCue::Fireball)
        { Stamp("FX_EmberBurst",Center,Cue.Radius*(1.f+T),.65f*(1.f-T),T*30.f); }
        if (Cue.Type==ELKVisualCue::Death || Cue.Type==ELKVisualCue::SiegeImpact || Cue.Type==ELKVisualCue::Summon || Cue.Type==ELKVisualCue::Revive)
        { Stamp("FX_Dust",Center,Cue.Radius*(2.f+T),.75f*(1.f-T),T*15.f); }
        auto Ring = [&](float Radius, int32 Count, float Start = 0.f, float Arc = 2.f*PI)
        {
            for (int32 I=0; I<Count; ++I)
            {
                const float A = Start + Arc * I / Count, B = Start + Arc * (I+1) / Count;
                Line(Center + FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius, Center + FVector(FMath::Cos(B),FMath::Sin(B),0)*Radius,C);
            }
        };
        if (Cue.Type == ELKVisualCue::Slash || Cue.Type == ELKVisualCue::HeavyHit)
        { Ring(Cue.Radius*(.6f+.4f*T),12,-1.f+T,PI*1.2f); continue; }
        if (Cue.Type == ELKVisualCue::Dash || Cue.Type == ELKVisualCue::Backstab || Cue.Type == ELKVisualCue::HealLink)
        {
            Line(Cue.Origin+FVector(0,0,25),Center,C,3.f);
            for (int I=0; I<3; ++I) { const FVector P=FMath::Lerp(Cue.Origin,Center,FMath::Fmod(T+I*.25f,1.f)); Line(P+FVector(15,0,0),P+FVector(-15,0,0),C); }
        }
        if (Cue.Type == ELKVisualCue::Command)
        { Ring(25.f+45.f*T,24); Line(Center,Center+FVector(70,0,0),C); Line(Center+FVector(70,0,0),Center+FVector(45,35,0),C,3); continue; }
        if (Cue.Type == ELKVisualCue::Coin)
        { Ring(18.f,16); Line(Center+FVector(-10,0,0),Center+FVector(10,0,0),C); continue; }
        if (Cue.Type == ELKVisualCue::Fireball)
        {
            // Visual casting streak only: damage remains instantaneous, as before C batch.
            const FVector Tail = Center+FVector(180,0,120)*(1.f-T);
            Line(Center,Tail,C,4.f); Ring(Cue.Radius*(.25f+.75f*T),32);
        }
        else if (Cue.Type != ELKVisualCue::Death && Cue.Type != ELKVisualCue::Impact)
        { Ring(Cue.Radius*(.4f+.6f*T),24); }
        const int32 Count = Cue.Type == ELKVisualCue::Fireball ? 12 : 6;
        for (int32 I=0; I<Count; ++I)
        {
            const float Angle = I*2.f*PI/Count + (Green ? T : 0.f);
            const FVector Radial(FMath::Cos(Angle),FMath::Sin(Angle),0);
            const FVector P=Center+Radial*Cue.Radius*(.25f+.8f*T)+FVector(Green ? 45.f*T : 0.f,0,0);
            const FVector Side(-Radial.Y,Radial.X,0);
            if (Green)
            { Stamp("FX_Leaf",P,75.f,1.f-T,Angle*180.f/PI); }
            else { Line(P,P+Radial*(12.f+12.f*(1.f-T)),C,2.f); }
        }
    }
}
