// Copyright Yoerik Roevens. All Rights Reserved.(c)


#include "Managers/AmbientBedManager.h"
#include "Managers/SoundListenerManager.h"
#include "DataAssets/DA_AmbientBed.h"
#include "Core/AudioSubsystem.h"
#include "Core/AudioUtils.h"
#include "Config/AudioConfig.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "AkRoomComponent.h"
#include "AkAudioEvent.h"
#include "AkAuxBus.h"

namespace Private_AmbientBeds
{
	static TAutoConsoleVariable<bool> CVar_AmbientSoundWeight_DebugDrawSpatialization(
		TEXT("WwiserR.AmbientBeds.DebugDraw.Spatialization"), false, TEXT("Visual debugging. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_AmbientSoundWeight_DebugDrawWeights(
		TEXT("WwiserR.AmbientBeds.DebugDraw.Weights"), false, TEXT("Show Ambient Sound Bed Weights. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_AmbientSoundWeight_ViewportStats(
		TEXT("WwiserR.AmbientBeds.ViewportStats"), false, TEXT("Show RTPC values in viewport. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_AmbientSoundWeight_ConsoleStats(
		TEXT("WwiserR.AmbientBeds.ConsoleStats"), false, TEXT("Show octree stats in console. (0 = off, 1 = on)"), ECVF_Cheat);

	bool bDebugDrawSpatialization = false;
	bool bDebugDrawWeights = false;
	bool bViewPortStats = false;
	bool bConsoleStats = false;

	static void OnAmbientSoundWeightManager()
	{
		bDebugDrawSpatialization = CVar_AmbientSoundWeight_DebugDrawSpatialization.GetValueOnGameThread();
		bViewPortStats = CVar_AmbientSoundWeight_ViewportStats.GetValueOnGameThread();
		bDebugDrawWeights = CVar_AmbientSoundWeight_DebugDrawWeights.GetValueOnGameThread();
		bConsoleStats = CVar_AmbientSoundWeight_ConsoleStats.GetValueOnGameThread();
	}

	FAutoConsoleVariableSink CStaticAmbientBedsConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnAmbientSoundWeightManager));
} // Private_AmbientBeds

#pragma region TAmbientWeightOctree
FAmbientWeightOctreeElement::FAmbientWeightOctreeElement(UAmbientBedWeightComponent* a_AmbientSoundWeightComponent)
	: AmbientSoundWeightComponent(a_AmbientSoundWeightComponent)
	, BoundingBox(FBoxCenterAndExtent(a_AmbientSoundWeightComponent->GetComponentLocation(), FVector(1.f, 1.f, 1.f)))
{}

void FAmbientWeightOctreeSemantics::SetElementId(FOctree& OctreeOwner, const FAmbientWeightOctreeElement& Element, FOctreeElementId2 Id)
{
	static_cast<TAmbientWeightOctree&>(OctreeOwner).ObjectToOctreeId.Add(Element.AmbientSoundWeightComponent->GetUniqueID(), Id);
}

void TAmbientWeightOctree::AddWeight(UAmbientBedWeightComponent* AmbientSoundWeightComponent)
{
	const uint32 weightID = AmbientSoundWeightComponent->GetUniqueID();

	AddElement(FAmbientWeightOctreeElement{ AmbientSoundWeightComponent });
	NumElements++;

#if !UE_BUILD_SHIPPING
	DebugConsoleStats(AmbientSoundWeightComponent);
#endif
}

void TAmbientWeightOctree::RemoveWeight(UAmbientBedWeightComponent* AmbientSoundWeightComponent)
{
	const FOctreeElementId2 elementID = ObjectToOctreeId[AmbientSoundWeightComponent->GetUniqueID()];
	if (!IsValidElementId(elementID)) { return; }

	RemoveElement(elementID);
	NumElements--;

#if !UE_BUILD_SHIPPING
	DebugConsoleStats(AmbientSoundWeightComponent);
#endif
}

void TAmbientWeightOctree::DebugConsoleStats(UAmbientBedWeightComponent* AmbientSoundWeightComponent) const
{
	if (!Private_AmbientBeds::bConsoleStats) { return; }

	FString groupName = AmbientSoundWeightComponent->AmbientBed->GetName();
	if (AmbientSoundWeightComponent->bOverrideAmbientBedGroup)
	{
		groupName.Append(FString::Printf(TEXT("_ID_%i"), AmbientSoundWeightComponent->GroupId));
	}

	WR_DBG_STATIC_FUNC(Log, "%s - elements: %i", *groupName, NumElements);
	//DumpStats();
}
#pragma endregion

#pragma region UAmbientBedEmitterComponent
UAmbientBedEmitterComponent::UAmbientBedEmitterComponent()
{
	SetIsReplicated(false);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PostUpdateWork;
}

void UAmbientBedEmitterComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	//if (EndPlayReason != EEndPlayReason::LevelTransition && EndPlayReason != EEndPlayReason::Quit && EndPlayReason != EEndPlayReason::EndPlayInEditor)
	if (EndPlayReason == EEndPlayReason::RemovedFromWorld)
	{
		Stop();
	}
}

