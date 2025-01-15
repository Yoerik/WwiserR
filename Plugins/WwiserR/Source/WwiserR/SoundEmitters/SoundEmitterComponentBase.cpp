// // Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "SoundEmitterComponentBase.h"

#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "AkAudioEvent.h"
#include "AkSwitchValue.h"
#include "AkAuxBus.h"
#include "AkSpotReflector.h"
#include "WwiseSoundEngine/Public/Wwise/API/WwiseSoundEngineAPI.h"
#include "Core/AudioSubsystem.h"
#include "Core/AudioUtils.h"
#include "Managers/SoundListenerManager.h"
#include "WorldSoundListenerComponent.h"
//#include "SpatialAudio/SpatialAudioVolume.h"

#if !UE_BUILD_SHIPPING
#include "Config/AudioConfig.h"
#include "Config/DebugTheme.h"
#include "Kismet/GameplayStatics.h"
#endif

#pragma region CVars
namespace Private_SoundEmitterComponentBase
{
	static TAutoConsoleVariable<bool> CVar_LogRepeatingOneShot(TEXT("WwiserR.SoundEmitter.RepeatingOneShot.DebugToConsole"),
		false, TEXT("Log non-critical messages to console. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_LogRepeatingOneShotAkEvents(
		TEXT("WwiserR.SoundEmitter.RepeatingOneShot.AkEvents.DebugToConsole"), false,
		TEXT("Log non-critical messages to console. (0 = off, 1 = on)"), ECVF_Cheat);

	static TAutoConsoleVariable<bool> CVar_SoundEmitter_DebugConsole(
		TEXT("WwiserR.SoundEmitter.DebugToConsole"), false, TEXT("Log non-critical messages to console. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_SoundEmitter_LogEvents(
		TEXT("WwiserR.SoundEmitter.DebugToConsole.Events"), true, TEXT("Log posting events to console. (0 = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<bool> CVar_SoundEmitter_LogGameSynchs(
		TEXT("WwiserR.SoundEmitter.DebugToConsole.GameSynchs"), true, TEXT("Log posting game synchs to console. (0 = off, 1 = on)"), ECVF_Cheat);

	static TAutoConsoleVariable<bool> CVar_SoundEmitter_DebugDraw(
		TEXT("WwiserR.SoundEmitter.DebugDraw"), false, TEXT("Enable visual sound emitter debugging. (0  = off, 1 = on)"), ECVF_Cheat);
	static TAutoConsoleVariable<float> CVar_SoundEmitter_DebugRegistration(TEXT("WwiserR.SoundEmitter.DebugDraw.Registration"), 1.f,
		TEXT("DebugDraw - show registered emitters. (0 = off, 1 = on, or enter range in meters)"), ECVF_Cheat);
	static TAutoConsoleVariable<float> CVar_SoundEmitter_DebugEmitterDistance(TEXT("WwiserR.SoundEmitter.DebugDraw.Distance"), 1.f,
		TEXT("DebugDraw - show distance. (0 = off, 1 = on, or enter range in meters)"), ECVF_Cheat);
	static TAutoConsoleVariable<float> CVar_SoundEmitter_DebugEvents(TEXT("WwiserR.SoundEmitter.DebugDraw.Events"), 1.f,
		TEXT("DebugDraw - show playing events. (0 = off, 1 = on, or enter range in meters)"), ECVF_Cheat);
	static TAutoConsoleVariable<float> CVar_SoundEmitter_DebugGizmos(TEXT("WwiserR.SoundEmitter.DebugDraw.Gizmos"), 0.f,
		TEXT("DebugDraw - show emitter gizmos. (0 = off, 1 = on, or enter range in meters)"), ECVF_Cheat);
	static TAutoConsoleVariable<float> CVar_SoundEmitter_DebugGizmoScale(
		TEXT("WwiserR.SoundEmitter.DebugDraw.GizmoScale"), 1.f, TEXT("DebugDraw - emitter gizmo scale."), ECVF_Cheat);

	bool bLogGameSynchs = true;
	bool bLogRepeatingOneShot = false;
	bool bLogRepeatingOneShotAkEvents = false;

	float fDebugDrawRegistration = 1.f;
	float fDebugDrawDistance = 1.f;
	float fDebugDrawEvents = 1.f;
	float fDebugDrawGizmos = 0.f;
	float fDebugDrawGizmoScale = 1.f;

	static void OnSoundEmitterComponentBaseUpdate()
	{
		//if (USoundEmitterComponentBase::s_debugDraw != CVar_SoundEmitter_DebugDraw.GetValueOnGameThread())
		{
			USoundEmitterComponentBase::s_debugDraw = CVar_SoundEmitter_DebugDraw.GetValueOnGameThread();
#if !UE_BUILD_SHIPPING
			if (USoundEmitterComponentBase::OnEmitterDebugDrawChanged.IsBound())
			{
				USoundEmitterComponentBase::OnEmitterDebugDrawChanged.Broadcast();
			}
#endif
		}

		USoundEmitterComponentBase::s_debugToConsole = CVar_SoundEmitter_DebugConsole.GetValueOnGameThread();
		USoundEmitterComponentBase::s_logEvents = CVar_SoundEmitter_LogEvents.GetValueOnGameThread();

		bLogGameSynchs = CVar_SoundEmitter_LogGameSynchs.GetValueOnGameThread();
		bLogRepeatingOneShot = CVar_LogRepeatingOneShot.GetValueOnGameThread();
		bLogRepeatingOneShotAkEvents = CVar_LogRepeatingOneShotAkEvents.GetValueOnGameThread();

		fDebugDrawRegistration = CVar_SoundEmitter_DebugRegistration.GetValueOnGameThread();
		fDebugDrawDistance = CVar_SoundEmitter_DebugEmitterDistance.GetValueOnGameThread();
		fDebugDrawEvents = CVar_SoundEmitter_DebugEvents.GetValueOnGameThread();
		fDebugDrawGizmos = CVar_SoundEmitter_DebugGizmos.GetValueOnGameThread();
		fDebugDrawGizmoScale = CVar_SoundEmitter_DebugGizmoScale.GetValueOnGameThread();
	}

	FAutoConsoleVariableSink CSoundEmitterComponentBaseConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnSoundEmitterComponentBaseUpdate));
} // namespace Private_SoundEmitterComponentBase
#pragma endregion

#pragma region URepeatingOneShot
URepeatingOneShot::URepeatingOneShot()
{
	if (UObject* outer = GetOuter())
	{
		OwningSoundEmitterComponent = Cast<USoundEmitterComponentBase>(GetOuter());
	}
}

void URepeatingOneShot::BeginDestroy()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (IsValid(OwningSoundEmitterComponent))
	{
		OwningSoundEmitterComponent->m_repeatingOneShots.Remove(this);
	}

	Super::BeginDestroy();
}

void URepeatingOneShot::EndPlay()
{
	if (IsPlaying())
	{
		OwningSoundEmitterComponent->StopOneShotByPlayingId(ActivePlayingID, 0);
	}

	OnEndRepeatingOneShot();
}

bool URepeatingOneShot::EndIfNoRepetitionsLeft(UWorld* world)
{
	if (!RepeatableOneShot.bShouldRepeat || RepetitionsLeft == 0)
	{
		OnEndRepeatingOneShot();
		return true;
	}

	return false;
}

