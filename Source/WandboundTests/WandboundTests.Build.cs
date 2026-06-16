// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundTests : ModuleRules
{
	public WandboundTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Json", "WandboundCore" });
	}
}
