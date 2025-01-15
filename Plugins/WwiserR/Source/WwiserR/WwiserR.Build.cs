// Copyright Yoerik Roevens. All Rights Reserved.(c)

using UnrealBuildTool;
using System.IO;

public class WwiserR : ModuleRules
{
	public WwiserR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		string parentDirectory = Path.GetFullPath(Path.Combine(ModuleDirectory, ".."));
		string scriptPath = Path.Combine(parentDirectory, "remove_trailing_whitespace.py");

		string sourceDirectory = Path.GetDirectoryName(ModuleDirectory);
		var allDirectories = Directory.GetDirectories(sourceDirectory, "*", SearchOption.AllDirectories);

		PublicIncludePaths.AddRange(allDirectories);
		PrivateIncludePaths.AddRange(allDirectories);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"AkAudio"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"WwiseSoundEngine",
				"ApplicationCore",
				"DeveloperSettings"
			}
			);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"WwiserR_Editor"
				}
				);
		}


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
