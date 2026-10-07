#pragma once

#include "CoreMinimal.h"
#include "ALKUnitBase.h"
#include "LKTypes.h"
#include "ALKUnitBuilding.generated.h"

/** Native building behaviors. Stationary buildings, oriented walls and the mobile siege cart share one actor. */
UCLASS()
class ALKUnitBuilding : public ALKUnitBase
{
    GENERATED_BODY()
public:
    ALKUnitBuilding();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetCombatEnabled(bool bEnabled) override;
    virtual bool IsMobileBuilding() const override { return BuildingBehavior == ELKBuildingBehavior::SiegeCart; }
    virtual void OnDamageReceived(AActor* DamageInstigator, float ActualAmount, const FLKCombatSource& Source) override;

    UPROPERTY(EditDefaultsOnly, Category = "LK|Building") ELKBuildingBehavior BuildingBehavior = ELKBuildingBehavior::None;
    UPROPERTY(EditDefaultsOnly, Category = "LK|Building") FName SpawnUnitId = NAME_None;
    UPROPERTY(EditDefaultsOnly, Category = "LK|Building", meta = (ClampMin = "1.0")) float SpawnInterval = 8.f;

    bool IsWall() const { return BuildingBehavior == ELKBuildingBehavior::Wall; }
    bool IsConstructionComplete() const { return ConstructionRemaining <= 0.f; }
    float GetConstructionProgress() const { return ConstructionDuration <= 0.f ? 1.f : 1.f - ConstructionRemaining / ConstructionDuration; }
    float GetConstructionRemaining() const { return ConstructionRemaining; }
    float GetWallLength() const { return WallLength; }
    float GetWallThickness() const { return WallThickness; }
    float GetWallYaw() const { return WallYaw; }
    void SetWallYaw(float Yaw);
    float GetWallAuraRange() const { return WallAuraRange; }
    FVector GetWallFront() const;
    bool IsInWallAura(const ALKUnitBase* Unit, bool bFront) const;
    float GetFrontAttackBonus(const ALKUnitBase* Unit) const;
    ALKUnitBase* GetOccupant() const { return Occupant.Get(); }
    ALKUnitBase* GetPatient() const { return Patient.Get(); }
    bool IsPatientReady() const;
    bool IsServiceAvailable() const;
    void ReleaseClient(ALKUnitBase* Unit);
    bool FindServiceApproach(ALKUnitBase* Unit, FVector& Result) const;

protected:
    virtual void OnUnitInitialized(const FLKUnitRow& Row) override;
    virtual void UpdateStateMachine(float DeltaSeconds) override;
    virtual void PerformAttack(AActor* Target) override;
    void TrySpawnUnit();
    void TickServices(float DeltaSeconds);
    void SelectOccupant();
    void SelectPatient();
    void ReleaseAllClients();
    void TickWallAura(float DeltaSeconds);

    float SpawnTimer = 0.f;
    float ConstructionDuration = 0.f;
    float ConstructionRemaining = 0.f;
    float WallLength = 300.f;
    float WallThickness = 36.f;
    float WallYaw = 0.f;
    float ReflectDamage = 0.f;
    float WallAuraRange = 300.f;
    float AuraHealTimer = 0.f;
    float ServiceRetryTimer = 0.f;
    TWeakObjectPtr<ALKUnitBase> Occupant;
    TWeakObjectPtr<ALKUnitBase> Patient;
    TMap<TWeakObjectPtr<AActor>, float> ReflectionLastTime;
};
