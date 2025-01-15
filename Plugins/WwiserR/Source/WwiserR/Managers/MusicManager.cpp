// Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "MusicManager.h"
#include "GlobalSoundEmitterManager.h"
#include "AkGameObject.h"
#include "AkAudioEvent.h"
#include "AkStateValue.h"
#include "AkSwitchValue.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"

#if WITH_EDITOR
#include "WwiserR_Editor/EditorAudioUtils.h"
#include "Core/AudioSubsystem.h"
#endif

namespace Private_MusicManager
{
	static TAutoConsoleVariable<bool> CVar_MusicConductor_DebugConsole(
		TEXT("WwiserR.MusicManager.DebugToConsole"), false, TEXT("Music manager debugging to console. (0 = off, 1 = on)"), ECVF_Cheat);

	static TAutoConsoleVariable<bool> CVar_MusicConductor_MuteMusic(
		TEXT("WwiserR.MusicManager.MuteMusic"), false, TEXT("Mute music. (0 = unmute, 1 = mute)"), ECVF_Cheat);

	bool bDebugConsole = false;

	static void OnMusicManagerUpdate()
	{
		bDebugConsole = CVar_MusicConductor_DebugConsole.GetValueOnGameThread();
	}

	FAutoConsoleVariableSink CMusicManagerConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnMusicManagerUpdate));
} // namespace Private_MusicManager

void UMusicManager::Initialize(UAkGameObject* a_GlobalMusicSoundEmitter)
{
	GlobalMusicSoundEmitter = a_GlobalMusicSoundEmitter;

	static const FName delegateName = TEXT("OnMuteMusicCVarChanged");
	Private_MusicManager::CVar_MusicConductor_MuteMusic.AsVariable()
		->SetOnChangedCallback(FConsoleVariableDelegate::CreateUFunction(this, delegateName));

	WR_DBG_NET(Log, "initialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));

	MainMusicStartIfNotPlaying();
}

void UMusicManager::Deinitialize()
{
	MainMusicStop();
	WR_DBG_NET(Log, "deinitialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}


void UMusicManager::OnMainMusicCallback(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo)
{
	switch (CallbackType)
	{
		case EAkCallbackType::EndOfEvent:

			if (Private_MusicManager::bDebugConsole)
			{
				WR_DBG_FUNC(Log, "main music event (%s) ended", *MainMusicEvent->GetName());
			}

			MainMusicPlayingID = AK_INVALID_PLAYING_ID;
			break;

		default:
			break;
	}
}

void UMusicManager::OnMuteMusicCVarChanged()
{
	MuteMusic(Private_MusicManager::CVar_MusicConductor_MuteMusic->GetBool());
}

int32 UMusicManager::MainMusicStartIfNotPlaying()
{
	if (!IsValid(MainMusicEvent))
	{
		WR_DBG_FUNC(Error, "main music event not set in music conductor blueprint");

		MainMusicPlayingID = AK_INVALID_PLAYING_ID;
		return AK_INVALID_PLAYING_ID;
	}

	if (MainMusicPlayingID != AK_INVALID_PLAYING_ID)
	{
		if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
		{
			if (AkAudioDevice->IsPlayingIDActive(MainMusicEvent->GetShortID(), MainMusicPlayingID))
			{
				WR_DBG_FUNC(Warning, "tried starting the main music event when it was already playing");
				return MainMusicPlayingID;
			}
		}
	}

	check(GlobalMusicSoundEmitter);

	static const FName delegateName = TEXT("OnMainMusicCallback");
	FOnAkPostEventCallback MainMusicEndCallBack;
	MainMusicEndCallBack.BindUFunction(this, delegateName);
	MainMusicPlayingID = GlobalMusicSoundEmitter->PostAkEvent(MainMusicEvent, AkCallbackType::AK_MusicSyncAll, MainMusicEndCallBack);

	if (IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get())
	{
		for (UAkStateValue* initialState : InitialMusicStates)
		{
			SoundEngine->SetState(initialState->GetGroupID(), initialState->GetShortID());
		}

		for (UAkSwitchValue* initialSwitch : InitialMusicSwitches)
		{
			SoundEngine->SetSwitch(initialSwitch->GetGroupID(), initialSwitch->GetShortID(), GlobalMusicSoundEmitter->GetAkGameObjectID());
		}
	}

	if (Private_MusicManager::bDebugConsole)
	{
		WR_DBG_FUNC(Log, "main music event (%s) posted to Wwise", *MainMusicEvent->GetName());
	}

	return MainMusicPlayingID;
}

bool UMusicManager::MainMusicStop()
{
	check(GlobalMusicSoundEmitter);

	if (MainMusicPlayingID != AK_INVALID_PLAYING_ID)
	{
		if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
		{
			if (AkAudioDevice->IsPlayingIDActive(MainMusicEvent->GetShortID(), MainMusicPlayingID))
			{
				AkAudioDevice->StopPlayingID(MainMusicPlayingID);
				MainMusicPlayingID = AK_INVALID_PLAYING_ID;

				if (Private_MusicManager::bDebugConsole)
				{
					WR_DBG_FUNC(Log, "main music stopped");
				}

				return true;
			}
		}
	}

	WR_DBG_FUNC(Warning, "main music could not be stopped because it wasn't playing");
	return false;
}

void UMusicManager::MuteMusic(bool bMute)
{
	check(IsValid(GlobalMusicSoundEmitter));

#if WITH_EDITOR
	if (UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld()))
	{
		if (const UWorld* world = GetWorld())
		{
			UAudioSubsystem::Get(world)->GetGlobalSoundEmitterManager()->ConnectGlobalEmitter(GlobalMusicSoundEmitter, !bMute);
		}

		return;
	}
#endif

	if (bMute)
	{
		if (IsValid(MuteMusicEvent))
		{
			GlobalMusicSoundEmitter->PostAkEvent(MuteMusicEvent, AkCallbackType::AK_EndOfEvent, FOnAkPostEventCallback());
			WR_DBG_FUNC(Warning, "music muted");
		}
		else
		{
			WR_DBG_FUNC(Error, "MuteMusicEvent event not set in AudioSubsystem blueprint");
		}
	}
	else
	{
		if (IsValid(UnmuteMusicEvent))
		{
			GlobalMusicSoundEmitter->PostAkEvent(UnmuteMusicEvent, AkCallbackType::AK_EndOfEvent, FOnAkPostEventCallback());
			WR_DBG_FUNC(Warning, "music unmuted");
		}
		else
		{
			WR_DBG_FUNC(Error, "UnmuteMusicEvent event not set in AudioSubsystem blueprint");
		}
	}
}
