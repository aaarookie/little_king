#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "PaperSprite.h"
#include "PaperFlipbook.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundConcurrency.h"
#include "../LKV083Art.h"
#include "../LKBattleArt.h"
#include "../LKPolishArt.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083ArtTest, "LittleKing.V083.ArtCatalogue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKV083ArtTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Eleven summon identities"), LKV083Art::UnitIds().Num(), 11);
    TestEqual(TEXT("Nine unlockable spell illustrations"), LKV083Art::SpellIds().Num(), 9);
    TSet<FName> Roles;
    TSet<UPaperSprite*> Poses;
    for (FName UnitId : LKV083Art::UnitIds())
    {
        Roles.Add(LKV083Art::Identity(UnitId));
        UPaperSprite* Default = LKBattleArt::Sprite(UnitId).LoadSynchronous();
        if (!TestNotNull(*UnitId.ToString(), Default)) { continue; }
        TestTrue(TEXT("Native animated sprite is recognised"), LKPolishArt::IsDefaultSprite(UnitId, Default));
        TestTrue(TEXT("Summoned angels do not gain hand card artwork"), LKBattleArt::CardIcon(UnitId).IsNull());
        for (FName State : {FName("Idle"), FName("Move"), FName("Attack"), FName("Hit"), FName("Death")})
        {
            UPaperFlipbook* Clip = LKPolishArt::Animation(UnitId, State);
            if (!TestNotNull(*(UnitId.ToString()+TEXT(" ")+State.ToString()), Clip)) { continue; }
            TestTrue(TEXT("New atlases own movement and do not accidentally use the historical correction set"), Clip->GetPathName().StartsWith(TEXT("/Game/Art/StorybookV1/V083/Animations/")));
            const int32 Expected = State == "Hit" || State == "Death" ? 2 : 4;
            TestEqual(TEXT("Five supported states have real authored keyframes"), Clip->GetNumKeyFrames(), Expected);
            TestEqual(TEXT("Visual animation has no gameplay collision"), Clip->GetCollisionSource(), EFlipbookCollisionMode::NoCollision);
            for (int32 Frame = 0; Frame < Expected; ++Frame)
            {
                UPaperSprite* Sprite = Clip->GetSpriteAtFrame(Frame);
                if (!TestNotNull(TEXT("Frame exists"), Sprite)) { continue; }
                Poses.Add(Sprite);
                TestTrue(TEXT("Positive finite pixels per unit"), FMath::IsFinite(Sprite->GetPixelsPerUnrealUnit()) && Sprite->GetPixelsPerUnrealUnit() > 0.f);
                UTexture2D* Texture = Sprite->GetSourceTexture();
                if (!TestNotNull(TEXT("Atlas texture exists"), Texture)) { continue; }
                const FVector2D UV = Sprite->GetSourceUV(), Size = Sprite->GetSourceSize();
                TestTrue(TEXT("Frame remains within source texture"), UV.X >= 0 && UV.Y >= 0 && Size.X > 0 && Size.Y > 0 &&
                    UV.X+Size.X <= Texture->Source.GetSizeX() && UV.Y+Size.Y <= Texture->Source.GetSizeY());
            }
        }
    }
    TestEqual(TEXT("Three angel tiers share five role identities"), Roles.Num(), 5);
    TestEqual(TEXT("Eighty authored angel poses"), Poses.Num(), 80);
    ULKGameData* Data = NewObject<ULKGameData>();
    Data->EnsureCardLibrary();
    TSet<UTexture2D*> Icons;
    for (FName Id : LKV083Art::SpellIds())
    {
        UTexture2D* Icon = LKBattleArt::CardIcon(Id).LoadSynchronous();
        Icons.Add(Icon);
        TestNotNull(*Id.ToString(), Icon);
        const TObjectPtr<ULKCardDefinition>* Card = Data->CardLibrary.FindByPredicate([Id](const TObjectPtr<ULKCardDefinition>& C) { return C && C->CardId == Id; });
        if (TestNotNull(TEXT("Spell has a registered card"), Card))
        { TestTrue(TEXT("Native default points to its distinct illustration"), (*Card)->Icon == LKBattleArt::CardIcon(Id)); }
    }
    TestEqual(TEXT("Nine separate card pictures"), Icons.Num(), 9);
    Data->EnsurePresentationDefaults();
    for (FName Id : LKV083Art::SoundIds())
    {
        USoundWave* Wave = LoadObject<USoundWave>(nullptr, *LKV083Art::SoundPath(Id));
        if (!TestNotNull(*Id.ToString(), Wave)) { continue; }
        TestFalse(TEXT("Cast cues never loop continuously"), Wave->bLooping);
        TestEqual(TEXT("Consistent mono combat cues"), Wave->NumChannels, 1);
        TestTrue(TEXT("Short gentle spell cue"), Wave->Duration > .5f && Wave->Duration < 2.f);
        TestEqual(TEXT("Spell cues share the existing total combat voice cap"), Wave->ConcurrencySet.Num(), 1);
        for (USoundConcurrency* Group : Wave->ConcurrencySet)
        { TestTrue(TEXT("Shared cap controls crowded casting"), Group && Group->GetPathName() == TEXT("/Game/Art/StorybookV1/Audio/SC_Combat.SC_Combat")); }
        const TSoftObjectPtr<USoundBase>* Entry = Data->SoundMap.Find(Id);
        TestTrue(TEXT("GameData has a native cue fallback"), Entry && Entry->ToString() == LKV083Art::SoundPath(Id));
        TestTrue(TEXT("Cue throttle exists"), LKV083Art::SoundInterval(Id) >= .25f);
    }
    TestTrue(TEXT("Unknown sprite has no accidental fallback"), LKV083Art::Sprite("Unit_Unknown").IsNull());
    TestNull(TEXT("Unknown animation state is rejected"), LKV083Art::Animation("Unit_DivineJudge", "Undefined"));
    TestTrue(TEXT("Unknown audio has no generated object path"), LKV083Art::SoundPath("Unknown").IsEmpty());
    return true;
}
#endif
