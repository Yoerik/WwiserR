// Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "Managers/SoundListenerManager.h"
#include "Managers/GlobalSoundEmitterManager.h"
#include "SoundEmitters/WorldSoundListenerComponent.h"
#include "Core/AudioUtils.h"
#include "AkAudioDevice.h"
//#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "AkComponent.h"
#include "AkRoomComponent.h"
#include "AkAcousticPortal.h"
#include "AkGeometryComponent.h"
#include "Components/BrushComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Config/AudioConfig.h"
#include "Config/DebugTheme.h"
#endif

#if WITH_EDITOR
#include "WwiserR_Editor/EditorAudioUtils.h"
#include "Core/AudioSubsystem.h"
#endif

#pragma region CVars
namespace Private_ListenerManager
{
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugConsole(
		TEXT("WwiserR.ListenerManager.DebugToConsole"), false, TEXT("Listener Manager: console logging. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugDraw(
		TEXT("WwiserR.ListenerManager.DebugDraw"), false, TEXT("Listener Manager: visual listener debugging. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugDrawListener(TEXT("WwiserR.ListenerManager.DebugDraw.Listener"), true,
		TEXT("Listener Manager: draw Listener. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugDrawTarget(TEXT("WwiserR.ListenerManager.DebugDraw.Target"), true,
		TEXT("Listener Manager: draw Listener Target. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugDrawDistanceProbe(TEXT("WwiserR.ListenerManager.DebugDraw.DistanceProbe"), true,
		TEXT("Listener Manager: draw Distance Probe. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugDrawGizmoListener(TEXT("WwiserR.ListenerManager.DebugDraw.Gizmos.Listener"),false,
		TEXT("Listener Manager: draw Listener gizmo. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugDrawGizmoTarget(TEXT("WwiserR.ListenerManager.DebugDraw.Gizmos.Target"), true,
		TEXT("Listener Manager: draw Listener Target gizmo. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_ShowProbeSpeed(TEXT("WwiserR.ListenerManager.DebugDraw.DistanceProbeSpeed"), false,
		TEXT("Listener Manager: show distance probe speed. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_ListenerManager_DebugDrawWorldListeners(TEXT("WwiserR.ListenerManager.DebugDraw.WorldListeners"), true,
		TEXT("Listener Manager: draw WorldListeners. (0 = off, 1 = on)"), ECVF_Cheat);

	bool bDebugConsole = false;
	bool bDebugDraw = false;

	bool bDebugDrawListener = true;
	bool bDebugDrawTarget = true;
	bool bDebugDrawProbe = true;
	bool bDebugDrawGizmoListener = false;
	bool bDebugDrawGizmoTarget = true;
	bool bDebugShowProbeSpeed = false;
	bool bDebugDrawWorldListeners = true;

	static void OnDebugListenerManagerUpdate()
	{
		bDebugConsole = CVar_ListenerManager_DebugConsole.GetValueOnGameThread();
		bDebugDraw = CVar_ListenerManager_DebugDraw.GetValueOnGameThread();

		bDebugDrawListener = CVar_ListenerManager_DebugDrawListener.GetValueOnGameThread();
		bDebugDrawTarget = CVar_ListenerManager_DebugDrawTarget.GetValueOnGameThread();
		bDebugDrawProbe = CVar_ListenerManager_DebugDrawDistanceProbe.GetValueOnGameThread();
		bDebugDrawGizmoListener = CVar_ListenerManager_DebugDrawGizmoListener.GetValueOnGameThread();
		bDebugDrawGizmoTarget = CVar_ListenerManager_DebugDrawGizmoTarget.GetValueOnGameThread();
		bDebugShowProbeSpeed = CVar_ListenerManager_ShowProbeSpeed.GetValueOnGameThread();
		bDebugDrawWorldListeners = CVar_ListenerManager_DebugDrawWorldListeners.GetValueOnGameThread();
	}

	FAutoConsoleVariableSink CListenerManagerDebugConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnDebugListenerManagerUpdate));
} // namespace Private_ListenerManager
#pragma endregion

#pragma region Data Asset Auditioning
#if WITH_EDITOR
void UDAListenerManagerSettings::Audition()
{
	for (USoundListenerManager* listenerManager : USoundListenerManager::s_allConnectedListenerManagers)
	{
		listenerManager->ImportListenerManagerComponentProperties(this);
	}
}

void UDAListenerManagerSettings::PostEditChangeProperty(FPropertyChangedEvent& e)
{
	if (bAutoAudition)
	{
		Audition();
	}
}
#endif
#pragma endregion

#pragma region USpatialProbeComponent
USpatialProbeComponent::USpatialProbeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicated(false);

	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bTickInEditor = false;
}
#pragma endregion

#pragma region USoundListenerManagerComponent
USoundListenerManagerComponent::USoundListenerManagerComponent()
{
	SetIsReplicated(false);

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	//PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;
	//PrimaryComponentTick.TickGroup = ETickingGroup::TG_PostUpdateWork;
}

void USoundListenerManagerComponent::Initialize(USoundListenerManager* SoundListenerManager)
{
	m_ListenerManager = SoundListenerManager;
}

void USoundListenerManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	m_currentWorld = GetWorld();
	if (m_currentWorld->GetNetMode() == ENetMode::NM_DedicatedServer) { return; }

	if (!IsValid(m_ListenerManager))
	{
		WR_DBG_FUNC(Log, "%s not initialized", *GetName());
		return;
	}

	m_akAudioDevice = FAkAudioDevice::Get();
	m_spatialAudioListener = m_akAudioDevice->GetSpatialAudioListener();
	m_ListenerManager->OnSpatialAudioListenerChanged_Internal(m_spatialAudioListener);
	m_playerController = m_currentWorld->GetFirstPlayerController();
	m_playerCameraManager = m_playerController->PlayerCameraManager;

	if (IsValid(m_playerCameraManager))
	{
		static const FName nameProbeComp{ TEXT("Distance Probe") };
		m_distanceProbe = NewObject<USpatialProbeComponent>(this, nameProbeComp);
		m_distanceProbe->RegisterComponentWithWorld(GetWorld());
		m_distanceProbe->AttachToComponent(m_playerCameraManager->GetDefaultAttachComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}

	ImportListenerManagerComponentProperties(m_ListenerManager->m_listenerManagerComponentProperties);
	UpdateTarget();
	SetComponentTickEnabled(true);
}

void USoundListenerManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void USoundListenerManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// lazy update spatial audio listener and player controller
	if (!IsValid(m_spatialAudioListener))
	{
		m_spatialAudioListener = m_akAudioDevice->GetSpatialAudioListener();
		m_ListenerManager->OnSpatialAudioListenerChanged_Internal(m_spatialAudioListener);
		if (!IsValid(m_spatialAudioListener)) { return; }

		auto onTransformUpdated =
			[this](USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlag, ETeleportType Teleport)->void
			{
				if (!(Teleport == ETeleportType::None)) { m_ListenerManager->OnTeleported(UpdatedComponent); }
			};

		m_spatialAudioListener->TransformUpdated.AddLambda(onTransformUpdated);

		m_ListenerManager->m_currentListenerIds.Add(m_spatialAudioListener->GetAkGameObjectID());

		if (m_ListenerManager->OnListenersUpdated.IsBound())
		{
			m_ListenerManager->OnListenersUpdated.Broadcast();
		}
	}

	/*if (m_playerController != m_currentWorld->GetFirstPlayerController())
	{
		m_playerController = m_currentWorld->GetFirstPlayerController();
	}*/

	// lazy update target
	if (m_playerCameraManager != m_playerController->PlayerCameraManager || !IsValid(m_targetComponent)
		|| (m_listManCompProperties.ListenerTarget == EListenerTarget::Player && m_playerPawn != m_playerController->GetPawn())
		|| (m_listManCompProperties.ListenerTarget == EListenerTarget::CameraViewTarget &&
			(!IsValid(m_playerCameraManager) || m_currentViewTarget != m_playerCameraManager->GetViewTarget()))
		)
	{
		UpdateTarget();
		if (!IsValid(m_playerCameraManager)) { WR_DBG_NET_FUNC(Error, "no valid PlayerCameraManager"); return; }
	}

	if (IsValid(m_characterMovementComp))
	{
		if (m_characterMovementComp->MovementMode == EMovementMode::MOVE_None
			&& m_listManCompProperties.MinimumCharacterReferenceMovementModeMaxSpeed == EMovementMode::MOVE_None
			&& m_characterMovementSpeed > 0.f)
		{
			m_characterMovementSpeed = 0.f;
		}
	}

	if (IsValid(m_targetComponent))
	{
		m_distanceProbe->SetWorldLocationAndRotation(
			FMath::Lerp(m_playerCameraManager->GetCameraLocation(), m_targetComponent->GetComponentLocation(),
				m_listManCompProperties.DistanceProbePositionLerp), m_targetComponent->GetComponentRotation());
	}
	else
	{
		m_distanceProbe->SetWorldLocationAndRotation(m_playerCameraManager->GetCameraLocation(), m_playerCameraManager->GetCameraRotation());
	}

	const FVector listenerPos = GetListenerPosition();
	const FRotator listenerRot = GetListenerRotation();

	m_playerController->SetAudioListenerOverride(nullptr, listenerPos, listenerRot);
	m_spatialAudioListener->SetWorldLocationAndRotation(listenerPos, listenerRot);

	if (IsValid(m_listenerSpeedRtpc))
	{
		const float normalizedListenerSpeed = FVector::Dist(listenerPos, m_lastListenerPosition) / GetWorld()->DeltaTimeSeconds;
		m_akAudioDevice->SetRTPCValue(m_listenerSpeedRtpc, normalizedListenerSpeed, 0, nullptr);
	}

	m_lastListenerPosition = listenerPos;
	//m_lastListenerRotation = listenerRot;

	if (Private_ListenerManager::bDebugDraw)
	{
		DebugDraw();
	}
}

void USoundListenerManagerComponent::ImportListenerManagerComponentProperties(const FListenerManagerComponentProperties& ListenerManagerComponentProperties)
{
	m_listManCompProperties = ListenerManagerComponentProperties;

	SetListenerTarget(m_listManCompProperties.ListenerTarget);						// updates m_targetComponent and ateenuation reference
	SetDistanceProbeMaxSpeed(m_listManCompProperties.SpatialAudioListenerMaxSpeed);	// broadcasts delegate if necessary

	if (IsValid(m_targetComponent))
	{
		m_targetComponent->DestroyComponent();
		m_targetComponent = nullptr;
	}

	UpdateTarget();
}

void USoundListenerManagerComponent::SetListenerRotation(const EListenerRotation NewListenerRotation)
{
	m_listManCompProperties.ListenerRotation = NewListenerRotation;
}

void USoundListenerManagerComponent::SetListenerTarget(const EListenerTarget NewListenerTarget)
{
	m_listManCompProperties.ListenerTarget = NewListenerTarget;

	UpdateTarget();
}

void USoundListenerManagerComponent::SetTargetTransform(FVector Location, FQuat Rotation)
{
	m_customTargetTransform.SetComponents(Rotation, Location, FVector{ 1.f, 1.f, 1.f });
	m_listManCompProperties.ListenerTarget = EListenerTarget::CustomPosition;
	UpdateTarget();
}

void USoundListenerManagerComponent::SetListenerPositionLerp(const float NewPositionLerp, const bool bTriggerEmitterRecull)
{
	m_listManCompProperties.ListenerPositionLerp = NewPositionLerp;

	if (bTriggerEmitterRecull && m_ListenerManager->OnAttenuationReferenceChanged.IsBound())
	{
		m_ListenerManager->OnAttenuationReferenceChanged.Broadcast();
	}
}

void USoundListenerManagerComponent::SetDistanceProbeMaxSpeed(float MaxSpeed)
{
	const bool bMaxSpeedIncreased = MaxSpeed > m_spatialAudioListenerMaxSpeed;
	m_spatialAudioListenerMaxSpeed = MaxSpeed;

	if (bMaxSpeedIncreased && m_ListenerManager->OnMaxSpeedIncreased.IsBound())
	{
		m_ListenerManager->OnMaxSpeedIncreased.Broadcast();
	}
}

float USoundListenerManagerComponent::GetMovementModeMaxSpeed(const UCharacterMovementComponent* Comp, const EMovementMode MovementMode) const
{
	if (MovementMode == EMovementMode::MOVE_Custom) { return Comp->MaxCustomMovementSpeed; }
	if (MovementMode == EMovementMode::MOVE_Falling) { return Comp->MaxFlySpeed; }
	if (MovementMode == EMovementMode::MOVE_Flying) { return Comp->MaxFlySpeed; }
	if (MovementMode == EMovementMode::MOVE_NavWalking) { return Comp->MaxWalkSpeed; }
	if (MovementMode == EMovementMode::MOVE_None) { return 0.f; }
	if (MovementMode == EMovementMode::MOVE_Swimming) { return Comp->MaxSwimSpeed; }
	if (MovementMode == EMovementMode::MOVE_Walking) { return Comp->MaxWalkSpeed; }

	return 0.0f;
}

void USoundListenerManagerComponent::UpdateGlidingPositionLerp(FGlideData& GlideData, float& PositionLerp)
{
	if (GlideData.Duration <= 0.f)
	{
		GlideData.bIsGlidingForward = false;
		GlideData.bIsGlidingReverse = false;

		if (Private_ListenerManager::bDebugConsole)
		{
			WR_DBG_FUNC(Log, "glide canceled because duration <= 0.f");
		}

		PositionLerp = GlideData.EndPositionLerp;
		return;
	}

	GlideData.Progress = (GetWorld()->GetTimeSeconds() - GlideData.LastTimeStarted) / GlideData.Duration;

	if (GlideData.bIsGlidingReverse)
	{
		GlideData.Progress = 1.f - GlideData.Progress;
	}

	GlideData.Progress = FMath::Clamp(GlideData.Progress, 0.f, 1.f);

	PositionLerp = UKismetMathLibrary::Ease(GlideData.StartPositionLerp, GlideData.EndPositionLerp,
		GlideData.Progress, GlideData.InterpolationEasingFunction, GlideData.InterpolationEasingFunctionBlendExponent);

	if (Private_ListenerManager::bDebugConsole)
	{
		WR_DBG_FUNC(Log, "progress = %f - listener position lerp = %f", GlideData.Progress, GlideData.EndPositionLerp);
	}

	const bool mustEndGlide =
		(GlideData.bIsGlidingForward && GlideData.Progress >= 1.f) ||
		(GlideData.bIsGlidingReverse && GlideData.Progress <= 0.f);

	if (mustEndGlide)
	{
		GlideData.bIsGlidingForward = false;
		GlideData.bIsGlidingReverse = false;

		if (Private_ListenerManager::bDebugConsole)
		{
			WR_DBG_FUNC(Log, "listener position lerp glide ended");
		}

		return;
	}
}

bool USoundListenerManagerComponent::UpdateTarget()
{
	bool targetUpdated = false;
	m_shouldAdjustForPawnEyeHeight = false;

	if (!IsValid(m_ListenerManager->m_playerController)) { return false; }
	const APlayerController* playerController = m_ListenerManager->m_playerController;

	{
		switch (m_listManCompProperties.ListenerTarget)
		{
		case EListenerTarget::Player:
			if (IsValid(m_targetComponent) && m_targetComponent->GetAttachParentActor() == playerController->GetPawn()) { return false; }
			targetUpdated = AttachTargetComponent(false, playerController->GetPawn(), m_listManCompProperties.TargetAttachmentPoint,
				FTransform(m_listManCompProperties.TargetAttachmentOffsetRotation, m_listManCompProperties.TargetAttachmentOffsetLocation));
			break;

		case EListenerTarget::CameraViewTarget:
			if (IsValid(m_targetComponent) && m_targetComponent->GetAttachParentActor() == m_playerCameraManager->GetViewTarget()) { return false; }
			targetUpdated = AttachTargetComponent(false, m_playerCameraManager->GetViewTarget(), m_listManCompProperties.TargetAttachmentPoint,
				FTransform(m_listManCompProperties.TargetAttachmentOffsetRotation, m_listManCompProperties.TargetAttachmentOffsetLocation));
			break;

		case EListenerTarget::CustomTarget:	// attachment set in AttachTargetToActor() or AttachTargetToComponent()
			break;

		case EListenerTarget::CustomPosition:
			targetUpdated = AttachTargetComponent(false, m_playerCameraManager->GetDefaultAttachComponent());
			m_targetComponent->SetWorldLocationAndRotation(m_customTargetTransform.GetLocation(), m_customTargetTransform.GetRotation());
			break;

		default:
			break;
		}
	}

	if (targetUpdated)
	{
		if (m_ListenerManager->OnAttenuationReferenceChanged.IsBound())
		{
			m_ListenerManager->OnAttenuationReferenceChanged.Broadcast();
		}

		if (Private_ListenerManager::bDebugConsole)
		{
			const FString enumTarget = UEnum::GetValueAsString(m_listManCompProperties.ListenerTarget);

			FString formattedTargetName{};
			{
				WR_ASSERT(IsValid(m_targetComponent), "m_targetComponent not valid!");

				const FString targetName = IsValid(m_targetComponent->GetAttachParentActor())
					? m_targetComponent->GetAttachParentActor()->GetActorNameOrLabel() : IsValid(m_targetComponent->GetAttachParent())
					? m_targetComponent->GetAttachParent()->GetName() : TEXT("(not found)");

				formattedTargetName = FString(TEXT("listener target: ")).Append(targetName);
			}

			WR_DBG_STATIC_FUNC(Log, "%s, %s", *enumTarget, *formattedTargetName);
		}
	}

	return targetUpdated;
}

FVector USoundListenerManagerComponent::GetListenerPosition()
{
	if (!IsValid(m_targetComponent))
	{
		return m_playerCameraManager->GetCameraLocation();
	}

	const FVector camLocation = m_playerCameraManager->GetCameraLocation();
	const FVector targetLocation = m_targetComponent->GetComponentLocation();

	if (m_listenerGlideData.bIsGlidingForward || m_listenerGlideData.bIsGlidingReverse)
	{
		UpdateGlidingPositionLerp(m_listenerGlideData, m_listManCompProperties.ListenerPositionLerp);
	}

	if (m_probeGlideData.bIsGlidingForward || m_probeGlideData.bIsGlidingReverse)
	{
		UpdateGlidingPositionLerp(m_probeGlideData, m_listManCompProperties.DistanceProbePositionLerp);
	}

	if (m_listManCompProperties.bKeepListenerInSameRoomAsTarget)
	{
		bool shouldClampLerp = false;
		float minLerp = 0.f;

		const TArray<UAkRoomComponent*> camRoomComps = m_akAudioDevice->FindRoomComponentsAtLocation(camLocation, m_currentWorld);
		const UAkRoomComponent* cameraRoom = camRoomComps.IsEmpty() ? nullptr : camRoomComps[0];
		const TArray<UAkRoomComponent*> targetRoomComps = m_akAudioDevice->FindRoomComponentsAtLocation(targetLocation, m_currentWorld);
		const UAkRoomComponent* targetRoom = targetRoomComps.IsEmpty() ? nullptr : targetRoomComps[0];

		const float cameraRoomPriority = IsValid(cameraRoom) ? cameraRoom->Priority : -1.f;
		const float targetRoomPriority = IsValid(targetRoom) ? targetRoom->Priority : -1.f;

		const UAkRoomComponent* innerRoom = cameraRoomPriority > targetRoomPriority ? cameraRoom : targetRoom;

		FVector orientedImpactNormal{};

		/*if (cameraRoom == targetRoom)
		{
			if (m_listenerReturnedToTargetRoomViaConnectingPortal)
			{
				m_listenerLeftTargetRoomViaConnectingPortal = false;
				m_freezeListenerTransform = false;
			}
		}
		else*/
		if (cameraRoom != targetRoom)
		{
			//m_listenerReturnedToTargetRoomViaConnectingPortal = false;
			shouldClampLerp = true;

			TArray<FHitResult> hits;
			static FCollisionObjectQueryParams params = FCollisionObjectQueryParams::AllDynamicObjects;
			params.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
			params.RemoveObjectTypesToQuery(ECollisionChannel::ECC_Camera);
			params.RemoveObjectTypesToQuery(ECollisionChannel::ECC_Pawn);

			GetWorld()->LineTraceMultiByObjectType(hits, camLocation, targetLocation, params);

			for (const FHitResult& hit : hits)
			{
				if (hit.GetComponent()->IsA<UBrushComponent>())
				{
					if (AActor* owner = hit.GetComponent()->GetOwner())
					{
						TSet<UActorComponent*> ownerComps = owner->GetComponents();

						for (const UActorComponent* hitComp : ownerComps)
						{
							if (hitComp->IsA<UAkPortalComponent>())
							{
								const UAkPortalComponent* portalComp = Cast<UAkPortalComponent>(hitComp);
								const TWeakObjectPtr<UAkRoomComponent> frontRoom = portalComp->GetFrontRoomComponent();
								const TWeakObjectPtr<UAkRoomComponent> backRoom = portalComp->GetBackRoomComponent();

								if (((frontRoom == targetRoom) && (backRoom == cameraRoom)) ||
									((frontRoom == cameraRoom) && (backRoom == targetRoom)))
								{
									if (portalComp->GetCurrentState() == AkAcousticPortalState::Closed)
									{
										shouldClampLerp = true;
										break;
									}
									/*else
									{
										m_listenerLeftTargetRoomViaConnectingPortal = true;
										m_listenerReturnedToTargetRoomViaConnectingPortal = false;
									}*/
								}

								continue;
							}

							if (hitComp == innerRoom)
							{
								bool wasHit = false;
								FVector hitLocation{};

								if (innerRoom == targetRoom)
								{
									hitLocation = hit.Location;
									orientedImpactNormal = -hit.ImpactNormal;
									wasHit = true;
								}
								else if (innerRoom == cameraRoom)
								{
									TArray<FHitResult> inversedHits;
									GetWorld()->LineTraceMultiByObjectType(inversedHits, targetLocation, camLocation, params);

									for (const FHitResult& inversedHit : inversedHits)
									{
										if (inversedHit.GetActor() == owner)
										{
											hitLocation = inversedHit.Location;
											orientedImpactNormal = inversedHit.ImpactNormal;
											wasHit = true;

											break;
										}
									}
								}

								// TODO fix
								if (wasHit)
								{
									TArray<FHitResult> geoHits;
									GetWorld()->LineTraceMultiByObjectType(geoHits, camLocation, hitLocation, params);

									for (const FHitResult& geoHit : geoHits)
									{
										if (geoHit.GetComponent()->IsA<UMeshComponent>())
										{
											TArray<USceneComponent*> childComps;
											geoHit.GetComponent()->GetChildrenComponents(false, childComps);

											for (USceneComponent* childComp : childComps)
											{
												if (childComp->IsA<UAkGeometryComponent>())
												{
													if (FVector::DistSquared(camLocation, geoHit.Location)
														< FVector::DistSquared(camLocation, hitLocation))
													{
														hitLocation = geoHit.Location;
														orientedImpactNormal = -geoHit.ImpactNormal;

														WR_DBG_FUNC(Error, "HITHITHIT");
													}
												}
											}
										}
									}

									minLerp = FVector::Dist(camLocation, hitLocation) / FVector::Dist(camLocation, targetLocation);
								}
							}
						}
					}
				}

				if (!shouldClampLerp) { break; }
			}

			/*if (m_listenerLeftTargetRoomViaConnectingPortal)
			{
				m_freezeListenerTransform = true;
				return m_lastListenerPosition;
			}*/
		}

		const float listenerPositionLerp = shouldClampLerp
			? FMath::Clamp(m_listManCompProperties.ListenerPositionLerp, minLerp, 1.f)
			: m_listManCompProperties.ListenerPositionLerp;
		FVector listenerPosition = FMath::Lerp(camLocation, targetLocation, listenerPositionLerp);

		if (shouldClampLerp)
		{
			listenerPosition += orientedImpactNormal;
		}

		return listenerPosition;
	}

	return FMath::Lerp(camLocation, targetLocation, m_listManCompProperties.ListenerPositionLerp);
}

FRotator USoundListenerManagerComponent::GetListenerRotation() const
{
	/*if (m_freezeListenerTransform) { return m_lastListenerRotation; }*/

	switch (m_listManCompProperties.ListenerRotation)
	{
	case EListenerRotation::Target:
		if (IsValid(m_distanceProbe))
		{
			return m_distanceProbe->GetComponentRotation();
		}
		break;

	case EListenerRotation::CameraToTarget:
		if (IsValid(m_targetComponent) && IsValid(m_playerCameraManager))
		{
			return (m_targetComponent->GetComponentLocation() - m_playerCameraManager->GetCameraLocation()).Rotation();
		}
		break;

	case EListenerRotation::Camera: break;

	default: break;
	}

	return IsValid(m_playerCameraManager) ? m_playerCameraManager->GetCameraRotation() : FRotator();
}

bool USoundListenerManagerComponent::AttachTargetComponent(bool isCustomTarget, USceneComponent* AttachToComponent, const FName AttachPointName, const FTransform& AttachPointOffset, EAttachLocation::Type LocationType)
{
	if (!IsValid(AttachToComponent)) { return false; }

	if (isCustomTarget)
	{
		m_listManCompProperties.ListenerTarget = EListenerTarget::CustomTarget;
		m_lastCustomTargetSettings = FCustomTargetSettings(AttachToComponent, AttachPointName, AttachPointOffset, LocationType);
	}

	if (!IsValid(m_targetComponent))
	{
		static const FName nameTargetComp{ TEXT("Listener Target") };
		m_targetComponent = NewObject<USceneComponent>(this, nameTargetComp);
		m_targetComponent->RegisterComponentWithWorld(m_currentWorld);
	}

	if (!IsValid(m_distanceProbe))
	{
		static const FName nameProbeComp{ TEXT("Distance Probe") };
		m_distanceProbe = NewObject<USpatialProbeComponent>(this, nameProbeComp);
		m_distanceProbe->RegisterComponentWithWorld(m_currentWorld);
		m_distanceProbe->AttachToComponent(m_playerCameraManager->GetDefaultAttachComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}

	m_targetComponent->AttachToComponent(AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform,
		AttachToComponent->DoesSocketExist(AttachPointName) ? AttachPointName : NAME_None);
	m_targetComponent->AddLocalRotation(AttachPointOffset.GetRotation());
	m_targetComponent->AddWorldOffset(AttachPointOffset.GetTranslation());

	{
		m_distanceProbe->SetWorldLocationAndRotation(
			FMath::Lerp(m_playerCameraManager->GetCameraLocation(), m_targetComponent->GetComponentLocation(),
				m_listManCompProperties.DistanceProbePositionLerp), m_targetComponent->GetComponentRotation());
	}

	m_akAudioDevice->SetDistanceProbe(m_spatialAudioListener, m_distanceProbe);

#if !UE_BUILD_SHIPPING
	m_lastProbeLocation = m_distanceProbe->GetComponentLocation();
#endif

	if (m_targetComponent->GetAttachParentActor()->IsA<ACharacter>())
	{
		m_characterMovementComp = m_targetComponent->GetAttachParentActor()->FindComponentByClass<UCharacterMovementComponent>();
		m_characterMovementSpeed = FMath::Max(m_characterMovementComp->GetMaxSpeed(),
			GetMovementModeMaxSpeed(m_characterMovementComp, m_listManCompProperties.MinimumCharacterReferenceMovementModeMaxSpeed));
	}
	else
	{
		m_characterMovementComp = nullptr;
		m_characterMovementSpeed = m_spatialAudioListenerMaxSpeed;
	}

	auto onTransformUpdated =
		[this](USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlag, ETeleportType Teleport)->void
		{
			if (!(Teleport == ETeleportType::None)) { m_ListenerManager->OnTeleported(UpdatedComponent); }

			else if (IsValid(m_characterMovementComp))
			{
				const float oldCharacterMovementSpeed = m_characterMovementSpeed;
				m_characterMovementSpeed = FMath::Max(m_characterMovementComp->GetMaxSpeed(),
					m_listManCompProperties.MinimumCharacterReferenceMovementModeMaxSpeed);

				if (m_characterMovementComp->GetMaxSpeed() > oldCharacterMovementSpeed && m_ListenerManager->OnAttenuationReferenceChanged.IsBound())
				{
					m_ListenerManager->OnAttenuationReferenceChanged.Broadcast();
				}
			}
		};

	m_targetComponent->TransformUpdated.AddLambda(onTransformUpdated);
	return true;
}

bool USoundListenerManagerComponent::AttachTargetComponent(bool isCustomTarget, AActor* AttachToActor, const FName AttachPointName, const FTransform& AttachPointOffset, EAttachLocation::Type LocationType)
{
	if (!IsValid(AttachToActor)) { return false; }

	m_shouldAdjustForPawnEyeHeight = false;

	if (AttachToActor->IsA<ACharacter>())
	{
		if (USkeletalMeshComponent* skeletalMeshComp = AttachToActor->FindComponentByClass<USkeletalMeshComponent>())
		{
			m_playerPawn = Cast<APawn>(AttachToActor);
			return AttachTargetComponent(isCustomTarget, skeletalMeshComp, AttachPointName, AttachPointOffset, LocationType);
		}
	}

	if (APawn* attachToPawn = Cast<APawn>(AttachToActor))
	{
		if (AttachTargetComponent(isCustomTarget, attachToPawn->GetRootComponent(), AttachPointName, AttachPointOffset, LocationType))
		{
			m_playerPawn = attachToPawn;
			m_shouldAdjustForPawnEyeHeight = true;
			return true;
		}
	}

	return AttachTargetComponent(isCustomTarget, AttachToActor->GetRootComponent(), AttachPointName, AttachPointOffset, LocationType);
}

void USoundListenerManagerComponent::GlideListenerPositionLerp(const float EndPositionLerp, const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition, const EEasingFunc::Type InterpolationEasingFunction, const float InterpolationEasingFunctionBlendExponent)
{
	m_listenerGlideData = FGlideData(!bReturnToStartPosition, m_listManCompProperties.ListenerPositionLerp, EndPositionLerp,
		Duration, GetWorld()->GetTimeSeconds(), InterpolationEasingFunctionBlendExponent, InterpolationEasingFunction);
}

void USoundListenerManagerComponent::GlideDistanceProbePositionLerp(const float EndPositionLerp, const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition, const EEasingFunc::Type InterpolationEasingFunction, const float InterpolationEasingFunctionBlendExponent)
{
	m_probeGlideData = FGlideData(!bReturnToStartPosition, m_listManCompProperties.DistanceProbePositionLerp, EndPositionLerp,
		Duration, GetWorld()->GetTimeSeconds(), InterpolationEasingFunctionBlendExponent, InterpolationEasingFunction);
}

float USoundListenerManagerComponent::GetDistanceProbeMaxSpeed() const
{
	return IsValid(m_characterMovementComp) ? m_characterMovementSpeed : m_spatialAudioListenerMaxSpeed;
}

void USoundListenerManagerComponent::DebugDraw() const
{
	WR_ASSERT(IsValid(m_playerCameraManager), "player camera manager not valid");
	WR_ASSERT(IsValid(m_spatialAudioListener), "spatial audio listener not valid");

	const FThemeListenerManager& debugTheme = GetDefault<UWwiserRThemeSettings>()->ThemeListenerManager;
	const FColor dbgListenerColor = debugTheme.ListenerColor;
	const FColor dbgListenerTargetColor = debugTheme.ListenerTargetColor;
	const float dbgListenerSize = debugTheme.ListenerSize;
	const float dbgListenerTargetSize = debugTheme.ListenerTargetSize;
	const float dbgListenerGizmoSize = debugTheme.ListenerGizmoSize;

	const FVector camLocation = m_playerCameraManager->GetCameraLocation();
	const FVector listenerLocation = m_spatialAudioListener->GetComponentLocation();
	const FVector probeLocation = m_distanceProbe->GetComponentLocation();
	const FVector targetLocation = IsValid(m_targetComponent) ? m_targetComponent->GetComponentLocation() : m_distanceProbe->GetComponentLocation();

	//*** WorldListeners ***//
	//if (Private_ListenerManager::bDebugDrawWorldListeners)
	{
		for (TWeakObjectPtr<UWorldSoundListener> worldListener : m_ListenerManager->m_worldListeners)
		{
			const FVector worldListenerLocation = worldListener->GetComponentLocation();
			const float dbgDistSquared = FVector::DistSquared(worldListenerLocation, m_distanceProbe->GetComponentLocation());
			const float dbgDistInMeters = FMath::Sqrt(dbgDistSquared) / 100;
			const float scaleFactor = FMath::Pow(25.f / FMath::Clamp(dbgDistInMeters, 10.f, 100.f), .11f);
			const float drawSize = dbgListenerSize * scaleFactor;

			DrawDebugCone(m_currentWorld, worldListenerLocation, worldListener->GetForwardVector(), drawSize, .5f, .5f, 24, dbgListenerColor);
			DrawDebugSphere(m_currentWorld, worldListenerLocation, .4f * drawSize * m_listManCompProperties.ListenerPositionLerp, 16, dbgListenerColor);
			UAudioUtils::DrawDebugArrow(m_currentWorld, worldListenerLocation, worldListener->GetForwardVector(), 1.2f * drawSize, dbgListenerColor);
		}
	}

	//*** Listener ***//
	if (Private_ListenerManager::bDebugDrawListener)
	{
		DrawDebugCone(m_currentWorld, listenerLocation, m_spatialAudioListener->GetForwardVector(), dbgListenerSize, .5f, .5f, 24, dbgListenerColor);
		DrawDebugSphere(m_currentWorld, listenerLocation, .2f * dbgListenerSize * m_listManCompProperties.ListenerPositionLerp, 16, dbgListenerColor);
		UAudioUtils::DrawDebugArrow(m_currentWorld, listenerLocation, m_spatialAudioListener->GetForwardVector(), 1.2f * dbgListenerSize, dbgListenerColor);
	}

	if (Private_ListenerManager::bDebugDrawGizmoListener)
	{
		UAudioUtils::DrawDebugGizmo(m_currentWorld, listenerLocation, m_spatialAudioListener->GetComponentRotation(), dbgListenerGizmoSize);
	}

	//*** Listener Target and Distance Probe ***//
	const FVector forwardVector = m_distanceProbe->GetForwardVector();
	const FVector rightVector = 1.5f * m_distanceProbe->GetRightVector();

	if (Private_ListenerManager::bDebugDrawTarget)
	{
		DrawDebugSphere(m_currentWorld, targetLocation, dbgListenerTargetSize / 2.f, 12, dbgListenerTargetColor);
		DrawDebugCone(m_currentWorld, targetLocation, forwardVector + rightVector, dbgListenerTargetSize, .3f, .3f, 24, dbgListenerTargetColor);
		DrawDebugCone(m_currentWorld, targetLocation, forwardVector - rightVector, dbgListenerTargetSize, .3f, .3f, 24, dbgListenerTargetColor);

		if (Private_ListenerManager::bDebugDrawGizmoTarget)
		{
			UAudioUtils::DrawDebugGizmo(m_currentWorld, targetLocation, m_distanceProbe->GetComponentRotation(), dbgListenerTargetSize);
		}
	}

	if (Private_ListenerManager::bDebugDrawProbe)
	{
		DrawDebugSphere(m_currentWorld, probeLocation, debugTheme.DistanceProbeSize, 12, debugTheme.DistanceProbeColor);
	}

	//*** Text ***//
	const float distToCam = FVector::Distance(listenerLocation, camLocation);
	FString distanceMsg = FString::Printf(TEXT("Listener: %.2f m"), distToCam / 100.f);

	const float distToProbe = FVector::Distance(listenerLocation, probeLocation);
	const float distToTarget = FVector::Distance(listenerLocation, targetLocation);

	if (Private_ListenerManager::bDebugDrawProbe)
	{
		distanceMsg.Append(FString::Printf(TEXT("\nProbe: % .2f m"), distToProbe / 100.f));
	}

	if (Private_ListenerManager::bDebugDrawTarget && (distToProbe != distToTarget))
	{
		distanceMsg.Append(FString::Printf(TEXT("  -  Target: %.2f"), distToTarget / 100.f));
	}

	const FVector textOrigin = m_listManCompProperties.ListenerPositionLerp > 0.f
		? listenerLocation + .2f * distToCam * (FVector(0.f, 0.f, -.5f) - m_spatialAudioListener->GetRightVector())
		: camLocation + 5.f * m_playerCameraManager->GetCameraRotation().Quaternion().GetForwardVector() + FVector(0.f, 0.f, -.5f)
		- m_spatialAudioListener->GetRightVector();

	DrawDebugString(m_currentWorld, textOrigin, distanceMsg, nullptr, dbgListenerColor, 0.f, true, 1.1f);

	//*** Text DistanceProbe ***//
	if (Private_ListenerManager::bDebugShowProbeSpeed)
	{
		const float probeSpeed = (FVector::Distance(probeLocation, m_lastProbeLocation) / m_currentWorld->DeltaTimeSeconds) / 100.f;
		const FString LPSpeedMsg = FString::Printf(TEXT("%.3f m/s"), probeSpeed);

		DrawDebugString(m_currentWorld, probeLocation + FVector(0.f, 0.f, 40.f), LPSpeedMsg, nullptr, FColor::White, 0.f, true, 1.1f);

		m_lastProbeLocation = probeLocation;
	}
}
#pragma endregion

#pragma region USoundListenerManager - Properties
UPARAM(DisplayName = "Settings Imported") bool USoundListenerManager::ImportListenerManagerComponentProperties(UDAListenerManagerSettings* DAListenerManagerSettings)
{
	if (!IsValid(DAListenerManagerSettings)) { WR_DBG_FUNC(Error, "Listener Manager Settings not found"); return false; }

	m_listenerManagerComponentProperties = DAListenerManagerSettings->ListenerManagerComponentProperties;
	m_defaultListenerMaxSpeed = DAListenerManagerSettings->DefaultListenerMaxSpeed;

	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->ImportListenerManagerComponentProperties(DAListenerManagerSettings->ListenerManagerComponentProperties);
	}

	return true;
}

void USoundListenerManager::SetListenerRotation(const EListenerRotation NewListenerRotation)
{
	m_listenerManagerComponentProperties.ListenerRotation = NewListenerRotation;

	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->SetListenerRotation(NewListenerRotation);
	}
}

void USoundListenerManager::SetListenerTarget(const EListenerTarget NewListenerTarget)
{
	m_listenerManagerComponentProperties.ListenerTarget = NewListenerTarget;

	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->SetListenerTarget(NewListenerTarget);
	}
}

void USoundListenerManager::SetListenerPositionLerp(const float NewPositionLerp, const bool bTriggerEmitterRecull)
{
	m_listenerManagerComponentProperties.ListenerPositionLerp = NewPositionLerp;

	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->SetListenerPositionLerp(NewPositionLerp, bTriggerEmitterRecull);
	}
}

void USoundListenerManager::SetDistanceProbeMaxSpeed(float MaxSpeed)
{
	m_listenerManagerComponentProperties.SpatialAudioListenerMaxSpeed = MaxSpeed;

	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->SetDistanceProbeMaxSpeed(MaxSpeed);
	}
}
#pragma endregion

