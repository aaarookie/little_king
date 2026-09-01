#include "ALKUnitBase.h"

#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"

#include "ALKProjectile.h"
#include "ALKBattleGameMode.h"
#include "LKDataTypes.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "ULKGameData.h"
#include "ULKUnitAttributeSet.h"
#include "ULKUnitMovementComponent.h"

ALKUnitBase::ALKUnitBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	SetRootComponent(SpriteComponent);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BodyCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	BodyCollision->SetupAttachment(SpriteComponent);
	BodyCollision->SetBoxExtent(FVector(BodyRadius, BodyRadius, 20.f));
	BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyCollision->SetCollisionObjectType(ECC_Pawn);
	BodyCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
	BodyCollision->SetGenerateOverlapEvents(true);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(false);

	UnitAttributes = CreateDefaultSubobject<ULKUnitAttributeSet>(TEXT("Attributes"));

	MovementComponent = CreateDefaultSubobject<ULKUnitMovementComponent>(TEXT("Movement"));
}

void ALKUnitBase::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystem && UnitAttributes)
	{
		AbilitySystem->AddAttributeSetSubobject(UnitAttributes.Get());
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

UAbilitySystemComponent* ALKUnitBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ALKUnitBase::InitUnit(const FLKUnitRow& Row, ULKGameData* InGameData)
{
	GameDataCached = InGameData;

	UnitId = Row.UnitId;
	UnitClass = Row.UnitClass;
	bIsMage = Row.bIsMage;
	AttackType = Row.AttackType;
	HeroTraits = Row.HeroTraits;
	BodyRadius = InGameData ? InGameData->UnitBodyRadius : 50.f;
	BodyCollision->SetBoxExtent(FVector(BodyRadius, BodyRadius, 20.f));
	MovementComponent->SetSeparationRadius(BodyRadius);
	if (InGameData)
	{
		MovementComponent->SetFieldBounds(FVector2D(InGameData->FieldHalfWidth, InGameData->FieldHalfHeight));
	}

	// 精灵（占位期可能没有，用调试色块代替）
	if (!Row.Sprite.IsNull())
	{
		if (UPaperSprite* Sprite = Row.Sprite.LoadSynchronous())
		{
			SpriteComponent->SetSprite(Sprite);
			SetActorScale3D(FVector(Row.SpriteScale.X, Row.SpriteScale.Y, 1.f));
		}
	}

	ApplyRowAttributes(Row);

	UE_LOG(LogLKUnit, Log, TEXT("[Unit] Spawn %s (class=%d, team=%d) @ %s"),
		*UnitId.ToString(), (int32)UnitClass, (int32)Team, *GetActorLocation().ToString());
}

void ALKUnitBase::ApplyRowAttributes(const FLKUnitRow& Row)
{
	if (!AbilitySystem)
	{
		return;
	}

	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), Row.BaseHealth);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetMaxHealthAttribute(), Row.BaseHealth);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetMoveSpeedAttribute(), Row.MoveSpeed);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetAttackRangeAttribute(), Row.AttackRange);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetAttackDamageAttribute(), Row.AttackDamage);
	AbilitySystem->SetNumericAttributeBase(ULKUnitAttributeSet::GetAttackIntervalAttribute(), Row.AttackInterval);
}

void ALKUnitBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	DrawDebugShape();

	// 部署阶段：单位冻结（不索敌/不移动/不攻击），开战后由 GameMode 打开
	if (!bCombatEnabled)
	{
		if (State != ELKUnitState::Idle)
		{
			State = ELKUnitState::Idle;
			MovementComponent->Stop();
		}
		return;
	}

	UpdateStateMachine(DeltaSeconds);
}

void ALKUnitBase::UpdateStateMachine(float DeltaSeconds)
{
	if (IsBuilding())
	{
		// 建筑由子类 ALKUnitBuilding 处理（哨塔/兵营）
		return;
	}

	// 定期重新索敌
	TargetRetryTimer -= DeltaSeconds;
	if (TargetRetryTimer <= 0.f)
	{
		AcquireTarget();
		TargetRetryTimer = 0.25f;
	}

	AActor* Target = TargetActor.Get();
	if (!IsValid(Target) || !Target->IsA<ALKUnitBase>() || Cast<ALKUnitBase>(Target)->IsDead())
	{
		TargetActor = nullptr;
		State = ELKUnitState::Idle;
		MovementComponent->Stop();
		return;
	}

	const float Dist = DistanceTo2D(Target);
	const float Range = GetAttackRange();

	// 滞回判定（防射程边界抖动）：
	// - 移动中：进入攻击需要 Dist <= Range
	// - 攻击中：只要 Dist <= Range + AttackStopBuffer 就继续攻击（不来回切换）
	const float StopDist = (State == ELKUnitState::Attacking)
		? Range + (GameDataCached ? GameDataCached->AttackStopBuffer : 30.f)
		: Range;

	if (Dist > StopDist)
	{
		State = ELKUnitState::Moving;
		MovementComponent->MoveToward(Target->GetActorLocation(), GetMoveSpeed());
	}
	else
	{
		State = ELKUnitState::Attacking;
		MovementComponent->Stop();
		TryAttack(DeltaSeconds);
	}
}

