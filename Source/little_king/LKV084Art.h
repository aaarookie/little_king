#pragma once

#include "CoreMinimal.h"

class UPaperSprite;
class UPaperFlipbook;
class UTexture2D;

/** Original v0.8.4 building assets. Static buildings deliberately use one-frame clips. */
namespace LKV084Art
{
    const TArray<FName>& UnitIds();
    const TArray<FName>& CardIds();
    FName Identity(FName UnitOrCardId);
    TSoftObjectPtr<UPaperSprite> Sprite(FName UnitId);
    TSoftObjectPtr<UTexture2D> CardIcon(FName CardId);
    UPaperFlipbook* Animation(FName UnitId, FName State);
    const TArray<FName>& SoundIds();
    FString SoundPath(FName Id);
    float SoundInterval(FName Id);
}