#pragma region USoundListenerManager - PIE instances
#if WITH_EDITOR
void USoundListenerManager::SetActiveListenerManager(USoundListenerManager* ListenerManager)
{
	s_activeListenerManagers.Reset();
	s_activeListenerManagers.Emplace(ListenerManager);
}

void USoundListenerManager::AddActiveListenerManager(USoundListenerManager* ListenerManager)
{
	if (!s_activeListenerManagers.Contains(ListenerManager))
	{
		s_activeListenerManagers.Emplace(ListenerManager);
	}
}

void USoundListenerManager::RemoveActiveListenerManager(USoundListenerManager* ListenerManager)
{
	if (s_activeListenerManagers.Contains(ListenerManager))
	{
		s_activeListenerManagers.Remove(ListenerManager);
	}
}
#endif
/*void USoundListenerManager::UpdateListeners(UAkComponent* AkComponent)
{
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	if (m_worldListeners.IsEmpty())
	{
		SoundEngine->ResetListenersToDefault(AkComponent->GetAkGameObjectID());
		return;
	}

	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AkAudioDevice)) { return; }

	const int numListeners = m_currentListenerIds.Num();
	auto pListenerIds = (AkGameObjectID*)alloca(numListeners * sizeof(AkGameObjectID));

	int i = 0;
	for (const AkGameObjectID gameObjId : m_currentListenerIds)
	{
		pListenerIds[i] = gameObjId;
		i++;
	}

	SoundEngine->SetListeners(AkComponent->GetAkGameObjectID(), pListenerIds, numListeners);
}*/
#pragma endregion

