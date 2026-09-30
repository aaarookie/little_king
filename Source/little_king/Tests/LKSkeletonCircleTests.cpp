#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"

#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../LKSkeletonCircle.h"
#include "../LKDataTypes.h"
#include "../ULKGameData.h"
#include "../ULKUnitStatusComponent.h"

#include <limits>

namespace
{
constexpr EAutomationTestFlags CircleTestFlags =
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

/** 测试用真实战场：真 GameMode + 真单位生成入口 + 真状态组件，不设置任何召唤计数。 */
struct FLKSkeletonCircleTestWorld : FTestWorldWrapper
{
    ALKBattleGameMode* GM = nullptr;

    bool Open(FAutomationTestBase& Test)
    {
        if (!CreateTestWorld(EWorldType::Game)) { ForwardErrorMessages(&Test); return false; }
        GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
        if (!BeginPlayInTestWorld()) { ForwardErrorMessages(&Test); return false; }
        GM = GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (!Test.TestNotNull(TEXT("Battle GameMode starts"), GM)) { return false; }
        GetTestWorld()->SpawnActor<ALKPlayerController>();
        GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes = false;
        GM->GetGameData()->bDrawFieldBounds = false;
        return true;
    }

    bool StartBattle(FAutomationTestBase& Test)
    {
        for (int32 i = 0; i < GM->AvailableHeroes.Num(); ++i)
        {
            const FVector Point((i - 1) * 700.f, -1300.f, 0.f);
            if (!Test.TestEqual(TEXT("Player hero deploys"),
                GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[i], Point), ELKPlayResult::Success))
            {
                return false;
            }
        }
        GM->ForceStartBattle();
        return Test.TestEqual(TEXT("Battle starts"), GM->GetPhase(), ELKGamePhase::Battle);
    }

    ALKUnitBase* SpawnUnit(FName UnitId, ELKTeam Team, const FVector& Location) const
    {
        return GM->SpawnUnitForTeam(UnitId, Team, Location);
    }

    /** 只生成 actor，不初始化（用于参数校验用例）。 */
    ALKSkeletonCircle* SpawnCircleActor(FAutomationTestBase& Test, const FVector& Center) const
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = GM;
        ALKSkeletonCircle* Circle = GetTestWorld()->SpawnActor<ALKSkeletonCircle>(
            ALKSkeletonCircle::StaticClass(), FVector(Center.X, Center.Y, 0.f), FRotator::ZeroRotator, SpawnParams);
        Test.TestNotNull(TEXT("Circle actor spawns"), Circle);
        return Circle;
    }

    /** 通过真实 InitializeCircle 入口启动法阵（与 GameMode 施法调用同一入口）。 */
    ALKSkeletonCircle* SpawnCircle(FAutomationTestBase& Test, const FLKSkeletonCircleParams& Params,
        const FVector& Center, ELKTeam Team = ELKTeam::Enemy, int32 Seed = 20260925,
        FName CardId = TEXT("Spell_SkeletonCircle"))
    {
        ALKSkeletonCircle* Circle = SpawnCircleActor(Test, Center);
        if (!Circle) { return nullptr; }
        if (!Test.TestTrue(TEXT("Circle initializes through the real entry point"),
            Circle->InitializeCircle(GM, Team, Center, Params, Seed, CardId)))
        {
            Circle->Destroy();
            return nullptr;
        }
        return Circle;
    }

    bool RejectInit(FAutomationTestBase& Test, ALKSkeletonCircle* Circle, const FVector& Center,
        const FLKSkeletonCircleParams& Params, const TCHAR* What) const
    {
        return Test.TestFalse(What,
            Circle->InitializeCircle(GM, ELKTeam::Enemy, Center, Params, 7, TEXT("Spell_SkeletonCircle")));
    }
};

/** 计划书数值：预警 0.75 / 持续 3 / 0.5 秒一次 ×6。半径默认放大，保证落点区域充足。 */
FLKSkeletonCircleParams CircleParams(float Radius = 800.f)
{
    FLKSkeletonCircleParams Params;
    Params.WarnSeconds = 0.75f;
    Params.Duration = 3.f;
    Params.Radius = Radius;
    Params.MoveMultiplier = 0.65f;
    Params.AttackSpeedMultiplier = 0.70f;
    Params.SummonInterval = 0.5f;
    Params.MaxSummons = 6;
    Params.SummonUnitId = "Unit_Skeleton";
    Params.PlacementAttempts = 12;
    return Params;
}
} // namespace

