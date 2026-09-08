#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "LKTypes.h"
#include "LKDataTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "ALKUnitBase.generated.h"

class UAbilitySystemComponent;
class ULKUnitAttributeSet;
class UPaperSpriteComponent;
class USphereComponent;
class ULKUnitMovementComponent;
class ULKTraitAuraComponent;
class ULKGameData;
class ULKUnitPassiveComponent;
struct FLKUnitRow;
struct FLKRunHeroState;

/**
 * 单位基类（佣兵 / 英雄 / 建筑）。
 * 属性走 GAS（ULKUnitAttributeSet），移动/索敌/攻击为自研轻量 FSM。
 * FLKUnitRow 提供运行时单位数据；内置单位由代码锁定玩法身份并从 DT_Units 合并调参数值，自定义 ID 完全按表读取。
 */
UCLASS()
class ALKUnitBase : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()
	friend struct FLKSprint5TestAccess;
	friend struct FLKD2TestAccess;

public:
	ALKUnitBase();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsCamp() const { return false; }
	bool IsTargetable() const { return !bDead && !IsCamp(); }
	float GetBodyRadius() const { return BodyRadius; }
	UPaperSpriteComponent* GetSpriteComponent() const { return SpriteComponent; }
	bool IsUnderFocusWarning() const { return FocusWarningRemaining > 0.f; }
	FLinearColor GetPlaceholderColor() const { return PlaceholderColor; }
	FText GetDisplayName() const { return DisplayName; }
	ULKUnitPassiveComponent* GetPassiveComponent() const { return PassiveComponent; }
	TArray<FName> GetTraits() const { return HeroTraits; }
	float GetTraitEffectValue(ELKTraitEffect Effect) const;
	void SetFocusWarning(float Seconds) { FocusWarningRemaining = FMath::Max(0.f, Seconds); }
	UFUNCTION(BlueprintCallable, Category = "LK|Traits") bool AddTrait(FName TraitId);
	UFUNCTION(BlueprintCallable, Category = "LK|Traits") bool RemoveTrait(FName TraitId);
	UFUNCTION(BlueprintPure, Category = "LK|Traits") bool HasTrait(FName TraitId) const { return HeroTraits.Contains(TraitId); }
	UFUNCTION(BlueprintPure, Category = "LK|Traits") bool HasTraitEffect(ELKTraitEffect Effect) const;
	void AddAuraTauntSource(ALKUnitBase* Source);
	void RemoveAuraTauntSource(ALKUnitBase* Source);
	bool ResolveTrait(FName TraitId, FLKTraitRow& OutRow) const;
	virtual bool CanPursueTarget(const ALKUnitBase* Target) const;
	virtual FVector GetChaseDestination(const ALKUnitBase* Target) const;
	virtual bool IsManualMoving() const { return false; }
	void CancelAttackWindup();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitDied, ALKUnitBase*, Unit);
	UPROPERTY(BlueprintAssignable, Category = "LK|Unit")
	FOnUnitDied OnUnitDied;

	/** 用数据表行初始化（GameMode 生成后调用）；FallbackUnitId 在行内 UnitId 为空时兜底（行名） */
	void InitUnit(const FLKUnitRow& Row, ULKGameData* InGameData, FName FallbackUnitId = NAME_None);
	/** D2：用纯值远征快照替换英雄永久状态，清理房间临时状态后恢复基础最大生命与当前生命。 */
	bool ApplyRunHeroState(const FLKRunHeroState& State);

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
	bool IsHero() const { return UnitClass == ELKUnitClass::Hero || UnitClass == ELKUnitClass::Boss; }
	UFUNCTION(BlueprintPure, Category = "LK|Unit") bool IsBoss() const { return UnitClass == ELKUnitClass::Boss; }
	UFUNCTION(BlueprintPure, Category = "LK|Unit") bool IsSoldier() const { return UnitClass == ELKUnitClass::Soldier; }
	UFUNCTION(BlueprintPure, Category = "LK|Unit") bool IsSkeleton() const { return bSkeleton; }
	UFUNCTION(BlueprintPure, Category = "LK|Unit") bool IsIncapacitated() const { return IsHero() && bDead; }

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
	float GetBaseMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetAttackDamage() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetAttackRange() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetMoveSpeed() const;

	/** 索敌范围（世界单位）：范围内出现敌人才自动锁定战斗；DT_Units 行可覆盖 */
	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	float GetAcquireRadius() const { return AcquireRadius; }

	ULKUnitAttributeSet* GetUnitAttributeSet() const { return UnitAttributes; }

	/** 生命变化（AttributeSet 回调；蓝图可覆写做血条/表现） */
	UFUNCTION(BlueprintImplementableEvent, Category = "LK|Unit")
	void OnHealthChanged(float Health, float MaxHealth);

	/** 佣兵/建筑死亡后销毁；英雄失能并保留，等待被动复活或战后恢复。 */
	void Die();
	bool ReviveDuringBattle();
	void RecoverAfterBattle(float Percent);

	/** 设置/获取索敌目标 */
	void SetTarget(AActor* NewTarget);

	/** 当前索敌目标（技能蓝图可用：单体技能找当前攻击对象） */
	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	AActor* GetTarget() const { return TargetActor.Get(); }

	/**
	 * 无敌状态（通用效果，英雄/佣兵/建筑都可用，未来技能/法术直接调用）：
	 *  免疫所有普通伤害（敌方攻击/弹道/法术/技能），但【不免疫"真伤"】（超时虚弱会穿透无敌）。
	 * DurationSeconds：>0 = 持续秒数；0 = 立即解除；<0 = 永久（直到再调解除）。
	 * 视觉：无敌期间脚下/调试框变为金色。
	 */
	UFUNCTION(BlueprintCallable, Category = "LK|Unit")
	void SetInvulnerable(float DurationSeconds = -1.f);

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	bool IsInvulnerable() const { return bInvulnerable; }

	/** 战斗开关：部署阶段关闭（不能移动/攻击/放技能），开战由 GameMode 统一打开 */
	virtual void SetCombatEnabled(bool bEnabled);
	bool IsCombatEnabled() const { return bCombatEnabled; }

	// ---------- S4：索敌优先级（嘲讽者 > ForcedTarget > 最近可追击敌人） ----------
	/** 强制目标（AI 集火指令）：低于有效嘲讽的索敌目标；DurationSeconds <= 0 立即清除 */
	void SetForcedTarget(AActor* InTarget, float DurationSeconds);

	bool HasForcedTarget() const { return ForcedTargetActor.IsValid(); }

	/** 嘲讽标记（特性 Taunt）：敌方索敌优先攻击本单位 */
	bool IsTaunting() const;

	/** S5 受击反馈：命中瞬间精灵闪白（由伤害管线统一调用；纯视觉无逻辑） */
	void TriggerHitFlash();

	/** S5 死亡表现时长（缩小+淡出动画后销毁） */
	static constexpr float DeathAnimDuration = 0.35f;

	UFUNCTION(BlueprintPure, Category = "LK|Unit")
	ELKAttackType GetAttackType() const { return AttackType; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<USceneComponent> LogicRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<UPaperSpriteComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<USphereComponent> BodyCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<ULKUnitAttributeSet> UnitAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<ULKUnitMovementComponent> MovementComponent;

	UPROPERTY(EditDefaultsOnly, Category = "LK|Unit")
	FName UnitId;
	FText DisplayName;
	FLinearColor PlaceholderColor = FLinearColor::Transparent;
	bool bSkeleton = false;

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

	/** 无敌：>0 = 剩余秒数；<0 = 永久（不倒数）；0 = 不在无敌 */
	bool bInvulnerable = false;
	float InvulnerableRemaining = 0.f;

	/** 强制目标（集火）：倒计时到期/目标失效自动清除 */
	TWeakObjectPtr<AActor> ForcedTargetActor;
	float ForcedTargetRemaining = 0.f;

	/** 嘲讽标记（Taunt 特性，InitUnit 时按 HeroTraits 解析） */
	bool bTaunting = false;
	TMap<TWeakObjectPtr<ALKUnitBase>, int32> AuraTauntSources;
	TArray<FActiveGameplayEffectHandle> SelfTraitHandles;
	TMap<FName, FLKTraitRow> ResolvedTraits;
	TWeakObjectPtr<AActor> WindupTarget;
	float FocusWarningRemaining = 0.f;
	void ChangeTarget(AActor* NewTarget);

	// ---------- S5 打击感状态 ----------
	/** 攻击前摇（秒，DT_Units AttackWindup 注入；0 = 无前摇） */
	float AttackWindup = 0.15f;
	/** 索敌范围（世界单位；DT_Units AcquireRadius 覆盖，0 则用 DA_GameData 默认） */
	float AcquireRadius = 900.f;
	bool bWindupActive = false;
	float WindupRemaining = 0.f;
	/** 命中反馈计时；不冻结战斗逻辑 */
	float HitStopRemaining = 0.f;
	/** 攻击缩放脉冲计时（命中瞬间 1.15 -> 1.0 回弹） */
	float AttackPulseRemaining = 0.f;
	/** 受击闪白计时 */
	float HitFlashRemaining = 0.f;
	/** 死亡缩小淡出动画计时 */
	float DeathAnimRemaining = 0.f;
	/** 精灵基础本地缩放（InitUnit 记录；脉冲/死亡动画在此基础上乘） */
	FVector BaseSpriteLocalScale = FVector(1.f, 1.f, 1.f);

	/** 光环组件（有光环特性时启用；所有单位都挂一个空组件，避免运行时动态创建） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<ULKTraitAuraComponent> TraitAuraComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LK|Unit")
	TObjectPtr<ULKUnitPassiveComponent> PassiveComponent;
	void RestoreHeroLife(float Health, bool bResumeCombat);

	/** 缓存全局配置（InitUnit 时注入） */
	UPROPERTY(Transient)
	TObjectPtr<ULKGameData> GameDataCached;

	// ---------- 战斗逻辑（子类可覆写） ----------
	virtual void UpdateStateMachine(float DeltaSeconds);
	void AcquireTarget();
	void TryAttack(float DeltaSeconds);
	virtual void PerformAttack(AActor* Target);
	/** 最近敌人：默认只考虑索敌范围内；bIgnoreAcquireRange=true 用于行军方向（全图最近） */
	AActor* FindNearestEnemy(bool bTauntersOnly = false, bool bIgnoreAcquireRange = false) const;
	/** 目标是否应放弃（普通目标离开索敌范围、嘲讽者离开嘲讽半径；集火目标不受限） */
	bool ShouldReleaseTarget(const ALKUnitBase* Target) const;
	float DistanceTo2D(const AActor* Other) const;
	void ApplyRowAttributes(const FLKUnitRow& Row);
	void DrawDebugShape() const;
	/** S5 打击感视觉（攻击脉冲/受击闪白恢复），Tick 调用 */
	void TickCombatFeedback(float DeltaSeconds);

	/** 按 HeroTraits + GameData.TraitTable 应用特性（自身修饰/光环/嘲讽标记），InitUnit 末尾调用 */
	void ApplyTraits();
	/** 强制目标到期处理（Tick 调用） */
	void TickForcedTarget(float DeltaSeconds);

	/** InitUnit 末尾回调：子类可在数据就绪后做初始化（英雄技能授予/冷却读取等） */
	virtual void OnUnitInitialized(const FLKUnitRow& Row);
	/** 进入新房间时只清临时战斗状态；永久特性随后由快照重新应用。 */
	virtual void ResetTransientRoomState();

	friend class ULKUnitAttributeSet;
	friend class ULKGameplayLibrary;
};
