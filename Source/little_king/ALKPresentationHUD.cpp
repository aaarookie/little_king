#include "ALKPresentationHUD.h"
#include "ULKUnitAnimationComponent.h"
#include "LKCardPresentation.h"
#include "ULKUnitStatusComponent.h"
#include "ALKBattleGameMode.h"
#include "ALKPlayerController.h"
#include "ALKHeroCamp.h"
#include "ALKUnitHero.h"
#include "ALKProjectile.h"
#include "ULKUnitPassiveComponent.h"
#include "ULKGameData.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PaperSpriteComponent.h"
#include "ULKPresentationSubsystem.h"
#include "LKPresentationStyle.h"

bool ALKPresentationHUD::ProjectPoint(const FVector& Location, FVector2D& Point) const
{
	const APlayerController* PC = GetOwningPlayerController();
	return PC && PC->ProjectWorldLocationToScreen(Location, Point, true);
}

void ALKPresentationHUD::DrawWorldCircle(const FVector& Center, float Radius, FLinearColor Color, float Thickness)
{
	constexpr int32 Segments = 48;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A = i * 2.f * PI / Segments, B = (i + 1) * 2.f * PI / Segments;
		FVector2D P, Q;
		if (ProjectPoint(Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius, P)
			&& ProjectPoint(Center + FVector(FMath::Cos(B), FMath::Sin(B), 0.f) * Radius, Q))
		{
			DrawLine(P.X, P.Y, Q.X, Q.Y, Color, Thickness);
		}
	}
}