// 恰好预警边界 + 分帧一致 + t=0 不召 + 正常六次。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCircleWarningBoundaryAndSixSummonsTest,
    "LittleKing.BalanceV1.Circle.WarningBoundaryAndSixSummons", CircleTestFlags)
bool FLKCircleWarningBoundaryAndSixSummonsTest::RunTest(const FString& Parameters)
{
    FLKSkeletonCircleTestWorld Env;
    if (!Env.Open(*this) || !Env.StartBattle(*this)) { return false; }

    ALKSkeletonCircle* Circle = Env.SpawnCircle(*this, CircleParams(), FVector(0.f, 900.f, 0.f));
    if (!Circle) { return false; }

    TestTrue(TEXT("Circle starts in warning"), Circle->IsWarned());
    TestEqual(TEXT("Warning starts with the authored duration"), Circle->GetWarningRemaining(), 0.75f);

    // 恰好一帧耗尽预警：Overflow == 0，本帧生效时间必须为 0，不能沿用整帧 DeltaSeconds。
    Circle->AdvanceForTest(0.75f);
    TestFalse(TEXT("Warning ends exactly at the boundary"), Circle->IsWarned());
    TestFalse(TEXT("Boundary frame does not finish the circle"), Circle->IsFinished());
    TestEqual(TEXT("Boundary frame advances no active time"), Circle->GetActiveElapsed(), 0.f);
    TestEqual(TEXT("Boundary frame keeps the full active duration"), Circle->GetActiveRemaining(), 3.f);
    TestEqual(TEXT("t=0 consumes no summon node"), Circle->GetSummonAttempts(), 0);
    TestEqual(TEXT("t=0 never summons"), Circle->GetSummonCount(), 0);

    // 分帧到达与一次到达必须等价：0.25 + 0.25 恰好走到第一个节点 t=0.5。
    Circle->AdvanceForTest(0.25f);
    TestEqual(TEXT("Sub-frame active time accumulates"), Circle->GetActiveElapsed(), 0.25f);
    TestEqual(TEXT("First node at 0.5 is not due yet"), Circle->GetSummonAttempts(), 0);
    Circle->AdvanceForTest(0.25f);
    TestEqual(TEXT("First node triggers exactly at 0.5"), Circle->GetSummonAttempts(), 1);
    TestEqual(TEXT("First node summons through the real spawn entry"), Circle->GetSummonCount(), 1);
    TestEqual(TEXT("No node is skipped while placements are legal"), Circle->GetSkippedSummons(), 0);
    TestTrue(TEXT("Attempts equal successes plus failures"), Circle->GetSummonAttempts() == Circle->GetSummonCount() + Circle->GetSkippedSummons());

    // t=1.0 … 3.0：剩余五个节点各一次，共六次，到期立即结束。
    for (int32 Step = 2; Step <= 6; ++Step)
    {
        Circle->AdvanceForTest(0.5f);
        const FString What = FString::Printf(TEXT("Node %d triggers once"), Step);
        TestEqual(What, Circle->GetSummonAttempts(), Step);
    }
    TestEqual(TEXT("Six nodes, six successes"), Circle->GetSummonCount(), 6);
    TestEqual(TEXT("No failures in an open area"), Circle->GetSkippedSummons(), 0);
    TestEqual(TEXT("Active time ends exactly at Duration"), Circle->GetActiveElapsed(), 3.f);
    TestTrue(TEXT("Circle finishes when Duration elapses"), Circle->IsFinished());
    TestFalse(TEXT("Finished circle is destroyed"), IsValid(Circle));

    // 结束后再推进不会产生额外召唤（对象已结束，调用被忽略）。
    const int32 AttemptsAfterFinish = Circle->GetSummonAttempts();
    Circle->AdvanceForTest(10.f);
    TestEqual(TEXT("Finished circle never attempts again"), Circle->GetSummonAttempts(), AttemptsAfterFinish);
    return true;
}

// 极大 DeltaTime + 召唤失败次数上限 + 参数上限钳制。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCircleHugeDeltaAndFailureCapTest,
    "LittleKing.BalanceV1.Circle.HugeDeltaAndFailureCap", CircleTestFlags)
