#include "ULKHomeListButtonWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "LKHomeUIStyle.h"
#include "LKPresentationStyle.h"

namespace
{
    void StyleEntry(UButton* Button, bool bSelected)
    {
        LKPresentationStyle::StyleButton(Button, bSelected);
    }
}

TSharedRef<SWidget> ULKHomeListButtonWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) { BuildNativeTree(); }
    return Super::RebuildWidget();
}

void ULKHomeListButtonWidget::BuildNativeTree()
{
    Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RowButton"));
    WidgetTree->RootWidget = Button;
    Button->OnClicked.AddDynamic(this, &ULKHomeListButtonWidget::HandleClicked);
    UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
    Button->SetContent(Content);
    if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Content->Slot))
    {
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Fill);
    }
    BadgeSize = WidgetTree->ConstructWidget<USizeBox>();
    BadgeSize->SetWidthOverride(44.f);
    BadgeSize->SetHeightOverride(44.f);
    UHorizontalBoxSlot* BadgeSlot = Content->AddChildToHorizontalBox(BadgeSize);
    BadgeSlot->SetPadding(FMargin(12.f, 10.f, 0.f, 10.f));
    BadgeSlot->SetVerticalAlignment(VAlign_Center);
    Badge = WidgetTree->ConstructWidget<UBorder>();
    BadgeSize->SetContent(Badge);
    Badge->SetVerticalAlignment(VAlign_Center);
    BadgeText = WidgetTree->ConstructWidget<UTextBlock>();
    BadgeText->SetJustification(ETextJustify::Center);
    Badge->SetContent(BadgeText);
    UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>();
    UHorizontalBoxSlot* CopySlot = Content->AddChildToHorizontalBox(Copy);
    CopySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CopySlot->SetPadding(FMargin(14.f, 10.f));
    CopySlot->SetVerticalAlignment(VAlign_Center);
    LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RowLabel"));
    ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RowValue"));
    Copy->AddChildToVerticalBox(LabelText);
    Copy->AddChildToVerticalBox(ValueText);
    for (UTextBlock* Text : {LabelText.Get(), ValueText.Get(), BadgeText.Get()})
    {
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = Text == ValueText ? 15 : 18;
        Text->SetFont(LKPresentationStyle::Font(Font.Size));
        Text->SetAutoWrapText(true);
    }
    LabelText->SetColorAndOpacity(LKPresentationStyle::Paper());
    ValueText->SetColorAndOpacity(FSlateColor(LKPresentationStyle::Muted()));
    bBuilt = true;
}

void ULKHomeListButtonWidget::SetupRow(int32 InIndex, const FLKHomePanelRow& Row, bool bSelected)
{
    Index = InIndex;
    if (!bBuilt && WidgetTree && !WidgetTree->RootWidget) { BuildNativeTree(); }
    if (!Button) { return; }
    LabelText->SetText(Row.Label);
    ValueText->SetText(Row.Value);
    ValueText->SetVisibility(Row.Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    const bool bInteractive = Row.bSelectable || Row.bToggleable;
    // Informational rows stay readable instead of inheriting disabled button tint.
    Button->SetIsEnabled(!bInteractive || Row.bEnabled);
    Button->SetVisibility(bInteractive ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
    BadgeSize->SetVisibility(Row.RowId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    Badge->SetBrushColor(LKHomeUIStyle::Accent(Row.RowId) * 0.65f);
    BadgeText->SetText(FText::FromString(Row.bToggleable ? (Row.bChecked ? TEXT("√") : TEXT("＋")) : Row.Label.ToString().Left(1)));
    BadgeText->SetAutoWrapText(false);
    StyleEntry(Button, bSelected || (Row.bToggleable && Row.bChecked));
}

void ULKHomeListButtonWidget::SetupAction(int32 InIndex, const FLKHomePanelAction& Action)
{
    Index = InIndex;
    if (!bBuilt && WidgetTree && !WidgetTree->RootWidget) { BuildNativeTree(); }
    if (!Button) { return; }
    LabelText->SetText(Action.Label);
    LabelText->SetAutoWrapText(false);
    LabelText->SetJustification(ETextJustify::Center);
    BadgeSize->SetVisibility(ESlateVisibility::Collapsed);
    ValueText->SetVisibility(ESlateVisibility::Collapsed);
    Button->SetIsEnabled(Action.bEnabled);
    StyleEntry(Button, Action.ActionId == "Start" || Action.ActionId == "Upgrade" || Action.ActionId == "SaveLoadout");
}

void ULKHomeListButtonWidget::SetCompactRow()
{
    if (!BadgeSize || !LabelText || !ValueText) { return; }
    BadgeSize->SetWidthOverride(34.f); BadgeSize->SetHeightOverride(34.f);
    if (UHorizontalBoxSlot* Layout = Cast<UHorizontalBoxSlot>(BadgeSize->Slot)) { Layout->SetPadding(FMargin(10.f,6.f,0.f,6.f)); }
    if (UPanelWidget* Copy = LabelText->GetParent())
    { if (UHorizontalBoxSlot* Layout = Cast<UHorizontalBoxSlot>(Copy->Slot)) { Layout->SetPadding(FMargin(12.f,6.f)); } }
    for (UTextBlock* Text : {LabelText.Get(), ValueText.Get(), BadgeText.Get()})
    { FSlateFontInfo Font = Text->GetFont(); Font.Size = Text == ValueText ? 13 : 16; Text->SetFont(LKPresentationStyle::Font(Font.Size)); }
}

void ULKHomeListButtonWidget::HandleClicked()
{
    if (Index != INDEX_NONE && OnHomeListClicked.IsBound()) { OnHomeListClicked.Execute(Index); }
}
