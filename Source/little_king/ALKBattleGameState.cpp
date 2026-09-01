#include "ALKBattleGameState.h"
#include "LKLog.h"

void ALKBattleGameState::SetPhase(ELKGamePhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}

	Phase = NewPhase;
	OnPhaseChanged.Broadcast(NewPhase);
	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 阶段切换 -> %d"), (int32)NewPhase);
}

void ALKBattleGameState::EndMatch(ELKTeam InWinner)
{
	if (Phase == ELKGamePhase::Result)
	{
		return;
	}

	Winner = InWinner;
	SetPhase(ELKGamePhase::Result);
	OnMatchEnded.Broadcast(Winner);
	UE_LOG(LogLKBattle, Log, TEXT("[Battle] 对局结束，胜者: %d"), (int32)Winner);
}
