// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/AudioUtils.h"
#include "Config/DebugTheme.h"
#include "AudioConfig.generated.h"


/**
 * Game Configuration
 */
UCLASS(Config = Game, defaultconfig, ClassGroup = "WwiserR", meta = (DisplayName = "WwiserR Config"))
class WWISERR_API UWwiserRGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/* Audio Subsystem Blueprint Class */
	//UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, NoClear, Category = "Audio Systems")
	//TSubclassOf<class UAudioSubsystem> AudioSubsystemClass;

	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Audio Systems")
	TSubclassOf<class UMusicManager> MusicManagerClass;

	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Audio Systems - Overrides")
	TSubclassOf<class USoundListenerManager> ListenerManagerClass;

	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Audio Settings")
	bool bMuteWhenGameNotInForeground = true;

	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Audio Globals", meta = (AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath MuteAllEvent;

	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Audio Globals", meta = (AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath UnmuteAllEvent;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Static Sound Manager")
	bool bSpreadOverMultipleFrames = true;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Static Sound Manager", meta = (EditCondition = "bSpreadOverMultipleFrames"))
	TArray<float> DistanceTresholds{5000.f, 10000.f};

	/** aux bus without listener relative routing to allow crossfading when passing through portals **/
	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Ambient Bed Manager", meta = (AllowedClasses = "/Script/AkAudio.AkAuxBus"))
	FSoftObjectPath DefaultAmbientBedPassthroughBuss;
};

/**
 * Editor Configuration
 */
UCLASS(Config = EditorPerProjectUserSettings, ClassGroup = "WwiserR", meta = (DisplayName = "WwiserR - Visual Themes"))
class WWISERR_API UWwiserRThemeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/* Visual Theme - Listener Manager */
	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Visual Themes")
	FThemeListenerManager ThemeListenerManager;

	/* Visual Theme - Sound Emitters */
	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Visual Themes")
	FThemeSoundEmitters ThemeSoundEmitters;

	/* Visual Theme - Static Sound Emitters */
	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Visual Themes")
	FThemeStaticSoundEmitters ThemeStaticSoundEmitters;
};
