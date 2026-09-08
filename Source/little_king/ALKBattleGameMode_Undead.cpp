#include "ALKBattleGameMode.h"
#include "ALKUnitBase.h"
#include "ALKOpponentBrain.h"
#include "ULKGameData.h"
#include "ULKUnitPassiveComponent.h"
#include "LKEncounterContent.h"
#include "LKGameplayHelpers.h"
#include "LKLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ALKBattleGameMode::ProcessDefeats()
{
    if (bProcessingDefeats) { return; }
    if (Phase != ELKGamePhase::Battle) { PendingDefeats.Reset(); return; }
    TGuardValue<bool> Processing(bProcessingDefeats, true);
    while (!PendingDefeats.IsEmpty() && Phase == ELKGamePhase::Battle)
    {
        // 原始伤害事件已入账。献祭可追加新的失能事件，继续排空同一批次。
        int32 Next = 0;
        while (Next < PendingDefeats.Num())
        {
            ALKUnitBase* Victim = PendingDefeats[Next++];
            if (!IsValid(Victim)) { continue; }
            TArray<ALKUnitBase*> Listeners;
            for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It) { if (!It->IsCamp()) { Listeners.Add(*It); } }
            ALKUnitBase* Summoner = nullptr;
            for (ALKUnitBase* Listener : Listeners)
            {
                if (!IsValid(Listener)) { continue; }
                Listener->GetPassiveComponent()->ObserveDefeat(Victim, PendingDefeats.Contains(Listener));
                if (!Listener->GetPassiveComponent()->CanSummonFrom(Victim)) { continue; }
                const float Distance = FVector::DistSquared2D(Listener->GetActorLocation(), Victim->GetActorLocation());
                const float Best = Summoner ? FVector::DistSquared2D(Summoner->GetActorLocation(), Victim->GetActorLocation()) : MAX_flt;
                if (!Summoner || Distance < Best || (Distance == Best && Listener->GetFName().LexicalLess(Summoner->GetFName()))) { Summoner = Listener; }
            }
            if (Summoner) { Summoner->GetPassiveComponent()->TrySummonFrom(Victim); }
        }
        // 同批骷髅死亡先全部计数，复活完成后才做胜负检查，避免 AoE 遍历顺序改变结局。
        const TArray<TObjectPtr<ALKUnitBase>> Candidates = PendingDefeats;
        PendingDefeats.Reset();
        for (ALKUnitBase* Unit : Candidates)
        {
            if (!IsValid(Unit) || !Unit->GetPassiveComponent()->TryRevive()) { continue; }
            const int32 Side = int32(Unit->GetTeam());
            AliveHeroes[Side].AddUnique(Unit); HeroCounts[Side] = AliveHeroes[Side].Num();
            FLKCombatEvent Revival;
            Revival.Source = LKGameplay::MakeSource(Unit, ELKCombatSourceKind::Revival, "Skill_BoneRegeneration");
            Revival.TargetTeam = Unit->GetTeam(); Revival.TargetUnitId = Unit->GetUnitId(); Revival.TargetInstanceId = Unit->GetFName();
            Revival.Location = Unit->GetActorLocation(); Revival.bIsHeal = true;
            Revival.RequestedAmount = Revival.ActualAmount = Revival.HealthAfter = Unit->GetHealth();
            RecordCombatEvent(Revival);
        }
        // 复活事件的监听者也可能引发伤害，先处理新事件再允许胜负结算。
    }
}

void ALKBattleGameMode::FinalizeHeroRecovery()
{
    BattleOutcome.RunId = BattleContext.RunId; BattleOutcome.NodeId = BattleContext.NodeId;
    BattleOutcome.AttemptId = BattleContext.AttemptId; BattleOutcome.Stats = MatchStats;
    BattleOutcome.PlayerHeroes.Reset(); BattleOutcome.EnemyHeroes.Reset();
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Hero = *It;
        if (!Hero->IsHero()) { continue; }
        const bool bPlayerSide = Hero->GetTeam() == ELKTeam::Player;
        FLKHeroBattleOutcome Snapshot;
        Snapshot.InstanceId = Hero->GetFName();
        Snapshot.bWasIncapacitated = Hero->IsIncapacitated();
        Snapshot.HealthBeforeRecovery = Hero->IsDead() ? 0.f : Hero->GetHealth();
        if (bPlayerSide || !bExpeditionBattle)
        {
            // 跨房继承只存在于玩家英雄：远征房间对玩家执行战后 +40% 恢复。
            // 独立单场（无远征）保留旧的双方恢复，仅供结算展示与调试。
            Hero->RecoverAfterBattle(GameData->HeroPostBattleRecovery);
        }
        else
        {
            // 远征房间的敌方单位状态从不跨房继承：下一房按遭遇配置满血重建。
            // 因此不对敌方执行结算恢复（避免"失能敌方在结算后以 40% 复活站场"的观感，
            // 也让 BattleHistory 里的敌方快照如实记录结算时状态）。
            UE_LOG(LogLKBattle, Verbose, TEXT("[Run] 敌方英雄 %s 跳过战后恢复（敌方不跨房继承，下一房满血重建）"),
                *Hero->GetUnitId().ToString());
        }
        Snapshot.RecoveredState.HeroId = Hero->GetUnitId();
        Snapshot.RecoveredState.Health = Hero->GetHealth();
        Snapshot.RecoveredState.MaxHealth = Hero->GetMaxHealth();
		Snapshot.RecoveredState.BaseMaxHealth = Hero->GetBaseMaxHealth();
        Snapshot.RecoveredState.Traits = Hero->GetTraits();
        (bPlayerSide ? BattleOutcome.PlayerHeroes : BattleOutcome.EnemyHeroes).Add(Snapshot);
    }
    // 结算统计/存活登记保留战斗结束瞬间的含义；恢复不会重新开战或改变胜负。
    BattleOutcome.bFinalized = true;
}