void URepeatingOneShot::OnAkPostEventCallback(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo)
{
	if (CallbackType == EAkCallbackType::EndOfEvent)
	{
		OnEndOneShot(CallbackInfo);
		ActivePlayingID = AK_INVALID_PLAYING_ID;
	}
	else
	{
		AkPostEventCallback.ExecuteIfBound(CallbackType, CallbackInfo);
	}
}

void URepeatingOneShot::OnStartOneShot(int32 PlayingID)
{
	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot)
	{
		WR_DBG_FUNC(Log, "AkEvent started: %s", *RepeatableOneShot.AkEvent->GetName());
	}

	OneShotPostedCallback.ExecuteIfBound(PlayingID);
}

void URepeatingOneShot::OnEndOneShot(UAkCallbackInfo* CallbackInfo)
{
	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot)
	{
		WR_DBG_FUNC(Log, "AkEvent ended: %s", *RepeatableOneShot.AkEvent->GetName());
	}

	if (bPostAkPostEventCallbackOnEndOfEvent)
	{
		AkPostEventCallback.ExecuteIfBound(EAkCallbackType::EndOfEvent, CallbackInfo);
	}

	if (bIsPaused)
	{
		OnPauseRepeatingOneShot();
	}
	else
	{
		EndIfNoRepetitionsLeft(GetWorld());
	}
}

void URepeatingOneShot::OnStartRepeatingOneShot()
{
	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot)
	{
		WR_DBG_FUNC(Log, "Repeating OneShot started: %s", *RepeatableOneShot.AkEvent->GetName());
	}
}

void URepeatingOneShot::OnPauseRepeatingOneShot()
{
	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot)
	{
		WR_DBG_FUNC(Log, "Repeating OneShot paused: %s", *RepeatableOneShot.AkEvent->GetName());
	}
}

void URepeatingOneShot::OnResumeRepeatingOneShot()
{
	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot)
	{
		WR_DBG_FUNC(Log, "Repeating OneShot resumed: %s", *RepeatableOneShot.AkEvent->GetName());
	}
}

void URepeatingOneShot::OnEndRepeatingOneShot()
{
	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot)
	{
		WR_DBG_FUNC(Log, "Repeating OneShot ended: %s", *RepeatableOneShot.AkEvent->GetName());
	}

	RepeatingOneShotEndedCallback.ExecuteIfBound(this);
	ConditionalBeginDestroy();
}

void URepeatingOneShot::PostRepeatingOneShotInternal()
{
	// this should not happen, just keeping it here for a while to check
	if (!IsValid(RepeatableOneShot.AkEvent))
	{
		WR_DBG_FUNC(Error, "AkEvent invalid");
		return;
	}

	// this should not happen, just keeping it here for a while to check
	if (!IsValid(OwningSoundEmitterComponent))
	{
		WR_DBG_FUNC(Error, "OwningSoundEmitterComponent invalid");
		return;
	}

	bool bShouldBlockOverlap = false;
	if (RepeatableOneShot.bBlockOverlaps)
	{
		if (ActivePlayingID != AK_INVALID_PLAYING_ID)
		{
			bShouldBlockOverlap = true;
		}
	}

	if (!bShouldBlockOverlap)
	{
		const int callbackMask = bPostAkPostEventCallbackOnEndOfEvent ? CallbackMask : CallbackMask + 1;

		ActivePlayingID = OwningSoundEmitterComponent->PostOneShot(RepeatableOneShot.AkEvent, callbackMask,
			AkPostEventCallbackInternal, AttenuationRangeBuffer, bQueryAndPostEnvironmentSwitches);

		if (RepetitionsLeft > 0)
		{
			RepetitionsLeft--;
		}

		if (ActivePlayingID != AK_INVALID_PLAYING_ID)
		{
			OnStartOneShot(ActivePlayingID);
		}
		else if (RepetitionsLeft == 0)
		{
			OnEndRepeatingOneShot();
		}
	}

	if (!RepeatableOneShot.bShouldRepeat)
	{
		RepetitionsLeft = 0;
	}

	if (RepetitionsLeft != 0)
	{
		ScheduleNextRepeatingOneShot();
	}
}

void URepeatingOneShot::ScheduleNextRepeatingOneShot()
{
	const float timeBeforePostingNextEvent = FMath::RandRange(RepeatableOneShot.MinTimeInterval, RepeatableOneShot.MaxTimeInterval);

	if (UWorld* world = GetWorld())
	{
		if (timeBeforePostingNextEvent > 0.f)
		{
			world->GetTimerManager().SetTimer(m_nextPostAkEventTimeHandle, this, &URepeatingOneShot::PostRepeatingOneShotInternal,
				timeBeforePostingNextEvent, false);
		}
		else
		{
			world->GetTimerManager().ClearTimer(m_nextPostAkEventTimeHandle);
			world->GetTimerManager().SetTimerForNextTick(this, &URepeatingOneShot::PostRepeatingOneShotInternal);
		}
	}
}

void URepeatingOneShot::Initialize(USoundEmitterComponentBase* a_Owner, UAkAudioEvent* a_AkEvent, int32 a_CallbackMask,
	const FOnAkPostEventCallback& a_AkPostEventCallback, const FOnOneShotCallback& a_OneShotPostedCallback,
	const FOnRepeatingOneShotEndedCallback& a_RepeatingOneShotEndedCallback, bool a_bShouldRepeat, int32 a_MaxRepetitions,
	float a_MinTimeInterval, float a_MaxTimeInterval, bool a_bBlockOverlaps, float a_AttenuationRangeBuffer,
	bool a_bQueryAndPostEnvironmentSwitches)
{
	Initialize(a_Owner, a_AkEvent, a_bShouldRepeat, a_MaxRepetitions, a_MinTimeInterval, a_MaxTimeInterval, a_bBlockOverlaps,
		a_AttenuationRangeBuffer, a_bQueryAndPostEnvironmentSwitches);

	CallbackMask = a_CallbackMask;
	AkPostEventCallback = a_AkPostEventCallback;
	OneShotPostedCallback = a_OneShotPostedCallback;
	RepeatingOneShotEndedCallback = a_RepeatingOneShotEndedCallback;

	bPostAkPostEventCallbackOnEndOfEvent = CallbackMask % 2 == 1;
}

void URepeatingOneShot::Initialize(USoundEmitterComponentBase* a_Owner, UAkAudioEvent* a_AkEvent, bool a_bShouldRepeat,
	int32 a_MaxRepetitions, float a_MinTimeInterval, float a_MaxTimeInterval, bool a_bBlockOverlaps, float a_AttenuationRangeBuffer,
	bool a_bQueryAndPostEnvironmentSwitches)
{
	OwningSoundEmitterComponent = a_Owner;
	RepeatableOneShot.AkEvent = a_AkEvent;
	RepeatableOneShot.bShouldRepeat = a_bShouldRepeat;
	RepetitionsLeft = a_MaxRepetitions;
	RepeatableOneShot.MinTimeInterval = a_MinTimeInterval;
	RepeatableOneShot.MaxTimeInterval = a_MaxTimeInterval;
	RepeatableOneShot.bBlockOverlaps = a_bBlockOverlaps;
	AttenuationRangeBuffer = a_AttenuationRangeBuffer;
	bQueryAndPostEnvironmentSwitches = a_bQueryAndPostEnvironmentSwitches;

	OwningSoundEmitterComponent->m_repeatingOneShots.Add(this);
}

