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
 *   StartBattle                 跳过部署直接开战
 *   ListUnits                   列出场上所有单位
 *   InvulnerableHeroes 秒数     让己方（玩家）在场英雄无敌 N 秒（省略 = 10 秒；
 *                               无敌免疫敌方伤害，但超时虚弱仍会扣血——可用来拖到超时观察虚弱）
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

private:
	ALKBattleGameMode* GetGameMode() const;
};
