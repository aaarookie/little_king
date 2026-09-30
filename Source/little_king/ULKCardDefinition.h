#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LKTypes.h"
#include "LKDataTypes.h"
#include "ULKCardDefinition.generated.h"

/**
 * 卡牌定义（每张卡一个 DataAsset，由 DA_GameData.CardLibrary 汇总）。
 * 牌库（Deck）中只存 CardId，运行时通过 CardLibrary 解析。
 */
UCLASS(BlueprintType)
class ULKCardDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName CardId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText CardName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 Cost = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELKCardType CardType = ELKCardType::Unit;

	/**
	 * 阵营权限。规范身份由 LKCardRules 按 CardId 注册并在运行时校正，
	 * 数据资产即使被误改也不会把敌方专属卡开放给玩家。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ELKCardFaction Faction = ELKCardFaction::Both;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    ELKSpellGrade SpellGrade = ELKSpellGrade::Novice1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
    bool bExpeditionOnly = false;

	/** 角色卡：生成的单位 ID（DT_Units） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Unit", EditConditionHides))
	FName SpawnUnitId = NAME_None;

	/** 建筑卡：生成的建筑单位 ID（DT_Units） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Building", EditConditionHides))
	FName BuildingUnitId = NAME_None;

	// ---------- 法术卡（原型期用 C++ 直接结算；后期换 GAS Ability） ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Spell", EditConditionHides))
	ELKSpellEffect SpellEffect = ELKSpellEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Spell", EditConditionHides, ClampMin = "0.0"))
	float SpellValue = 0.f;

	/** Additional max-health healing for heroes only; affected by expedition spell upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spell", meta=(ClampMin="0",ClampMax="1"))
	float HeroHealPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Spell", EditConditionHides, ClampMin = "10.0"))
	float SpellRadius = 300.f;

	/** 区域召唤法术（SpellEffect=SummonZone）的专用参数；与 Damage/Heal 的 SpellValue 通道独立。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "SpellEffect==ELKSpellEffect::SummonZone", EditConditionHides))
	FLKSkeletonCircleParams Circle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<class UTexture2D> Icon;

	/** 同类建筑上限：-2 继承 GameData，-1 不限，>=0 指定上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Building", EditConditionHides))
	int32 BuildingTypeLimitOverride = -2;
};