void UAmbientBedEmitterComponent::Initialize(UDA_AmbientBed* AmbientBed, const FString& Name)
{
	m_ambientEmitter = NewObject<UAkComponent>(this, *Name);
	m_ambientEmitter->RegisterComponentWithWorld(GetWorld());
	m_ambientEmitter->SetWorldRotation(AmbientBed->WorldRotation);
	m_ambientEmitter->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
	m_emitterId = m_ambientEmitter->GetAkGameObjectID();

	const float radius = AmbientBed->Radius;
	m_ambientEmitter->SetGameObjectRadius(radius, radius * AmbientBed->InnerVolume);
	m_ambientEmitter->OcclusionRefreshInterval = 0.f;
	m_distanceRtpc = AmbientBed->DistanceRtpc;
	m_weightRtpc = AmbientBed->WeightRtpc;

	if (IsValid(AmbientBed->PropagationAuxBus))
	{
		m_auxBusID = AmbientBed->PropagationAuxBus->GetWwiseShortID();
	}
	else
	{
		WR_DBG_FUNC(Error, "aux bus could not be set. Please assign an aux bus in AmbientBed data asset.")
			m_auxBusID = AK_INVALID_AUX_ID;
	}

	if (IsValid(AmbientBed->PassthroughAuxBusOverride) || IsValid(s_passthroughAuxBus))
	{
		m_passthroughAuxBusID = IsValid(AmbientBed->PassthroughAuxBusOverride) ?
			AmbientBed->PassthroughAuxBusOverride->GetWwiseShortID() :
			s_passthroughAuxBus->GetWwiseShortID();
	}
	else
	{
		WR_DBG_FUNC(Error, "passthrough aux bus could not be set. Please assign a passthrough aux bus in the project settings or AmbientBed data asset.")
			m_passthroughAuxBusID = AK_INVALID_AUX_ID;
	}

	FTimerDelegate startPlay;
	startPlay.BindUObject(this, &UAmbientBedEmitterComponent::StartPlay, AmbientBed);
	GetWorld()->GetTimerManager().SetTimerForNextTick(startPlay);
}

void UAmbientBedEmitterComponent::StartPlay(UDA_AmbientBed* AmbientBed)
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }
	
	// initialize emitter-listener relations
	auto pListenerIds = (AkGameObjectID*)alloca(0);
	SoundEngine->SetListeners(m_emitterId, pListenerIds, 0);
	SoundEngine->SetListeners(m_listenerId, pListenerIds, 0);

	m_isInListenerRoom = IsInListenerRoom();
	m_FadePos = m_isInListenerRoom ? 0.f : 1.f;
	m_isCrossfading = false;

	SetEmitterListenerRelations();

	// start audio
	m_ambientEmitter->PostAkEvent(AmbientBed->LoopEvent);
}

void UAmbientBedEmitterComponent::Stop()
{
	SetComponentTickEnabled(false);
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

	if (IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get())
	{
		SoundEngine->StopAll(m_ambientEmitter->GetAkGameObjectID());
		m_ambientEmitter->DestroyComponent();
		m_ambientEmitter = nullptr;
	}
}

