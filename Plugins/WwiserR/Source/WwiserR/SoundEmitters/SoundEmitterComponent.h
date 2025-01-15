// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "SoundEmitterComponentBase.h"
#include "SoundEmitterComponent.generated.h"


// keeps track culling on CharacterMovementComponents
USTRUCT()
struct WWISERR_API FCharachterMovementData
{
	GENERATED_BODY()

public:
	class UCharacterMovementComponent* MovementComp = nullptr;
	float LastCulledMaxSpeed{ 0.f };
	bool HasReculled{ false };

public:
	FCharachterMovementData() {}
};

/**
	* SoundEmitterComponent
	* ---------------------
	*
	* - handles distance culling
	*    - one shots: distance check on post event
	*    - loops:
			- timer/event based checks based on maximum speed of this emitter and the spatial audio listener
			- maximum speed of this component is - by default - calculated automatically and updated when either increased or reaching zero.
			As this requires ticking, this is intended as a fall-back mechanism to ensure correct behaviour, and should be manually
			overriden whenever possible
		- a distance attenuation buffer can be set when posting the event, e.g so we hear explosions even if we were just out of range at the
		time the event was posted
	* - an AkComponent is spawned only when a sound is posted and within culling range, and destroyed when it's no longer needed
	*    - a cooldown time can be set to prevent spamming spawning/unspawning AkComponents (e.g. footsteps), or the distance culling system can
		   be forcibly ignored entirely
	*    - a simple emitter pool can trivially be implemented later on, however, this comes at the cost of game objects not being named
		   according to their parent actor/component in the Wwise authoring tool
**/
UCLASS(ClassGroup = "WwiserR" , meta = (BlueprintSpawnableComponent))
class WWISERR_API USoundEmitterComponent : public USoundEmitterComponentBase
{
	GENERATED_BODY()

#pragma region Class Properties
public:
	/** Destroys the emiiter when all (at least one) events have stopped playing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter")
	bool bAutoDestroy = false;
	
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Distance Culling")
	void SetUseDistanceCulling(bool bShouldUseDistanceCulling);

	/** Set to true only if this emitter can move. Can be changed at runtime via SetEmitterCanMove() */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|Distance Culling|Looping events only")
	bool bCanMove = true;

	/** Use automatic max speed for distance culling. Optionally reset max speed to zero.
	(true by default, set to false if max speed is known for better performance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Emitter|Distance Culling|Looping events only")
	bool bAutoEmitterMaxSpeed = true;

	/** Resets the auto calculated max speed to zero when the emitter stops moving.
	Useful to reduce the frequency of distance culling checks when this emitter only moves occasionally, but comes at a slightly higher cost
	when the emitter starts moving again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides = "bAutoEmitterMaxSpeed"),
		Category = "Sound Emitter|Distance Culling|Looping events only")
	bool bResetAutoEmitterMaxSpeedOnStopMoving = true;

	/** Maximum speed at which this emitter (or its parent when bUseParentLocationForCulling = true) can move */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides = "!bAutoEmitterMaxSpeed"),
		Category = "Sound Emitter|Distance Culling|Looping events only")
	float ManualEmitterMaxSpeed = 2000.f;

	/** Use parent location for distance culling */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Distance Culling")
	void SetUseParentLocationForCulling(bool bUseParentLocationForDistanceCulling);

	/** Use automatic max speed for distance culling. Optionally reset max speed to zero. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Distance Culling|Looping events only")
	void SetEmitterMaxSpeedAuto(bool bResetMaxSpeed = true, bool bResetMaxSpeedOnStopMoving = true);

	/** Use manual max speed for distance culling. Optionally set a new max speed (in Centimeters per Second) by setting NewEmitterMaxSpeed >= 0. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Distance Culling|Looping events only")
	void SetEmitterMaxSpeedManual(float NewEmitterMaxSpeed = -100.f);

	/** manually set this when the emitter starts/stops moving to optimize distance culling performance */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Distance Culling|Looping events only")
	void SetEmitterCanMove(bool bEmitterCanMove);

	//UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|AkComponent")
	void SetAttenuationScalingFactor(float Value) override;
#pragma endregion

#pragma region Member Variables
private:
	TArray<FPlayingAudioLoop> m_culledPlayingLoops{};
	FTimerHandle m_timerNextDistanceCull{};

	FVector m_lastCullingLocation = FVector::ZeroVector;
	bool	m_bMustRecalculateAllLoopCullTimes = false;
	float	m_nextCullTime = INFINITY;
	int		m_nextCullIndex = 0;

	FCharachterMovementData m_characterMovementData{};

	inline static int32 s_newVirtualLoopPlayingId = MAXINT32;

protected:
	float	m_emitterMaxSpeed = 0.f;
#pragma endregion

protected:
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool UnregisterEmitterIfInactive() override;

	bool DestroyAkComponent() override;
	void OnDebugDrawChanged() override;

