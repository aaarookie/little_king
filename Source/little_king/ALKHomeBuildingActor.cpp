#include "ALKHomeBuildingActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "ULKHomeBuildingLabelWidget.h"
#include "LKHomeUIStyle.h"
#include "LKPresentationStyle.h"
#include "LKPolishArt.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "LKHomeContent.h"
#include "LKLog.h"

namespace
{
	const FLinearColor PlaceholderHighlight(0.42f, 0.52f, 0.68f, 1.f);
	const FLinearColor LabelColor(0.93f, 0.95f, 0.98f, 1.f);
}

ALKHomeBuildingActor::ALKHomeBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	PlaceholderMesh->SetupAttachment(Root);
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetGenerateOverlapEvents(false);
	PlaceholderMesh->SetCastShadow(false);
	Illustration = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("Illustration"));
	Illustration->SetupAttachment(Root);
	Illustration->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Illustration->SetCastShadow(false);
	Illustration->SetRelativeRotation(FRotationMatrix::MakeFromXY(FVector(0, 1, 0), FVector(0, 0, 1)).Rotator());
	Illustration->SetRelativeLocation(FVector(0, 0, 5));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded()) { PlaceholderMesh->SetStaticMesh(CubeMesh.Object); }
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded()) { PlaceholderMesh->SetMaterial(0, ShapeMaterial.Object); }

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(Root);
	NameLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	NameLabel->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	NameLabel->SetWorldSize(80.f);
	NameLabel->SetTextRenderColor(LabelColor.ToFColor(true));
	NameLabel->SetCastShadow(false);
	NameLabel->SetRelativeRotation(FRotator(90.f, 90.f, 0.f)); // 俯视相机下文字朝上
	NameLabel->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	NameLabel->SetVisibility(false); // TextRender's offline font does not cover Chinese; use screen UMG.
	ScreenLabel = CreateDefaultSubobject<UWidgetComponent>(TEXT("ScreenLabel"));
	ScreenLabel->SetupAttachment(Root);
	ScreenLabel->SetWidgetSpace(EWidgetSpace::Screen);
	ScreenLabel->SetDrawAtDesiredSize(true);
	ScreenLabel->SetPivot(FVector2D(0.5f, 0.5f));
	ScreenLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HitSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HitSphere"));
	HitSphere->SetupAttachment(Root);
	HitSphere->SetSphereRadius(130.f);
	HitSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitSphere->SetGenerateOverlapEvents(false);
}

void ALKHomeBuildingActor::BeginPlay()
{
	Super::BeginPlay();
	if (!bInitialized)
	{
		// 编辑器里手工摆放、只填了 BuildingId 的情况：名称与用途从内容表解析。
		const FLKBuildingDefinition* Definition = LKHomeContent::FindBuilding(BuildingId);
		InitializeBuilding(BuildingId,
			Definition ? Definition->DisplayName : FText::FromName(BuildingId),
			Definition && Definition->bUpgradable);
	}
}

void ALKHomeBuildingActor::InitializeBuilding(FName InBuildingId, const FText& InName, bool bInUpgradable)
{
	BuildingId = InBuildingId;
	DisplayName = InName;
	bUpgradable = bInUpgradable;
	bInitialized = true;
	ApplyPlaceholderAppearance();
	RefreshLabel();
}