void ALKPresentationHUD::DrawHUD()
{
	Super::DrawHUD();
	ALKPlayerController* PC = Cast<ALKPlayerController>(GetOwningPlayerController());
	ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
	if (!Canvas || !PC || !GM || !GM->GetGameData()) { return; }
	if (!bBound)
	{
		PC->OnPlayResult.AddDynamic(this, &ALKPresentationHUD::HandleResult);
		GM->OnDamageEvent.AddDynamic(this, &ALKPresentationHUD::HandleDamage);
		bBound = true;
	}
	const float Scale = FMath::Clamp(Canvas->ClipX / 1920.f, 0.75f, 1.5f);
    ULKPresentationSubsystem* Presentation = GetWorld()->GetSubsystem<ULKPresentationSubsystem>();
    if (GM->GetPhase() == ELKGamePhase::Result) { Presentation->ClearEffects(); }
    else { Presentation->Draw(this); }
	// Y=0 是规则使用的半场边界；Canvas 投影不会被地面遮挡，也不依赖 Debug 开关。
	FVector2D LineStart, LineEnd;
	const float HalfWidth = GM->GetGameData()->FieldHalfWidth;
    const float FieldHalfHeight=GM->GetGameData()->FieldHalfHeight;
    const FVector FieldCorners[]={{-HalfWidth,-FieldHalfHeight,0},{HalfWidth,-FieldHalfHeight,0},{HalfWidth,FieldHalfHeight,0},{-HalfWidth,FieldHalfHeight,0}};
    for (int I=0;I<4;++I)
    {
        FVector2D A,B;
        if (ProjectPoint(FieldCorners[I],A) && ProjectPoint(FieldCorners[(I+1)%4],B))
        { DrawLine(A.X,A.Y,B.X,B.Y,LKPresentationStyle::Gold(),2.f*Scale); }
    }
	if (ProjectPoint(FVector(-HalfWidth, 0.f, 0.f), LineStart) && ProjectPoint(FVector(HalfWidth, 0.f, 0.f), LineEnd))
	{
		DrawLine(LineStart.X, LineStart.Y, LineEnd.X, LineEnd.Y, FLinearColor::Yellow, 2.f * Scale);
	}
	for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
	{
		ALKUnitBase* Unit = *It;
		if (!Unit->IsAlive() && !Unit->IsHero()) { continue; }
		FVector2D Screen;
		if (!ProjectPoint(Unit->GetActorLocation(), Screen)) { continue; }
		const FLinearColor TeamColor = Unit->GetTeam() == ELKTeam::Player ? FLinearColor(0.3f, 0.85f, 0.55f) : FLinearColor(0.95f, 0.35f, 0.35f);
		if (Unit->IsIncapacitated())
		{
			DrawRect(FLinearColor(0.3f, 0.3f, 0.3f), Screen.X - 9.f * Scale, Screen.Y - 9.f * Scale, 18.f * Scale, 18.f * Scale);
			DrawText(TEXT("失能"), FLinearColor::Gray, Screen.X - 14.f * Scale, Screen.Y - 30.f * Scale, nullptr, Scale);
			continue;
		}
		if (ALKHeroCamp* Camp = Cast<ALKHeroCamp>(Unit))
		{
			const ALKUnitHero* Hero = Camp->GetHero();
			const bool bAlive = Hero && Hero->IsAlive();
			const FLinearColor CampColor = bAlive ? TeamColor : FLinearColor(0.4f, 0.4f, 0.4f);
			DrawWorldCircle(Unit->GetActorLocation(), Unit->GetBodyRadius(), CampColor, 3.f);
            if (UPaperSpriteComponent* CampSprite = Camp->GetSpriteComponent(); CampSprite && CampSprite->GetSprite())
            { CampSprite->SetSpriteColor(bAlive ? FLinearColor::White : FLinearColor(.45f,.45f,.45f)); }
            else
            {
                DrawLine(Screen.X - 9.f * Scale, Screen.Y + 5.f * Scale, Screen.X, Screen.Y - 10.f * Scale, CampColor, 3.f);
                DrawLine(Screen.X, Screen.Y - 10.f * Scale, Screen.X + 9.f * Scale, Screen.Y + 5.f * Scale, CampColor, 3.f);
            }
			if (PC->GetSelectedCamp() == Camp && bAlive) { DrawWorldCircle(Hero->GetCampCenter(), Hero->GetCampMoveRadius(), FLinearColor(1.f, 0.8f, 0.2f), 2.f); }
			continue; // 营地没有血条。
		}
		UPaperSpriteComponent* Sprite = Unit->GetSpriteComponent();
		float Top = Screen.Y - 15.f * Scale;
		if (Sprite && Sprite->GetSprite())
		{
			const FBox Bounds = Unit->GetAnimationComponent()->GetIdleBounds().GetBox();
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector Point((Corner & 1) ? Bounds.Max.X : Bounds.Min.X, (Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
				FVector2D Projected;
				if (ProjectPoint(Point, Projected)) { Top = FMath::Min(Top, float(Projected.Y) - 5.f * Scale); }
			}
		}
		else
		{
			const FLinearColor Color = Unit->GetPlaceholderColor().A > 0.f ? Unit->GetPlaceholderColor() : TeamColor;
			const float Size = (Unit->IsBoss() ? 28.f : (Unit->IsHero() ? 22.f : 14.f)) * Scale;
			DrawRect(Color, Screen.X - Size * 0.5f, Screen.Y - Size * 0.5f, Size, Size);
			if (Unit->GetPlaceholderColor().A > 0.f)
			{
				DrawText(FString::Printf(TEXT("%s·%s"), *Unit->GetDisplayName().ToString(), *LKCardPresentation::QualityName(Unit->GetQuality()).ToString()),
                    LKCardPresentation::QualityColor(Unit->GetQuality()), Screen.X - Size, Screen.Y + Size * 0.6f, nullptr, 0.8f * Scale);
			}
		}
		const float Width = (Unit->IsHero() ? 52.f : 36.f) * Scale;
		const float Ratio = FMath::Clamp(Unit->GetHealth() / FMath::Max(1.f, Unit->GetMaxHealth()), 0.f, 1.f);
		DrawRect(FLinearColor(0.03f, 0.03f, 0.03f, 0.9f), Screen.X - Width * 0.5f - 1.f, Top - 1.f, Width + 2.f, 6.f * Scale);
		DrawRect(TeamColor, Screen.X - Width * 0.5f, Top, Width * Ratio, 4.f * Scale);
		if (Unit->GetPassiveComponent()->GetAbility() == ELKPassiveAbility::BoneRegeneration)
		{
			DrawText(FString::Printf(TEXT("%d / %d"), Unit->GetPassiveComponent()->GetBoneCount(), Unit->GetPassiveComponent()->GetRevivalThreshold()),
				FLinearColor::Yellow, Screen.X - Width * 0.5f, Top - 18.f * Scale, nullptr, 0.85f * Scale);
		}
        if (Unit->IsUnderFocusWarning())
        {
            DrawText(TEXT("!"), FLinearColor::Yellow, Screen.X - 3.f * Scale, Top - 25.f * Scale, nullptr, 1.3f * Scale);
            DrawWorldCircle(Unit->GetActorLocation(), Unit->GetBodyRadius()+14.f,FLinearColor(1.f,.4f,.22f),2.f);
        }
        if (Unit->IsTaunting())
        {
            const FLinearColor Gold(1.f,.75f,.2f);
            DrawWorldCircle(Unit->GetActorLocation(), Unit->GetBodyRadius()+6.f, Gold);
            const FVector2D P(Screen.X+Width*.6f, Top+10.f);
            const FVector2D Points[]={{-5,-6},{5,-6},{5,2},{0,7},{-5,2},{-5,-6}};
            for (int I=0;I<5;++I) { DrawLine(P.X+Points[I].X*Scale,P.Y+Points[I].Y*Scale,P.X+Points[I+1].X*Scale,P.Y+Points[I+1].Y*Scale,Gold,2.f); }
        }
        const ULKUnitStatusComponent* Status = Unit->GetStatusComponent();
        const float Clock = GetWorld()->GetTimeSeconds();
        if (Status->IsFrozen())
        {
            for (int I=0;I<6;++I)
            {
                const float A=I*PI/3.f,B=(I+1)*PI/3.f;
                const float R=22.f*Scale;
                DrawLine(Screen.X+FMath::Cos(A)*R,Screen.Y+FMath::Sin(A)*R,Screen.X+FMath::Cos(B)*R,Screen.Y+FMath::Sin(B)*R,FLinearColor(.5f,.85f,1.f,.8f),2.f);
            }
        }
        if (Status->IsStunned())
        {
            for (int I=0;I<3;++I)
            {
                const float A=Clock*2.f+I*2.f*PI/3.f;
                const FVector2D P(Screen.X+FMath::Cos(A)*20.f*Scale,Top-8.f*Scale+FMath::Sin(A)*5.f*Scale);
                DrawLine(P.X-3*Scale,P.Y,P.X+3*Scale,P.Y,FLinearColor(1.f,.85f,.35f),2);
                DrawLine(P.X,P.Y-3*Scale,P.X,P.Y+3*Scale,FLinearColor(1.f,.85f,.35f),2);
            }
        }
        if (Status->GetBurnStacks()>0)
        {
            for (int I=0;I<FMath::Min(5,Status->GetBurnStacks());++I)
            {
                const float X=Screen.X+(I-2)*6.f*Scale,Y=Screen.Y+12.f*Scale;
                const float H=(9.f+3.f*FMath::Sin(Clock*5.f+I))*Scale;
                DrawLine(X-3*Scale,Y,X,Y-H,FLinearColor(1.f,.38f,.12f),2);
                DrawLine(X,Y-H,X+3*Scale,Y,FLinearColor(1.f,.72f,.25f),2);
            }
        }
        if (Status->IsEmpowered())
        {
            DrawWorldCircle(Unit->GetActorLocation(),Unit->GetBodyRadius()+10.f,FLinearColor(.45f,.85f,.40f,.65f),2);
        }
        FString StatusLabel;
        if (Status->IsStunned()) { StatusLabel += TEXT("眩晕 "); }
        if (Status->IsFrozen()) { StatusLabel += TEXT("冰冻 "); }
        if (Status->IsEmpowered()) { StatusLabel += FString::Printf(TEXT("强化 %.0f%% "), Status->GetStunMeter() * 100.f); }
        if (Status->GetBurnStacks() > 0) { StatusLabel += FString::Printf(TEXT("点燃×%d"), Status->GetBurnStacks()); }
        if (!StatusLabel.IsEmpty()) { DrawText(StatusLabel, FLinearColor(1.f,.8f,.25f), Screen.X - 28.f * Scale, Top - 17.f * Scale, nullptr, .75f * Scale); }
		if (GM->GetGameData()->bDrawTeamRing) { DrawWorldCircle(Unit->GetActorLocation(), Unit->GetBodyRadius(), TeamColor); }
	}
	for (TActorIterator<ALKProjectile> It(GetWorld()); It; ++It)
	{
		if (!(*It)->IsPooledActive()) { continue; }
		FVector2D A, B;
		const FVector Head = (*It)->GetActorLocation();
		if (ProjectPoint(Head, A) && ProjectPoint(Head - (*It)->GetFlightDirection() * 65.f, B))
		{
            const FLinearColor ShotColor = It->GetBreathHead() == ELKBreathHead::Ice ? FLinearColor(.3f,.8f,1.f)
                : (It->GetBreathHead() == ELKBreathHead::Fire ? FLinearColor(1.f,.3f,.1f) : FLinearColor(1.f,.83f,.3f));
            const FVector2D Dir=(A-B).GetSafeNormal(), Side(-Dir.Y,Dir.X);
            if (It->IsSiegeShot())
            {
                DrawLine(A.X,A.Y,B.X,B.Y,FLinearColor(.7f,.64f,.5f,.65f),4.f*Scale);
                const FVector2D P[]={A+Side*5.f*Scale,A+Dir*6.f*Scale,A-Side*5.f*Scale,A-Dir*6.f*Scale,A+Side*5.f*Scale};
                for (int I=0;I<4;++I) { DrawLine(P[I].X,P[I].Y,P[I+1].X,P[I+1].Y,FLinearColor(.92f,.86f,.70f),3.f*Scale); }
            }
            else
            {
                const bool Breath=It->GetBreathHead()!=ELKBreathHead::None;
                DrawLine(A.X,A.Y,B.X,B.Y,ShotColor,(Breath?5.f:2.f)*Scale);
                const FVector2D L=A-Dir*9.f*Scale+Side*4.f*Scale,R=A-Dir*9.f*Scale-Side*4.f*Scale;
                DrawLine(A.X,A.Y,L.X,L.Y,ShotColor,2.f*Scale); DrawLine(A.X,A.Y,R.X,R.Y,ShotColor,2.f*Scale);
                if (Breath) { DrawWorldCircle(Head,18.f,ShotColor,2.f); }
            }
		}
	}
	FVector Preview; float Radius = 0.f, AttackRadius = 0.f; bool bValid = false;
	if (PC->GetPlacementPreview(Preview, Radius, bValid, &AttackRadius))
	{
		DrawWorldCircle(Preview, Radius, bValid ? FLinearColor::Green : FLinearColor::Red, 2.f);
		if (AttackRadius > 0.f)
		{
            const FLinearColor RangeColor = bValid ? FLinearColor(1.f,.75f,.15f) : FLinearColor::Red;
            const float HalfHeight = GM->GetGameData()->FieldHalfHeight;
            if (AttackRadius >= FVector2D(HalfWidth, HalfHeight).Size() * 2.f)
            {
                const FVector Corners[] = { {-HalfWidth,-HalfHeight,0}, {HalfWidth,-HalfHeight,0}, {HalfWidth,HalfHeight,0}, {-HalfWidth,HalfHeight,0} };
                for (int32 Edge = 0; Edge < 4; ++Edge)
                {
                    FVector2D A, B;
                    if (ProjectPoint(Corners[Edge], A) && ProjectPoint(Corners[(Edge + 1) % 4], B)) { DrawLine(A.X,A.Y,B.X,B.Y,RangeColor,2.f * Scale); }
                }
                DrawText(TEXT("全图射程 · 仅攻击敌方建筑"), RangeColor, 24.f * Scale, 90.f * Scale, nullptr, Scale);
            }
            else { DrawWorldCircle(Preview, AttackRadius, RangeColor, 1.5f * Scale); }
		}
	}
	const float Delta = GetWorld()->GetDeltaSeconds();
	for (int32 i = DamageTexts.Num() - 1; i >= 0; --i)
	{
		FDamageText& Text = DamageTexts[i]; Text.Remaining -= Delta;
		if (Text.Remaining <= 0.f || GM->GetPhase() == ELKGamePhase::Result) { DamageTexts.RemoveAtSwap(i); continue; }
		FVector2D Point;
		if (ProjectPoint(Text.WorldLocation, Point))
		{
			FLinearColor Color = Text.bHeal ? FLinearColor::Green : FLinearColor::White;
			Color.A = FMath::Min(1.f, Text.Remaining * 3.f);
			DrawText(FString::Printf(TEXT("%s%.0f"), Text.bHeal ? TEXT("+") : TEXT("-"), Text.Amount), Color, Point.X, Point.Y - (1.f - Text.Remaining) * 45.f * Scale, nullptr, Scale);
		}
	}
	TipRemaining -= Delta;
	if (TipRemaining > 0.f) { DrawText(Tip, FLinearColor::Yellow, Canvas->ClipX * 0.32f, Canvas->ClipY * 0.16f, nullptr, Scale); }
	if (GM->GetPhase() == ELKGamePhase::Deployment)
	{
        DrawRect(LKPresentationStyle::Panel(),16.f*Scale,22.f*Scale,630.f*Scale,34.f*Scale);
        DrawText(FString::Printf(TEXT("部署英雄 %d / %d；全部部署后点击「开始」或按空格/回车"), GM->GetDeployedPlayerHeroCount(), GM->GetRequiredHeroCount()), LKPresentationStyle::Paper(), 24.f * Scale, 30.f * Scale, nullptr, Scale);
		if (!GM->HasValidDecks())
		{
			DrawText(TEXT("牌库配置无效：每方至少 HandSize + 1 种卡，且均需加入 CardLibrary"), FLinearColor::Yellow, 24.f * Scale, 60.f * Scale, nullptr, Scale);
		}
	}
	else if (GM->GetPhase() == ELKGamePhase::Battle && !GM->IsAnyUnitCombatEnabled())
	{
		// 兜底提示：战斗阶段却没有任何单位处于可战斗状态（面对面不攻击类问题一眼可见）。
		DrawText(TEXT("战斗已开始但单位均被冻结：请查看日志 [Battle] 无法进入战斗阶段"), FLinearColor::Red, 24.f * Scale, 30.f * Scale, nullptr, Scale);
	}
}

void ALKPresentationHUD::HandleDamage(FVector Location, float Amount, bool bHeal)
{
	const ALKBattleGameMode* GM = GetWorld()->GetAuthGameMode<ALKBattleGameMode>();
	if (GM && GM->GetGameData()->bNativeDamageText && Amount > 0.f)
	{
		if (DamageTexts.Num() >= 96) { DamageTexts.RemoveAt(0); }
		DamageTexts.Add({ Location, Amount, bHeal, 0.8f });
	}
}

void ALKPresentationHUD::HandleResult(ELKPlayResult Result)
{
	switch (Result)
	{
	case ELKPlayResult::Success: return;
	case ELKPlayResult::DeploymentIncomplete: Tip = TEXT("请先部署全部 3 名英雄"); break;
	case ELKPlayResult::SpellLocked: Tip = TEXT("没有全场施法特性：法术只能放在己方半场"); break;
	case ELKPlayResult::HeroMoveBlocked: Tip = TEXT("目标地点不可达：被阻挡或为建筑/营地"); break;
	case ELKPlayResult::NotEnoughSilver: Tip = TEXT("银币不足"); break;
	case ELKPlayResult::AlreadyDeployed: Tip = TEXT("该英雄已部署，请点击营地指挥"); break;
	case ELKPlayResult::UnitLimitReached: Tip = TEXT("单位数量达到上限"); break;
	case ELKPlayResult::InvalidCardData: Tip = TEXT("数据配置无效，请查看日志和 Sprint 5 迁移教程"); break;
	case ELKPlayResult::HandEmpty: Tip = TEXT("手牌尚未就绪，请检查牌库配置"); break;
	case ELKPlayResult::WrongPhase: Tip = TEXT("当前阶段不能执行此操作"); break;
	default: Tip = TEXT("不能放在这里：检查范围、占用或建筑上限"); break;
	}
	TipRemaining = 2.5f;
}
