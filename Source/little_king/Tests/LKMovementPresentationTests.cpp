#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PaperFlipbook.h"
#include "../ALKUnitBase.h"
#include "../ALKHeroCamp.h"
#include "../LKUnitContent.h"
#include "../LKWorldArt.h"
#include "../LKPolishArt.h"
#include "../ULKGameData.h"
#include "../ULKRunSubsystem.h"
#include "../ULKUnitAnimationComponent.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKUnitStatusComponent.h"
#include "../LKGameplayHelpers.h"

namespace
{
struct FMovementVisualWorld
{
    FTestWorldWrapper Env;
    UWorld* World=nullptr;
    ULKGameData* Data=nullptr;
    bool Open()
    {
        if(!Env.CreateTestWorld(EWorldType::Game)){return false;}
        World=Env.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
        World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->SetAutoSaveEnabled(false);
        if(!Env.BeginPlayInTestWorld()){return false;}
        Data=NewObject<ULKGameData>(World);
        return true;
    }
    ALKUnitBase* Spawn(FVector Position,ELKTeam Team=ELKTeam::Player)
    {
        auto* Unit=World->SpawnActor<ALKUnitBase>();
        Unit->SetActorLocation(Position);
        Unit->InitUnit(*LKUnitContent::Find("Unit_TrollWarrior"),Data);
        Unit->SetTeam(Team);Unit->SetCombatEnabled(true);
        return Unit;
    }
};
void VisualTick(ALKUnitBase* Unit,float Delta=.016f)
{Unit->GetAnimationComponent()->TickComponent(Delta,LEVELTICK_All,nullptr);}
void Walk(ALKUnitBase* Unit,FVector Destination,float Speed,float Delta)
{
    Unit->GetMovementComponent()->MoveToward(Destination,Speed);
    Unit->GetMovementComponent()->TickComponent(Delta,LEVELTICK_All,nullptr);
    VisualTick(Unit,Delta);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSpriteDepthRegression,"LittleKing.Presentation.OverlapDepthAndCamps",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKSpriteDepthRegression::RunTest(const FString&)
{
    FMovementVisualWorld F;if(!F.Open()){return false;}
    auto* A=F.Spawn(FVector::ZeroVector);
    auto* B=F.Spawn(FVector::ZeroVector,ELKTeam::Enemy);
    auto* Camp=F.World->SpawnActor<ALKHeroCamp>();
    FLKUnitRow CampRow;CampRow.UnitId="Building_HeroCamp";CampRow.UnitClass=ELKUnitClass::Building;
    CampRow.Sprite=LKWorldArt::CampSprite("Hero_Knight");
    Camp->InitUnit(CampRow,F.Data);
    TSet<double> Depths;
    for(ALKUnitBase* Unit:{A,B,static_cast<ALKUnitBase*>(Camp)})
    {
        const auto Position=Unit->GetActorLocation();
        const float Radius=Unit->GetBodyRadius();
        VisualTick(Unit);
        Depths.Add(Unit->GetSpriteComponent()->GetRelativeLocation().Z);
        TestEqual(TEXT("Sorting never moves logical actor"),Unit->GetActorLocation(),Position);
        TestEqual(TEXT("Sorting never changes collision"),Unit->GetBodyRadius(),Radius);
        TestFalse(TEXT("Flat artwork does not cast overlap shadows"),bool(Unit->GetSpriteComponent()->CastShadow));
    }
    TestEqual(TEXT("Equal-position units and camp have distinct depth planes"),Depths.Num(),3);
    const double Stable=A->GetSpriteComponent()->GetRelativeLocation().Z;
    for(int I=0;I<32;++I){A->GetAnimationComponent()->Attack();VisualTick(A);VisualTick(B);VisualTick(Camp);}
    TestEqual(TEXT("Animated bounds do not change the contact order"),A->GetSpriteComponent()->GetRelativeLocation().Z,Stable);
    B->SetActorLocation(FVector(-400,0,0));VisualTick(A);VisualTick(B);
    TestTrue(TEXT("Screen-lower unit covers the upper unit"),B->GetSpriteComponent()->GetRelativeLocation().Z>A->GetSpriteComponent()->GetRelativeLocation().Z);
    B->SetActorLocation(FVector(400,0,0));VisualTick(B);VisualTick(A);
    TestTrue(TEXT("Crossing reverses depth consistently"),B->GetSpriteComponent()->GetRelativeLocation().Z<A->GetSpriteComponent()->GetRelativeLocation().Z);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSpriteFacingRegression,"LittleKing.Presentation.FacingFollowsVoluntaryTravel",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKSpriteFacingRegression::RunTest(const FString&)
{
    FMovementVisualWorld F;if(!F.Open()){return false;}
    auto* Unit=F.Spawn(FVector(0,0,0));
    auto* Enemy=F.Spawn(FVector(0,1000,0),ELKTeam::Enemy);
    VisualTick(Enemy);
    TestFalse(TEXT("Enemy initially faces into the field"),Enemy->GetAnimationComponent()->IsFacingRight());
    Unit->SetTarget(Enemy);
    const auto Rotation=Unit->GetActorRotation();
    Walk(Unit,FVector(0,-600,0),100,.1f);
    TestFalse(TEXT("Travel overrides target on opposite side"),Unit->GetAnimationComponent()->IsFacingRight());
    TestTrue(TEXT("Only sprite local width is mirrored"),Unit->GetSpriteComponent()->GetRelativeScale3D().X<0);
    TestEqual(TEXT("Turning leaves actor rotation intact"),Unit->GetActorRotation(),Rotation);
    Walk(Unit,Unit->GetActorLocation()+FVector(400,0,0),100,.1f);
    TestFalse(TEXT("Vertical path retains last facing"),Unit->GetAnimationComponent()->IsFacingRight());
    Unit->GetMovementComponent()->Stop();VisualTick(Unit);
    TestTrue(TEXT("Stationary attacker faces target"),Unit->GetAnimationComponent()->IsFacingRight());
    Unit->SetVisualFacingRight(false);
    LKGameplay::ApplyDamage(Unit,Unit->GetMaxHealth()*10,nullptr,true);
    Unit->Tick(.1f);
    TestTrue(TEXT("Death feedback preserves mirror sign"),Unit->GetSpriteComponent()->GetRelativeScale3D().X<0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWalkDistanceRegression,"LittleKing.Presentation.WalkDistanceAndInterruption",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKWalkDistanceRegression::RunTest(const FString&)
{
    FMovementVisualWorld F;if(!F.Open()){return false;}
    auto* Slow=F.Spawn(FVector(-600,0,0));
    auto* Fast=F.Spawn(FVector(600,0,0));
    Walk(Slow,FVector(-600,800,0),100,.1f);
    Walk(Fast,FVector(600,800,0),200,.05f);
    TestEqual(TEXT("Voluntary travel selects walk"),Slow->GetAnimationComponent()->GetVisualState(),FName("Move"));
    const float Phase=Slow->GetAnimationComponent()->GetWalkPhase();
    TestTrue(TEXT("Equal distances give equal gait phase regardless of speed/time"),FMath::IsNearlyEqual(Phase,Fast->GetAnimationComponent()->GetWalkPhase(),.0001f));
    Slow->GetMovementComponent()->Stop();VisualTick(Slow,.5);
    TestEqual(TEXT("Stopping keeps the next step rather than restarting it"),Slow->GetAnimationComponent()->GetWalkPhase(),Phase);
    TestEqual(TEXT("Stationary unit is idle"),Slow->GetAnimationComponent()->GetVisualState(),FName("Idle"));
    Slow->SetActorLocation(Slow->GetActorLocation()+FVector(20,30,0));VisualTick(Slow);
    TestEqual(TEXT("External displacement is not voluntary walking"),Slow->GetAnimationComponent()->GetWalkPhase(),Phase);
    Walk(Slow,Slow->GetActorLocation()+FVector(0,400,0),100,.1f);
    TestTrue(TEXT("Resuming continues accumulated step"),Slow->GetAnimationComponent()->GetWalkPhase()>Phase);
    const float BeforeFreeze=Slow->GetAnimationComponent()->GetWalkPhase();
    Slow->GetStatusComponent()->Freeze(1.f);
    Walk(Slow,Slow->GetActorLocation()+FVector(0,400,0),100,.1f);
    TestEqual(TEXT("Control does not advance walking"),Slow->GetAnimationComponent()->GetWalkPhase(),BeforeFreeze);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWalkAtlasRegression,"LittleKing.Presentation.CorrectedWalkAtlases",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKWalkAtlasRegression::RunTest(const FString&)
{
    TestEqual(TEXT("All biped walkers have corrected clips"),LKPolishArt::WalkingUnitIds().Num(),24);
    for(FName Id:LKPolishArt::WalkingUnitIds())
    {
        auto* Clip=LKPolishArt::Animation(Id,"Move");
        if(!TestNotNull(*Id.ToString(),Clip)){continue;}
        TestEqual(TEXT("Four contact/passing poses"),Clip->GetNumKeyFrames(),4);
        for(int I=0;I<4;++I)
        {TestTrue(*(Id.ToString()+TEXT(" uses corrected walk art")),Clip->GetSpriteAtFrame(I)->GetPathName().StartsWith(TEXT("/Game/Art/StorybookV1/MovementFix/")));}
    }
    return true;
}
#endif