void UAmbientBedEmitterComponent::AccumulatePositionAndDistance(UDA_AmbientBed* AmbientBed)
{
	WR_ASSERT(IsValid(m_ambientEmitter), "!IsValid(m_ambientAkComp)")

	m_localPosition /= m_summedWeight;
	m_ambientEmitter->SetRelativeLocation(m_localPosition);

	m_avgWeightedDistance *= 100.f / m_summedWeight;
	m_avgWeightedDistance = FMath::Clamp(m_avgWeightedDistance, 0.f, 100.f);

	const float accumWeight = m_summedWeight / AmbientBed->MaxAccumulatedWeight;
	if (accumWeight > m_maxAccumWeight)
	{
		m_maxAccumWeight = accumWeight;
	}
	const float weightRtpcValue = FMath::Clamp(accumWeight * (100.f + m_avgWeightedDistance),
		0.f, FMath::Min(100.f, m_maxAccumWeight * 100.f));


	if (IsValid(m_distanceRtpc))
	{
		m_ambientEmitter->SetRTPCValue(m_distanceRtpc, m_avgWeightedDistance, 0, FString());
	}

	if (IsValid(m_weightRtpc))
	{
		m_ambientEmitter->SetRTPCValue(m_weightRtpc, weightRtpcValue, 0, FString());
	}

	if (m_isInListenerRoom != IsInListenerRoom())
	{
		m_isCrossfading = true;
		m_isInListenerRoom = IsInListenerRoom();
	}

	if (m_isCrossfading)
	{
		if (AmbientBed->PortalCrossfadeTime <= 0.f)
		{
			m_FadePos = m_isInListenerRoom ? 0.f : 1.f;
			m_isCrossfading = false;
		}
		else
		{
			if (m_isInListenerRoom)
			{
				m_FadePos -= GetWorld()->DeltaTimeSeconds / AmbientBed->PortalCrossfadeTime;
				if (m_FadePos <= 0.f)
				{
					m_isCrossfading = false;
				}
			}
			else
			{
				m_FadePos += GetWorld()->DeltaTimeSeconds / AmbientBed->PortalCrossfadeTime;
				if (m_FadePos >= 1.f)
				{
					m_isCrossfading = false;
				}
			}
		}
		
		SetEmitterListenerRelations();
	}

#if !UE_BUILD_SHIPPING
	{
		FScopeLock Lock(&AAmbientBedWorldManager::s_critSectDbgValues);
		AAmbientBedWorldManager::s_dbgViewportValues.Add(this,
			FDebugValues(m_avgWeightedDistance, weightRtpcValue, m_summedWeight));
	}
#endif

	m_localPosition = FVector();
	m_avgWeightedDistance = 0.f;
	m_summedWeight = 0.f;
	m_numWeights = 0;
}

void UAmbientBedEmitterComponent::SetEmitterListenerRelations()
{
	if (m_auxBusID == AK_INVALID_AUX_ID || m_passthroughAuxBusID == AK_INVALID_AUX_ID) { return; }
	
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }
	
	m_FadePos = FMath::Clamp(m_FadePos, 0.f, 1.f);
	auto pAuxSendValues = (AkAuxSendValue*)alloca(2 * sizeof(AkAuxSendValue));

	pAuxSendValues[0].listenerID = m_roomId;
	pAuxSendValues[0].auxBusID = m_auxBusID;
	pAuxSendValues[0].fControlValue = m_FadePos; //UAudioUtils::LogaritmicInterpolation(m_FadePos);

	pAuxSendValues[1].listenerID = m_listenerId;
	pAuxSendValues[1].auxBusID = m_passthroughAuxBusID;
	pAuxSendValues[1].fControlValue = 1.f - m_FadePos; // UAudioUtils::LogaritmicInterpolation(1.f - m_FadePos);
		
	SoundEngine->SetGameObjectAuxSendValues(m_emitterId, pAuxSendValues, 2);
	//SoundEngine->SetGameObjectOutputBusVolume(m_emitterId, m_listenerId, 1.f - m_FadePos);
}

bool UAmbientBedEmitterComponent::IsInListenerRoom()
{
	return FAkAudioDevice::Get()->GetSpatialAudioListener()->GetSpatialAudioRoomID() == AkRoomID::FromGameObjectID(m_roomId);
}
#pragma endregion

#pragma region UAmbientBedWeightComponent
UAmbientBedWeightComponent::UAmbientBedWeightComponent()
{
	SetIsReplicated(false);
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAmbientBedWeightComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (IsValid(AmbientBed) && IsValid(AmbientBed->LoopEvent))
	{
		UWorld* world = GetWorld();

		if (Weight > 0.f)
		{
			if (UAmbientBedManager* ambientSoundManager = UAudioSubsystem::Get(world)->GetAmbientSoundManager())
			{
				ambientSoundManager->AddWeight(world, this, AmbientBed);
				m_isInWeightOctree = true;
			}
		}
	}
}

void UAmbientBedWeightComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::RemovedFromWorld 
		&& IsValid(AmbientBed) && IsValid(AmbientBed->LoopEvent))
	{
		UWorld* world = GetWorld();

		if (UAmbientBedManager* ambientSoundManager = UAudioSubsystem::Get(world)->GetAmbientSoundManager())
		{
			ambientSoundManager->RemoveWeight(world, this, AmbientBed);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UAmbientBedWeightComponent::SetWeight(float NewWeight)
{
	Weight = NewWeight;

	if (m_isInWeightOctree && Weight <= 0.f)
	{
		UWorld* world = GetWorld();

		if (UAmbientBedManager* ambientSoundManager = UAudioSubsystem::Get(world)->GetAmbientSoundManager())
		{
			ambientSoundManager->RemoveWeight(world, this, AmbientBed);
			m_isInWeightOctree = false;
		}
	}
	else if (!m_isInWeightOctree && Weight > 0.f)
	{
		UWorld* world = GetWorld();

		if (UAmbientBedManager* ambientSoundManager = UAudioSubsystem::Get(world)->GetAmbientSoundManager())
		{
			ambientSoundManager->AddWeight(world, this, AmbientBed);
			m_isInWeightOctree = true;
		}
	}
}
#pragma endregion

#pragma region AAmbientBedWorldManager
AAmbientBedWorldManager::AAmbientBedWorldManager()
{
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;
}

void AAmbientBedWorldManager::Initialize(USoundListenerManager* SoundListenerManager)
{
	m_listenerManager = SoundListenerManager;
	FAmbientBedGroup::s_colorSeed = WEIGHTCOLORSTARTSEED;

#if !UE_BUILD_SHIPPING
	s_dbgViewportValues.Empty();
#endif
}

void AAmbientBedWorldManager::Deinitialize()
{
	m_listenerManager = nullptr;
}

void AAmbientBedWorldManager::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Deinitialize();
}

void AAmbientBedWorldManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!AkAudioDevice || !SoundEngine)) { return; }

	UWorld* world = GetWorld();

#if !UE_BUILD_SHIPPING
	if (Private_AmbientBeds::bDebugDrawWeights)
	{
		int32 lastCount = m_dbgWeightComps.Num();
		m_dbgWeightComps.Empty(lastCount);
	}
#endif

	const UAkComponent* spatialListener = AkAudioDevice->GetSpatialAudioListener();
	const FVector listenerPosition = spatialListener->GetComponentLocation();
	const FVector distanceProbePosition = m_listenerManager->GetDistanceProbePosition();

	// prepare weight components array for efficient parallel access
	TArray<TPair<FAmbientBedGroup, TSharedPtr<TAmbientWeightOctree>>> m_weightCompsArray;
	m_weightCompsArray.Reserve(m_weightComps.Num());
	for (const auto& weightComp : m_weightComps)
	{
		m_weightCompsArray.Emplace(weightComp);
	}

	TSet<TPair<UAkRoomComponent*, FAmbientBedGroup&>> emittersToCreate;
	TSet<FAmbientBedGroup*> bedGroupsToKeep;
	FCriticalSection critSectEmittersToCreate;

	// process weight components in parallel
	ParallelFor(m_weightCompsArray.Num(), [&](int32 Index)
		{
			auto& pair = m_weightCompsArray[Index];
			FAmbientBedGroup& bedGroup = pair.Key;
			const TSharedPtr<TAmbientWeightOctree> weightOctree = pair.Value;

			UDA_AmbientBed* ambientBed = bedGroup.AmbientBed;
			const float range = ambientBed->Range;
			const float rangeSquared = range * range;
			const float posLerp = ambientBed->ReferencePositionLerp;
			const FVector referencePosition = FMath::Lerp(distanceProbePosition, listenerPosition, posLerp);

			const FBox searchBox(referencePosition - FVector(range), referencePosition + FVector(range));
			TSet<UAkRoomComponent*> foundRooms;

			weightOctree->FindElementsWithBoundsTest(FBoxCenterAndExtent(searchBox),
				[&](const FAmbientWeightOctreeElement& WeightElement)
				{
					const float distanceToListenerSquared = FVector::DistSquared(referencePosition, WeightElement.BoundingBox.Center);
					if (distanceToListenerSquared > rangeSquared) { return; }

					// initialize ambient emitter if not present
					{
						FScopeLock Lock(&m_critSectAmbientEmitters);
						if (!m_playingAmbientBedEmitters.Contains(bedGroup))
						{
							{
								m_playingAmbientBedEmitters.Emplace(bedGroup, TMap<UAkRoomComponent*, UAmbientBedEmitterComponent*>());
							}
						}
					}

					// find room components at location and create ambient emitter if necessary
					TArray<UAkRoomComponent*> roomComps = AkAudioDevice->FindRoomComponentsAtLocation(WeightElement.BoundingBox.Center, world);
					if (roomComps.IsEmpty()) { return; }
					UAkRoomComponent* roomComp = roomComps[0];
					foundRooms.Emplace(roomComp);
					
					if (!m_playingAmbientBedEmitters[bedGroup].Contains(roomComp))
					{
						FScopeLock Lock(&critSectEmittersToCreate);
						emittersToCreate.FindOrAdd(
							TPair<UAkRoomComponent*, FAmbientBedGroup&>(roomComp, bedGroup));
						bedGroupsToKeep.FindOrAdd(&bedGroup);
					}
					else
					{
						UpdateEmitterPosition(bedGroup, roomComp, WeightElement, listenerPosition, range);
					}

#if !UE_BUILD_SHIPPING
					if (Private_AmbientBeds::bDebugDrawWeights)
					{
						FScopeLock Lock(&m_critSectDbgWeightComps);
						m_dbgWeightComps.Add(WeightElement.AmbientSoundWeightComponent, &bedGroup);
					}
#endif
				});

			CleanupUnusedEmitters(bedGroup, foundRooms, bedGroupsToKeep);
		});

	for (auto& emitter : emittersToCreate)
	{
		CreateAmbientEmitter(emitter.Value, emitter.Key, emitter.Value.AmbientBed);
	}

	CleanupRoomListeners();
	
	const FQuat listenerRotation = AkAudioDevice->GetSpatialAudioListener()->GetComponentQuat();
	for (auto& roomListener : m_roomListeners)
	{
		roomListener.Value->SetWorldRotation(listenerRotation);
	}

