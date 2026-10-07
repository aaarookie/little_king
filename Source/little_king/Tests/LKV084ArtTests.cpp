#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "PaperSprite.h"
#include "PaperFlipbook.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundConcurrency.h"
#include "../LKV084Art.h"
#include "../LKBattleArt.h"
#include "../LKPolishArt.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084ArtTest, "LittleKing.V084.ArtCatalogue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKV084ArtTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Seven building identities"), LKV084Art::UnitIds().Num(), 7);
    TestEqual(TEXT("Seven unlockable building illustrations"), LKV084Art::CardIds().Num(), 7);
    TSet<UPaperSprite*> Poses;
    for (FName Id : LKV084Art::UnitIds())
    {
        UPaperSprite* Default = LKBattleArt::Sprite(Id).LoadSynchronous();
        if (!TestNotNull(*Id.ToString(), Default)) { continue; }
        TestTrue(TEXT("Native sprite is recognised by presentation"), LKPolishArt::IsDefaultSprite(Id, Default));
        if (Id.ToString().StartsWith(TEXT("Unit_Wall")))
        { TestTrue(TEXT("Wall source footprint is exactly three metres long"), FMath::IsNearlyEqual(float(Default->GetSourceSize().X/Default->GetPixelsPerUnrealUnit()), 300.f, .01f)); }
        for (FName State : {FName("Idle"), FName("Move"), FName("Attack"), FName("Hit"), FName("Death")})
        {
            UPaperFlipbook* Clip = LKPolishArt::Animation(Id, State);
            if (!TestNotNull(*(Id.ToString()+State.ToString()), Clip)) { continue; }
            const int32 Expected = Id != "Unit_SiegeCart" ? 1 : (State == "Hit" || State == "Death" ? 2 : 4);
            TestEqual(TEXT("Fixed buildings stay fixed; cart has real motion poses"), Clip->GetNumKeyFrames(), Expected);
            TestEqual(TEXT("Presentation never supplies gameplay collision"), Clip->GetCollisionSource(), EFlipbookCollisionMode::NoCollision);
            for (int32 I=0; I<Expected; ++I)
            {
                UPaperSprite* Sprite = Clip->GetSpriteAtFrame(I);
                if (!TestNotNull(TEXT("Pose loads"), Sprite)) { continue; }
                Poses.Add(Sprite);
                const UTexture2D* Texture = Sprite->GetSourceTexture();
                if (!TestNotNull(TEXT("Source PNG imported"), Texture)) { continue; }
                const FVector2D UV=Sprite->GetSourceUV(), Size=Sprite->GetSourceSize();
                TestTrue(TEXT("Pose UV stays inside its atlas"), UV.X>=0 && UV.Y>=0 && Size.X>0 && Size.Y>0 &&
                    UV.X+Size.X<=Texture->Source.GetSizeX() && UV.Y+Size.Y<=Texture->Source.GetSizeY());
            }
        }
    }
    TestEqual(TEXT("Six fixed sprites plus sixteen cart poses"), Poses.Num(), 22);
    ULKGameData* Data = NewObject<ULKGameData>();
    Data->EnsureCardLibrary();
    TSet<UTexture2D*> Icons;
    for (FName Id : LKV084Art::CardIds())
    {
        UTexture2D* Icon = LKBattleArt::CardIcon(Id).LoadSynchronous();
        TestNotNull(*Id.ToString(), Icon); Icons.Add(Icon);
        const TObjectPtr<ULKCardDefinition>* Card = Data->CardLibrary.FindByPredicate([Id](const TObjectPtr<ULKCardDefinition>& C) { return C && C->CardId==Id; });
        if (TestNotNull(TEXT("Building has registered card"), Card))
        { TestTrue(TEXT("Card uses its matching icon"), (*Card)->Icon == LKBattleArt::CardIcon(Id)); }
    }
    TestEqual(TEXT("Seven independent card textures"), Icons.Num(), 7);
    Data->EnsurePresentationDefaults();
    for (FName Id : LKV084Art::SoundIds())
    {
        USoundWave* Wave = LoadObject<USoundWave>(nullptr, *LKV084Art::SoundPath(Id));
        if (!TestNotNull(*Id.ToString(), Wave)) { continue; }
        TestFalse(TEXT("Building cues never loop"), Wave->bLooping);
        TestEqual(TEXT("Mono cue format"), Wave->NumChannels, 1);
        TestTrue(TEXT("Short restrained building sound"), Wave->Duration>=.3f && Wave->Duration<1.5f);
        TestEqual(TEXT("Global combat voice cap reused"), Wave->ConcurrencySet.Num(), 1);
        for (USoundConcurrency* Group : Wave->ConcurrencySet)
        { TestTrue(TEXT("Same combat concurrency object"), Group && Group->GetPathName()==TEXT("/Game/Art/StorybookV1/Audio/SC_Combat.SC_Combat")); }
        const TSoftObjectPtr<USoundBase>* Entry=Data->SoundMap.Find(Id);
        TestTrue(TEXT("Native fallback registered"), Entry && Entry->ToString()==LKV084Art::SoundPath(Id));
        TestTrue(TEXT("Crowded sounds are throttled"), LKV084Art::SoundInterval(Id)>=.12f);
        Data->SoundMap.Add(Id, TSoftObjectPtr<USoundBase>());
    }
    Data->EnsurePresentationDefaults();
    for (FName Id : LKV084Art::SoundIds())
    { TestTrue(TEXT("Authored silence overrides fallback"), Data->SoundMap.FindChecked(Id).IsNull()); }
    TestTrue(TEXT("Unknown artwork has no false fallback"), LKV084Art::Sprite("Unknown").IsNull());
    TestNull(TEXT("Unknown animation rejected"), LKV084Art::Animation("Unit_SiegeCart", "Undefined"));
    TestTrue(TEXT("Unknown cue rejected"), LKV084Art::SoundPath("Unknown").IsEmpty());
    return true;
}
#endif