#pragma region USoundListenerManager - Internal
void USoundListenerManager::Initialize()
{
#if WITH_EDITOR
	if (UEditorAudioUtils::GetRunUnderOneProcess() && UEditorAudioUtils::GetSoloAudioInFirstPIEClient())
	{
		SetActiveListenerManager(this);
	}
	else
	{
		AddActiveListenerManager(this);
	}

	if (!s_allConnectedListenerManagers.Contains(this)) { s_allConnectedListenerManagers.Add(this); }
#endif

	if (!IsValid(ListenerManagerSettings))
	{
		static const FName settingsName{ TEXT("Default Listener Manager Settings") };
		ListenerManagerSettings = NewObject< UDAListenerManagerSettings>(this, settingsName);
	}

	m_listenerManagerComponentProperties = ListenerManagerSettings->ListenerManagerComponentProperties;
	ImportListenerManagerComponentProperties(ListenerManagerSettings);

	//FWorldDelegates::OnPostWorldCreation.AddUObject(this, &USoundListenerManager::BeginPlay);
	FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &USoundListenerManager::EndPlay);

	WR_DBG_NET(Log, "initialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}

void USoundListenerManager::Deinitialize()
{
	//FWorldDelegates::OnPostWorldCreation.RemoveAll(this);
	FWorldDelegates::OnWorldBeginTearDown.RemoveAll(this);

	OnAttenuationReferenceChanged.Clear();
	OnMaxSpeedIncreased.Clear();

	/*if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		for (TWeakObjectPtr<UAkComponent> defaultListener : AkAudioDevice->GetDefaultListeners())
		{
			AkAudioDevice->RemoveDefaultListener(defaultListener.Get());
		}
	}*/

#if WITH_EDITOR
	s_activeListenerManagers.Remove(this);
	s_allConnectedListenerManagers.Remove(this);
#endif

	WR_DBG_NET(Log, "deinitialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}

/*void USoundListenerManager::BeginPlay(UWorld* world)
{
	//UWorld* world = GetWorld();
	m_playerController = world->GetFirstPlayerController();

	//if (Private_ListenerManager::bDebugConsole)
	{
		WR_DBG_FUNC(Log, "world = %s, PC = %s", *world->GetName(),
			IsValid(m_playerController) ? *m_playerController->GetName() : TEXT("none"));
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		OnSpatialAudioListenerChanged_Internal(AkAudioDevice->GetSpatialAudioListener());
	}
}*/

void USoundListenerManager::EndPlay(UWorld* World)
{
	OnMaxSpeedIncreased.Clear();
	OnAttenuationReferenceChanged.Clear();
}

void USoundListenerManager::Tick(float DeltaTime)
{
	// lazy instantiation of SoundListenerManagerComponent on the player controller, because the player controller might not yet be
	// in the new world when that world is initialized. We stop ticking as soon as instantiation is done (see IsTickable() in .h

	if (m_lastTickFrame == GFrameCounter) { return; }
	m_lastTickFrame = GFrameCounter;

	UWorld* world = GetWorld();
	m_playerController = world->GetFirstPlayerController();

	const FName nameListenerManagerComp{ TEXT("SoundListenerManagerComponent") };
	m_SoundListenerManagerComponent = NewObject<USoundListenerManagerComponent>(m_playerController, nameListenerManagerComp);
	m_SoundListenerManagerComponent->Initialize(this);
	m_SoundListenerManagerComponent->RegisterComponentWithWorld(world);
}

void USoundListenerManager::OnTeleported(USceneComponent* UpdatedComponent)
{
	if (UpdatedComponent != m_spatialAudioListener) return;

	if (Private_ListenerManager::bDebugConsole)
	{
		WR_DBG(Log, "distance probe teleported")
	}

	OnAttenuationReferenceChanged.Broadcast();
}

void USoundListenerManager::RemoveSpatialAudioListener()
{
	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		if (UAkComponent* spatialAudioListener = AkAudioDevice->GetSpatialAudioListener())
		{
				AkAudioDevice->RemoveDefaultListener(spatialAudioListener);
				m_currentListenerIds.Remove(m_spatialAudioListenerID);
				m_spatialAudioListenerID = AK_INVALID_GAME_OBJECT;
				m_spatialAudioListener = nullptr;
		}
	}
}

void USoundListenerManager::OnSpatialAudioListenerChanged_Internal(UAkComponent* NewSpatialAudioListener)
{
	if (m_spatialAudioListenerID != AK_INVALID_GAME_OBJECT)
	{
		m_currentListenerIds.Remove(m_spatialAudioListenerID);
	}

	if (!IsValid(NewSpatialAudioListener)) { return; }

	m_spatialAudioListener = NewSpatialAudioListener;
	m_spatialAudioListenerID = NewSpatialAudioListener->GetAkGameObjectID();
	m_currentListenerIds.Add(m_spatialAudioListenerID);

#if WITH_EDITOR
	if (UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld()))
	{
		ConnectSpatialAudioListener();
	}
#endif

	if (OnSpatialAudioListenerChanged.IsBound())
	{
		OnSpatialAudioListenerChanged.Broadcast(GetWorld(), m_spatialAudioListener);
	}

	auto onTransformUpdated =
		[this](USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlag, ETeleportType Teleport)->void
		{
			if (!(Teleport == ETeleportType::None)) { OnTeleported(UpdatedComponent); }
		};

	m_spatialAudioListener->TransformUpdated.AddLambda(onTransformUpdated);
}

#if WITH_EDITOR
void USoundListenerManager::ConnectSpatialAudioListener()
{
	if (!UEditorAudioUtils::GetSoloAudioInFirstPIEClient() && !UAudioSubsystem::Get(GetWorld())->m_muteWhenGameNotInForeground)
	{
		return;
	}

	const UWorld* world = GetWorld();
	if (!IsValid(world)) { return; }

	if (!UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld())) { return; }

	ConnectSpatialAudioListener_Internal();
	FTimerDelegate connectInternal;
	world->GetTimerManager().SetTimerForNextTick(this, &USoundListenerManager::ConnectSpatialAudioListener_Internal);
}

