// Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "AudioSubsystem.h"
#include "Managers/SoundListenerManager.h"
#include "Managers/GlobalSoundEmitterManager.h"
#include "Managers/StaticSoundEmitterManager.h"
#include "Managers/AmbientBedManager.h"
#include "Managers/MusicManager.h"
//#include "SoundEmitters/PooledSoundEmitterComponent.h"
#include "Core/AudioUtils.h"
#include "Config/AudioConfig.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkComponent.h"
#include "Runtime/ApplicationCore/Public/HAL/PlatformApplicationMisc.h"

#if !UE_BUILD_SHIPPING
#include "SoundEmitters/SoundEmitterComponentBase.h"
#include "Config/AudioConfig.h"
#endif
#if WITH_EDITOR
#include "WwiserR_Editor/Config/EditorAudioConfig.h"
#endif

namespace Private_AudioSubsystem
{
	static TAutoConsoleVariable<bool> CVar_AudioSubsystem_DebugToConsole(
		TEXT("WwiserR.AudioSubsystem.DebugToConsole"), false, TEXT("Audio Subsystem: Log to console. (0 = unmute, 1 = mute)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_AudioSubsystem_MuteAll(
		TEXT("WwiserR.AudioSubsystem.MuteAll"), false, TEXT("Audio Subsystem: mute all. (0 = unmute, 1 = mute)"), ECVF_Cheat);

#if WITH_EDITOR
	static TAutoConsoleVariable<int> CVar_AudioSubsystem_MuteInstance(
		TEXT("WwiserR.AudioSubsystem.MuteInstance"), -1, TEXT("mute specified instance (whem running PIE under one process)"), ECVF_Cheat);
	static TAutoConsoleVariable<int> CVar_AudioSubsystem_UnmuteInstance(
		TEXT("WwiserR.AudioSubsystem.UnmuteInstance"), -1, TEXT("unmute specified instance (whem running PIE under one process)"), ECVF_Cheat);
#endif

	bool bDebugToConsole = false;

	static void OnAudioSubsystemUpdate()
	{
		bDebugToConsole = CVar_AudioSubsystem_DebugToConsole.GetValueOnGameThread();
	}

	FAutoConsoleVariableSink CAudioSubsystemConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnAudioSubsystemUpdate));
} // namespace Private_AudioSubsystem

#pragma region Initialization
void UAudioSubsystem::Initialize(FSubsystemCollectionBase& a_collection)
{
	Super::Initialize(a_collection);

#if WITH_EDITOR
	s_allAudioSubsystems.Add(this);
#endif

	if (const UWorld* world = GetWorld())
	{
		if (UAudioUtils::IsClient(world))
		{
			InitializeOnClient();
		}

		if (UAudioUtils::IsServer(world))
		{
			InitializeOnServer();
		}
	}
}

void UAudioSubsystem::Deinitialize()
{
	if (const UWorld* world = GetWorld())
	{
		if (UAudioUtils::IsClient(world))
		{
			DeinitializeOnClient();
		}

		if (UAudioUtils::IsServer(world))
		{
			DeinitializeOnServer();
		}
	}

#if WITH_EDITOR
	s_allAudioSubsystems.Remove(this);
#endif

	Super::Deinitialize();
}

void UAudioSubsystem::InitializeOnServer()
{
	WR_DBG_INST_NET(Log, "initialized (server)");
}

void UAudioSubsystem::InitializeOnClient()
{
	const UWwiserRGameSettings* audioConfig = GetDefault<UWwiserRGameSettings>();

#if WITH_EDITOR
	m_PIEInstance = UEditorAudioUtils::GetPIEInstance(GetWorld());
#endif

	InitializeMembers(audioConfig);
	ClientBindDelegates();

	InitializeGlobalEmitterManager();
	//InitializePooledEmitterManager();
	InitializeListenerManager(audioConfig);
	InitializeMusicManager(audioConfig);
	InitializeStaticSoundEmitterManager();
	InitializeAmbientBedManager();

	//ConditionalMutePieInstance();
	UpdateAppHasAudioFocus();

	WR_DBG_INST_NET(Log, "initialized (client)");

#if WITH_EDITOR
	if (UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld())) { return; }

	WR_DBG_INST_NET(Log, "not running under one process");
	WR_DBG_INST_FUNC(Error, "%s", *UEnum::GetValueAsString(m_ShouldInstancePlayAudio));

	MuteAllAudio(!ShouldInstancePlayAudio());
