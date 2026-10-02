#pragma once
class ALKBattleGameMode;
namespace LKV083Preview
{
#if WITH_EDITOR
    /** Explicit isolated renderer preview only; the regular PIE boot path is unchanged. */
    void Tick(ALKBattleGameMode* Mode);
    void TickHUD(ALKBattleGameMode* Mode);
#endif
}
