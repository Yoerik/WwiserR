// // Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AkGameplayTypes.h"	// for AkCallback types
#include "AkSettings.h"			// for EAkCollisionChannel
#include "Core/AudioUtils.h"
#include "SoundEmitterComponentBase.generated.h"

#pragma region Repeating OneShots
// struct for posting repeating one shot events
USTRUCT(BlueprintType) struct WWISERR_API FRepeatableOneShot
{
	GENERATED_BODY()

public:
	/** Wwise one shot event to play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	class UAkAudioEvent* AkEvent = nullptr;

	/** should the one shot Wwise event repeat over time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repeatable OneShot")
	bool bShouldRepeat = false;

	/** prevent posting a new instance of the Wwise event if a previous instance of it is still playing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repeatable OneShot", meta = (EditConditionHides = "bShouldRepeat"))
	bool bBlockOverlaps = true;

	/** minimum time between posting 2 consecutive one shots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repeatable OneShot",
		meta = (EditConditionHides = "bShouldRepeat", ClampMin = 0.0f))
	float MinTimeInterval = 0.f;

	/** maximum time between posting 2 consecutive one shots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repeatable OneShot",
		meta = (EditConditionHides = "bShouldRepeat", ClampMin = 0.0f))
	float MaxTimeInterval = 0.f;

	/** amount of times to repeat posting the one shot Wwise event (-1 is infinite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repeatable OneShot", meta = (EditConditionHides = "bShouldRepeat"))
	int32 MaxRepetitions = -1;

public:
	FRepeatableOneShot() {}
};

FORCEINLINE uint32 GetTypeHash(const FRepeatableOneShot& RepeatableOneShot)
{
	uint32 Hash = FCrc::MemCrc32(&RepeatableOneShot, sizeof(FRepeatableOneShot));
	return Hash;
}

inline bool operator==(const FRepeatableOneShot& a, const FRepeatableOneShot& b)
{
	return GetTypeHash(a) == GetTypeHash(b);
}

// Callback when a new repeating one shot is posted
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnOneShotCallback, int32, PlayingID);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRepeatingOneShotEndedCallback, URepeatingOneShot*, RepeatingOneShot);

// keeps track of repeating one shot events
UCLASS(ClassGroup = "WwiserR", BlueprintType, Abstract, Transient) class WWISERR_API URepeatingOneShot : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient) USoundEmitterComponentBase* OwningSoundEmitterComponent = nullptr;
	UPROPERTY(Transient, BlueprintReadWrite) FRepeatableOneShot RepeatableOneShot = FRepeatableOneShot();

	int32 CallbackMask;
	FOnAkPostEventCallback& AkPostEventCallback = s_dummyAkPostEventCallback;
	FOnOneShotCallback& OneShotPostedCallback = s_dummyOneShotPostedCallback;
	FOnRepeatingOneShotEndedCallback& RepeatingOneShotEndedCallback = s_dummyRepeatingOneShotEndedCallback;
	UPROPERTY() FOnAkPostEventCallback AkPostEventCallbackInternal = s_dummyAkPostEventCallback;

	UPROPERTY(BlueprintReadWrite) float AttenuationRangeBuffer = 50.f;
	UPROPERTY(BlueprintReadWrite) bool bQueryAndPostEnvironmentSwitches = false;

	UPROPERTY(BlueprintReadOnly) int32 ActivePlayingID = AK_INVALID_PLAYING_ID;
	UPROPERTY(BlueprintReadOnly) int32 RepetitionsLeft = -1;
	UPROPERTY(BlueprintReadOnly) bool bIsPaused = false;

protected:
	FTimerHandle m_nextPostAkEventTimeHandle = FTimerHandle();
	bool bPostAkPostEventCallbackOnEndOfEvent = false;

	inline static FOnAkPostEventCallback s_dummyAkPostEventCallback{};
	inline static FOnOneShotCallback s_dummyOneShotPostedCallback{};
	inline static FOnRepeatingOneShotEndedCallback s_dummyRepeatingOneShotEndedCallback{};

public:
	URepeatingOneShot();
	void BeginDestroy() override;
	virtual void EndPlay();

protected:
	bool EndIfNoRepetitionsLeft(UWorld* world);

	UFUNCTION()
	virtual void OnAkPostEventCallback(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo);

	virtual void OnStartOneShot(int32 PlayingID);
	virtual void OnEndOneShot(UAkCallbackInfo* CallbackInfo);
	virtual void OnStartRepeatingOneShot();
	virtual void OnPauseRepeatingOneShot();
	virtual void OnResumeRepeatingOneShot();
	virtual void OnEndRepeatingOneShot();

	UFUNCTION()
	void PostRepeatingOneShotInternal();

	void ScheduleNextRepeatingOneShot();

public:
	virtual void Initialize(USoundEmitterComponentBase* Owner, UAkAudioEvent* a_AkEvent, int32 a_CallbackMask,
		const FOnAkPostEventCallback& a_AkPostEventCallback, const FOnOneShotCallback& a_OneShotPostedCallback,
		const FOnRepeatingOneShotEndedCallback& a_RepeatingOneShotEndedCallback, bool a_bShouldRepeat = false,
		int32 a_MaxRepetitions = -1, float a_MinTimeInterval = 0.f, float a_MaxTimeInterval = 0.f, bool a_bBlockOverlaps = true,
		float a_AttenuationRangeBuffer = 50.f, bool a_bQueryAndPostEnvironmentSwitches = false);

	virtual void Initialize(USoundEmitterComponentBase* a_Owner, UAkAudioEvent* a_AkEvent, bool a_bShouldRepeat = false,
		int32 a_MaxRepetitions = -1, float a_MinTimeInterval = 0.f, float a_MaxTimeInterval = 0.f, bool a_bBlockOverlaps = true,
		float a_AttenuationRangeBuffer = 50.f, bool a_bQueryAndPostEnvironmentSwitches = false);

	virtual void Initialize(USoundEmitterComponentBase* a_Owner, const FRepeatableOneShot& a_RepeatableOneShot,
		float a_AttenuationRangeBuffer = 50.f, bool a_bQueryAndPostEnvironmentSwitches = false);

	virtual bool Play();

	virtual void Pause(bool bStopPlayingEventImmediately = false, int32 TransitionDurationInMs = 10,
		EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log3);

	virtual bool Resume();

	virtual bool End(bool bStopPlayingEventImmediately = false, int32 TransitionDurationInMs = 10,
		EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log3);

	virtual bool IsPlaying();
};
#pragma endregion

#if !UE_BUILD_SHIPPING
// keeps track of all one shot events posted on this emitter when debugging events is enabled
struct WWISERR_API FPlayingOneShot
{
public:
	UAkAudioEvent*	AkEvent{};
	AkPlayingID		PlayingID{};

public:
	FPlayingOneShot(UAkAudioEvent* a_AkEvent, AkPlayingID a_PlayingID)
		: AkEvent(a_AkEvent)
		, PlayingID(a_PlayingID)
	{
	}

	FPlayingOneShot() {}
};

FORCEINLINE uint32 GetTypeHash(const FPlayingOneShot& PlayingOneShot)
{
	uint32 Hash = FCrc::MemCrc32(&PlayingOneShot, sizeof(FPlayingOneShot));
	return Hash;
}

inline bool operator==(const FPlayingOneShot& a, const FPlayingOneShot& b)
{
	return GetTypeHash(a) == GetTypeHash(b);
}
#endif

#pragma region SoundEmitterComponentBase
/**
	* SoundEmitterComponentBase
	* -------------------------
	*
	* - replaces AkComponent
	* - used to post events and gamesynchs in the 3d world
	* - an AkComponent is spawned only when a sound should play (e.g. after distance culling by child classes)
	* - a simple emitter pool can trivially be implemented later on, however, this comes at the cost of game objects not being named
		according to their parent actor/component in the Wwise authoring tool
**/
UCLASS(ClassGroup = "WwiserR", hidecategories	=
	(Variable, Rendering, Mobility, LOD, Component, ComponentTick, ComponentReplication, Replication, Physics, Activation, Collision))
	class WWISERR_API USoundEmitterComponentBase : public USceneComponent
{
	GENERATED_BODY()

	friend class URepeatingOneShot;

#if !UE_BUILD_SHIPPING
	DECLARE_MULTICAST_DELEGATE(FOnDebugDrawChanged)
#endif

#pragma region Class Properties
public:
	/*** SoundEmitter3dComponent Properties ***/
	/******************************************/

	/** Cull events based on their attenuation distance before posting the event to Wwise.
	Can be disabled for emitters that are always in range (e.g. player), or one shots that should play when the player teleports in range
	after a one shot was posted (e.g. important explosion).*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|Distance Culling")
	bool bUseDistanceCulling = true;

	/** Switcfhes to be set when initializing this emitter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter")
	TSet<class UAkSwitchValue*> InitialSwitches;

	/*** AkComponent Registration ***/
	/********************************/

	/** Always create an AkComponent and keep it registered with Wwise */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|Wwise Registration")
	bool bNeverUnregister = false;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Wwise Registration")
	virtual void SetNeverUnregister(bool bMustNeverUnregister);

	/** Cooldown time before destroying/unregister the AkComponent when all sounds finished playing.
	Useful for preventing continuous creation/destruction of the AkComponent, e.g. in the case of footsteps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|Wwise Registration", meta = (Units = "Seconds"))
	float UnregistrationCooldown = 0.f;

	/** Use the parent actor's location and speed as the distance culling reference.
	Useful when this components moves relatively to it's parent to reduce the frequency of distance culling checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|Distance Culling")
	bool bUseParentLocationForCulling = false;

	/*** AkComponent Properties ***/
	/******************************/

	/** Immediately stop all sound when owning actor or this emitter is destroyed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|AkComponent")
	bool bStopWhenOwnerDestroyed = false;

	/** Fade out and stop all loops when DestroyComponent() is explicitly called on this emitter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "!bStopWhenOwnerDestroyed"), Category = "Sound Emitter|AkComponent")
	bool bStopLoopsWhenEmitterDestroyed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "!bStopWhenOwnerDestroyed"), Category = "Sound Emitter|AkComponent")
	int32 LoopFadeOutTimeOnEmitterDestroyedInMs = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "!bStopWhenOwnerDestroyed"), Category = "Sound Emitter|AkComponent")
	EAkCurveInterpolation LoopFadeOutCurveOnEmitterDestroyed = EAkCurveInterpolation::Log1;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|AkComponent")
	void SetStopWhenOwnerDestroyed(bool bShouldStopWhenOwnerDestroyed);

	/** Modifies the attenuation computations on this game object to simulate sounds with a a larger or smaller area of effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|AkComponent")
	float AttenuationScalingFactor = 1.0f;

	/** Sets the attenuation scaling factor, which modifies the attenuation computations on this game object to simulate sounds with a a
	 * larger or smaller area of effect. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|AkComponent")
	virtual void SetAttenuationScalingFactor(float Value);

	/** Should this emitter use reverb volumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|AkComponent")
	bool bUseReverbVolumes = true;

	/**
	The line trace channel to use when doing line-of-sight traces for occlusion calculations. When set to 'Use Integration Settings
	Default', the value will be taken from the DefaultOcclusionCollisionChannel in the Wwise Integration Settings.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|AkComponent|Occlusion")
	TEnumAsByte<EAkCollisionChannel> OcclusionCollisionChannel = { EAkCollisionChannel::EAKCC_UseIntegrationSettingsDefault };

	/** Set the time interval between occlusion/obstruction checks (direct line of sight between the listener and this game object). Set to
	 * 0 to disable occlusion/obstruction on this component. We recommend disabling it if you want to use full Spatial Audio diffraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|AkComponent|Occlusion")
	float OcclusionRefreshInterval = 0.0f;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|AkComponent|Occlusion")
	ECollisionChannel GetOcclusionCollisionChannel() const;

	/**Enable spot reflectors for this Ak Component */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|AkComponent|Spatial Audio")
	bool bEnableSpotReflectors = false;

	/**
	 *	Define an outer radius around each sound position to simulate a radial sound source.
	 *	If the listener is outside the outer radius, the spread is defined by the area that the sphere takes in the listener field of view.
	 *	When the listener intersects the outer radius, the spread is exactly 50%. When the listener is in between the inner and outer
	 *radius, the spread interpolates linearly from 50% to 100%.
	 */
	UPROPERTY(
		EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|AkComponent|Spatial Audio|Radial Emitter", meta = (ClampMin = 0.0f))
	float outerRadius = .0f;

	/**
	 *	Define an inner radius around each sound position to simulate a radial sound source.
	 *	If the listener is inside the inner radius, the spread is 100%.
	 */
	UPROPERTY(
		EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|AkComponent|Spatial Audio|Radial Emitter", meta = (ClampMin = 0.0f))
	float innerRadius = .0f;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|AkComponent")
	void SetGameObjectRadius(float Outer, float Inner);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|AkComponent")
	void SetEnableSpotReflectors(bool bEnable);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|AkComponent|Spatial Audio|Debug Draw")
	bool DrawFirstOrderReflections = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|AkComponent|Spatial Audio|Debug Draw")
	bool DrawSecondOrderReflections = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|AkComponent|Spatial Audio|Debug Draw")
	bool DrawHigherOrderReflections = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Emitter|AkComponent|Spatial Audio|Debug Draw")
	bool DrawDiffraction = false;
#pragma endregion

#pragma region Member Variables
#if !UE_BUILD_SHIPPING
public:
	static inline FOnDebugDrawChanged OnEmitterDebugDrawChanged{};
#endif

private:
	inline static AkSwitchGroupID s_environmentSwitchDefaultGroupID = AkSwitchGroupID();
	TSet<URepeatingOneShot*>	m_repeatingOneShots{};
	FTimerHandle				m_timerNextUnregistration{};
	bool						m_isPendingUnregistration = false;

protected:
#if WITH_EDITOR
	class USoundListenerManager* s_listenerManager;	// non-static in editor to avoid problems when running under one process
#else
	inline static class USoundListenerManager* s_listenerManager = nullptr;
#endif

	TSet<TWeakObjectPtr<UAkComponent>>		m_connectedListeners{};
	TMap<AkSwitchGroupID, UAkSwitchValue*>	m_activeSwitches{};
	TSet<FActiveRtpc>						m_activeRtpcs{};
	bool m_isMuted = false;
	bool m_forceRegistration = false;	// allows overriding bNeverUnregister in child classes

public:
	UPROPERTY(Transient)
	class UAkComponent* m_AkComp;

protected:
	void UpdateNeverUnregister();
	FORCEINLINE bool IsUnregistrationForbidden() const
	{
		return bNeverUnregister || m_forceRegistration;
	}
	bool IsInListenerRange(UAkAudioEvent* AkEvent, float RangeBuffer);
	virtual TSet<TWeakObjectPtr<class UWorldSoundListener>> GetWorldListeners();
	virtual void InitializeListeners();
	//void SetListeners(const TSet<TWeakObjectPtr<UAkComponent>>& Listeners);

public:
	FORCEINLINE bool UseParentLocationForCulling() const { return bUseParentLocationForCulling && IsValid(GetOwner()); }

	FORCEINLINE FVector GetCullingLocation() const
	{
		return UseParentLocationForCulling() ? GetOwner()->GetActorLocation() : GetComponentLocation();
	}

#pragma endregion

#pragma region Internal - SceneComponent
public:
	USoundEmitterComponentBase();

protected:
	void BeginPlay() override;
	void InitializeInitialSwitches();
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool TryToStopTicking();
	USoundListenerManager* GetListenerManager();

public:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#pragma endregion

#pragma region Internal - AkComponent
protected:
	/** return true if a new AkComponent was created */
	virtual bool CreateAkComponentIfNeeded();

	/** return true if a valid AkComponent has been destroyed */
	virtual bool DestroyAkComponent();

	/** Sets m_timerUnregistration to UnregisterCooldown */
	virtual void ScheduleUnregistration(const UAkComponent* AkComponent);

	UFUNCTION()
	void CallUnregisterEmitterIfInactive() { UnregisterEmitterIfInactive(); };
	
	/** destroys the AkComponent if !bNeverUnregister and !HasActiveEvents(). Returns true if unregistration was succesful. */
	virtual bool UnregisterEmitterIfInactive();

	virtual void InitializeAkComponent();
	virtual void InitializeGameSynchs();

	virtual void OnListenersUpdated();
	virtual void OnAllWorldListenersRemoved();
#pragma endregion

#pragma region Internal - Debug
protected:
	FORCEINLINE FString GetFullComponentName() const
	{
		FString compMsg = IsValid(GetOwner()) ? (GetOwner()->GetName()).Append(TEXT("->")) : TEXT("");
		return compMsg.Append(GetName());
	}

	UFUNCTION()	virtual void OnDebugDrawChanged();

#if !UE_BUILD_SHIPPING
public:
	inline static bool s_debugToConsole = false;
	inline static bool s_debugDraw = false;
	inline static bool s_logEvents = false;

protected:
	FString m_msgEmitterDebugPersistent{};
	FString m_msgEmitterDebug{};

	virtual void DebugDrawOnTick(UWorld* World);
	bool ShouldDrawDebug(float InRange, float DistSquared);

private:
	TSet<FPlayingOneShot> m_debugOneShotEvents;
#endif
#pragma endregion

#pragma region Public Functions - Sound Emitter
public:
	void QueryAndPostEnvironmentSwitches();

	virtual int32 PostOneShot(UAkAudioEvent* AkEvent, float ActivationRangeBuffer = 50.f
		, bool bQueryAndPostEnvironmentSwitches = false, bool IgnoreDistanceCulling = false);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|OneShot",
		meta = (AdvancedDisplay = "1", AutoCreateRefTerm = "PostEventCallback"))
	virtual UPARAM(DisplayName = "PlayingID") int32 PostOneShot(UAkAudioEvent* AkEvent,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) const int32 CallbackMask,
		const FOnAkPostEventCallback& PostEventCallback, float ActivationRangeBuffer = 50.f
		, bool bQueryAndPostEnvironmentSwitches = false, bool IgnoreDistanceCulling = false);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|OneShot",
		meta = (AdvancedDisplay = "1", Latent, LatentInfo = "LatentInfo"))
	virtual UPARAM(DisplayName = "PlayingID") int32 PostOneShotAndWaitForEnd(UAkAudioEvent* AkEvent, FLatentActionInfo LatentInfo,
		float ActivationRangeBuffer = 50.f, bool bQueryAndPostEnvironmentSwitches = false, bool IgnoreDistanceCulling = false);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Repeating OneShot",
		meta = (AdvancedDisplay = "3", AutoCreateRefTerm = "AkPostEventCallback, OneShotPostedCallback, RepeatingOneShotEndedCallback"))
	virtual URepeatingOneShot* PostRepeatingOneShot(UAkAudioEvent* AkEvent, float MinTimeInterval, float MaxTimeInterval,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) const int32 CallbackMask,
		const FOnAkPostEventCallback& AkPostEventCallback, const FOnOneShotCallback& OneShotPostedCallback,
		const FOnRepeatingOneShotEndedCallback& RepeatingOneShotEndedCallback, int32 MaxRepetitions = -1, bool bBlockOverlaps = true,
		float ActivationRangeBuffer = 50.f, bool bQueryAndPostEnvironmentSwitches = false);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Repeating OneShot",
		meta = (AdvancedDisplay = "1", AutoCreateRefTerm = "RepeatableOneShot, AkPostEventCallback, OneShotPostedCallback, RepeatingOneShotEndedCallback"))
	virtual URepeatingOneShot* PostRepeatableOneShot(const FRepeatableOneShot& RepeatableOneShot,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) const int32 CallbackMask,
		const FOnAkPostEventCallback& AkPostEventCallback, const FOnOneShotCallback& OneShotPostedCallback,
		const FOnRepeatingOneShotEndedCallback& RepeatingOneShotEndedCallback, float ActivationRangeBuffer = 50.f,
		bool bQueryAndPostEnvironmentSwitches = false);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Repeating OneShot",
		meta = (AdvancedDisplay = "1"))
	virtual void PauseRepeatingOneShot(URepeatingOneShot* RepeatingOneShot, bool bStopPlayingEventImmediately = false,
		int32 TransitionDurationInMs = 10, EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log1);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Repeating OneShot",
		meta = (AdvancedDisplay = "1"))
	virtual UPARAM(DisplayName = "IsResumed") bool ResumeRepeatingOneShot(URepeatingOneShot* RepeatingOneShot);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Repeating OneShot",
		meta = (AdvancedDisplay = "1"))
	virtual UPARAM(DisplayName = "IsEnded") bool EndRepeatingOneShot(URepeatingOneShot* RepeatingOneShot,
		bool bStopPlayingEventImmediately = false, int32 TransitionDurationInMs = 10,
		EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log1);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, BlueprintPure, Category = "WwiserR|Sound Emitter|Events|Repeating OneShot")
	virtual bool IsRepeatingOneShotPlaying(URepeatingOneShot* RepeatingOneShot);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|OneShot", meta = (AdvancedDisplay = "1"))
	virtual void StopOneShotByPlayingId(
		int32 PlayingID, int32 TransitionDurationInMs = 10, EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log1) const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events")
	virtual UPARAM(DisplayName = "Succeeded") bool SeekOnEvent(
		UAkAudioEvent* AkAudioEvent, int32 SeekPositionMs, bool bSeekToNearestMarker, int32 PlayingID);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events")
	virtual void BreakEvent(int32 in_playingID) const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|OneShot")
	virtual void PostTrigger(class UAkTrigger* AkTrigger);

	/*UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs")
	virtual void SetPersistentSwitch(UAkSwitchValue* AkSwitchValue);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs")
	virtual void SetPersistentSwitches(TArray<UAkSwitchValue*> AkSwitchValues);*/

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs")
	virtual void SetSwitch(UAkSwitchValue* AkSwitchValue);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs")
	virtual void SetSwitches(const TArray<UAkSwitchValue*>& AkSwitchValues);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs")
	virtual void ResetSwitchGroup(class UAkGroupValue* AkSwitchGroup);

	//virtual void ResetSwitchGroup(TArray<UAkSwitchValue*> Switches, UAkGroupValue* AkSwitchGroup);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs")
	virtual void ResetAllSwitchGroups();
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs", meta = (AdvancedDisplay = "2"))
	virtual void SetRtpc(class UAkRtpc* AkRtpc, float Value, int32 InterpolationTimeMs = 0, float Epsilon = 0);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs", meta = (AdvancedDisplay = "2"))
	virtual void ResetRtpcValue(class UAkRtpc* AkRtpc);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs", meta = (AdvancedDisplay = "2"))
	virtual void ResetAllRtpcValues();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Utilities")
	virtual void MuteEmitter(bool bMute);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Utilities")
	virtual void StopAll();
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Utilities")
	virtual bool IsPlayingIdActive(UAkAudioEvent* AkEvent, int32 PlayingID) const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Utilities")
	virtual bool IsPlaying() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Utilities")
	virtual bool HasActiveEvents() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Utilities")
	virtual bool HasAkComponent() const;
#pragma endregion
};
