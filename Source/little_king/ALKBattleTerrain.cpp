#include "ALKBattleTerrain.h"
#include "ALKUnitBase.h"
#include "ULKUnitStatusComponent.h"
#include "LKPolishArt.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

namespace
{
bool RectanglesTouch(const LKNavigation::FObstacle& SA,const LKNavigation::FObstacle& SB)
{
    const FVector2D EA=SA.BoxHalfExtent+FVector2D(SA.Radius),EB=SB.BoxHalfExtent+FVector2D(SB.Radius);
    const FVector AX=FRotator(0,SA.BoxYaw,0).Vector(), AY=FVector::CrossProduct(FVector::UpVector,AX);
    const FVector BX=FRotator(0,SB.BoxYaw,0).Vector(), BY=FVector::CrossProduct(FVector::UpVector,BX);
    const FVector D=SA.Center-SB.Center;
    for (const FVector& Axis : {AX,AY,BX,BY})
    {
        const double RA=FMath::Abs(FVector::DotProduct(AX,Axis))*EA.X+FMath::Abs(FVector::DotProduct(AY,Axis))*EA.Y;
        const double RB=FMath::Abs(FVector::DotProduct(BX,Axis))*EB.X+FMath::Abs(FVector::DotProduct(BY,Axis))*EB.Y;
        if (FMath::Abs(FVector::DotProduct(D,Axis))>RA+RB+.01) { return false; }
    }
    return true;
}
FLinearColor TerrainColor(ELKBattleTerrainKind Kind)
{
    switch(Kind)
    {
    case ELKBattleTerrainKind::Brush:return FLinearColor(.12f,.27f,.10f);
    case ELKBattleTerrainKind::Swamp:return FLinearColor(.23f,.29f,.19f);
    case ELKBattleTerrainKind::Campfire:return FLinearColor(.30f,.21f,.12f);
    case ELKBattleTerrainKind::Highland:return FLinearColor(.39f,.37f,.30f);
    case ELKBattleTerrainKind::Rock:return FLinearColor(.33f,.35f,.32f);
    case ELKBattleTerrainKind::Tree:return FLinearColor(.17f,.29f,.13f);
    case ELKBattleTerrainKind::Road:return FLinearColor(.43f,.34f,.22f);
    case ELKBattleTerrainKind::Water:return FLinearColor(.17f,.33f,.35f);
    case ELKBattleTerrainKind::Flowers:return FLinearColor(.40f,.33f,.31f);
    default:return FLinearColor(.26f,.36f,.16f);
    }
}
}

