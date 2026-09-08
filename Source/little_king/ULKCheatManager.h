#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ULKCheatManager.generated.h"

class ALKBattleGameMode;

/**
 * 调试命令（PIE 中按 `~` 打开控制台输入）：
 *   AddSilver 10                给玩家加银币
 *   DrawCard                    玩家抽一张牌
 *   SpawnUnit 单位ID 阵营 X Y   生成单位（阵营 0=玩家 1=敌方）
 *     半场约定：玩家 Y<0（左）、敌方 Y>0（右）；写反会自动镜像到本阵营半场并提示
 *     例：SpawnUnit Unit_Swordsman 1 0 300   → 敌方半场
 *         SpawnUnit Unit_Swordsman 0 0 -300  → 玩家半场
 *   KillAll 阵营                杀死某阵营全部单位（0=玩家 1=敌方）
 *   WinMatch 阵营               直接结束对局并指定胜者
 *   StartBattle                 全部英雄部署后开始（不能绕过门槛）
 *   ListUnits                   列出场上所有单位
 *   InvulnerableHeroes 秒数     让己方（玩家）在场英雄无敌 N 秒（省略 = 10 秒；
 *                               无敌免疫敌方伤害，但超时虚弱仍会扣血——可用来拖到超时观察虚弱）
 *   RunHeroMaxHealth 英雄ID 数值  房间间修改英雄永久基础最大生命
 *   RunHeroTrait 英雄ID 特性ID 0/1 房间间删除/添加英雄永久特性
 *   ListRunState               列出远征、当前遭遇奖励档和英雄永久状态
 */
UCLASS()
class ULKCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void AddSilver(float Amount);

	UFUNCTION(Exec)
	void DrawCard();

	UFUNCTION(Exec)
	void SpawnUnit(const FString& UnitId, int32 TeamIdx, float X, float Y);

	UFUNCTION(Exec)
	void KillAll(int32 TeamIdx);

	UFUNCTION(Exec)
	void WinMatch(int32 TeamIdx);

	UFUNCTION(Exec)
	void StartBattle();

	UFUNCTION(Exec)
	void ListUnits();

	UFUNCTION(Exec)
	void InvulnerableHeroes(float Seconds);

	/** 弹道对象池状态（总量/在飞数；对局结束应归零） */
	UFUNCTION(Exec)
	void ProjectilePool();

	/** 仅调试：修改己方指定英雄特性，Enabled 0 删除 / 1 添加。 */
	UFUNCTION(Exec) void HeroTrait(const FString& HeroId, const FString& TraitId, int32 Enabled);
	/** 仅调试：对首个匹配单位施加无来源、穿透无敌的伤害。 */
	UFUNCTION(Exec) void DamageUnit(const FString& UnitId, int32 TeamIdx, float Amount);
	/** 部署阶段切换亡灵测试遭遇：Patrol / Elite / Boss。 */
	UFUNCTION(Exec) void UndeadEncounter(const FString& Preset);
	/** 仅调试：结算后的房间间阶段修改英雄永久基础最大生命。 */
	UFUNCTION(Exec) void RunHeroMaxHealth(const FString& HeroId, float NewBaseMaxHealth);
	/** 仅调试：结算后的房间间阶段添加/删除永久特性。 */
	UFUNCTION(Exec) void RunHeroTrait(const FString& HeroId, const FString& TraitId, int32 Enabled);
	/** 输出当前远征、房间、遭遇奖励档和英雄永久状态。 */
	UFUNCTION(Exec) void ListRunState();

private:
	ALKBattleGameMode* GetGameMode() const;
};
