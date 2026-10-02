#pragma once

#include "CoreMinimal.h"

class UPaperSprite;
class UPaperFlipbook;
class UTexture2D;

/** v0.8.3 storybook presentation. The three tiers share role artwork, never gameplay data. */
namespace LKV083Art
{
    const TArray<FName>& UnitIds();
    const TArray<FName>& SpellIds();
    FName Identity(FName UnitId);
    TSoftObjectPtr<UPaperSprite> Sprite(FName UnitId);
    TSoftObjectPtr<UTexture2D> CardIcon(FName CardId);
    UPaperFlipbook* Animation(FName UnitId, FName State);
    const TArray<FName>& SoundIds();
    FString SoundPath(FName Id);
    float SoundInterval(FName Id);
}
