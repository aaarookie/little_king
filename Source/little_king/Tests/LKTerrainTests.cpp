#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "../ALKBattleTerrain.h"
#include "../ALKUnitBase.h"
#include "../LKGameplayHelpers.h"
#include "../ULKGameData.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKUnitStatusComponent.h"

namespace
{
constexpr EAutomationTestFlags TerrainFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FTerrainWorld
{
    FTestWorldWrapper Env;
    UWorld* World=nullptr;
    ULKGameData* Data=nullptr;
    bool Open()
    {
        if(!Env.CreateTestWorld(EWorldType::Game)) { return false; }
        World=Env.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
        if(!Env.BeginPlayInTestWorld()) { return false; }
        Data=NewObject<ULKGameData>(World); Data->bDrawDebugShapes=false;
        return true;
    }
    ALKBattleTerrain* Terrain(ELKBattleTerrainKind Kind,FVector P,FVector2D Size,float Yaw=0.f)
    {
        auto* Result=World->SpawnActor<ALKBattleTerrain>();
        return Result && Result->Initialize(Kind,P,Size,Yaw) ? Result : nullptr;
    }
    ALKUnitBase* Unit(FVector P,ELKTeam Team=ELKTeam::Player,ELKAttackType Attack=ELKAttackType::Melee)
    {
        auto* Result=World->SpawnActor<ALKUnitBase>();
        FLKUnitRow Row; Row.UnitId="Test_TerrainUnit"; Row.BaseHealth=500.f; Row.MoveSpeed=100.f;
        Row.AttackType=Attack; Row.AttackDamage=20.f; Row.AttackRange=600.f; Row.AcquireRadius=1200.f;
        Result->SetActorLocation(P); Result->InitUnit(Row,Data); Result->SetTeam(Team); Result->SetCombatEnabled(true);
        Result->SetActorTickEnabled(false); Result->GetMovementComponent()->SetComponentTickEnabled(false);
        return Result;
    }
    void Advance(float Seconds)
    { while(Seconds>KINDA_SMALL_NUMBER) { const float Step=FMath::Min(.05f,Seconds); Env.TickTestWorld(Step); Seconds-=Step; } }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBrushVisibilityTest,"LittleKing.Terrain.BrushConnectivityExposureAndCloud",TerrainFlags)
bool FLKBrushVisibilityTest::RunTest(const FString&)
{
    FTerrainWorld E; if(!TestTrue(TEXT("world"),E.Open())) { return false; }
    auto* A=E.Terrain(ELKBattleTerrainKind::Brush,FVector::ZeroVector,FVector2D(200,200));
    auto* B=E.Terrain(ELKBattleTerrainKind::Brush,FVector(200,0,0),FVector2D(200,200));
    auto* C=E.Terrain(ELKBattleTerrainKind::Brush,FVector(400,0,0),FVector2D(200,200));
    auto* Hidden=E.Unit(FVector::ZeroVector,ELKTeam::Enemy,ELKAttackType::Ranged);
    auto* Outside=E.Unit(FVector(700,0,0));
    auto* Inside=E.Unit(FVector(400,0,0));
    auto* Friend=E.Unit(FVector(160,0,0),ELKTeam::Enemy);
    auto* FarFriend=E.Unit(FVector(400,20,0),ELKTeam::Enemy);
    TestFalse(TEXT("external unit cannot acquire brush resident"),Outside->CanDiscoverTarget(Hidden));
    TestTrue(TEXT("three touching patches form one connected brush"),Inside->CanDiscoverTarget(Hidden));
    Hidden->GetStatusComponent()->ApplyConcealment("TestCloud",5.f);
    Hidden->RevealFromBrush(3.f);
    TestFalse(TEXT("cloud wins even inside same brush and exposed"),Inside->CanDiscoverTarget(Hidden));
    Hidden->GetStatusComponent()->RemoveConcealment("TestCloud");
    E.Advance(3.1f);
    ALKBattleTerrain::NotifyHostileAction(Hidden,Inside);
    TestFalse(TEXT("attacking within same connected brush does not expose"),Hidden->IsBrushExposed());
    ALKBattleTerrain::NotifyHostileAction(Hidden,Outside);
    TestTrue(TEXT("outward attack exposes attacker"),Outside->CanDiscoverTarget(Hidden));
    TestTrue(TEXT("friendly brush units within two meters exposed"),Friend->IsBrushExposed());
    TestFalse(TEXT("far allies not exposed"),FarFriend->IsBrushExposed());
    TestFalse(TEXT("opponents not exposed by attacker"),Inside->IsBrushExposed());
    E.Advance(3.1f);
    TestFalse(TEXT("exposure expires after three seconds"),Outside->CanDiscoverTarget(Hidden));
    B->Destroy();
    TestFalse(TEXT("removed bridge splits visibility components"),Inside->CanDiscoverTarget(Hidden));
    Hidden->SetActorLocation(FVector(700,200,0));
    TestTrue(TEXT("leaving brush is immediately visible"),Outside->CanDiscoverTarget(Hidden));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSwampAffinityTest,"LittleKing.Terrain.SwampRuntimeAffinity",TerrainFlags)
bool FLKSwampAffinityTest::RunTest(const FString&)
{
    FTerrainWorld E; if(!E.Open()) { return false; }
    E.Terrain(ELKBattleTerrainKind::Swamp,FVector::ZeroVector,FVector2D(400,200),30.f);
    E.Terrain(ELKBattleTerrainKind::Swamp,FVector::ZeroVector,FVector2D(200,200));
    auto* Unit=E.Unit(FVector::ZeroVector);
    TestEqual(TEXT("overlapping swamps slow once by 30%"),Unit->GetMoveSpeed(),70.f);
    Unit->SetSwampMovementMultiplier(1.f); TestEqual(TEXT("runtime immunity"),Unit->GetMoveSpeed(),100.f);
    Unit->SetSwampMovementMultiplier(1.3f); TestTrue(TEXT("runtime affinity speeds up"),FMath::IsNearlyEqual(Unit->GetMoveSpeed(),130.f));
    Unit->SetActorLocation(FVector(900,0,0)); TestEqual(TEXT("leaving clears multiplier without stale effect"),Unit->GetMoveSpeed(),100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHighlandCombatTest,"LittleKing.Terrain.HighlandCombatAndForcedMovement",TerrainFlags)
bool FLKHighlandCombatTest::RunTest(const FString&)
{
    FTerrainWorld E; if(!E.Open()) { return false; }
    E.Terrain(ELKBattleTerrainKind::Highland,FVector::ZeroVector,FVector2D(300,300));
    auto* Guard=E.Unit(FVector::ZeroVector,ELKTeam::Enemy,ELKAttackType::Ranged);
    auto* Melee=E.Unit(FVector(260,0,0));
    auto* Archer=E.Unit(FVector(260,150,0),ELKTeam::Player,ELKAttackType::Ranged);
    TestFalse(TEXT("ground melee does not chase highland resident"),Melee->CanPursueTarget(Guard));
    TestTrue(TEXT("archer can target resident"),Archer->CanPursueTarget(Guard));
    TestEqual(TEXT("melee skill cannot bypass highland"),LKGameplay::ApplyDamage(Guard,20.f,Melee),0.f);
    TestEqual(TEXT("ranged damage works"),LKGameplay::ApplyDamage(Guard,20.f,Archer),20.f);
    FLKCombatSource Spell=LKGameplay::MakeSource(Melee,ELKCombatSourceKind::Spell,"TestSpell");
    TestEqual(TEXT("spell from a melee source works"),LKGameplay::ApplyDamage(Guard,20.f,Melee,false,&Spell),20.f);
    Guard->GetStatusComponent()->Freeze(1.f);
    TestTrue(TEXT("spell control still works"),Guard->IsControlled());
    auto* Move=Guard->GetMovementComponent();
    Move->MoveToward(FVector(500,0,0),100.f); TestFalse(TEXT("resident cannot start walking"),Move->IsMoving());
    TestEqual(TEXT("wind cannot remove resident"),Guard->ApplyWindDisplacement(FVector(0,500,0)),FVector::ZeroVector);
    TestEqual(TEXT("knockback cannot remove resident"),Move->MoveSkillDelta(FVector(500,0,0)),FVector::ZeroVector);
    TestFalse(TEXT("teleport cannot remove resident"),Move->TeleportToFreePoint(FVector(500,0,0)));
    TestTrue(TEXT("ordinary spawn rejected on highland"),ALKBattleTerrain::IsLocationBlocked(E.World,FVector::ZeroVector,50));
    TestFalse(TEXT("authored resident fits wholly on platform"),ALKBattleTerrain::IsLocationBlocked(E.World,FVector::ZeroVector,50,true));
    TestTrue(TEXT("authored resident may not hang over edge"),ALKBattleTerrain::IsLocationBlocked(E.World,FVector(140,0,0),50,true));
    TestFalse(TEXT("ground teleport cannot climb onto platform"),Melee->GetMovementComponent()->TeleportToFreePoint(FVector(0,80,0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTerrainNavigationTest,"LittleKing.Terrain.OrientedSolidsAndNavigation",TerrainFlags)
bool FLKTerrainNavigationTest::RunTest(const FString&)
{
    FTerrainWorld E; if(!E.Open()) { return false; }
    auto* Fire=E.Terrain(ELKBattleTerrainKind::Campfire,FVector::ZeroVector,FVector2D(300,100),45.f);
    TestFalse(TEXT("fire has no unit health or target API"),Fire->IsA(ALKUnitBase::StaticClass()));
    TestTrue(TEXT("rotated campfire center blocked"),ALKBattleTerrain::IsLocationBlocked(E.World,FVector::ZeroVector));
    TestFalse(TEXT("rotated rectangle outside corner clear"),ALKBattleTerrain::IsLocationBlocked(E.World,FVector(140,-140,0)));
    TestTrue(TEXT("wall center outside but endpoint overlaps campfire"),ALKBattleTerrain::IsObstacleBlocked(E.World,{FVector(260,0,0),20.f,FVector(180,0,0)}));
    TestFalse(TEXT("entire wall beyond solid is valid"),ALKBattleTerrain::IsObstacleBlocked(E.World,{FVector(600,0,0),20.f,FVector(180,0,0)}));
    TArray<LKNavigation::FObstacle> Obstacles; ALKBattleTerrain::AppendNavigationObstacles(E.World,Obstacles,51.f);
    LKNavigation::FBounds Bounds; TArray<FVector> Path;
    const FVector Start(-500,0,0),Goal(500,0,0);
    TestFalse(TEXT("direct path crosses terrain"),LKNavigation::IsSegmentClear(Start,Goal,Obstacles));
    TestTrue(TEXT("finds path around campfire"),LKNavigation::FindPath(Start,Goal,Obstacles,Bounds,Path));
    FVector Last=Start;
    for(const FVector& Point:Path) { TestTrue(TEXT("each detour leg is collision clear"),LKNavigation::IsSegmentClear(Last,Point,Obstacles)); Last=Point; }
    auto* Unit=E.Unit(Start);
    Unit->GetMovementComponent()->MoveSkillDelta(FVector(1000,0,0));
    TestTrue(TEXT("dash stops before solid instead of tunneling"),Unit->GetActorLocation().X<0.f);
    TestFalse(TEXT("dash endpoint stays outside clearance"),ALKBattleTerrain::IsLocationBlocked(E.World,Unit->GetActorLocation(),50.f));
    // Force the grid branch and its cached rotated-box spatial index.
    for(int I=0;I<13;++I) { Obstacles.Add({FVector(-900+I*100,1200,0),15.f}); }
    LKNavigation::FSearchStats Stats;
    TestTrue(TEXT("grid path handles rotated boxes"),LKNavigation::FindPath(Start,Goal,Obstacles,Bounds,Path,&Stats));
    TestTrue(TEXT("large obstacle set uses bounded grid"),Stats.bUsedGrid);
    return true;
}
#endif
