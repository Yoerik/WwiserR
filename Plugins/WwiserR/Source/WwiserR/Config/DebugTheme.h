// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DebugTheme.generated.h"


USTRUCT(BlueprintType)
	struct WWISERR_API FThemeListenerManager
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Listener Manager")
	float ListenerSize = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Listener Manager")
	float ListenerGizmoSize = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Listener Manager")
	FColor ListenerColor = FColor::Cyan;

	UPROPERTY(EditDefaultsOnly, Category = "Listener Manager")
	float ListenerTargetSize = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Listener Manager")
	FColor ListenerTargetColor = FColor::Emerald;

	UPROPERTY(EditDefaultsOnly, Category = "Listener Manager")
	float DistanceProbeSize = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Listener Manager")
	FColor DistanceProbeColor = FColor::Purple;
};

USTRUCT(BlueprintType)
struct WWISERR_API FThemeSoundEmitters
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Sound Emitters", meta = (ClampMin = 0.f, ClampMax = 100.f))
	float FontScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Emitters", meta = (ClampMin = 0.f, ClampMax = 100.f))
	float EmitterScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Emitters")
	FColor ActiveSoundEmitterColor = FColor(0, 55, 0);	// dark green

	UPROPERTY(EditDefaultsOnly, Category = "Sound Emitters")
	FColor ActiveSoundEmitterTextColor = FColor::Green;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Emitters")
	FColor ActiveActivationRangeColor = FColor::Blue;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Emitters")
	FColor InactiveActivationRangeColor = FColor::Black;

	UPROPERTY(EditDefaultsOnly, Category = "Sound Emitters")
	float GizmoSize = 30.f;
};

USTRUCT(BlueprintType)
struct WWISERR_API FThemeStaticSoundEmitters
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Static Sound Emitters")
	FColor StaticSoundEmitterColor = FColor::Purple;

	UPROPERTY(EditDefaultsOnly, Category = "Static Sound Emitters")
	float Radius = 100.f;
};
