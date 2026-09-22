#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundConcurrency.h"
#include "../ALKHomeBuildingActor.h"
#include "../LKHomeContent.h"
#include "../ULKGameData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKPresentationAudioTest, "LittleKing.Presentation.AssetsAndAudioDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKPresentationAudioTest::RunTest(const FString& Parameters)
{
    TestNotNull(TEXT("Menu illustration resolves"), LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Art/StorybookV1/Textures/T_MenuKingdom.T_MenuKingdom")));
    ULKGameData* Data = NewObject<ULKGameData>();
    Data->EnsurePresentationDefaults();
    TestEqual(TEXT("Eight base and 28 specialised cues have defaults"), Data->SoundMap.Num(), 36);
    for (const auto& Pair : Data->SoundMap)
    {
        USoundWave* Sound = Cast<USoundWave>(Pair.Value.LoadSynchronous());
        if (!TestNotNull(*Pair.Key.ToString(), Sound)) { continue; }
        TestTrue(TEXT("Finite short sound"), Sound->Duration > 0 && Sound->Duration <= 3.f);
        TestFalse(TEXT("One-shot cannot accidentally loop"), Sound->bLooping);
        TestEqual(TEXT("Mono gameplay sound"), Sound->NumChannels, 1);
        TestEqual(TEXT("Shared concurrency is assigned"), Sound->ConcurrencySet.Num(), 1);
    }
    USoundWave* Click = LoadObject<USoundWave>(nullptr, TEXT("/Game/Art/StorybookV1/Audio/S_UIClick.S_UIClick"));
    TestNotNull(TEXT("UI press sound resolves"), Click);
    Data->SoundMap[TEXT("MeleeHit")] = nullptr;
    Data->EnsurePresentationDefaults();
    TestTrue(TEXT("Explicit silent key remains silent"), Data->SoundMap[TEXT("MeleeHit")].IsNull());
    Data->SoundMap.Reset(); Data->bUseDefaultSoundSet = false; Data->EnsurePresentationDefaults();
    TestEqual(TEXT("Default set can be disabled"), Data->SoundMap.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKPresentationBuildingsTest, "LittleKing.Presentation.HomeIllustrations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKPresentationBuildingsTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper World; World.CreateTestWorld(EWorldType::Game);
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
    for (const FLKBuildingDefinition& Def : LKHomeContent::Buildings())
    {
        ALKHomeBuildingActor* Actor = World.GetTestWorld()->SpawnActor<ALKHomeBuildingActor>();
        Actor->InitializeBuilding(Def.BuildingId, Def.DisplayName, Def.bUpgradable);
        TestTrue(*Def.BuildingId.ToString(), Actor->HasIllustration());
        TestEqual(TEXT("Stable gameplay identity"), Actor->GetBuildingId(), Def.BuildingId);
        TestEqual(TEXT("Click radius unchanged"), Actor->HitRadius, 300.f);
        UPaperSpriteComponent* Sprite = Actor->FindComponentByClass<UPaperSpriteComponent>();
        if (TestNotNull(TEXT("Paper2D component exists"), Sprite) && Sprite->GetSprite())
        {
            const FBoxSphereBounds Bounds = Sprite->CalcBounds(Sprite->GetComponentTransform());
            TestTrue(TEXT("Baked sprite has non-empty world geometry"), Bounds.BoxExtent.X > 100 && Bounds.BoxExtent.Y > 100);
            TestTrue(TEXT("Illustration cannot change click collision"), Sprite->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
            Actor->SetHighlighted(true);
            TestTrue(TEXT("Hover feedback applied"), Sprite->GetRelativeScale3D().X > 1.f);
            Actor->SetHighlighted(false);
            TestEqual(TEXT("Hover scale restored"), Sprite->GetRelativeScale3D().X, 1.0);
        }
        Actor->Destroy();
    }
    return true;
}
#endif