bool ALKBattleGameMode::ConfigureEnemyEncounter(FName EncounterId)
{
	if (Phase != ELKGamePhase::Deployment || !GameData || !OpponentBrain) { return false; }
	TArray<FLKEncounterRow> Catalog;
	FString Error;
	if (!BuildEncounterCatalog(Catalog, Error))
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 目录加载失败：%s"), *Error);
		return false;
	}
	const FLKEncounterRow* Encounter = LKEncounterContent::Find(Catalog, EncounterId);
	if (!Encounter)
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 找不到 %s"), *EncounterId.ToString());
		return false;
	}
	return ApplyEnemyEncounter(*Encounter);
}

bool ALKBattleGameMode::ApplyEnemyEncounter(const FLKEncounterRow& Source)
{
	if (Phase != ELKGamePhase::Deployment || !GameData || !OpponentBrain) { return false; }
	FLKEncounterRow Encounter;
	FString Error;
	if (!LKEncounterContent::NormalizeAndValidate(Source.EncounterId, Source, Encounter, Error))
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 配置无效：%s"), *Error);
		return false;
	}
    for (FName Id : Encounter.EnemyHeroIds)
    {
        const FLKUnitRow* Row = GetUnitRow(Id);
        if (!Row || (Row->UnitClass != ELKUnitClass::Hero && Row->UnitClass != ELKUnitClass::Boss)) { return false; }
    }
	for (const FLKWaveEntry& Wave : Encounter.Waves)
	{
		const FLKUnitRow* Row = GetUnitRow(Wave.UnitId);
		if (!Row || Row->UnitClass == ELKUnitClass::Hero || Row->UnitClass == ELKUnitClass::Boss) { return false; }
	}
	if (Encounter.bEnemyUsesCards)
	{
		if (Encounter.EnemyCards.Num() < GameData->HandSize + 1) { return false; }
		for (FName CardId : Encounter.EnemyCards) { if (!FindCard(CardId)) { return false; } }
	}
    // 仅清理当前部署阶段的敌方预览；不发死亡事件、不触发被动或战后恢复。
    TArray<ALKUnitBase*> OldUnits;
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It) { if (It->GetTeam() == ELKTeam::Enemy) { OldUnits.Add(*It); } }
    for (ALKUnitBase* Unit : OldUnits) { Unit->OnUnitDied.RemoveDynamic(this, &ALKBattleGameMode::HandleUnitDied); Unit->Destroy(); }
    AliveHeroes[1].Reset(); HeroCounts[1] = 0; DeployedHeroes[1].Reset(); BuildingCounts[1].Reset();
	CurrentEncounter = Encounter;
	EnemyHeroIds = Encounter.EnemyHeroIds;
	bEnemyUsesCards = Encounter.bEnemyUsesCards;
	GameData->EnemyEncounterId = Encounter.EncounterId;
	GameData->DefaultEnemyDeck = Encounter.EnemyCards;
	BattleContext.EncounterId = Encounter.EncounterId;
	BattleContext.Encounter = Encounter;
	BattleContext.EnemyHeroIds = Encounter.EnemyHeroIds;
	BattleContext.EnemyCards = Encounter.EnemyCards;
	BattleContext.bEnemyUsesCards = Encounter.bEnemyUsesCards;
	if (!OpponentBrain->ConfigureEncounter(Encounter)) { return false; }
    AutoDeployDefaultHeroes(ELKTeam::Enemy);
	UE_LOG(LogLKBattle, Log, TEXT("[Encounter] %s：敌方英雄%d 波次%d 出牌=%d 奖励档=%d；玩家仍需全部三英雄"),
		*Encounter.EncounterId.ToString(), HeroCounts[1], Encounter.Waves.Num(), int32(Encounter.bEnemyUsesCards), Encounter.RewardTier);
	return HeroCounts[1] == Encounter.EnemyHeroIds.Num();
}