void USoundListenerManager::ConnectSpatialAudioListener_Internal()
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AkAudioDevice)) { return; }

	if (UEditorAudioUtils::GetSoloAudioInFirstPIEClient())
	{
		for (USoundListenerManager* listenerManager : s_allConnectedListenerManagers)
		{
			if (!s_activeListenerManagers.Contains(listenerManager))
			{
				listenerManager->RemoveSpatialAudioListener();
			}
		}

		for (USoundListenerManager* listenerManager : s_allConnectedListenerManagers)
		{
			if (s_activeListenerManagers.Contains(listenerManager))
			{
				listenerManager->RemoveSpatialAudioListener();
			}
		}
	}
	else // not solo first client
	{
		SetActiveListenerManager(this);

		UWorld* world = GetWorld();

		if (IsValid(world) && IsValid(world->GetFirstPlayerController()))
		{
			if (APlayerCameraManager* cameraManager = world->GetFirstPlayerController()->PlayerCameraManager)
			{
				UAkComponent* newListener = cameraManager->FindComponentByClass<UAkComponent>();

				if (!IsValid(newListener))
				{
					static const FName newListenerName{ TEXT("New Listener") };
					newListener = NewObject<UAkComponent>(this, newListenerName);
					newListener->RegisterComponentWithWorld(world);
				}

				AkAudioDevice->AddDefaultListener(newListener);
				AkAudioDevice->SetSpatialAudioListener(newListener);

				UAkComponentSet& defaultListeners = AkAudioDevice->GetDefaultListeners();

				for (TWeakObjectPtr<UAkComponent> defaultListener : defaultListeners)
				{
					if (defaultListener->GetWorld() != world)
					{
						AkAudioDevice->RemoveDefaultListener(defaultListener.Get());
					}
				}
			}
		}
	}

	OnSpatialAudioListenerChanged_Internal(AkAudioDevice->GetSpatialAudioListener());
}

