#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "AbilitySystemComponent.h"
#include "HAL/PlatformTime.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../ALKUnitBuilding.h"
#include "../ALKProjectile.h"
#include "../LKBuildingGeometry.h"
#include "../LKNavigation.h"
#include "../LKUnitContent.h"
#include "../LKHomeContent.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKUnitAttributeSet.h"
#include "../ULKDeckState.h"
#include "../ULKSilverComponent.h"
#include "../ULKRunSubsystem.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FWallWorld : FTestWorldWrapper
{
    ALKBattleGameMode* GM = nullptr;
    bool Open(FAutomationTestBase& Test)
    {
        if (!CreateTestWorld(EWorldType::Game)) { ForwardErrorMessages(&Test); return false; }
        auto* Run = GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
        Run->ConfigureStorage(TEXT("LittleKing_V084_Walls_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Run->SetAutoSaveEnabled(false);
        GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
        if (!BeginPlayInTestWorld()) { ForwardErrorMessages(&Test); return false; }
        GM = GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (!Test.TestNotNull(TEXT("Native battle authority"), GM)) { return false; }
        GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes = false; GM->GetGameData()->bDrawFieldBounds = false;
        GM->GetGameData()->MaxUnitsPerTeam = 80;
        for (int32 I = 0; I < GM->AvailableHeroes.Num(); ++I)
        { if (GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[I], FVector((I - 1) * 700, -1300, 0)) != ELKPlayResult::Success) { return false; } }
        GM->ForceStartBattle();
        GM->GetTeamSilver(ELKTeam::Player)->SetCap(100.f);
        GM->GetTeamSilver(ELKTeam::Player)->AddSilver(100.f);
        return Test.TestEqual(TEXT("Fixture enters battle phase"), GM->GetPhase(), ELKGamePhase::Battle);
    }
    ALKUnitBuilding* Wall(FName Id, const FVector& P, float Yaw = 0.f, ELKTeam Team = ELKTeam::Player)
    { return Cast<ALKUnitBuilding>(GM->SpawnUnitForTeam(Id, Team, P, ELKUnitClass::Building, NAME_None, Yaw)); }
    void Finish(ALKUnitBuilding* Wall) const { Wall->Tick(Wall->GetConstructionRemaining() + .01f); }
    void Health(ALKUnitBase* Unit, float HP) const
    { Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), HP); }
    int32 PutInHand(FName Id) const
    {
        auto* Deck = GM->GetTeamDeck(ELKTeam::Player);
        TArray<FName> Cards = LKHomeContent::DefaultUnlockedCards(); Cards[0] = Id;
        for (int32 Seed = 0; Seed < 128; ++Seed)
        {
            if (!Deck->InitDeck(Cards, 4, Seed)) { return INDEX_NONE; }
            const int32 Slot = Deck->GetHand().IndexOfByKey(Id); if (Slot != INDEX_NONE) { return Slot; }
        }
        return INDEX_NONE;
    }
    TArray<ALKUnitBuilding*> Walls() const
    {
        TArray<ALKUnitBuilding*> Result;
        for (TActorIterator<ALKUnitBuilding> It(GetTestWorld()); It; ++It)
        { if (It->IsAlive() && It->IsWall()) { Result.Add(*It); } }
        return Result;
    }
};
bool FullUniqueHand(FAutomationTestBase& Test, const ULKDeckState* Deck)
{
    const auto Hand = Deck->GetHand(); TSet<FName> Ids;
    for (FName Id : Hand) { if (Id.IsNone()) { return Test.TestTrue(TEXT("Hand has no empty slots"), false); } Ids.Add(Id); }
    return Test.TestTrue(TEXT("A successful wall operation preserves four distinct ready cards"), Hand.Num() == 4 && Ids.Num() == 4 && Deck->IsReady());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallCapsuleGeometry, "LittleKing.V084.Walls.RotatedFootprintAndFullSpanRayHit", Flags)