void URepeatingOneShot::Initialize(USoundEmitterComponentBase* a_Owner, const FRepeatableOneShot& a_RepeatableOneShot,
	float a_AttenuationRangeBuffer, bool a_bQueryAndPostEnvironmentSwitches)
{
	OwningSoundEmitterComponent = a_Owner;
	RepeatableOneShot = a_RepeatableOneShot;
	AttenuationRangeBuffer = a_AttenuationRangeBuffer;
	bQueryAndPostEnvironmentSwitches = a_bQueryAndPostEnvironmentSwitches;

	OwningSoundEmitterComponent->m_repeatingOneShots.Add(this);
}

bool URepeatingOneShot::Play()
{
	if (IsValid(OwningSoundEmitterComponent) && IsValid(RepeatableOneShot.AkEvent))
	{
		static const FName funcOnAkPostEventCallback = FName(TEXT("OnAkPostEventCallback"));
		AkPostEventCallbackInternal.BindUFunction(this, funcOnAkPostEventCallback);

		OnStartRepeatingOneShot();
		PostRepeatingOneShotInternal();
		return true;
	}

	return false;
}

void URepeatingOneShot::Pause(bool bStopPlayingEventImmediately, int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve)
{
	bIsPaused = RepetitionsLeft != 0;
	GetWorld()->GetTimerManager().ClearTimer(m_nextPostAkEventTimeHandle);

	if (IsPlaying())
	{
		if (bStopPlayingEventImmediately)
		{
			OwningSoundEmitterComponent->StopOneShotByPlayingId(ActivePlayingID, TransitionDurationInMs, FadeCurve);
		}
	}
	else
	{
		OnPauseRepeatingOneShot();
	}
}

bool URepeatingOneShot::Resume()
{
	if (bIsPaused)
	{
		bIsPaused = false;
		OnResumeRepeatingOneShot();
		ScheduleNextRepeatingOneShot();

		return true;
	}

	return false;
}

bool URepeatingOneShot::End(bool bStopPlayingEventImmediately, int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve)
{
	if (!OwningSoundEmitterComponent->m_repeatingOneShots.Contains(this))
	{
		return false;
	}

	RepetitionsLeft = 0;
	GetWorld()->GetTimerManager().ClearTimer(m_nextPostAkEventTimeHandle);

	if (IsPlaying())
	{
		if (bStopPlayingEventImmediately)
		{
			OwningSoundEmitterComponent->StopOneShotByPlayingId(ActivePlayingID, TransitionDurationInMs, FadeCurve);
		}
	}
	else
	{
		OnEndRepeatingOneShot();
	}

	return true;
}

bool URepeatingOneShot::IsPlaying()
{
	return OwningSoundEmitterComponent->IsPlayingIdActive(RepeatableOneShot.AkEvent, ActivePlayingID);
}
#pragma endregion

#pragma region Internal - SceneComponent
USoundEmitterComponentBase::USoundEmitterComponentBase()
{
	SetIsReplicated(false);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

#if !UE_BUILD_SHIPPING
	m_debugOneShotEvents.Empty();
#endif
}

void USoundEmitterComponentBase::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == ENetMode::NM_DedicatedServer) {	return;	}

	UAudioSubsystem* audioSubsystem = UAudioSubsystem::Get(GetWorld());
	if (!IsValid(audioSubsystem))
	{
		WR_DBG_NET_FUNC(Error, "audio subsystem not found");
		return;
	}

	InitializeInitialSwitches();

	/*if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		m_listeners = AkAudioDevice->GetDefaultListeners();
	}*/

	GetListenerManager();

	if (IsUnregistrationForbidden())
	{
		CreateAkComponentIfNeeded();
	}

#if !UE_BUILD_SHIPPING
	//static const FName funcOnDebugDrawChanged = TEXT("OnDebugDrawChanged");
	USoundEmitterComponentBase::OnEmitterDebugDrawChanged.AddUObject(this, &USoundEmitterComponentBase::OnDebugDrawChanged);

	OnDebugDrawChanged();
#endif
}

void USoundEmitterComponentBase::InitializeInitialSwitches()
{
	m_activeSwitches.Empty(InitialSwitches.Num());

	for (UAkSwitchValue* switchValue : InitialSwitches)
	{
		m_activeSwitches.Add(switchValue->GetGroupID(), switchValue);

		if (IsValid(m_AkComp))
		{
			m_AkComp->SetSwitch(switchValue, FString(), FString());
		}
	}
}

void USoundEmitterComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
#if !UE_BUILD_SHIPPING
		USoundEmitterComponentBase::OnEmitterDebugDrawChanged.RemoveAll(this);
#endif

		GetListenerManager()->OnListenersUpdated.RemoveAll(this);
		GetListenerManager()->OnAllWorldListenersRemoved.RemoveAll(this);

		if (bStopWhenOwnerDestroyed)
		{
			if (IsValid(m_AkComp))
			{
				if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
				{
					AkAudioDevice->StopGameObject(m_AkComp);
				}

				DestroyAkComponent();
			}
		}

		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

		TSet<URepeatingOneShot*> toEnd = MoveTemp(m_repeatingOneShots);

		for (URepeatingOneShot* repeatingOneShot : toEnd)
		{
			repeatingOneShot->EndPlay();
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool USoundEmitterComponentBase::TryToStopTicking()
{
	if (!s_debugDraw)
	{
		SetComponentTickEnabled(false);
		return true;
	}

	return false;
}

USoundListenerManager* USoundEmitterComponentBase::GetListenerManager()
{
	if (!IsValid(s_listenerManager))
	{
		s_listenerManager = UAudioSubsystem::Get(GetWorld())->GetListenerManager();

		if (!s_listenerManager->OnListenersUpdated.IsBoundToObject(this))
		{
			//static const FName funcOnListenersUpdated{ TEXT("OnListenersUpdated") };
			s_listenerManager->OnListenersUpdated.AddUObject(this, &USoundEmitterComponentBase::OnListenersUpdated);
		}

		if (!s_listenerManager->OnAllWorldListenersRemoved.IsBoundToObject(this))
		{
			//static const FName funcOnAllWorldListenersRemoved{ TEXT("OnAllWorldListenersRemoved") };
			s_listenerManager->OnAllWorldListenersRemoved.AddUObject(this, &USoundEmitterComponentBase::OnAllWorldListenersRemoved);
		}
	}

	return s_listenerManager;
}

void USoundEmitterComponentBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	if (s_debugDraw)
	{
		DebugDrawOnTick(GetWorld());
	}
#endif
}

