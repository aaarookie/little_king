#include "ULKJourneyPresentationSubsystem.h"
#include "ALKStartMenuGameMode.h"
#include "ALKHomeGameMode.h"
#include "ALKBattleGameMode.h"
#include "ULKRunSubsystem.h"
#include "LKPresentationStyle.h"
#include "LKPolishArtPreview.h"
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void ULKJourneyPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); bInitialized=true;
    BodyFont=LKPolishArt::Font(false);TitleFont=LKPolishArt::Font(true);
    MapLoadedHandle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,&ThisClass::OnMapLoaded);
}
void ULKJourneyPresentationSubsystem::Deinitialize()
{
    bInitialized=false;
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
    for(UAudioComponent* Voice:{CurrentMusic.Get(),PreviousMusic.Get()})
    { if(IsValid(Voice)){Voice->Stop();Voice->DestroyComponent();} }
    CurrentMusic=nullptr;PreviousMusic=nullptr;Tracks.Reset(); Reveals.Reset(); RemoveCurtain();
    BodyFont=nullptr;TitleFont=nullptr;
    Super::Deinitialize();
}
TStatId ULKJourneyPresentationSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(LKJourneyPresentation,STATGROUP_Tickables); }
UWorld* ULKJourneyPresentationSubsystem::GetTickableGameObjectWorld() const { return GetWorld(); }
int32 ULKJourneyPresentationSubsystem::GetMusicVoiceCount() const
{ return int32(IsValid(CurrentMusic)&&CurrentMusic->IsPlaying())+int32(IsValid(PreviousMusic)&&PreviousMusic->IsPlaying()); }
void ULKJourneyPresentationSubsystem::SetMusicVolume(float Volume)
{ if(FMath::IsFinite(Volume)){MusicVolume=FMath::Clamp(Volume,0.f,1.f);} }
void ULKJourneyPresentationSubsystem::SetMusic(FName Scene)
{
    if(Scene==MusicScene||!GetWorld()||!GetWorld()->IsGameWorld()){return;}
    if(Scene!="Home"&&Scene!="Expedition"&&Scene!="Battle"&&Scene!="Boss"){return;}
    USoundBase* Track=Tracks.FindRef(Scene);
    if(!Track)
    {
        const FString Name=TEXT("M_")+Scene.ToString();
        Track=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Art/StorybookV1/Polish/Music/%s.%s"),*Name,*Name),nullptr,LOAD_NoWarn);
        if(!Track){return;} Tracks.Add(Scene,Track);
    }
    // Always retire the oldest voice before allocating: rapid UI/travel changes cannot stack tracks.
    if(IsValid(PreviousMusic)){PreviousMusic->Stop();PreviousMusic->DestroyComponent();}
    PreviousMusic=CurrentMusic;
    PreviousMusicWeight=MusicBlend;
    CurrentMusic=UGameplayStatics::CreateSound2D(GetWorld(),Track,1.f,1.f,0.f,nullptr,true,false);
    if(CurrentMusic){CurrentMusic->SetVolumeMultiplier(0.f);CurrentMusic->Play();}
    MusicScene=Scene; MusicBlend=0.f;
}
void ULKJourneyPresentationSubsystem::UpdateMusicScene()
{
    UWorld* World=GetWorld();if(!World||!World->IsGameWorld()){return;}
    AGameModeBase* Mode=World->GetAuthGameMode();
    if(Cast<ALKHomeGameMode>(Mode)||Cast<ALKStartMenuGameMode>(Mode)){SetMusic("Home");return;}
    if(ALKBattleGameMode* Battle=Cast<ALKBattleGameMode>(Mode))
    {
        ULKRunSubsystem* Run=GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
        const bool bBetweenRooms=Run && (Run->GetRunPhase()==ELKRunPhase::ChoosingNode||Run->HasServiceNode()||Run->HasPendingRewardChoice()||Run->IsTerminal());
        if(bBetweenRooms){SetMusic("Expedition");return;}
        const bool bBoss=Battle->GetCurrentEncounter().Rank==ELKEncounterRank::Boss
            ||(Run&&Run->GetNode(Run->GetRunState().CurrentNodeId).Type==ELKDungeonNodeType::Boss);
        SetMusic(bBoss?"Boss":"Battle");
    }
}
void ULKJourneyPresentationSubsystem::Reveal(UUserWidget* Widget)
{
    if(!Widget||!Widget->GetGameInstance()){return;}
    auto* Self=Widget->GetGameInstance()->GetSubsystem<ULKJourneyPresentationSubsystem>();
    if(!Self||Self->Reveals.ContainsByPredicate([Widget](const FReveal& R){return R.Widget.Get()==Widget;})){return;}
    FReveal Entry;Entry.Widget=Widget;Entry.Opacity=Widget->GetRenderOpacity();Entry.Translation=Widget->GetRenderTransform().Translation;
    Self->Reveals.Add(Entry);Widget->SetRenderOpacity(0.f);Widget->SetRenderTranslation(Entry.Translation+FVector2D(0,8));
}
void ULKJourneyPresentationSubsystem::Travel(const UObject* Context,FName Map,const FText& Caption)
{
    UGameInstance* Instance=UGameplayStatics::GetGameInstance(Context);
    if(Instance)
    { if(auto* Self=Instance->GetSubsystem<ULKJourneyPresentationSubsystem>()){Self->BeginTravel(Map,Caption);return;} }
    UGameplayStatics::OpenLevel(Context,Map);
}
void ULKJourneyPresentationSubsystem::BeginTravel(FName Map,const FText& Caption)
{
    if(IsTravelling()||Map.IsNone()){return;}
    UGameViewportClient* Viewport=GetGameInstance()->GetGameViewportClient();
    if(!Viewport){UGameplayStatics::OpenLevel(GetWorld(),Map);return;}
    PendingMap=Map;TravelState=1;TravelTime=0.f;
    SAssignNew(Curtain,SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(LKPresentationStyle::Ink())
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        .OnMouseButtonDown_Lambda([](const FGeometry&,const FPointerEvent&){return FReply::Handled();})
        [SNew(STextBlock).Text(Caption.IsEmpty()?FText::FromString(TEXT("踏上新的旅程…")):Caption)
            .Font(LKPresentationStyle::Font(28,true)).ColorAndOpacity(LKPresentationStyle::Gold())];
    Curtain->SetRenderOpacity(0.f);Viewport->AddViewportWidgetContent(Curtain.ToSharedRef(),10000);
}
void ULKJourneyPresentationSubsystem::OnMapLoaded(UWorld* World)
{
    if(World&&World->GetGameInstance()==GetGameInstance()&&TravelState==2)
    { TravelState=3;TravelTime=0.f;ScenePoll=0.f; }
}
void ULKJourneyPresentationSubsystem::RemoveCurtain()
{
    if(Curtain.IsValid()&&GetGameInstance()&&GetGameInstance()->GetGameViewportClient())
    {GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(Curtain.ToSharedRef());}
    Curtain.Reset();TravelState=0;PendingMap=NAME_None;
}
void ULKJourneyPresentationSubsystem::Tick(float DeltaTime)
{
#if WITH_EDITOR
    LKPolishArtPreview::TickJourney(this,DeltaTime);
#endif
    ScenePoll-=DeltaTime;
    if(ScenePoll<=0.f){ScenePoll=.25f;UpdateMusicScene();}
    MusicBlend=FMath::Min(1.f,MusicBlend+DeltaTime/1.2f);
    if(IsValid(CurrentMusic)){CurrentMusic->SetVolumeMultiplier(MusicVolume*MusicBlend);}
    if(IsValid(PreviousMusic))
    {
        PreviousMusic->SetVolumeMultiplier(MusicVolume*PreviousMusicWeight*(1.f-MusicBlend));
        if(MusicBlend>=1.f){PreviousMusic->Stop();PreviousMusic->DestroyComponent();PreviousMusic=nullptr;}
    }
    for(int32 Index=Reveals.Num()-1;Index>=0;--Index)
    {
        FReveal& Entry=Reveals[Index];UUserWidget* Widget=Entry.Widget.Get();
        if(!Widget){Reveals.RemoveAtSwap(Index);continue;}
        Entry.Time+=DeltaTime;const float T=FMath::Clamp(Entry.Time/.18f,0.f,1.f);
        const float Ease=1.f-FMath::Square(1.f-T);
        Widget->SetRenderOpacity(Entry.Opacity*Ease);Widget->SetRenderTranslation(Entry.Translation+FVector2D(0,8.f*(1.f-Ease)));
        if(T>=1.f){Reveals.RemoveAtSwap(Index);}
    }
    if(TravelState==0){return;}TravelTime+=DeltaTime;
    if(TravelState==1)
    {
        if(Curtain){Curtain->SetRenderOpacity(FMath::Min(1.f,TravelTime/.2f));}
        if(TravelTime>=.2f){TravelState=2;TravelTime=0.f;UGameplayStatics::OpenLevel(GetWorld(),PendingMap);}
    }
    else if(TravelState==3)
    {
        if(Curtain){Curtain->SetRenderOpacity(1.f-FMath::Min(1.f,TravelTime/.3f));}
        if(TravelTime>=.3f){RemoveCurtain();}
    }
    else if(TravelTime>20.f){RemoveCurtain();} // Failed travel must never leave an opaque, blocking viewport.
}
