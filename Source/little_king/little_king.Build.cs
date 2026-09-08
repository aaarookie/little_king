// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class little_king : ModuleRules
{
	public little_king(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Paper2D",              // 2D 精灵
			"GameplayAbilities",    // GAS（5.8 起为插件模块）
			"GameplayTags",
			"GameplayTasks",
			"UMG"                   // UI 组件
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
