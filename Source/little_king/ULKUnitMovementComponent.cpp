#include "ULKUnitMovementComponent.h"
#include "Engine/World.h"
#include "CollisionShape.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "ALKUnitBase.h"
#include "LKLog.h"

ULKUnitMovementComponent::ULKUnitMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void ULKUnitMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMoving || !GetOwner() || DeltaTime <= 0.f)
	{
		return;
	}

	FVector Current = GetOwner()->GetActorLocation();
	FVector ToDest = Destination - Current;
	ToDest.Z = 0.f;

	const float Dist = ToDest.Size();
	const float Step = Speed * DeltaTime;

	if (Dist <= Step || Dist < 1.f)
	{
		// 到达目的地
		GetOwner()->SetActorLocation(FVector(Destination.X, Destination.Y, Current.Z));
		bMoving = false;
	}
	else
	{
		GetOwner()->AddActorWorldOffset(ToDest.GetSafeNormal() * Step);
	}

	ApplySeparation(DeltaTime);
	ClampToFieldBounds();
}

void ULKUnitMovementComponent::ClampToFieldBounds()
{
	AActor* Owner = GetOwner();
	if (!Owner || !bHasFieldBounds)
	{
		return;
	}

	const FVector Loc = Owner->GetActorLocation();
	const FVector Clamped(
		FMath::Clamp(Loc.X, -FieldHalfExtent.X, FieldHalfExtent.X),
		FMath::Clamp(Loc.Y, -FieldHalfExtent.Y, FieldHalfExtent.Y),
		Loc.Z);

	if (!Clamped.Equals(Loc))
	{
		Owner->SetActorLocation(Clamped);
	}
}

void ULKUnitMovementComponent::MoveToward(const FVector& InDestination, float InSpeed)
{
	bMoving = true;
	Destination = InDestination;
	Speed = FMath::Max(0.f, InSpeed);
}

void ULKUnitMovementComponent::ApplySeparation(float DeltaTime)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || DeltaTime <= 0.f)
	{
		return;
	}

	const FVector OwnerLoc = Owner->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionShape Shape = FCollisionShape::MakeSphere(SeparationRadius * 2.f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	if (!World->OverlapMultiByChannel(Overlaps, OwnerLoc, FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ALKUnitBase* Other = Cast<ALKUnitBase>(Overlap.GetActor());
		if (!Other || Other == Owner || Other->IsDead() || Other->IsBuilding())
		{
			continue;
		}

		const FVector OtherLoc = Other->GetActorLocation();
		FVector Delta = OwnerLoc - OtherLoc;
		Delta.Z = 0.f;

		const float Dist = Delta.Size();
		const float MinDist = SeparationRadius * 2.f;

		if (Dist > 0.1f && Dist < MinDist)
		{
			const FVector PushDir = Delta.GetSafeNormal();
			const float Push = (MinDist - Dist) * 0.5f;

			Owner->AddActorWorldOffset(PushDir * Push);
			Other->AddActorWorldOffset(-PushDir * Push);
		}
	}
}
