#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LKNavigation.h"
#include "LKTypes.h"
#include "ALKBattleTerrain.generated.h"

class ALKUnitBase;
class AHUD;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ELKBattleTerrainKind : uint8
{
    Brush, Swamp, Campfire, Highland, Rock, Tree, Grass, Road, Water, Flowers
};

/** Level-authored, non-targetable terrain. All combat positions stay in the XY plane. */
UCLASS()
class ALKBattleTerrain : public AActor
{
    GENERATED_BODY()
public:
    ALKBattleTerrain();
    UFUNCTION(BlueprintCallable, Category="LK|Terrain")
    bool Initialize(ELKBattleTerrainKind Kind, FVector Center, FVector2D SizeCm, float YawDegrees=0.f);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    ELKBattleTerrainKind GetKind() const { return Kind; }
    FVector2D GetSize() const { return Size; }
    bool IsBlocking() const;
    /** Positive padding expands; negative padding requires the center to be inside by that margin. */
    bool ContainsPoint(const FVector& Point, float Padding=0.f) const;
    LKNavigation::FObstacle Shape(float Clearance=0.f) const;
    void Draw(AHUD* HUD) const;

    static bool IsLocationBlocked(UWorld* World, const FVector& Location, float Clearance=0.f, bool bAllowHighland=false);
    /** Checks the complete circle/capsule/box footprint, including rotated walls. */
    static bool IsObstacleBlocked(UWorld* World, const LKNavigation::FObstacle& Footprint);
    static ALKBattleTerrain* HighlandAt(const ALKUnitBase* Unit);
    static ALKBattleTerrain* HighlandAt(UWorld* World, const FVector& Location);
    static bool IsInSwamp(const ALKUnitBase* Unit);
    static void AppendNavigationObstacles(UWorld* World, TArray<LKNavigation::FObstacle>& Out, float Clearance=0.f);
    static bool CanSeeTarget(const ALKUnitBase* Observer, const ALKUnitBase* Target);
    static bool IsVisibleToTeam(const ALKUnitBase* Target, ELKTeam Team);
    static void NotifyHostileAction(ALKUnitBase* Attacker, const ALKUnitBase* Target);
    static bool AllowsDamage(const ALKUnitBase* Target, const ALKUnitBase* Attacker, const FLKCombatSource& Source);
private:
    bool SharesBrush(const ALKUnitBase* Unit) const;
    static void RebuildBrushConnections(UWorld* World);
    void BuildVisuals();
    ELKBattleTerrainKind Kind=ELKBattleTerrainKind::Grass;
    FVector2D Size=FVector2D(100.f,100.f);
    TWeakObjectPtr<ALKBattleTerrain> BrushGroup;
    bool bInitialized=false;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Visuals;
};