bool FLKV084WallCapsuleGeometry::RunTest(const FString&)
{
    const auto Horizontal = LKBuildingGeometry::WallShape(FVector::ZeroVector, 0.f, 300.f, 36.f);
    const auto Vertical = LKBuildingGeometry::WallShape(FVector::ZeroVector, 90.f, 300.f, 36.f);
    TestTrue(TEXT("Wall total footprint is exactly 300 cm long and 36 cm thick"), FMath::IsNearlyEqual(Horizontal.HalfSegment.X + Horizontal.Radius, 150.f) && FMath::IsNearlyEqual(Horizontal.Radius * 2.f, 36.f));
    TestTrue(TEXT("Rotation changes the obstacle geometry, not just the sprite"), FMath::IsNearlyZero(Vertical.HalfSegment.X, .01f) && FMath::IsNearlyEqual(Vertical.HalfSegment.Y, 132.f, .01f));
    TestFalse(TEXT("Ray through wall body far from its center is blocked"), LKNavigation::IsSegmentClear(FVector(120, -500, 0), FVector(120, 500, 0), {Horizontal}));
    const float Hit = LKBuildingGeometry::RayHit(FVector(120, -500, 0), FVector(0, 1, 0), 1000.f, Horizontal);
    TestTrue(TEXT("Earliest ray hit is the near wall face, not the wall center"), FMath::Abs(Hit - 482.f) < .1f);
    const float RotatedHit = LKBuildingGeometry::RayHit(FVector(-500, 120, 0), FVector(1, 0, 0), 1000.f, Vertical);
    TestTrue(TEXT("Rotated full wall span uses the same collision rule"), FMath::Abs(RotatedHit - 482.f) < .1f);
    TestEqual(TEXT("Ray outside the complete end cap remains clear"), LKBuildingGeometry::RayHit(FVector(151, -500, 0), FVector(0, 1, 0), 1000.f, Horizontal), -1.f);
    TestTrue(TEXT("Rounded end cap still catches an edge projectile"), LKBuildingGeometry::RayHit(FVector(149, -500, 0), FVector(0, 1, 0), 1000.f, Horizontal) >= 0.f);
    TestEqual(TEXT("Ray beginning inside a wall immediately hits it"), LKBuildingGeometry::RayHit(FVector(120, 0, 0), FVector(0, 1, 0), 1000.f, Horizontal), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallNavigation, "LittleKing.V084.Walls.DetourAndClosedEnclosure", Flags)
bool FLKV084WallNavigation::RunTest(const FString&)
{
    LKNavigation::FBounds Bounds; Bounds.HalfExtent = FVector2D(1000.f, 1000.f);
    const FVector Start(0, -600, 0), Goal(0, 0, 0);
    TArray<LKNavigation::FObstacle> Walls = { LKBuildingGeometry::WallShape(FVector(0, -150, 0), 0, 300, 36, 35) };
    TArray<FVector> Path;
    TestTrue(TEXT("An open wall has a navigable detour"), LKNavigation::FindPath(Start, Goal, Walls, Bounds, Path));
    TestTrue(TEXT("Detour has intermediate waypoints"), Path.Num() > 1);
    FVector Previous = Start;
    for (const FVector& P : Path)
    { TestTrue(TEXT("Every detour segment clears the full padded wall"), LKNavigation::IsSegmentClear(Previous, P, Walls)); Previous = P; }
    TestTrue(TEXT("Detour arrives at the intended destination"), !Path.IsEmpty() && Path.Last().Equals(Goal, .01f));
    Walls.Add(LKBuildingGeometry::WallShape(FVector(0, 150, 0), 0, 300, 36, 35));
    Walls.Add(LKBuildingGeometry::WallShape(FVector(-150, 0, 0), 90, 300, 36, 35));
    Walls.Add(LKBuildingGeometry::WallShape(FVector(150, 0, 0), 90, 300, 36, 35));
    TestFalse(TEXT("A fully enclosed target has no invisible route through wall corners"), LKNavigation::FindPath(Start, Goal, Walls, Bounds, Path));
    TestTrue(TEXT("Failed navigation leaves no stale path"), Path.IsEmpty());
    Walls.RemoveAt(0);
    TestTrue(TEXT("Breaking the front wall makes the enclosed target reachable again"), LKNavigation::FindPath(Start, Goal, Walls, Bounds, Path));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084LargeWallNavigation, "LittleKing.V084.Walls.BoundedLargeNavigationAndGeometryCache", Flags)
bool FLKV084LargeWallNavigation::RunTest(const FString&)
{
    LKNavigation::FBounds Bounds; Bounds.HalfExtent = FVector2D(1600.f, 2200.f);
    const FVector Start(0, -1700, 0), Goal(0, 1700, 0);
    TArray<LKNavigation::FObstacle> Walls;
    // Four alternating barriers of eight overlapping wall segments. Each touches one
    // side boundary, so reaching the goal requires routes around opposite barrier ends.
    for (int32 Row = 0; Row < 4; ++Row)
    {
        const float Offset = Row % 2 ? -400.f : 400.f;
        for (int32 Col = 0; Col < 8; ++Col)
        { Walls.Add(LKBuildingGeometry::WallShape(FVector(-1050.f + Col * 300.f + Offset, -900.f + Row * 600.f, 0), 0.f, 300.f, 36.f, 35.f)); }
    }
    TArray<FVector> Path; LKNavigation::FSearchStats Stats;
    const double ColdStart = FPlatformTime::Seconds();
    if (!TestTrue(TEXT("Thirty-two wall maze has a route"), LKNavigation::FindPath(Start, Goal, Walls, Bounds, Path, &Stats))) { return false; }
    const double ColdMs = (FPlatformTime::Seconds() - ColdStart) * 1000.0;
    TestTrue(TEXT("Large scenes use bounded grid search"), Stats.bUsedGrid && Stats.GridNodes <= LKNavigation::GridNodeLimit);
    TestTrue(TEXT("A node is expanded at most once"), Stats.VisitedNodes > 0 && Stats.VisitedNodes <= Stats.GridNodes);
    FVector Previous = Start; bool bUsesLeft = false, bUsesRight = false;
    for (const FVector& P : Path)
    {
        TestTrue(TEXT("Smoothed segment never intersects any exact wall capsule"), LKNavigation::IsSegmentClear(Previous, P, Walls));
        TestTrue(TEXT("Every returned waypoint remains in bounds and outside walls"), LKNavigation::IsPointValid(P, Walls, Bounds));
        bUsesLeft |= P.X < -800.f; bUsesRight |= P.X > 800.f; Previous = P;
    }
    TestTrue(TEXT("Route follows both alternating openings"), bUsesLeft && bUsesRight);
    TestTrue(TEXT("Route reaches exact unquantized destination"), !Path.IsEmpty() && Path.Last().Equals(Goal, .001f));
    const auto Original = Path;
    const double WarmStart = FPlatformTime::Seconds();
    for (int32 Repeat = 0; Repeat < 20; ++Repeat)
    {
        TestTrue(TEXT("Repeated same geometry route remains possible"), LKNavigation::FindPath(Start, Goal, Walls, Bounds, Path, &Stats));
        TestTrue(TEXT("Same geometry reuses node and exact-edge cache"), Stats.bReusedGrid);
        TestTrue(TEXT("Equal inputs give a deterministic path"), Path == Original);
    }
    const double WarmMs = (FPlatformTime::Seconds() - WarmStart) * 1000.0 / 20.0;
    const int32 MazeNodes = Stats.GridNodes, MazeVisited = Stats.VisitedNodes;

    const int32 FrontWallIndex = Walls.Num();
    Walls.Add(LKBuildingGeometry::WallShape(Goal + FVector(0, -150, 0), 0, 300, 36, 35));
    Walls.Add(LKBuildingGeometry::WallShape(Goal + FVector(0, 150, 0), 0, 300, 36, 35));
    Walls.Add(LKBuildingGeometry::WallShape(Goal + FVector(-150, 0, 0), 90, 300, 36, 35));
    Walls.Add(LKBuildingGeometry::WallShape(Goal + FVector(150, 0, 0), 90, 300, 36, 35));
    const double ClosedStart = FPlatformTime::Seconds();
    TestFalse(TEXT("Large-scene closed enclosure cannot be crossed by a grid diagonal"), LKNavigation::FindPath(Start, Goal, Walls, Bounds, Path, &Stats));
    const double ClosedMs = (FPlatformTime::Seconds() - ClosedStart) * 1000.0;
    TestTrue(TEXT("Unreachable search exhausts only its bounded graph"), Stats.VisitedNodes <= Stats.GridNodes && Stats.GridNodes <= LKNavigation::GridNodeLimit);
    TestTrue(TEXT("Failed large query clears previous path"), Path.IsEmpty());
    Walls.RemoveAt(FrontWallIndex);
    TestTrue(TEXT("Destroyed wall opens route without retaining stale blocked edges"), LKNavigation::FindPath(Start, Goal, Walls, Bounds, Path, &Stats));
    TestFalse(TEXT("Changed collision geometry cannot reuse stale grid"), Stats.bReusedGrid);
    Previous = Start;
    for (const FVector& P : Path)
    { TestTrue(TEXT("Route after wall removal clears remaining capsules"), LKNavigation::IsSegmentClear(Previous, P, Walls)); Previous = P; }
    AddInfo(FString::Printf(TEXT("Navigation benchmark (non-gating): 32 walls cold %.3f ms; warm mean %.3f ms; %d grid nodes, %d visited; closed 36-wall graph %.3f ms."), ColdMs, WarmMs, MazeNodes, MazeVisited, ClosedMs));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallNavigationLimit, "LittleKing.V084.Walls.TwoHundredObstaclesStayWithinSearchBudget", Flags)
bool FLKV084WallNavigationLimit::RunTest(const FString&)
{
    LKNavigation::FBounds Bounds; Bounds.HalfExtent = FVector2D(1600.f, 2200.f);
    TArray<LKNavigation::FObstacle> Obstacles;
    // A dense collection of 200 partially overlapping segments is a worst-case collision
    // candidate set, while still leaving a clearly open route above and below the strip.
    for (int32 I = 0; I < 200; ++I)
    {
        const float X = -700.f + (I % 40) * 35.f;
        const float Y = -120.f + (I / 40) * 60.f;
        Obstacles.Add(LKBuildingGeometry::WallShape(FVector(X, Y, 0), float(I % 3) * 15.f, 300.f, 36.f, 35.f));
    }
    const FVector Start(0, -1500, 0), Goal(0, 1500, 0);
    TArray<FVector> Path; LKNavigation::FSearchStats Stats;
    const double Time = FPlatformTime::Seconds();
    if (!TestTrue(TEXT("Dense wall strip can be detoured"), LKNavigation::FindPath(Start, Goal, Obstacles, Bounds, Path, &Stats))) { return false; }
    const double Milliseconds = (FPlatformTime::Seconds() - Time) * 1000.0;
    TestTrue(TEXT("Two hundred obstacles do not create a 3200-node dense visibility graph"), Stats.bUsedGrid);
    TestTrue(TEXT("Search memory and expansions have a fixed cap"), Stats.GridNodes <= LKNavigation::GridNodeLimit && Stats.VisitedNodes <= Stats.GridNodes);
    FVector Previous = Start;
    for (const FVector& P : Path)
    { TestTrue(TEXT("Dense-field returned route clears every real capsule"), LKNavigation::IsSegmentClear(Previous, P, Obstacles)); Previous = P; }
    AddInfo(FString::Printf(TEXT("Navigation 200-obstacle benchmark (non-gating): %.3f ms, %d nodes, %d visited, %d indexed exact segment checks."), Milliseconds, Stats.GridNodes, Stats.VisitedNodes, Stats.ExactSegmentTests));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallPlacement, "LittleKing.V084.Walls.PlacementBoundsOverlapAndMatching", Flags)
bool FLKV084WallPlacement::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    const auto& Row = *LKUnitContent::Find("Unit_WallCommon");
    const FVector2D Bounds(Env.GM->GetGameData()->FieldHalfWidth, Env.GM->GetGameData()->FieldHalfHeight);
    auto CanPlace = [&](FVector P, float Yaw) { return LKBuildingGeometry::CanPlaceWall(Env.GetTestWorld(), ELKTeam::Player, Row, P, Yaw, Bounds); };
    TestFalse(TEXT("A player wall may not cross the center line with its far end"), CanPlace(FVector(0, -100, 0), 90.f));
    TestTrue(TEXT("Turning the same center sideways keeps the full wall on the friendly half"), CanPlace(FVector(0, -100, 0), 0.f));
    TestFalse(TEXT("Full width matters near field edge"), CanPlace(FVector(Bounds.X - 100, -400, 0), 0.f));
    TestTrue(TEXT("A rotated thin footprint fits the same side margin"), CanPlace(FVector(Bounds.X - 100, -400, 0), 90.f));
    const FVector Center(0, -500, 0);
    auto* Wall = Env.Wall("Unit_WallCommon", Center, 20.f);
    if (!TestNotNull(TEXT("Existing wall"), Wall)) { return false; }
    TestTrue(TEXT("Partially overlapping wall segments can form corners"), CanPlace(Center + FVector(100, 0, 0), 90.f));
    TestNull(TEXT("Partial overlap without close center does not become repair"), LKBuildingGeometry::FindMatchingWall(Env.GetTestWorld(), ELKTeam::Player, Center + FVector(100, 0, 0), 20.f));
    TestTrue(TEXT("Close matching placement repairs the old wall"), LKBuildingGeometry::FindMatchingWall(Env.GetTestWorld(), ELKTeam::Player, Center + FVector(20, 0, 0), 25.f) == Wall);
    TestTrue(TEXT("Exact distance threshold is inclusive"), LKBuildingGeometry::FindMatchingWall(Env.GetTestWorld(), ELKTeam::Player, Center + FVector(35, 0, 0), 20.f) == Wall);
    TestTrue(TEXT("Exact angle threshold is inclusive"), LKBuildingGeometry::FindMatchingWall(Env.GetTestWorld(), ELKTeam::Player, Center, 30.f) == Wall);
    TestNull(TEXT("Different orientation at the same center builds another segment"), LKBuildingGeometry::FindMatchingWall(Env.GetTestWorld(), ELKTeam::Player, Center, 31.f));
    TestTrue(TEXT("A reversed wall is the same physical segment"), LKBuildingGeometry::FindMatchingWall(Env.GetTestWorld(), ELKTeam::Player, Center, 200.f) == Wall);
    TestNull(TEXT("Enemy walls can never be repaired by friendly deployment"), LKBuildingGeometry::FindMatchingWall(Env.GetTestWorld(), ELKTeam::Enemy, Center, 20.f));
    auto* Tower = Env.GM->SpawnUnitForTeam("Building_ArrowTower", ELKTeam::Player, FVector(500, -500, 0));
    if (!TestNotNull(TEXT("Existing fixed building"), Tower)) { return false; }
    TestFalse(TEXT("Walls cannot overlap other fixed buildings"), CanPlace(Tower->GetActorLocation(), 0.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084ThinWallAtFieldEdge, "LittleKing.V084.Walls.ThinFootprintAtFieldEdgeCanBuildAndReplace", Flags)
bool FLKV084ThinWallAtFieldEdge::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    auto* Data = Env.GM->GetGameData();
    auto* Silver = Env.GM->GetTeamSilver(ELKTeam::Player);
    auto* Deck = Env.GM->GetTeamDeck(ELKTeam::Player);
    // This margin fits the real 18 cm wall half-thickness, but intentionally
    // does not fit the generic 50 cm circular unit radius used by other units.
    const FVector Center(Data->FieldHalfWidth - 25.f, -500.f, 0.f);
    constexpr float Yaw = 90.f;
    TestTrue(TEXT("Regression fixture exercises a wall thinner than the generic unit body"),
        25.f < Data->UnitBodyRadius && 25.f > LKUnitContent::Find("Unit_WallCommon")->WallThickness * .5f);
    int32 Slot = Env.PutInHand("Card_WallCommon");
    if (!TestTrue(TEXT("Common wall available for edge placement"), Slot != INDEX_NONE)) { return false; }
    TestEqual(TEXT("A wall whose actual thickness crosses the boundary is still rejected"),
        Env.GM->ValidateCardPlay(ELKTeam::Player, Slot, FVector(Data->FieldHalfWidth - 17.f, -850.f, 0.f), Yaw), ELKPlayResult::InvalidLocation);
    TestEqual(TEXT("Preview accepts a complete thin wall footprint inside the field"),
        Env.GM->ValidateCardPlay(ELKTeam::Player, Slot, Center, Yaw), ELKPlayResult::Success);
    const float Before = Silver->GetSilver();
    TestEqual(TEXT("Actual card placement agrees with the thin-wall preview"),
        Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, Center, Yaw), ELKPlayResult::Success);
    auto Walls = Env.Walls();
    if (!TestEqual(TEXT("Edge placement creates one live wall"), Walls.Num(), 1)) { return false; }
    auto* Old = Walls[0];
    TestTrue(TEXT("Legal edge placement retains requested position and orientation"),
        Old->GetActorLocation().Equals(Center) && FMath::IsNearlyEqual(Old->GetWallYaw(), Yaw));
    TestEqual(TEXT("Edge placement charges three silver exactly once"), Silver->GetSilver(), Before - 3.f);
    TestEqual(TEXT("Edge placement starts normal construction"), Old->GetConstructionRemaining(), 10.f);
    FullUniqueHand(*this, Deck);
    Env.Finish(Old);

    Slot = Env.PutInHand("Card_WallRare");
    if (!TestTrue(TEXT("Higher wall available for edge replacement"), Slot != INDEX_NONE)) { return false; }
    const float BeforeReplacement = Silver->GetSilver();
    const int32 UnitsBeforeReplacement = Env.GM->CountAliveUnits(ELKTeam::Player);
    TestEqual(TEXT("Higher-tier replacement preview accepts the existing edge footprint"),
        Env.GM->ValidateCardPlay(ELKTeam::Player, Slot, Center, Yaw + 180.f), ELKPlayResult::Success);
    TestEqual(TEXT("Higher-tier replacement can spawn at the original thin-wall footprint"),
        Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, Center, Yaw + 180.f), ELKPlayResult::Success);
    Walls = Env.Walls();
    if (!TestEqual(TEXT("Replacement leaves exactly one live edge wall"), Walls.Num(), 1)) { return false; }
    auto* New = Walls[0];
    TestTrue(TEXT("Old edge wall is replaced only after the upgraded wall exists"), New != Old && Old->IsActorBeingDestroyed());
    TestEqual(TEXT("Replacement has rare wall identity"), New->GetUnitId(), FName("Unit_WallRare"));
    TestTrue(TEXT("Reversed matching placement preserves old center and front"),
        New->GetActorLocation().Equals(Center) && FMath::IsNearlyEqual(New->GetWallYaw(), Yaw));
    TestEqual(TEXT("Upgraded edge wall starts its own construction timer"), New->GetConstructionRemaining(), 15.f);
    TestEqual(TEXT("Upgraded edge wall receives the correct health"), New->GetMaxHealth(), 5000.f);
    TestEqual(TEXT("Edge replacement charges five silver exactly once"), Silver->GetSilver(), BeforeReplacement - 5.f);
    TestEqual(TEXT("Edge replacement keeps the team unit count stable"), Env.GM->CountAliveUnits(ELKTeam::Player), UnitsBeforeReplacement);
    FullUniqueHand(*this, Deck);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallRepairReplacement, "LittleKing.V084.Walls.RepairReplacePaymentAndFullHand", Flags)