bool USoundEmitterComponentBase::IsInListenerRange(UAkAudioEvent* AkEvent, float RangeBuffer)
{
#if WITH_EDITOR // play sounds in (animation sequence) editor
	if (!GWorld->HasBegunPlay()) { return true; }
#endif

	if (!bUseDistanceCulling || AkEvent->MaxAttenuationRadius == 0.f)
	{
		return true;
	}

	if (AttenuationScalingFactor > 0.f && IsValid(AkEvent))
	{
		const float cullRange = AkEvent->MaxAttenuationRadius * AttenuationScalingFactor + innerRadius + RangeBuffer;
		const float cullRangeSquared = cullRange * cullRange;

		// world listeners
		for (TWeakObjectPtr<UWorldSoundListener> worldListener : GetWorldListeners())
		{
			if (FVector::DistSquared(GetCullingLocation(), worldListener->GetComponentLocation()) < cullRangeSquared)
			{
				return true;
			}
		}

		// default listeners
		if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
		{
			for (TWeakObjectPtr<UAkComponent> defaultListener : AkAudioDevice->GetDefaultListeners())
			{
				if (defaultListener.Get() == AkAudioDevice->GetSpatialAudioListener())
				{
					continue;
				}

				if (FVector::DistSquared(GetCullingLocation(), defaultListener->GetComponentLocation()) < cullRangeSquared)
				{
					return true;
				}
			}
		}

		// spatial audio listener
		return GetListenerManager()->GetSquaredDistanceToDistanceProbe(GetCullingLocation()) < cullRangeSquared;
	}

	return false;
}
TSet<TWeakObjectPtr<UWorldSoundListener>> USoundEmitterComponentBase::GetWorldListeners()
{
	WR_ASSERT(IsValid(GetListenerManager()), "no valid listener manager!")
	return s_listenerManager->GetWorldListeners();
}
void USoundEmitterComponentBase::InitializeListeners()
{
	const TSet<TWeakObjectPtr<UWorldSoundListener>> worldListeners = GetWorldListeners();
	if (worldListeners.IsEmpty()) { return; }

	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!AkAudioDevice || !SoundEngine)) { return; }

	const UAkComponentSet defaultListeners = AkAudioDevice->GetDefaultListeners();
	const int numDefaultListeners = defaultListeners.Num();
	const int numWorldListeners = worldListeners.Num();
	const int numListeners = numDefaultListeners + numWorldListeners;

	auto pListenerIds = (AkGameObjectID*)alloca(numListeners * sizeof(AkGameObjectID));

	for (int i = 0; i < numDefaultListeners; i++)
	{
		pListenerIds[i] = defaultListeners.Array()[i]->GetAkGameObjectID();
	}

	for (int i = 0; i < numWorldListeners; i++)
	{
		pListenerIds[i + numWorldListeners] = worldListeners.Array()[i]->GetAkGameObjectID();
	}

	SoundEngine->SetListeners(m_AkComp->GetAkGameObjectID(), pListenerIds, numListeners);
}
#pragma endregion

#pragma region Internal - AkComponent Registration
bool USoundEmitterComponentBase::CreateAkComponentIfNeeded()
{
	if (GetNetMode() == ENetMode::NM_DedicatedServer || (FAkAudioDevice::Get == nullptr))
	{
		return false;
	}

	UWorld* world = GetWorld();

	if (IsValid(world))
	{
		world->GetTimerManager().ClearTimer(m_timerNextUnregistration);
		m_isPendingUnregistration = false;
	}

	if (IsValid(m_AkComp))
	{
		return false;
	}

	m_AkComp = NewObject<UAkComponent>(this, *GetName());
	check(m_AkComp);

	if (IsValid(world))
	{
		m_AkComp->RegisterComponentWithWorld(world);
	}
	else
	{
		m_AkComp->RegisterComponent();
	}

	m_AkComp->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (!IsUnregistrationForbidden() && !m_AkComp->OnAllEventsEnded.IsBoundToObject(this))
	{
		m_AkComp->OnAllEventsEnded.BindUObject(this, &USoundEmitterComponentBase::ScheduleUnregistration);
	}

	InitializeAkComponent();

	//*** tick for visual debugging ***/
#if !UE_BUILD_SHIPPING
	OnDebugDrawChanged();
#endif

	return true;
}

bool USoundEmitterComponentBase::DestroyAkComponent()
{
	if (!IsValid(m_AkComp))
	{
		return false;
	}

	if (m_AkComp->OnAllEventsEnded.IsBoundToObject(this))
	{
		m_AkComp->OnAllEventsEnded.Unbind();
	}

	m_AkComp->DestroyComponent();
	m_AkComp = nullptr;

	if (s_debugToConsole)
	{
		WR_DBG_FUNC(Log, "AkComponent destroyed");
	}

#if !UE_BUILD_SHIPPING
	//SetComponentTickEnabled(false);
	OnDebugDrawChanged();
#endif

	return true;
}

void USoundEmitterComponentBase::ScheduleUnregistration(const UAkComponent* AkComponent)
{
	WR_ASSERT(AkComponent == m_AkComp, "trying to unregister an AkComponent that was not registered");

#if !UE_BUILD_SHIPPING
	m_debugOneShotEvents.Empty();
#endif

	if (m_isPendingUnregistration || IsUnregistrationForbidden())
	{
		return;
	}

	if (UnregistrationCooldown <= 0.f)
	{
		UnregisterEmitterIfInactive();
	}
	else
	{
		FTimerDelegate timerDelegate;
		GetWorld()->GetTimerManager().SetTimer(
			m_timerNextUnregistration, this, &USoundEmitterComponentBase::CallUnregisterEmitterIfInactive, UnregistrationCooldown, false);

		m_isPendingUnregistration = true;
	}
}

bool USoundEmitterComponentBase::UnregisterEmitterIfInactive()
{
	if (IsUnregistrationForbidden() || !IsValid(m_AkComp) || m_AkComp->HasActiveEvents())
	{
		return false;
	}

	DestroyAkComponent();
	m_isPendingUnregistration = false;
	return true;
}

void USoundEmitterComponentBase::InitializeAkComponent()
{
	m_AkComp->SetAutoDestroy(false);
	m_AkComp->SetStopWhenOwnerDestroyed(bStopWhenOwnerDestroyed);
/*
#if !WITH_EDITOR
	m_AkComp->SetListeners(UAudioUtils::WeakObjectPtrSetToArray(m_listeners));
#endif*/


	if (AttenuationScalingFactor >= 0.f && AttenuationScalingFactor != 1.0f) // && AttenuationScalingFactor > 0.f)
	{
		m_AkComp->SetAttenuationScalingFactor(AttenuationScalingFactor);
	}

	m_AkComp->OcclusionCollisionChannel = OcclusionCollisionChannel;
	m_AkComp->OcclusionRefreshInterval = OcclusionRefreshInterval;
	m_AkComp->bUseReverbVolumes = bUseReverbVolumes;
	m_AkComp->SetEnableSpotReflectors(bEnableSpotReflectors);
	m_AkComp->SetGameObjectRadius(outerRadius, innerRadius);

	m_AkComp->DrawFirstOrderReflections = DrawFirstOrderReflections;
	m_AkComp->DrawSecondOrderReflections = DrawSecondOrderReflections;
	m_AkComp->DrawHigherOrderReflections = DrawHigherOrderReflections;
	m_AkComp->DrawDiffraction = DrawDiffraction;

	//InitializeListeners();
	UWorldSoundListener::UpdateSoundEmitterSendLevels(m_AkComp);

	if (s_debugToConsole)
	{
		WR_DBG_FUNC(Log, "AkComponent initialized");
	}

	InitializeGameSynchs();
}

void USoundEmitterComponentBase::InitializeGameSynchs()
{
	WR_ASSERT(IsValid(m_AkComp), "trying to initialize gamesynchs without a valid AkComponent");

	/*for (const UAkSwitchValue* Switch : PersistentSwitches)
	{
		m_AkComp->SetSwitch(Switch, FString(), FString());

		if (s_debugToConsole && Private_SoundEmitterComponentBase::bLogGameSynchs)
		{
			WR_DBG_FUNC(Log, "initializing persistent Switch: %s ", *Switch->GetFullName());
		}
	}*/

	for (const TPair<AkSwitchGroupID, UAkSwitchValue*>& activeSwitch : m_activeSwitches)
	{
		m_AkComp->SetSwitch(activeSwitch.Value, FString(), FString());

		if (s_debugToConsole && Private_SoundEmitterComponentBase::bLogGameSynchs)
		{
			WR_DBG_FUNC(Log, "initializing Switch: %s ", *activeSwitch.Value->GetFullName());
		}
	}

	for (const FActiveRtpc& Rtpc : m_activeRtpcs)
	{
		{
			m_AkComp->SetRTPCValue(Rtpc.Rtpc, Rtpc.Value, 0, FString());
		}

		if (s_debugToConsole && Private_SoundEmitterComponentBase::bLogGameSynchs)
		{
			WR_DBG_FUNC(Log, "initializing Rtpc: %s: %f", *Rtpc.Rtpc->GetFullName(), Rtpc.Value);
		}
	}
}

