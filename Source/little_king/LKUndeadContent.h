#pragma once
#include "CoreMinimal.h"
#include "LKDataTypes.h"

namespace LKUndeadContent
{
    /** 五种亡灵的默认数值和稳定规则；统一合并入口位于 LKUnitContent。 */
    const TMap<FName, FLKUnitRow>& Units();
}