#endif
}

void UAudioSubsystem::InitializeMembers(const UWwiserRGameSettings* audioConfig)
{
	m_muteWhenGameNotInForeground = audioConfig->bMuteWhenGameNotInForeground;
#if WITH_EDITOR
	m_ShouldInstancePlayAudio = UEditorAudioUtils::ShouldInstancePlayAudio(GetWorld());
	s_soloAudioInFirstPIEClient = UEditorAudioUtils::GetSoloAudioInFirstPIEClient();
	//m_shouldMutePieInstance = UEditorAudioUtils::ShouldMutePieInstance(GetWorld());

	const UWwiserREditorSettings* editorSettings = GetDefault<UWwiserREditorSettings>();
	if (editorSettings->MuteWhenGameNotInForeground != EMuteWhenGameNotInForeground::UseGameSettings)
	{
		m_muteWhenGameNotInForeground = editorSettings->MuteWhenGameNotInForeground == EMuteWhenGameNotInForeground::MuteWhenGameNotInForeground ?
			true : false;
	}

	if (s_soloAudioInFirstPIEClient)
	{
		m_muteWhenGameNotInForeground = false;
	}
#endif

	// load mute Wwise events
	audioConfig->MuteAllEvent.TryLoad();
	audioConfig->UnmuteAllEvent.TryLoad();
	MuteAllEvent = Cast<UAkAudioEvent>(audioConfig->MuteAllEvent.ResolveObject());
	UnmuteAllEvent = Cast<UAkAudioEvent>(audioConfig->UnmuteAllEvent.ResolveObject());
}

void UAudioSubsystem::DeinitializeOnServer()
{
	WR_DBG_INST_NET(Log, "deinitialized (server)");
}

void UAudioSubsystem::DeinitializeOnClient()
{
	ClientUnbindDelegates();

	DeinitializeAmbientBedManager();
	DeinitializeStaticSoundEmitterManager();
	DeinitializeMusicManager();
	DeinitializeListenerManager();
	//DeinitializePooledEmitterManager();
	DeinitializeGlobalEmitterManager();
	WR_DBG_INST_NET(Log, "deinitialized (client)");
}

void UAudioSubsystem::InitializeGlobalEmitterManager()
{
	static const FName globalSoundEmitterManagerName{ TEXT("GlobalSoundEmitterManager") };
	m_globalSoundEmitterManager = NewObject<UGlobalSoundEmitterManager>(this, globalSoundEmitterManagerName);
	m_globalSoundEmitterManager->Initialize();

#if WITH_EDITOR
	if (UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld()))
	{
		InitializeGlobalListenerConnection();
	}
#endif*/
}

void UAudioSubsystem::DeinitializeGlobalEmitterManager()
{
	if (IsValid(m_globalSoundEmitterManager))
	{
		m_globalSoundEmitterManager->Deinitialize();
		m_globalSoundEmitterManager = nullptr;
	}
}

/*void UAudioSubsystem::InitializePooledEmitterManager()
{
	static const FName pooledSoundEmitterManagerName{ TEXT("PooledSoundEmitterManager") };
	m_pooledSoundEmitterManager = NewObject<UPooledSoundEmitterManager>(this, pooledSoundEmitterManagerName);
	m_pooledSoundEmitterManager->Initialize();
}

void UAudioSubsystem::DeinitializePooledEmitterManager()
{
	if (IsValid(m_pooledSoundEmitterManager))
	{
		m_pooledSoundEmitterManager->Deinitialize();
		m_pooledSoundEmitterManager = nullptr;
	}
}*/

