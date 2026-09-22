#pragma once
class ALKBattleGameMode;
class ULKJourneyPresentationSubsystem;
namespace LKPolishArtPreview
{
#if WITH_EDITOR
    void Tick(ALKBattleGameMode* Mode);
    void TickJourney(ULKJourneyPresentationSubsystem* Journey,float DeltaTime);
#endif
}