ALKBattleTerrain::ALKBattleTerrain()
{
    PrimaryActorTick.bCanEverTick=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TerrainRoot")));
    SetActorEnableCollision(false); // Custom navigation is the authoritative solid geometry.
}
bool ALKBattleTerrain::Initialize(ELKBattleTerrainKind InKind,FVector Center,FVector2D SizeCm,float YawDegrees)
{
    if (Center.ContainsNaN() || SizeCm.ContainsNaN() || !FMath::IsFinite(YawDegrees)
        || SizeCm.X<1.f || SizeCm.Y<1.f || SizeCm.X>10000.f || SizeCm.Y>10000.f
        || uint8(InKind)>uint8(ELKBattleTerrainKind::Flowers)) { return false; }
    Kind=InKind; Size=SizeCm; bInitialized=true;
    SetActorLocationAndRotation(FVector(Center.X,Center.Y,0.f),FRotator(0,YawDegrees,0));
    RebuildBrushConnections(GetWorld()); BuildVisuals();
    return true;
}
void ALKBattleTerrain::EndPlay(const EEndPlayReason::Type Reason)
{
    bInitialized=false; RebuildBrushConnections(GetWorld()); Super::EndPlay(Reason);
}
bool ALKBattleTerrain::IsBlocking() const
{
    return bInitialized && (Kind==ELKBattleTerrainKind::Campfire || Kind==ELKBattleTerrainKind::Highland
        || Kind==ELKBattleTerrainKind::Rock || Kind==ELKBattleTerrainKind::Tree);
}
LKNavigation::FObstacle ALKBattleTerrain::Shape(float Clearance) const
{
    return {GetActorLocation(),FMath::Max(0.f,Clearance),FVector::ZeroVector,Size*.5f,float(GetActorRotation().Yaw)};
}
bool ALKBattleTerrain::ContainsPoint(const FVector& Point,float Padding) const
{
    if (!bInitialized || Point.ContainsNaN() || !FMath::IsFinite(Padding)) { return false; }
    return LKNavigation::PointDistance(Point,Shape())<=Padding;
}
void ALKBattleTerrain::AppendNavigationObstacles(UWorld* World,TArray<LKNavigation::FObstacle>& Out,float Clearance)
{
    if (!World) { return; }
    for(TActorIterator<ALKBattleTerrain> It(World);It;++It) { if(It->IsBlocking()) { Out.Add(It->Shape(Clearance)); } }
}
bool ALKBattleTerrain::IsLocationBlocked(UWorld* World,const FVector& Location,float Clearance,bool bAllowHighland)
{
    if (!World || Location.ContainsNaN() || !FMath::IsFinite(Clearance) || Clearance<0.f) { return true; }
    for(TActorIterator<ALKBattleTerrain> It(World);It;++It)
    {
        if (!It->IsBlocking()) { continue; }
        if (bAllowHighland && It->Kind==ELKBattleTerrainKind::Highland && It->ContainsPoint(Location,-Clearance)) { continue; }
        if (It->ContainsPoint(Location,Clearance)) { return true; }
    }
    return false;
}
bool ALKBattleTerrain::IsObstacleBlocked(UWorld* World,const LKNavigation::FObstacle& Footprint)
{
    if(!World || Footprint.Center.ContainsNaN() || Footprint.HalfSegment.ContainsNaN()
        || Footprint.BoxHalfExtent.ContainsNaN() || !FMath::IsFinite(Footprint.BoxYaw)
        || !FMath::IsFinite(Footprint.Radius) || Footprint.Radius<0.f) { return true; }
    for(TActorIterator<ALKBattleTerrain> It(World);It;++It)
    {
        if(!It->IsBlocking()) { continue; }
        if(Footprint.IsBox())
        { if(RectanglesTouch(Footprint,It->Shape())) { return true; } }
        else if(LKNavigation::SegmentHitsObstacle(Footprint.Center-Footprint.HalfSegment,
            Footprint.Center+Footprint.HalfSegment,It->Shape(Footprint.Radius))) { return true; }
    }
    return false;
}
ALKBattleTerrain* ALKBattleTerrain::HighlandAt(UWorld* World,const FVector& Location)
{
    if (World) { for(TActorIterator<ALKBattleTerrain> It(World);It;++It)
        { if (It->Kind==ELKBattleTerrainKind::Highland && It->ContainsPoint(Location)) { return *It; } } }
    return nullptr;
}
ALKBattleTerrain* ALKBattleTerrain::HighlandAt(const ALKUnitBase* Unit)
{ return Unit ? HighlandAt(Unit->GetWorld(),Unit->GetActorLocation()) : nullptr; }
bool ALKBattleTerrain::IsInSwamp(const ALKUnitBase* Unit)
{
    if (Unit && Unit->GetWorld()) { for(TActorIterator<ALKBattleTerrain> It(Unit->GetWorld());It;++It)
        { if(It->Kind==ELKBattleTerrainKind::Swamp && It->ContainsPoint(Unit->GetActorLocation())) { return true; } } }
    return false;
}
void ALKBattleTerrain::RebuildBrushConnections(UWorld* World)
{
    if (!World) { return; }
    TArray<ALKBattleTerrain*> Brushes;
    for(TActorIterator<ALKBattleTerrain> It(World);It;++It)
    { if(It->bInitialized && It->Kind==ELKBattleTerrainKind::Brush) { It->BrushGroup.Reset(); Brushes.Add(*It); } }
    for(ALKBattleTerrain* Root:Brushes)
    {
        if(Root->BrushGroup.IsValid()) { continue; }
        TArray<ALKBattleTerrain*> Queue={Root}; Root->BrushGroup=Root;
        for(int32 I=0;I<Queue.Num();++I) { for(ALKBattleTerrain* Other:Brushes)
            { if(!Other->BrushGroup.IsValid() && RectanglesTouch(Queue[I]->Shape(),Other->Shape())) { Other->BrushGroup=Root; Queue.Add(Other); } } }
    }
}
bool ALKBattleTerrain::SharesBrush(const ALKUnitBase* Unit) const
{
    if(!Unit || !BrushGroup.IsValid()) { return false; }
    for(TActorIterator<ALKBattleTerrain> It(GetWorld());It;++It)
    { if(It->Kind==ELKBattleTerrainKind::Brush && It->BrushGroup==BrushGroup && It->ContainsPoint(Unit->GetActorLocation())) { return true; } }
    return false;
}
bool ALKBattleTerrain::CanSeeTarget(const ALKUnitBase* Observer,const ALKUnitBase* Target)
{
    if(!Observer || !Target) { return false; }
    if(!Target->GetWorld()) { return true; }
    if(Observer->GetTeam()==Target->GetTeam() || Target->IsBrushExposed()) { return true; }
    bool bInside=false;
    for(TActorIterator<ALKBattleTerrain> It(Target->GetWorld());It;++It)
    {
        if(It->Kind!=ELKBattleTerrainKind::Brush || !It->ContainsPoint(Target->GetActorLocation())) { continue; }
        bInside=true;
        if(It->SharesBrush(Observer)) { return true; }
    }
    return !bInside;
}
bool ALKBattleTerrain::IsVisibleToTeam(const ALKUnitBase* Target,ELKTeam Team)
{
    if(!Target || !Target->GetWorld()) { return false; }
    if(Target->GetTeam()==Team) { return true; }
    if(Target->GetStatusComponent()->IsConcealed()) { return false; }
    bool bInside=false;
    for(TActorIterator<ALKBattleTerrain> It(Target->GetWorld());It;++It)
    { if(It->Kind==ELKBattleTerrainKind::Brush && It->ContainsPoint(Target->GetActorLocation())) { bInside=true; break; } }
    if(!bInside || Target->IsBrushExposed()) { return true; }
    for(TActorIterator<ALKUnitBase> It(Target->GetWorld());It;++It)
    { if(It->IsAlive() && !It->IsCamp() && It->GetTeam()==Team && CanSeeTarget(*It,Target)) { return true; } }
    return false;
}
void ALKBattleTerrain::NotifyHostileAction(ALKUnitBase* Attacker,const ALKUnitBase* Target)
{
    if(!Attacker || !Target || Attacker->GetTeam()==Target->GetTeam()) { return; }
    bool bLeavingBrush=false;
    for(TActorIterator<ALKBattleTerrain> It(Attacker->GetWorld());It;++It)
    {
        if(It->Kind==ELKBattleTerrainKind::Brush && It->ContainsPoint(Attacker->GetActorLocation()))
        { if(It->SharesBrush(Target)) { return; } bLeavingBrush=true; }
    }
    if(!bLeavingBrush) { return; }
    Attacker->RevealFromBrush(3.f);
    for(TActorIterator<ALKUnitBase> It(Attacker->GetWorld());It;++It)
    {
        if(!It->IsAlive() || It->GetTeam()!=Attacker->GetTeam() || FVector::DistSquared2D(It->GetActorLocation(),Attacker->GetActorLocation())>40000.f) { continue; }
        for(TActorIterator<ALKBattleTerrain> Grass(Attacker->GetWorld());Grass;++Grass)
        { if(Grass->Kind==ELKBattleTerrainKind::Brush && Grass->ContainsPoint(It->GetActorLocation())) { It->RevealFromBrush(3.f); break; } }
    }
}
bool ALKBattleTerrain::AllowsDamage(const ALKUnitBase* Target,const ALKUnitBase* Attacker,const FLKCombatSource& Source)
{
    if(!HighlandAt(Target) || Source.Kind==ELKCombatSourceKind::Spell || Source.Kind==ELKCombatSourceKind::HealthCost
        || Source.Kind==ELKCombatSourceKind::Overtime || Source.Kind==ELKCombatSourceKind::Revival || Source.bRangedSource) { return true; }
    // No climb path exists. Only a same-platform resident may use melee on another resident.
    return Attacker && HighlandAt(Attacker)==HighlandAt(Target);
}
void ALKBattleTerrain::BuildVisuals()
{
    for(auto& Visual:Visuals) { if(Visual) { Visual->DestroyComponent(); } } Visuals.Reset();
    UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/Home/M_HomePlaceholder.M_HomePlaceholder"));
    if(!Cube || !Material) { return; }
    auto Add=[&](FVector P,FVector Scale,FLinearColor Color,float Yaw=0.f)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(this); AddInstanceComponent(Mesh); Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetStaticMesh(Cube); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false);
        Mesh->SetRelativeLocation(P); Mesh->SetRelativeScale3D(Scale/100.f); Mesh->SetRelativeRotation(FRotator(0,Yaw,0));
        auto* MID=UMaterialInstanceDynamic::Create(Material,this); MID->SetVectorParameterValue(TEXT("Color"),Color); Mesh->SetMaterial(0,MID);
        Mesh->RegisterComponent(); Visuals.Add(Mesh);
    };
    const FLinearColor Color=TerrainColor(Kind);
    // Ground art is at Z=2; every terrain layer stays above it and below unit sprites (Z>=8).
    // A decorative grass foundation must never cover authored gameplay terrain.
    const int32 LayerOrder=Kind==ELKBattleTerrainKind::Grass?0:Kind==ELKBattleTerrainKind::Road?1
        :Kind==ELKBattleTerrainKind::Water?2:Kind==ELKBattleTerrainKind::Flowers?3
        :Kind==ELKBattleTerrainKind::Brush?4:Kind==ELKBattleTerrainKind::Swamp?5
        :Kind==ELKBattleTerrainKind::Campfire?6:Kind==ELKBattleTerrainKind::Highland?7
        :Kind==ELKBattleTerrainKind::Rock?8:9;
    const float Layer=2.15f+LayerOrder*.45f+float(GetUniqueID()%71)*.0001f;
    Add(FVector(0,0,Layer),FVector(Size.X,Size.Y,.08f),Color);
    if(Kind==ELKBattleTerrainKind::Campfire)
    {
        Add(FVector(0,0,Layer+.1f),FVector(Size.X*.64f,Size.Y*.12f,.08f),FLinearColor(.16f,.10f,.06f),35.f);
        Add(FVector(0,0,Layer+.18f),FVector(Size.X*.64f,Size.Y*.12f,.08f),FLinearColor(.23f,.15f,.09f),-35.f);
        Add(FVector(0,0,Layer+.26f),FVector(Size.X*.25f,Size.Y*.33f,.08f),FLinearColor(1.f,.34f,.05f),25.f);
        Add(FVector(0,0,Layer+.34f),FVector(Size.X*.12f,Size.Y*.19f,.08f),FLinearColor(1.f,.78f,.26f),-15.f);
    }
    else if(Kind==ELKBattleTerrainKind::Highland || Kind==ELKBattleTerrainKind::Rock)
    {
        for(int32 I=1;I<=3;++I)
        { const float S=1.f-I*.12f; Add(FVector(0,0,Layer+I*.1f),FVector(Size.X*S,Size.Y*S,.08f),Color*(1.f+I*.14f)); }
    }
    else
    {
        const int32 XCount=FMath::Clamp(FMath::FloorToInt(Size.X/120.f),1,4),YCount=FMath::Clamp(FMath::FloorToInt(Size.Y/120.f),1,4);
        for(int32 X=0;X<XCount;++X) { for(int32 Y=0;Y<YCount;++Y)
        {
            const FVector P((X+.5f)*Size.X/XCount-Size.X*.5f,(Y+.5f)*Size.Y/YCount-Size.Y*.5f,Layer+.15f);
            const bool bReeds=Kind==ELKBattleTerrainKind::Brush || Kind==ELKBattleTerrainKind::Tree;
            Add(P,FVector(Size.X/XCount*.12f,Size.Y/YCount*(bReeds?.55f:.15f),.1f),Color*1.4f,bReeds ? float((X+Y)%2 ? 25 : -25) : 0.f);
        } }
    }
}
void ALKBattleTerrain::Draw(AHUD* HUD) const
{
    if(!HUD || !HUD->GetOwningPlayerController() || !bInitialized) { return; }
    const TCHAR* Label=Kind==ELKBattleTerrainKind::Brush?TEXT("草丛") : Kind==ELKBattleTerrainKind::Swamp?TEXT("沼泽 -30%")
        : Kind==ELKBattleTerrainKind::Campfire?TEXT("篝火 · 阻挡") : Kind==ELKBattleTerrainKind::Highland?TEXT("高山 · 驻守"):nullptr;
    if(!Label) { return; }
    FVector2D P;
    // Keep the highland label below its residents; other labels sit just outside the upper edge.
    const bool bBelow=Kind==ELKBattleTerrainKind::Highland;
    const FVector Edge=GetActorLocation()+GetActorRotation().RotateVector(FVector(0,(bBelow?1.f:-1.f)*Size.Y*.5f,0));
    if(HUD->GetOwningPlayerController()->ProjectWorldLocationToScreen(Edge,P,true))
    {
        int32 ViewWidth=1280,ViewHeight=720;
        HUD->GetOwningPlayerController()->GetViewportSize(ViewWidth,ViewHeight);
        const float Scale=FMath::Clamp(ViewWidth/1280.f,.9f,1.4f);
        const float FontScale=1.65f*Scale;
        UFont* Font=LKPolishArt::Font(false);
        float Width=0.f,Height=0.f;
        HUD->GetTextSize(Label,Width,Height,Font,FontScale);
        const float PadX=8.f*Scale,PadY=4.f*Scale;
        const float X=P.X-Width*.5f,Y=bBelow ? P.Y+6.f*Scale : P.Y-Height-10.f*Scale;
        HUD->DrawRect(FLinearColor(.06f,.09f,.07f,.88f),X-PadX,Y-PadY,Width+2.f*PadX,Height+2.f*PadY);
        HUD->DrawText(Label,FLinearColor(.96f,.90f,.72f),X,Y,Font,FontScale);
    }
}
