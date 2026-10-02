#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LKTypes.h"
#include "ALKSpellField.generated.h"
class ALKBattleGameMode;
class ALKUnitBase;
class AHUD;
class ULKCardDefinition;
class UTexture2D;

/** World-owned timed effects; no timer survives room/result/exit transitions. */
UCLASS()
class ALKSpellField : public AActor
{
    GENERATED_BODY()
public:
    ALKSpellField();
    bool Initialize(ALKBattleGameMode* GM, const ULKCardDefinition* Card, ELKTeam Team, FVector Center, float Scale, const FLKCombatSource& Source);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Draw(AHUD* HUD) const;
    ELKSpellEffect GetEffect() const { return Effect; }
    float GetRemaining() const { return Remaining; }
private:
    void RefreshCloud();
    void ClearCloud();
    void StrikeLowest();
    UPROPERTY(Transient) TObjectPtr<ALKBattleGameMode> GameMode;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> CloudTexture;
    ELKSpellEffect Effect = ELKSpellEffect::None;
    ELKTeam CasterTeam = ELKTeam::Player;
    FVector Center = FVector::ZeroVector;
    FVector2D HalfExtents = FVector2D::ZeroVector;
    float Radius = 0.f, Remaining = 0.f, Duration = 0.f, Speed = 0.f, SecondDamage = 0.f;
    FLKCombatSource Source;
    TSet<TWeakObjectPtr<ALKUnitBase>> HiddenUnits;
};
