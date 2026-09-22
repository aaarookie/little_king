#include "ULKUnitAnimationComponent.h"
#include "ALKUnitBase.h"
#include "ULKUnitMovementComponent.h"
#include "LKPolishArt.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "EngineUtils.h"

ULKUnitAnimationComponent::ULKUnitAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
void ULKUnitAnimationComponent::Initialize()
{
    Unit=Cast<ALKUnitBase>(GetOwner()); Clips.Reset(); ResetPresentation();
    if(!Unit){return;}
    AddTickPrerequisiteActor(Unit);
    AddTickPrerequisiteComponent(Unit->GetMovementComponent());
    if(!Unit->IsCamp()&&LKPolishArt::IsDefaultSprite(Unit->GetUnitId(),Unit->GetSpriteComponent()->GetSprite()))
    {
        for(FName Name:{FName("Idle"),FName("Move"),FName("Attack"),FName("Hit"),FName("Death")})
        {
            UPaperFlipbook* Clip=LKPolishArt::Animation(Unit->GetUnitId(),Name);
            if(!Clip||Clip->GetNumKeyFrames()==0){Clips.Reset();break;}
            Clips.Add(Clip);
        }
    }
    if(HasAnimations()){Unit->GetSpriteComponent()->SetSprite(Clips[0]->GetSpriteAtTime(0.f));}
    if(const UPaperSprite* Sprite=Unit->GetSpriteComponent()->GetSprite())
    {
        const auto Bounds=Sprite->GetRenderBounds().TransformBy(Unit->GetSpriteComponent()->GetComponentTransform());
        FootOffsetX=Bounds.Origin.X-Bounds.BoxExtent.X-Unit->GetActorLocation().X;
        StrideDistance=FMath::Clamp(float(Bounds.BoxExtent.X*1.4),100.f,280.f);
    }
    bLastEnemy=Unit->GetTeam()==ELKTeam::Enemy;
    bFacingRight=!bLastEnemy;
    if(!Unit->IsBuilding()){Unit->SetVisualFacingRight(bFacingRight);}
    UpdateDepth();
}
void ULKUnitAnimationComponent::ResetPresentation()
{
    State="Idle"; Clock=AttackRemaining=HitRemaining=WalkPhase=0.f; bWasDead=false;
    if(Unit){LastPosition=Unit->GetActorLocation();}
}
void ULKUnitAnimationComponent::Attack(float Windup)
{ AttackDuration=FMath::Max(.32f,Windup+.16f);AttackRemaining=AttackDuration; }
void ULKUnitAnimationComponent::Impact()
{ if(AttackRemaining<=0.f){Attack();} AttackRemaining=FMath::Min(AttackRemaining,.16f); }
void ULKUnitAnimationComponent::Hit() { HitRemaining=.16f; }
FBoxSphereBounds ULKUnitAnimationComponent::GetIdleBounds() const
{
    if(HasAnimations()&&Unit)
    { return Clips[0]->GetSpriteAtFrame(0)->GetRenderBounds().TransformBy(Unit->GetSpriteComponent()->GetComponentTransform()); }
    return Unit?Unit->GetSpriteComponent()->Bounds:FBoxSphereBounds(ForceInit);
}
float ULKUnitAnimationComponent::GetVisualFootX() const
{ return Unit?Unit->GetActorLocation().X+FootOffsetX:0.f; }

void ULKUnitAnimationComponent::UpdateDepth()
{
    if(!Unit||!Unit->GetSpriteComponent()->GetSprite()){return;}
    // Screen-down is world -X. Masked sprites write depth, so translucency sort
    // priority alone cannot resolve their coplanar intersections. Give every
    // ground-contact row a distinct visual plane; never lift the logical body.
    const int32 Row=FMath::FloorToInt(GetVisualFootX()/2.f);
    int32 Rank=0;
    for(TActorIterator<ALKUnitBase> It(GetWorld());It;++It)
    {
        ALKUnitBase* Other=*It;
        if(Other==Unit||!Other->GetSpriteComponent()->GetSprite()){continue;}
        const int32 OtherRow=FMath::FloorToInt(Other->GetAnimationComponent()->GetVisualFootX()/2.f);
        if(OtherRow>Row||(OtherRow==Row&&Other->GetUniqueID()<Unit->GetUniqueID())){++Rank;}
    }
    FVector Location=Unit->GetSpriteComponent()->GetRelativeLocation();
    Location.Z=8.f+Rank*.5f;
    Unit->GetSpriteComponent()->SetRelativeLocation(Location);
}

