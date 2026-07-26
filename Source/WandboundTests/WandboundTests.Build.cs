// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundTests : ModuleRules
{
	public WandboundTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "Niagara", "UMG", "Slate", "SlateCore", "WandboundCore", "WandboundRuntime" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}
	}
}
