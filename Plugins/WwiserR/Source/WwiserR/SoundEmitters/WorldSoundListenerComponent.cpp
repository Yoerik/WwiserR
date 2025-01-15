// Copyright Yoerik Roevens. All Rights Reserved.(c)


#include "WorldSoundListenerComponent.h"
#include "AuxSoundEmitterComponent.h"
#include "Managers/SoundListenerManager.h"
#include "Core/AudioSubsystem.h"
#include "Managers/GlobalSoundEmitterManager.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "AkAuxBus.h"
//#include "WwiseSoundEngine/Public/Wwise/API/WwiseSpatialAudioAPI.h"


#pragma region UWorldSoundListener
UWorldSoundListener::UWorldSoundListener(const FObjectInitializer& ObjectInitializer) :	Super(ObjectInitializer) {}

void UWorldSoundListener::BeginPlay()
{
	Super::BeginPlay();

	UpdatePosition();

	s_listenerManager = UAudioSubsystem::Get(this)->GetListenerManager();
	s_listenerManager->AddWorldListener(this);
	s_worldListenersAuxBusParams.Add(this, TMap<UAkAuxBus*, FAuxBusParams>{});

	if (IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get())
	{
		SoundEngine->SetListenerSpatialization((AkGameObjectID)this, false, AkChannelConfig());

		auto pListenerIds = (AkGameObjectID*)alloca(0 * sizeof(AkGameObjectID));
		SoundEngine->SetListeners((AkGameObjectID)this, pListenerIds, 0);
	}

	auto onTransformUpdated =
		[this](USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlag, ETeleportType Teleport)->void
		{
			if (!(Teleport == ETeleportType::None))
			{
				if (s_listenerManager->OnAttenuationReferenceChanged.IsBound())
				{
					s_listenerManager->OnAttenuationReferenceChanged.Broadcast();
				}
			}
		};

	TransformUpdated.AddLambda(onTransformUpdated);

	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			if (s_listenerManager->OnListenersUpdated.IsBound())
			{
				s_listenerManager->OnListenersUpdated.Broadcast();
			}
		});
}

void UWorldSoundListener::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	s_listenerManager->RemoveWorldListener(this);
	s_worldListenersAuxBusParams.Remove(this);
	TSet<UAkAuxBus*> keys;
	m_auxConfigurations.GetKeys(keys);

	for (UAkAuxBus* key : keys)
	{
		RemoveAuxBus(key);
	}

	UpdateWorldListener();

	if (s_listenerManager->GetWorldListeners().IsEmpty())
	{
		if (s_listenerManager->OnAllWorldListenersRemoved.IsBound())
		{
			s_listenerManager->OnAllWorldListenersRemoved.Broadcast();
		}
	}
	else
	{
		if (s_listenerManager->OnListenersUpdated.IsBound())
		{
			s_listenerManager->OnListenersUpdated.Broadcast();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UWorldSoundListener::UpdatePosition()
{
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkSoundPosition SoundPosition;
		FQuat OrientationQuat(GetComponentRotation());
		AkAudioDevice->FVectorsToAKWorldTransform(
			GetComponentLocation(), OrientationQuat.GetForwardVector(), OrientationQuat.GetUpVector(), SoundPosition);
		AkAudioDevice->SetPosition(this, SoundPosition);
	}
}

void UWorldSoundListener::UpdateWorldListener()
{
	UpdateSoundEmitterSendLevels(this);
}

void UWorldSoundListener::UpdateSoundEmitterSendLevels(UAkComponent* AkComponent)
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	int auxCount = 0;

	TArray<UWorldSoundListener*> keys;
	s_worldListenersAuxBusParams.GetKeys(keys);

	for (UWorldSoundListener* worldListener : keys)
	{
		auxCount += s_worldListenersAuxBusParams[worldListener].Num();
	}

	auto pAuxSendValues = (AkAuxSendValue*)alloca(auxCount * sizeof(AkAuxSendValue));

	int index = 0;
	for (UWorldSoundListener* worldListener : keys)
	{
		for (TPair<UAkAuxBus*, FAuxBusParams> auxBusParams : s_worldListenersAuxBusParams[worldListener])
		{
			float controlValueFactor = 1.f;
			const FVector relativePos = AkComponent->GetComponentLocation() - worldListener->GetComponentLocation();

			if (auxBusParams.Value.Directivity > 0)
			{
				const float dotProduct = FVector::DotProduct(worldListener->GetForwardVector().GetSafeNormal(), relativePos.GetSafeNormal());
				controlValueFactor = (dotProduct + 1.f) / 2.f;
			}

			pAuxSendValues[index].listenerID = auxBusParams.Value.BusAkGameObject->GetAkGameObjectID();
			pAuxSendValues[index].auxBusID = auxBusParams.Key->GetWwiseShortID();
			pAuxSendValues[index].fControlValue = auxBusParams.Value.SendLevel* controlValueFactor;

			index++;
			/*WR_DBG_STATIC_FUNC(Log, "%s sent to %s, level = %f", *AkComponent->GetOwner()->GetName(),
				*auxBusParams.Key->GetName(), pAuxSendValues[index - 1].fControlValue * 100.f)*/
		}
	}

	SoundEngine->SetGameObjectAuxSendValues(AkComponent->GetAkGameObjectID(), pAuxSendValues, auxCount);
	//SoundEngine->SetGameObjectOutputBusVolume(GetAkGameObjectID(), GetAkGameObjectID(), 0.f);*/
}

