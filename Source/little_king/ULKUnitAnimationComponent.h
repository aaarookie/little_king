#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LKAnimationMetadata.h"
#include "ULKUnitAnimationComponent.generated.h"
class ALKUnitBase;
class UPaperFlipbook;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLKAnimationPresentationDelegate, const FLKAnimationPresentationEvent&, Event);
DECLARE_MULTICAST_DELEGATE_OneParam(FLKAnimationPresentationNative, const FLKAnimationPresentationEvent&);
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
    void CancelAttack();
    void Hit();
    void ResetPresentation();
    bool HasAnimations() const { return Clips.Num()==5; }
    FName GetVisualState() const { return State; }
    FBoxSphereBounds GetIdleBounds() const;
    /** Stable ground contact used for ordering, independent of the current pose. */
    float GetVisualFootX() const;
    bool IsFacingRight() const { return bFacingRight; }
    float GetWalkPhase() const { return float(WalkPhase); }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") bool bOverrideAnimationMetadata=false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(EditCondition="bOverrideAnimationMetadata"))
    FLKAnimationMetadata AnimationMetadata;
    /** Apply a private validated copy and cancel effects authored against the previous metadata. */
    UFUNCTION(BlueprintCallable, Category="Animation") void SetAnimationMetadata(const FLKAnimationMetadata& Metadata);
    UFUNCTION(BlueprintCallable, Category="Animation") void UseDefaultAnimationMetadata();
    UFUNCTION(BlueprintPure, Category="Animation") int64 GetPresentationInstanceId() const { return Actions.GetActiveId(); }
    const FLKAnimationMetadata& GetResolvedAnimationMetadata() const { return ResolvedMetadata; }
    bool DidMetadataUseFallback() const { return bMetadataUsedFallback; }
    /** Transfer ownership of a newly-created effect component; never register gameplay/default subobjects. */
    UFUNCTION(BlueprintCallable, Category="Animation") bool RegisterActionComponent(int64 InstanceId, UActorComponent* Component);
    UPROPERTY(BlueprintAssignable, Category="Animation") FLKAnimationPresentationDelegate OnPresentationEvent;
    FLKAnimationPresentationNative& OnPresentationEventNative() { return PresentationEventNative; }
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UPaperFlipbook>> Clips;
    UPROPERTY(Transient) TObjectPtr<ALKUnitBase> Unit;
    FName State="Idle";
    float Clock=0.f, AttackRemaining=0.f, AttackDuration=.32f, AttackWindupDuration=.16f, HitRemaining=0.f;
    FVector LastPosition=FVector::ZeroVector;
    bool bWasDead=false;
    bool bFacingRight=true;
    bool bLastEnemy=false;
    float FootOffsetX=0.f;
    double WalkPhase=0.;
    float StrideDistance=150.f;
    float EstimatedStrideDistance=150.f;
    bool bRestartAttack=false, bRestartHit=false, bAwaitingImpact=false, bAttackImpacted=false;
    bool bMetadataUsedFallback=false;
    FLKAnimationMetadata ResolvedMetadata;
    LKAnimation::FActionState Actions;
    FLKAnimationPresentationNative PresentationEventNative;
    TMap<int64,TArray<TWeakObjectPtr<UActorComponent>>> ActionComponents;
    void ResolveMetadata();
    void Dispatch(const TArray<FLKAnimationPresentationEvent>& Events);
    void ClearActionComponents(int64 InstanceId);
    double AttackContactPhase(const UPaperFlipbook* Clip) const;
    void UpdateDepth();
    void UpdateFacing(const FVector& Travel, bool bWindDriven = false);
};