void USoundEmitterComponentBase::OnListenersUpdated()
{
	if (IsValid(m_AkComp))
	{
		UWorldSoundListener::UpdateSoundEmitterSendLevels(m_AkComp);
		//GetListenerManager()->UpdateListeners(m_AkComp);
	}
}

void USoundEmitterComponentBase::OnAllWorldListenersRemoved()
{
	if (IsValid(m_AkComp))
	{
		IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
		if (UNLIKELY(SoundEngine == nullptr)) { return; }

		SoundEngine->ResetListenersToDefault(m_AkComp->GetAkGameObjectID());
	}
}
#pragma endregion


#pragma region Internal - Debug
void USoundEmitterComponentBase::OnDebugDrawChanged()
{
	SetComponentTickEnabled(s_debugDraw && IsValid(m_AkComp));
}

#if !UE_BUILD_SHIPPING
void USoundEmitterComponentBase::DebugDrawOnTick(UWorld* World)
{
	if (!IsValid(World)) { return; }

	const FThemeSoundEmitters debugTheme = IsValid(GetDefault<UWwiserRThemeSettings>()) ?
		GetDefault<UWwiserRThemeSettings>()->ThemeSoundEmitters : FThemeSoundEmitters{};

	const FColor dbgActiveEmitterColor = debugTheme.ActiveSoundEmitterColor;
	const FColor dbgActiveEmitterTextColor = debugTheme.ActiveSoundEmitterTextColor;
	const float dbgFontScale = debugTheme.FontScale;
	const float dbgEmitterScale = debugTheme.EmitterScale;
	const float dbgGizmoSize = debugTheme.GizmoSize;

	const float dbgDistSquared = GetListenerManager()->GetSquaredDistanceToDistanceProbe(GetComponentLocation());
	const float dbgDistInMeters = FMath::Sqrt(dbgDistSquared) / 100;
	const float scaleFactor = FMath::Pow(25.f / FMath::Clamp(dbgDistInMeters, 10.f, 100.f), .2f);

	bool bDrawText = false;
	FString msgDebugText;

	// add text: distance
	if (IsValid(m_AkComp) && ShouldDrawDebug(Private_SoundEmitterComponentBase::fDebugDrawDistance, dbgDistSquared))
	{
		msgDebugText.Append(FString::Printf(TEXT("%.2f m\n"), dbgDistInMeters));
		bDrawText = true;
	}

	// add text: persistent msg
	if (!msgDebugText.IsEmpty())
	{
		msgDebugText.Append(m_msgEmitterDebugPersistent);
		bDrawText = true;
	}

	// add text : child classes
	if (!m_msgEmitterDebug.IsEmpty())
	{
		msgDebugText.Append(m_msgEmitterDebug);
		bDrawText = true;
	}

	// draw registered emitters
	if (ShouldDrawDebug(Private_SoundEmitterComponentBase::fDebugDrawRegistration, dbgDistSquared))
	{
		if (IsValid(m_AkComp) && m_AkComp->HasBeenRegisteredWithWwise())
		{
			const FVector componentLocation = GetComponentLocation();
			const float emitterScale = 12.f * dbgEmitterScale / scaleFactor;
			const float emitterDrawRadius = FMath::Max(outerRadius, emitterScale);

			DrawDebugSphere(World, componentLocation, emitterDrawRadius, 8, dbgActiveEmitterColor);

			if (ShouldDrawDebug(Private_SoundEmitterComponentBase::fDebugDrawGizmos, dbgDistSquared))
			{
				const float gizmoSize = (dbgGizmoSize + m_AkComp->outerRadius) * Private_SoundEmitterComponentBase::fDebugDrawGizmoScale;
				UAudioUtils::DrawDebugGizmo(World, componentLocation, GetComponentRotation(), gizmoSize);
			}
		}
	}

	// draw event names
	if (ShouldDrawDebug(Private_SoundEmitterComponentBase::fDebugDrawEvents, dbgDistSquared))
	{
		// one shots
		if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
		{
			TSet<FPlayingOneShot> toRemove{};

			for (const FPlayingOneShot& playingOneShot : m_debugOneShotEvents)
			{
				if (!AkAudioDevice->IsPlayingIDActive(playingOneShot.AkEvent->GetShortID(), (uint32)playingOneShot.PlayingID))
				{
					toRemove.Add(playingOneShot);
				}
			}

			for (const FPlayingOneShot& playingOneShot : toRemove)
			{
				m_debugOneShotEvents.Remove(playingOneShot);
			}

			if (!m_debugOneShotEvents.IsEmpty())
			{
				for (const FPlayingOneShot& playingOneShot : m_debugOneShotEvents)
				{
					msgDebugText.Append(playingOneShot.AkEvent->GetName().Append("\n"));
				}

				bDrawText = true;
			}
		}
	}

	// draw distance and events text
	if (bDrawText)
	{
		const float fontSize = dbgFontScale * scaleFactor;
		DrawDebugString(World, GetComponentLocation(), msgDebugText, nullptr, dbgActiveEmitterTextColor, 0.f, false, fontSize);
	}

	m_msgEmitterDebugPersistent.Empty();
	m_msgEmitterDebug.Empty();
}

bool USoundEmitterComponentBase::ShouldDrawDebug(float InRange, float DistSquared)
{
	//if (m_listeners.IsEmpty()) { return false; }

	return (InRange == 1.f || InRange * InRange * 10000.f > DistSquared);
}
#endif
#pragma endregion

#pragma region Class Properties
void USoundEmitterComponentBase::SetNeverUnregister(bool bMustNeverUnregister)
{
	if (bNeverUnregister == bMustNeverUnregister) {	return;	}

	bNeverUnregister = bMustNeverUnregister;
	UpdateNeverUnregister();
}

void USoundEmitterComponentBase::UpdateNeverUnregister()
{
	if (IsUnregistrationForbidden())
	{
		if (IsValid(m_AkComp))
		{
			if (m_AkComp->OnAllEventsEnded.IsBoundToObject(this))
			{
				m_AkComp->OnAllEventsEnded.Unbind();
			}
		}
		else
		{
			CreateAkComponentIfNeeded();
		}
	}
	else
	{
		if (IsValid(m_AkComp) && !m_AkComp->OnAllEventsEnded.IsBoundToObject(this))
		{
			{
				m_AkComp->OnAllEventsEnded.BindUObject(this, &USoundEmitterComponentBase::ScheduleUnregistration);
			}

			UnregisterEmitterIfInactive();
		}
	}
}

void USoundEmitterComponentBase::SetStopWhenOwnerDestroyed(bool bShouldStopWhenOwnerDestroyed)
{
	bStopWhenOwnerDestroyed = bShouldStopWhenOwnerDestroyed;

	if (IsValid(m_AkComp))
	{
		m_AkComp->SetStopWhenOwnerDestroyed(bStopWhenOwnerDestroyed);
	}
}

