// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "AkGameplayTypes.h"
#include "SoundEmitters/SoundEmitterComponentBase.h"
#include "StaticSoundEmitterComponent.generated.h"

/*****************************
* UStaticSoundEmitterComponent
* ****************************
*
* Extension of SoundEmitter3dComponent to be used for static (not moving) emitters with a considerable amount of instances in a map/world.
* - avoids virtualizing voices in Wwise
* - provides better voice limiting options, by limiting both total instance count, and instance per horizontal quadrant around the listener (probe)
***/

UCLASS(ClassGroup = "WwiserR", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class WWISERR_API UStaticSoundEmitterComponent : public USoundEmitterComponentBase
{
	GENERATED_BODY()

	friend class AStaticSoundEmitterWorldManager;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayingStateChanged, bool, bIsPlaying, UDA_StaticSoundLoop*, StaticSoundLoop);

public:
	UPROPERTY(BlueprintAssignable, Category = "WwiserR|StaticSoundEmitterComponent")
	FOnPlayingStateChanged OnPlayingStateChanged{};

protected:
	TSet<UDA_StaticSoundLoop*> m_postedLoops{};
	TMap<UAkAudioEvent*, AkPlayingID> m_playingLoops{};

public:
	UStaticSoundEmitterComponent();

protected:
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// called from AStaticSoundEmitterWorldManager
	void StartPlayAudio(UDA_StaticSoundLoop* StaticSoundLoop);
	void StopPlayAudio(UDA_StaticSoundLoop* StaticSoundLoop);

#if !UE_BUILD_SHIPPING
	void DebugDrawOnTick(UWorld* World) override;
#endif

public:
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|StaticSoundEmitterComponent")
	void PostStaticSoundLoop(class UDA_StaticSoundLoop* StaticSoundLoop);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|StaticSoundEmitterComponent")
	void StopStaticSoundLoop(UDA_StaticSoundLoop* StaticSoundLoop); // , int32 TransitionDurationInMs, EAkCurveInterpolation FadeCurve);

	bool HasActiveEvents() const override;
};
