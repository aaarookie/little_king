#pragma once

#include "CoreMinimal.h"
#include "LKDataTypes.h"

class ULKCardDefinition;
class ULKGameData;

/** 建筑的稳定身份；仅通过图书馆研究对应图纸永久解锁。 */
namespace LKV084Content
{
    const TMap<FName, FLKUnitRow>& Units();
    const TArray<FName>& CardIds();
    /** 显式映射，避免把所有 Card_ 前缀的扩展卡误当作建筑。 */
    FName UnitIdForCard(FName CardId);
    bool ConfigureCard(ULKCardDefinition& Card, bool bInitializeTuning);
    void EnsureCards(ULKGameData& Data);
}
