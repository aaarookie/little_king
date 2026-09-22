#pragma once
#include "CoreMinimal.h"
#if WITH_EDITOR
class ALKBattleGameMode;
class ULKGameData;
namespace LKBattleArtPreview
{
    bool Enabled();
    void Prepare(ALKBattleGameMode* Mode, TObjectPtr<ULKGameData>& Data);
    void Tick(ALKBattleGameMode* Mode);
}
#endif