bool FLKV084WallRepairReplacement::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    const FVector Center(0, -450, 0); const float Yaw = 27.f;
    auto* Wall = Env.Wall("Unit_WallRare", Center, Yaw);
    if (!TestNotNull(TEXT("Existing rare wall"), Wall)) { return false; }
    Env.Finish(Wall); Env.Health(Wall, 2500.f);
    auto* Silver = Env.GM->GetTeamSilver(ELKTeam::Player); auto* Deck = Env.GM->GetTeamDeck(ELKTeam::Player);
    Env.GM->GetGameData()->MaxUnitsPerTeam = Env.GM->CountAliveUnits(ELKTeam::Player);
    int32 Slot = Env.PutInHand("Card_WallCommon"); if (!TestTrue(TEXT("Common wall available"), Slot != INDEX_NONE)) { return false; }
    float Before = Silver->GetSilver(); const FName Next = Deck->GetNextCard();
    TestEqual(TEXT("Repair succeeds even when global unit count is full"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, Center + FVector(20, 0, 0), Yaw + 180.f), ELKPlayResult::Success);
    TestEqual(TEXT("Lower tier repairs by incoming wall's 100% maximum health"), Wall->GetHealth(), 3500.f);
    TestTrue(TEXT("Repair preserves original center and front"), Wall->GetActorLocation().Equals(Center) && FMath::IsNearlyEqual(Wall->GetWallYaw(), Yaw));
    TestEqual(TEXT("Repair does not create another wall"), Env.Walls().Num(), 1);
    TestEqual(TEXT("Repair costs three silver once"), Silver->GetSilver(), Before - 3.f);
    TestEqual(TEXT("Played slot is immediately filled by queued next card"), Deck->GetHandCard(Slot), Next); FullUniqueHand(*this, Deck);

    Env.Health(Wall, 4900.f); Slot = Env.PutInHand("Card_WallRare"); Before = Silver->GetSilver();
    TestEqual(TEXT("Equal tier repairs rather than replaces"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, Center, Yaw), ELKPlayResult::Success);
    TestEqual(TEXT("Repair clamps at old maximum health"), Wall->GetHealth(), Wall->GetMaxHealth());
    TestTrue(TEXT("Repair does not restart construction"), Wall->IsConstructionComplete());
    TestEqual(TEXT("Equal repair cost"), Silver->GetSilver(), Before - 5.f);
    FullUniqueHand(*this, Deck);

    Slot = Env.PutInHand("Card_WallEpic"); Before = Silver->GetSilver();
    const int32 UnitsBefore = Env.GM->CountAliveUnits(ELKTeam::Player);
    TestEqual(TEXT("Higher tier replaces atomically even at global unit cap"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, Center + FVector(15, 0, 0), Yaw + 180.f), ELKPlayResult::Success);
    const auto Walls = Env.Walls();
    if (!TestEqual(TEXT("Replacement leaves exactly one live wall"), Walls.Num(), 1)) { return false; }
    auto* New = Walls[0];
    TestTrue(TEXT("Old actor removed and upgraded actor created"), New != Wall && Wall->IsActorBeingDestroyed());
    TestEqual(TEXT("New epic wall identity"), New->GetUnitId(), FName("Unit_WallEpic"));
    TestTrue(TEXT("Higher wall preserves exact old position and front"), New->GetActorLocation().Equals(Center) && FMath::IsNearlyEqual(New->GetWallYaw(), Yaw));
    TestEqual(TEXT("New construction begins at twenty seconds"), New->GetConstructionRemaining(), 20.f);
    TestEqual(TEXT("Replacement starts with epic health"), New->GetMaxHealth(), 10000.f);
    TestFalse(TEXT("Replacement is not instantly built"), New->IsConstructionComplete());
    TestEqual(TEXT("Replacement keeps the unit count stable"), Env.GM->CountAliveUnits(ELKTeam::Player), UnitsBefore);
    TestEqual(TEXT("Replacement costs seven silver once"), Silver->GetSilver(), Before - 7.f);
    FullUniqueHand(*this, Deck);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallPlacementRollback, "LittleKing.V084.Walls.RejectedPlacementKeepsSilverAndDeck", Flags)
