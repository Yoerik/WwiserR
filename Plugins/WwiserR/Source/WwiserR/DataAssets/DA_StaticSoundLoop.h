// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AkGameplayTypes.h"	// for EAkCurveInterpolation
#include "DA_StaticSoundLoop.generated.h"

/**
 * Data asset to configure events that can be posted on StaticSoundEmitterComponents
 */
UCLASS(ClassGroup = "WwiserR", BlueprintType)
class WWISERR_API UDA_StaticSoundLoop : public UDataAsset
{
	GENERATED_BODY()

public:
	/* looping Wwise event */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looping Audio Event")
	class UAkAudioEvent* LoopEvent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Looping Audio Event")
	int32 FadeOutTimeInMs = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Looping Audio Event")
	EAkCurveInterpolation FadeOutCurve = EAkCurveInterpolation::Linear;

	/** Lerps the reference position between the camera and the distance probe. 0 = Camera, 1 = Distance Probe **/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance Culling", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float ReferencePositionLerp = 1.f;

	/* maximum instances that are aloud to play at once */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance Culling")
	uint8 MaxInstances = 0;

	/* maximum instances per quadrant around the listener (probe) that are aloud to play at once */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance Culling")
	uint8 MaxInstancesPerQuadrant = 0;

	/* range (in Unreal units) within which this event will be considered for playback.
	If no strictly positive value is set, the attenuation range of the Wwise event will be used.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Distance Culling")
	float ActivationRangeOverride = -1.f;

	bool operator==(const UDA_StaticSoundLoop& Other) const
	{
		return LoopEvent == Other.LoopEvent && MaxInstances == Other.MaxInstances && MaxInstancesPerQuadrant == Other.MaxInstancesPerQuadrant;
	}
};

FORCEINLINE uint32 GetTypeHash(const UDA_StaticSoundLoop& DA_StaticSoundLoop)
{
	uint32 Hash = FCrc::MemCrc32(&DA_StaticSoundLoop, sizeof(UDA_StaticSoundLoop));
	return Hash;
}
