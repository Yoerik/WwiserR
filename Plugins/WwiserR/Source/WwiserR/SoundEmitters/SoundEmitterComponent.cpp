// Copyright Yoerik Roevens. All Rights Reserved.(c)


#include "SoundEmitterComponent.h"
#include "Managers/SoundListenerManager.h"
#include "WorldSoundListenerComponent.h"
#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "AkAudioEvent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if !UE_BUILD_SHIPPING
#include "Config/AudioConfig.h"
#endif

#pragma region CVars
namespace Private_SoundEmitterComponent
{
	static TAutoConsoleVariable<bool> CVar_SoundEmitter_DebugCulling(TEXT("WwiserR.SoundEmitter.DebugToConsole.Culling"), false,
		TEXT("Debug distancec culling on sound emitters to console/log"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_SoundEmitter_DebugPoll(TEXT("WwiserR.SoundEmitter.DebugToConsole.DistancePolling"), false,
		TEXT("Debug distance polling for culling loops on 3D audio emitters to console/log"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_SoundEmitter_DebugTick(TEXT("WwiserR.SoundEmitter.DebugToConsole.Tick"), false,
		TEXT("Debug start/stop ticking on sound emitters to console/log"), ECVF_Cheat);
	static TAutoConsoleVariable<float> CVar_SoundEmitter_DebugRanges(TEXT("WwiserR.SoundEmitter.DebugDraw.ActivationRanges"), 0.f,
		TEXT("DebugDraw - show culling activation ranges. (0 = off, 1 = on, or enter range in meters)"), ECVF_Cheat);

	bool bDebugCull = false;
	bool bDebugPoll = false;
	bool bDebugTick = false;
	float fDebugDrawActivationRange = 1.f;

	static void OnSoundEmitterComponentUpdate()
	{
		bDebugCull = CVar_SoundEmitter_DebugCulling.GetValueOnGameThread();
		bDebugPoll = CVar_SoundEmitter_DebugPoll.GetValueOnGameThread();
		bDebugTick = CVar_SoundEmitter_DebugTick.GetValueOnGameThread();
		fDebugDrawActivationRange = CVar_SoundEmitter_DebugRanges.GetValueOnGameThread();
	}

	FAutoConsoleVariableSink CSoundEmitterComponentConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnSoundEmitterComponentUpdate));
} // namespace Private_SoundEmitterComponent
#pragma endregion

USoundEmitterComponent::USoundEmitterComponent()
{
	if (const AActor* owningActor = GetOwner())
	{
		if (owningActor->IsA<ACharacter>())
		{
			m_characterMovementData.MovementComp = owningActor->FindComponentByClass<UCharacterMovementComponent>();

			if (IsValid(m_characterMovementData.MovementComp))
			{
				bUseParentLocationForCulling = true;
				bAutoEmitterMaxSpeed = true;
			}
		}
	}
}

void USoundEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == ENetMode::NM_DedicatedServer) { return; }

	if (bAutoEmitterMaxSpeed)
	{
		SetEmitterMaxSpeedAuto(true, bResetAutoEmitterMaxSpeedOnStopMoving);
	}
	else
	{
		SetEmitterMaxSpeedManual(ManualEmitterMaxSpeed);
	}
}

void USoundEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		if (bStopWhenOwnerDestroyed)
		{
			StopAllLoops();
		}
		else
		{
			if (bStopLoopsWhenEmitterDestroyed)
			{
				StopAllLoops(LoopFadeOutTimeOnEmitterDestroyedInMs, LoopFadeOutCurveOnEmitterDestroyed);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool USoundEmitterComponent::UnregisterEmitterIfInactive()
{
	if (Super::UnregisterEmitterIfInactive())
	{
		if (bAutoDestroy && m_culledPlayingLoops.IsEmpty())
		{
			DestroyComponent();
		}

		return true;
	}

	return false;
}

bool USoundEmitterComponent::DestroyAkComponent()
{
	if (m_culledPlayingLoops.IsEmpty())
	{
		SetComponentTickEnabled(false);
		if (Private_SoundEmitterComponent::bDebugTick)
		{
			WR_DBG_FUNC(Log, "stopped ticking (AkComponent destroyed)");
		}
	}

	return Super::DestroyAkComponent();
}

void USoundEmitterComponent::OnDebugDrawChanged()
{
	Super::OnDebugDrawChanged();

	if (s_debugDraw)
	{
		if (!m_culledPlayingLoops.IsEmpty() || IsValid(m_AkComp))
		{
			SetComponentTickEnabled(true);

			if (Private_SoundEmitterComponent::bDebugTick)
			{
				WR_DBG_FUNC(Log, "started ticking for visual debugging");
			}
		}
	}
	else
	{
		if (!bAutoEmitterMaxSpeed)
		{
			TryToStopTicking();

			if (Private_SoundEmitterComponent::bDebugTick)
			{
				WR_DBG_FUNC(Log, "stopped ticking for visual debugging");
			}
		}
	}
}

void USoundEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// update emitter max speed if moving
	if (bAutoEmitterMaxSpeed && bCanMove && bUseDistanceCulling)
	{
		const FVector newCullingLocation = UseParentLocationForCulling() ? GetOwner()->GetActorLocation() : GetComponentLocation();

		if (newCullingLocation == m_lastCullingLocation)
		{
			UpdateAutoEmitterMaxSpeed(0.f);

			TryToStopTicking();

			if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugTick)
			{
				WR_DBG_FUNC(Log, "stopped ticking for auto max speed calculation");
			}
		}
	}
}

#pragma region Class Properties
void USoundEmitterComponent::SetUseDistanceCulling(bool bShouldUseDistanceCulling)
{
	if (bUseDistanceCulling == bShouldUseDistanceCulling) {	return; }

	bUseDistanceCulling = bShouldUseDistanceCulling;

	UpdateOnMovedDelegates();
	RecullAllLoops();
}

