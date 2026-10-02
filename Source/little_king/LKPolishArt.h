#pragma once
#include "CoreMinimal.h"
class UPaperFlipbook;
class UPaperSprite;
class UFont;
namespace LKPolishArt
{
    const TArray<FName>& UnitIds();
    /** Legacy biped identities using the dedicated MovementFix override; new atlases own their walk clips. */
    const TArray<FName>& WalkingUnitIds();
    bool IsDefaultSprite(FName Id, const UPaperSprite* Sprite);
    UPaperFlipbook* Animation(FName Id, FName State);
    UPaperSprite* HomeLevel(FName Id, int32 Level);
    UFont* Font(bool bTitle);
}
