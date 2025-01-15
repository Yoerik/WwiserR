// Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "EditorAudioUtils.h"
#include "Config/EditorAudioConfig.h"

bool UEditorAudioUtils::GetEditorGameSoundEnabled()
{
	const ULevelEditorPlaySettings* editorPlaySettings = GetDefault<ULevelEditorPlaySettings>();
	return editorPlaySettings->EnableGameSound;
}

bool UEditorAudioUtils::GetSoloAudioInFirstPIEClient()
{
	const ULevelEditorPlaySettings* editorPlaySettings = GetDefault<ULevelEditorPlaySettings>();
	return editorPlaySettings->SoloAudioInFirstPIEClient;
}

bool UEditorAudioUtils::GetRunUnderOneProcess()
{
	const ULevelEditorPlaySettings* editorPlaySettings = GetDefault<ULevelEditorPlaySettings>();
	bool runUnderOneProcess;
	editorPlaySettings->GetRunUnderOneProcess(runUnderOneProcess);

	return runUnderOneProcess;
}

bool UEditorAudioUtils::IsRunningUnderOneProcess(const UWorld* World)
{
	if (IsValid(World) && World->WorldType != EWorldType::PIE) { return false; }

	const ULevelEditorPlaySettings* editorPlaySettings = GetDefault<ULevelEditorPlaySettings>();
	bool runUnderOneProcess;
	editorPlaySettings->GetRunUnderOneProcess(runUnderOneProcess);

	return runUnderOneProcess;
}

bool UEditorAudioUtils::HasPIEViewportFocus(const UWorld* World)
{
	if (!IsValid(World)) { return false; }

	if (UGameViewportClient* gameViewportClient = World->GetGameViewport())
	{
		return gameViewportClient->HasAudioFocus();
		UE_LOG(LogTemp, Log, TEXT("%s"), (gameViewportClient->HasAudioFocus() ? TEXT("FOCUSED") : TEXT("NOT FOCUSED")));

		/*if (IsValid(GEngine->GameViewport) && (GEngine->GameViewport->Viewport != nullptr))
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), (gameViewportClient->IsFocused(GEngine->GameViewport->Viewport) ? TEXT("FOCUSED") : TEXT("NOT FOCUSED")));
			return gameViewportClient->IsFocused(GEngine->GameViewport->Viewport);
		}*/
	}

	return false;
}

bool UEditorAudioUtils::IsPrimaryPIEInstance(const UWorld* World)
{
	if (!IsValid(World)) { return false; }

	return GEngine->GetWorldContextFromWorld(World)->bIsPrimaryPIEInstance;
}

uint8 UEditorAudioUtils::GetPIEInstance(const UWorld* world)
{
	if (!IsValid(world) || world->WorldType != EWorldType::PIE || GEngine->GetWorldContextFromWorld(world) == nullptr)
	{
		return -1;
	}

	return GEngine->GetWorldContextFromWorld(world)->PIEInstance;
	/*uint8 pieInstance = GEngine->GetWorldContextFromWorld(world)->PIEInstance;

	const ULevelEditorPlaySettings* playSettings = Cast<ULevelEditorPlaySettings>(ULevelEditorPlaySettings::StaticClass()->GetDefaultObject());
	EPlayNetMode playNetMode;
	bool bRunUnderOneProcess;
	playSettings->GetPlayNetMode(playNetMode);
	playSettings->GetRunUnderOneProcess(bRunUnderOneProcess);

	if (bRunUnderOneProcess && playNetMode == EPlayNetMode::PIE_Client)
	{
		pieInstance--;
	}

	return pieInstance;*/
}

EShouldInstancePlayAudio UEditorAudioUtils::ShouldInstancePlayAudio(const UWorld* world)
{
	const ULevelEditorPlaySettings* editorPlaySettings = GetDefault<ULevelEditorPlaySettings>();

	if (!editorPlaySettings->EnableGameSound) { return EShouldInstancePlayAudio::Never; }

	if (IsValid(world))
	{
		if (editorPlaySettings->SoloAudioInFirstPIEClient)
		{
			UE_LOG(LogTemp, Error, TEXT("GetSoloAudioInFirstPIEClient = %s, %s"),
				editorPlaySettings->SoloAudioInFirstPIEClient ? TEXT("true") : TEXT("false"),
				*UEnum::GetValueAsString(UEditorAudioUtils::IsPrimaryPIEInstance(world) ? EShouldInstancePlayAudio::Always : EShouldInstancePlayAudio::Never));

			return (UEditorAudioUtils::IsPrimaryPIEInstance(world) ? EShouldInstancePlayAudio::Always : EShouldInstancePlayAudio::Never);
		}

		const UWwiserREditorSettings* editorConfig = GetDefault<UWwiserREditorSettings>();

		switch (editorConfig->MuteWhenGameNotInForeground)
		{
			case EMuteWhenGameNotInForeground::UseGameSettings:
				return EShouldInstancePlayAudio::UseGameForegroundSettings;
			case EMuteWhenGameNotInForeground::MuteWhenGameNotInForeground:
				return EShouldInstancePlayAudio::InForegroundOnly;
			case EMuteWhenGameNotInForeground::NeverMute:
				return EShouldInstancePlayAudio::Always;
		}
	}

	return EShouldInstancePlayAudio::Undefined;
}

/*EShouldMutePieInstance UEditorAudioUtils::ShouldMutePieInstance(const UWorld* world)
{
	if (!IsValid(world)) { return EShouldMutePieInstance::Undefined; }

	if (world->WorldType != EWorldType::PIE)
	{
		return EShouldMutePieInstance::No;
	}

	const ULevelEditorPlaySettings* editorPlaySettings = GetDefault<ULevelEditorPlaySettings>();
	//if (!editorPlaySettings->EnableGameSound) { return EShouldMutePieInstance::Yes; }

	if (FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(world))
	{
		return !editorPlaySettings->EnableGameSound
			|| (!WorldContext->bIsPrimaryPIEInstance && editorPlaySettings->GetSoloAudioInFirstPIEClient)
			? EShouldMutePieInstance::Yes : EShouldMutePieInstance::No;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s: no valid WorldContext"), *world->GetName());
	}

	return EShouldMutePieInstance::No;
}*/
