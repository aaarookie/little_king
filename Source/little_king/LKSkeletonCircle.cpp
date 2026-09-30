#include "LKSkeletonCircle.h"

#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "LKLog.h"
#include "ULKGameData.h"
#include "ULKUnitStatusComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Object.h"

namespace
{
	/** 区域减速最短刷新时长；低帧率时按帧长放大，避免单位状态在下一帧刷新前过期。 */
	constexpr float AreaSlowRefreshSeconds = 0.25f;
	/** 单次区域减速刷新允许的最长时长（秒），防止极大 DeltaTime 写出超长状态。 */
	constexpr float AreaSlowMaxSeconds = 5.f;

	/** 参数上限：与 FLKSkeletonCircleParams 的 ClampMin/ClampMax 一致，防止极端值破坏计时、循环与采样。 */
	constexpr int32 SummonAttemptsCap = 12;
	constexpr int32 PlacementAttemptsCap = 64;
	constexpr float WarnSecondsCap = 30.f;
	constexpr float DurationCap = 120.f;
	constexpr float SummonIntervalCap = 120.f;
	constexpr float RadiusCap = 10000.f;
	constexpr float SlowMultiplierFloor = 0.05f;
}

ALKSkeletonCircle::ALKSkeletonCircle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetActorEnableCollision(false);
	// 法阵是场地效果，不参与单位索敌/碰撞；用一个空场景根即可。
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ALKSkeletonCircle::ResetSerial()
{
	// 兼容空操作：跨 World 共享的全局序号已移除。
	// 身份改用本 World 内唯一的 Actor FName（见 InitializeCircle），随机流由调用方 InSeed 决定。
}

void ALKSkeletonCircle::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(true);
}

void ALKSkeletonCircle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Destroy / 结束战斗 / 换房：必须立刻解除本法阵来源的全部减速，不残留到下一阶段。
	bFinished = true;
	ClearAreaSlow();
	Super::EndPlay(EndPlayReason);
}

bool ALKSkeletonCircle::InitializeCircle(ALKBattleGameMode* InGameMode, ELKTeam InTeam, const FVector& InCenter,
	const FLKSkeletonCircleParams& InParams, int32 InSeed, FName InCardId)
{
	if (!InGameMode || InCenter.ContainsNaN())
	{
		return false;
	}
	// 有限性与硬性下界：非法参数直接拒绝，绝不进入计时/召唤循环。
	if (!FMath::IsFinite(InParams.WarnSeconds) || InParams.WarnSeconds < 0.f
		|| !FMath::IsFinite(InParams.Duration) || InParams.Duration <= 0.f
		|| !FMath::IsFinite(InParams.SummonInterval) || InParams.SummonInterval <= 0.f
		|| !FMath::IsFinite(InParams.Radius) || InParams.Radius <= 0.f
		|| !FMath::IsFinite(InParams.MoveMultiplier) || InParams.MoveMultiplier <= 0.f
		|| !FMath::IsFinite(InParams.AttackSpeedMultiplier) || InParams.AttackSpeedMultiplier <= 0.f
		|| InParams.MaxSummons < 1)
	{
		return false;
	}

	GameMode = InGameMode;
	CasterTeam = InTeam;
	Center = FVector(InCenter.X, InCenter.Y, 0.f);
	Params = InParams;
	// 参数上限：超出即钳制（不静默接受无限增长），保证时间点数量与单帧补齐都是有界的。
	Params.WarnSeconds = FMath::Clamp(Params.WarnSeconds, 0.f, WarnSecondsCap);
	Params.Duration = FMath::Clamp(Params.Duration, 0.05f, DurationCap);
	Params.SummonInterval = FMath::Clamp(Params.SummonInterval, 0.02f, SummonIntervalCap);
	Params.Radius = FMath::Clamp(Params.Radius, 1.f, RadiusCap);
	Params.MaxSummons = FMath::Clamp(Params.MaxSummons, 1, SummonAttemptsCap);
	Params.PlacementAttempts = FMath::Clamp(Params.PlacementAttempts, 1, PlacementAttemptsCap);
	Params.MoveMultiplier = FMath::Clamp(Params.MoveMultiplier, SlowMultiplierFloor, 1.f);
	Params.AttackSpeedMultiplier = FMath::Clamp(Params.AttackSpeedMultiplier, SlowMultiplierFloor, 1.f);
	CardId = InCardId;

	// 来源身份：本 World 内唯一的 Actor FName；不再依赖跨 World 共享的全局序号。
	SourceId = FName(*FString::Printf(TEXT("Spell_SkeletonCircle_%s"), *GetFName().ToString()));
	// 专用随机流：只由调用方提供的 InSeed 决定（每次施法派生种子由调用方负责），不使用全局随机。
	Stream.Initialize(InSeed);

	bInitialized = true;
	bWarning = Params.WarnSeconds > 0.f;
	bFinished = false;
	WarningRemaining = Params.WarnSeconds;
	ActiveRemaining = 0.f;
	ActiveElapsed = 0.f;
	NextSummonAt = Params.SummonInterval;
	SummonAttempts = 0;
	SummonCount = 0;
	SkippedSummons = 0;
	AffectedUnits.Reset();
	SetActorTickEnabled(true);
	if (!bWarning) { EnterActive(); }
	UE_LOG(LogLKBattle, Log, TEXT("[Circle] 骷髅法阵就绪 %s：中心 %s 半径 %.0f 预警 %.2f 持续 %.2f 召唤 %.1f×%d 种子 %d"),
		*SourceId.ToString(), *Center.ToCompactString(), Params.Radius, Params.WarnSeconds, Params.Duration,
		Params.SummonInterval, Params.MaxSummons, InSeed);
	return true;
}

