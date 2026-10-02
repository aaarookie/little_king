#include "LKV083Preview.h"
#if WITH_EDITOR
#include "ALKBattleGameMode.h"
#include "ALKOpponentBrain.h"
#include "ALKUnitBase.h"
#include "ALKHeroCamp.h"
#include "ALKSpellField.h"
#include "ULKBattleHUDWidget.h"
#include "ULKDeckState.h"
#include "ULKSilverComponent.h"
#include "Components/TextBlock.h"
#include "ULKUnitAnimationComponent.h"
#include "ULKUnitMovementComponent.h"
#include "ULKGameData.h"
#include "ULKCardDefinition.h"
#include "ULKPresentationSubsystem.h"
#include "LKSpellExecutor.h"
#include "LKV083Art.h"
#include "LKPolishArt.h"
#include "LKCardPresentation.h"
#include "PaperSpriteComponent.h"
#include "PaperFlipbook.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Debug/DebugDrawService.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
int32 Frame = 0;
FString Caption;
bool bCards = false;
FDelegateHandle DrawHandle;
TWeakObjectPtr<ALKBattleGameMode> PreviewMode;
TWeakObjectPtr<ACameraActor> PreviewCamera;
TArray<TWeakObjectPtr<ALKUnitBase>> Units;
TArray<TStrongObjectPtr<UTexture2D>> Icons;
const TArray<FName> RoleUnits = { "Unit_AngelWarrior_Rare", "Unit_AngelArcher_Rare", "Unit_AngelPriest_Rare", "Unit_DivineJudge", "Unit_ChosenHighPriest" };
const TCHAR* RoleNames[] = { TEXT("战斗天使"), TEXT("弓箭手天使"), TEXT("牧师天使"), TEXT("神圣审判者"), TEXT("神选牧师长") };