void UWorldSoundListener::UpdateAuxEmitterCompConnections(UAkAuxBus* AuxBus)
{
	if (!m_auxConfigurations.Contains(AuxBus)) { return; }
	//WR_DBG_FUNC(Error, "worldlistener: %s, auxbus: %s", *GetOwner()->GetName(), *AuxBus->GetName())

	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	UAkComponentSet auxEmitterAkCompsToConnect;

	for (TWeakObjectPtr<UAuxSoundEmitterComponent> auxSoundEmitter : m_auxConfigurations[AuxBus].AuxSoundEmitters)
	{
		if (IsValid(auxSoundEmitter->m_AkComp))
		{
			auxEmitterAkCompsToConnect.Add(auxSoundEmitter->m_AkComp);
		}
	}

	const int countGlobals = m_auxConfigurations[AuxBus].GlobalSoundObjects.Num();
	const int countAkComps = auxEmitterAkCompsToConnect.Num();
	const int countConnections = countGlobals + countAkComps;

	auto pReceiverIds = (AkGameObjectID*)alloca(countConnections * sizeof(AkGameObjectID));

	TArray<TWeakObjectPtr<UAkGameObject>> globalSoundObjectsToConnectArray = m_auxConfigurations[AuxBus].GlobalSoundObjects.Array();
	for (int i = 0; i < countGlobals; i++)
	{
		pReceiverIds[i] = globalSoundObjectsToConnectArray[i]->GetAkGameObjectID();
	}

	TArray<TWeakObjectPtr<UAkComponent>> auxEmittersToConnectArray = auxEmitterAkCompsToConnect.Array();
	for (int i = 0; i < countAkComps; i++)
	{
		pReceiverIds[i + countGlobals] = auxEmittersToConnectArray[i]->GetAkGameObjectID();
	}

	SoundEngine->SetListeners(m_auxConfigurations[AuxBus].BusAkGameObject->GetAkGameObjectID(), pReceiverIds, countConnections);
}

UWorldSoundListener* UWorldSoundListener::AddWorldListener(const UObject* Context, float MaxSpeed, USceneComponent* AttachToComponent, const FName Socket)
{
	if (!IsValid(Context) || !IsValid(AttachToComponent)) { return nullptr; }

	if (UWorld* world = Context->GetWorld())
	{
		UWorldSoundListener* newListener = NewObject<UWorldSoundListener>(AttachToComponent, *AttachToComponent->GetName().Append(TEXT(".WorldListener")));
		newListener->RegisterComponentWithWorld(world);
		newListener->AttachToComponent(AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform, Socket);
		newListener->MaxSpeed = MaxSpeed;

		return newListener;
	}

	return nullptr;
}