void ALKSkeletonCircle::EnterActive()
{
	bWarning = false;
	WarningRemaining = 0.f;
	ActiveRemaining = Params.Duration;
	ActiveElapsed = 0.f;
	NextSummonAt = Params.SummonInterval;
	UE_LOG(LogLKBattle, Log, TEXT("[Circle] %s 预警结束，开始生效 %.2f 秒"), *SourceId.ToString(), Params.Duration);
}

void ALKSkeletonCircle::Finish()
{
	if (bFinished) { return; }
	bFinished = true;
	ClearAreaSlow();
	SetActorTickEnabled(false);
	UE_LOG(LogLKBattle, Log, TEXT("[Circle] %s 结束：尝试 %d 次，成功 %d 个，跳过 %d 次"),
		*SourceId.ToString(), SummonAttempts, SummonCount, SkippedSummons);
	Destroy();
}

void ALKSkeletonCircle::ClearAreaSlow()
{
	for (const TWeakObjectPtr<ALKUnitBase>& Weak : AffectedUnits)
	{
		if (ALKUnitBase* Unit = Weak.Get())
		{
			if (ULKUnitStatusComponent* Status = Unit->GetStatusComponent()) { Status->RemoveAreaSlow(SourceId); }
		}
	}
	AffectedUnits.Reset();
}

void ALKSkeletonCircle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized || bFinished) { return; }

	UWorld* World = GetWorld();
	if (!World)
	{
		Finish();
		return;
	}
	// GameMode 失效或已不在战斗：立即清理区域状态与召唤，不把残留带进结算/下一房。
	if (!IsValid(GameMode.Get()) || GameMode->GetPhase() != ELKGamePhase::Battle)
	{
		Finish();
		return;
	}
	// 非法 DeltaTime 按 0 处理：本帧不推进时间，也不吞掉状态刷新。
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) { DeltaSeconds = 0.f; }

	if (bWarning)
	{
		WarningRemaining = FMath::Max(0.f, WarningRemaining);
		if (DeltaSeconds < WarningRemaining)
		{
			// 仍在预警：本帧不推进生效时间。
			WarningRemaining -= DeltaSeconds;
			DrawCirclePresentation();
			return;
		}
		// 本帧跨越预警结束：只把超出的部分计入生效时间。
		// 恰好归零时 Overflow == 0，本帧生效时间必须为 0，不能沿用整帧 DeltaSeconds 提前推进。
		const float Overflow = FMath::Max(0.f, DeltaSeconds - WarningRemaining);
		EnterActive();
		DeltaSeconds = Overflow;
	}

	// ActiveElapsed 是唯一计时真源并钳制在 [0, Duration]：极大 DeltaTime 不会把计时推过持续时间。
	ActiveElapsed = FMath::Min(ActiveElapsed + DeltaSeconds, Params.Duration);
	ActiveRemaining = FMath::Max(0.f, Params.Duration - ActiveElapsed);
	ProcessSummonSteps();
	RefreshAreaSlow(DeltaSeconds);
	DrawCirclePresentation();

	if (ActiveRemaining <= 0.f)
	{
		// 最后一次（t = Duration）已在上面处理；到期立即清理区域状态。
		Finish();
	}
}