void USoundEmitterComponent::SetUseParentLocationForCulling(bool bUseParentLocationForDistanceCulling)
{
	if (bUseParentLocationForCulling == bUseParentLocationForDistanceCulling) { return; }

	bUseParentLocationForCulling = bUseParentLocationForDistanceCulling;

	UpdateOnMovedDelegates();
	RecullAllLoops();
}

void USoundEmitterComponent::SetEmitterMaxSpeedAuto(bool bResetMaxSpeed, bool bResetMaxSpeedOnStopMoving)
{
	bAutoEmitterMaxSpeed = true;
	bResetAutoEmitterMaxSpeedOnStopMoving = bResetMaxSpeedOnStopMoving;

	m_lastCullingLocation = GetCullingLocation();

	if (bResetMaxSpeed)
	{
		m_emitterMaxSpeed = 0.f;
		UpdateDistanceCullingRelativeMaxSpeed();
	}

	UpdateOnMovedDelegates();
}

void USoundEmitterComponent::SetEmitterMaxSpeedManual(float NewEmitterMaxSpeed)
{
	if (NewEmitterMaxSpeed < 0.f) { return; }

	bAutoEmitterMaxSpeed = false;

	ManualEmitterMaxSpeed = NewEmitterMaxSpeed;

	if (m_emitterMaxSpeed != ManualEmitterMaxSpeed)
	{
		m_emitterMaxSpeed = ManualEmitterMaxSpeed;
		UpdateDistanceCullingRelativeMaxSpeed();
	}

	UpdateOnMovedDelegates();
}

void USoundEmitterComponent::SetEmitterCanMove(bool bEmitterCanMove)
{
	if (bEmitterCanMove == bCanMove)
	{
		return;
	}

	bCanMove = bEmitterCanMove;

	if (!bCanMove)
	{
		UpdateDistanceCullingRelativeMaxSpeed();
	}
	else
	{
		if (!bAutoEmitterMaxSpeed)
		{
			m_emitterMaxSpeed = ManualEmitterMaxSpeed;
			UpdateDistanceCullingRelativeMaxSpeed();
		}
	}

	if (s_debugToConsole && (!bCanMove || !bAutoEmitterMaxSpeed))
	{
		WR_DBG_FUNC(Log, "new sound emitter max speed: %.3f", m_emitterMaxSpeed);
	}

	UpdateOnMovedDelegates();
}

void USoundEmitterComponent::SetAttenuationScalingFactor(float Value)
{
	const float oldAttenuationScalingFactor = AttenuationScalingFactor;
	Super::SetAttenuationScalingFactor(Value);

	if (oldAttenuationScalingFactor != Value)
	{
		RecullAllLoops();
	}
}
#pragma endregion

#pragma region Internal - Distance Culling
void USoundEmitterComponent::UpdateOnMovedDelegates()
{
	ToggleTickForAutoEmitterMaxSpeedCalculation();

	if (!m_culledPlayingLoops.IsEmpty() && IsValid(s_listenerManager))
	{
		if (!s_listenerManager->OnMaxSpeedIncreased.IsBoundToObject(this))
		{
			s_listenerManager->OnMaxSpeedIncreased.AddUObject(this, &USoundEmitterComponent::UpdateDistanceCullingRelativeMaxSpeed);
		}

		if (!s_listenerManager->OnAttenuationReferenceChanged.IsBoundToObject(this))
		{
			s_listenerManager->OnAttenuationReferenceChanged.AddUObject(this, &USoundEmitterComponent::RecullAllLoops);
		}
	}
	else if(m_culledPlayingLoops.IsEmpty() && IsValid(s_listenerManager))
	{
		s_listenerManager->OnMaxSpeedIncreased.RemoveAll(this);
		s_listenerManager->OnAttenuationReferenceChanged.RemoveAll(this);
	}

	if (!bUseDistanceCulling || !bAutoEmitterMaxSpeed || !bCanMove || m_culledPlayingLoops.IsEmpty())
	{
		return RemoveOnMovedDelegates();
	}

	auto onTransformUpdated =
		[this](USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlag, ETeleportType Teleport)->void
		{
			if (Teleport != ETeleportType::None)
			{
				OnTeleported();
			}
			else
			{
				if (bAutoEmitterMaxSpeed && bCanMove && bUseDistanceCulling)
				{
					if (UseParentLocationForCulling() && IsValid(m_characterMovementData.MovementComp))
					{
						m_characterMovementData.HasReculled = false;

						const float movementCompMaxSpeed = m_characterMovementData.MovementComp->GetMaxSpeed();

						if (m_emitterMaxSpeed != movementCompMaxSpeed)
						{
							m_emitterMaxSpeed = movementCompMaxSpeed;

							if (m_emitterMaxSpeed > m_characterMovementData.LastCulledMaxSpeed)
							{
								UpdateDistanceCullingRelativeMaxSpeed();

								if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugCull)
								{
									WR_DBG_FUNC(Log, "new sound emitter max speed: %.3f", m_emitterMaxSpeed);
								}
							}
						}
					}

					else
					{
						const FVector newCullingLocation = UseParentLocationForCulling() ? GetOwner()->GetActorLocation() : GetComponentLocation();
						UpdateAutoEmitterMaxSpeed(FVector::Distance(m_lastCullingLocation, newCullingLocation) / GetWorld()->GetDeltaSeconds());
					}
				}
			}
		};

	if (UseParentLocationForCulling())
	{
		TransformUpdated.Clear();

		if (AActor* owner = GetOwner())
		{
			if (!owner->GetRootComponent()->TransformUpdated.IsBoundToObject(this))
			{
				owner->GetRootComponent()->TransformUpdated.AddLambda(onTransformUpdated);
			}
		}
	}
	else
	{
		if (AActor* owner = GetOwner())
		{
			owner->GetRootComponent()->TransformUpdated.Clear();
		}

		if (!TransformUpdated.IsBoundToObject(this))
		{
			TransformUpdated.AddLambda(onTransformUpdated);
		}
	}
}

