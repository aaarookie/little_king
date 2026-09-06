#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LKTypes.h"
#include "ALKProjectile.generated.h"

class USphereComponent;
class UPaperSpriteComponent;

/**
 * 远程攻击的直线弹道：命中敌方单位造成伤害后【回收】（S5 对象池），
 * 由 ALKBattleGameMode::AcquireProjectile / ReleaseProjectile 管理复用。
 * 无 GameMode 的直生成旧路径（Init）仍保留作兜底。
 */
UCLASS()
class ALKProjectile : public AActor
{
	GENERATED_BODY()

public:
	ALKProjectile();

	virtual void Tick(float DeltaSeconds) override;

	/** 直生成旧路径（无池兜底）：按当前生成位置直接启用 */
	void Init(float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection);

	/** 从池取出：设置位置/参数并启用（可见+碰撞+飞行） */
	void ActivateFromPool(const FVector& InLocation, float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection);

	/** 回收：隐藏+停碰撞+停用（可再次激活复用） */
	void DeactivateToPool();

	bool IsPooledActive() const { return bPooledActive; }
	FVector GetFlightDirection() const { return Direction; }

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Projectile")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Projectile")
	TObjectPtr<UPaperSpriteComponent> SpriteComp;

	/** 申请发射参数（Activate 时统一设置） */
	void ApplyLaunchParams(float InDamage, ELKTeam InTeam, AActor* InInstigator, const FVector& InDirection);

	float Damage = 0.f;
	ELKTeam Team = ELKTeam::Player;
	TWeakObjectPtr<AActor> InstigatorActor;
	FVector Direction = FVector::ForwardVector;
	float Speed = 900.f;
	float Lifetime = 3.f;
	bool bPooledActive = false;
	FLKCombatSource LaunchSource;
};
