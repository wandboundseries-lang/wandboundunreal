// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundCore : ModuleRules
{
	public WandboundCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
