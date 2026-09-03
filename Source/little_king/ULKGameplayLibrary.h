#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ULKGameplayLibrary.generated.h"

class AActor;

/**
 * 技能蓝图（GA_*）函数库：英雄技能里"把效果打出去"的统一入口。
 * - 伤害/治疗走 LKGameplay（GAS 运行时 GE + SetByCaller），敌我过滤在这里完成；
 * - 蓝图技能只负责：选中心/填数值 -> 调用本库函数 -> End Ability。
 */
UCLASS()
class ULKGameplayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 范围伤害：Caster 是单位时只打其【敌方】；Caster 不是单位时打半径内除 Caster 外所有单位。返回命中数 */
	UFUNCTION(BlueprintCallable, Category = "LK|Skill")
	static int32 LK_ApplyDamageInRadius(AActor* CenterActor, float Radius, float Damage, AActor* Caster);

	/** 范围治疗：Caster 是单位时只治疗其【友方】（含自己）；Caster 不是单位时不生效。返回命中数 */
	UFUNCTION(BlueprintCallable, Category = "LK|Skill")
	static int32 LK_ApplyHealInRadius(AActor* CenterActor, float Radius, float HealAmount, AActor* Caster);

	/** 找 Unit 最近的敌方单位（单体技能索敌用）；无敌人返回空 */
	UFUNCTION(BlueprintPure, Category = "LK|Skill")
	static AActor* LK_GetNearestEnemy(AActor* Unit);

	/** 读单位当前生命（任意有 GAS 属性的 Actor；没有则返回 0） */
	UFUNCTION(BlueprintPure, Category = "LK|Skill")
	static float LK_GetUnitHealth(AActor* Unit);

	/** 读单位最大生命 */
	UFUNCTION(BlueprintPure, Category = "LK|Skill")
	static float LK_GetUnitMaxHealth(AActor* Unit);
};