void USoundEmitterComponentBase::SetAttenuationScalingFactor(float Value)
{
	if (s_debugToConsole)
	{
		WR_DBG_FUNC(Log, "AttenuationScalingFactor set to %f", Value);
	}

	if (AttenuationScalingFactor != Value)
	{
		AttenuationScalingFactor = Value;

		if (IsValid(m_AkComp) && AttenuationScalingFactor > 0.f)
		{
			m_AkComp->SetAttenuationScalingFactor(Value);
		}
	}
}

ECollisionChannel USoundEmitterComponentBase::GetOcclusionCollisionChannel() const
{
	return UAkSettings::ConvertOcclusionCollisionChannel(OcclusionCollisionChannel.GetValue());
}

void USoundEmitterComponentBase::SetGameObjectRadius(float Outer, float Inner)
{
	outerRadius = Outer;
	innerRadius = Inner;

	if (IsValid(m_AkComp))
	{
		m_AkComp->SetGameObjectRadius(Outer, Inner);
	}
}

void USoundEmitterComponentBase::SetEnableSpotReflectors(bool bEnable)
{
	if (bEnableSpotReflectors != bEnable)
	{
		bEnableSpotReflectors = bEnable;

		if (IsValid(m_AkComp))
		{
			m_AkComp->EnableSpotReflectors = bEnable;
			AAkSpotReflector::UpdateSpotReflectors(m_AkComp);
		}
	}
}
#pragma endregion

#pragma region Public Functions - 3D Audio Emitter
/*void USoundEmitterComponentBase::SetListeners(const UAkComponentSet& Listeners)
{
	if (IsValid(m_AkComp))
	{
		m_AkComp->SetListeners(UAudioUtils::WeakObjectPtrSetToArray(Listeners));
	}
}*/

void USoundEmitterComponentBase::QueryAndPostEnvironmentSwitches()
{
	if (!IsValid(m_AkComp))
	{
		return;
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		TArray<UAkRoomComponent*> roomComponents;
		roomComponents = AkAudioDevice->FindRoomComponentsAtLocation(GetComponentLocation(), GetWorld());

		if (roomComponents.Num() > 0)
		{
			/* TODO
			if (ASpatialAudioVolume* SpatialAudioVolume = Cast<ASpatialAudioVolume>(roomComponents[0]->GetOwner()))
			{
				if (const UAkSwitchValue* environmentSwitch = SpatialAudioVolume->EnvironmentSwitch)
				{
					if (s_environmentSwitchDefaultGroupID == AkSwitchGroupID())
					{
						s_environmentSwitchDefaultGroupID = environmentSwitch->GetGroupID();
					}
					else if (environmentSwitch->GetGroupID() != s_environmentSwitchDefaultGroupID)
					{
						WR_DBG_FUNC(Warning,
							"More than one EnvironmentSwitch switch group used. Make sure the EnvironmentSwitch switch group is always the same.");
					}

					m_AkComp->SetSwitch(environmentSwitch, FString(), FString());

					if (s_bDebugToConsole)
					{
						WR_DBG_FUNC(Log, "EnvironmentSwitch %s set", *environmentSwitch->GetName());
					}

					return;
				}
			}*/
		}

		if (s_environmentSwitchDefaultGroupID != AkSwitchGroupID())
		{
			AkAudioDevice->SetSwitch(s_environmentSwitchDefaultGroupID, AkSwitchStateID(), m_AkComp);

			if (s_debugToConsole && Private_SoundEmitterComponentBase::bLogGameSynchs)
			{
				WR_DBG_FUNC(Log, "default EnvironmentSwitch set");
			}
		}
	}
}

int32 USoundEmitterComponentBase::PostOneShot(UAkAudioEvent* AkEvent
	, float ActivationRangeBuffer, 	bool bQueryAndPostEnvironmentSwitches, bool IgnoreDistanceCulling)
{
	FOnAkPostEventCallback DummyCallback;
	return PostOneShot(AkEvent, 0, DummyCallback, ActivationRangeBuffer, bQueryAndPostEnvironmentSwitches, IgnoreDistanceCulling);
}

int32 USoundEmitterComponentBase::PostOneShot(UAkAudioEvent* AkEvent
	, const int32 CallbackMask,	const FOnAkPostEventCallback& PostEventCallback,
	float ActivationRangeBuffer, bool bQueryAndPostEnvironmentSwitches, bool IgnoreDistanceCulling)
{
	if (!IsValid(AkEvent))
	{
		WR_DBG(Warning, "missing Wwise event in [PostOneShot] node");
		return AK_INVALID_PLAYING_ID;
	}

	const bool bShouldPost = (IsInListenerRange(AkEvent, ActivationRangeBuffer) || IgnoreDistanceCulling) && !m_isMuted;

	if (!bShouldPost)
	{
		return AK_INVALID_PLAYING_ID;
	}

	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	CreateAkComponentIfNeeded();

	if (bQueryAndPostEnvironmentSwitches)
	{
		QueryAndPostEnvironmentSwitches();
	}

	playingID = m_AkComp->PostAkEvent(AkEvent, CallbackMask, PostEventCallback);

#if !UE_BUILD_SHIPPING
	if (s_debugToConsole && s_logEvents)
	{
		WR_DBG_FUNC(Log, "OneShot posted: %s", *AkEvent->GetName());
	}

	if (s_debugDraw && Private_SoundEmitterComponentBase::fDebugDrawEvents > 0.f)
	{
		m_debugOneShotEvents.Add(FPlayingOneShot(AkEvent, playingID));
	}
#endif

	return playingID;
}

int32 USoundEmitterComponentBase::PostOneShotAndWaitForEnd(UAkAudioEvent* AkEvent, FLatentActionInfo LatentInfo,
	float ActivationRangeBuffer, bool bQueryAndPostEnvironmentSwitches, bool IgnoreDistanceCulling)
{
	if (!IsValid(AkEvent))
	{
		WR_DBG(Warning, "missing Wwise event in [PostOneShotAndWaitForEnd] node");
		return AK_INVALID_PLAYING_ID;
	}

	int32 playingID = AK_INVALID_PLAYING_ID;

	bool bShouldPost = (IsInListenerRange(AkEvent, ActivationRangeBuffer) || IgnoreDistanceCulling) && !m_isMuted;
	if (!bShouldPost)
	{
		return AK_INVALID_PLAYING_ID;
	}

	CreateAkComponentIfNeeded();

	if (bQueryAndPostEnvironmentSwitches)
	{
		QueryAndPostEnvironmentSwitches();
	}

	playingID = m_AkComp->PostAkEventAndWaitForEnd(AkEvent, LatentInfo);

#if !UE_BUILD_SHIPPING
	if (s_debugToConsole && s_logEvents)
	{
		WR_DBG_FUNC(Log, "OneShot posted (with wait for end): %s", *AkEvent->GetName());
	}

	if (s_debugDraw && Private_SoundEmitterComponentBase::fDebugDrawEvents > 0.f)
	{
		m_debugOneShotEvents.Add(FPlayingOneShot(AkEvent, playingID));
	}
#endif

	return playingID;
}

