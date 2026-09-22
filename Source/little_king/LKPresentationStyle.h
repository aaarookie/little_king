#pragma once
#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Sound/SoundBase.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"
#include "LKPolishArt.h"

/** Storybook V1: shared display palette; never used to infer gameplay identity. */
namespace LKPresentationStyle
{
    inline FSlateFontInfo Font(int32 Size, bool bTitle=false)
    {
        if(UFont* Asset=LKPolishArt::Font(bTitle||Size>=26)){return FSlateFontInfo(Asset,Size,"Default");}
        return FCoreStyle::GetDefaultFontStyle(bTitle?"Bold":"Regular",Size);
    }
    inline FLinearColor Ink() { return FLinearColor::FromSRGBColor(FColor(22, 37, 32)); }
    inline FLinearColor Panel() { return FLinearColor::FromSRGBColor(FColor(31, 54, 45)); }
    inline FLinearColor Card() { return FLinearColor::FromSRGBColor(FColor(45, 67, 54)); }
    inline FLinearColor Hover() { return FLinearColor::FromSRGBColor(FColor(63, 87, 67)); }
    inline FLinearColor Gold() { return FLinearColor::FromSRGBColor(FColor(209, 177, 112)); }
    inline FLinearColor Paper() { return FLinearColor::FromSRGBColor(FColor(244, 230, 200)); }
    inline FLinearColor Muted() { return FLinearColor::FromSRGBColor(FColor(189, 193, 164)); }
    inline FSlateBrush Frame(FLinearColor Fill, bool bEmphasis = false)
    { return FSlateRoundedBoxBrush(Fill, 6.f, bEmphasis ? Gold() : Gold() * .42f, bEmphasis ? 1.5f : 1.f); }
    inline void StyleButton(UButton* Button, bool bPrimary = false)
    {
        if (!Button) { return; }
        FButtonStyle Style = Button->GetStyle();
        Style.Normal = Frame(bPrimary ? Hover() : Card(), bPrimary);
        Style.Hovered = Frame(Hover(), true);
        Style.Pressed = Frame(Panel(), true);
        Style.Disabled = Frame(Ink());
        // Resource held by FSlateSound inside the widget style; no per-click disk loading.
        if (USoundBase* Sound = LoadObject<USoundBase>(nullptr,
            TEXT("/Game/Art/StorybookV1/Audio/S_UIClick.S_UIClick"), nullptr, LOAD_NoWarn))
        { Style.PressedSlateSound.SetResourceObject(Sound); }
        Button->SetStyle(Style);
    }
    inline void StylePanel(UBorder* Border, FLinearColor Fill)
    {
        if (!Border) { return; }
        Border->SetBrush(Frame(Fill));
        Border->SetBrushColor(FLinearColor::White);
    }
}