void USoundListenerManager::MuteInstance(bool bMute)
{
	if (IsValid(GetWorld()) && !UEditorAudioUtils::IsRunningUnderOneProcess(GetWorld())) { return; }

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		UAkComponentSet defaultListeners = AkAudioDevice->GetDefaultListeners();

		if (UAkComponent* spatialAudioListener = AkAudioDevice->GetSpatialAudioListener())
		{
			if (bMute)
			{
				RemoveActiveListenerManager(this);

				if (defaultListeners.Contains(spatialAudioListener))
				{
					AkAudioDevice->RemoveDefaultListener(spatialAudioListener);
				}
			}
			else
			{
				AddActiveListenerManager(this);

				if (UWorld* world = GetWorld())
				{
					if (IsValid(world) && IsValid(world->GetFirstPlayerController()))
					{
						if (APlayerCameraManager* cameraManager = world->GetFirstPlayerController()->PlayerCameraManager)
						{
							UAkComponent* newListener = cameraManager->FindComponentByClass<UAkComponent>();

							if (!IsValid(newListener))
							{
								static const FName newListenerName{ TEXT("New Listener") };
								newListener = NewObject<UAkComponent>(this, newListenerName);
								newListener->RegisterComponentWithWorld(world);
							}

							if (!defaultListeners.Contains(newListener))
							{
								AkAudioDevice->AddDefaultListener(newListener);

							}

							AkAudioDevice->SetSpatialAudioListener(newListener);
						}
					}
				}
			}
		}
	}
}
#endif