void Text(UCanvas* Canvas, const FString& String, float X, float Y, float Height, float Width, FLinearColor Color)
{
    UFont* Font = LKPolishArt::Font(false);
    if (!Font) { Font = GEngine ? GEngine->GetSmallFont() : nullptr; }
    if (!Font) { return; }
    int32 MeasuredWidth = 0, MeasuredHeight = 0;
    UCanvas::ClippedStrLen(Font, 1.f, 1.f, MeasuredWidth, MeasuredHeight, String);
    const float Scale = FMath::Min(Height / FMath::Max(1, MeasuredHeight), Width / FMath::Max(1, MeasuredWidth));
    FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(String), Font, Color);
    Item.Scale = FVector2D(Scale);
    Item.EnableShadow(FLinearColor(0, 0, 0, .45f));
    Canvas->DrawItem(Item);
}
void Rect(UCanvas* Canvas, float X, float Y, float W, float H, FLinearColor Color)
{
    FCanvasTileItem Item(FVector2D(X, Y), FVector2D(W, H), Color);
    Item.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Item);
}
void Draw(UCanvas* Canvas, APlayerController* PC)
{
    ALKBattleGameMode* Mode = PreviewMode.Get();
    if (!Canvas || !Mode || !Mode->GetWorld()) { return; }
    if (!PC) { PC = Mode->GetWorld()->GetFirstPlayerController(); }
    const float S = Canvas->ClipY / 1080.f;
    const FLinearColor Pine(.10f, .23f, .20f), Parchment(.90f, .84f, .68f), Gold(.74f, .59f, .31f);
    if (bCards) { Rect(Canvas, 0, 0, Canvas->ClipX, Canvas->ClipY, Pine); }
    Rect(Canvas, 0, 0, Canvas->ClipX, 88*S, Pine.CopyWithNewOpacity(.95f));
    Text(Canvas, TEXT("Little King · v0.8.3 · 隔离素材预览"), 36*S, 14*S, 27*S, Canvas->ClipX-72*S, Gold);
    Text(Canvas, Caption, 36*S, 48*S, 23*S, Canvas->ClipX-72*S, Parchment);
    if (bCards)
    {
        const float Margin = 30*S, Gap = 20*S;
        const float CW = (Canvas->ClipX - 2*Margin - 2*Gap)/3;
        const float CH = (Canvas->ClipY - 140*S - 2*Gap)/3;
        for (int32 I=0; I<LKV083Art::SpellIds().Num(); ++I)
        {
            const FName Id = LKV083Art::SpellIds()[I];
            const auto* Entry = Mode->GetGameData()->CardLibrary.FindByPredicate([Id](const TObjectPtr<ULKCardDefinition>& C){return C && C->CardId==Id;});
            const ULKCardDefinition* Card = Entry ? Entry->Get() : nullptr;
            const float X = Margin + (I%3)*(CW+Gap), Y = 105*S + (I/3)*(CH+Gap);
            Rect(Canvas, X, Y, CW, CH, FLinearColor(.15f, .31f, .26f));
            const float ImageSize = FMath::Min(CH-40*S, CW*.40f);
            if (Icons.IsValidIndex(I) && Icons[I].IsValid() && Icons[I]->GetResource())
            {
                FCanvasTileItem Tile(FVector2D(X+14*S, Y+14*S), Icons[I]->GetResource(), FVector2D(ImageSize), FLinearColor::White);
                Tile.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Tile);
            }
            const float TX = X+ImageSize+26*S, TW = CW-ImageSize-38*S;
            Text(Canvas, Card ? Card->CardName.ToString() : Id.ToString(), TX, Y+22*S, 25*S, TW, Parchment);
            if (Card)
            {
                Text(Canvas, LKCardPresentation::SpellGradeName(Card->SpellGrade).ToString(), TX, Y+70*S, 21*S, TW, Gold);
                Text(Canvas, FString::Printf(TEXT("%d 银币 · 图书馆研究解锁"), Card->Cost), TX, Y+106*S, 20*S, TW, Parchment);
                if (Card->DrawCooldown > 0.f)
                { Text(Canvas, FString::Printf(TEXT("施放后 %.0f 秒不再抽到"), Card->DrawCooldown), TX, Y+142*S, 19*S, TW, Parchment); }
            }
        }
    }
    else if (Frame<685 && Units.Num()==5 && PC)
    {
        for (int32 I=0; I<Units.Num(); ++I)
        {
            ALKUnitBase* Unit = Units[I].Get(); if (!Unit) { continue; }
            FVector2D P;
            if (PC->ProjectWorldLocationToScreen(Unit->GetActorLocation()+FVector(-210,0,0), P, true))
            { Text(Canvas, RoleNames[I], P.X-125*S, P.Y, 25*S, 250*S, Parchment); }
        }
    }
    Rect(Canvas, 0, Canvas->ClipY-34*S, Canvas->ClipX, 34*S, Pine.CopyWithNewOpacity(.90f));
    Text(Canvas, TEXT("独立进程 · 永久档读写关闭 · 远征自动存储关闭 · 不修改玩家进度"), 36*S, Canvas->ClipY-29*S, 18*S, Canvas->ClipX-72*S, Parchment);
}
void Clear(UWorld* World)
{
    for (TActorIterator<ALKUnitBase> It(World); It; ++It) { It->Destroy(); }
    for (TActorIterator<ALKHeroCamp> It(World); It; ++It) { It->Destroy(); }
    Units.Reset();
}
ALKUnitBase* Add(ALKBattleGameMode* Mode, FName Id, ELKTeam Team, FVector Position)
{
    ALKUnitBase* Unit = Mode->SpawnUnitForTeam(Id, Team, FVector(900,-1600,0));
    if (Unit)
    {
        Unit->SetActorLocation(Position); Unit->SetCombatEnabled(true); Unit->SetActorTickEnabled(false);
        Unit->GetMovementComponent()->SetComponentTickEnabled(false);
        Unit->GetAnimationComponent()->SetComponentTickEnabled(false);
        Units.Add(Unit);
    }
    return Unit;
}
void Shot(UWorld* World, const FString& Name)
{
    int32 Width=0, Height=0; World->GetFirstPlayerController()->GetViewportSize(Width, Height);
    const FString Dir = FPaths::ProjectSavedDir()/TEXT("Screenshots/V083");
    IFileManager::Get().MakeDirectory(*Dir, true);
    FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%s_%d.png"), *Name, Height), true, false);
}
}