bool FLKV084WallPlacementRollback::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    const int32 Slot = Env.PutInHand("Card_WallCommon"); if (Slot == INDEX_NONE) { return false; }
    auto* Silver = Env.GM->GetTeamSilver(ELKTeam::Player); auto* Deck = Env.GM->GetTeamDeck(ELKTeam::Player);
    const auto BeforeHand = Deck->GetHand(); const auto BeforeCards = Deck->GetAllCards(); const float BeforeSilver = Silver->GetSilver();
    TestEqual(TEXT("Center inside friendly half is insufficient if the end crosses midfield"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, FVector(0, -100, 0), 90.f), ELKPlayResult::InvalidLocation);
    TestTrue(TEXT("Invalid footprint leaves hand and queue unchanged"), Deck->GetHand() == BeforeHand && Deck->GetAllCards() == BeforeCards);
    TestEqual(TEXT("Invalid footprint does not spend silver"), Silver->GetSilver(), BeforeSilver);
    TestEqual(TEXT("Invalid footprint creates no walls"), Env.Walls().Num(), 0);
    auto* Existing = Env.Wall("Unit_WallCommon", FVector(0, -500, 0));
    if (!TestNotNull(TEXT("Existing wall for malformed request"), Existing)) { return false; }
    TestEqual(TEXT("Non-finite rotation cannot masquerade as repair"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, Existing->GetActorLocation(), std::numeric_limits<float>::quiet_NaN()), ELKPlayResult::InvalidLocation);
    TestEqual(TEXT("Malformed orientation cannot spend silver"), Silver->GetSilver(), BeforeSilver);
    TestTrue(TEXT("Malformed orientation cannot cycle cards"), Deck->GetHand() == BeforeHand && Deck->GetAllCards() == BeforeCards);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallConstructionDisplacement, "LittleKing.V084.Walls.ConstructionDisplacesCoveredUnitSafely", Flags)
