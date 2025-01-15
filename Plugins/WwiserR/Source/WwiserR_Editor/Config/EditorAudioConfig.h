// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EditorAudioConfig.generated.h"

UENUM(BlueprintType)
enum class EMuteWhenGameNotInForeground
{
	UseGameSettings,
	MuteWhenGameNotInForeground,
	NeverMute
};

/**
 * Editor Configuration
 */
UCLASS(Config = EditorPerProjectUserSettings, ClassGroup = "WwiserR", meta = (DisplayName = "WwiserR - Editor Config"))
class WWISERR_EDITOR_API UWwiserREditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, BlueprintReadOnly, EditDefaultsOnly, Category = "Audio Settings")
	EMuteWhenGameNotInForeground MuteWhenGameNotInForeground = EMuteWhenGameNotInForeground::UseGameSettings;
};