void LKV083Preview::Tick(ALKBattleGameMode* Mode)
{
    if (!Mode || !Mode->GetBattleHUDWidget() || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())) { return; }
    ++Frame; UWorld* World = Mode->GetWorld();
    if (Frame==1)
    {
        PreviewMode=Mode;
        for (int32 I=0; I<Mode->AvailableHeroes.Num(); ++I)
        { Mode->DeployHero(ELKTeam::Player, Mode->AvailableHeroes[I], FVector((I-1)*700.f,-1300,0)); }
        Mode->ForceStartBattle(); Clear(World);
        PreviewCamera=World->SpawnActor<ACameraActor>();
        PreviewCamera->SetActorLocationAndRotation(FVector(0,0,5000), FRotator(-90,0,0));
        PreviewCamera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Orthographic;
        PreviewCamera->GetCameraComponent()->OrthoWidth=2700;
        PreviewCamera->GetCameraComponent()->bConstrainAspectRatio=false;
        World->GetFirstPlayerController()->SetViewTarget(PreviewCamera.Get());
        Mode->GetBattleHUDWidget()->SetVisibility(ESlateVisibility::Hidden);
        if (AHUD* HUD=World->GetFirstPlayerController()->GetHUD()) { HUD->bShowHUD=false; }
        DrawHandle=UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(&Draw));
        for (int32 I=0; I<RoleUnits.Num(); ++I)
        { Add(Mode, RoleUnits[I], ELKTeam::Player, FVector(180,(I-2)*500.f,0)); }
        for (FName Id:LKV083Art::SpellIds()) { Icons.Emplace(LKV083Art::CardIcon(Id).LoadSynchronous()); }
        if (Units.Num()!=5 || Icons.Num()!=9)
        { UE_LOG(LogTemp, Error, TEXT("V083_PREVIEW_FAILED missing preview assets")); FPlatformMisc::RequestExit(false); return; }
    }
    if (Frame>=60 && Frame<540)
    {
        const int32 Index=(Frame-60)/30;
        const FName State=Index<4?FName("Idle"):Index<8?FName("Move"):Index<12?FName("Attack"):Index<14?FName("Hit"):FName("Death");
        const int32 Local=Index<12?Index%4:Index%2;
        if ((Frame-60)%30==0)
        {
            Caption=FString::Printf(TEXT("五种天使角色 · %s · 第 %d 帧 · 真实 Sprite 渲染"), *State.ToString(), Local+1);
            for (auto Weak:Units) { if (ALKUnitBase* Unit=Weak.Get())
            { if (UPaperFlipbook* Clip=LKPolishArt::Animation(Unit->GetUnitId(),State)) { Unit->GetSpriteComponent()->SetSprite(Clip->GetSpriteAtFrame(Local)); } } }
        }
        if ((Frame-60)%30==15) { Shot(World, FString::Printf(TEXT("Pose_%02d_%s"), Index, *State.ToString())); }
    }
    if (Frame==550) { bCards=true; Caption=TEXT("九张解锁法术 · 独立卡图、阶位与费用 · 原生 Canvas"); }
    if (Frame==575) { Shot(World, TEXT("SpellCards")); }
    if (Frame==590)
    {
        bCards=false; Caption=TEXT("真实移动驱动动画 · 向右");
        for (int32 I=0; I<Units.Num(); ++I) { if (ALKUnitBase* Unit=Units[I].Get())
        { Unit->SetActorLocation(FVector(180,(I-2)*500.f-100,0)); Unit->GetAnimationComponent()->ResetPresentation(); } }
    }
    if ((Frame>=600 && Frame<632) || (Frame>=640 && Frame<672))
    {
        const bool Right=Frame<632; const int32 Local=Right?Frame-600:Frame-640;
        Caption=Right?TEXT("真实移动驱动动画 · 向右 · 四步态采样"):TEXT("真实移动驱动动画 · 向左 · 四步态采样");
        for (auto Weak:Units) { if (ALKUnitBase* Unit=Weak.Get())
        {
            if (Local==0) { Unit->GetAnimationComponent()->ResetPresentation(); }
            const float Stride=FMath::Clamp(float(Unit->GetAnimationComponent()->GetIdleBounds().BoxExtent.X*1.4),100.f,280.f);
            auto* Move=Unit->GetMovementComponent();
            Move->MoveToward(Unit->GetActorLocation()+FVector(0,Right?500:-500,0), Stride/1.6f);
            Move->TickComponent(.05f,LEVELTICK_All,nullptr); Unit->GetAnimationComponent()->TickComponent(.05f,LEVELTICK_All,nullptr);
        } }
        if (Local%8==7) { Shot(World, FString::Printf(TEXT("Walk_%s_%02d"),Right?TEXT("Right"):TEXT("Left"),Local/8)); }
    }
    if (Frame==633 || Frame==673)
    {
        for (auto Weak:Units) { if (ALKUnitBase* Unit=Weak.Get())
        { Unit->GetMovementComponent()->Stop(); Unit->GetAnimationComponent()->TickComponent(.05f,LEVELTICK_All,nullptr); } }
        Caption=TEXT("停止移动 · 回到待机 · 保留最后朝向");
        Shot(World, Frame==633?TEXT("Stopped_Right"):TEXT("Stopped_Left"));
    }
    if (Frame==685)
    {
        Clear(World); Caption=TEXT("黑云、冰冻与向右飓风 · 原生范围和状态反馈");
        PreviewCamera->GetCameraComponent()->OrthoWidth=4000;
        Add(Mode,"Unit_AngelWarrior_Rare",ELKTeam::Player,FVector(100,-900,0));
        Add(Mode,"Unit_Skeleton",ELKTeam::Enemy,FVector(-120,-850,0));
        Add(Mode,"Unit_AngelArcher_Rare",ELKTeam::Player,FVector(100,800,0));
        Add(Mode,"Unit_SkeletonArcher",ELKTeam::Enemy,FVector(-150,850,0));
        Add(Mode,"Unit_Skeleton",ELKTeam::Enemy,FVector(-600,0,0));
        for (const TPair<FName,FVector>& Spell:{TPair<FName,FVector>("Spell_BlackCloud",FVector(0,-850,0)),TPair<FName,FVector>("Spell_Hurricane",FVector(0,850,0))})
        {
            const auto* Card=Mode->GetGameData()->CardLibrary.FindByPredicate([&](const TObjectPtr<ULKCardDefinition>& C){return C && C->CardId==Spell.Key;});
            if (Card) { LKSpellExecutor::Execute(Mode,Card->Get(),ELKTeam::Player,Spell.Value,nullptr,true); }
        }
        for (TActorIterator<ALKSpellField> It(World); It; ++It) { It->Tick(.15f); It->SetActorTickEnabled(false); }
        if (AHUD* HUD=World->GetFirstPlayerController()->GetHUD()) { HUD->bShowHUD=true; }
    }
    if (Frame==708)
    {
        const auto* Card=Mode->GetGameData()->CardLibrary.FindByPredicate([](const TObjectPtr<ULKCardDefinition>& C){return C && C->CardId=="Spell_Freeze";});
        if (Card) { LKSpellExecutor::Execute(Mode,Card->Get(),ELKTeam::Player,FVector(-600,0,0),nullptr,true); }
    }
    if (Frame==713) { Shot(World,TEXT("Fields")); }
    if (Frame==730)
    {
        UDebugDrawService::Unregister(DrawHandle); Icons.Reset();
        UE_LOG(LogTemp, Display, TEXT("V083_PREVIEW_OK roleIdentities=5 poseCaptures=16 cardCaptures=1 walkCaptures=8 stoppedCaptures=2 fieldCaptures=1"));
        FPlatformMisc::RequestExit(false);
    }
}

