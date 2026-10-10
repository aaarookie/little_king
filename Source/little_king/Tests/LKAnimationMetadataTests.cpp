#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Components/SceneComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PaperFlipbook.h"
#include "../LKAnimationMetadata.h"
#include "../ULKUnitAnimationComponent.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKUnitStatusComponent.h"
#include "../ULKRunSubsystem.h"
#include "../ULKGameData.h"
#include "../ALKUnitBase.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../LKUnitContent.h"
#include "../LKPolishArt.h"
#include "../LKGameplayHelpers.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAnimationMetadataValidation,"LittleKing.Animation.MetadataValidationAndTiming",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKAnimationMetadataValidation::RunTest(const FString&)
{
    const auto Defaults=LKAnimation::DefaultsForUnit("Unit_Swordsman",175.f);
    FLKAnimationMetadata Authored=Defaults;
    Authored.StrideDistance=std::numeric_limits<float>::quiet_NaN();
    Authored.ContactPhases={-1.f,.5f};
    Authored.AttackContactFrame=-3;
    Authored.Timing.FindOrAdd("Attack").FrameWeights={1.f,0.f,1.f,1.f};
    bool Fallback=false;
    const auto Resolved=LKAnimation::Resolve(Authored,Defaults,Fallback);
    TestTrue(TEXT("Invalid fields report fallback"),Fallback);
    TestEqual(TEXT("Invalid stride inherits authored unit default"),Resolved.StrideDistance,Defaults.StrideDistance);
    TestEqual(TEXT("Invalid contact data falls back as a unit"),Resolved.ContactPhases,Defaults.ContactPhases);
    TestEqual(TEXT("Invalid frame durations inherit valid timing"),Resolved.Timing.FindChecked("Attack").FrameWeights,Defaults.Timing.FindChecked("Attack").FrameWeights);
    TestEqual(TEXT("Invalid contact frame falls back"),Resolved.AttackContactFrame,Defaults.AttackContactFrame);
    TestTrue(TEXT("Resolution never mutates author data"),FMath::IsNaN(Authored.StrideDistance));
    Authored=Defaults; Authored.StrideDistance=200.f; Authored.ContactPhases={.5f,0.f,.5f};
    const auto Clean=LKAnimation::Resolve(Authored,Defaults,Fallback);
    TestFalse(TEXT("Valid overrides remain valid"),Fallback);
    TestEqual(TEXT("Duplicate contacts cannot double fire"),Clean.ContactPhases.Num(),2);
    TestEqual(TEXT("Contacts sorted for stable indices"),Clean.ContactPhases[0],0.f);
    const TArray<float> Weights={2.f,1.f,1.f};
    TestEqual(TEXT("Long first hold is preserved"),LKAnimation::SampleKeyFrame(.49,3,Weights),0);
    TestEqual(TEXT("Exact boundary enters next pose"),LKAnimation::SampleKeyFrame(.5,3,Weights),1);
    TestEqual(TEXT("End clamps to final pose"),LKAnimation::SampleKeyFrame(1.,3,Weights),2);
    TestEqual(TEXT("Incompatible atlas count falls back to native timing"),LKAnimation::SampleKeyFrame(.5,4,Weights),INDEX_NONE);
    const auto& AttackWeights=Defaults.Timing.FindChecked("Attack").FrameWeights;
    TestEqual(TEXT("Impact anchor samples its exact authored keyframe"),LKAnimation::SampleKeyFrame(LKAnimation::PhaseAtKeyFrame(2,AttackWeights),4,AttackWeights),2);
    TestEqual(TEXT("Unconfigured roles retain previous estimate"),LKAnimation::DefaultsForUnit("Unit_TrollWarrior",175.f).StrideDistance,175.f);
    TestEqual(TEXT("Only explicit audible role uses the existing footstep cue"),LKAnimation::DefaultsForUnit("Unit_Colossus",175.f).FootstepSound,FName("Footstep"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAnimationCrossedContacts,"LittleKing.Animation.ContactIntervals",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKAnimationCrossedContacts::RunTest(const FString&)
{
    const TArray<float> Contacts={0.f,.25f,.5f,.75f};
    const auto Endpoint=LKAnimation::AdvanceWalk(0.,50.,100.,Contacts,true);
    TestEqual(TEXT("Low frame rate crosses every intervening contact"),Endpoint.Contacts,TArray<int32>({1,2}));
    const auto After=LKAnimation::AdvanceWalk(Endpoint.Phase,25.,100.,Contacts,true);
    TestEqual(TEXT("Shared endpoint is not emitted twice"),After.Contacts,TArray<int32>({3}));
    const auto Wrap=LKAnimation::AdvanceWalk(.75,50.,100.,Contacts,true);
    TestEqual(TEXT("Wrap preserves chronological marker order"),Wrap.Contacts,TArray<int32>({0,1}));
    const auto Stopped=LKAnimation::AdvanceWalk(Wrap.Phase,0.,100.,Contacts,true);
    TestEqual(TEXT("Stop retains exact phase"),Stopped.Phase,Wrap.Phase);
    TestTrue(TEXT("Stopped tick has no contact"),Stopped.Contacts.IsEmpty());
    const auto Jump=LKAnimation::AdvanceWalk(.25,1050.,100.,Contacts,true);
    TestEqual(TEXT("Large displacement keeps gait phase"),Jump.Phase,.75);
    TestTrue(TEXT("Large displacement cannot burst old footsteps"),Jump.Contacts.IsEmpty());
    const auto Wind=LKAnimation::AdvanceWalk(.25,50.,100.,Contacts,false);
    TestEqual(TEXT("Forced displacement keeps existing phase semantics"),Wind.Phase,.75);
    TestTrue(TEXT("Forced displacement has no ordinary footstep"),Wind.Contacts.IsEmpty());
    const auto Invalid=LKAnimation::AdvanceWalk(.25,std::numeric_limits<double>::quiet_NaN(),100.,Contacts,true);
    TestEqual(TEXT("Invalid travel cannot poison phase"),Invalid.Phase,.25);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAnimationActionLifetimes,"LittleKing.Animation.ActionLifetime",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKAnimationActionLifetimes::RunTest(const FString&)
{
    LKAnimation::FActionState Action;
    TArray<FLKAnimationPresentationEvent> Events;
    Action.Select("Move",false,Events);
    const int64 First=Action.GetActiveId();
    Action.Select("Move",false,Events);
    TestEqual(TEXT("Steady state begins once"),Events.Num(),1);
    Action.Finish(true,Events);
    Action.Finish(true,Events);
    Action.Contact(0,Events);
    TestEqual(TEXT("Cancel repeated twice and late contact emit only one end"),Events.Num(),2);
    TestEqual(TEXT("Interrupted action gets cancellation"),Events.Last().Type,ELKAnimationPresentationEvent::Cancelled);
    Action.Reset(Events); Action.Select("Attack",false,Events);
    TestTrue(TEXT("Reset does not recycle instance identities"),Action.GetActiveId()>First);
    Action.Select("Attack",true,Events);
    TestEqual(TEXT("Restart cancels previous instance"),Events[Events.Num()-2].Type,ELKAnimationPresentationEvent::Cancelled);
    Action.Finish(false,Events);
    const int32 Count=Events.Num();
    Action.Finish(false,Events); Action.Select("Attack",false,Events);
    TestEqual(TEXT("Completed pose neither completes nor begins again"),Events.Num(),Count);
    TestEqual(TEXT("Completed actions cannot own new effects"),Action.GetActiveId(),int64(0));
    return true;
}

namespace
{
struct FAnimationWorld
{
    // Env tears down before Events, so EndPlay callbacks never capture a destroyed array.
    TArray<FLKAnimationPresentationEvent> Events;
    FTestWorldWrapper Env;
    UWorld* World=nullptr;
    ALKUnitBase* Unit=nullptr;
    ULKUnitAnimationComponent* Animation=nullptr;
    bool Open(FAutomationTestBase& Test)
    {
        if(!Test.TestTrue(TEXT("Create animation fixture world"),Env.CreateTestWorld(EWorldType::Game))){return false;}
        World=Env.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
        World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->SetAutoSaveEnabled(false);
        if(!Test.TestTrue(TEXT("Begin animation fixture world"),Env.BeginPlayInTestWorld()))
        {Env.ForwardErrorMessages(&Test);return false;}
        auto* GM=World->GetAuthGameMode<ALKBattleGameMode>();
        if(!Test.TestNotNull(TEXT("Animation fixture has real battle authority"),GM)){return false;}
        World->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        for(int32 I=0;I<GM->AvailableHeroes.Num();++I)
        {
            if(!Test.TestEqual(TEXT("Deploy fixture hero"),
                GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700,-1300,0)),ELKPlayResult::Success))
            {return false;}
        }
        GM->ForceStartBattle();
        if(!Test.TestEqual(TEXT("Animation fixture starts a real battle"),GM->GetPhase(),ELKGamePhase::Battle)){return false;}
        // The original eight units keep their sprite references in DT_Units. Canonical
        // gameplay rows alone intentionally have no legacy sprite; use the same merge as runtime.
        auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/DT_Units.DT_Units"));
        if(!Test.TestNotNull(TEXT("Authored presentation table loads"),Table)){return false;}
        const auto* Authored=Table->FindRow<FLKUnitRow>("Hero_Mage",TEXT("AnimationFixture"),false);
        if(!Test.TestNotNull(TEXT("Authored mage presentation exists"),Authored)){return false;}
        const FLKUnitRow Row=LKUnitContent::MergeAuthoredTuning(*LKUnitContent::Find("Hero_Mage"),Authored);
        Unit=World->SpawnActor<ALKUnitBase>();
        if(!Test.TestNotNull(TEXT("Animation fixture hero spawns"),Unit)){return false;}
        Unit->InitUnit(Row,GM->GetGameData());
        Unit->SetTeam(ELKTeam::Player); Unit->SetCombatEnabled(true);
        Animation=Unit->GetAnimationComponent();
        Animation->OnPresentationEventNative().AddLambda([this](const auto& Event){Events.Add(Event);});
        return Test.TestTrue(TEXT("Authored mage sprite enables all five animation clips"),Animation->HasAnimations());
    }
    void Tick(float Delta=.01f){Animation->TickComponent(Delta,LEVELTICK_All,nullptr);}
    void Walk(float Distance)
    {
        Unit->GetMovementComponent()->MoveToward(Unit->GetActorLocation()+FVector(0,Distance,0),100.f);
        Unit->GetMovementComponent()->TickComponent(Distance/100.f,LEVELTICK_All,nullptr);
        Tick(Distance/100.f);
    }
    int32 Count(ELKAnimationPresentationEvent Type,int64 Id=0) const
    {
        int32 Result=0;
        for(const auto& Event:Events){if(Event.Type==Type&&(!Id||Event.InstanceId==Id)){++Result;}}
        return Result;
    }
    USceneComponent* Effect()
    {
        auto* Component=NewObject<USceneComponent>(Unit,NAME_None,RF_Transient);
        Component->RegisterComponent();
        return Component;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAnimationComponentContacts,"LittleKing.Animation.ComponentContactsAndOverrides",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKAnimationComponentContacts::RunTest(const FString&)
{
    FAnimationWorld F; if(!F.Open(*this)){return false;}
    FLKAnimationMetadata Metadata; Metadata.StrideDistance=100.f; Metadata.ContactPhases={0.f,.25f,.5f,.75f};
    Metadata.Timing.FindOrAdd("Move").FrameWeights={1.f,2.f};
    F.Animation->SetAnimationMetadata(Metadata);
    TestTrue(TEXT("Mismatched keyframe count reports native fallback"),F.Animation->DidMetadataUseFallback());
    TestTrue(TEXT("Author override is copied, never rewritten"),Metadata.Timing.FindChecked("Move").FrameWeights.Num()==2);
    const float Health=F.Unit->GetHealth();
    F.Walk(60.f);
    TestTrue(TEXT("Configured stride drives actual distance"),FMath::IsNearlyEqual(F.Animation->GetWalkPhase(),.6f));
    TestEqual(TEXT("Two markers crossed in a single movement tick"),F.Count(ELKAnimationPresentationEvent::FootContact),2);
    F.Tick(.2f);
    TestEqual(TEXT("Consumed movement cannot replay markers"),F.Count(ELKAnimationPresentationEvent::FootContact),2);
    F.Walk(40.f);
    TestEqual(TEXT("Resume crosses wrap markers once"),F.Count(ELKAnimationPresentationEvent::FootContact),4);
    F.Unit->GetMovementComponent()->MoveWindDelta(FVector(0,50,0)); F.Tick();
    TestTrue(TEXT("Wind still advances visible gait"),FMath::IsNearlyEqual(F.Animation->GetWalkPhase(),.5f));
    TestEqual(TEXT("Wind never creates ordinary footstep cues"),F.Count(ELKAnimationPresentationEvent::FootContact),4);
    F.Unit->GetStatusComponent()->Freeze(1.f);
    F.Unit->GetMovementComponent()->MoveWindDelta(FVector(0,-20,0)); F.Tick(.2f);
    TestTrue(TEXT("Control freezes gait even when wind moves the body"),FMath::IsNearlyEqual(F.Animation->GetWalkPhase(),.5f));
    TestEqual(TEXT("Controlled travel has no contacts"),F.Count(ELKAnimationPresentationEvent::FootContact),4);
    F.Unit->GetStatusComponent()->Clear();
    TestEqual(TEXT("Presentation never changes health"),F.Unit->GetHealth(),Health);
    F.Animation->SetAnimationMetadata(Metadata);
    const int32 ContactsBeforeCancel=F.Count(ELKAnimationPresentationEvent::FootContact);
    const auto CancelHandle=F.Animation->OnPresentationEventNative().AddLambda([&F](const FLKAnimationPresentationEvent& Event)
    {if(Event.Type==ELKAnimationPresentationEvent::FootContact){F.Animation->ResetPresentation();}});
    F.Walk(60.f);
    F.Animation->OnPresentationEventNative().Remove(CancelHandle);
    TestEqual(TEXT("A marker callback cancellation suppresses remaining queued markers"),F.Count(ELKAnimationPresentationEvent::FootContact),ContactsBeforeCancel+1);
    auto* Custom=NewObject<UPaperSprite>(F.Unit);
    F.Unit->GetSpriteComponent()->SetSprite(Custom); F.Animation->Initialize();
    const int32 Before=F.Events.Num();
    F.Animation->Attack(); F.Tick(.5f);
    TestFalse(TEXT("Custom sprite retains static fallback"),F.Animation->HasAnimations());
    TestEqual(TEXT("Custom illustration remains intact"),F.Unit->GetSpriteComponent()->GetSprite(),Custom);
    TestEqual(TEXT("Custom fallback creates no action events"),F.Events.Num(),Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAnimationComponentCleanup,"LittleKing.Animation.ComponentPauseImpactAndCleanup",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKAnimationComponentCleanup::RunTest(const FString&)
{
    FAnimationWorld F; if(!F.Open(*this)){return false;}
    F.Animation->Attack(.5f); F.Tick(.49f); F.Tick(.005f);
    auto* AttackClip=LKPolishArt::Animation("Hero_Mage","Attack");
    TestTrue(TEXT("Windup stays before the authoritative contact pose"),F.Unit->GetSpriteComponent()->GetSprite()!=AttackClip->GetKeyFrameChecked(2).Sprite.Get());
    const int64 First=F.Animation->GetPresentationInstanceId();
    auto* Effect=F.Effect();
    TestTrue(TEXT("Current action can own a temporary effect"),F.Animation->RegisterActionComponent(First,Effect));
    TestFalse(TEXT("Default gameplay sprite cannot be transferred"),F.Animation->RegisterActionComponent(First,F.Unit->GetSpriteComponent()));
    auto* Pose=F.Unit->GetSpriteComponent()->GetSprite();
    const int32 Before=F.Events.Num();
    auto* Pauser=F.World->SpawnActor<APlayerState>();
    F.World->GetWorldSettings()->SetPauserPlayerState(Pauser);
    TestTrue(TEXT("Fixture world is paused"),F.World->IsPaused());
    F.Tick(1.f);
    TestEqual(TEXT("Pause produces no lifecycle or marker events"),F.Events.Num(),Before);
    TestEqual(TEXT("Pause holds authored pose"),F.Unit->GetSpriteComponent()->GetSprite(),Pose);
    TestFalse(TEXT("Pause does not destroy active effects"),Effect->IsBeingDestroyed());
    F.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    F.Animation->Impact(); F.Tick(.001f);
    TestEqual(TEXT("Authority immediately selects contact pose"),F.Unit->GetSpriteComponent()->GetSprite(),AttackClip->GetKeyFrameChecked(2).Sprite.Get());
    F.Animation->CancelAttack(); F.Animation->CancelAttack();
    TestTrue(TEXT("Cancellation really destroys owned components"),Effect->IsBeingDestroyed());
    TestEqual(TEXT("Repeated cancel emits only one end"),F.Count(ELKAnimationPresentationEvent::Cancelled,First),1);
    auto* LateEffect=F.Effect();
    TestFalse(TEXT("Stale instance cannot register effects"),F.Animation->RegisterActionComponent(First,LateEffect));
    LateEffect->DestroyComponent();
    F.Animation->Attack(); F.Tick();
    const int64 Second=F.Animation->GetPresentationInstanceId();
    TestTrue(TEXT("Next attack has a new identity"),Second>First);
    auto* ControlledEffect=F.Effect(); F.Animation->RegisterActionComponent(Second,ControlledEffect);
    F.Unit->GetStatusComponent()->Freeze(1.f); F.Tick();
    TestTrue(TEXT("Control cleans cancelled action components"),ControlledEffect->IsBeingDestroyed());
    TestEqual(TEXT("Control cancels once"),F.Count(ELKAnimationPresentationEvent::Cancelled,Second),1);
    F.Unit->GetStatusComponent()->Clear();
    F.Animation->Attack(); F.Tick();
    const int64 Completed=F.Animation->GetPresentationInstanceId();
    auto* CompletedEffect=F.Effect(); F.Animation->RegisterActionComponent(Completed,CompletedEffect);
    F.Tick(.4f); F.Tick();
    TestTrue(TEXT("Natural completion destroys owned effects"),CompletedEffect->IsBeingDestroyed());
    TestEqual(TEXT("Natural completion is emitted once"),F.Count(ELKAnimationPresentationEvent::Completed,Completed),1);
    LKGameplay::ApplyDamage(F.Unit,F.Unit->GetMaxHealth()*10,nullptr,true);
    F.Tick(.2f);
    const int64 Death=F.Animation->GetPresentationInstanceId();
    F.Tick(.2f); F.Tick(.2f);
    TestEqual(TEXT("Death completes once while final pose holds"),F.Count(ELKAnimationPresentationEvent::Completed,Death),1);
    TestTrue(TEXT("Hero can still revive"),F.Unit->ReviveDuringBattle()); F.Tick();
    TestEqual(TEXT("Revive resets gait"),F.Animation->GetWalkPhase(),0.f);
    TestEqual(TEXT("Revive restores idle"),F.Animation->GetVisualState(),FName("Idle"));
    TestTrue(TEXT("Revive cannot reuse a stale death instance"),F.Animation->GetPresentationInstanceId()>Death);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAnimationAuthorityCancellation,"LittleKing.Animation.AuthoritativeWindupCancellationAndCommit",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKAnimationAuthorityCancellation::RunTest(const FString&)
{
    FAnimationWorld F; if(!F.Open(*this)){return false;}
    auto* GM=F.World->GetAuthGameMode<ALKBattleGameMode>();
    auto* Target=F.World->SpawnActor<ALKUnitBase>();
    if(!TestNotNull(TEXT("Attack target spawns"),Target)){return false;}
    Target->InitUnit(*LKUnitContent::Find("Unit_TrollWarrior"),GM->GetGameData());
    Target->SetTeam(ELKTeam::Enemy); Target->SetCombatEnabled(true);
    Target->SetActorLocation(FVector(0,100,0));
    F.Unit->SetTarget(Target); F.Unit->Tick(.01f); F.Tick(.01f);
    if(!TestEqual(TEXT("Real attack pipeline starts the windup pose"),F.Animation->GetVisualState(),FName("Attack"))){return false;}
    const int64 Cancelled=F.Animation->GetPresentationInstanceId();
    auto* CancelledEffect=F.Effect();
    TestTrue(TEXT("Pending windup owns its effect"),F.Animation->RegisterActionComponent(Cancelled,CancelledEffect));
    const float TargetHealth=Target->GetHealth();

    Target->SetActorLocation(FVector(0,F.Unit->GetAttackRange()+300.f,0));
    F.Unit->Tick(.01f); // Run the real out-of-range/target-loss path, before any visual tick.
    TestTrue(TEXT("Logical interruption immediately destroys windup effects"),CancelledEffect->IsBeingDestroyed());
    TestEqual(TEXT("Logical interruption emits one cancellation"),F.Count(ELKAnimationPresentationEvent::Cancelled,Cancelled),1);
    F.Unit->CancelAttackWindup(); F.Tick(.5f);
    TestEqual(TEXT("Repeated cleanup cannot cancel the instance twice"),F.Count(ELKAnimationPresentationEvent::Cancelled,Cancelled),1);
    TestEqual(TEXT("Interrupted windup never reports natural completion"),F.Count(ELKAnimationPresentationEvent::Completed,Cancelled),0);
    TestEqual(TEXT("Cancelled attack cannot damage its former target"),Target->GetHealth(),TargetHealth);

    Target->SetActorLocation(FVector(0,100,0));
    F.Unit->SetTarget(Target); F.Unit->Tick(F.Unit->GetAttackInterval()+.1f); F.Tick(.01f);
    if(!TestEqual(TEXT("Next real windup starts"),F.Animation->GetVisualState(),FName("Attack"))){return false;}
    const int64 Committed=F.Animation->GetPresentationInstanceId();
    TestTrue(TEXT("New windup uses a fresh instance"),Committed>Cancelled);
    auto* CommittedEffect=F.Effect();
    TestTrue(TEXT("Successful windup can own its effect"),F.Animation->RegisterActionComponent(Committed,CommittedEffect));
    int32 TotalBefore=0, ActiveBefore=0; GM->GetProjectilePoolStats(TotalBefore,ActiveBefore);
    F.Unit->Tick(1.f); F.Tick(.001f);
    int32 TotalAfter=0, ActiveAfter=0; GM->GetProjectilePoolStats(TotalAfter,ActiveAfter);
    TestEqual(TEXT("Authoritative ranged strike launches exactly one projectile"),ActiveAfter,ActiveBefore+1);
    TestEqual(TEXT("Successful commitment keeps its original instance"),F.Animation->GetPresentationInstanceId(),Committed);
    TestEqual(TEXT("Successful strike emits no cancellation"),F.Count(ELKAnimationPresentationEvent::Cancelled,Committed),0);
    TestFalse(TEXT("Successful contact retains the effect through recovery"),CommittedEffect->IsBeingDestroyed());
    auto* AttackClip=LKPolishArt::Animation("Hero_Mage","Attack");
    TestEqual(TEXT("Successful authority still selects contact pose"),F.Unit->GetSpriteComponent()->GetSprite(),AttackClip->GetKeyFrameChecked(2).Sprite.Get());
    F.Tick(.2f);
    TestEqual(TEXT("Successful recovery completes exactly once"),F.Count(ELKAnimationPresentationEvent::Completed,Committed),1);
    TestTrue(TEXT("Successful completion cleans the effect"),CommittedEffect->IsBeingDestroyed());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAnimationReentrantBegin,"LittleKing.Animation.ReentrantBeginCancellationAndCustomFallback",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKAnimationReentrantBegin::RunTest(const FString&)
{
    FAnimationWorld F; if(!F.Open(*this)){return false;}
    auto* IdlePose=F.Unit->GetSpriteComponent()->GetSprite();
    int64 MetadataInstance=0; USceneComponent* MetadataEffect=nullptr;
    const auto MetadataHandle=F.Animation->OnPresentationEventNative().AddLambda([&](const FLKAnimationPresentationEvent& Event)
    {
        if(Event.Type!=ELKAnimationPresentationEvent::Began||Event.Action!="Attack"){return;}
        MetadataInstance=Event.InstanceId; MetadataEffect=F.Effect();
        TestTrue(TEXT("Begin callback can register an effect"),F.Animation->RegisterActionComponent(Event.InstanceId,MetadataEffect));
        FLKAnimationMetadata Override; Override.StrideDistance=125.f;
        F.Animation->SetAnimationMetadata(Override);
    });
    F.Animation->Attack(); F.Tick();
    F.Animation->OnPresentationEventNative().Remove(MetadataHandle);
    TestTrue(TEXT("Metadata callback ran for a live instance"),MetadataInstance>0);
    TestEqual(TEXT("Metadata replacement cancels that instance once"),F.Count(ELKAnimationPresentationEvent::Cancelled,MetadataInstance),1);
    TestTrue(TEXT("Metadata replacement cleans its registered effect"),MetadataEffect&&MetadataEffect->IsBeingDestroyed());
    TestEqual(TEXT("Cancelled begin never writes an old attack pose"),F.Unit->GetSpriteComponent()->GetSprite(),IdlePose);
    TestEqual(TEXT("Metadata cancellation does not complete an obsolete action"),F.Count(ELKAnimationPresentationEvent::Completed,MetadataInstance),0);
    TestEqual(TEXT("Callback metadata remains in effect"),F.Animation->GetResolvedAnimationMetadata().StrideDistance,125.f);
    F.Tick();

    auto* Custom=NewObject<UPaperSprite>(F.Unit);
    int64 CustomInstance=0; USceneComponent* CustomEffect=nullptr;
    const auto CustomHandle=F.Animation->OnPresentationEventNative().AddLambda([&](const FLKAnimationPresentationEvent& Event)
    {
        if(Event.Type!=ELKAnimationPresentationEvent::Began||Event.Action!="Attack"){return;}
        CustomInstance=Event.InstanceId; CustomEffect=F.Effect();
        TestTrue(TEXT("Replacement callback owns a temporary component"),F.Animation->RegisterActionComponent(Event.InstanceId,CustomEffect));
        F.Unit->GetSpriteComponent()->SetSprite(Custom);
        F.Animation->Initialize();
    });
    F.Animation->Attack(); F.Tick();
    F.Animation->OnPresentationEventNative().Remove(CustomHandle);
    TestTrue(TEXT("Custom fallback callback ran"),CustomInstance>MetadataInstance);
    TestFalse(TEXT("Callback removes the previous atlas"),F.Animation->HasAnimations());
    TestEqual(TEXT("Reentrant initialization preserves the custom sprite"),F.Unit->GetSpriteComponent()->GetSprite(),Custom);
    TestTrue(TEXT("Reentrant initialization destroys the old effect"),CustomEffect&&CustomEffect->IsBeingDestroyed());
    TestEqual(TEXT("Reentrant initialization cancels once"),F.Count(ELKAnimationPresentationEvent::Cancelled,CustomInstance),1);
    const int32 EventCount=F.Events.Num(); F.Tick(.5f);
    TestEqual(TEXT("Static fallback cannot replay cancelled callbacks"),F.Events.Num(),EventCount);
    return true;
}
#endif
