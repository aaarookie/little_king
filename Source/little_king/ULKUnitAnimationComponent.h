#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ULKUnitAnimationComponent.generated.h"
class ALKUnitBase;
class UPaperFlipbook;
/** Samples authored frames on the existing sprite; never drives movement, damage or combat RNG. */
UCLASS()
class ULKUnitAnimationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULKUnitAnimationComponent();
    void Initialize();
    void Attack(float Windup=0.f);
    void Impact();
    void Hit();
    void ResetPresentation();
    bool HasAnimations() const { return Clips.Num()==5; }
    FName GetVisualState() const { return State; }
    FBoxSphereBounds GetIdleBounds() const;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UPaperFlipbook>> Clips;
    UPROPERTY(Transient) TObjectPtr<ALKUnitBase> Unit;
    FName State="Idle";
    float Clock=0.f, AttackRemaining=0.f, AttackDuration=.32f, HitRemaining=0.f;
    FVector LastPosition=FVector::ZeroVector;
    bool bWasDead=false;
};