bool FLKCircleHugeDeltaAndFailureCapTest::RunTest(const FString& Parameters)
{
    FLKSkeletonCircleTestWorld Env;
    if (!Env.Open(*this) || !Env.StartBattle(*this)) { return false; }

    // A) 一帧 1000 秒：只允许处理持续时间内的六个节点，绝不允许越过持续时间补发到成功为止。
    {
        ALKSkeletonCircle* Circle = Env.SpawnCircle(*this, CircleParams(), FVector(0.f, 900.f, 0.f));
        if (!Circle) { return false; }
        Circle->AdvanceForTest(1000.f);
        TestEqual(TEXT("Huge delta consumes exactly the six nodes"), Circle->GetSummonAttempts(), 6);
        TestTrue(TEXT("Huge delta never exceeds MaxSummons"), Circle->GetSummonAttempts() <= Circle->GetMaxSummons());
        TestEqual(TEXT("Attempts stay successes plus failures"), Circle->GetSummonAttempts(), Circle->GetSummonCount() + Circle->GetSkippedSummons());
        TestEqual(TEXT("Huge delta clamps active time to Duration"), Circle->GetActiveElapsed(), 3.f);
        TestTrue(TEXT("Huge delta finishes the circle on time"), Circle->IsFinished());
    }

    // B) 无法落点（半径小于单位体积 => 搜索半径为 0）：六次尝试全部失败，失败消耗时间点、不重试不补发。
    {
        ALKSkeletonCircle* Failed = Env.SpawnCircle(*this, CircleParams(40.f), FVector(-700.f, 900.f, 0.f));
        if (!Failed) { return false; }
        Failed->AdvanceForTest(1000.f);
        TestEqual(TEXT("Failure path still consumes exactly six nodes"), Failed->GetSummonAttempts(), 6);
        TestEqual(TEXT("No summon succeeds without a legal location"), Failed->GetSummonCount(), 0);
        TestEqual(TEXT("Every failed node is counted as skipped"), Failed->GetSkippedSummons(), 6);
        TestTrue(TEXT("Failed circle still ends at Duration"), Failed->IsFinished());
    }

    // C) 参数上限：MaxSummons 被钳制到结构上限 12；长持续时间 + 极大 Delta 也只尝试 12 次。
    {
        FLKSkeletonCircleParams Params = CircleParams();
        Params.MaxSummons = 999;
        Params.Duration = 30.f;
        ALKSkeletonCircle* Capped = Env.SpawnCircle(*this, Params, FVector(700.f, 900.f, 0.f));
        if (!Capped) { return false; }
        TestEqual(TEXT("MaxSummons is clamped to the documented cap"), Capped->GetMaxSummons(), 12);
        Capped->AdvanceForTest(1000.f);
        TestEqual(TEXT("Capped circle attempts exactly the cap"), Capped->GetSummonAttempts(), 12);
        TestEqual(TEXT("Capped attempts stay successes plus failures"), Capped->GetSummonAttempts(), Capped->GetSummonCount() + Capped->GetSkippedSummons());
        TestEqual(TEXT("Capped active time clamps to Duration"), Capped->GetActiveElapsed(), 30.f);
        TestTrue(TEXT("Capped circle finishes at Duration"), Capped->IsFinished());
    }
    return true;
}

// 敌友减速、离开区域、重叠来源互不误删、销毁清理。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCircleAreaSlowAndCleanupTest,
    "LittleKing.BalanceV1.Circle.AreaSlowTeamsLeaveOverlapAndCleanup", CircleTestFlags)
