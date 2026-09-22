#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LKRunTypes.h"
#include "ULKPresentationSubsystem.generated.h"
class ALKBattleGameMode;
class AHUD;
class UPaperSpriteComponent;
class USoundBase;

enum class ELKVisualCue : uint8
{
    Slash, Impact, Fireball, Heal, HealLink, Summon, Sacrifice, Revive,
    Backstab, Coin, Mimic, Dash, Empower, HeavyHit, SiegeImpact, Command, Death
};
struct FLKVisualCue
{
    ELKVisualCue Type;
    FVector Location, Origin;
    double Started = 0;
    float Duration = .6f, Radius = 70.f;
};

/** World-owned bounded visual queue. Never changes attributes, targeting, timers or RNG. */
UCLASS()
class ULKPresentationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    static void Emit(UWorld* World, ELKVisualCue Type, FVector Location, float Radius = 70.f, FVector Origin = FVector::ZeroVector);
    static void Sound(UWorld* World, FName Id, FVector Location = FVector::ZeroVector, bool bUI = false);
    static void Fireball(UWorld* World, FVector Location, float Radius);
    void SetupBattlefield(ALKBattleGameMode* GM);
    void SetRegion(FName RegionId, ELKDungeonNodeType Type);
    void Draw(AHUD* HUD);
    void ClearEffects();
    void Prune(double Now);
    bool AdmitSound(FName Id, double Now);
    const TArray<FLKVisualCue>& GetEffects() const { return Effects; }
    UPaperSpriteComponent* GetGround() const { return Ground; }
    ELKDungeonNodeType GetBattleType() const { return BattleType; }
    static constexpr int32 MaxEffects = 128;
private:
    TArray<FLKVisualCue> Effects;
    TMap<FName, double> LastSound;
    UPROPERTY(Transient) TObjectPtr<UPaperSpriteComponent> Ground;
    UPROPERTY(Transient) TObjectPtr<UPaperSpriteComponent> Backdrop;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<class UTexture2D>> EffectTextures;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> UISounds;
    ELKDungeonNodeType BattleType = ELKDungeonNodeType::Battle;
};