#pragma endregion

#pragma region USoundListenerManager - Static Getters
UAkComponent* USoundListenerManager::GetSpatialAudioListener() const
{
	return m_spatialAudioListener;
}

FTransform USoundListenerManager::GetSpatialAudioListenerTransform() const
{
	return IsValid(m_spatialAudioListener) ? m_spatialAudioListener->GetComponentTransform() : FTransform();
}

FVector USoundListenerManager::GetSpatialAudioListenerPosition() const
{
	return IsValid(m_spatialAudioListener) ? m_spatialAudioListener->GetComponentLocation() : FVector();
}

FRotator USoundListenerManager::GetSpatialAudioListenerRotation() const
{
	return IsValid(m_spatialAudioListener) ? m_spatialAudioListener->GetComponentRotation() : FRotator();
}

USceneComponent* USoundListenerManager::GetListenerTargetComp() const
{
	return IsValid(m_SoundListenerManagerComponent) ? m_SoundListenerManagerComponent->GetListenerTargetComponent() : nullptr;
}

FVector USoundListenerManager::GetDistanceProbePosition() const
{
	if (IsValid(m_SoundListenerManagerComponent))
	{
		if (UAkComponent* distanceProbe = m_SoundListenerManagerComponent->GetDistanceProbe())
		{
			return distanceProbe->GetComponentLocation();
		}
	}

	if (IsValid(m_spatialAudioListener))
	{
		return m_spatialAudioListener->GetComponentLocation();
	}

	return FVector();
}

