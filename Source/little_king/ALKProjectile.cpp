#include "ALKProjectile.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "PaperSpriteComponent.h"

#include "ALKUnitBase.h"
#include "LKGameplayHelpers.h"
#include "ULKGameData.h"
#include "ALKBattleGameMode.h"
#include "LKLog.h"

ALKProjectile::ALKProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComp);
	CollisionComp->InitSphereRadius(12.f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_Pawn);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Overlap);
	CollisionComp->SetGenerateOverlapEvents(true);
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ALKProjectile::OnOverlapBegin);

	SpriteComp = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Sprite"));
	SpriteComp->SetupAttachment(CollisionComp);
	SpriteComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALKProjectile::Init(float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection)
{
	Damage = InDamage;
	Team = InTeam;
	InstigatorActor = InInstigator;
	Direction = InDirection.GetSafeNormal2D();

	if (const ALKBattleGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALKBattleGameMode>() : nullptr)
	{
		if (const ULKGameData* Data = GameMode->GetGameData())
		{
			Speed = Data->ProjectileSpeed;
			Lifetime = Data->ProjectileLifetime;
		}
	}
}

void ALKProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Lifetime -= DeltaSeconds;
	if (Lifetime <= 0.f)
	{
		Destroy();
		return;
	}

	AddActorWorldOffset(Direction * Speed * DeltaSeconds);
}

void ALKProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ALKUnitBase* Unit = Cast<ALKUnitBase>(OtherActor);
	if (!Unit || Unit->IsDead() || Unit->GetTeam() == Team)
	{
		return;
	}

	LKGameplay::ApplyDamage(Unit, Damage, InstigatorActor.Get());
	Destroy();
}
