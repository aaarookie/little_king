#pragma once

#include "CoreMinimal.h"
#include "LKDataTypes.h"

/**
 * 内置核心单位的稳定规则。
 * DT_Units 可以调整数值、名称和表现，但不能意外改变这些单位的玩法身份。
 * 不在此注册表中的扩展单位仍完全使用数据表定义。
 */
namespace LKUnitContent
{
    /** 汇总核心、亡灵、临时佣兵、召唤单位与研究建筑目录。 */
    const TMap<FName, FLKUnitRow>& Units();
    /** 支持已登记建筑卡的 Card_* → Unit_* 显式别名，不会猜测未知卡。 */
    const FLKUnitRow* Find(FName UnitId);

    /** 将作者数据的可调字段与代码规则合并；Canonical 必须来自 Units。 */
    FLKUnitRow MergeAuthoredTuning(const FLKUnitRow& Canonical, const FLKUnitRow* Authored);
}