void USoundEmitterComponent::RemoveOnMovedDelegates()
{
	TransformUpdated.RemoveAll(this);

	if (AActor* owner = GetOwner())
	{
		owner->GetRootComponent()->TransformUpdated.RemoveAll(this);
	}

	return;
}

void USoundEmitterComponent::ToggleTickForAutoEmitterMaxSpeedCalculation()
{
	if (bCanMove && bUseDistanceCulling && bResetAutoEmitterMaxSpeedOnStopMoving
		&& (bAutoEmitterMaxSpeed && !(UseParentLocationForCulling() && IsValid(m_characterMovementData.MovementComp)))
		&& !m_culledPlayingLoops.IsEmpty())
	{
		SetComponentTickEnabled(true);
		m_lastCullingLocation = UseParentLocationForCulling() ? GetOwner()->GetActorLocation() : GetComponentLocation();

		if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugTick)
		{
			WR_DBG_FUNC(Log, "started ticking for auto max speed calculation");
		}
	}
	else
	{
		TryToStopTicking();

		if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugTick)
		{
			WR_DBG_FUNC(Log, "stopped ticking for auto max speed calculation");
		}
	}
}

void USoundEmitterComponent::UpdateAutoEmitterMaxSpeed(float NewMaxSpeed)
{
	if (NewMaxSpeed > m_emitterMaxSpeed)
	{
		m_emitterMaxSpeed = NewMaxSpeed;
	}
	else if(NewMaxSpeed == 0.f && bResetAutoEmitterMaxSpeedOnStopMoving && m_emitterMaxSpeed > 0.f)
	{
		m_emitterMaxSpeed = 0.f;
	}
	else { return; }

	UpdateDistanceCullingRelativeMaxSpeed();

	if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugCull)
	{
		WR_DBG_FUNC(Log, "new sound emitter max speed: %.3f", m_emitterMaxSpeed);
	}
}

void USoundEmitterComponent::UpdateDistanceCullingRelativeMaxSpeed()
{
	if (!m_culledPlayingLoops.IsEmpty() && bUseDistanceCulling && !m_isMuted)
	{
		m_bMustRecalculateAllLoopCullTimes = true;

		// if (m_nextCullIndex > 0)
		{
			CalculateAndSetNextLoopCullTime(m_culledPlayingLoops[m_nextCullIndex]);
		}

		ScheduleNextDistanceCulling();
	}
}

bool USoundEmitterComponent::ScheduleNextDistanceCulling()
{
	const UWorld* world = GetWorld();

	if (!IsValid(world))
	{
		return false;
	}

	if (!FMath::IsFinite(m_nextCullTime))
	{
		world->GetTimerManager().ClearTimer(m_timerNextDistanceCull);
		return false;
	}

	const float nextCullTimeFromNow = m_nextCullTime - world->GetTimeSeconds();

	if (nextCullTimeFromNow > 0.f)
	{
		world->GetTimerManager().SetTimer(m_timerNextDistanceCull, this, &USoundEmitterComponent::DistanceCull, nextCullTimeFromNow, false);
		return true;
	}
	else
	{
		world->GetTimerManager().ClearTimer(m_timerNextDistanceCull);
		m_timerNextDistanceCull = world->GetTimerManager().SetTimerForNextTick(this, &USoundEmitterComponent::DistanceCull);
		return true;
	}

	return false;
}

void USoundEmitterComponent::DistanceCull()
{
	//if (m_nextCullIndex > 0)
	{
		CullByDistance(m_culledPlayingLoops[m_nextCullIndex]);
	}

	UpdateNextCullTimeAndLoopIndex();
}

void USoundEmitterComponent::CullByDistance(FPlayingAudioLoop& Loop)
{
#if !UE_BUILD_SHIPPING
	if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugPoll && IsValid(Loop.AkEvent))
	{
		const FVector cullingLocation = GetCullingLocation();
		float listDist = INFINITY;
		UAkComponent* listener = nullptr;

		if (IsValid(s_listenerManager) && IsValid(s_listenerManager->GetSpatialAudioListener()))
		{
			listDist = FMath::Sqrt(s_listenerManager->GetSquaredDistanceToDistanceProbe(cullingLocation));
			listener = s_listenerManager->GetSpatialAudioListener();
		}

		for (TWeakObjectPtr<UWorldSoundListener> worldListener : GetWorldListeners())// m_worldListeners)
		{
			const float worldListDist = FVector::Dist(cullingLocation, worldListener->GetComponentLocation());
			if (worldListDist < listDist)
			{
				listDist = worldListDist;
				listener = worldListener.Get();
			}
		}

		WR_DBG_FUNC(Log, "%s, closest listener: %s, distance: %.2f m",
			*Loop.AkEvent->GetName(), listener == s_listenerManager->GetSpatialAudioListener() ?
			TEXT("SpatialAudioListener") : *listener->GetOwner()->GetActorNameOrLabel(), listDist / 100.f);
	}
#endif

	if (Loop.bIsVirtual && IsInListenerRange(Loop.AkEvent, Loop.AttenuationRangeBuffer))
	{
		DevirtualizeLoop(Loop);
	}
	else if (!Loop.bIsVirtual && !IsInListenerRange(Loop.AkEvent, Loop.AttenuationRangeBuffer))
	{
		VirtualizeLoop(Loop);
	}

	CalculateAndSetNextLoopCullTime(Loop);
}