void LKV083Preview::TickHUD(ALKBattleGameMode* Mode)
{
    if (!Mode || !Mode->GetBattleHUDWidget() || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())) { return; }
    static int32 HUDFrame=0;
    ++HUDFrame;
    UWorld* World=Mode->GetWorld();
    if (HUDFrame==1)
    {
        // Cap the isolated UI process so it observes real battle-time progress,
        // rather than assuming a renderer frame count represents a duration.
        GEngine->SetMaxFPS(30.f);
        for (int32 I=0; I<Mode->AvailableHeroes.Num(); ++I)
        { Mode->DeployHero(ELKTeam::Player,Mode->AvailableHeroes[I],FVector((I-1)*700.f,-1300,0)); }
        Mode->ForceStartBattle();
        for (TActorIterator<ALKUnitBase> It(World); It; ++It)
        { It->SetActorTickEnabled(false); It->GetMovementComponent()->SetComponentTickEnabled(false); }
        for (TActorIterator<ALKOpponentBrain> It(World); It; ++It) { It->SetScriptedWaves({}); }
        auto* Deck=Mode->GetTeamDeck(ELKTeam::Player);
        const TArray<FName> Cards={"Spell_DivineBlessing","Spell_MariaNovice","Spell_MariaIntermediate","Spell_MariaAdvanced","Spell_MariaDivine"};
        for (int32 Seed=0; Seed<128; ++Seed)
        { Deck->InitDeck(Cards,4,Seed); if (Deck->GetHand().Contains("Spell_DivineBlessing")) { break; } }
        Mode->GetTeamSilver(ELKTeam::Player)->AddSilver(20.f);
    }
    if (HUDFrame==40)
    {
        const int32 Slot=Mode->GetTeamDeck(ELKTeam::Player)->GetHand().IndexOfByKey("Spell_DivineBlessing");
        if (Mode->PlayCardForTeam(ELKTeam::Player,Slot,FVector(0,-850,0))!=ELKPlayResult::Success)
        { UE_LOG(LogTemp,Error,TEXT("V083_HUD_PREVIEW_FAILED actual blessing cast failed")); FPlatformMisc::RequestExit(false); return; }
    }
    if (HUDFrame==90)
    {
        auto* Deck=Mode->GetTeamDeck(ELKTeam::Player);
        auto* Label=Cast<UTextBlock>(Mode->GetBattleHUDWidget()->GetWidgetFromName("SpellCooldownText"));
        TSet<FName> Unique(Deck->GetHand());
        if (!Label || Label->GetText().IsEmpty() || !Label->IsVisible() || Deck->GetCooldownRemaining("Spell_DivineBlessing")>=149.9f || Mode->GetTeamSilver(ELKTeam::Player)->GetSilver()<=12.5f || Deck->GetHandSize()!=4 || Unique.Num()!=4 || Deck->GetHand().Contains("Spell_DivineBlessing"))
        { UE_LOG(LogTemp,Error,TEXT("V083_HUD_PREVIEW_FAILED hand=%d unique=%d cooldown=%.2f silver=%.2f phase=%d label=%s"),Deck->GetHandSize(),Unique.Num(),Deck->GetCooldownRemaining("Spell_DivineBlessing"),Mode->GetTeamSilver(ELKTeam::Player)->GetSilver(),int32(Mode->GetPhase()),Label?*Label->GetText().ToString():TEXT("missing")); FPlatformMisc::RequestExit(false); return; }
        Shot(World,TEXT("HUDCooldown"));
        UE_LOG(LogTemp,Display,TEXT("V083_HUD_PREVIEW_OK hand=4 unique=4 cooldown=%.2f silver=%.2f label=%s"),Deck->GetCooldownRemaining("Spell_DivineBlessing"),Mode->GetTeamSilver(ELKTeam::Player)->GetSilver(),*Label->GetText().ToString());
    }
    if (HUDFrame==110) { FPlatformMisc::RequestExit(false); }
}
#endif