#if !UE_BUILD_SHIPPING
	DebugDrawOnTick(world);
#endif
}

#if !UE_BUILD_SHIPPING
void AAmbientBedWorldManager::DebugDrawOnTick(UWorld* world)
{
	// debug distances in viewport
	if (Private_AmbientBeds::bViewPortStats && !s_dbgViewportValues.IsEmpty())
	{
		int key = 1;
		
		const FString msgLabels{ TEXT("\n\n\n\nDistance Rtpc - Weight Rtpc - Summed Weight\n") };
		GEngine->AddOnScreenDebugMessage(key, 1.f, FColor::White, msgLabels);

		for (const TPair<UAmbientBedEmitterComponent*, FDebugValues>& emitter : s_dbgViewportValues)
		{
			const FString msgValues = FString::Printf(TEXT("%s: %.2f - %.2f - %.2f"), *emitter.Key->GetName(),
				emitter.Value.DistanceRtpc, emitter.Value.WeightRtpc, emitter.Value.MaxWeight);

			GEngine->AddOnScreenDebugMessage(++key, 1.f, emitter.Key->m_dbgColor, msgValues);
		}		
	}

	if (!IsValid(world)) { return; }

	// 3d debug relative to screen space
	if (Private_AmbientBeds::bDebugDrawSpatialization)
	{
		const APlayerController* playerController = world->GetFirstPlayerController();
		if (UNLIKELY(!playerController)) { return; }

		const FVector cameraPosition = playerController->PlayerCameraManager->GetCameraLocation();
		const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
		const float posX = ViewportSize.X * .85f;
		const float posY = ViewportSize.Y * .20f;
		FVector worldPosition;
		FVector worldDirection;

		playerController->DeprojectScreenPositionToWorld(posX, posY, worldPosition, worldDirection);

		const FVector hudBasePosition = cameraPosition + (300.f * worldDirection);
		DrawDebugCrosshairs(world, hudBasePosition, worldDirection.Rotation(), 32.f, FColor::Orange);
		DrawDebugSphere(GetWorld(), hudBasePosition, 10.f, 4, FColor::Silver);

		for (auto roomListener : m_roomListeners)
		{
			UAudioUtils::DrawDebugGizmo(world, roomListener.Value->GetComponentLocation(),
				roomListener.Value->GetComponentRotation(), 500.f);

			DrawDebugSphere(world, roomListener.Value->GetComponentLocation(), 25.f, 16, FColor::Green);
		}

		for (auto ambientBedEmitter : m_playingAmbientBedEmitters)
		{
			const FColor outerColor = ambientBedEmitter.Key.GroupColor;
			const FColor innerColor = FColor(outerColor.R / 2.f, outerColor.G / 2.f, outerColor.B / 2.f);

			for (auto ambientEmitter : ambientBedEmitter.Value)
			{
				const UAkComponent* emitterAkComp = ambientEmitter.Value->m_ambientEmitter;
				const FVector emitterLocation = emitterAkComp->GetComponentLocation();

				DrawDebugSphere(world, emitterLocation, emitterAkComp->innerRadius, 10, FColor::Orange);
				DrawDebugSphere(world, emitterLocation, emitterAkComp->outerRadius, 10, FColor::Purple);

				const FVector hudBedPosition = hudBasePosition + (25.f * emitterAkComp->GetRelativeLocation());
				const float innerScale = emitterAkComp->innerRadius / emitterAkComp->outerRadius;

				DrawDebugSphere(world, hudBedPosition, 10.f, 10, outerColor);
				DrawDebugSphere(world, hudBedPosition, 10.f * innerScale, 12, innerColor);
			}
		}
	}

	// draw weight comps
	if (Private_AmbientBeds::bDebugDrawWeights)
	{
		for (const auto& dbgWeightComp : m_dbgWeightComps)
		{
			const float weight = dbgWeightComp.Key->Weight;
			if (weight < 0.005) { continue; }

			const FColor msgColor = dbgWeightComp.Value->GroupColor;
			const FVector msgPos = dbgWeightComp.Key->GetComponentLocation();
			const FString msg = FString::Printf(TEXT("%.2f"), weight);

			DrawDebugString(world, msgPos, msg, 0, msgColor, 0.f, false, 1.1f);

			constexpr float drawRange = 5000.f;
			constexpr float sqDrawRange = drawRange * drawRange;
		}
	}
}
#endif

