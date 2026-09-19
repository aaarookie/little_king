#include "ALKProjectile.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "PaperSpriteComponent.h"

#include "ALKUnitBase.h"
#include "ALKBattleGameMode.h"
#include "LKGameplayHelpers.h"
#include "ULKGameData.h"
#include "ULKUnitStatusComponent.h"
#include "LKLog.h"
#include "EngineUtils.h"

ALKProjectile::ALKProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComp);
	CollisionComp->InitSphereRadius(12.f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComp->SetCollisionObjectType(ECC_Pawn);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Overlap);
	CollisionComp->SetGenerateOverlapEvents(true);
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ALKProjectile::OnOverlapBegin);

	SpriteComp = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	SpriteComp->SetupAttachment(CollisionComp);
	SpriteComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALKProjectile::ApplyLaunchParams(float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection)
{
	Damage = InDamage;
    BreathHead = ELKBreathHead::None; bSiegeShot = false; SiegeTarget.Reset();
	Team = InTeam;
	InstigatorActor = InInstigator;
    LaunchSource = LKGameplay::MakeSource(InInstigator, ELKCombatSourceKind::Projectile, TEXT("RangedAttack"));
	Direction = InDirection.GetSafeNormal2D();

	// 速度/寿命从 DA_GameData 读（运行时默认值兜底）
	if (const ALKBattleGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALKBattleGameMode>() : nullptr)
	{
		if (const ULKGameData* Data = GameMode->GetGameData())
		{
			Speed = Data->ProjectileSpeed;
			Lifetime = Data->ProjectileLifetime;
		}
	}
}

void ALKProjectile::Init(float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection)
{
	ApplyLaunchParams(InDamage, InTeam, InInstigator, InDirection);
	bPooledActive = true;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(false);
    SetActorTickEnabled(true);
}

void ALKProjectile::SetAttackPayload(ELKBreathHead Head, ALKUnitBase* BuildingTarget)
{
    BreathHead = Head; bSiegeShot = BuildingTarget != nullptr; SiegeTarget = BuildingTarget;
    if (Head != ELKBreathHead::None) { LaunchSource.ActionId = Head == ELKBreathHead::Ice ? FName("Attack_IceHead") : FName("Attack_FireHead"); }
    if (BuildingTarget)
    {
        LaunchSource.ActionId = "Attack_Siege";
        Lifetime = FMath::Max(Lifetime, FVector::Dist2D(GetActorLocation(), BuildingTarget->GetActorLocation()) / FMath::Max(1.f, Speed) + 1.f);
    }
}

void ALKProjectile::ActivateFromPool(const FVector& InLocation, float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection)
{
	SetActorLocation(InLocation);
	ApplyLaunchParams(InDamage, InTeam, InInstigator, InDirection);
	bPooledActive = true;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(false);
    SetActorTickEnabled(true);
}

void ALKProjectile::DeactivateToPool()
{
    BreathHead = ELKBreathHead::None; bSiegeShot = false; SiegeTarget.Reset();
    bPooledActive = false;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    SetActorLocation(FVector(0.f, 0.f, -100000.f));
    InstigatorActor = nullptr;
    LaunchSource = FLKCombatSource();
}

void ALKProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPooledActive || DeltaSeconds <= 0.f) { return; }
    ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (GM && GM->GetPhase() != ELKGamePhase::Battle) { GM->ReleaseProjectile(this); return; }
    const float TravelTime = FMath::Min(Lifetime, DeltaSeconds);
    const float Travel = FMath::Max(0.f, Speed * TravelTime);
    const FVector Start = GetActorLocation();
    ALKUnitBase* Closest = nullptr;
    float ClosestDistance = Travel + 1.f;
    if (bSiegeShot)
    {
        ALKUnitBase* Target = SiegeTarget.Get();
        if (!Target || !Target->IsTargetable() || !Target->IsBuilding() || Target->GetTeam() == Team)
        { if (GM) { GM->ReleaseProjectile(this); } else { Destroy(); } return; }
        Direction = (Target->GetActorLocation() - Start).GetSafeNormal2D();
        if (FVector::Dist2D(Start, Target->GetActorLocation()) <= Travel + Target->GetBodyRadius()) { Closest = Target; }
    }
    // 沿整段路径找最早命中的敌方体积，避免低帧率下跨过目标。
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Unit = *It;
        if (bSiegeShot || !Unit->IsTargetable() || Unit->GetTeam() == Team) { continue; }
        FVector Offset = Unit->GetActorLocation() - Start; Offset.Z = 0.f;
        const float Radius = Unit->GetBodyRadius() + 12.f;
        const float Along = FVector::DotProduct(Offset, Direction);
        const float PerpendicularSq = Offset.SizeSquared() - Along * Along;
        if (PerpendicularSq > Radius * Radius) { continue; }
        const float Root = FMath::Sqrt(FMath::Max(0.f, Radius * Radius - PerpendicularSq));
        const float Enter = FMath::Max(0.f, Along - Root);
        if (Along + Root >= 0.f && Enter <= Travel && Enter < ClosestDistance) { Closest = Unit; ClosestDistance = Enter; }
    }
    if (Closest)
    {
        const float HitDamage = Damage;
        const FLKCombatSource Source = LaunchSource;
        const ELKBreathHead Head = BreathHead;
        AActor* SourceActor = InstigatorActor.Get();
        if (GM) { GM->ReleaseProjectile(this); } else { DeactivateToPool(); }
        if (Head != ELKBreathHead::None) { Closest->GetStatusComponent()->ReceiveBreath(Head, HitDamage, SourceActor, Source); }
        else { LKGameplay::ApplyDamage(Closest, HitDamage, SourceActor, false, &Source); }
        if (!GM) { Destroy(); }
        return;
    }
    SetActorLocation(Start + Direction * Travel);
    Lifetime -= DeltaSeconds;
    if (Lifetime <= 0.f) { if (GM) { GM->ReleaseProjectile(this); } else { Destroy(); } }
}

void ALKProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 新版命中统一由 Tick 的连续路径查询处理。
}
