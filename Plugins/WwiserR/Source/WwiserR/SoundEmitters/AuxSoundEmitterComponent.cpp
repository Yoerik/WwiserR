// Copyright Yoerik Roevens. All Rights Reserved.(c)


#include "SoundEmitters/AuxSoundEmitterComponent.h"
#include "Managers/SoundListenerManager.h"
#include "WorldSoundListenerComponent.h"
#include "Core/AudioUtils.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "AkAuxBus.h"

#if !UE_BUILD_SHIPPING
#include "Config/AudioConfig.h"
#include "Config/DebugTheme.h"
#endif

#pragma region CVars
namespace Private_AuxSoundEmitterComponent
{
	static TAutoConsoleVariable<bool> CVar_AuxBusEmitter_DebugAuxBusGroupName(TEXT("WwiserR.AuxSoundEmitter.DebugAuxBusGroupName"),
		true, TEXT("Show aux bus group name instead of aux bus name in visual debugging. (0 = off, 1 = on)"), ECVF_Cheat);

	bool bDebugAuxBusGroupName = true;

	static void OnAuxSoundEmitterComponent()
	{
		bDebugAuxBusGroupName = CVar_AuxBusEmitter_DebugAuxBusGroupName.GetValueOnGameThread();
	}

	FAutoConsoleVariableSink CSoundEmitterComponentBaseConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnAuxSoundEmitterComponent));
} // namespace Private_AuxSoundEmitterComponent
#pragma endregion


UAuxSoundEmitterComponent::UAuxSoundEmitterComponent()
{
	SetIsReplicated(false);

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bAutoEmitterMaxSpeed = true;
}

void UAuxSoundEmitterComponent::AddAuxBus(UAkAuxBus* AuxBus, const FAuxBusConfig& AuxBusConfig)
{
	m_auxBusses.Add(TPair<UAkAuxBus*, FAuxBusConfig>{AuxBus, AuxBusConfig});
	UpdateAuxEmitterAttenuationRange();
	//UpdateAuxEmitterMaxSpeed_Internal();

	//m_playingAuxBusses.Emplace(AuxBus);
	CullAuxBus();

	//UpdateListenerConnections();
	//UpdateSendLevels();
}

void UAuxSoundEmitterComponent::RemoveAuxBus(UAkAuxBus* AuxBus)
{
	m_auxBusses.Remove(AuxBus);
	//m_playingAuxBusses.Remove(AuxBus);
	UpdateAuxEmitterAttenuationRange();
	//UpdateAuxEmitterMaxSpeed_Internal();

	if (m_auxBusses.IsEmpty())
	{
		m_auxCullingTimerHandle.Invalidate();
		m_forceRegistration = false;
		UpdateNeverUnregister();
	}
	else
	{
		CullAuxBus();
	}
}

/*void UAuxSoundEmitterComponent::SetNeverUnregister(bool bMustNeverUnregister)
{
	if (bNeverUnregister == bMustNeverUnregister) { return; }

	bNeverUnregister = bMustNeverUnregister;

	if (m_auxBusses.IsEmpty())
	{
		m_neverUnregister = bNeverUnregister;
		UpdateNeverUnregister();
	}
}*/

void UAuxSoundEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	//const FName funcCullAuxBus{ TEXT("CullAuxBus") };
	m_auxCullingTimerDelegate.BindUObject(this, &UAuxSoundEmitterComponent::CullAuxBus);
	s_listenerManager->OnAttenuationReferenceChanged.AddUObject(this, &UAuxSoundEmitterComponent::CullAuxBus);
	//m_neverUnregisterParent = bNeverUnregister;
	//SetNeverUnregister(true);
}

void UAuxSoundEmitterComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	m_auxCullingTimerHandle.Invalidate();

	Super::EndPlay(EndPlayReason);
}