void USoundEmitterComponent::VirtualizeLoop(FPlayingAudioLoop& Loop)
{
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->StopPlayingID(Loop.LastPlayingID, 100, AkCurveInterpolation::AkCurveInterpolation_Linear);
	}

	Loop.LastPlayingID = AK_INVALID_PLAYING_ID;
	Loop.bIsVirtual = true;

	if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugCull && IsValid(Loop.AkEvent))
	{
		const float listDist = FMath::Sqrt(s_listenerManager->GetSquaredDistanceToDistanceProbe(GetCullingLocation()));
		const float scaledAttenuationRadius = Loop.AkEvent->MaxAttenuationRadius * AttenuationScalingFactor;
		const float effectiveBuffer = listDist - scaledAttenuationRadius;
		const float relativeBuffer = effectiveBuffer - Loop.AttenuationRangeBuffer;

		WR_DBG_FUNC(Log, "%s virtualized at listener distance: %f cm *** effective buffer: %f cm *** relative buffer: %f cm",
			*Loop.AkEvent->GetName(), listDist, effectiveBuffer, relativeBuffer);
	}
}

int32 USoundEmitterComponent::DevirtualizeLoop(FPlayingAudioLoop& Loop)
{
	if (!IsValid(Loop.AkEvent))
	{
		WR_DBG_FUNC(Error, "could not devirtualize loop because loop event is not valid");
		return AK_INVALID_PLAYING_ID;
	}

	CreateAkComponentIfNeeded();

	if (Loop.bQueryAndPostEnvironmentSwitches)
	{
		QueryAndPostEnvironmentSwitches();
	}

	Loop.LastPlayingID = m_AkComp->PostAkEvent(Loop.AkEvent, 0, FOnAkPostEventCallback());
	Loop.bIsVirtual = false;

	for (TPair<UAkRtpc*, float> rtpcOnPlayingID : Loop.RtpcsOnPlayingID)
	{
		if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
		{
			AkAudioDevice->SetRTPCValueByPlayingID(rtpcOnPlayingID.Key->GetWwiseShortID(), rtpcOnPlayingID.Value, Loop.LastPlayingID, 0);
		}
	}

	if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugCull)
	{
		const float listDist = FMath::Sqrt(s_listenerManager->GetSquaredDistanceToDistanceProbe(GetCullingLocation()));
		const float scaledAttenuationRadius = Loop.AkEvent->MaxAttenuationRadius * AttenuationScalingFactor;
		const float effectiveBuffer = listDist - scaledAttenuationRadius;
		const float relativeBuffer = effectiveBuffer - Loop.AttenuationRangeBuffer;

		WR_DBG_FUNC(Log, "%s devirtualized at listener distance: %f cm *** effective buffer: %f cm *** relative buffer: %f cm",
			*Loop.AkEvent->GetName(), listDist, effectiveBuffer, relativeBuffer);
	}

	return Loop.LastPlayingID;
}

float USoundEmitterComponent::CalculateAndSetNextLoopCullTime(FPlayingAudioLoop& Loop)
{
	WR_ASSERT(Loop.AkEvent, "invalid AkEvent found in culledPlayingLoops array");

	Loop.NextCullTime = INFINITY;

	if (m_isMuted || !bUseDistanceCulling || Loop.AkEvent->MaxAttenuationRadius == 0 || AttenuationScalingFactor <= 0.f)
	{
		return INFINITY;
	}

	const FVector emitterLocation = GetCullingLocation();
	const float currentTime = GetWorld()->GetTimeSeconds();
	const float emitterMaxSpeed = bCanMove ? m_emitterMaxSpeed : 0.f;

	if (bCanMove && (UseParentLocationForCulling() && IsValid(m_characterMovementData.MovementComp)))
	{
		m_characterMovementData.LastCulledMaxSpeed = m_emitterMaxSpeed;
		m_characterMovementData.HasReculled = true;
	}

	const float cullRange = Loop.AkEvent->MaxAttenuationRadius * AttenuationScalingFactor + innerRadius + Loop.AttenuationRangeBuffer;
	float maxRelativeSpeed{ 0.f };

	// world listeners
	for (TWeakObjectPtr<UWorldSoundListener> worldListener : GetWorldListeners())
	{
		maxRelativeSpeed = emitterMaxSpeed + worldListener->GetMaxSpeed();

		if (maxRelativeSpeed > 0.f)
		{
			const float minDistanceToTravel = FMath::Abs(
				FVector::Distance(emitterLocation, worldListener->GetComponentLocation()) - cullRange);
			const float nextLoopCullTime = currentTime + minDistanceToTravel / maxRelativeSpeed;

			if (nextLoopCullTime < Loop.NextCullTime)
			{
				Loop.NextCullTime = nextLoopCullTime;
			}
		}
	}

	// default listeners
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		maxRelativeSpeed = emitterMaxSpeed + s_listenerManager->GetDefaultListenerMaxSpeed();

		if (maxRelativeSpeed > 0.f)
		{
			for (TWeakObjectPtr<UAkComponent> defaultListener : AkAudioDevice->GetDefaultListeners())
			{
				if (defaultListener.Get() == AkAudioDevice->GetSpatialAudioListener())
				{
					continue;
				}

				const float minDistanceToTravel = FMath::Abs(
					FVector::Distance(emitterLocation, defaultListener->GetComponentLocation()) - cullRange);
				const float nextLoopCullTime = currentTime + minDistanceToTravel / maxRelativeSpeed;

				if (nextLoopCullTime < Loop.NextCullTime)
				{
					Loop.NextCullTime = nextLoopCullTime;
				}
			}
		}
	}

	// spatial audio listener
	maxRelativeSpeed = emitterMaxSpeed + s_listenerManager->GetDistanceProbeMaxSpeed();
	if (maxRelativeSpeed > 0.f)
	{
		const float minDistanceToTravel = FMath::Abs(
			FMath::Sqrt(s_listenerManager->GetSquaredDistanceToDistanceProbe(emitterLocation)) - cullRange);
		const float nextLoopCullTime = currentTime + minDistanceToTravel / maxRelativeSpeed;

		if (nextLoopCullTime < Loop.NextCullTime)
		{
			Loop.NextCullTime = nextLoopCullTime;
		}
	}

	return Loop.NextCullTime;
}