bool UWorldSoundListener::RemoveWorldListener(UWorldSoundListener* WorldListener)
{
	if (!IsValid(WorldListener)) { return false; }

	UAudioSubsystem::Get(WorldListener)->GetListenerManager()->RemoveWorldListener(WorldListener);

	WorldListener->DestroyComponent();

	return true;
}

FAuxBusComps UWorldSoundListener::RouteToAuxBus(UAkAuxBus* AuxBus, const TSet<USceneComponent*>& AttachToComponents, const FName Socket,
	const FName AuxBusGroupName, const float AuxSendLevelInPercent, const float AttenuationRange, const float a_MaxSpeed, const float Directivity)
{
	if (!IsValid(AuxBus))
	{
		WR_DBG_FUNC(Error, "no aux assigned", *AuxBus->GetName());
		return FAuxBusComps{};
	}

	if (m_auxConfigurations.Contains(AuxBus))
	{
		WR_DBG_FUNC(Warning, "worldlistener already routed to aux bus %s", *AuxBus->GetName());

		TSet<UAuxSoundEmitterComponent*> auxSoundEmitters{};
		UAudioUtils::WeakObjectPtrTSetToTSet(m_auxConfigurations[AuxBus].AuxSoundEmitters, auxSoundEmitters);

		TSet<UAkGameObject*> globalSoundObjects{};
		UAudioUtils::WeakObjectPtrTSetToTSet(m_auxConfigurations[AuxBus].GlobalSoundObjects, globalSoundObjects);

		return FAuxBusComps(m_auxConfigurations[AuxBus].PreSendAkGameObject, m_auxConfigurations[AuxBus].BusAkGameObject,
			auxSoundEmitters, globalSoundObjects);
	}

	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return FAuxBusComps{}; }

	const float auxLevel = FMath::Clamp(AuxSendLevelInPercent / 100.f, 0.f, 1.f);

	// aux bus AkGameObjects
	FString auxBusName = FString::Printf(TEXT("[Aux Send] %s"), *AuxBus->GetName());
	UAkGameObject* busAkGameObject = NewObject<UAkComponent>(this, *auxBusName);
	busAkGameObject->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	busAkGameObject->RegisterComponentWithWorld(GetWorld());
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->RegisterGameObject(busAkGameObject->GetAkGameObjectID(), *auxBusName);
	}

	auxBusName = FString::Printf(TEXT("[Aux Pre-Send] %s"), *AuxBus->GetName());
	UAkGameObject* preSendAkGameObject = NewObject<UAkComponent>(this, *auxBusName);
	preSendAkGameObject->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	preSendAkGameObject->RegisterComponentWithWorld(GetWorld());
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->RegisterGameObject(preSendAkGameObject->GetAkGameObjectID(), *auxBusName);
	}

	auto pAuxSendValues = (AkAuxSendValue*)alloca(sizeof(AkAuxSendValue));
	pAuxSendValues[0].listenerID = busAkGameObject->GetAkGameObjectID();
	pAuxSendValues[0].auxBusID = AuxBus->GetWwiseShortID();
	pAuxSendValues[0].fControlValue = auxLevel;

	const AkGameObjectID preSendId = preSendAkGameObject->GetAkGameObjectID();
	SoundEngine->SetListeners(preSendId, (AkGameObjectID*)alloca(0), 0);
	SoundEngine->SetGameObjectAuxSendValues(preSendId, pAuxSendValues, 1);

	auxBusName = FString::Printf(TEXT("[Aux Post-Send] %s"), *AuxBus->GetName());
	UAkGameObject* postSendAkGameObject = NewObject<UAkComponent>(this, *auxBusName);
	postSendAkGameObject->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	postSendAkGameObject->RegisterComponentWithWorld(GetWorld());
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->RegisterGameObject(postSendAkGameObject->GetAkGameObjectID(), *auxBusName);
	}

	const AkGameObjectID postSendId = postSendAkGameObject->GetAkGameObjectID();
	AkGameObjectID* pBusObjID = (AkGameObjectID*)alloca(sizeof(AkGameObjectID));
	pBusObjID[0] = busAkGameObject->GetAkGameObjectID();

	SoundEngine->SetListeners(postSendId, pBusObjID, 1);
	//SoundEngine->SetGameObjectOutputBusVolume(preSendId, preSendId, 0.f);

	// add aux bus configuration
	TSet<TWeakObjectPtr<UAkGameObject>> globalSoundObjectsToConnect{};
	TSet<TWeakObjectPtr<UAuxSoundEmitterComponent>> auxEmittersToConnect{};

	for (USceneComponent* attachToComponent : AttachToComponents)
	{
		// global sound objects
		if (attachToComponent->IsA<UAkGameObject>())
		{
			UAkGameObject* attachToSoundObject = Cast<UAkGameObject>(attachToComponent);

			if (UAudioSubsystem::Get(this)->GetGlobalSoundEmitterManager()->GetAllGlobalSoundObjects().Contains(attachToSoundObject))
			{
				globalSoundObjectsToConnect.Emplace(attachToSoundObject);
				continue;
			}
		}

		// auxsoundemitter comps
		bool wasCompCreated;
		UAuxSoundEmitterComponent* auxBusAttachComp =
			UAuxSoundEmitterComponent::GetAttachedAuxSoundEmitterComponent(attachToComponent, wasCompCreated, Socket);

		auxBusAttachComp->AddAuxBus(AuxBus, FAuxBusConfig{ this, busAkGameObject, AuxBusGroupName, auxLevel, AttenuationRange});
		auxEmittersToConnect.Emplace(auxBusAttachComp);
	}

	const TPair<UAkAuxBus*, FAuxBusParams> auxConfig = TPair<UAkAuxBus*, FAuxBusParams>(AuxBus,
		FAuxBusParams(busAkGameObject, preSendAkGameObject, postSendAkGameObject, auxEmittersToConnect, globalSoundObjectsToConnect,
			auxLevel, AttenuationRange, Directivity, AuxBusGroupName));

	m_auxConfigurations.Add(auxConfig);
	s_worldListenersAuxBusParams[this].Add(auxConfig);

	// connect attached sound objects
	UpdateAuxEmitterCompConnections(AuxBus);
	/*const int countGlobals = globalSoundObjectsToConnect.Num();
	const int countAkComps = auxEmittersToConnect.Num();
	const int countConnections = countGlobals + countAkComps;

	auto pReceiverIds = (AkGameObjectID*)alloca(countConnections * sizeof(AkGameObjectID));

	TArray<TWeakObjectPtr<UAkGameObject>> globalSoundObjectsToConnectArray = globalSoundObjectsToConnect.Array();
	for (int i = 0; i < countGlobals; i++)
	{
		pReceiverIds[i] = globalSoundObjectsToConnectArray[i]->GetAkGameObjectID();
	}

	TArray<TWeakObjectPtr<UAuxSoundEmitterComponent>> auxEmittersToConnectArray = auxEmittersToConnect.Array();
	for (int i = 0; i < countAkComps; i++)
	{
		pReceiverIds[i + countGlobals] = auxEmittersToConnectArray[i]->m_AkComp->GetAkGameObjectID();
	}

	SoundEngine->SetListeners(busAkGameObject->GetAkGameObjectID(), pReceiverIds, countConnections);*/

	// update sends to AuxBusAkGameObjects
	TArray<UAkAuxBus*> keys;
	m_auxConfigurations.GetKeys(keys);
	const int auxCount = keys.Num();

	pAuxSendValues = (AkAuxSendValue*)alloca(auxCount * sizeof(AkAuxSendValue));

	for (int i = 0; i < auxCount; i++)
	{
		pAuxSendValues[i].listenerID = m_auxConfigurations[keys[i]].BusAkGameObject->GetAkGameObjectID();
		pAuxSendValues[i].auxBusID = keys[i]->GetWwiseShortID();
		pAuxSendValues[i].fControlValue = m_auxConfigurations[keys[i]].SendLevel;
	}

	SoundEngine->SetListeners(GetAkGameObjectID(), (AkGameObjectID*)alloca(0), 0);
	SoundEngine->SetGameObjectAuxSendValues(GetAkGameObjectID(), pAuxSendValues, auxCount);
	//SoundEngine->SetGameObjectOutputBusVolume(GetAkGameObjectID(), GetAkGameObjectID(), 0.f);

	TSet<UAuxSoundEmitterComponent*> auxSoundEmitters{};
	UAudioUtils::WeakObjectPtrTSetToTSet(auxEmittersToConnect, auxSoundEmitters);

	TSet<UAkGameObject*> globalSoundObjects{};
	UAudioUtils::WeakObjectPtrTSetToTSet(globalSoundObjectsToConnect, globalSoundObjects);

	if (s_listenerManager->OnListenersUpdated.IsBound())
	{
		s_listenerManager->OnListenersUpdated.Broadcast();
	}

	return FAuxBusComps(preSendAkGameObject, postSendAkGameObject, auxSoundEmitters, globalSoundObjects);
}

