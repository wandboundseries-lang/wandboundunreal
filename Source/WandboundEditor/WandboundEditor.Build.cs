// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundEditor : ModuleRules
{
	public WandboundEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"WandboundCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"AssetTools",
			"InterchangeCore",
			"InterchangeEngine",
			"InterchangeImport",
			"InterchangePipelines",
			"JsonUtilities",
			"Projects",
			"UnrealEd",
			"WandboundRuntime"
		});

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}
