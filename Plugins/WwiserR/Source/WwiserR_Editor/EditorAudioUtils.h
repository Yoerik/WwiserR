// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "EditorAudioUtils.generated.h"

/*enum class EShouldMutePieInstance
{
	Yes,
	No,
	Undefined
};*/

UENUM()
enum class EShouldInstancePlayAudio
{
	Always,
	Never,
	InForegroundOnly,
	UseGameForegroundSettings,
	//ManuallyMuted,
	//ManuallyUnmuted,
	Undefined
};

// WwiserR Editor - helper functions
UCLASS(ClassGroup = "WwiserR_Editor") class WWISERR_EDITOR_API UEditorAudioUtils : public UObject
{
	GENERATED_BODY()

public:
	static bool GetEditorGameSoundEnabled();
	static bool GetSoloAudioInFirstPIEClient();
	static bool GetRunUnderOneProcess();

	static bool IsRunningUnderOneProcess(const UWorld* World);
	static bool HasPIEViewportFocus(const UWorld* World);
	static bool IsPrimaryPIEInstance(const UWorld* World);
	static uint8 GetPIEInstance(const UWorld* world);
	static EShouldInstancePlayAudio ShouldInstancePlayAudio(const UWorld* world);

	//static uint8 GetAllInstances(const UWorld* world);
	//static EShouldMutePieInstance ShouldMutePieInstance(const UWorld* world);
};