void ALKUnitBase::AcquireTarget()
{
	if (TargetActor.IsValid())
	{
		AActor* Current = TargetActor.Get();
		if (IsValid(Current) && Current->IsA<ALKUnitBase>() && !Cast<ALKUnitBase>(Current)->IsDead()
			&& Cast<ALKUnitBase>(Current)->GetTeam() != Team)
		{
			return; // 目标仍有效
		}
	}

	TargetActor = FindNearestEnemy();
}

AActor* ALKUnitBase::FindNearestEnemy() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(4000.f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, Shape, Params);

	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ALKUnitBase* Other = Cast<ALKUnitBase>(Overlap.GetActor());
		if (!Other || Other == this || Other->IsDead() || Other->GetTeam() == Team)
		{
			continue;
		}

		const float Dist = DistanceTo2D(Other);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Other;
		}
	}

	return Best;
}

void ALKUnitBase::TryAttack(float DeltaSeconds)
{
	AttackCooldownRemaining -= DeltaSeconds;

	AActor* Target = TargetActor.Get();
	if (AttackCooldownRemaining > 0.f || !IsValid(Target))
	{
		return;
	}

	// 目标已死亡（尚未销毁的 0.5 秒内）不攻击，并清除目标
	const ALKUnitBase* TargetUnit = Cast<ALKUnitBase>(Target);
	if (TargetUnit && TargetUnit->IsDead())
	{
		TargetActor = nullptr;
		return;
	}

	if (DistanceTo2D(Target) > GetAttackRange())
	{
		return;
	}

	PerformAttack(Target);
	AttackCooldownRemaining = GetAttackInterval();
}

void ALKUnitBase::PerformAttack(AActor* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	if (AttackType == ELKAttackType::Ranged)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		FVector Muzzle = GetActorLocation();
		Muzzle.Z = 20.f;
		const FVector Dir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

		FActorSpawnParameters Params;
		Params.Owner = this;
		ALKProjectile* Projectile = World->SpawnActor<ALKProjectile>(ALKProjectile::StaticClass(), Muzzle, FRotator::ZeroRotator, Params);
		if (Projectile)
		{
			Projectile->Init(GetAttackDamage(), Team, this, Dir);
		}
	}
	else
	{
		LKGameplay::ApplyDamage(Target, GetAttackDamage(), this);
	}
}

float ALKUnitBase::DistanceTo2D(const AActor* Other) const
{
	if (!Other)
	{
		return TNumericLimits<float>::Max();
	}
	return FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());
}

float ALKUnitBase::GetHealth() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetHealthAttribute(), 1.f);
}

float ALKUnitBase::GetMaxHealth() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetMaxHealthAttribute(), 1.f);
}

float ALKUnitBase::GetAttackDamage() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetAttackDamageAttribute(), 0.f);
}

float ALKUnitBase::GetAttackRange() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetAttackRangeAttribute(), 0.f);
}

float ALKUnitBase::GetAttackInterval() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetAttackIntervalAttribute(), 1.f);
}

float ALKUnitBase::GetMoveSpeed() const
{
	return LKGameplay::GetAttributeValue(this, ULKUnitAttributeSet::GetMoveSpeedAttribute(), 0.f);
}

void ALKUnitBase::Die()
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	State = ELKUnitState::Dead;
	MovementComponent->Stop();
	OnUnitDied.Broadcast(this);
	SetLifeSpan(0.5f);

	UE_LOG(LogLKUnit, Log, TEXT("[Unit] %s (%s) 阵亡"), *UnitId.ToString(), *GetName());
}

void ALKUnitBase::DrawDebugShape() const
{
	if (!GameDataCached || !GameDataCached->bDrawDebugShapes || SpriteComponent->GetSprite())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor Color = (Team == ELKTeam::Player) ? FColor::Green : FColor::Red;
	DrawDebugBox(World, GetActorLocation() + FVector(0.f, 0.f, 10.f),
		FVector(BodyRadius, BodyRadius, 10.f), Color, false, -1.f, 0, 2.f);

	// 索敌线：单位 -> 当前目标（调试 AI 行为用）
	if (TargetActor.IsValid())
	{
		DrawDebugLine(World,
			GetActorLocation() + FVector(0.f, 0.f, 20.f),
			TargetActor->GetActorLocation() + FVector(0.f, 0.f, 20.f),
			Color, false, -1.f, 0, 1.f);
	}
}