void USoundEmitterComponent::UpdateNextCullTimeAndLoopIndex()
{
	m_nextCullIndex = 0;
	m_nextCullTime = INFINITY;

	if (m_bMustRecalculateAllLoopCullTimes)
	{
		for (int i = 0; i < m_culledPlayingLoops.Num(); i++)
		{
			CalculateAndSetNextLoopCullTime(m_culledPlayingLoops[i]);
		}

		m_bMustRecalculateAllLoopCullTimes = false;
	}

	for (int i = 0; i < m_culledPlayingLoops.Num(); i++)
	{
		if (m_culledPlayingLoops[i].NextCullTime < m_nextCullTime)
		{
			m_nextCullTime = m_culledPlayingLoops[i].NextCullTime;
			m_nextCullIndex = i;
		}
	}

#if !UE_BUILD_SHIPPING
	if (/*s_debugToConsole && */Private_SoundEmitterComponent::bDebugPoll &&
		m_culledPlayingLoops.IsValidIndex(m_nextCullIndex) && IsValid(m_culledPlayingLoops[m_nextCullIndex].AkEvent))
	{
		if (IsValid(s_listenerManager) && IsValid(s_listenerManager->GetSpatialAudioListener()))
		{
			WR_DBG_FUNC(Log, "%s: next distance culling in %f seconds",
				*m_culledPlayingLoops[m_nextCullIndex].AkEvent->GetName(),
				m_culledPlayingLoops[m_nextCullIndex].NextCullTime - GetWorld()->GetTimeSeconds());
		}
	}
#endif

	ScheduleNextDistanceCulling();
}

void USoundEmitterComponent::RecullAllLoops()
{
	if (m_culledPlayingLoops.IsEmpty())
	{
		return;
	}

	if (m_isMuted)
	{
		for (FPlayingAudioLoop& Loop : m_culledPlayingLoops)
		{
			if (Loop.bIsVirtual == false)
			{
				VirtualizeLoop(Loop);
			}

			Loop.NextCullTime = INFINITY;
		}

		m_nextCullTime = INFINITY;
		ScheduleNextDistanceCulling();
		return;
	}

	if (s_debugToConsole)
	{
		WR_DBG_FUNC(Log, "resetting distance culling");
	}

	if (bUseDistanceCulling)
	{
		for (FPlayingAudioLoop& Loop : m_culledPlayingLoops)
		{
			CullByDistance(Loop);
		}

		UpdateNextCullTimeAndLoopIndex();
	}
	else // !m_isMuted && !bUseDistanceCulling
	{
		for (FPlayingAudioLoop& Loop : m_culledPlayingLoops)
		{
			if (Loop.bIsVirtual == true)
			{
				DevirtualizeLoop(Loop);
			}

			Loop.NextCullTime = INFINITY;
		}

		m_nextCullTime = INFINITY;
	}

	ScheduleNextDistanceCulling();
}

void USoundEmitterComponent::OnListenersUpdated()
{
	Super::OnListenersUpdated();

	if (!m_culledPlayingLoops.IsEmpty())
	{
		RecullAllLoops();
	}
}
#pragma endregion

#pragma region Debug
#if !UE_BUILD_SHIPPING
void USoundEmitterComponent::DebugDrawOnTick(UWorld* World)
{
	if (!IsValid(World)) { return; }

	// culled loop event names
	for (const FPlayingAudioLoop& Loop : m_culledPlayingLoops)
	{
		WR_ASSERT(IsValid(Loop.AkEvent), "invalid AkEvent found in m_culledPlayingLoops[]");

		if (!Loop.bIsVirtual)
		{
			m_msgEmitterDebug.Append(Loop.AkEvent->GetName().Append("\n"));
		}
	}

	// draw activation ranges for loops
	const float dbgDistSquared = s_listenerManager->GetSquaredDistanceToDistanceProbe(GetComponentLocation());

	if (ShouldDrawDebug(Private_SoundEmitterComponent::fDebugDrawActivationRange, dbgDistSquared))
	{
		for (const FPlayingAudioLoop& Loop : m_culledPlayingLoops)
		{
			WR_ASSERT(IsValid(Loop.AkEvent), "invalid AkEvent found in m_culledPlayingLoops[]");

			DrawDebugActivationRanges(
				World, Loop.AkEvent->MaxAttenuationRadius * AttenuationScalingFactor, Loop.AttenuationRangeBuffer, Loop.bIsVirtual);
		}
	}

	Super::DebugDrawOnTick(World);
}

void USoundEmitterComponent::DrawDebugActivationRanges(UWorld* world, float MaxAttenuationRadius, float RangeBuffer, bool bIsVirtual, float LifeTime)
{
	static const FThemeSoundEmitters& debugTheme = IsValid(GetDefault<UWwiserRThemeSettings>()) ?
		GetDefault<UWwiserRThemeSettings>()->ThemeSoundEmitters : FThemeSoundEmitters{};
	static FColor activeActivationRangeColor = debugTheme.ActiveActivationRangeColor;
	static FColor inactiveActivationRangeColor = debugTheme.InactiveActivationRangeColor;

	// for clarity, don't draw ranges on local player
	if (APawn* MyPawn = Cast<APawn>(GetOwner()))
	{
		if (MyPawn->IsLocallyControlled())
		{
			return;
		}
	}

	const float rangeSegments = 20;
	const FColor rangeColor = bIsVirtual ? activeActivationRangeColor : inactiveActivationRangeColor;

	DrawDebugSphere(world, GetComponentLocation(), MaxAttenuationRadius * AttenuationScalingFactor + innerRadius + RangeBuffer,
		rangeSegments, rangeColor, false, LifeTime);
}
#endif
#pragma endregion

#pragma region Public Functions - Sound Emitter
void USoundEmitterComponent::MuteEmitter(bool bMute)
{
	Super::MuteEmitter(bMute);
	RecullAllLoops();
}

void USoundEmitterComponent::StopAll()
{
	StopAllLoops();
	Super::StopAll();
}

bool USoundEmitterComponent::HasActiveEvents() const
{
	return (!m_culledPlayingLoops.IsEmpty() || Super::HasActiveEvents());
}