float USoundListenerManager::GetSquaredDistanceToDistanceProbe(const FVector& Location) const
{
	if (IsValid(m_SoundListenerManagerComponent))
	{
		if (UAkComponent* distanceProbe = m_SoundListenerManagerComponent->GetDistanceProbe())
		{
			return FVector::DistSquared(Location, distanceProbe->GetComponentLocation());
		}
	}

	if (IsValid(m_spatialAudioListener))
	{
		return FVector::DistSquared(Location, m_spatialAudioListener->GetComponentLocation());
	}

	return INFINITY;
}

float USoundListenerManager::GetDistanceProbeMaxSpeed() const
{
	return IsValid(m_SoundListenerManagerComponent) ? m_SoundListenerManagerComponent->GetDistanceProbeMaxSpeed() : 0.f;
}
#pragma endregion

#pragma region USoundListenerManager - Public Methods
void USoundListenerManager::AttachTargetToComponent(
	USceneComponent* AttachToComponent, const FName AttachPointName, const FTransform& AttachPointOffset, EAttachLocation::Type LocationType)
{
	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->AttachTargetComponent(true, AttachToComponent, AttachPointName, AttachPointOffset, LocationType);
	}
}

void USoundListenerManager::AttachTargetToActor(
	AActor* AttachToActor, const FName AttachPointName, const FTransform& AttachPointOffset, EAttachLocation::Type LocationType)
{
	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->AttachTargetComponent(true, AttachToActor, AttachPointName, AttachPointOffset, LocationType);
	}
}