void ULKUnitAnimationComponent::UpdateFacing(const FVector& Travel)
{
    if(Unit->IsBuilding()||Unit->IsDead()||Unit->IsControlled()){return;}
    const bool Enemy=Unit->GetTeam()==ELKTeam::Enemy;
    if(Enemy!=bLastEnemy){bFacingRight=!Enemy;bLastEnemy=Enemy;}
    FVector Direction=Travel.GetSafeNormal2D();
    if(Direction.IsNearlyZero()&&!Unit->IsManualMoving())
    {
        if(const AActor* Target=Unit->GetTarget();IsValid(Target))
        {Direction=(Target->GetActorLocation()-Unit->GetActorLocation()).GetSafeNormal2D();}
    }
    // Artwork has two horizontal facings. Retain the previous facing on vertical
    // motion rather than jittering with tiny sideways path/separation changes.
    if(FMath::Abs(Direction.Y)>.15f){bFacingRight=Direction.Y>0.f;}
    Unit->SetVisualFacingRight(bFacingRight);
}

void ULKUnitAnimationComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime,TickType,Function);
    if(!Unit){return;}
    UpdateDepth();
    const FVector Position=Unit->GetActorLocation();
    FVector Travel=Unit->GetMovementComponent()->ConsumeVisualTravel();
    if(Unit->IsSkillMoving()){Travel=Position-LastPosition;}
    LastPosition=Position;
    UpdateFacing(Travel);
    if(!HasAnimations()){return;}
    const bool Moving=Travel.SizeSquared2D()>.0001f&&!Unit->IsBuilding();
    if(bWasDead&&!Unit->IsDead()){ResetPresentation();}
    bWasDead=Unit->IsDead();
    const bool Frozen=Unit->IsControlled();
    if(Frozen||Unit->IsManualMoving()||Unit->IsSkillMoving()||!Unit->IsCombatEnabled()){AttackRemaining=0.f;}
    if(!Frozen&&!Unit->IsDead()&&Moving)
    {WalkPhase=FMath::Fmod(WalkPhase+float(Travel.Size2D())/StrideDistance,1.f);}
    FName Next="Idle";int32 Index=0;
    if(Unit->IsDead()){Next="Death";Index=4;}
    else if(HitRemaining>0.f){Next="Hit";Index=3;}
    else if(!Frozen&&AttackRemaining>0.f){Next="Attack";Index=2;}
    else if(!Frozen&&Moving){Next="Move";Index=1;}
    if(State!=Next){State=Next;Clock=0.f;}
    UPaperFlipbook* Clip=Clips[Index];
    const float Length=Clip->GetTotalDuration();
    float Time=Clock;
    if(Index==1){Time=WalkPhase*Length;}
    else if(Index==2){Time=(1.f-AttackRemaining/AttackDuration)*Length;}
    else if(Index==3){Time=(1.f-HitRemaining/.16f)*Length;}
    else if(Index==4){Time=FMath::Min(Clock/ALKUnitBase::DeathAnimDuration,1.f)*Length;}
    else if(Length>0.f){Time=FMath::Fmod(Clock,Length);}
    Unit->GetSpriteComponent()->SetSprite(Clip->GetSpriteAtTime(Time,true));
    if(!Frozen||Unit->IsDead()){Clock+=DeltaTime;}
    AttackRemaining=FMath::Max(0.f,AttackRemaining-DeltaTime);
    HitRemaining=FMath::Max(0.f,HitRemaining-DeltaTime);
}