URepeatingOneShot* USoundEmitterComponentBase::PostRepeatingOneShot(UAkAudioEvent* AkEvent, float MinTimeInterval,
	float MaxTimeInterval, const int32 CallbackMask, const FOnAkPostEventCallback& AkPostEventCallback,
	const FOnOneShotCallback& OneShotPostedCallback, const FOnRepeatingOneShotEndedCallback& RepeatingOneShotEndedCallback,
	int32 MaxRepetitions, bool bBlockOverlaps, float ActivationRangeBuffer, bool bQueryAndPostEnvironmentSwitches)
{
	if (!IsValid(AkEvent))
	{
		WR_DBG_FUNC(Warning, "No Wwise event assigned");
		return nullptr;
	}

	const bool bShouldRepeat = MaxTimeInterval > 0.f;

	URepeatingOneShot* repeatingOneShot = NewObject<URepeatingOneShot>(this);
	repeatingOneShot->Initialize(this, AkEvent, CallbackMask, AkPostEventCallback, OneShotPostedCallback, RepeatingOneShotEndedCallback,
		bShouldRepeat, MaxRepetitions, MinTimeInterval, MaxTimeInterval, bBlockOverlaps, ActivationRangeBuffer,
		bQueryAndPostEnvironmentSwitches);
	repeatingOneShot->Play();

	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot && IsValid(repeatingOneShot))
	{
		WR_DBG_FUNC(Log, "Repeating OneShot posted: %s", *repeatingOneShot->RepeatableOneShot.AkEvent->GetName());
	}

	return repeatingOneShot;
}

URepeatingOneShot* USoundEmitterComponentBase::PostRepeatableOneShot(const FRepeatableOneShot& RepeatableOneShot,
	const int32 CallbackMask, const FOnAkPostEventCallback& AkPostEventCallback, const FOnOneShotCallback& OneShotPostedCallback,
	const FOnRepeatingOneShotEndedCallback& RepeatingOneShotEndedCallback, float ActivationRangeBuffer,
	bool bQueryAndPostEnvironmentSwitches)
{
	if (!IsValid(RepeatableOneShot.AkEvent))
	{
		WR_DBG_FUNC(Warning, "No Wwise event assigned in RepeatableOneShot");
		return nullptr;
	}

	const bool bShouldRepeat = RepeatableOneShot.bShouldRepeat && RepeatableOneShot.MaxTimeInterval > 0.f;

	URepeatingOneShot* repeatingOneShot = NewObject<URepeatingOneShot>(this);
	repeatingOneShot->Initialize(this, RepeatableOneShot.AkEvent, CallbackMask, AkPostEventCallback, OneShotPostedCallback,
		RepeatingOneShotEndedCallback, bShouldRepeat, RepeatableOneShot.MaxRepetitions, RepeatableOneShot.MinTimeInterval,
		RepeatableOneShot.MaxTimeInterval, RepeatableOneShot.bBlockOverlaps, ActivationRangeBuffer, bQueryAndPostEnvironmentSwitches);
	repeatingOneShot->Play();

	if (Private_SoundEmitterComponentBase::bLogRepeatingOneShot && IsValid(repeatingOneShot))
	{
		WR_DBG_FUNC(Log, "Repeatable OneShot posted: %s", *repeatingOneShot->RepeatableOneShot.AkEvent->GetName());
	}

	return repeatingOneShot;
}

void USoundEmitterComponentBase::PauseRepeatingOneShot(URepeatingOneShot* RepeatingOneShot, bool bStopPlayingEventImmediately,
	int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve)
{
	RepeatingOneShot->Pause(bStopPlayingEventImmediately, TransitionDurationInMs, FadeCurve);
}

bool USoundEmitterComponentBase::ResumeRepeatingOneShot(URepeatingOneShot* RepeatingOneShot)
{
	return RepeatingOneShot->Resume();
}

bool USoundEmitterComponentBase::EndRepeatingOneShot(URepeatingOneShot* RepeatingOneShot, bool bStopPlayingEventImmediately,
	int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve)
{
	if (IsValid(RepeatingOneShot))
	{
		return RepeatingOneShot->End(bStopPlayingEventImmediately, TransitionDurationInMs, FadeCurve);
	}

	return false;
}

bool USoundEmitterComponentBase::IsRepeatingOneShotPlaying(URepeatingOneShot* RepeatingOneShot)
{
	return IsValid(RepeatingOneShot) && (RepeatingOneShot->IsPlaying());
}

void USoundEmitterComponentBase::StopOneShotByPlayingId(int32 PlayingID, int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve) const
{
	// this could be a static function, but for the user it's neater like this.

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->StopPlayingID(PlayingID, TransitionDurationInMs, (AkCurveInterpolation)FadeCurve);
	}
}

bool USoundEmitterComponentBase::SeekOnEvent(UAkAudioEvent* AkAudioEvent, int32 SeekPositionMs, bool bSeekToNearestMarker, int32 PlayingID)
{
	if (!IsValid(m_AkComp) || !IsValid(AkAudioEvent))
	{
		return false;
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		const AkUInt32 eventShortID = AkAudioEvent->GetShortID();
		const AKRESULT result = AkAudioDevice->SeekOnEvent(
			eventShortID, m_AkComp, SeekPositionMs, bSeekToNearestMarker, (AkPlayingID)PlayingID);

		if (s_debugToConsole && s_logEvents)
		{
			if (result == AKRESULT::AK_Success)
			{
				WR_DBG_FUNC(Log, "%s: seek to %i ms", *AkAudioEvent->GetName(), SeekPositionMs);
			}
			else
			{
				WR_DBG_FUNC(Warning, "%s: seek to %i ms FAILED", *AkAudioEvent->GetName(), SeekPositionMs);
			}
		}

		return result == AKRESULT::AK_Success;
	}

	return false;
}

void USoundEmitterComponentBase::BreakEvent(int32 PlayingID) const
{
	// this could be a static function, but for the user it's neater like this.

	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(SoundEngine == nullptr)) { return; }
	SoundEngine->ExecuteActionOnPlayingID(AK::SoundEngine::AkActionOnEventType_Break, PlayingID);
}

void USoundEmitterComponentBase::PostTrigger(UAkTrigger* AkTrigger)
{
	if (AkTrigger == nullptr)
	{
		WR_DBG(Warning, "missing Wwise trigger in [PostTrigger] node");
		return;
	}

	CreateAkComponentIfNeeded();
	m_AkComp->PostTrigger(AkTrigger, FString());
}

/*void USoundEmitterComponentBase::SetPersistentSwitch(UAkSwitchValue* AkSwitchValue)
{
	if (!IsValid(AkSwitchValue))
	{
		WR_DBG(Warning, "missing Wwise switch in [SetPersistentSwitch] node");
		return;
	}

	for (UAkSwitchValue*& switchValue : PersistentSwitches)
	{
		// switch group has already been posted to this emitter
		if (switchValue->GetGroupID() == AkSwitchValue->GetGroupID())
		{
			if (switchValue != AkSwitchValue)
			{
				if (IsValid(m_AkComp))
				{
					m_AkComp->SetSwitch(AkSwitchValue, FString(), FString());
				}

				switchValue = AkSwitchValue;
			}

			return;
		}
	}

	// new switch group on this emitter
	PersistentSwitches.Add(AkSwitchValue);

	if (IsValid(m_AkComp))
	{
		m_AkComp->SetSwitch(AkSwitchValue, FString(), FString());
	}
}

void USoundEmitterComponentBase::SetPersistentSwitches(TArray<UAkSwitchValue*> AkSwitchValues)
{
	for (UAkSwitchValue* switchValue : AkSwitchValues)
	{
		SetPersistentSwitch(switchValue);
	}
}*/

