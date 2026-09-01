#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LKTypes.h"
#include "ALKProjectile.generated.h"

class USphereComponent;
class UPaperSpriteComponent;

/**
 * 远程攻击的简单直线弹道：命中敌方单位造成伤害后销毁。
 * S6 再做对象池与粒子表现。
 */
UCLASS()
class ALKProjectile : public AActor
{
	GENERATED_BODY()

public:
	ALKProjectile();

	virtual void Tick(float DeltaSeconds) override;

	void Init(float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Projectile")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Projectile")
	TObjectPtr<UPaperSpriteComponent> SpriteComp;

	float Damage = 0.f;
	ELKTeam Team = ELKTeam::Player;
	TWeakObjectPtr<AActor> InstigatorActor;
	FVector Direction = FVector::ForwardVector;
	float Speed = 900.f;
	float Lifetime = 3.f;
};
