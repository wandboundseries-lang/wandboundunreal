// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundRuntime : ModuleRules
{
	public WandboundRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Niagara", "UMG", "Slate", "SlateCore", "WandboundCore", "WandboundCardDB" });
		PrivateDependencyModuleNames.Add("Json");
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/ProductionMatchReplay.schema.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/production_replay_smoke_match_spec.json",
			StagedFileType.NonUFS);
	}
}
