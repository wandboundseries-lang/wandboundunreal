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
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/TerminalFixture/root_manifest.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/TerminalFixture/bundle_manifest.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/TerminalFixture/units.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/TerminalFixture/markers.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/TerminalFixture/match_spec.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridReplacementFixture/root_manifest.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridReplacementFixture/bundle_manifest.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridReplacementFixture/units.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridReplacementFixture/markers.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridReplacementFixture/match_spec.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridNonHeroFixture/root_manifest.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridNonHeroFixture/bundle_manifest.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridNonHeroFixture/units.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridNonHeroFixture/markers.json",
			StagedFileType.NonUFS);
		RuntimeDependencies.Add(
			"$(ProjectDir)/Data/Replay/HybridNonHeroFixture/match_spec.json",
			StagedFileType.NonUFS);
	}
}
