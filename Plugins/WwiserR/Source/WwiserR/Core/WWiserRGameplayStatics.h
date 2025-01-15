// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WWiserRGameplayStatics.generated.h"

/**
 * WWiserRGameplayStatics
 * ----------------------
 *
 * - use this for posting global gamesynchs instead of AkGameplayStatics because
 * - AkGameplayStatics will cause issues with posting global gamesynchs in PIE with multiple clients
 */

UCLASS(Blueprintable)
class WWISERR_API UWWiserRGameplayStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

protected:
	static bool ShouldPostGlobalGameSynch(const UObject* WorldContextObject);

public:
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR", meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalRtpcValue(const UObject* WorldContextObject, UAkRtpc* AkRtpc, float Value, int32 InterpolationTimeMs);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR", meta = (WorldContext = "WorldContextObject"))
	static void ResetGlobalRtpcValue(const UObject* WorldContextObject, UAkRtpc* AkRtpc, int32 InterpolationTimeMs = 0);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR", meta = (WorldContext = "WorldContextObject"))
	static void SetState(const UObject* WorldContextObject, UAkStateValue* StateValue);
};
