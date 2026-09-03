// Copyright Epic Games, Inc. All Rights Reserved.

#include "little_king.h"
#include "Modules/ModuleManager.h"
#include "GameplayTagsManager.h"

class Flittle_kingModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// 注册英雄技能激活标签：BP 技能资产（GA_*）的 AbilityTags 里填 LK.Ability 即可命中，
		// 无需手动去 Project Settings -> GameplayTags 配置（编辑器里标签选择器也能直接看到它）。
		UGameplayTagsManager::Get().AddNativeGameplayTag(
			TEXT("LK.Ability"),
			TEXT("英雄自动施放技能的激活标签（Sprint 3）"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(Flittle_kingModule, little_king, "little_king");
