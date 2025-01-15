// Copyright Yoerik Roevens. All Rights Reserved.(c)


#include "Core/WWiserRGameplayStatics.h"
#include "AkAudioDevice.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "AkGameObject.h"
#include "AkRtpc.h"
#include "AkStateValue.h"
#include "Core/AudioUtils.h"
#include "Core/AudioSubsystem.h"
#include "Managers/GlobalSoundEmitterManager.h"

#if WITH_EDITOR
#include "WwiserR_Editor/EditorAudioUtils.h"
#endif

bool UWWiserRGameplayStatics::ShouldPostGlobalGameSynch(const UObject* WorldContextObject)
{
#if !WITH_EDITOR
	return true;
#else
	if (!IsValid(WorldContextObject)) { return false; }

	if (const UWorld* world = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UEditorAudioUtils::IsRunningUnderOneProcess(WorldContextObject->GetWorld()))
		{
			if (UAudioSubsystem* audioSubsystem = UAudioSubsystem::Get(world))
			{
				return audioSubsystem->ShouldInstancePlayAudio();
			}

			return false;
		}
	}

	return true;
#endif
}

void UWWiserRGameplayStatics::SetGlobalRtpcValue(const UObject* WorldContextObject, UAkRtpc* AkRtpc, float Value, int32 InterpolationTimeMs)
{
#if WITH_EDITOR
	if (!IsValid(WorldContextObject) || !ShouldPostGlobalGameSynch(WorldContextObject)) { return; }
#endif

	if (!IsValid(AkRtpc))
	{
		WR_DBG_STATIC(Warning, "missing Rtpc in [SetGlobalRtpcValue] node");
		return;
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->SetRTPCValue(AkRtpc, Value, InterpolationTimeMs, nullptr);
	}
}

void UWWiserRGameplayStatics::ResetGlobalRtpcValue(const UObject* WorldContextObject, UAkRtpc* AkRtpc, int32 InterpolationTimeMs)
{
#if WITH_EDITOR
	if (!IsValid(WorldContextObject) || !ShouldPostGlobalGameSynch(WorldContextObject)) { return; }
#endif

	if (!IsValid(AkRtpc))
	{
		WR_DBG_STATIC(Warning, "missing Rtpc in [ResetGlobalRtpcValue] node");
		return;
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->ResetRTPCValue(AkRtpc,
			UGlobalSoundEmitterManager::GetGlobalListener(GEngine->GetWorld())->GetAkGameObjectID(), InterpolationTimeMs);
	}
}

void UWWiserRGameplayStatics::SetState(const UObject* WorldContextObject, UAkStateValue* StateValue)
{
#if WITH_EDITOR
	if (!IsValid(WorldContextObject) || !ShouldPostGlobalGameSynch(WorldContextObject)) { return; }
#endif

	if (!IsValid(StateValue))
	{
		WR_DBG_STATIC(Warning, "missing state in [SetState] node");
		return;
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->SetState(StateValue);
	}
}
