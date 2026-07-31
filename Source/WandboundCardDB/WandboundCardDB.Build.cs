// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WandboundCardDB : ModuleRules
{
	public WandboundCardDB(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "WandboundCore" });
		PrivateDependencyModuleNames.Add("Json");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		string[] RuntimeDataFiles =
		{
			"ProductionCardDB.schema.json",
			"ProductionManifest.schema.json",
			"ProductionMatchSpec.schema.json",
			"ActiveFormat.schema.json",
			"GameStartAddendum.schema.json",
			"Production/InitialCanonical/root_manifest.json",
			"Production/InitialCanonical/bundle_manifest.json",
			"Production/InitialCanonical/definitions/characters.json",
			"Production/InitialCanonical/definitions/npcs.json",
			"Production/InitialCanonical/definitions/traps.json",
			"Production/InitialCanonical/bundle_lock.json",
			"Production/InitialCanonical/match_status.json",
			"Production/InitialCanonical/active_format_v1.json",
			"Production/InitialCanonical/game_start_addendum_v1.json",
			"Production/InitialCanonical/match_spec.json",
			"Production/InitialCanonical/README.md"
		};
		foreach (string RelativePath in RuntimeDataFiles)
		{
			RuntimeDependencies.Add(
				"$(ProjectDir)/Data/CardDB/" + RelativePath,
				StagedFileType.NonUFS);
		}
	}
}