#if !UE_BUILD_SHIPPING
	void DebugDrawOnTick(UWorld* World) override;
	void DrawDebugActivationRanges(UWorld* world, float MaxAttenuationRadius, float RangeBuffer, bool bIsVirtual = false, float LifeTime = -1.f);
#endif

public:
	explicit USoundEmitterComponent();
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#pragma region Internal - Distance Culling
private:
	void UpdateOnMovedDelegates();
	void RemoveOnMovedDelegates();

	/** toggles ticking depending on whether we need it for calculating auto emitter max speed */
	void ToggleTickForAutoEmitterMaxSpeedCalculation();

	/** updates m_emitterMaxSpeed when bAutoEmitterMaxSpeed = true, if speed has increased or is set to zero when
	 *  bResetAutoEmitterMaxSpeedOnStopMoving = true */
	void UpdateAutoEmitterMaxSpeed(float NewMaxSpeed);

	/** adjusts and reschedules the next culling time, and forces a recalculation of the next culling time for all loops on that next culling pass.
	*  this spreads calculations for multiple emitters with multiple loops over time, rather than having them happen at the same frame, e.g.
	when the listener probe max speed has increased.
	*/
	virtual void UpdateNextCullTimeAndLoopIndex();

	/** schedules the next distance culling pass for loop m_culledPlayingLoops[m_NextCullIndex] at time m_nextCullTime */
	bool ScheduleNextDistanceCulling();

	/** culls the currently cued loop, and selects the next loop to be culled */
	virtual void DistanceCull();
	/** culls a loop, (de)virtualizes it if necessary, and sets its next cull time */
	void CullByDistance(FPlayingAudioLoop& Loop);

	void VirtualizeLoop(FPlayingAudioLoop& Loop);
	int32 DevirtualizeLoop(FPlayingAudioLoop& Loop);

	float CalculateAndSetNextLoopCullTime(FPlayingAudioLoop& Loop);
	void OnListenersUpdated() override;

protected:
	virtual void RecullAllLoops();
	virtual void UpdateDistanceCullingRelativeMaxSpeed();
#pragma endregion

#pragma region Public Functions - Sound Emitter
public:
	void MuteEmitter(bool bMute) override;
	void StopAll() override;
	bool HasActiveEvents() const override;

	/** forces a distance culling reset */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Utilities")
	virtual void OnTeleported();

	/** forces a distance culling reset */
	UFUNCTION(BlueprintCosmetic)
	virtual void OnListenerTeleported();

	/** gets the most recent PlayingId of a loop event posted on this emitter */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Loop")
	virtual int32 GetLoopLastPlayingID(int32 InitialPlayingID);

	/** for looping events, both the original and last playing IDs can be used as PlayingId */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|GameSyncs", meta = (AdvancedDisplay = "3"))
	virtual void SetRtpcByPlayingID(UAkRtpc* AkRtpc, int32 PlayingID, float Value, int32 InterpolationTimeMs = 0);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Loop", meta = (AdvancedDisplay = "1"))
	virtual UPARAM(DisplayName = "PlayingID") int32
		PostLoop(UAkAudioEvent* LoopAkEvent, float ActivationRangeBuffer = 50.f, bool bQueryAndPostEnvironmentSwitches = false);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Loop", meta = (AdvancedDisplay = "1"))
	virtual bool StopLoop(
		UAkAudioEvent* LoopAkEvent, int32 PlayingID, int32 TransitionDurationInMs = 10, EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log1);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Loop",
		meta = (AdvancedDisplay = "2", AutoCreateRefTerm = "PostEventCallback"))
	virtual UPARAM(DisplayName = "LastPlayingID") int32 StopLoopUsingStopEvent(UAkAudioEvent* LoopAkEvent, UAkAudioEvent* StopAkEvent,
		int32 PlayingID, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) const int32 CallbackMask,
		const FOnAkPostEventCallback& PostEventCallback);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Loop", meta = (AdvancedDisplay = "1"))
	virtual bool StopAllMatchingLoops(
		UAkAudioEvent* LoopAkEvent, int32 TransitionDurationInMs = 10, EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log1);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Sound Emitter|Events|Loop", meta = (AdvancedDisplay = "1"))
	virtual bool StopAllLoops(int32 TransitionDurationInMs = 10, EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Log1);
#pragma endregion

#pragma region Public Functions - Static
public:
	UFUNCTION(BlueprintCallable, Category = "WwiserR|Sound Emitter")
	static UPARAM(DisplayName = "Sound Emitter Component") USoundEmitterComponent* GetAttachedSoundEmitterComponent(
		USceneComponent* AttachToComponent, bool& ComponentCreated, FName Socket = NAME_None,
		EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset);

	UFUNCTION(BlueprintCallable, Category = "WwiserR|Sound Emitter", meta = (WorldContext = "Context"))
	static UPARAM(DisplayName = "Sound Emitter Component") USoundEmitterComponent* SpawnSoundEmitterAtLocation(
		UObject* Context, FVector Location, FRotator Orientation, bool AutoDestroy = true);
#pragma endregion
};
