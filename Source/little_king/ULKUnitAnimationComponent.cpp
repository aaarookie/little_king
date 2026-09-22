#include "ULKUnitAnimationComponent.h"
#include "ALKUnitBase.h"
#include "ULKUnitMovementComponent.h"
#include "LKPolishArt.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"

ULKUnitAnimationComponent::ULKUnitAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
void ULKUnitAnimationComponent::Initialize()
{
    Unit=Cast<ALKUnitBase>(GetOwner()); Clips.Reset(); ResetPresentation();
    if(!Unit||Unit->IsCamp()||!LKPolishArt::IsDefaultSprite(Unit->GetUnitId(),Unit->GetSpriteComponent()->GetSprite())){return;}
    for(FName Name:{FName("Idle"),FName("Move"),FName("Attack"),FName("Hit"),FName("Death")})
    {
        UPaperFlipbook* Clip=LKPolishArt::Animation(Unit->GetUnitId(),Name);
        if(!Clip||Clip->GetNumKeyFrames()==0){Clips.Reset();return;}
        Clips.Add(Clip);
    }
    Unit->GetSpriteComponent()->SetSprite(Clips[0]->GetSpriteAtTime(0.f));
}
void ULKUnitAnimationComponent::ResetPresentation()
{
    State="Idle"; Clock=AttackRemaining=HitRemaining=0.f; bWasDead=false;
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
void ULKUnitAnimationComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime,TickType,Function);
    if(!Unit||!HasAnimations()){return;}
    const FVector Position=Unit->GetActorLocation();
    const bool Moving=(Unit->IsManualMoving()||Unit->IsSkillMoving()||Unit->GetMovementComponent()->IsMoving())
        && FVector::DistSquared2D(Position,LastPosition)>.01f;
    LastPosition=Position;
    if(bWasDead&&!Unit->IsDead()){ResetPresentation();}
    bWasDead=Unit->IsDead();
    const bool Frozen=Unit->IsControlled();
    if(Frozen||(!Unit->IsCombatEnabled()&&!Unit->IsManualMoving())){AttackRemaining=0.f;}
    FName Next="Idle";int32 Index=0;
    if(Unit->IsDead()){Next="Death";Index=4;}
    else if(HitRemaining>0.f){Next="Hit";Index=3;}
    else if(!Frozen&&AttackRemaining>0.f){Next="Attack";Index=2;}
    else if(!Frozen&&Moving){Next="Move";Index=1;}
    if(State!=Next){State=Next;Clock=0.f;}
    UPaperFlipbook* Clip=Clips[Index];
    const float Length=Clip->GetTotalDuration();
    float Time=Clock;
    if(Index==2){Time=(1.f-AttackRemaining/AttackDuration)*Length;}
    else if(Index==3){Time=(1.f-HitRemaining/.16f)*Length;}
    else if(Index==4){Time=FMath::Min(Clock/ALKUnitBase::DeathAnimDuration,1.f)*Length;}
    else if(Length>0.f){Time=FMath::Fmod(Clock,Length);}
    Unit->GetSpriteComponent()->SetSprite(Clip->GetSpriteAtTime(Time,true));
    if(!Frozen||Unit->IsDead()){Clock+=DeltaTime;}
    AttackRemaining=FMath::Max(0.f,AttackRemaining-DeltaTime);
    HitRemaining=FMath::Max(0.f,HitRemaining-DeltaTime);
}
