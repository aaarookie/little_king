#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Engine/Texture2D.h"
#include "../LKBattleArt.h"
#include "../LKUnitContent.h"
#include "../ALKBattleGameMode.h"
#include "../ALKUnitBase.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKRunSubsystem.h"
#include "../ULKUnitAnimationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBattleArtDefaultsTest, "LittleKing.Presentation.BattleArtDefaultsAndOverrides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKBattleArtDefaultsTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Battle catalogue includes eleven summon-only angel units"), LKBattleArt::UnitIds().Num(), 31);
    ULKGameData* Authored = LoadObject<ULKGameData>(nullptr, TEXT("/Game/Data/DA_GameData.DA_GameData"));
    if (!TestNotNull(TEXT("Shipped data loads"), Authored)) { return false; }
    ULKGameData* Data = DuplicateObject<ULKGameData>(Authored, GetTransientPackage());
    Data->EnsureCardLibrary();
    TestEqual(TEXT("All cards retained, including nine unlockable v0.8.3 spells"), Data->CardLibrary.Num(), 41);
    int32 Defaults = 0;
    for (const auto& Card : Data->CardLibrary)
    {
        TestNotNull(*FString::Printf(TEXT("Card illustration %s"), *Card->CardId.ToString()), Card->Icon.LoadSynchronous());
        if (!LKBattleArt::CardIcon(Card->CardId).IsNull()) { ++Defaults; }
    }
    TestEqual(TEXT("Seventeen unit cards and nine new spell illustrations"), Defaults, 26);
    UPaperSprite* Original = LoadObject<UPaperSprite>(nullptr, TEXT("/Game/Sprites/Unit_Swordsman_sprite.Unit_Swordsman_sprite"));
    for (FName Id : LKBattleArt::UnitIds())
    {
        const FLKUnitRow* Canonical = LKUnitContent::Find(Id);
        if (!TestNotNull(*Id.ToString(), Canonical)) { continue; }
        TestNotNull(*FString::Printf(TEXT("Sprite resolves %s"), *Id.ToString()), Canonical->Sprite.LoadSynchronous());
        FLKUnitRow Row = *Canonical; Row.Sprite = nullptr; Row.BaseHealth = 1234.f;
        const FLKUnitRow Merged = LKUnitContent::MergeAuthoredTuning(*Canonical, &Row);
        TestTrue(TEXT("Missing sprite uses native art"), Merged.Sprite == Canonical->Sprite);
        TestEqual(TEXT("Authored health remains intact"), Merged.BaseHealth, 1234.f);
        Row.Sprite = Original;
        TestTrue(TEXT("Explicit artist sprite wins"), LKUnitContent::MergeAuthoredTuning(*Canonical, &Row).Sprite.Get() == Original);
    }
    ULKGameData* SharedOwner = NewObject<ULKGameData>();
    ULKCardDefinition* Shared = NewObject<ULKCardDefinition>(SharedOwner);
    Shared->CardId = "Unit_Skeleton";
    Data->CardLibrary = { Shared }; Data->EnsureCardLibrary();
    TestTrue(TEXT("Shared asset is never modified"), Shared->Icon.IsNull());
    TestTrue(TEXT("Missing image filled on private copy"), Data->CardLibrary[0] != Shared && !Data->CardLibrary[0]->Icon.IsNull());
    UTexture2D* Custom = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/StorybookV1/Textures/T_MenuKingdom.T_MenuKingdom"));
    Data->CardLibrary[0]->Icon = Custom; Data->EnsureCardLibrary();
    TestTrue(TEXT("Artist override survives refresh"), Data->CardLibrary[0]->Icon.Get() == Custom);
    TestTrue(TEXT("Unknown id keeps existing fallback"), LKBattleArt::Sprite("Unknown").IsNull());
    TestTrue(TEXT("Enemy hero is not a reward card"), LKBattleArt::CardIcon("Hero_Necromancer").IsNull());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBattleArtSpawnTest, "LittleKing.Presentation.BattleArtSpawnGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKBattleArtSpawnTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Env;
    if (!Env.CreateTestWorld(EWorldType::Game)) { return false; }
    UWorld* World = Env.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr,
        TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
    World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->SetAutoSaveEnabled(false);
    if (!Env.BeginPlayInTestWorld()) { Env.ForwardErrorMessages(this); return false; }
    ALKBattleGameMode* GM = World->GetAuthGameMode<ALKBattleGameMode>();
    if (!TestNotNull(TEXT("Shipped game mode loads"), GM)) { return false; }
    for (TActorIterator<ALKUnitBase> It(World); It; ++It) { It->Destroy(); }
    TArray<FName> Ids = { "Hero_Knight", "Hero_Mage", "Hero_Ranger", "Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer", "Building_ArrowTower", "Building_Barracks" };
    Ids.Append(LKBattleArt::UnitIds());
    // Grow the catalogue without extending the historical four-row grid beyond
    // the actual map bounds. Every shipped identity still spawns and is checked.
    constexpr int32 Columns = 7;
    const int32 Rows = FMath::DivideAndRoundUp(Ids.Num(), Columns);
    const float Inset = GM->GetGameData()->UnitBodyRadius + 100.f;
    const float HalfX = GM->GetGameData()->FieldHalfWidth - Inset;
    const float HalfY = GM->GetGameData()->FieldHalfHeight - Inset;
    if (!TestTrue(TEXT("Fixture field has room inside the collision margins"), HalfX > 0.f && HalfY > 0.f)) { return false; }
    int32 Spawned = 0;
    for (int32 Index = 0; Index < Ids.Num(); ++Index)
    {
        const FVector Position(FMath::Lerp(HalfX, -HalfX, float(Index / Columns) / FMath::Max(1, Rows - 1)),
            FMath::Lerp(-HalfY, HalfY, float(Index % Columns) / (Columns - 1)), 0);
        TestTrue(TEXT("Each geometry fixture stays in its configured battlefield"), GM->IsInsideField(Position));
        ALKUnitBase* Unit = GM->SpawnUnitForTeam(Ids[Index], ELKTeam::Player, Position);
        if (!TestNotNull(*Ids[Index].ToString(), Unit)) { continue; }
        ++Spawned;
        Unit->SetCombatEnabled(false);
        TestTrue(*FString::Printf(TEXT("Shipped entity initializes D animation: %s"),*Ids[Index].ToString()),Unit->GetAnimationComponent()->HasAnimations());
        UPaperSpriteComponent* Sprite = Unit->GetSpriteComponent();
        if (!TestNotNull(TEXT("Spawn has sprite"), Sprite->GetSprite())) { continue; }
        TestEqual(TEXT("Presentation cannot change collision radius"), Unit->GetBodyRadius(), GM->GetGameData()->UnitBodyRadius);
        TestEqual(TEXT("Actor scale stays unit scale"), Unit->GetActorScale3D(), FVector::OneVector);
        TestTrue(TEXT("Masked art sits above floor without lifting body"), Sprite->GetRelativeLocation().Z > 0 && Unit->GetActorLocation().Z == 0);
        TestTrue(TEXT("Sprite never blocks deployment"), Sprite->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
        TestTrue(TEXT("No vertical stretching"), FMath::IsNearlyEqual(Sprite->GetRelativeScale3D().X, Sprite->GetRelativeScale3D().Z));
        const FBoxSphereBounds Bounds = Sprite->CalcBounds(Sprite->GetComponentTransform());
        TestTrue(TEXT("Visible geometry correctly baked"), Bounds.BoxExtent.X > 50 && Bounds.BoxExtent.Y > 30);
    }
    TestEqual(TEXT("Geometry coverage includes every registered shipped entity"), Spawned, Ids.Num());
    // Full viewport rendering is exercised by the editor-only -BattleArtPreview workflow.
    return true;
}
#endif