void USoundListenerManager::SetTargetTransform(FVector Location, FQuat Rotation)
{
	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->SetTargetTransform(Location, Rotation);
	}
}

void USoundListenerManager::ResetToDefaultSettings()
{
	ImportListenerManagerComponentProperties(ListenerManagerSettings);
}

void USoundListenerManager::GlideListenerPositionLerp(const float EndPositionLerp,
	const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition,
	const EEasingFunc::Type InterpolationEasingFunction, const float InterpolationEasingFunctionBlendExponent)
{
	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->GlideListenerPositionLerp(EndPositionLerp, TargetVerticalOffset, Duration,
			bReturnToStartPosition, InterpolationEasingFunction, InterpolationEasingFunctionBlendExponent);
	}
}
void USoundListenerManager::GlideDistanceProbePositionLerp(const float EndPositionLerp,
	const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition,
	const EEasingFunc::Type InterpolationEasingFunction, const float InterpolationEasingFunctionBlendExponent)
{
	if (IsValid(m_SoundListenerManagerComponent))
	{
		m_SoundListenerManagerComponent->GlideDistanceProbePositionLerp(EndPositionLerp, TargetVerticalOffset, Duration,
			bReturnToStartPosition, InterpolationEasingFunction, InterpolationEasingFunctionBlendExponent);
	}
}

void USoundListenerManager::AddWorldListener(UWorldSoundListener* WorldListener)
{
	m_worldListeners.Add(WorldListener);
	m_currentListenerIds.Add(WorldListener->GetAkGameObjectID());
}

void USoundListenerManager::RemoveWorldListener(UWorldSoundListener* WorldListener)
{
	m_worldListeners.Remove(WorldListener);
	m_currentListenerIds.Remove(WorldListener->GetAkGameObjectID());
}
#pragma endregion
