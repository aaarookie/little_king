#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "LKDataTypes.h"
#include "ULKTraitAuraComponent.generated.h"

class ALKUnitBase;

/**
 * 特性光环组件（S4）：挂在有"光环类特性"的单位上（如骑士光环）。
 * 行为：每 0.5s 刷新一次——范围内友军施加【无限时长】的属性 GE（记录 handle），
 * 离开范围/死亡则按 handle 移除（否则 buff 永久生效）。
 * 每个光环修改器一组独立管理；不自己 Tick，由所属单位 ALKUnitBase::Tick 驱动 TickAura。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ULKTraitAuraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULKTraitAuraComponent();

	/** 注册一个光环修改器（Mod.AuraRadius 需 > 0；Value 为百分比加成，如 0.15 = +15% 基础值） */
	void AddAuraModifier(const FLKTraitModifier& Mod, float Radius);

	/** 由所属单位每帧驱动（内部按 0.5s 节流刷新） */
	void TickAura(float DeltaSeconds);

	/** 立即移除对所有目标施加的光环效果（单位死亡时调用） */
	void RemoveAllAuras();

	/** 当前生效中的光环效果总数（调试/日志用） */
	int32 GetActiveEffectCount() const;

private:
	struct FLKAuraGroup
	{
		FLKTraitModifier Modifier;
		float Radius = 0.f;
		/** 目标 -> 施加的 GE handle（用于离圈移除） */
		TMap<TWeakObjectPtr<ALKUnitBase>, FActiveGameplayEffectHandle> Applied;
	};

	TArray<FLKAuraGroup> Groups;
	float RefreshTimer = 0.f;
	float RefreshInterval = 0.5f;
	bool bCleanedUp = false;

	/** 收集半径内的存活友军（不含自己） */
	void CollectTargetsInRange(const ALKUnitBase* OwnerUnit, const FLKAuraGroup& Group, TArray<ALKUnitBase*>& OutTargets) const;
	void RefreshGroup(ALKUnitBase* OwnerUnit, FLKAuraGroup& Group);
};