bool FLKV084WallConstructionDisplacement::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    auto* Wall = Env.Wall("Unit_WallCommon", FVector(0, -500, 0));
    if (!TestNotNull(TEXT("New wall starts construction"), Wall)) { return false; }
    auto* Walker = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Player, Wall->GetActorLocation());
    if (!TestNotNull(TEXT("Walking unit can occupy not-yet-solid construction area"), Walker)) { return false; }
    TestFalse(TEXT("Unfinished wall is not a navigation obstacle"), LKBuildingGeometry::BlocksMovement(Wall));
    const float HP = Walker->GetHealth(); Env.Finish(Wall);
    TestTrue(TEXT("Completed wall becomes solid"), LKBuildingGeometry::BlocksMovement(Wall));
    TestFalse(TEXT("Covered unit is no longer intersecting the completed wall"), LKBuildingGeometry::Overlaps(LKBuildingGeometry::Shape(Wall), LKBuildingGeometry::Shape(Walker)));
    TestTrue(TEXT("Evacuated unit remains at a valid walkable point"), Walker->GetMovementComponent()->CanStandAt(Walker->GetActorLocation()));
    TestEqual(TEXT("Construction displacement causes no damage"), Walker->GetHealth(), HP);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallProjectile, "LittleKing.V084.Walls.ProjectileHitsFarEndBeforeUnitBehind", Flags)
