#pragma once
#include "CoreMinimal.h"
#include "ALKUnitBase.h"
#include "ALKHeroCamp.generated.h"

class ALKUnitHero;

/** 固定的实体营地：建筑，占用空间，不可攻击，不计入兵力和建筑卡上限。 */
UCLASS()
class ALKHeroCamp : public ALKUnitBase
{
	GENERATED_BODY()
public:
	ALKHeroCamp();
	virtual bool IsCamp() const override { return true; }
	void InitializeCamp(ALKUnitHero* Hero, ULKGameData* Data);
	ALKUnitHero* GetHero() const;
private:
	TWeakObjectPtr<ALKUnitHero> LinkedHero;
};
