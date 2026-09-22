#pragma once

#include "CoreMinimal.h"

class UPaperSprite;
class UTexture2D;

/** Presentation defaults only. Authored Sprite/Icon overrides remain authoritative. */
namespace LKBattleArt
{
    const TArray<FName>& UnitIds();
    TSoftObjectPtr<UPaperSprite> Sprite(FName UnitId);
    TSoftObjectPtr<UTexture2D> CardIcon(FName CardId);
}
