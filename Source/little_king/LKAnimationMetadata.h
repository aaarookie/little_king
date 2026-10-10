#pragma once

#include "CoreMinimal.h"
#include "LKAnimationMetadata.generated.h"

/** Relative hold durations, in source keyframe order. Empty preserves the authored flipbook timing. */
USTRUCT(BlueprintType)
struct FLKAnimationFrameTiming
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation", meta=(ClampMin="0.001"))
    TArray<float> FrameWeights;
};

/** Presentation-only authoring data. Runtime components resolve a private, validated copy. */
USTRUCT(BlueprintType)
struct FLKAnimationMetadata
{
    GENERATED_BODY()
    /** Zero chooses the unit default; distance is world units per complete gait cycle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation", meta=(ClampMin="0", ClampMax="2000"))
    float StrideDistance = 0.f;
    /** Contact markers in [0,1); an empty array intentionally disables them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    TArray<float> ContactPhases = {0.f, .5f};
    /** Existing presentation SoundMap key. None keeps contact events silent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    FName FootstepSound;
    /** Contact pose for attacks with a gameplay windup; authority still comes from Impact(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation", meta=(ClampMin="0", ClampMax="63"))
    int32 AttackContactFrame = 2;
    /** Idle / Move / Attack / Hit / Death. Unknown keys do not change game rules. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    TMap<FName, FLKAnimationFrameTiming> Timing;
};

UENUM(BlueprintType)
enum class ELKAnimationPresentationEvent : uint8
{
    Began, Completed, Cancelled, FootContact
};

USTRUCT(BlueprintType)
struct FLKAnimationPresentationEvent
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Animation") int64 InstanceId = 0;
    UPROPERTY(BlueprintReadOnly, Category="Animation") FName Action;
    UPROPERTY(BlueprintReadOnly, Category="Animation") ELKAnimationPresentationEvent Type = ELKAnimationPresentationEvent::Began;
    /** Index in the resolved, sorted contact phase array; INDEX_NONE for lifecycle events. */
    UPROPERTY(BlueprintReadOnly, Category="Animation") int32 ContactIndex = INDEX_NONE;
};

namespace LKAnimation
{
    FLKAnimationMetadata DefaultsForUnit(FName UnitId, float EstimatedStride);
    FLKAnimationMetadata Resolve(const FLKAnimationMetadata& Authored, const FLKAnimationMetadata& Defaults, bool& bUsedFallback);
    /** INDEX_NONE means keep native flipbook timing. The end phase selects the final pose. */
    int32 SampleKeyFrame(double Phase, int32 KeyFrameCount, const TArray<float>& Weights);
    double PhaseAtKeyFrame(int32 Index, const TArray<float>& Weights);
    struct FWalkAdvance
    {
        double Phase = 0.;
        TArray<int32> Contacts;
    };
    /** Open on the previous endpoint, closed on the new endpoint. Large jumps never burst cues. */
    FWalkAdvance AdvanceWalk(double PreviousPhase, double Distance, double Stride,
        const TArray<float>& Contacts, bool bEmitContacts);

    /** No damage, movement, UObject or RNG dependency. IDs survive resets and are never recycled. */
    class FActionState
    {
    public:
        void Select(FName Next, bool bRestart, TArray<FLKAnimationPresentationEvent>& Events);
        void Finish(bool bCancelled, TArray<FLKAnimationPresentationEvent>& Events);
        void Reset(TArray<FLKAnimationPresentationEvent>& Events);
        void Contact(int32 Index, TArray<FLKAnimationPresentationEvent>& Events) const;
        int64 GetActiveId() const { return bActive ? InstanceId : 0; }
        FName GetAction() const { return Action; }
    private:
        FName Action;
        int64 InstanceId = 0;
        int64 Serial = 0;
        bool bActive = false;
    };
}