void UAuxSoundEmitterComponent::CullAuxBus()
{
	const bool shouldPlay = !m_cullAuxBusses || IsInAuxListenerRange();

	/*WR_DBG_FUNC(Log, "in aux listener range = %s, m_isAuxActive = %s", shouldPlay ? TEXT("true") : TEXT("false"),
		m_isAuxActive ? TEXT("true") : TEXT("false"));*/

	if (m_isAuxActive != shouldPlay)
	{
		WR_DBG_FUNC(Log, "%s", shouldPlay ? TEXT("enabling aux busses") : TEXT("disabling aux busses"));

		m_isAuxActive = shouldPlay && !m_auxBusses.IsEmpty();
		m_forceRegistration = m_isAuxActive;
		UpdateNeverUnregister();

		if (m_isAuxActive)
		{
			SetComponentTickEnabled(true);
			CreateAkComponentIfNeeded();
		}
		else
		{
			TryToStopTicking();
			UnregisterEmitterIfInactive();
		}

		TArray<UAkAuxBus*> keys;
		m_auxBusses.GetKeys(keys);

		for (UAkAuxBus* auxBus : keys)
		{
			m_auxBusses[auxBus].WorldSoundListener->UpdateAuxEmitterCompConnections(auxBus);
		}
	}

	//SetNeverUnregister(IsInAuxListenerRange());
	UWorld* world = GetWorld();
	const float nextBusCullInterval = m_cullAuxBusses ? CalculateNextAuxBusCullTime() - world->GetTimeSeconds() : INFINITY;

	WR_DBG_FUNC(Log, "next aux bus culling in %f seconds", nextBusCullInterval);

	FTimerManager& timerManager = world->GetTimerManager();
	timerManager.ClearTimer(m_auxCullingTimerHandle);

	if (nextBusCullInterval < INFINITY)
	{
		timerManager.SetTimer(m_auxCullingTimerHandle, m_auxCullingTimerDelegate, nextBusCullInterval, false);
	}
}

void UAuxSoundEmitterComponent::RecullAllLoops()
{
	Super::RecullAllLoops();
	CullAuxBus();
}

void UAuxSoundEmitterComponent::UpdateDistanceCullingRelativeMaxSpeed()
{
	Super::UpdateDistanceCullingRelativeMaxSpeed();
	CullAuxBus();
}

float UAuxSoundEmitterComponent::CalculateNextAuxBusCullTime()
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AkAudioDevice)) { return 0.f; }

	const FVector compLocation = GetComponentLocation();
	const float currentTime = GetWorld()->GetTimeSeconds();
	float nextCullTime = INFINITY;
	float maxRelativeSpeed = 0.f;

	TArray<UAkAuxBus*> keys;
	m_auxBusses.GetKeys(keys);

	const float cullRange = FMath::Sqrt(m_squaredAuxAttRange);

	// default listeners
	for (const TWeakObjectPtr<UAkComponent> defaultListener : AkAudioDevice->GetDefaultListeners())
	{
		if (defaultListener == AkAudioDevice->GetSpatialAudioListener())
		{
			maxRelativeSpeed = m_emitterMaxSpeed + s_listenerManager->GetDistanceProbeMaxSpeed();

			if (maxRelativeSpeed > 0.f)
			{
				const float minDistanceToTravel = FMath::Abs(
					FMath::Sqrt(s_listenerManager->GetSquaredDistanceToDistanceProbe(compLocation)) - cullRange);

				const float nextBusCullTime = currentTime + minDistanceToTravel / maxRelativeSpeed;

				if (nextBusCullTime < nextCullTime)
				{
					nextCullTime = nextBusCullTime;
				}
			}
		}
		else
		{
			maxRelativeSpeed = m_emitterMaxSpeed + s_listenerManager->GetDefaultListenerMaxSpeed();

			if (maxRelativeSpeed > 0.f)
			{
				const float minDistanceToTravel = FMath::Abs(
					FVector::Distance(compLocation, defaultListener->GetComponentLocation()) - cullRange);
				const float nextBusCullTime = currentTime + minDistanceToTravel / maxRelativeSpeed;

				if (nextBusCullTime < nextCullTime)
				{
					nextCullTime = nextBusCullTime;
				}
			}
		}
	}

	// world listeners
	for (const TWeakObjectPtr<UWorldSoundListener> worldListener : GetWorldListeners())
	{
		maxRelativeSpeed = m_emitterMaxSpeed + worldListener->GetMaxSpeed();

		if (maxRelativeSpeed > 0.f)
		{
			const float minDistanceToTravel = FMath::Abs(
				FVector::Distance(compLocation, worldListener->GetComponentLocation()) - cullRange);

			const float nextBusCullTime = currentTime + minDistanceToTravel / maxRelativeSpeed;

			if (nextBusCullTime < nextCullTime)
			{
				nextCullTime = nextBusCullTime;
			}
		}
	}

	return nextCullTime;
}