bool FLKV084WallProjectile::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    auto* Wall = Env.Wall("Unit_WallCommon", FVector(0, 300, 0), 0.f, ELKTeam::Enemy);
    if (!TestNotNull(TEXT("Enemy wall"), Wall)) { return false; }
    Env.Finish(Wall);
    auto* Behind = Env.GM->SpawnUnitForTeam("Unit_Skeleton", ELKTeam::Enemy, FVector(120, 450, 0));
    auto* Archer = Env.GM->SpawnUnitForTeam("Unit_Archer", ELKTeam::Player, FVector(120, -100, 0));
    if (!Behind || !Archer) { return false; }
    const float WallHealth = Wall->GetHealth(), BehindHealth = Behind->GetHealth();
    auto* Shot = Env.GM->AcquireProjectile(Archer->GetActorLocation(), 75.f, ELKTeam::Player, Archer, FVector(0, 1, 0));
    if (!TestNotNull(TEXT("Real pooled attack projectile"), Shot)) { return false; }
    Shot->Tick(1.f);
    TestEqual(TEXT("Projectile crossing far end damages wall instead of passing through"), Wall->GetHealth(), WallHealth - 75.f);
    TestEqual(TEXT("Unit behind far wall span is protected"), Behind->GetHealth(), BehindHealth);
    TestFalse(TEXT("Projectile is consumed at the wall"), Shot->IsPooledActive());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084CrossedWallMelee, "LittleKing.V084.Walls.CrossedSegmentsRemainBreakableByMeleeAndCart", Flags)
