#pragma once
#include "CoreMinimal.h"
#include "LKRunTypes.h"
class UTexture2D;
class UPaperSprite;

/** C-batch display catalogue; IDs never determine combat rules. */
namespace LKWorldArt
{
    int32 RegionIndex(FName RegionId);
    UTexture2D* GroundTexture(FName RegionId);
    UPaperSprite* GroundSprite(FName RegionId);
    UPaperSprite* CampSprite(FName HeroId);
    UTexture2D* NodeIcon(ELKDungeonNodeType Type);
    UTexture2D* EffectTexture(FName Id);
    const TArray<FName>& SoundIds();
    float SoundInterval(FName Id);
    FString SoundPath(FName Id);
}
