#include "ULKUnitAnimationComponent.h"
#include "ALKUnitBase.h"
#include "ULKUnitMovementComponent.h"
#include "ULKPresentationSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "LKPolishArt.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "EngineUtils.h"

ULKUnitAnimationComponent::ULKUnitAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    // Spell fields apply forced movement in PostPhysics. Sample all resulting
    // travel afterwards, so facing/gait match the position rendered this frame.
    PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
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
        EstimatedStrideDistance=FMath::Clamp(float(Bounds.BoxExtent.X*1.4),100.f,280.f);
    }
    ResolveMetadata();
    bLastEnemy=Unit->GetTeam()==ELKTeam::Enemy;
    bFacingRight=!bLastEnemy;
    if(!Unit->IsBuilding()||Unit->IsMobileBuilding()){Unit->SetVisualFacingRight(bFacingRight);}
    UpdateDepth();
}
void ULKUnitAnimationComponent::ResetPresentation()
{
    TArray<FLKAnimationPresentationEvent> Events;
    Actions.Reset(Events);
    State="Idle"; Clock=AttackRemaining=HitRemaining=0.f; WalkPhase=0.; bWasDead=false;
    bRestartAttack=bRestartHit=bAwaitingImpact=bAttackImpacted=false;
    if(Unit){LastPosition=Unit->GetActorLocation();}
    Dispatch(Events);
}
void ULKUnitAnimationComponent::Attack(float Windup)
{
    if(!Unit||!HasAnimations()||Unit->IsDead()||Unit->IsControlled()||Unit->IsManualMoving()||Unit->IsSkillMoving()||!Unit->IsCombatEnabled()){return;}
    const float SafeWindup=FMath::IsFinite(Windup)?FMath::Max(0.f,Windup):0.f;
    AttackDuration=FMath::Max(.32f,SafeWindup+.16f); AttackRemaining=AttackDuration;
    AttackWindupDuration=SafeWindup;
    bRestartAttack=true; bAwaitingImpact=SafeWindup>0.f; bAttackImpacted=false;
}
void ULKUnitAnimationComponent::Impact()
{
    if(AttackRemaining<=0.f){Attack();}
    if(AttackRemaining>0.f){AttackRemaining=FMath::Min(AttackRemaining,.16f);bAttackImpacted=true;}
}
void ULKUnitAnimationComponent::CancelAttack()
{
    AttackRemaining=0.f; bRestartAttack=bAwaitingImpact=bAttackImpacted=false;
    if(Actions.GetAction()=="Attack")
    {
        TArray<FLKAnimationPresentationEvent> Events;
        Actions.Reset(Events); Dispatch(Events);
    }
}
void ULKUnitAnimationComponent::Hit()
{
    if(!Unit||!HasAnimations()||Unit->IsDead()||Unit->IsControlled()){return;}
    CancelAttack();
    HitRemaining=.16f; bRestartHit=true;
}
void ULKUnitAnimationComponent::SetAnimationMetadata(const FLKAnimationMetadata& Metadata)
{
    AnimationMetadata=Metadata; bOverrideAnimationMetadata=true;
    ResetPresentation(); ResolveMetadata();
}
void ULKUnitAnimationComponent::UseDefaultAnimationMetadata()
{
    bOverrideAnimationMetadata=false;
    ResetPresentation(); ResolveMetadata();
}
void ULKUnitAnimationComponent::ResolveMetadata()
{
    const auto Defaults=LKAnimation::DefaultsForUnit(Unit?Unit->GetUnitId():NAME_None,EstimatedStrideDistance);
    ResolvedMetadata=LKAnimation::Resolve(bOverrideAnimationMetadata?AnimationMetadata:Defaults,Defaults,bMetadataUsedFallback);
    static const FName States[]={"Idle","Move","Attack","Hit","Death"};
    if(HasAnimations())
    {
        for(int32 I=0;I<5;++I)
        {
            auto* Timing=ResolvedMetadata.Timing.Find(States[I]);
            if(Timing&&!Timing->FrameWeights.IsEmpty()&&Timing->FrameWeights.Num()!=Clips[I]->GetNumKeyFrames())
            {
                // A different atlas is valid; incompatible timing falls back to its native keyframe runs.
                Timing->FrameWeights.Reset(); bMetadataUsedFallback=true;
            }
        }
    }
    StrideDistance=ResolvedMetadata.StrideDistance;
}
bool ULKUnitAnimationComponent::RegisterActionComponent(int64 InstanceId,UActorComponent* Component)
{
    if(InstanceId==0||InstanceId!=Actions.GetActiveId()||!IsValid(Component)||Component==this||
        Component->GetOwner()!=GetOwner()||Component->HasAnyFlags(RF_DefaultSubObject)||Component->IsBeingDestroyed())
    {return false;}
    ActionComponents.FindOrAdd(InstanceId).AddUnique(Component);
    return true;
}
void ULKUnitAnimationComponent::ClearActionComponents(int64 InstanceId)
{
    TArray<TWeakObjectPtr<UActorComponent>> Components;
    if(!ActionComponents.RemoveAndCopyValue(InstanceId,Components)){return;}
    for(const auto& Weak:Components)
    {
        if(auto* Component=Weak.Get())
        {
            if(auto* Audio=Cast<UAudioComponent>(Component)){Audio->Stop();}
            Component->DestroyComponent();
        }
    }
}
void ULKUnitAnimationComponent::Dispatch(const TArray<FLKAnimationPresentationEvent>& Events)
{
    for(const auto& Event:Events)
    {
        if(Event.Type==ELKAnimationPresentationEvent::Completed||Event.Type==ELKAnimationPresentationEvent::Cancelled)
        {ClearActionComponents(Event.InstanceId);}
        else if(Event.InstanceId!=Actions.GetActiveId()){continue;}
        if(Event.Type==ELKAnimationPresentationEvent::FootContact&&!ResolvedMetadata.FootstepSound.IsNone()&&Unit)
        {ULKPresentationSubsystem::Sound(GetWorld(),ResolvedMetadata.FootstepSound,Unit->GetActorLocation());}
        PresentationEventNative.Broadcast(Event);
        if((Event.Type==ELKAnimationPresentationEvent::Began||Event.Type==ELKAnimationPresentationEvent::FootContact)&&
            Event.InstanceId!=Actions.GetActiveId()){continue;}
        OnPresentationEvent.Broadcast(Event);
    }
}
void ULKUnitAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetPresentation();
    // Also handle components registered by an event listener during teardown.
    TArray<int64> Remaining; ActionComponents.GetKeys(Remaining);
    for(int64 Id:Remaining){ClearActionComponents(Id);}
    Super::EndPlay(EndPlayReason);
}
double ULKUnitAnimationComponent::AttackContactPhase(const UPaperFlipbook* Clip) const
{
    if(const auto* Timing=ResolvedMetadata.Timing.Find("Attack");Timing&&!Timing->FrameWeights.IsEmpty())
    {return LKAnimation::PhaseAtKeyFrame(ResolvedMetadata.AttackContactFrame,Timing->FrameWeights);}
    TArray<float> NativeWeights;
    for(int32 I=0;I<Clip->GetNumKeyFrames();++I){NativeWeights.Add(float(Clip->GetKeyFrameChecked(I).FrameRun));}
    return LKAnimation::PhaseAtKeyFrame(ResolvedMetadata.AttackContactFrame,NativeWeights);
}
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