void AAmbientBedWorldManager::CreateAmbientEmitter(const FAmbientBedGroup& BedGroup,
	UAkRoomComponent* RoomComp, UDA_AmbientBed* AmbientBed)
{
	FString baseName = AmbientBed->GetName();
	baseName.Append(FString::Printf(TEXT("_ID%i"), BedGroup.GroupID));

	// create and initialize emitter and listener components
	UAkComponent* roomListener = GetOrCreateRoomListener(RoomComp, baseName);

	UAmbientBedEmitterComponent* ambientComp;
	const FString compName = FString::Printf(TEXT("[AmbientComp].%s"), *baseName);
	ambientComp = NewObject<UAmbientBedEmitterComponent>(RoomComp, *compName);
	ambientComp->RegisterComponentWithWorld(GetWorld());
	ambientComp->AttachToComponent(RoomComp, FAttachmentTransformRules::KeepRelativeTransform);

	const FString emitterName = FString::Printf(TEXT("[AmbientEmitter].%s"), *baseName);
	ambientComp->Initialize(AmbientBed, emitterName);

	ambientComp->m_listenerId = roomListener->GetAkGameObjectID();
	ambientComp->m_roomId = RoomComp->GetAkGameObjectID();

#if !UE_BUILD_SHIPPING
	ambientComp->m_dbgColor = BedGroup.GroupColor;
#endif

	FScopeLock Lock(&m_critSectAmbientEmitters);
	m_playingAmbientBedEmitters[BedGroup].Emplace(RoomComp, ambientComp);
}

UAkComponent* AAmbientBedWorldManager::GetOrCreateRoomListener(UAkRoomComponent* RoomComp, const FString& BaseName)
{
	UAkComponent* roomListener;

	if (m_roomListeners.Contains(RoomComp))
	{
		roomListener = m_roomListeners[RoomComp];
	}
	else
	{
		const FString listenerName = FString::Printf(TEXT("[AmbientListener].%s"), *BaseName);
		roomListener = NewObject<UAkComponent>(RoomComp, *listenerName);
		roomListener->RegisterComponentWithWorld(GetWorld());
		roomListener->AttachToComponent(RoomComp, FAttachmentTransformRules::KeepRelativeTransform);
		roomListener->OcclusionRefreshInterval = 0.f;

		FScopeLock Lock(&m_critSectRoomListeners);
		m_roomListeners.Emplace(RoomComp, roomListener);
	}

	return roomListener;
}

void AAmbientBedWorldManager::UpdateEmitterPosition(const FAmbientBedGroup& BedGroup,
	UAkRoomComponent* RoomComp, const FAmbientWeightOctreeElement& WeightElement,
	const FVector& ListenerPosition, float Range)
{
	const float weight = WeightElement.AmbientSoundWeightComponent->Weight;
	const FVector weightedRelPos = weight * (WeightElement.BoundingBox.Center - ListenerPosition) / Range;

	UAmbientBedEmitterComponent*& ambientEmitter = m_playingAmbientBedEmitters[BedGroup][RoomComp];
	ambientEmitter->m_localPosition += weightedRelPos;
	ambientEmitter->m_avgWeightedDistance += weightedRelPos.Length();
	ambientEmitter->m_summedWeight += weight;
	ambientEmitter->m_numWeights++;
}