bool FLKCircleAreaSlowAndCleanupTest::RunTest(const FString& Parameters)
{
    FLKSkeletonCircleTestWorld Env;
    if (!Env.Open(*this) || !Env.StartBattle(*this)) { return false; }

    const FVector Center(0.f, 900.f, 0.f);
    ALKUnitBase* Victim = Env.SpawnUnit("Unit_Swordsman", ELKTeam::Player, Center);
    ALKUnitBase* Ally = Env.SpawnUnit("Unit_Skeleton", ELKTeam::Enemy, Center + FVector(120.f, 0.f, 0.f));
    ALKUnitBase* Outside = Env.SpawnUnit("Unit_Archer", ELKTeam::Player, Center + FVector(0.f, -1600.f, 0.f));
    if (!TestNotNull(TEXT("Player victim spawns"), Victim)
        || !TestNotNull(TEXT("Caster-side ally spawns"), Ally)
        || !TestNotNull(TEXT("Out-of-radius player unit spawns"), Outside))
    {
        return false;
    }

    ALKSkeletonCircle* Circle = Env.SpawnCircle(*this, CircleParams(320.f), Center);
    if (!Circle) { return false; }
    const FName Source = Circle->GetSourceId();
    TestFalse(TEXT("Source identity is not empty"), Source.IsNone());

    // 预警期间不减速（可躲避）。
    Circle->AdvanceForTest(0.5f);
    TestFalse(TEXT("Warning does not slow the hostile unit"), Victim->GetStatusComponent()->HasAreaSlow(Source));

    // 生效帧（恰好跨过预警）：区域状态立即建立。
    Circle->AdvanceForTest(0.25f);
    TestTrue(TEXT("Active frame slows the hostile unit"), Victim->GetStatusComponent()->HasAreaSlow(Source));
    TestTrue(TEXT("Move multiplier is reduced"), Victim->GetStatusComponent()->AreaSlowMoveMultiplier() < 1.f);
    TestTrue(TEXT("Attack interval is lengthened (attack speed 0.70 means interval / 0.70)"),
        Victim->GetStatusComponent()->IntervalMultiplier() > 1.f);
    TestEqual(TEXT("Only the hostile unit inside the radius is affected"), Circle->GetAffectedUnits().Num(), 1);
    TestFalse(TEXT("Caster-side unit is never slowed"), Ally->GetStatusComponent()->HasAreaSlow(Source));
    TestFalse(TEXT("Unit outside the radius is never slowed"), Outside->GetStatusComponent()->HasAreaSlow(Source));

    // 离开区域立即解除该来源。
    Victim->SetActorLocation(Center + FVector(0.f, 1000.f, 0.f));
    Circle->AdvanceForTest(0.5f);
    TestFalse(TEXT("Leaving the radius removes the slow immediately"), Victim->GetStatusComponent()->HasAreaSlow(Source));
    TestEqual(TEXT("Move multiplier returns to normal after leaving"), Victim->GetStatusComponent()->AreaSlowMoveMultiplier(), 1.f);

    // 重叠来源：A、B 同时覆盖同一单位，离开 A 不得误删 B 的来源。
    ALKSkeletonCircle* A = Env.SpawnCircle(*this, CircleParams(320.f), Center, ELKTeam::Enemy, 111);
    ALKSkeletonCircle* B = Env.SpawnCircle(*this, CircleParams(320.f), FVector(500.f, 900.f, 0.f), ELKTeam::Enemy, 222);
    if (!A || !B) { return false; }
    Victim->SetActorLocation(FVector(250.f, 900.f, 0.f)); // 距 A 250、距 B 250：两个来源都覆盖
    A->AdvanceForTest(0.75f);
    B->AdvanceForTest(0.75f);
    TestTrue(TEXT("First overlapping source registers its slow"), Victim->GetStatusComponent()->HasAreaSlow(A->GetSourceId()));
    TestTrue(TEXT("Second overlapping source registers its slow"), Victim->GetStatusComponent()->HasAreaSlow(B->GetSourceId()));

    Victim->SetActorLocation(FVector(700.f, 900.f, 0.f)); // 距 A 700（圈外）、距 B 200（圈内）
    A->AdvanceForTest(0.25f);
    TestFalse(TEXT("Leaving one overlapping source removes only that source"), Victim->GetStatusComponent()->HasAreaSlow(A->GetSourceId()));
    TestTrue(TEXT("The other overlapping source stays"), Victim->GetStatusComponent()->HasAreaSlow(B->GetSourceId()));
    TestTrue(TEXT("Unit stays slowed by the remaining source"), Victim->GetStatusComponent()->AreaSlowMoveMultiplier() < 1.f);

    // 销毁清理：Destroy 走 EndPlay，必须解除本法阵登记的全部来源。
    B->Destroy();
    TestFalse(TEXT("Destroyed circle is gone"), IsValid(B));
    TestFalse(TEXT("Destroy removes the source it registered"), Victim->GetStatusComponent()->HasAreaSlow(B->GetSourceId()));
    TestEqual(TEXT("No area slow remains after the last source is destroyed"), Victim->GetStatusComponent()->AreaSlowMoveMultiplier(), 1.f);
    return true;
}

// 低帧率下的状态刷新：刷新时长必须覆盖当前长帧，否则减速会在下一帧刷新前过期。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCircleLowFrameRateRefreshTest,
    "LittleKing.BalanceV1.Circle.LowFrameRateSlowRefresh", CircleTestFlags)
