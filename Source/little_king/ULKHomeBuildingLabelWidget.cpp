#include "ULKHomeBuildingLabelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

TSharedRef<SWidget> ULKHomeBuildingLabelWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
        Size->SetWidthOverride(196.f);
        WidgetTree->RootWidget = Size;
        AccentBorder = WidgetTree->ConstructWidget<UBorder>();
        AccentBorder->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
        Size->SetContent(AccentBorder);
        UBorder* Plate = WidgetTree->ConstructWidget<UBorder>();
        Plate->SetBrushColor(FLinearColor(0.016f, 0.025f, 0.04f, 0.97f));
        Plate->SetPadding(FMargin(10.f, 8.f));
        AccentBorder->SetContent(Plate);
        UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
        Plate->SetContent(Content);
        TitleText = WidgetTree->ConstructWidget<UTextBlock>();
        HintText = WidgetTree->ConstructWidget<UTextBlock>();
        for (UTextBlock* Text : {TitleText.Get(), HintText.Get()})
        {
            FSlateFontInfo Font = Text->GetFont();
            Font.Size = Text == TitleText ? 17 : 12;
            Text->SetFont(Font);
            Text->SetJustification(ETextJustify::Center);
            Content->AddChildToVerticalBox(Text);
        }
        HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.73f, 0.80f)));
        SetVisibility(ESlateVisibility::HitTestInvisible);
        UpdateLabel();
    }
    return Super::RebuildWidget();
}

void ULKHomeBuildingLabelWidget::SetLabel(const FText& InTitle, const FText& InHint, FLinearColor InAccent)
{
    Title = InTitle; Hint = InHint; Accent = InAccent;
    UpdateLabel();
}

void ULKHomeBuildingLabelWidget::UpdateLabel()
{
    if (TitleText) { TitleText->SetText(Title); }
    if (HintText) { HintText->SetText(Hint); }
    if (AccentBorder) { AccentBorder->SetBrushColor(Accent); }
}