bool UAuxSoundEmitterComponent::IsInAuxListenerRange()
{
	//if (m_auxBusses.IsEmpty()) { return; }

	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AkAudioDevice)) { return false; }

	const FVector compLocation = GetComponentLocation();

	for (const TWeakObjectPtr<UAkComponent> defaultListener : AkAudioDevice->GetDefaultListeners())
	{
		if (defaultListener == AkAudioDevice->GetSpatialAudioListener())
		{
			//WR_DBG_FUNC(Error, "%s : %f   AuxAttRange = %f", TEXT("SpatialAudioListener"),
				//FMath::Sqrt((float)s_listenerManager->GetSquaredDistanceToDistanceProbe(compLocation)), FMath::Sqrt(m_squaredAuxAttRange));

			if (s_listenerManager->GetSquaredDistanceToDistanceProbe(compLocation) < m_squaredAuxAttRange)
			{
				return true;
			}
		}
		else
		{
			if (FVector::DistSquared(compLocation, defaultListener->GetComponentLocation()) < m_squaredAuxAttRange)
			{
				//WR_DBG_FUNC(Error, "%s : %f   AuxAttRange = %f", *defaultListener->GetName(),
					//FMath::Sqrt((float)FVector::DistSquared(compLocation, defaultListener->GetComponentLocation())), FMath::Sqrt(m_squaredAuxAttRange));
				return true;
			}
		}
	}

	for (const TWeakObjectPtr<UWorldSoundListener> worldListener : GetWorldListeners())
	{
		//WR_DBG_FUNC(Error, "%s : %f   AuxAttRange = %f", *worldListener->GetOwner()->GetName(),
			//FMath::Sqrt((float)FVector::DistSquared(compLocation, worldListener->GetComponentLocation())), FMath::Sqrt(m_squaredAuxAttRange));

		if (FVector::DistSquared(compLocation, worldListener->GetComponentLocation()) < m_squaredAuxAttRange)
		{
			return true;
		}
	}

	return false;
}

void UAuxSoundEmitterComponent::UpdateAuxEmitterAttenuationRange()
{
	m_squaredAuxAttRange = 0.f;

	if (m_auxBusses.IsEmpty()) { return; }

	TArray<UAkAuxBus*> keys;
	m_auxBusses.GetKeys(keys);

	for (const UAkAuxBus* auxBus : keys)
	{
		if (m_auxBusses[auxBus].SquaredAttenuationRange <= 0.f)
		{
			m_squaredAuxAttRange = 0.f;
			return;
		}

		if (m_auxBusses[auxBus].SquaredAttenuationRange > m_squaredAuxAttRange)
		{
			m_squaredAuxAttRange = m_auxBusses[auxBus].SquaredAttenuationRange;
		}
	}
}

/*void UAuxSoundEmitterComponent::UpdateAuxEmitterMaxSpeed_Internal()
{
	m_auxEmitterMaxSpeed = 0.f;

	if (m_auxBusses.IsEmpty()) { return; }

	TArray<UAkAuxBus*> keys;
	m_auxBusses.GetKeys(keys);

	for (const UAkAuxBus* auxBus : keys)
	{
		if (m_auxBusses[auxBus].MaxSpeed > m_auxEmitterMaxSpeed)
		{
			m_auxEmitterMaxSpeed = m_auxBusses[auxBus].MaxSpeed;
		}
	}
}*/

void UAuxSoundEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	//UpdateListenerConnections();
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAuxSoundEmitterComponent::InitializeAkComponent()
{
	Super::InitializeAkComponent();

	m_AkComp->OcclusionRefreshInterval = 0.f;
	//UpdateListenerConnections();
	//UpdateSendLevels();
}

