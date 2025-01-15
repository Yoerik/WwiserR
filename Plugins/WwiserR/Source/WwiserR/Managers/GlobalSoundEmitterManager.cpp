// Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "GlobalSoundEmitterManager.h"
#include "AkGameObject.h"
#include "AkComponent.h"
#include "AkAudioDevice.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "Core/AudioUtils.h"
#include "Core/AudioSubsystem.h"  // NOTE: move static getters for global listener and emitters into UAudioSubsystem to remove circular dependency?


#pragma region GlobalSoundEmitterManager
UGlobalSoundEmitterManager::~UGlobalSoundEmitterManager()
{
	if (IsValid(m_globalListener))
	{
		Deinitialize();
	}
}

void UGlobalSoundEmitterManager::Initialize()
{
	if (!UAudioUtils::IsClient(GetWorld())) { return; }

#if WITH_EDITOR
	if (!s_allConnectedGlobalEmitterManagers.Contains(this)) { s_allConnectedGlobalEmitterManagers.Add(this); }
#endif

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
		if (UNLIKELY(!SoundEngine)) { return; }

#if WITH_EDITOR
		const UWorld* world = GetWorld();
		uint8 pieInstance = world->WorldType == EWorldType::PIE ? UAudioSubsystem::Get(world)->m_PIEInstance : 0;
		if (pieInstance < 0) {	return;	}

		FString nameGlobalListener{ TEXT("Global Listener - PIE ") };
		nameGlobalListener.AppendInt(pieInstance);
#else
		static const FString nameGlobalListener{ TEXT("Global Listener") };
#endif

		// create and register global listener
		m_globalListener = NewObject<UAkGameObject>(this, FName(nameGlobalListener));
		m_globalListenerID = m_globalListener->GetAkGameObjectID();
		AkAudioDevice->RegisterGameObject(m_globalListenerID, nameGlobalListener);

		auto pListenerIds = (AkGameObjectID*)alloca(sizeof(AkGameObjectID));
		pListenerIds[0] = m_globalListenerID;
		SoundEngine->SetListeners(m_globalListenerID, pListenerIds, 1);
		SoundEngine->SetListenerSpatialization(m_globalListenerID, false, AkChannelConfig());

		// create and register global sound emitters
		m_globalEmitters.Init(nullptr, (uint8)EGlobalSoundEmitter::Count);
		m_globalEmitterIDs.Init(AkGameObjectID{}, (uint8)EGlobalSoundEmitter::Count);

		for (uint8 i = 0; i < (uint8)EGlobalSoundEmitter::Count; i++)
		{
#if WITH_EDITOR
			FString emitterName{ TEXT("Global Emitter: ") };
			emitterName.Append(StaticEnum<EGlobalSoundEmitter>()->GetDisplayNameTextByIndex(i).ToString()).Append(TEXT(" - PIE "));
			emitterName.AppendInt(pieInstance);
#else
			const FString emitterName = FString(TEXT("Global Emitter: ")).Append(
				StaticEnum<EGlobalSoundEmitter>()->GetDisplayNameTextByIndex(i).ToString());
#endif
			m_globalEmitters[i]	= NewObject<UAkGameObject>(this, FName(emitterName));
			m_globalEmitterIDs[i] = m_globalEmitters[i]->GetAkGameObjectID();
			AkAudioDevice->RegisterGameObject(m_globalEmitterIDs[i], emitterName);

			pListenerIds[0] = m_globalListenerID;
			SoundEngine->SetListeners(m_globalEmitterIDs[i], pListenerIds, 1);
			SoundEngine->SetListenerSpatialization(m_globalEmitterIDs[i], false, AkChannelConfig());
		}
	}

	WR_DBG_NET(Log, "initialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}

void UGlobalSoundEmitterManager::Deinitialize()
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	if (!m_globalEmitters.IsEmpty())
	{
		for (int i = 0; i < (uint8)EGlobalSoundEmitter::Count; i++)
		{
			SoundEngine->RemoveListener(m_globalEmitterIDs[i], m_globalListenerID);
			SoundEngine->UnregisterGameObj(m_globalEmitterIDs[i]);
		}

		m_globalEmitters.Empty();
	}

	SoundEngine->RemoveListener(m_globalListenerID, m_globalListenerID);
	SoundEngine->UnregisterGameObj(m_globalListenerID);

#if WITH_EDITOR
	s_allConnectedGlobalEmitterManagers.Remove(this);
#endif

	if (UWorld* world = GEngine->GetWorld())
	{
		WR_DBG_NET(Log, "deinitialized (%s)", *UAudioUtils::GetClientOrServerString(world));
	}
	else
	{
		WR_DBG_NET(Log, "deinitialized");
	}
}

