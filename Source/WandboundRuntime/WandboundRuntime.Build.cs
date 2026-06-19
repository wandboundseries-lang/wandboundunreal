// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundRuntime : ModuleRules
{
	public WandboundRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "WandboundCore" });
	}
}