void UAudioSubsystem::InitializeListenerManager(const UWwiserRGameSettings* AudioConfig)
{
	if (IsValid(AudioConfig->ListenerManagerClass))
	{
		ListenerManager = NewObject<USoundListenerManager>(this, *AudioConfig->ListenerManagerClass);
	}
	else
	{
		static const FName listenerManagerName{ TEXT("Default Listener Manager") };
		ListenerManager = NewObject<USoundListenerManager>(this, listenerManagerName);
	}

	ListenerManager->Initialize();
}

void UAudioSubsystem::DeinitializeListenerManager()
{
	if (IsValid(ListenerManager))
	{
		ListenerManager->Deinitialize();
		ListenerManager = nullptr;
	}
}

void UAudioSubsystem::InitializeMusicManager(const UWwiserRGameSettings* AudioConfig)
{
	if (const TSubclassOf<UMusicManager> musicManagerClass = AudioConfig->MusicManagerClass)
	{
		check(m_globalSoundEmitterManager);

		MusicManager = NewObject<UMusicManager>(this, *musicManagerClass);
		MusicManager->Initialize(m_globalSoundEmitterManager->GetGlobalSoundEmitter(EGlobalSoundEmitter::Music));
	}
}

void UAudioSubsystem::DeinitializeMusicManager()
{
	if (IsValid(MusicManager))
	{
		MusicManager->Deinitialize();
		MusicManager = nullptr;
	}
}

void UAudioSubsystem::InitializeStaticSoundEmitterManager()
{
	static const FName staticSoundEmitterManagerName{ TEXT("StaticSoundEmitterManager") };
	m_staticSoundEmitterManager = NewObject<UStaticSoundEmitterManager>(this, staticSoundEmitterManagerName);
	m_staticSoundEmitterManager->Initialize(ListenerManager);
}

void UAudioSubsystem::DeinitializeStaticSoundEmitterManager()
{
	if (IsValid(m_staticSoundEmitterManager))
	{
		m_staticSoundEmitterManager->Deinitialize();
		m_staticSoundEmitterManager = nullptr;
	}
}

void UAudioSubsystem::InitializeAmbientBedManager()
{
	static const FName ambientBedManagerName{ TEXT("AmbientBedManager") };
	m_ambientBedManager = NewObject<UAmbientBedManager>(this, ambientBedManagerName);
	m_ambientBedManager->Initialize(ListenerManager);
}

void UAudioSubsystem::DeinitializeAmbientBedManager()
{
	if (IsValid(m_ambientBedManager))
	{
		m_ambientBedManager->Deinitialize();
		m_ambientBedManager = nullptr;
	}
}

void UAudioSubsystem::ClientBindDelegates()
{
	//static const FName funcBeginPlay{ "ClientBeginPlay" };
	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UAudioSubsystem::ClientBeginPlay);

	static const FName funcOnMuteAllCVarChanged{ TEXT("OnMuteAllCVarChanged") };
	Private_AudioSubsystem::CVar_AudioSubsystem_MuteAll.AsVariable()
		->SetOnChangedCallback(FConsoleVariableDelegate::CreateUFunction(this, funcOnMuteAllCVarChanged));

#if WITH_EDITOR
	static const FName funcOnMuteInstanceCVarChanged{ TEXT("OnMuteInstanceCVarChanged") };
	Private_AudioSubsystem::CVar_AudioSubsystem_MuteInstance.AsVariable()
		->SetOnChangedCallback(FConsoleVariableDelegate::CreateUFunction(this, funcOnMuteInstanceCVarChanged));

	static const FName funcOnUnmuteInstanceCVarChanged{ TEXT("OnUnmuteInstanceCVarChanged") };
	Private_AudioSubsystem::CVar_AudioSubsystem_UnmuteInstance.AsVariable()
		->SetOnChangedCallback(FConsoleVariableDelegate::CreateUFunction(this, funcOnUnmuteInstanceCVarChanged));
#endif
}

void UAudioSubsystem::ClientUnbindDelegates()
{
	FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
	OnAppHasAudioFocusChanged.Clear();
}
#pragma endregion

void UAudioSubsystem::ClientBeginPlay(UWorld* World, const FWorldInitializationValues IVS)
{
	WR_DBG_INST_NET_FUNC(Log, "new World: %s - %s", *World->GetName(), *FString(UAudioUtils::WorldTypeToString(World->WorldType)));

	UpdateAppHasAudioFocus();
}