void AAmbientBedWorldManager::CleanupUnusedEmitters(const FAmbientBedGroup& BedGroup,
	const TSet<UAkRoomComponent*>& FoundRooms, const TSet<FAmbientBedGroup*>& BedGroupsToKeep)
{
	if (!m_playingAmbientBedEmitters.Contains(BedGroup)) { return; }
	
	if (FoundRooms.IsEmpty())
	{
		for (auto& emitter : m_playingAmbientBedEmitters[BedGroup])
		{
			emitter.Value->DestroyComponent();
#if !UE_BUILD_SHIPPING
			s_dbgViewportValues.Remove(emitter.Value);
#endif
		}

		FScopeLock Lock(&m_critSectAmbientEmitters);
		m_playingAmbientBedEmitters.Remove(BedGroup);

		return;
	}

	TSet<UAkRoomComponent*> toRemove;
	for (auto& emitter : m_playingAmbientBedEmitters[BedGroup])
	{
		if (!FoundRooms.Contains(emitter.Key))
		{
			emitter.Value->Stop();
			toRemove.Add(emitter.Key);

#if !UE_BUILD_SHIPPING
			s_dbgViewportValues.Remove(emitter.Value);
#endif
		}
		else
		{
			emitter.Value->AccumulatePositionAndDistance(BedGroup.AmbientBed);
		}
	}

	for (UAkRoomComponent* room : toRemove)
	{
		FScopeLock Lock(&m_critSectAmbientEmitters);
		m_playingAmbientBedEmitters[BedGroup].Remove(room);
	}

	if (m_playingAmbientBedEmitters[BedGroup].IsEmpty() && !BedGroupsToKeep.Contains(&BedGroup))
	{
		FScopeLock Lock(&m_critSectAmbientEmitters);
		m_playingAmbientBedEmitters.Remove(BedGroup);
	}
}

void AAmbientBedWorldManager::CleanupRoomListeners()
{
	TSet<UAkRoomComponent*> roomListenersKeys;
	m_roomListeners.GetKeys(roomListenersKeys);

	TSet<UAkRoomComponent*> activeRoomListeners;
	for (const auto& roomEmitter : m_playingAmbientBedEmitters)
	{
		for (const auto& activeRoom : roomEmitter.Value)
		{
			activeRoomListeners.Add(activeRoom.Key);
		}
	}

	for (UAkRoomComponent* room : roomListenersKeys)
	{
		if (!activeRoomListeners.Contains(room))
		{
			m_roomListeners[room]->DestroyComponent();
			m_roomListeners.Remove(room);
		}
	}
}

void AAmbientBedWorldManager::AddWeight(
	UAmbientBedWeightComponent* AmbientSoundWeightComponent, UDA_AmbientBed* AmbientBed)
{
	float activationRange = AmbientBed->Range;

#if !UE_BUILD_SHIPPING
	if (activationRange <= 0.f)
	{
		if (!m_postedBedsWithoutValidRange.Contains(AmbientBed))
		{
			WR_DBG_FUNC(Warning, "no range set on %s (%s). Loop was not posted",
				*AmbientBed->GetName(), *AmbientBed->LoopEvent->GetName());
			m_postedBedsWithoutValidRange.Add(AmbientBed);
		}

		return;
	}
#endif
	const FAmbientBedGroup bedGroup{ AmbientBed,
		AmbientSoundWeightComponent->bOverrideAmbientBedGroup ? AmbientSoundWeightComponent->GroupId : -1 };

	if (!m_weightComps.Contains(bedGroup))
	{
		TSharedPtr<TAmbientWeightOctree> emitterOctree = MakeShared<TAmbientWeightOctree>();
		m_weightComps.Add(TPair<FAmbientBedGroup, TSharedPtr<TAmbientWeightOctree>>(bedGroup, emitterOctree));
	}

	m_weightComps[bedGroup]->AddWeight(AmbientSoundWeightComponent);
}

