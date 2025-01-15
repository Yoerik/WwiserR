// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AK\SpatialAudio\Common\AkSpatialAudioTypes.h"
#include "DA_AmbientBed.generated.h"

/**
 * Data asset to configure events that can be posted on AmbientSoundWeight components
 */
UCLASS(ClassGroup = "WwiserR", BlueprintType)
class WWISERR_API UDA_AmbientBed : public UDataAsset
{
	GENERATED_BODY()

public:
	/** enable listener relative routing(position + orientation), disable diffraction/transmission
		set attenuation for spread/focus only **/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wwise Configuration")
	class UAkAudioEvent* LoopEvent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wwise Configuration")
	class UAkRtpc* DistanceRtpc;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wwise Configuration")
	UAkRtpc* WeightRtpc;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wwise Configuration")
	float MaxAccumulatedWeight = 100.f;

	/** enable listener relative routing(position + orientation), attenuation(through portals), 
		and diffraction/transmission **/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wwise Configuration")
	class UAkAuxBus* PropagationAuxBus;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wwise Configuration")
	class UAkAuxBus* PassthroughAuxBusOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance")
	float Range = -1.f;

	/** Lerps the reference position between the camera and the distance probe. 0 = Camera, 1 = Distance Probe **/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float ReferencePositionLerp = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spatialization")
	FQuat WorldRotation = FQuat();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spatialization")
	float Radius = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spatialization", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float InnerVolume = .5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spatialization")
	float PortalCrossfadeTime = .5f;
};

FORCEINLINE uint32 GetTypeHash(const UDA_AmbientBed& DA_AmbientBed)
{
	uint32 Hash = FCrc::MemCrc32(&DA_AmbientBed, sizeof(DA_AmbientBed));
	return Hash;
}
