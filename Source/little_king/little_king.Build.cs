// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class little_king : ModuleRules
{
	public little_king(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 关闭本模块的 Unity 合并编译：本项目多个 .cpp 各自定义了同名的文件内辅助符号
		// （MakeText/StyleButton/配色常量等），Unity 把它们合进同一 TU 会报"重定义"。
		// 关闭后每个 .cpp 独立编译，新增文件也不会再因合并分组变化触发同类错误。
		bUseUnity = false;

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