void USoundEmitterComponentBase::SetSwitch(UAkSwitchValue* AkSwitchValue)
{
	if (!IsValid(AkSwitchValue))
	{
		WR_DBG(Warning, "missing Wwise switch in [SetSwitch] node");
		return;
	}

	for (TPair <AkSwitchGroupID, UAkSwitchValue*>& activeSwitch : m_activeSwitches)
	{
		// switch group has already been posted to this emitter
		if (activeSwitch.Value->GetGroupID() == AkSwitchValue->GetGroupID())
		{
			if (activeSwitch.Value != AkSwitchValue)
			{
				if (IsValid(m_AkComp))
				{
					m_AkComp->SetSwitch(AkSwitchValue, FString(), FString());
				}

				activeSwitch.Value = AkSwitchValue;
			}

			return;
		}
	}

	// new switch group on this emitter
	m_activeSwitches.Add(AkSwitchValue->GetGroupID(), AkSwitchValue);

	if (IsValid(m_AkComp))
	{
		m_AkComp->SetSwitch(AkSwitchValue, FString(), FString());
	}
}

void USoundEmitterComponentBase::SetSwitches(const TArray<UAkSwitchValue*>& AkSwitchValues)
{
	for (UAkSwitchValue* AkSwitchValue : AkSwitchValues)
	{
		SetSwitch(AkSwitchValue);
	}
}

void USoundEmitterComponentBase::ResetSwitchGroup(UAkGroupValue* AkSwitchGroup)
{
	if (!IsValid(AkSwitchGroup))
	{
		WR_DBG_FUNC(Warning, "no valid AkSwitchGroup");
		return;
	}

	const AkSwitchGroupID switchGroupID = AkSwitchGroup->GetGroupID();

	for (const TPair<AkSwitchGroupID, UAkSwitchValue*>& activeSwitch : m_activeSwitches)
	{
		if (activeSwitch.Key == switchGroupID)
		{
			if (IsValid(m_AkComp))
			{
				if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
				{
					AkAudioDevice->SetSwitch(switchGroupID, AkSwitchStateID(), m_AkComp);
				}
			}

			m_activeSwitches.Remove(activeSwitch.Key);
			return;
		}
	}
}

/*void USoundEmitterComponentBase::ResetSwitchGroup(TArray<UAkSwitchValue*> Switches, UAkGroupValue* AkSwitchGroup)
{
	if (Switches.IsEmpty() || !IsValid(AkSwitchGroup))
	{
		return;
	}

	const AkSwitchGroupID switchGroupID = AkSwitchGroup->GetGroupID();

	for (int i = Switches.Num() - 1; i >= 0; i--)
	{
		if (Switches[i]->GetGroupID() == switchGroupID)
		{
			if (IsValid(m_AkComp))
			{
				if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
				{
					AkAudioDevice->SetSwitch(switchGroupID, AkSwitchStateID(), m_AkComp);
				}
			}

			Switches.RemoveAtSwap(i);
			return;
		}
	}
}*/

void USoundEmitterComponentBase::ResetAllSwitchGroups()
{
	if (IsValid(m_AkComp))
	{
		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		if (UNLIKELY(!AkAudioDevice)) { return; }

		for (const TPair<AkSwitchGroupID, UAkSwitchValue*> activeSwitch : m_activeSwitches)
		{
			if (!InitialSwitches.Contains(activeSwitch.Value))
			{
				AkAudioDevice->SetSwitch(activeSwitch.Key, AkSwitchStateID(), m_AkComp);
			}
		}
	}

	InitializeInitialSwitches();
}

void USoundEmitterComponentBase::SetRtpc(UAkRtpc* AkRtpc, float Value, int32 InterpolationTimeMs, float Epsilon)
{
	if (!IsValid(AkRtpc))
	{
		WR_DBG(Warning, "missing Rtpc in [SetRtpc] node");
		return;
	}

	bool alreadyPosted = false;
	bool epsilonExceeded = false;

	for (FActiveRtpc& ActiveRtpc : m_activeRtpcs)
	{
		// Rtpc has already been posted on this Emitter
		if (ActiveRtpc.Rtpc == AkRtpc)
		{
			// Rtpc value has changed more than Epsilon
			if (FMath::Abs(Value - ActiveRtpc.Value) > Epsilon)
			{
				ActiveRtpc.Value = Value;
				epsilonExceeded = true;
			}

			alreadyPosted = true;
			break;
		}
	}

	if (!alreadyPosted)
	{
		m_activeRtpcs.Add(FActiveRtpc{ AkRtpc, Value });
	}

	if ((!alreadyPosted || epsilonExceeded) && IsValid(m_AkComp))
	{
		m_AkComp->SetRTPCValue(AkRtpc, Value, InterpolationTimeMs, FString());
	}
}

void USoundEmitterComponentBase::ResetRtpcValue(UAkRtpc* AkRtpc)
{
	for (const FActiveRtpc& activeRtpc : m_activeRtpcs)
	{
		if (activeRtpc.Rtpc == AkRtpc)
		{
			m_activeRtpcs.Remove(activeRtpc);
			break;
		}
	}

	IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) { return; }

	SoundEngine->ResetRTPCValue(AkRtpc->GetShortID(), m_AkComp->GetAkGameObjectID());
}

void USoundEmitterComponentBase::ResetAllRtpcValues()
{
	if (IsValid(m_AkComp))
	{
		IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get();
		if (UNLIKELY(!SoundEngine)) { return; }

		for (const FActiveRtpc& activeRtpc : m_activeRtpcs)
		{
			SoundEngine->ResetRTPCValue(activeRtpc.Rtpc->GetShortID(), m_AkComp->GetAkGameObjectID());
		}
	}

	m_activeRtpcs.Empty();
}

void USoundEmitterComponentBase::MuteEmitter(bool bMute)
{
	if (bMute == m_isMuted)
	{
		return;
	}

	m_isMuted = bMute;

	if (bMute)
	{
		if (IsValid(m_AkComp))
		{
			if (m_AkComp->HasActiveEvents())
			{
				if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
				{
					AkAudioDevice->StopGameObject(m_AkComp);
				}
			}
		}
	}
}

void USoundEmitterComponentBase::StopAll()
{
	m_repeatingOneShots.Empty();
	if (!IsValid(m_AkComp)) { return; }

	if (IWwiseSoundEngineAPI* SoundEngine = IWwiseSoundEngineAPI::Get())
	{
		SoundEngine->StopAll(m_AkComp->GetAkGameObjectID());
	}
}

bool USoundEmitterComponentBase::IsPlayingIdActive(UAkAudioEvent* AkEvent, int32 PlayingID) const
{
	if (PlayingID == AK_INVALID_PLAYING_ID || !IsValid(AkEvent))
	{
		return false;
	}

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		return AkAudioDevice->IsPlayingIDActive(AkEvent->GetWwiseShortID(), (uint32)PlayingID);
	}

	return false;
}

bool USoundEmitterComponentBase::IsPlaying() const
{
	return (IsValid(m_AkComp) && m_AkComp->HasActiveEvents());
}

bool USoundEmitterComponentBase::HasActiveEvents() const
{
	return (IsValid(m_AkComp) && m_AkComp->HasActiveEvents()) || !m_repeatingOneShots.IsEmpty(); // || !m_playingLoops.IsEmpty();
}

bool USoundEmitterComponentBase::HasAkComponent() const
{
	return IsValid(m_AkComp);
}
#pragma endregion