void USoundEmitterComponent::OnTeleported()
{
	RecullAllLoops();
}

void USoundEmitterComponent::OnListenerTeleported()
{
	RecullAllLoops();
}

int32 USoundEmitterComponent::GetLoopLastPlayingID(int32 InitialPlayingID)
{
	if (InitialPlayingID == 0)
	{
		WR_DBG_FUNC(Warning, "no valid PlayingID");
		return AK_INVALID_PLAYING_ID;
	}

	for (const FPlayingAudioLoop& loop : m_culledPlayingLoops)
	{
		if (loop.InitialPlayingID == InitialPlayingID)
		{
			return loop.LastPlayingID;
		}
	}

	WR_DBG_FUNC(Warning, "no playing loop with PlayingID %i found", InitialPlayingID);
	return AK_INVALID_PLAYING_ID;
}

void USoundEmitterComponent::SetRtpcByPlayingID(UAkRtpc* AkRtpc, int32 PlayingID, float Value, int32 InterpolationTimeMs)
{
	if (!IsValid(AkRtpc))
	{
		WR_DBG_FUNC(Warning, "missing Wwise Rtpc");
		return;
	}

	int32 playingID = PlayingID;

	for (FPlayingAudioLoop& loop : m_culledPlayingLoops)
	{
		if ((loop.LastPlayingID == PlayingID) || (loop.InitialPlayingID == PlayingID))
		{
			if (loop.RtpcsOnPlayingID.Contains(AkRtpc))
			{
				loop.RtpcsOnPlayingID[AkRtpc] = Value;
			}
			else
			{
				loop.RtpcsOnPlayingID.Add(AkRtpc, Value);
			}

			playingID = loop.LastPlayingID;
			break;
		}
	}

	if (IsValid(m_AkComp))
	{
		if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
		{
			AkAudioDevice->SetRTPCValueByPlayingID(AkRtpc->GetWwiseShortID(), Value, playingID, InterpolationTimeMs);
		}
	}
}

int32 USoundEmitterComponent::PostLoop(UAkAudioEvent* LoopAkEvent, float ActivationRangeBuffer, bool bQueryAndPostEnvironmentSwitches)
{
	if (!IsValid(LoopAkEvent))
	{
		WR_DBG_FUNC(Warning, "No Wwise event assigned");
		return AK_INVALID_PLAYING_ID;
	}

	FPlayingAudioLoop Loop(LoopAkEvent, bQueryAndPostEnvironmentSwitches, AK_INVALID_PLAYING_ID, true, ActivationRangeBuffer, INFINITY);

	const bool bShouldPost = IsInListenerRange(LoopAkEvent, ActivationRangeBuffer) && !m_isMuted;
	if (bShouldPost)
	{
		CreateAkComponentIfNeeded();

		if (bQueryAndPostEnvironmentSwitches)
		{
			QueryAndPostEnvironmentSwitches();
		}

		Loop.InitialPlayingID = m_AkComp->PostAkEvent(LoopAkEvent, 0, FOnAkPostEventCallback());
		Loop.bIsVirtual = false;
	}
	else
	{
		Loop.InitialPlayingID = --s_newVirtualLoopPlayingId;
	}

	Loop.LastPlayingID = Loop.InitialPlayingID;

	CalculateAndSetNextLoopCullTime(Loop);

	m_culledPlayingLoops.Add(Loop);

	if (Loop.NextCullTime < m_nextCullTime)
	{
		m_nextCullTime = Loop.NextCullTime;
		m_nextCullIndex = m_culledPlayingLoops.Num() - 1;

		ScheduleNextDistanceCulling();
	}

	if (/*s_debugToConsole && */s_logEvents)
	{
		WR_DBG_FUNC(Log, "Loop posted : %s", *LoopAkEvent->GetName());
	}

	UpdateOnMovedDelegates();

	/*** tick for debug draw ***/
#if !UE_BUILD_SHIPPING
	if (s_debugDraw)
	{
		SetComponentTickEnabled(true);
		if (Private_SoundEmitterComponent::bDebugTick)
		{
			WR_DBG_FUNC(Log, "Started ticking for visual debugging");
		}
	}
#endif

	return Loop.InitialPlayingID;
}

bool USoundEmitterComponent::StopLoop(UAkAudioEvent* LoopAkEvent, int32 PlayingID, int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve)
{
	if (!IsValid(LoopAkEvent) && PlayingID == 0)
	{
		WR_DBG_FUNC(Warning, "missing looping Wwise event or PlayingID");

		return false;
	}

	bool bLoopStopped = false;

	if (IsValid(LoopAkEvent))
	{
		for (int i = 0; i < m_culledPlayingLoops.Num(); i++)
		{
			const FPlayingAudioLoop& Loop = m_culledPlayingLoops[i];

			if (Loop.AkEvent == LoopAkEvent && (PlayingID == 0 || PlayingID == Loop.InitialPlayingID || PlayingID == Loop.LastPlayingID))
			{
				if (!Loop.bIsVirtual)
				{
					if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
					{
						AkAudioDevice->StopPlayingID(Loop.LastPlayingID, TransitionDurationInMs, (AkCurveInterpolation)FadeCurve);
					}

					if (/*s_debugToConsole && */s_logEvents)
					{
						WR_DBG_FUNC(Log, "loop [%s] stopped. Fade out time: %i ms", *LoopAkEvent->GetName(), TransitionDurationInMs);
					}
				}

				m_culledPlayingLoops.RemoveAtSwap(i);
				bLoopStopped = true;

				break;
			}
		}

		if (!bLoopStopped)
		{
			if (PlayingID == 0)
			{
				WR_DBG_FUNC(Warning, "tried to stop Wwise event [%s] that wasn't playing", *LoopAkEvent->GetName());
			}
			else
			{
				WR_DBG_FUNC(Warning, "tried to stop Wwise event [%s] with PlayingID [%i] that wasn't playing", *LoopAkEvent->GetName(), PlayingID);
			}
		}
	}
	else
	{
		for (int i = m_culledPlayingLoops.Num() - 1; i >= 0; i--)
		{
			const FPlayingAudioLoop& Loop = m_culledPlayingLoops[i];

			if (PlayingID == Loop.InitialPlayingID || PlayingID == Loop.LastPlayingID)
			{
				if (!Loop.bIsVirtual)
				{
					if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
					{
						AkAudioDevice->StopPlayingID(Loop.LastPlayingID, TransitionDurationInMs, (AkCurveInterpolation)FadeCurve);
					}

					if (/*s_debugToConsole && */s_logEvents)
					{
						WR_DBG_FUNC(Log, "loop [%s] stopped. Fade out time: %i ms", *Loop.AkEvent->GetName(), TransitionDurationInMs);
					}
				}

				m_culledPlayingLoops.RemoveAtSwap(i);
				bLoopStopped = true;
				break;
			}
		}

		if (!bLoopStopped)
		{
			WR_DBG_FUNC(Warning, "tried to stop PlayingID [%i] that wasn't playing", PlayingID);
		}
	}

	if (bLoopStopped)
	{
		UpdateNextCullTimeAndLoopIndex();

		if (m_culledPlayingLoops.IsEmpty())
		{
			UpdateOnMovedDelegates();
		}
	}

	return bLoopStopped;
}