void ULKUnitAnimationComponent::UpdateFacing(const FVector& Travel, bool bWindDriven)
{
    if((Unit->IsBuilding()&&!Unit->IsMobileBuilding())||Unit->IsDead()||(Unit->IsControlled()&&!bWindDriven)){return;}
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
    if(!Unit||!FMath::IsFinite(DeltaTime)||DeltaTime<=0.f||(GetWorld()&&GetWorld()->IsPaused())){return;}
    UpdateDepth();
    const FVector Position=Unit->GetActorLocation();
    bool bWindDriven=false;
    FVector Travel=Unit->GetMovementComponent()->ConsumeVisualTravel(&bWindDriven);
    if(Unit->IsSkillMoving()){Travel=Position-LastPosition;}
    LastPosition=Position;
    UpdateFacing(Travel,bWindDriven);
    if(!HasAnimations()){return;}
    const bool Moving=Travel.SizeSquared2D()>.0001f&&(!Unit->IsBuilding()||Unit->IsMobileBuilding());
    if(bWasDead&&!Unit->IsDead()){ResetPresentation();}
    bWasDead=Unit->IsDead();
    const bool Frozen=Unit->IsControlled();
    if(Frozen||Unit->IsManualMoving()||Unit->IsSkillMoving()||!Unit->IsCombatEnabled()){CancelAttack();}
    if(Frozen){HitRemaining=0.f;bRestartHit=false;}
    LKAnimation::FWalkAdvance Walk; Walk.Phase=WalkPhase;
    if(!Frozen&&!Unit->IsDead()&&Moving)
    {
        Walk=LKAnimation::AdvanceWalk(WalkPhase,Travel.Size2D(),StrideDistance,ResolvedMetadata.ContactPhases,
            !bWindDriven&&!Unit->IsSkillMoving());
        WalkPhase=Walk.Phase;
    }
    FName Next="Idle";int32 Index=0;
    if(Unit->IsDead()){Next="Death";Index=4;}
    else if(HitRemaining>0.f){Next="Hit";Index=3;}
    else if(!Frozen&&AttackRemaining>0.f){Next="Attack";Index=2;}
    else if(!Frozen&&Moving){Next="Move";Index=1;}
    const bool Restart=(Index==2&&bRestartAttack)||(Index==3&&bRestartHit);
    if(State!=Next||Restart){State=Next;Clock=0.f;}
    TArray<FLKAnimationPresentationEvent> Events;
    Actions.Select(Next,Restart,Events);
    const int64 SelectedInstance=Actions.GetActiveId();
    Dispatch(Events); Events.Reset();
    // Public event listeners may cancel, replace metadata, or reinitialize with a
    // custom sprite. Never sample the old action/atlas after those callbacks.
    if(IsBeingDestroyed()||!IsValid(Unit)||Actions.GetActiveId()!=SelectedInstance||State!=Next||
        !HasAnimations()||!Clips.IsValidIndex(Index)||!IsValid(Clips[Index].Get())){return;}
    if(Index==2){bRestartAttack=false;}
    if(Index==3){bRestartHit=false;}
    if(!Frozen||Unit->IsDead()){Clock+=DeltaTime;}
    AttackRemaining=FMath::Max(0.f,AttackRemaining-DeltaTime);
    HitRemaining=FMath::Max(0.f,HitRemaining-DeltaTime);
    UPaperFlipbook* Clip=Clips[Index];
    const float Length=Clip->GetTotalDuration();
    double Phase=Length>0.f?FMath::Fmod(Clock,Length)/Length:0.;
    if(Index==1){Phase=WalkPhase;}
    else if(Index==2)
    {
        const double Contact=AttackContactPhase(Clip);
        if(bAttackImpacted){Phase=Contact+(1.-Contact)*(1.-AttackRemaining/.16f);}
        else if(bAwaitingImpact)
        {
            const double Windup=FMath::Max(.001f,AttackWindupDuration);
            Phase=FMath::Min(Contact*(AttackDuration-AttackRemaining)/Windup,FMath::Max(0.,Contact-0.000001));
        }
        else{Phase=1.-AttackRemaining/AttackDuration;}
    }
    else if(Index==3){Phase=1.-HitRemaining/.16f;}
    else if(Index==4){Phase=FMath::Min(Clock/ALKUnitBase::DeathAnimDuration,1.f);}
    const auto* Timing=ResolvedMetadata.Timing.Find(Next);
    const int32 KeyFrame=Timing?LKAnimation::SampleKeyFrame(Phase,Clip->GetNumKeyFrames(),Timing->FrameWeights):INDEX_NONE;
    Unit->GetSpriteComponent()->SetSprite(KeyFrame==INDEX_NONE?Clip->GetSpriteAtTime(float(Phase)*Length,true):Clip->GetKeyFrameChecked(KeyFrame).Sprite.Get());
    if(Index==1)
    {
        for(int32 Contact:Walk.Contacts){Actions.Contact(Contact,Events);}
        Dispatch(Events); Events.Reset();
    }
    if((Index==2&&AttackRemaining<=0.f)||(Index==3&&HitRemaining<=0.f)||
        (Index==4&&Clock>=ALKUnitBase::DeathAnimDuration))
    {Actions.Finish(false,Events);Dispatch(Events);}
}
