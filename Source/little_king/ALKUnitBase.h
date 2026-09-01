#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "LKTypes.h"
#include "ALKUnitBase.generated.h"

class UAbilitySystemComponent;
class ULKUnitAttributeSet;
class UPaperSpriteComponent;
class UBoxComponent;
class ULKUnitMovementComponent;
class ULKGameData;
struct FLKUnitRow;

/**
 * 单位基类（佣兵 / 英雄 / 建筑）。
 * 属性走 GAS（ULKUnitAttributeSet），移动/索敌/攻击为自研轻量 FSM。
 * 全部数值由 FLKUnitRow（DT_Units）驱动；找不到行时使用内置默认值，保证无资产可运行。
 */
UCLASS()
class ALKUnitBase : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALKUnitBase();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitDied, ALKUnitBase*, Unit);
	UPROPERTY(BlueprintAssignable, Category = "LK|Unit")
	FOnUnitDied OnUnitDied;

	/** 用数据表行初始化（GameMode 生成后调用） */
	void InitUnit(const FLKUnitRow& Row, ULKGameData* InGameData);

	/** 阵营由 GameMode 生成时指定（InitUnit 之后调用） */
	void SetTeam(ELKTeam InTeam) { Team = InTeam; }

	// ---------- IAbilitySystemInterface ----------
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ---------- 查询 ----------
	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	ELKTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	FName GetUnitId() const { return UnitId; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	bool IsHero() const { return UnitClass == ELKUnitClass::Hero; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	bool IsMage() const { return bIsMage; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	bool IsBuilding() const { return UnitClass == ELKUnitClass::Building; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	bool IsAlive() const { return !bDead; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	ELKUnitState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetAttackDamage() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetAttackRange() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetMoveSpeed() const;

	ULKUnitAttributeSet* GetUnitAttributeSet() const { return UnitAttributes; }

	/** 生命变化（AttributeSet 回调；蓝图可覆写做血条/表现） */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Unit")
	void OnHealthChanged(float Health, float MaxHealth);

	/** 死亡：标记 + 广播 + 短延迟销毁 */
	void Die();

	/** 设置/获取索敌目标 */
	void SetTarget(AActor* NewTarget) { TargetActor = NewTarget; }
	AActor* GetTarget() const { return TargetActor.Get(); }

	/** 战斗开关：部署阶段关闭（不能移动/攻击/放技能），开战由 GameMode 统一打开 */
	void SetCombatEnabled(bool bEnabled) { bCombatEnabled = bEnabled; }
	bool IsCombatEnabled() const { return bCombatEnabled; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<UPaperSpriteComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<UBoxComponent> BodyCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<ULKUnitAttributeSet> UnitAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<ULKUnitMovementComponent> MovementComponent;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Unit")
	FName UnitId;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Unit")
	ELKTeam Team = ELKTeam::Player;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Unit")
	ELKUnitClass UnitClass = ELKUnitClass::Soldier;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Unit")
	bool bIsMage = false;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Unit")
	ELKAttackType AttackType = ELKAttackType::Melee;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Unit")
	TArray<FName> HeroTraits;

	ELKUnitState State = ELKUnitState::Idle;
	TWeakObjectPtr<AActor> TargetActor;
	float TargetRetryTimer = 0.f;
	float AttackCooldownRemaining = 0.f;
	bool bDead = false;
	bool bCombatEnabled = false;
	float BodyRadius = 50.f;

	/** 缓存全局配置（InitUnit 时注入） */
	UPROPERTY(Transient)
	TObjectPtr<ULKGameData> GameDataCached;

	// ---------- 战斗逻辑（子类可覆写） ----------
	virtual void UpdateStateMachine(float DeltaSeconds);
	void AcquireTarget();
	void TryAttack(float DeltaSeconds);
	virtual void PerformAttack(AActor* Target);
	AActor* FindNearestEnemy() const;
	float DistanceTo2D(const AActor* Other) const;
	void ApplyRowAttributes(const FLKUnitRow& Row);
	void DrawDebugShape() const;

	friend class ULKUnitAttributeSet;
};
