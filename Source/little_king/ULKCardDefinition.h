#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LKTypes.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Spell", EditConditionHides, ClampMin = "10.0"))
	float SpellRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<class UTexture2D> Icon;

	/** 同类建筑是否受数量上限限制（-1 = 不受限制，见 GDD） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "CardType==ELKCardType::Building", EditConditionHides))
	int32 BuildingTypeLimitOverride = -1;
};
