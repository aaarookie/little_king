#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "ULKJourneyPresentationSubsystem.generated.h"
class UAudioComponent;
class USoundBase;
class UFont;
class UUserWidget;
class SBorder;

/** Game-instance lifetime: music survives travel, while the viewport curtain hides world replacement. */
UCLASS()
class ULKJourneyPresentationSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return !IsTemplate() && bInitialized; }
    virtual UWorld* GetTickableGameObjectWorld() const override;
    static void Travel(const UObject* Context, FName Map, const FText& Caption = FText());
    static void Reveal(UUserWidget* Widget);
    void SetMusic(FName Scene);
    UFUNCTION(BlueprintCallable, Category="LK|Presentation") void SetMusicVolume(float Volume);
    UFUNCTION(BlueprintPure, Category="LK|Presentation") float GetMusicVolume() const { return MusicVolume; }
    FName GetMusicScene() const { return MusicScene; }
    bool IsTravelling() const { return TravelState != 0; }
    int32 GetMusicVoiceCount() const;
private:
    void BeginTravel(FName Map, const FText& Caption);
    void OnMapLoaded(UWorld* World);
    void RemoveCurtain();
    void UpdateMusicScene();
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> CurrentMusic;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> PreviousMusic;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<USoundBase>> Tracks;
    // Slate holds weak font objects; retain both across OpenLevel's garbage collection.
    UPROPERTY(Transient) TObjectPtr<UFont> BodyFont;
    UPROPERTY(Transient) TObjectPtr<UFont> TitleFont;
    struct FReveal { TWeakObjectPtr<UUserWidget> Widget; float Time=0.f; float Opacity=1.f; FVector2D Translation; };
    TArray<FReveal> Reveals;
    TSharedPtr<SBorder> Curtain;
    FDelegateHandle MapLoadedHandle;
    FName MusicScene, PendingMap;
    float MusicVolume=.25f, MusicBlend=1.f, ScenePoll=0.f, TravelTime=0.f;
    float PreviousMusicWeight=0.f;
    int32 TravelState=0; // 0 idle, 1 fade out, 2 loading, 3 fade in
    bool bInitialized=false;
};