bool UWorldSoundListener::RemoveAuxBus(UAkAuxBus* AuxBus)
{
	if (!m_auxConfigurations.Contains(AuxBus)) { return false; }

	for (TWeakObjectPtr<UAuxSoundEmitterComponent> auxEmitterComp : m_auxConfigurations[AuxBus].AuxSoundEmitters)
	{
		auxEmitterComp->RemoveAuxBus(AuxBus);
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->UnregisterComponent(m_auxConfigurations[AuxBus].BusAkGameObject->GetAkGameObjectID());
	}

	m_auxConfigurations.Remove(AuxBus);

	if (s_worldListenersAuxBusParams.Contains(this))
	{
		s_worldListenersAuxBusParams[this].Remove(AuxBus);
	}

	if (s_listenerManager->OnListenersUpdated.IsBound())
	{
		s_listenerManager->OnListenersUpdated.Broadcast();
	}
	//UpdateListenerAssociations();

	return true;
}

void UWorldSoundListener::SetAuxLevel(UAkAuxBus* AuxBus, const float AuxSendPercent)
{
	if (!m_auxConfigurations.Contains(AuxBus))
	{
		WR_DBG_FUNC(Error, "worldlistener not routed to aux bus %s", *AuxBus->GetName());
		return;
	}

	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	const float auxLevel = FMath::Clamp(AuxSendPercent / 100.f, 0.f, 1.f);

	if (m_auxConfigurations[AuxBus].SendLevel != auxLevel)
	{
		m_auxConfigurations[AuxBus].SendLevel = auxLevel;

		auto pAuxSendValues = (AkAuxSendValue*)alloca(sizeof(AkAuxSendValue));

		pAuxSendValues[0].listenerID = m_auxConfigurations[AuxBus].BusAkGameObject->GetAkGameObjectID();
		pAuxSendValues[0].auxBusID = AuxBus->GetWwiseShortID();
		pAuxSendValues[0].fControlValue = auxLevel;

		const AkGameObjectID preSendId = m_auxConfigurations[AuxBus].PreSendAkGameObject->GetAkGameObjectID();

		SoundEngine->SetGameObjectAuxSendValues(preSendId, pAuxSendValues, 1);
		SoundEngine->SetGameObjectOutputBusVolume(preSendId, preSendId, 0.f);

		UpdateWorldListener();
	}
}

void UWorldSoundListener::SetMaxSpeed(const float NewMaxSpeed)
{
	const bool mustBroadcast = NewMaxSpeed > MaxSpeed;
	MaxSpeed = NewMaxSpeed;

	if (mustBroadcast)
	{
		USoundListenerManager* listenerManager = UAudioSubsystem::Get(this)->GetListenerManager();

		if (listenerManager->OnMaxSpeedIncreased.IsBound())
		{
			listenerManager->OnMaxSpeedIncreased.Broadcast();
		}
	}
}

UAkGameObject* UWorldSoundListener::GetBusSendAkComp(const UAkAuxBus* AuxBus, bool& WasFound) const
{
	if (m_auxConfigurations.Contains(AuxBus))
	{
		WasFound = true;
		return m_auxConfigurations[AuxBus].BusAkGameObject;
	}

	WasFound = false;
	return nullptr;
}
#pragma endregion
