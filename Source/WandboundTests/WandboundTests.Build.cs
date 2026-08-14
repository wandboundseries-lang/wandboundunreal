// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundTests : ModuleRules
{
	public WandboundTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Test fixtures intentionally use file-local helper names extensively.
		bUseUnity = false;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "Niagara", "UMG", "Slate", "SlateCore", "WandboundCore", "WandboundCardDB", "WandboundRuntime" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("AssetRegistry");
			PrivateDependencyModuleNames.Add("WandboundEditor");
		}
	}
}