void UAudioSubsystem::Tick(float DeltaTime)
{
	if (m_lastTickFrame == GFrameCounter) { return; }
	m_lastTickFrame = GFrameCounter;

	UpdateAppHasAudioFocus();
}

bool UAudioSubsystem::IsAllowedToTick() const
{
	return m_muteWhenGameNotInForeground;
}

void UAudioSubsystem::OnMuteAllCVarChanged()
{
	PostWwiseMuteEvent(Private_AudioSubsystem::CVar_AudioSubsystem_MuteAll->GetBool());
}

#if WITH_EDITOR
void UAudioSubsystem::OnMuteInstanceCVarChanged()
{
	if (!UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld())) {	return;	}

	const TArray<USoundListenerManager*> listenerManagers = USoundListenerManager::GetAllConnectedListenerManagers();
	const TArray<UGlobalSoundEmitterManager*> globalEmitterManagers = UGlobalSoundEmitterManager::GetAllConnectedListenerManagers();
	const int pieInstance = Private_AudioSubsystem::CVar_AudioSubsystem_MuteInstance->GetInt();

	if (pieInstance >= 0 && pieInstance < listenerManagers.Num() && IsValid(listenerManagers[pieInstance]))
	{
		listenerManagers[pieInstance]->MuteInstance(true);
	}

	if (pieInstance >= 0 && pieInstance < globalEmitterManagers.Num() && IsValid(globalEmitterManagers[pieInstance]))
	{
		globalEmitterManagers[pieInstance]->ConnectGlobalListener(false);
	}
}

void UAudioSubsystem::OnUnmuteInstanceCVarChanged()
{
	if (!UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld())) { return; }

	const TArray<USoundListenerManager*> listenerManagers = USoundListenerManager::GetAllConnectedListenerManagers();
	const TArray<UGlobalSoundEmitterManager*> globalEmitterManagers = UGlobalSoundEmitterManager::GetAllConnectedListenerManagers();
	const int pieInstance = Private_AudioSubsystem::CVar_AudioSubsystem_UnmuteInstance->GetInt();

	if (pieInstance >= 0 && pieInstance < listenerManagers.Num())
	{
		listenerManagers[pieInstance]->MuteInstance(false);
	}

	if (pieInstance >= 0 && pieInstance < globalEmitterManagers.Num())
	{
		globalEmitterManagers[pieInstance]->ConnectGlobalListener(true);
	}
}
#endif

void UAudioSubsystem::UpdateAppHasAudioFocus()
{
	bool isFocused = FPlatformApplicationMisc::IsThisApplicationForeground();

#if WITH_EDITOR
	const UWorld* world = GetWorld();
	isFocused = isFocused && UEditorAudioUtils::HasPIEViewportFocus(world);
#endif

	if (!m_muteWhenGameNotInForeground) { return; }

	if (m_isAppForeground != isFocused)
	{
		WR_DBG_INST_FUNC(Error, "m_isAppForeground != isFocused");

		m_isAppForeground = isFocused;

#if WITH_EDITOR
		if (UEditorAudioUtils::IsRunningUnderOneProcess(world) && m_muteWhenGameNotInForeground && !s_soloAudioInFirstPIEClient)
		{
			for (UAudioSubsystem* audioSubsystem : s_allAudioSubsystems)
			{
				if (!audioSubsystem->m_isAppForeground)
				{
					audioSubsystem->ListenerManager->MuteInstance(true);
				}
			}
		}
#endif

		if (!MuteAllAudio(!m_isAppForeground))
		{
#if WITH_EDITOR
			if (UEditorAudioUtils::IsRunningUnderOneProcess(world))
			{
				if (m_isAppForeground || s_soloAudioInFirstPIEClient)
				{
					ListenerManager->ConnectSpatialAudioListener();
				}
				/*else if (!m_isAppForeground && !s_soloAudioInFirstPIEClient)
				{
					ListenerManager->MuteInstance(true);
				}*/
			}
#endif
		}

		if (OnAppHasAudioFocusChanged.IsBound())
		{
			OnAppHasAudioFocusChanged.Broadcast(m_isAppForeground);
		}
	}
}

