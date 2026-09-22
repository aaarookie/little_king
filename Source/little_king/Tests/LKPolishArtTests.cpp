#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "GameFramework/WorldSettings.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PaperFlipbook.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundConcurrency.h"
#include "Blueprint/UserWidget.h"
#include "../LKPolishArt.h"
#include "../LKUnitContent.h"
#include "../ALKUnitBase.h"
#include "../ALKHomeBuildingActor.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ULKUnitAnimationComponent.h"
#include "../ULKUnitStatusComponent.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKJourneyPresentationSubsystem.h"
#include "../ULKRunSubsystem.h"
#include "../ULKProfileSubsystem.h"
#include "../LKGameplayHelpers.h"
#include "../ULKHomeBuildingLabelWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKPolishCatalogueTest,"LittleKing.Presentation.PolishCatalogue",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKPolishCatalogueTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("All battle entities animated"),LKPolishArt::UnitIds().Num(),28);
    TSet<UPaperSprite*> Sprites;
    for(FName Id:LKPolishArt::UnitIds())
    {
        for(FName State:{FName("Idle"),FName("Move"),FName("Attack"),FName("Hit"),FName("Death")})
        {
            auto* Clip=LKPolishArt::Animation(Id,State);
            if(!TestNotNull(*(Id.ToString()+State.ToString()),Clip)){continue;}
            const int Count=(State=="Hit"||State=="Death")?2:4;
            TestEqual(TEXT("Expected unique frame count"),Clip->GetNumKeyFrames(),Count);
            TestEqual(TEXT("Visual clips have no collision"),Clip->GetCollisionSource(),EFlipbookCollisionMode::NoCollision);
            for(int I=0;I<Count;++I)
            {
                UPaperSprite* Sprite=Clip->GetSpriteAtFrame(I);
                if(!TestNotNull(TEXT("Frame resolves"),Sprite)){continue;}
                Sprites.Add(Sprite);
                const FVector2D UV=Sprite->GetSourceUV(),Size=Sprite->GetSourceSize();
                UTexture2D* Texture=Sprite->GetSourceTexture();
                if(!TestNotNull(TEXT("Source texture"),Texture)){continue;}
                TestTrue(TEXT("Frame inside source texture"),UV.X>=0&&UV.Y>=0&&Size.X>0&&Size.Y>0&&
                    UV.X+Size.X<=Texture->Source.GetSizeX()&&UV.Y+Size.Y<=Texture->Source.GetSizeY());
                TestTrue(TEXT("Physical display scale is finite"),Sprite->GetPixelsPerUnrealUnit()>0);
                const auto Bounds=Sprite->GetRenderBounds();
                TestTrue(*(Id.ToString()+State.ToString()+TEXT(" stays near the same logical body")),
                    FMath::Abs(Bounds.Origin.X)<=Bounds.BoxExtent.X+5.f&&FMath::Abs(Bounds.Origin.Z)<1200.f);
            }
        }
    }
    TestEqual(TEXT("448 distinct authored poses"),Sprites.Num(),448);
    TestNull(TEXT("Unknown entities keep fallback"),LKPolishArt::Animation("Unknown","Idle"));
    for(bool Title:{false,true})
    {
        UFont* Font=LKPolishArt::Font(Title);
        if(!TestNotNull(TEXT("Chinese composite font"),Font)){continue;}
        const auto& Entries=Font->GetCompositeFont()->DefaultTypeface.Fonts;
        TestTrue(TEXT("Font has a default face"),Entries.Num()>0);
        for(const auto& Entry:Entries)
        {
            auto* Face=Cast<UFontFace>(Entry.Font.GetFontFaceAsset());
            if(TestNotNull(TEXT("Embedded font face"),Face))
            {TestEqual(TEXT("No external machine font dependency"),Face->LoadingPolicy,EFontLoadingPolicy::Inline);}
        }
    }
    for(const TCHAR* Id:{TEXT("Home"),TEXT("Expedition"),TEXT("Battle"),TEXT("Boss")})
    {
        auto* Wave=LoadObject<USoundWave>(nullptr,*FString::Printf(TEXT("/Game/Art/StorybookV1/Polish/Music/M_%s.M_%s"),Id,Id));
        if(!TestNotNull(Id,Wave)){continue;}
        TestTrue(TEXT("Looping soundtrack"),Wave->bLooping);
        TestTrue(TEXT("Actual loop duration"),Wave->Duration>20.f&&Wave->Duration<60.f);
        TestEqual(TEXT("Stereo music"),Wave->NumChannels,2);
        TestEqual(TEXT("Music has its own concurrency group"),Wave->ConcurrencySet.Num(),1);
        for(USoundConcurrency* Group:Wave->ConcurrencySet)
        {TestEqual(TEXT("Crossfade limited to two voices"),Group->Concurrency.MaxCount,2);}
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKPolishAnimationTest,"LittleKing.Presentation.PolishAnimationIsolation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKPolishAnimationTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Env;if(!Env.CreateTestWorld(EWorldType::Game)){return false;}
    UWorld* World=Env.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
    World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->SetAutoSaveEnabled(false);
    if(!Env.BeginPlayInTestWorld()){Env.ForwardErrorMessages(this);return false;}
    auto* GM=World->GetAuthGameMode<ALKBattleGameMode>();
    World->SpawnActor<ALKPlayerController>();GM->Tick(0.f);
    for(int I=0;I<GM->AvailableHeroes.Num();++I){GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700,-1300,0));}
    GM->ForceStartBattle();
    if(!TestEqual(TEXT("Fixture battle starts"),GM->GetPhase(),ELKGamePhase::Battle)){return false;}
    ALKUnitBase* Unit=GM->SpawnUnitForTeam("Hero_Necromancer",ELKTeam::Player,FVector(700,-800,0));
    if(!TestNotNull(TEXT("Animated test hero"),Unit)){return false;}
    auto* Animation=Unit->GetAnimationComponent();
    TestTrue(TEXT("Default sprite uses animation"),Animation->HasAnimations());
    Unit->SetCombatEnabled(true);
    const float Health=Unit->GetHealth(),Radius=Unit->GetBodyRadius();
    const FVector Position=Unit->GetActorLocation();
    const int Seed=GM->GetBattleRandom().GetCurrentSeed();
    Animation->Attack(.25f);Animation->TickComponent(.05f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Attack event selects attack clip"),Animation->GetVisualState(),FName("Attack"));
    Animation->Hit();Animation->TickComponent(.05f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Impact reaction takes visual priority"),Animation->GetVisualState(),FName("Hit"));
    for(int I=0;I<30;++I){Animation->TickComponent(.05f,LEVELTICK_All,nullptr);}
    TestEqual(TEXT("Animation never changes health"),Unit->GetHealth(),Health);
    TestEqual(TEXT("Animation never moves logic body"),Unit->GetActorLocation(),Position);
    TestEqual(TEXT("Animation never changes collider"),Unit->GetBodyRadius(),Radius);
    TestEqual(TEXT("Animation never draws combat RNG"),GM->GetBattleRandom().GetCurrentSeed(),Seed);
    Unit->GetStatusComponent()->Freeze(2.f);
    Animation->Attack();Animation->TickComponent(.05f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Controlled unit cannot show attack"),Animation->GetVisualState(),FName("Idle"));
    auto* FrozenSprite=Unit->GetSpriteComponent()->GetSprite();
    Animation->TickComponent(.5f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Frozen pose holds still"),Unit->GetSpriteComponent()->GetSprite(),FrozenSprite);
    Unit->GetStatusComponent()->Clear();
    Unit->GetMovementComponent()->MoveToward(Position+FVector(150,0,0),100.f);
    Unit->GetMovementComponent()->TickComponent(.1f,LEVELTICK_All,nullptr);
    Animation->TickComponent(.05f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Actual body movement selects walk clip"),Animation->GetVisualState(),FName("Move"));
    Unit->GetMovementComponent()->Stop();Animation->TickComponent(.05f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Stopped body returns to idle"),Animation->GetVisualState(),FName("Idle"));
    LKGameplay::ApplyDamage(Unit,Health*10,nullptr,true);
    Animation->TickComponent(.2f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Hero incapacity uses death clip"),Animation->GetVisualState(),FName("Death"));
    Animation->TickComponent(.2f,LEVELTICK_All,nullptr);
    Animation->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Incapacity holds final pose"),Unit->GetSpriteComponent()->GetSprite(),LKPolishArt::Animation("Hero_Necromancer","Death")->GetSpriteAtFrame(1));
    Unit->Tick(.4f);
    TestEqual(TEXT("Hero fallen pose stays visible after its old fade timer"),Unit->GetSpriteComponent()->GetSpriteColor().A,1.f);
    TestTrue(TEXT("Incapacitated hero is visually dimmed"),Unit->GetSpriteComponent()->GetSpriteColor().R<1.f);
    TestTrue(TEXT("Existing hero revive remains usable"),Unit->ReviveDuringBattle());
    Animation->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Revive restores idle"),Animation->GetVisualState(),FName("Idle"));
    Unit->GetSpriteComponent()->SetSprite(LoadObject<UPaperSprite>(nullptr,TEXT("/Game/Sprites/Unit_Swordsman_sprite.Unit_Swordsman_sprite")));
    auto* Custom=Unit->GetSpriteComponent()->GetSprite(); Animation->Initialize();
    TestFalse(TEXT("Custom illustration is not overwritten"),Animation->HasAnimations());
    Animation->Attack();Animation->TickComponent(.5f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Fallback preserves artist sprite"),Unit->GetSpriteComponent()->GetSprite(),Custom);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKPolishHomeTest,"LittleKing.Presentation.PolishHomeLevelGeometry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKPolishHomeTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Env;Env.CreateTestWorld(EWorldType::Game);
    Env.GetTestWorld()->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    if(!Env.BeginPlayInTestWorld()){return false;}
    for(FName Id:{FName("Home_StatueSaintMaria"),FName("Home_Treasury")})
    {
        auto* Actor=Env.GetTestWorld()->SpawnActor<ALKHomeBuildingActor>();Actor->InitializeBuilding(Id,FText::FromName(Id),true);
        auto* Sprite=Actor->FindComponentByClass<UPaperSpriteComponent>();auto* LevelOne=Sprite->GetSprite();
        const int Cap=Id=="Home_Treasury"?5:4;
        for(int Level=2;Level<=Cap;++Level)
        {
            Actor->SetHighlighted(true);Actor->SetDisplayedLevel(Level);
            TestEqual(TEXT("Visual follows actual displayed level"),Sprite->GetSprite(),LKPolishArt::HomeLevel(Id,Level));
            TestTrue(TEXT("Hover survives upgrade"),Sprite->GetRelativeScale3D().X>1.);
            TestEqual(TEXT("Upgrade never changes click radius"),Actor->HitRadius,300.f);
            TestEqual(TEXT("Actor size unchanged"),Actor->GetActorScale3D(),FVector::OneVector);
            TestEqual(TEXT("Illustration never gains collision"),Sprite->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
        }
        TestEqual(TEXT("Future levels safely use highest current art"),LKPolishArt::HomeLevel(Id,99),LKPolishArt::HomeLevel(Id,Cap));
        Actor->SetDisplayedLevel(1);TestEqual(TEXT("Level one retains original art"),Sprite->GetSprite(),LevelOne);
    }
    TestNull(TEXT("Other buildings unchanged"),LKPolishArt::HomeLevel("Home_Library",5));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKPolishJourneyTest,"LittleKing.Presentation.PolishMusicAndRevealLifetime",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLKPolishJourneyTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Env;Env.CreateTestWorld(EWorldType::Game);
    Env.GetTestWorld()->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    if(!Env.BeginPlayInTestWorld()){return false;}
    auto* Journey=Env.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKJourneyPresentationSubsystem>();
    if(!TestNotNull(TEXT("Instance lifetime presentation exists"),Journey)){return false;}
    Journey->SetMusicVolume(3.f);TestEqual(TEXT("Volume upper clamp"),Journey->GetMusicVolume(),1.f);
    Journey->SetMusicVolume(-1.f);TestEqual(TEXT("Volume lower clamp"),Journey->GetMusicVolume(),0.f);
    Journey->SetMusicVolume(.25f);
    for(int I=0;I<12;++I)
    {Journey->SetMusic(I%2?FName("Battle"):FName("Home"));Journey->Tick(.02f);TestTrue(TEXT("Rapid switches never stack more than two tracks"),Journey->GetMusicVoiceCount()<=2);}
    const FName Scene=Journey->GetMusicScene();Journey->SetMusic("Invalid");TestEqual(TEXT("Invalid music key ignored"),Journey->GetMusicScene(),Scene);
    Journey->Tick(1.3f);TestTrue(TEXT("Old voice retired after blend"),Journey->GetMusicVoiceCount()<=1);
    auto* Widget=CreateWidget<ULKHomeBuildingLabelWidget>(Env.GetTestWorld(),ULKHomeBuildingLabelWidget::StaticClass());
    if(Widget)
    {
        Widget->SetRenderOpacity(.8f);Widget->SetRenderTranslation(FVector2D(3,5));
        ULKJourneyPresentationSubsystem::Reveal(Widget);ULKJourneyPresentationSubsystem::Reveal(Widget);
        Journey->Tick(.2f);
        TestEqual(TEXT("Reveal preserves authored opacity"),Widget->GetRenderOpacity(),.8f);
        TestEqual(TEXT("Reveal restores authored position once"),Widget->GetRenderTransform().Translation,FVector2D(3,5));
    }
    return true;
}
#endif
