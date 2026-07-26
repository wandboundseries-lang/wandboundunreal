// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
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

		string CardDBDataRoot = Path.GetFullPath(
			Path.Combine(ModuleDirectory, "..", "..", "Data", "CardDB"));
		if (Directory.Exists(CardDBDataRoot))
		{
			foreach (string SourceFile in Directory.GetFiles(
				CardDBDataRoot,
				"*",
				SearchOption.AllDirectories))
			{
				string RelativePath = SourceFile.Substring(CardDBDataRoot.Length + 1)
					.Replace('\\', '/');
				RuntimeDependencies.Add(
					"$(ProjectDir)/Data/CardDB/" + RelativePath,
					StagedFileType.NonUFS);
			}
		}
	}
}