int32 USoundEmitterComponent::StopLoopUsingStopEvent(
	UAkAudioEvent* LoopAkEvent, UAkAudioEvent* StopAkEvent, int32 PlayingID, const int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback)
{
	if (!IsValid(LoopAkEvent) && PlayingID == 0)
	{
		WR_DBG_FUNC(Warning, "missing looping Wwise event or PlayingID");
		return AK_INVALID_PLAYING_ID;
	}

	if (!IsValid(StopAkEvent))
	{
		WR_DBG_FUNC(Warning, "missing Wwise stop event");
		return AK_INVALID_PLAYING_ID;
	}

	int32 pID = AK_INVALID_PLAYING_ID;
	bool bLoopStopped = false;

	if (IsValid(LoopAkEvent))
	{
		for (int i = m_culledPlayingLoops.Num() - 1; i >= 0; i--)
		{
			const FPlayingAudioLoop& Loop = m_culledPlayingLoops[i];

			if (Loop.AkEvent == LoopAkEvent && (PlayingID == 0 || PlayingID == Loop.InitialPlayingID || PlayingID == Loop.LastPlayingID))
			{
				if (!Loop.bIsVirtual && IsValid(m_AkComp))
				{
					pID = m_AkComp->PostAkEvent(StopAkEvent, CallbackMask, PostEventCallback);
				}

				if (/*s_debugToConsole && */s_logEvents)
				{
					WR_DBG_FUNC(Log, "%s stopped using StopEvent %s", *LoopAkEvent->GetName(), *StopAkEvent->GetName());
				}

				m_culledPlayingLoops.RemoveAtSwap(i);
				bLoopStopped = true;
				break;
			}
		}

		if (!bLoopStopped)
		{
			if (PlayingID == 0)
			{
				WR_DBG_FUNC(Warning, "tried to stop Wwise event [%s] that wasn't playing", *LoopAkEvent->GetName());
			}
			else
			{
				WR_DBG_FUNC(Warning, "tried to stop Wwise event [%s] with PlayingID [%i] that wasn't playing", *LoopAkEvent->GetName(), PlayingID);
			}
		}
	}
	else
	{
		for (int i = m_culledPlayingLoops.Num() - 1; i >= 0; i--)
		{
			const FPlayingAudioLoop& Loop = m_culledPlayingLoops[i];

			if (PlayingID == Loop.InitialPlayingID || PlayingID == Loop.LastPlayingID)
			{
				if (!Loop.bIsVirtual && IsValid(m_AkComp))
				{
					pID = m_AkComp->PostAkEvent(StopAkEvent, CallbackMask, PostEventCallback);
				}

				if (/*s_debugToConsole && */s_logEvents)
				{
					WR_DBG_FUNC(Log, "%s stopped using StopEvent %s", *Loop.AkEvent->GetName(), *StopAkEvent->GetName());
				}

				m_culledPlayingLoops.RemoveAtSwap(i);
				bLoopStopped = true;
				break;
			}
		}

		if (!bLoopStopped)
		{
			WR_DBG_FUNC(Warning, "tried to stop PlayingID [%i] that wasn't playing", PlayingID);
		}
	}

	if (bLoopStopped)
	{
		UpdateNextCullTimeAndLoopIndex();

		if (m_culledPlayingLoops.IsEmpty())
		{
			UpdateOnMovedDelegates();
		}
	}

	return pID;
}

bool USoundEmitterComponent::StopAllMatchingLoops(
	UAkAudioEvent* LoopAkEvent, int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve)
{
	if (!IsValid(LoopAkEvent))
	{
		WR_DBG_FUNC(Warning, "tried to stop missing Wwise event");
		return false;
	}

	int loopsStopped = 0;

	for (int i = m_culledPlayingLoops.Num() - 1; i >= 0; i--)
	{
		const FPlayingAudioLoop& Loop = m_culledPlayingLoops[i];

		if (Loop.AkEvent == LoopAkEvent)
		{
			if (!Loop.bIsVirtual)
			{
				if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
				{
					AkAudioDevice->StopPlayingID(Loop.LastPlayingID, TransitionDurationInMs, (AkCurveInterpolation)FadeCurve);
				}
			}

			m_culledPlayingLoops.RemoveAtSwap(i, 1);
			++loopsStopped;
		}
	}

	if (loopsStopped > 0)
	{
		UpdateNextCullTimeAndLoopIndex();

		if (s_debugToConsole)
		{
			WR_DBG_FUNC(Log, "%i instances of loop [%s] stopped, fade out time: %i ms", *LoopAkEvent->GetName(), TransitionDurationInMs);
		}

		if (m_culledPlayingLoops.IsEmpty())
		{
			UpdateOnMovedDelegates();
		}

		return true;
	}

	WR_DBG_FUNC(Warning, "tried to stop Wwise event [%s] that wasn't playing", *LoopAkEvent->GetName());
	return false;
}