bool UGlobalSoundEmitterManager::GetIndexOf(const UAkGameObject* Emitter, uint8& Index) const
{
	for (uint8 i = 0; i < (uint8)EGlobalSoundEmitter::Count; i++)
	{
		if (Emitter == m_globalEmitters[i])
		{
			Index = i;
			return true;
		}
	}

	Index = -1;
	return false;
}

void UGlobalSoundEmitterManager::ConnectGlobalListener(const bool bConnect)
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	if (bConnect)
	{
		auto pListenerIds = (AkGameObjectID*)alloca(sizeof(AkGameObjectID));
		pListenerIds[0] = m_globalListenerID;

		SoundEngine->SetListeners(m_globalListenerID, pListenerIds, 1);

		for (int i = 0; i < (uint8)EGlobalSoundEmitter::Count; i++)
		{
			SoundEngine->SetListeners(m_globalEmitterIDs[i], pListenerIds, 1);
		}
	}
	else
	{
		for (int i = 0; i < (uint8)EGlobalSoundEmitter::Count; i++)
		{
			SoundEngine->RemoveListener(m_globalEmitterIDs[i], m_globalListenerID);
		}

		SoundEngine->RemoveListener(m_globalListenerID, m_globalListenerID);
	}
}

void UGlobalSoundEmitterManager::ConnectGlobalEmitter(UAkGameObject* Emitter, const bool bConnect)
{
	uint8 emitterIndex;

	if (GetIndexOf(Emitter, emitterIndex))
	{
		IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
		if (UNLIKELY(!SoundEngine)) { return; }

		if (bConnect)
		{
			auto pListenerIds = (AkGameObjectID*)alloca(sizeof(AkGameObjectID));
			pListenerIds[0] = m_globalListenerID;
			SoundEngine->SetListeners(m_globalEmitterIDs[emitterIndex], pListenerIds, 1);
		}
		else
		{
			SoundEngine->RemoveListener(m_globalEmitterIDs[emitterIndex], m_globalListenerID);
		}
	}
}

UPARAM(DisplayName = "Global SoundEmitter") UAkGameObject* UGlobalSoundEmitterManager::GetGlobalListener(const UObject* Context)
{
	if (const UAudioSubsystem* audioSubsystem = UAudioSubsystem::Get(Context))
	{
		if (const UGlobalSoundEmitterManager* globalSoundEmitterManager = audioSubsystem->GetGlobalSoundEmitterManager())
		{
			return globalSoundEmitterManager->m_globalListener;
		}
	}

	return nullptr;
}

UAkGameObject* UGlobalSoundEmitterManager::GetGlobalSoundEmitter(const UObject* Context, EGlobalSoundEmitter GlobalSoundEmitter)
{
	if (const UAudioSubsystem* audioSubsystem = UAudioSubsystem::Get(Context))
	{
		if (const UGlobalSoundEmitterManager* globalSoundEmitterManager = audioSubsystem->GetGlobalSoundEmitterManager())
		{
			return globalSoundEmitterManager->m_globalEmitters[(uint8)GlobalSoundEmitter];
		}
	}

	return nullptr;
}
void UGlobalSoundEmitterManager::ConnectListenerToGlobalSoundObject(UAkComponent* Listener, UAkGameObject* GlobalSoundObject, bool bResetConnections)
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	if (bResetConnections)
	{
		auto pListenerIds = (AkGameObjectID*)alloca(sizeof(AkGameObjectID));
		pListenerIds[0] = GlobalSoundObject->GetAkGameObjectID();
		SoundEngine->SetListeners(Listener->GetAkGameObjectID(), pListenerIds, 1);
	}
	else
	{
		SoundEngine->AddListener(Listener->GetAkGameObjectID(), GlobalSoundObject->GetAkGameObjectID());
	}
}
void UGlobalSoundEmitterManager::DisconnectListenerFromGlobalSoundObject(UAkComponent* Listener, UAkGameObject* GlobalSoundObject)
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	SoundEngine->RemoveListener(Listener->GetAkGameObjectID(), GlobalSoundObject->GetAkGameObjectID());
}
#pragma endregion
