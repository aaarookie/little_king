#include "ULKBattlePauseWidget.h"
#include "ALKBattleGameMode.h"
#include "ALKPlayerController.h"
#include "Engine/World.h"
#include "LKPresentationStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Kismet/GameplayStatics.h"

TSharedRef<SWidget> ULKBattlePauseWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        auto Text=[&](const TCHAR* Copy,int32 Size)
        { auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetFont(LKPresentationStyle::Font(Size)); T->SetText(FText::FromString(Copy)); T->SetColorAndOpacity(LKPresentationStyle::Paper()); T->SetAutoWrapText(true); return T; };
        PauseButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("PauseBattleButton")); LKPresentationStyle::StyleButton(PauseButton);
        PauseButton->SetContent(Text(TEXT("暂停 · Esc"),20)); PauseButton->OnClicked.AddDynamic(this,&ULKBattlePauseWidget::PauseClicked);
        auto* P=Root->AddChildToCanvas(PauseButton); P->SetAnchors(FAnchors(1,0)); P->SetAlignment(FVector2D(1,0)); P->SetPosition(FVector2D(-24,18)); P->SetSize(FVector2D(170,50));
        Modal=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("PauseModal")); Modal->SetBrushColor(FLinearColor(0.01f,.02f,.025f,.94f));
        auto* M=Root->AddChildToCanvas(Modal); M->SetAnchors(FAnchors(0,0,1,1)); M->SetOffsets(FMargin(0)); M->SetZOrder(1);
        Modal->SetHorizontalAlignment(HAlign_Center); Modal->SetVerticalAlignment(VAlign_Center);
        USizeBox* Frame=WidgetTree->ConstructWidget<USizeBox>(); Frame->SetWidthOverride(570); Modal->SetContent(Frame);
        UBorder* Panel=WidgetTree->ConstructWidget<UBorder>(); LKPresentationStyle::StylePanel(Panel,LKPresentationStyle::Panel()); Panel->SetPadding(FMargin(30)); Frame->SetContent(Panel);
        UVerticalBox* Body=WidgetTree->ConstructWidget<UVerticalBox>(); Panel->SetContent(Body);
        Body->AddChildToVerticalBox(Text(TEXT("战斗已暂停"),30))->SetPadding(FMargin(0,0,0,18));
        Body->AddChildToVerticalBox(Text(TEXT("退出后回到开始界面。继续存档将从本场战斗前重试，已选路线与敌阵保持不变。"),19))->SetPadding(FMargin(0,0,0,24));
        auto* Continue=WidgetTree->ConstructWidget<UButton>(); LKPresentationStyle::StyleButton(Continue); Continue->SetContent(Text(TEXT("继续"),23)); Continue->OnClicked.AddDynamic(this,&ULKBattlePauseWidget::Resume);
        Body->AddChildToVerticalBox(Continue)->SetPadding(FMargin(0,6));
        auto* Quit=WidgetTree->ConstructWidget<UButton>(); LKPresentationStyle::StyleButton(Quit); Quit->SetContent(Text(TEXT("退出游戏"),23)); Quit->OnClicked.AddDynamic(this,&ULKBattlePauseWidget::Exit);
        Body->AddChildToVerticalBox(Quit)->SetPadding(FMargin(0,6));
        Status=Text(TEXT(""),17); Body->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,12,0,0));
        Modal->SetVisibility(ESlateVisibility::Collapsed);
    }
    return Super::RebuildWidget();
}
void ULKBattlePauseWidget::NativeTick(const FGeometry& G,float D)
{
    Super::NativeTick(G,D);
    const auto* GM=GetWorld()?GetWorld()->GetAuthGameMode<ALKBattleGameMode>():nullptr;
    const bool Enabled=GM && (GM->GetPhase()==ELKGamePhase::Battle || GM->GetPhase()==ELKGamePhase::Deployment) && !GM->IsRecoveryJunction() && !GM->IsRecoveryTerminal();
    if (PauseButton) { PauseButton->SetVisibility(Enabled?ESlateVisibility::Visible:ESlateVisibility::Collapsed); }
}
void ULKBattlePauseWidget::PauseClicked() { TogglePause(); }
void ULKBattlePauseWidget::TogglePause()
{
    if (bOpen) { Resume(); return; }
    const auto* GM=GetWorld()?GetWorld()->GetAuthGameMode<ALKBattleGameMode>():nullptr;
    if (!GM || (GM->GetPhase()!=ELKGamePhase::Battle && GM->GetPhase()!=ELKGamePhase::Deployment) || GM->IsRecoveryJunction() || GM->IsRecoveryTerminal()) { return; }
    APlayerController* Controller=GetOwningPlayer(); if (!Controller) { Controller=GetWorld()->GetFirstPlayerController(); }
    if (!Controller || !Controller->SetPause(true)) { return; }
    bOpen=true;
    if (auto* PC=Cast<ALKPlayerController>(GetOwningPlayer())) { PC->CancelPlacement(); }
    Status->SetText(FText::GetEmpty()); Modal->SetVisibility(ESlateVisibility::Visible);
}
void ULKBattlePauseWidget::Resume()
{
    APlayerController* Controller=GetOwningPlayer(); if (!Controller) { Controller=GetWorld()->GetFirstPlayerController(); }
    if (Controller) { Controller->SetPause(false); }
    bOpen=false; if(Modal) { Modal->SetVisibility(ESlateVisibility::Collapsed); }
}
void ULKBattlePauseWidget::Exit()
{
    auto* GM=GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (!GM || !GM->ExitToStartMenu()) { Status->SetText(FText::FromString(TEXT("存档未完成，请重试；当前战斗仍暂停。"))); }
    else { bOpen=false; SetIsEnabled(false); }
}