bool FLKCircleLowFrameRateRefreshTest::RunTest(const FString& Parameters)
{
    FLKSkeletonCircleTestWorld Env;
    if (!Env.Open(*this) || !Env.StartBattle(*this)) { return false; }

    const FVector Center(0.f, 900.f, 0.f);
    ALKUnitBase* Victim = Env.SpawnUnit("Unit_Swordsman", ELKTeam::Player, Center);
    if (!TestNotNull(TEXT("Player victim spawns"), Victim)) { return false; }

    ALKSkeletonCircle* Circle = Env.SpawnCircle(*this, CircleParams(320.f), Center);
    if (!Circle) { return false; }
    const FName Source = Circle->GetSourceId();

    Circle->AdvanceForTest(0.75f); // 恰好进入生效
    Circle->AdvanceForTest(1.f);   // 低帧率：单帧 1 秒
    TestTrue(TEXT("Slow is applied on a long frame"), Victim->GetStatusComponent()->HasAreaSlow(Source));

    // 单位在同一长帧内结算状态：减速必须仍然存在（旧的 0.25 秒刷新会在这里过期）。
    Victim->GetStatusComponent()->TickStatus(0.9f);
    TestTrue(TEXT("Slow survives a frame longer than the nominal refresh window"), Victim->GetStatusComponent()->HasAreaSlow(Source));
    TestTrue(TEXT("Slow still reduces move speed after the long frame"), Victim->GetStatusComponent()->AreaSlowMoveMultiplier() < 1.f);
    return true;
}

// GameMode 失效/不在战斗的立即清理 + 参数有限性与上限校验。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCircleGuardAndParamValidationTest,
    "LittleKing.BalanceV1.Circle.GameModeGuardAndParamValidation", CircleTestFlags)
bool FLKCircleGuardAndParamValidationTest::RunTest(const FString& Parameters)
{
    FLKSkeletonCircleTestWorld Env;
    if (!Env.Open(*this)) { return false; } // 故意不开始战斗：Phase 仍为 Deployment

    // 不在战斗阶段：第一次 Tick 就必须清理自己，不把区域状态/召唤带进战斗。
    ALKSkeletonCircle* OutOfBattle = Env.SpawnCircle(*this, CircleParams(320.f), FVector(0.f, 900.f, 0.f));
    if (!OutOfBattle) { return false; }
    OutOfBattle->AdvanceForTest(0.1f);
    TestTrue(TEXT("Circle out of battle cleans itself up"), OutOfBattle->IsFinished());
    TestFalse(TEXT("Out-of-battle circle is destroyed"), IsValid(OutOfBattle));

    // GameMode 为空：拒绝初始化。
    ALKSkeletonCircle* Probe = Env.SpawnCircleActor(*this, FVector(0.f, 900.f, 0.f));
    if (!Probe) { return false; }
    const FVector Center(0.f, 900.f, 0.f);
    TestFalse(TEXT("Null GameMode is rejected"),
        Probe->InitializeCircle(nullptr, ELKTeam::Enemy, Center, CircleParams(), 1, TEXT("Spell_SkeletonCircle")));

    const float Inf = std::numeric_limits<float>::infinity();
    const float NaN = std::numeric_limits<float>::quiet_NaN();

    FLKSkeletonCircleParams Params = CircleParams();
    Params.Duration = 0.f;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Zero duration is rejected"));
    Params = CircleParams();
    Params.Duration = Inf;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Infinite duration is rejected"));
    Params = CircleParams();
    Params.Radius = 0.f;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Zero radius is rejected"));
    Params = CircleParams();
    Params.Radius = NaN;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("NaN radius is rejected"));
    Params = CircleParams();
    Params.SummonInterval = 0.f;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Zero summon interval is rejected"));
    Params = CircleParams();
    Params.MaxSummons = 0;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Zero MaxSummons is rejected"));
    Params = CircleParams();
    Params.WarnSeconds = -1.f;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Negative warning time is rejected"));
    Params = CircleParams();
    Params.MoveMultiplier = 0.f;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Zero move multiplier is rejected"));
    Params = CircleParams();
    Params.AttackSpeedMultiplier = Inf;
    Env.RejectInit(*this, Probe, Center, Params, TEXT("Infinite attack speed multiplier is rejected"));
    Params = CircleParams();
    Env.RejectInit(*this, Probe, FVector(NaN, 0.f, 0.f), Params, TEXT("NaN center is rejected"));

    // 边界内的合法参数仍然可以通过（上述拒绝不能误伤正常用法）。
    Params = CircleParams();
    TestTrue(TEXT("Valid parameters still initialize"),
        Probe->InitializeCircle(Env.GM, ELKTeam::Enemy, Center, Params, 3, TEXT("Spell_SkeletonCircle")));
    Probe->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