void UAuxSoundEmitterComponent::UpdateListenerConnections()
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();

	UAkComponentSet listenersToConnect{};
	FVector compLocation = GetComponentLocation();

	// spatial audio listener
	listenersToConnect.Emplace(s_listenerManager->GetSpatialAudioListener());

	// default listeners
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		for (TWeakObjectPtr<UAkComponent> defaultListener : AkAudioDevice->GetDefaultListeners())
		{
			if (defaultListener.Get() == AkAudioDevice->GetSpatialAudioListener())
			{
				continue;
			}

			listenersToConnect.Emplace(defaultListener);
		}
	}

	// world listeners
	for (TWeakObjectPtr<UWorldSoundListener> worldListener : GetWorldListeners())
	{
		listenersToConnect.Emplace(worldListener);
	}

	if (listenersToConnect.IsEmpty())
	{
		if (IsValid(m_AkComp))
		{
			auto pListenersIds = (AkGameObjectID*)alloca(0);
			SoundEngine->SetListeners(m_AkComp->GetAkGameObjectID(), pListenersIds, listenersToConnect.Num());
			SoundEngine->SetGameObjectAuxSendValues(m_AkComp->GetAkGameObjectID(), NULL, 0);
		}
	}
	else
	{
		if (IsValid(m_AkComp))
		{
			if (!UAudioUtils::AreTSetsEqual(m_connectedListeners, listenersToConnect))
			{
				auto pListenersIds = (AkGameObjectID*)alloca(listenersToConnect.Num() * sizeof(AkGameObjectID));
				auto listenersToConnectArray = listenersToConnect.Array();

				for (int i = 0; i < listenersToConnectArray.Num(); i++)
				{
					pListenersIds[i] = listenersToConnectArray[i]->GetAkGameObjectID();
				}

				// this -> in world listeners
				SoundEngine->SetListeners(m_AkComp->GetAkGameObjectID(), pListenersIds, listenersToConnect.Num());
				m_connectedListeners = listenersToConnect;
			}
		}
	}
}

TSet<TWeakObjectPtr<UWorldSoundListener>> UAuxSoundEmitterComponent::GetWorldListeners()
{
	TSet<TWeakObjectPtr<UWorldSoundListener>> worldListeners = s_listenerManager->GetWorldListeners();

	/*for (TPair<UAkAuxBus*, FAuxBusConfig> auxBus : m_auxBusses)
	{
		worldListeners.Remove(auxBus.Value.WorldSoundListener);
	}*/

	return worldListeners;
}

#if !UE_BUILD_SHIPPING
bool UAuxSoundEmitterComponent::TryToStopTicking()
{
	if (s_debugDraw && !m_auxBusses.IsEmpty()) { return false; }

	return Super::TryToStopTicking();
}

void UAuxSoundEmitterComponent::DebugDrawOnTick(UWorld* World)
{
	if (!IsValid(World)) { return; }

	TSet<FString> displayedAuxNames{};

	// aux bus names
	if (m_isAuxActive)
	{
		TArray<UAkAuxBus*> keys;
		m_auxBusses.GetKeys(keys);

		for (UAkAuxBus* auxBus : keys)
		{
			const FString auxBusDisplayName = Private_AuxSoundEmitterComponent::bDebugAuxBusGroupName && !m_auxBusses[auxBus].AuxBusGroupName.IsNone()
				? m_auxBusses[auxBus].AuxBusGroupName.ToString() : auxBus->GetName();

			if (!displayedAuxNames.Contains(auxBusDisplayName))
			{
				m_msgEmitterDebugPersistent.Append(FString::Printf(TEXT("[Aux] %s\n"), *auxBusDisplayName));
				displayedAuxNames.Emplace(auxBusDisplayName);
			}
		}
	}

	Super::DebugDrawOnTick(World);
}

UPARAM(DisplayName = "Aux Sound Emitter")UAuxSoundEmitterComponent* UAuxSoundEmitterComponent::GetAttachedAuxSoundEmitterComponent(
	USceneComponent* AttachToComponent, bool& ComponentCreated, FName Socket, EAttachLocation::Type LocationType)
{
	if (!IsValid(AttachToComponent))
	{
		ComponentCreated = false;
		return nullptr;
	}

	// look for matching existing aux comp
	TArray<USceneComponent*> attachChildrenComps;
	AttachToComponent->GetChildrenComponents(true, attachChildrenComps);

	for (USceneComponent* attachChildComp : attachChildrenComps)
	{
		if (attachChildComp->IsA<UAuxSoundEmitterComponent>() && attachChildComp->GetAttachSocketName() == Socket)
		{
			return Cast<UAuxSoundEmitterComponent>(attachChildComp);
		}
	}

	const FName nameAuxAttachComp{ TEXT("[Aux Return]") };
	UAuxSoundEmitterComponent* auxBusAttachComp = NewObject<UAuxSoundEmitterComponent>(AttachToComponent, nameAuxAttachComp);
	auxBusAttachComp->RegisterComponentWithWorld(AttachToComponent->GetWorld());
	auxBusAttachComp->AttachToComponent(AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform, Socket);

	return auxBusAttachComp;
}
void UAuxSoundEmitterComponent::SetCullingForAuxBusses(bool bMustCullAuxBusses)
{
	if (m_cullAuxBusses == bMustCullAuxBusses) { return; }

	m_cullAuxBusses = bMustCullAuxBusses;
	CullAuxBus();
}
#endif
