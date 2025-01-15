// Copyright Epic Games, Inc. All Rights Reserved.

#include "WwiserR_Editor.h"
#include "Settings/ProjectPackagingSettings.h"
#include "ISourceControlModule.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FWwiserREditorModule"

void FWwiserREditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFilesLoaded().AddStatic(&FWwiserREditorModule::OnAssetRegistryFilesLoaded);
}

void FWwiserREditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FWwiserREditorModule::OnAssetRegistryFilesLoaded()
{
	EnsurePluginContentIsInAlwaysCook();
}

void FWwiserREditorModule::EnsurePluginContentIsInAlwaysCook()
{
	UProjectPackagingSettings* packagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	bool packageSettingsNeedUpdate = false;

	TArray<FString> PathsToCheck = { TEXT("/WwiserR/Config"), TEXT("/WwiserR/AnimNotifies"), TEXT("/WwiserR/Blueprints") };

	for (auto pathToCheck : PathsToCheck)
	{
		if (!packagingSettings->DirectoriesToAlwaysCook.ContainsByPredicate(
			[pathToCheck](FDirectoryPath PathInArray) { return PathInArray.Path == pathToCheck; })
			)
		{
			FDirectoryPath newPath;
			newPath.Path = pathToCheck;

			packagingSettings->DirectoriesToAlwaysCook.Add(newPath);
			packageSettingsNeedUpdate = true;
		}
	}

	if (packageSettingsNeedUpdate)
	{
		SaveConfigFile(packagingSettings);
	}
}

bool FWwiserREditorModule::SaveConfigFile(UObject* ConfigObject)
{
	const FString configFilename = ConfigObject->GetDefaultConfigFilename();
	if (ISourceControlModule::Get().IsEnabled())
	{
		if (!SettingsHelpers::IsCheckedOut(configFilename, true))
		{
			if (!SettingsHelpers::CheckOutOrAddFile(configFilename, true))
			{
				return false;
			}
		}
	}

#if ENGINE_MAJOR_VERSION >= 5
	return ConfigObject->TryUpdateDefaultConfigFile();
#else
	ConfigObject->UpdateDefaultConfigFile();
	return true;
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWwiserREditorModule, WwiserR_Editor)
