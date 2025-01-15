// Copyright Yoerik Roevens. All Rights Reserved.(c)

using UnrealBuildTool;
using System.IO;

public class WwiserR_Editor : ModuleRules
{
	public WwiserR_Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        string sourceDirectory = Path.GetDirectoryName(ModuleDirectory);
        var allDirectories = Directory.GetDirectories(sourceDirectory, "*", SearchOption.AllDirectories);

        PublicIncludePaths.AddRange(allDirectories);
        PrivateIncludePaths.AddRange(allDirectories);

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "SharedSettingsWidgets",
                "DeveloperToolSettings",
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "UnrealEd",
                "DeveloperSettings"
			}
			);
	}
}