bool USoundEmitterComponent::StopAllLoops(int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve)
{
	bool bLoopsStopped = false;

	if (!m_culledPlayingLoops.IsEmpty())
	{
		for (const FPlayingAudioLoop& Loop : m_culledPlayingLoops)
		{
			if (!Loop.bIsVirtual && IsValid(Loop.AkEvent))
			{
				if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
				{
					AkAudioDevice->StopPlayingID(Loop.LastPlayingID, TransitionDurationInMs, (AkCurveInterpolation)FadeCurve);
				}

				if (/*s_debugToConsole && */s_logEvents)
				{
					WR_DBG_FUNC(Log, "loop [%s] stopped, fade out time: %i ms", *Loop.AkEvent->GetName(), TransitionDurationInMs);
				}
			}
		}

		m_culledPlayingLoops.Empty();
		UpdateNextCullTimeAndLoopIndex();
		UpdateOnMovedDelegates();

		bLoopsStopped = true;
	}

	return bLoopsStopped;
}
#pragma endregion

#pragma region Public Functions - Static
USoundEmitterComponent* USoundEmitterComponent::GetAttachedSoundEmitterComponent(
	USceneComponent* AttachToComponent, bool& ComponentCreated, FName Socket, EAttachLocation::Type LocationType)
{
	ComponentCreated = false;

	if (!IsValid(AttachToComponent))
	{
		WR_DBG_STATIC_FUNC(Warning, "no AttachToComponent specified!");
		return nullptr;
	}

	const FAttachmentTransformRules attachRules = FAttachmentTransformRules::KeepRelativeTransform;
	AActor* attachActor = AttachToComponent->GetOwner();

	if (attachActor != nullptr)
	{
		if (!IsValid(attachActor))
		{
			// Avoid creating component if we're trying to play a sound on an already destroyed actor.
			WR_DBG_STATIC_FUNC(Error, "attach actor is no longer valid!");
			return nullptr;
		}

		TArray<USoundEmitterComponent*> soundEmitterComps;
		attachActor->GetComponents<USoundEmitterComponent>(soundEmitterComps);

		for (USoundEmitterComponent* soundEmitterComp : soundEmitterComps)
		{
			if (IsValid(soundEmitterComp) && soundEmitterComp->IsRegistered())
			{
				if (AttachToComponent == soundEmitterComp)
				{
					WR_DBG_STATIC_FUNC(Log, "AttachToComponent is itself");
					return soundEmitterComp;
				}

				if (AttachToComponent != soundEmitterComp->GetAttachParent() || Socket != soundEmitterComp->GetAttachSocketName())
				{
					continue;
				}

				// SoundEmitterComponent found which exactly matches the attachment: reuse it.
				if (s_debugToConsole)
				{
					WR_DBG_STATIC_FUNC(Log, "existing SoundEmitterComponent found");
				}
				return soundEmitterComp;
			}
		}
	}
	else // attachActor == nullptr
	{
		// Try to find if there is a SoundEmitterComponent attached to AttachToComponent (will be the case if AttachToComponent has no owner)
		const TArray<USceneComponent*> AttachChildren = AttachToComponent->GetAttachChildren();

		for (int32 i = 0; i < AttachChildren.Num(); i++)
		{
			USoundEmitterComponent* soundEmitterComp = Cast<USoundEmitterComponent>(AttachChildren[i]);

			if (IsValid(soundEmitterComp) && soundEmitterComp->IsRegistered())
			{
				// There is an associated AkComponent to AttachToComponent, no need to add another one.
				return soundEmitterComp;
			}
		}
	}

	// no matching SoundEmitterComponent found

	USoundEmitterComponent* soundEmitterComp = nullptr;

	if (IsValid(attachActor))
	{
		static const FString txtSoundEmitter = TEXT(".SoundEmitter");
		const FName emitterName = FName(Socket.ToString() + txtSoundEmitter);

		soundEmitterComp = NewObject<USoundEmitterComponent>(attachActor, emitterName);
	}
	else
	{
		soundEmitterComp = NewObject<USoundEmitterComponent>();
	}

	check(soundEmitterComp);

	soundEmitterComp->RegisterComponentWithWorld(AttachToComponent->GetWorld());
	soundEmitterComp->AttachToComponent(AttachToComponent, attachRules, Socket);

	if (s_debugToConsole)
	{
		WR_DBG_STATIC_FUNC(Warning, "%s: no SoundEmitterComponent found at socket [%s], new SoundEmitterComponent created",
			*AttachToComponent->GetName(), *Socket.ToString());
	}

	ComponentCreated = true;
	return soundEmitterComp;
}

UPARAM(DisplayName = "Sound Emitter Component")USoundEmitterComponent* USoundEmitterComponent::SpawnSoundEmitterAtLocation(
	UObject* Context, FVector Location, FRotator Orientation, bool AutoDestroy)
{
	USoundEmitterComponent* soundEmitterComponent = nullptr;
	UWorld* world = Context->GetWorld();

	if (IsValid(world))
	{
		soundEmitterComponent = NewObject<USoundEmitterComponent>(world->GetWorldSettings());
	}
	else
	{
		soundEmitterComponent = NewObject<USoundEmitterComponent>();
	}

	if (IsValid(soundEmitterComponent))
	{
		soundEmitterComponent->SetWorldLocationAndRotation(Location, Orientation.Quaternion());
		if (IsValid(world))
		{
			soundEmitterComponent->RegisterComponentWithWorld(world);
		}

		soundEmitterComponent->bAutoDestroy = AutoDestroy;
	}

	return soundEmitterComponent;
}
#pragma endregion