void AAmbientBedWorldManager::RemoveWeight(
	UAmbientBedWeightComponent* AmbientSoundWeightComponent, UDA_AmbientBed* AmbientBed)
{
	const float activationRange = AmbientBed->Range;
	const FAmbientBedGroup bedGroup{ AmbientBed,
		AmbientSoundWeightComponent->bOverrideAmbientBedGroup ? AmbientSoundWeightComponent->GroupId : -1 };

	// remove from m_weights and m_ambientEmitters
	if (m_weightComps[bedGroup]->ObjectToOctreeId.Contains(AmbientSoundWeightComponent->GetUniqueID()))
	{
		m_weightComps[bedGroup]->RemoveWeight(AmbientSoundWeightComponent);
	}

	if (m_weightComps[bedGroup]->NumElements <= 0)
	{
		m_weightComps.Remove(bedGroup);
		m_playingAmbientBedEmitters.Remove(bedGroup);
	}
}
#pragma endregion

#pragma region UAmbientBedManager
void UAmbientBedManager::Initialize(USoundListenerManager* SoundListenerManager)
{
	const UWwiserRGameSettings* audioConfig = GetDefault<UWwiserRGameSettings>();
	
	audioConfig->DefaultAmbientBedPassthroughBuss.TryLoad();
	UAmbientBedEmitterComponent::s_passthroughAuxBus 
		= Cast<UAkAuxBus>(audioConfig->DefaultAmbientBedPassthroughBuss.ResolveObject());

	/*if (!IsValid(UAmbientBedEmitterComponent::s_passthroughAuxBus))
	{
		WR_DBG_NET(Error, "initialization failed (%s): no passthrough aux bus set in project settings",
			*UAudioUtils::GetClientOrServerString(GetWorld()));

		return;
	}*/
	
	m_listenerManager = SoundListenerManager;
	m_listenerManager->OnSpatialAudioListenerChanged.AddUObject(this, &UAmbientBedManager::OnSpatialAudioListenerChanged);
	FWorldDelegates::OnPostWorldCleanup.AddUObject(this, &UAmbientBedManager::OnPostWorldCleanup);

	m_isInitialized = true;
	WR_DBG_NET(Log, "initialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}

void UAmbientBedManager::Deinitialize()
{
	if (!m_isInitialized) { return; }
	
	m_listenerManager->OnSpatialAudioListenerChanged.RemoveAll(this);
	FWorldDelegates::OnPostWorldCleanup.RemoveAll(this);

	for (TPair<UWorld*, AAmbientBedWorldManager*> worldManager : m_worldManagers)
	{
		if (IsValid(worldManager.Value))
		{
			worldManager.Value->Deinitialize();
		}
	}

	m_worldManagers.Empty();
	m_listenerManager = nullptr;

	WR_DBG_NET(Log, "deinitialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}

void UAmbientBedManager::AddWeight(
	UWorld* World, UAmbientBedWeightComponent* AmbientSoundWeightComponent, UDA_AmbientBed* AmbientBed)
{
	if (!(IsValid(AmbientSoundWeightComponent) && IsValid(AmbientBed))) { return; }

	if (!IsValid(AmbientBed->LoopEvent))
	{
#if !UE_BUILD_SHIPPING
		if (!m_postedBedsWithoutValidAkEvent.Contains(AmbientBed))
		{
			WR_DBG_FUNC(Warning, "no valid AkAudioEvent assigned in %s. Ambient sound bed cannot be posted."
				, *AmbientBed->GetName());
			m_postedBedsWithoutValidAkEvent.Add(AmbientBed);
		}
#endif

		return;
	}

	if (!m_worldManagers.Contains(World))
	{
		m_worldManagers.Add(TPair<UWorld*, AAmbientBedWorldManager*>
			(World, World->SpawnActor<AAmbientBedWorldManager>(FActorSpawnParameters())));

		m_worldManagers[World]->Initialize(m_listenerManager);
	}

	m_worldManagers[World]->AddWeight(AmbientSoundWeightComponent, AmbientBed);
}

void UAmbientBedManager::RemoveWeight(
	UWorld* World, UAmbientBedWeightComponent* AmbientSoundWeightComponent, UDA_AmbientBed* AmbientBed)
{
	if (!IsValid(AmbientBed) /* || !IsValid(AmbientSoundWeightComponent)*/) { return; }

	if (m_worldManagers.Contains(World))
	{
		m_worldManagers[World]->RemoveWeight(AmbientSoundWeightComponent, AmbientBed);
	}
}

void UAmbientBedManager::OnSpatialAudioListenerChanged(UWorld* NewWorld, UAkComponent* SpatialAudioListener)
{
	for (TPair<UWorld*, AAmbientBedWorldManager*> worldManager : m_worldManagers)
	{
		worldManager.Value->SetActorTickEnabled(worldManager.Key == NewWorld);
	}
}

void UAmbientBedManager::OnPostWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	m_worldManagers.Remove(World);
}
#pragma endregion