bool FLKV084CrossedWallMelee::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    const FVector Center(0, -250, 0);
    auto* Horizontal = Env.Wall("Unit_WallCommon", Center, 0.f, ELKTeam::Enemy);
    auto* Vertical = Env.Wall("Unit_WallCommon", Center, 90.f, ELKTeam::Enemy);
    if (!Horizontal || !Vertical) { return false; }
    Env.Finish(Horizontal); Env.Finish(Vertical); Env.Health(Vertical, 25.f);
    auto* Soldier = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Player, Center + FVector(0, -250, 0));
    if (!TestNotNull(TEXT("Melee attacker outside the exposed wall end"), Soldier)) { return false; }
    TestTrue(TEXT("Melee collision stops at the target wall's exposed face, not its crossed center"), Soldier->GetMovementComponent()->HasClearMeleePath(Vertical));
    Soldier->SetForcedTarget(Vertical, 30.f);
    const float HorizontalBefore = Horizontal->GetHealth();
    for (int32 I = 0; I < 60 && Vertical->IsAlive(); ++I) { Soldier->Tick(.05f); }
    TestTrue(TEXT("Real attack windups can break a wall whose center overlaps another wall"), Vertical->IsDead());
    TestEqual(TEXT("The intersecting wall behind the hit face receives no accidental damage"), Horizontal->GetHealth(), HorizontalBefore);

    const FVector CartCenter(600, -250, 0);
    auto* Cross = Env.Wall("Unit_WallCommon", CartCenter, 0.f, ELKTeam::Enemy);
    auto* CartTarget = Env.Wall("Unit_WallCommon", CartCenter, 90.f, ELKTeam::Enemy);
    if (!Cross || !CartTarget) { return false; }
    Env.Finish(Cross); Env.Finish(CartTarget); Env.Health(CartTarget, 150.f);
    auto* Cart = Env.GM->SpawnUnitForTeam("Unit_SiegeCart", ELKTeam::Player, CartCenter + FVector(0, -250, 0));
    if (!TestNotNull(TEXT("Siege cart outside the crossed walls"), Cart)) { return false; }
    for (int32 I = 0; I < 30 && CartTarget->IsAlive(); ++I) { Cart->Tick(.05f); }
    TestTrue(TEXT("Cart's normal closest-building targeting and windup can break exposed crossed wall"), CartTarget->IsDead());
    TestEqual(TEXT("Cart does not need to destroy the wall behind the target face first"), Cross->GetHealth(), Cross->GetMaxHealth());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallBreachTarget, "LittleKing.V084.Walls.EnclosedTargetRequiresReachableBreach", Flags)
