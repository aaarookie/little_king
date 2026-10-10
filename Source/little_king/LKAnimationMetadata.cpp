#include "LKAnimationMetadata.h"

namespace
{
bool ValidStride(float Value) { return FMath::IsFinite(Value) && Value >= 10.f && Value <= 2000.f; }
bool ValidWeights(const TArray<float>& Values)
{
    if (Values.Num() > 64) { return false; }
    double Sum = 0.;
    for (float Value : Values)
    {
        if (!FMath::IsFinite(Value) || Value <= 0.f || Value > 10000.f) { return false; }
        Sum += Value;
    }
    return FMath::IsFinite(Sum);
}
bool ValidContacts(const TArray<float>& Values)
{
    if (Values.Num() > 8) { return false; }
    for (float Value : Values)
    { if (!FMath::IsFinite(Value) || Value < 0.f || Value >= 1.f) { return false; } }
    return true;
}
FLKAnimationPresentationEvent Event(int64 Id, FName Action, ELKAnimationPresentationEvent Type, int32 Index=INDEX_NONE)
{
    FLKAnimationPresentationEvent Result;
    Result.InstanceId=Id; Result.Action=Action; Result.Type=Type; Result.ContactIndex=Index;
    return Result;
}
}

FLKAnimationMetadata LKAnimation::DefaultsForUnit(FName UnitId, float EstimatedStride)
{
    FLKAnimationMetadata Result;
    Result.StrideDistance=ValidStride(EstimatedStride) ? EstimatedStride : 150.f;
    // Conservative authoring samples; other units retain their previous bounds-derived stride.
    if (UnitId=="Unit_Swordsman")
    {
        Result.StrideDistance=150.f;
        Result.Timing.FindOrAdd("Attack").FrameWeights={1.4f,1.2f,.6f,.8f};
    }
    else if (UnitId=="Unit_Colossus")
    {
        Result.StrideDistance=240.f;
        Result.FootstepSound="Footstep";
        Result.Timing.FindOrAdd("Move").FrameWeights={1.2f,.8f,1.2f,.8f};
        Result.Timing.FindOrAdd("Attack").FrameWeights={1.5f,1.1f,.6f,.8f};
    }
    else if (UnitId=="Hero_Mage")
    {
        Result.StrideDistance=140.f;
        Result.Timing.FindOrAdd("Attack").FrameWeights={1.2f,1.4f,.6f,.8f};
    }
    return Result;
}

FLKAnimationMetadata LKAnimation::Resolve(const FLKAnimationMetadata& Authored, const FLKAnimationMetadata& Defaults, bool& bUsedFallback)
{
    FLKAnimationMetadata Result=Authored;
    bUsedFallback=false;
    if (!ValidStride(Result.StrideDistance))
    {
        // Zero is an intentional inheritance request, not a corrupt value.
        bUsedFallback=Result.StrideDistance!=0.f;
        Result.StrideDistance=ValidStride(Defaults.StrideDistance) ? Defaults.StrideDistance : 150.f;
    }
    if (!ValidContacts(Result.ContactPhases))
    {
        Result.ContactPhases=ValidContacts(Defaults.ContactPhases) ? Defaults.ContactPhases : TArray<float>{0.f,.5f};
        bUsedFallback=true;
    }
    Result.ContactPhases.Sort();
    if (Result.AttackContactFrame<0 || Result.AttackContactFrame>63)
    {
        Result.AttackContactFrame=FMath::Clamp(Defaults.AttackContactFrame,0,63);
        bUsedFallback=true;
    }
    for (int32 I=Result.ContactPhases.Num()-1; I>0; --I)
    { if (Result.ContactPhases[I]==Result.ContactPhases[I-1]) { Result.ContactPhases.RemoveAt(I); } }
    for (auto& Pair : Result.Timing)
    {
        if (!ValidWeights(Pair.Value.FrameWeights))
        {
            const auto* Fallback=Defaults.Timing.Find(Pair.Key);
            Pair.Value.FrameWeights=Fallback && ValidWeights(Fallback->FrameWeights) ? Fallback->FrameWeights : TArray<float>{};
            bUsedFallback=true;
        }
    }
    return Result;
}