#if WITH_EDITOR
void UAudioSubsystem::InitializeGlobalListenerConnection()
{
	WR_ASSERT(m_globalSoundEmitterManager, "Global Sound Emitter Manager not found");

	m_globalSoundEmitterManager->ConnectGlobalListener(ShouldInstancePlayAudio());
}
#endif

#pragma region Public Methods
#if WITH_EDITOR
bool UAudioSubsystem::ShouldInstancePlayAudio()
{
	switch (m_ShouldInstancePlayAudio)
	{
		case EShouldInstancePlayAudio::Always:
			return true;

		case EShouldInstancePlayAudio::Never:
			return false;

		case EShouldInstancePlayAudio::UseGameForegroundSettings:
			if (!m_muteWhenGameNotInForeground) { return true; }
			[[fallthrough]];
		case EShouldInstancePlayAudio::InForegroundOnly:
			return FPlatformApplicationMisc::IsThisApplicationForeground();
			break;
	}

	return true;
}
#endif

bool UAudioSubsystem::MuteAllAudio(const bool bMute)
{
	if (bMute == m_isMuted)
	{
		return false;
	}

	m_isMuted = bMute;

#if WITH_EDITOR
	if (UEditorAudioUtils::GetRunUnderOneProcess())
	{
		if (!s_soloAudioInFirstPIEClient)
		{
			m_globalSoundEmitterManager->ConnectGlobalListener(!bMute);
		}

		if (!bMute)
		{
			ListenerManager->ConnectSpatialAudioListener();
		}

		if (Private_AudioSubsystem::bDebugToConsole)
		{
			const FString msg{ bMute ? TEXT("all audio muted") : TEXT("all audio unmuted") };
			WR_DBG_INST_FUNC(Log, "%s", *msg);
		}

		return true;
	}

	/*else
	{
		ListenerManager->SetIsSpatialAudioListenerConnected(!bMute);
	}*/
#endif

	PostWwiseMuteEvent(bMute);
	return true;
}

bool UAudioSubsystem::PostWwiseMuteEvent(const bool bMute)
{
	UAkAudioEvent* eventToPost = bMute ? MuteAllEvent : UnmuteAllEvent;

	if (IsValid(eventToPost))
	{
		if (IsValid(m_globalSoundEmitterManager))
		{
			if (UAkGameObject* globalListener = m_globalSoundEmitterManager->GetGlobalListener())
			{
				globalListener->PostAkEvent(eventToPost, AkCallbackType::AK_EndOfEvent, FOnAkPostEventCallback());
			}
		}
		else if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
		{
			if (UAkComponent* spatialAudioListener = AkAudioDevice->GetSpatialAudioListener())
			{
				spatialAudioListener->PostAkEvent(eventToPost, 0, FOnAkPostEventCallback());
			}
		}
	}
	else // && !IsValid(eventToPost)
	{
		const FString msgEvent{ bMute ? TEXT("MuteAllEvent") : TEXT("UnmuteAllEvent") };
		WR_DBG_INST_FUNC(Error, "%s event not set in project settings", *msgEvent);

		return false;
	}

	if (Private_AudioSubsystem::bDebugToConsole)
	{
		const FString msg{ bMute ? TEXT("all audio muted") : TEXT("all audio unmuted") };
		WR_DBG_INST_FUNC(Log, "%s", *msg);
	}

	return true;
}

void UAudioSubsystem::SetMuteWhenAppNotInForeground(const bool bMute)
{
#if WITH_EDITOR
	if (!(m_ShouldInstancePlayAudio == EShouldInstancePlayAudio::UseGameForegroundSettings)) { return; }
#endif

	m_muteWhenGameNotInForeground = bMute;
	MuteAllAudio(bMute);
}

USoundListenerManager* UAudioSubsystem::GetListenerManager()
{
	return ListenerManager;
}

UMusicManager* UAudioSubsystem::GetMusicManager()
{
	return MusicManager;
}
#pragma endregion
