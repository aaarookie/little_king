#pragma once

#include "CoreMinimal.h"
#include "LKDataTypes.h"

class ULKCardDefinition;

/** V0.8.3 召唤单位与法术的稳定身份；研究资格由 LKResearchContent 登记。 */
namespace LKV083Content
{
    const TMap<FName, FLKUnitRow>& Units();
    const TArray<FName>& SpellIds();
    /** 只在初次创建时写数值，重复加载保留作者调参；规则身份始终校正。 */
    bool ConfigureCard(ULKCardDefinition& Card, bool bInitializeTuning);
}