bool FLKV084WallBreachTarget::RunTest(const FString&)
{
    FWallWorld Env; if (!Env.Open(*this)) { return false; }
    const FVector Center(0, -350, 0);
    auto* Target = Env.GM->SpawnUnitForTeam("Unit_Skeleton", ELKTeam::Enemy, Center);
    auto* Attacker = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Player, Center + FVector(0, -600, 0));
    if (!Target || !Attacker) { return false; }
    TArray<ALKUnitBuilding*> Walls = {
        Env.Wall("Unit_WallCommon", Center + FVector(0, -150, 0), 0, ELKTeam::Enemy),
        Env.Wall("Unit_WallCommon", Center + FVector(0, 150, 0), 0, ELKTeam::Enemy),
        Env.Wall("Unit_WallCommon", Center + FVector(-150, 0, 0), 90, ELKTeam::Enemy),
        Env.Wall("Unit_WallCommon", Center + FVector(150, 0, 0), 90, ELKTeam::Enemy),
    };
    for (auto* Wall : Walls) { if (!TestNotNull(TEXT("Enclosure wall"), Wall)) { return false; } Env.Finish(Wall); }
    TestFalse(TEXT("Enclosed opponent cannot be reached by normal movement"), Attacker->GetMovementComponent()->CanReach(Center));
    auto* Breach = Attacker->GetMovementComponent()->FindBlockingEnemyWall(Center);
    if (!TestNotNull(TEXT("Navigation supplies a wall that can be attacked to open the route"), Breach)) { return false; }
    TestTrue(TEXT("Selected wall belongs to the hostile enclosure"), Walls.Contains(Breach) && Breach->GetTeam() == ELKTeam::Enemy);
    TestTrue(TEXT("Attacker can reach the selected wall's near attack point"), Attacker->GetMovementComponent()->CanReach(Attacker->GetChaseDestination(Breach)));
    Attacker->SetTarget(Target); Attacker->Tick(.01f);
    TestTrue(TEXT("Battle targeting changes from unreachable victim to a breakable wall"), Cast<ALKUnitBuilding>(Attacker->GetTarget()) != nullptr);
    Breach->Die();
    TestTrue(TEXT("After the wall breaks the original victim is reachable"), Attacker->GetMovementComponent()->CanReach(Center));
    return true;
}
#endif