void ALKSkeletonCircle::ProcessSummonSteps()
{
	if (!IsValid(GameMode.Get())) { return; }
	// 只处理持续时间内的节点（ActiveElapsed 已钳制在 [0, Duration]）。
	// 每个节点最多一次尝试，失败也消耗该节点：绝不因为失败而在同一帧向后续节点重试到成功为止。
	const float AttemptWindow = FMath::Min(ActiveElapsed, Params.Duration);
	while (SummonAttempts < Params.MaxSummons && NextSummonAt <= AttemptWindow + KINDA_SMALL_NUMBER)
	{
		const float StepTime = NextSummonAt;
		// 先推进节点：即使 SummonInterval 远小于当前时间也不会卡在同一节点（次数上限保证循环有界）。
		NextSummonAt += Params.SummonInterval;
		++SummonAttempts;
		if (!TrySummonOne())
		{
			++SkippedSummons;
			UE_LOG(LogLKBattle, Log, TEXT("[Circle] %s t=%.2f 召唤点无合法位置或已达上限，跳过（不穿模、不在圈外补发）"),
				*SourceId.ToString(), StepTime);
		}
	}
}

bool ALKSkeletonCircle::TrySummonOne()
{
	if (!IsValid(GameMode.Get()) || Params.SummonUnitId.IsNone()) { return false; }
	ULKGameData* Data = GameMode->GetGameData();
	if (!Data) { return false; }
	if (Data->MaxUnitsPerTeam > 0 && GameMode->CountAliveUnits(CasterTeam) >= Data->MaxUnitsPerTeam) { return false; }
	const float SearchRadius = FMath::Max(0.f, Params.Radius - Data->UnitBodyRadius);
	if (!FMath::IsFinite(SearchRadius) || SearchRadius <= 0.f) { return false; }
	FVector Location = Center;
	if (!GameMode->FindFreeSpawnLocation(Center, SearchRadius, Location, Stream, Params.PlacementAttempts))
	{
		return false;
	}
	ALKUnitBase* Summon = GameMode->SpawnUnitForTeam(Params.SummonUnitId, CasterTeam, Location);
	if (!Summon) { return false; }
	++SummonCount;
	UE_LOG(LogLKBattle, Log, TEXT("[Circle] %s 召唤 %s #%d @ %s"),
		*SourceId.ToString(), *Params.SummonUnitId.ToString(), SummonCount, *Location.ToCompactString());
	return true;
}

void ALKSkeletonCircle::RefreshAreaSlow(float FrameDelta)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	// 低帧率刷新：减速时长至少覆盖本帧（+0.1 秒余量），否则长帧内单位状态会在下一帧刷新前过期。
	const float Frame = FMath::IsFinite(FrameDelta) && FrameDelta > 0.f ? FrameDelta : 0.f;
	const float SlowSeconds = FMath::Clamp(Frame + 0.1f, AreaSlowRefreshSeconds, AreaSlowMaxSeconds);

	TSet<TWeakObjectPtr<ALKUnitBase>> Inside;
	for (TActorIterator<ALKUnitBase> It(World); It; ++It)
	{
		ALKUnitBase* Unit = *It;
		if (!Unit || Unit->IsCamp() || !Unit->IsAlive() || !Unit->IsCombatEnabled()) { continue; }
		// 只影响敌对单位（法阵是敌方指挥官法术）。
		if (Unit->GetTeam() == CasterTeam) { continue; }
		if (FVector::Dist2D(Center, Unit->GetActorLocation()) > Params.Radius) { continue; }
		if (ULKUnitStatusComponent* Status = Unit->GetStatusComponent())
		{
			Inside.Add(Unit);
			Status->ApplyAreaSlow(SourceId, Params.MoveMultiplier, Params.AttackSpeedMultiplier, SlowSeconds);
			AffectedUnits.Add(Unit);
		}
	}
	// 离开区域：立即解除本法阵来源，不影响其它法阵/控制效果。
	for (auto It = AffectedUnits.CreateIterator(); It; ++It)
	{
		ALKUnitBase* Unit = It->Get();
		if (!Unit) { It.RemoveCurrent(); continue; }
		if (Inside.Contains(*It)) { continue; }
		if (ULKUnitStatusComponent* Status = Unit->GetStatusComponent()) { Status->RemoveAreaSlow(SourceId); }
		It.RemoveCurrent();
	}
}

TArray<TWeakObjectPtr<ALKUnitBase>> ALKSkeletonCircle::GetAffectedUnits() const
{
	TArray<TWeakObjectPtr<ALKUnitBase>> Result;
	for (const TWeakObjectPtr<ALKUnitBase>& Weak : AffectedUnits) { if (Weak.IsValid()) { Result.Add(Weak); } }
	return Result;
}

void ALKSkeletonCircle::DrawCirclePresentation() const
{
	// ALKPresentationHUD owns the projected circle and label in all build modes.
	// No world-space debug duplicate: it can intersect the flat battlefield artwork.
}