int32 LKAnimation::SampleKeyFrame(double Phase, int32 KeyFrameCount, const TArray<float>& Weights)
{
    if (KeyFrameCount<=0 || Weights.Num()!=KeyFrameCount || Weights.IsEmpty() || !ValidWeights(Weights) || !FMath::IsFinite(Phase))
    { return INDEX_NONE; }
    double Sum=0.;
    for (float Weight : Weights) { Sum+=Weight; }
    const double Position=FMath::Clamp(Phase,0.,1.);
    double End=0.;
    for (int32 I=0; I<Weights.Num(); ++I)
    {
        End+=Weights[I];
        if (Position<End/Sum) { return I; }
    }
    return Weights.Num()-1;
}

double LKAnimation::PhaseAtKeyFrame(int32 Index, const TArray<float>& Weights)
{
    if (Weights.IsEmpty() || !ValidWeights(Weights)) { return 0.; }
    Index=FMath::Clamp(Index,0,Weights.Num()-1);
    double Before=0.,Total=0.;
    for (int32 I=0; I<Weights.Num(); ++I)
    {
        Total+=Weights[I];
        if (I<Index) { Before+=Weights[I]; }
    }
    return Before/Total;
}

LKAnimation::FWalkAdvance LKAnimation::AdvanceWalk(double PreviousPhase, double Distance, double Stride,
    const TArray<float>& Contacts, bool bEmitContacts)
{
    FWalkAdvance Result;
    Result.Phase=FMath::IsFinite(PreviousPhase) ? FMath::Frac(FMath::Max(0.,PreviousPhase)) : 0.;
    if (!FMath::IsFinite(Distance) || Distance<=0. || !FMath::IsFinite(Stride) || Stride<=0.) { return Result; }
    const double Delta=Distance/Stride;
    if (!FMath::IsFinite(Delta)) { return Result; }
    const double Start=Result.Phase;
    Result.Phase=FMath::Frac(Start+Delta);
    // Teleports, a long hitch, wind and skill displacement may change the pose but must not replay footsteps.
    if (!bEmitContacts || Delta>1. || !ValidContacts(Contacts)) { return Result; }
    TArray<TPair<double,int32>> Crossed;
    for (int32 I=0; I<Contacts.Num(); ++I)
    {
        const double Marker=Contacts[I];
        const double Offset=Marker>Start ? Marker-Start : 1.+Marker-Start;
        if (Offset<=Delta) { Crossed.Emplace(Offset,I); }
    }
    Crossed.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});
    for (const auto& Pair : Crossed) { Result.Contacts.Add(Pair.Value); }
    return Result;
}

void LKAnimation::FActionState::Select(FName Next, bool bRestart, TArray<FLKAnimationPresentationEvent>& Events)
{
    if (Next==Action && !bRestart) { return; }
    Finish(true,Events);
    Action=Next;
    InstanceId=++Serial;
    bActive=true;
    Events.Add(Event(InstanceId,Action,ELKAnimationPresentationEvent::Began));
}
void LKAnimation::FActionState::Finish(bool bCancelled, TArray<FLKAnimationPresentationEvent>& Events)
{
    if (!bActive) { return; }
    bActive=false;
    Events.Add(Event(InstanceId,Action,bCancelled ? ELKAnimationPresentationEvent::Cancelled : ELKAnimationPresentationEvent::Completed));
}
void LKAnimation::FActionState::Reset(TArray<FLKAnimationPresentationEvent>& Events)
{
    Finish(true,Events);
    Action=NAME_None;
}
void LKAnimation::FActionState::Contact(int32 Index, TArray<FLKAnimationPresentationEvent>& Events) const
{
    if (bActive && Action=="Move" && Index>=0)
    { Events.Add(Event(InstanceId,Action,ELKAnimationPresentationEvent::FootContact,Index)); }
}
