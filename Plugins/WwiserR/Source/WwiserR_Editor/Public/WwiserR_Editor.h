// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FWwiserREditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;

protected:
	static void OnAssetRegistryFilesLoaded();

	static void EnsurePluginContentIsInAlwaysCook();
	static bool SaveConfigFile(UObject* ConfigObject);
};