void ALKHomeBuildingActor::ApplyPlaceholderAppearance()
{
	const FString Name = TEXT("SP_") + (BuildingId == TEXT("Home_StatueSaintMaria") ? FString(TEXT("Home_Statue")) : BuildingId.ToString());
	const FString Path = TEXT("/Game/Art/StorybookV1/Sprites/") + Name + TEXT(".") + Name;
	UPaperSprite* Sprite = LKPolishArt::HomeLevel(BuildingId, DisplayedLevel);
    if (!Sprite) { Sprite = LoadObject<UPaperSprite>(nullptr, *Path, nullptr, LOAD_NoWarn); }
	Illustration->SetSprite(Sprite);
	Illustration->SetRelativeScale3D(FVector(1.f)); // Importer scales the actual texture width to a 500cm canvas.
	Illustration->SetSpriteColor(FLinearColor::White);
	PlaceholderMesh->SetVisibility(!Sprite);
	if (PlaceholderMesh)
	{
		PlaceholderMesh->SetRelativeScale3D(FVector(PlaceholderSize.X / 100.f, PlaceholderSize.Y / 100.f, PlaceholderSize.Z / 100.f));
		PlaceholderMesh->SetRelativeLocation(FVector(0.f, 0.f, PlaceholderSize.Z * 0.5f));
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Home/M_HomePlaceholder.M_HomePlaceholder"), nullptr, LOAD_NoWarn);
		if (!Base) { Base = PlaceholderMesh->GetMaterial(0); }
		if (Base && !PlaceholderMaterial)
		{
			PlaceholderMaterial = UMaterialInstanceDynamic::Create(Base, this);
			PlaceholderMesh->SetMaterial(0, PlaceholderMaterial);
		}
	}

	const FLinearColor Color = LKHomeUIStyle::Accent(BuildingId);
	if (PlaceholderMaterial)
	{
		PlaceholderMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
	else if (PlaceholderMesh)
	{
		PlaceholderMesh->SetCustomPrimitiveDataVector4(0, FVector4(Color.R, Color.G, Color.B, Color.A));
	}

	if (HitSphere) { HitSphere->SetSphereRadius(FMath::Max(10.f, HitRadius)); }
	if (NameLabel) { NameLabel->SetRelativeLocation(FVector(0.f, 0.f, PlaceholderSize.Z + 120.f)); }
	if (ScreenLabel) { ScreenLabel->SetRelativeLocation(Sprite ? FVector(-245.f, 0.f, 30.f) : FVector(-120.f, 0.f, PlaceholderSize.Z + 20.f)); }
}

bool ALKHomeBuildingActor::HasIllustration() const { return Illustration && Illustration->GetSprite(); }

void ALKHomeBuildingActor::RefreshLabel()
{
	if (!NameLabel) { return; }
	const FString LevelText = bUpgradable ? FString::Printf(TEXT(" Lv%d"), DisplayedLevel) : FString();
	NameLabel->SetText(FText::FromString(FString::Printf(TEXT("%s%s"), *DisplayName.ToString(), *LevelText)));
	if (!LabelWidget && GetWorld())
	{
		LabelWidget = CreateWidget<ULKHomeBuildingLabelWidget>(GetWorld(), ULKHomeBuildingLabelWidget::StaticClass());
		if (ScreenLabel && LabelWidget) { ScreenLabel->SetWidget(LabelWidget); }
	}
	if (LabelWidget)
	{
		const ELKHomeBuilding Kind = LKHomeContent::BuildingFromId(BuildingId);
		const TCHAR* Hint = bUpgradable ? TEXT("点击查看与升级") : Kind == ELKHomeBuilding::Gate ? TEXT("确认战备 · 开始远征")
			: Kind == ELKHomeBuilding::WarRoom ? TEXT("三名英雄 · 配置牌组") : TEXT("点击查看已解锁内容");
		LabelWidget->SetLabel(FText::FromString(DisplayName.ToString() + LevelText), FText::FromString(Hint), LKPresentationStyle::Gold());
	}
}

void ALKHomeBuildingActor::SetDisplayedLevel(int32 InLevel)
{
	if (DisplayedLevel == FMath::Max(1, InLevel)) { return; }
	DisplayedLevel = FMath::Max(1, InLevel);
    ApplyPlaceholderAppearance();
    // Reapply the hover state after swapping only the illustration.
    const bool bRestoreHighlight = bHighlighted;
    bHighlighted = false;
    SetHighlighted(bRestoreHighlight);
	RefreshLabel();
}

void ALKHomeBuildingActor::SetHighlighted(bool bInHighlighted)
{
	if (bHighlighted == bInHighlighted) { return; }
	bHighlighted = bInHighlighted;
	if (HasIllustration())
	{
		Illustration->SetRelativeScale3D(FVector(bHighlighted ? 1.035f : 1.f));
		Illustration->SetSpriteColor(bHighlighted ? FLinearColor(1.08f, 1.06f, 1.f) : FLinearColor::White);
	}

	FLinearColor Color = LKHomeUIStyle::Accent(BuildingId);
	if (bHighlighted) { Color = Color * 1.6f + PlaceholderHighlight * 0.35f; }

	if (PlaceholderMaterial) { PlaceholderMaterial->SetVectorParameterValue(TEXT("Color"), Color); }
	if (PlaceholderMesh)
	{
		const float ScaleBoost = bHighlighted ? 1.08f : 1.f;
		PlaceholderMesh->SetRelativeScale3D(FVector(PlaceholderSize.X / 100.f, PlaceholderSize.Y / 100.f, PlaceholderSize.Z / 100.f) * ScaleBoost);
	}
	if (NameLabel)
	{
		NameLabel->SetTextRenderColor((bHighlighted ? FLinearColor(1.f, 0.86f, 0.45f, 1.f) : LabelColor).ToFColor(true));
	}
}
