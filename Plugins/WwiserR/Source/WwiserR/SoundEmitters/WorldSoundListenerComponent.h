// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SoundEmitterComponentBase.h"
#include "AkComponent.h"
#include "WorldSoundListenerComponent.generated.h"

class UAkGameObject;
class UAuxSoundEmitterComponent;
class USoundListenerManager;

#pragma region Structs
USTRUCT()
struct WWISERR_API FAuxBusParams
{
	GENERATED_BODY()

	UPROPERTY() UAkGameObject* BusAkGameObject;
	UPROPERTY() UAkGameObject* PreSendAkGameObject;
	UPROPERTY() UAkGameObject* PostSendAkGameObject;
	TSet<TWeakObjectPtr<class UAuxSoundEmitterComponent>> AuxSoundEmitters{};
	TSet<TWeakObjectPtr<UAkGameObject>> GlobalSoundObjects;
	float SendLevel{};
	float AttenuationRange{};
	float Directivity{};
	FName AuxGroupName{};

	FAuxBusParams() {}
	FAuxBusParams(UAkGameObject* a_BusAkGameObject, UAkGameObject* a_PreSendAkGameObject, UAkGameObject* a_PostSendAkGameObject,
		TSet<TWeakObjectPtr<UAuxSoundEmitterComponent>> a_AuxSoundEmitters, TSet<TWeakObjectPtr<UAkGameObject>> a_GlobalSoundObjects,
		float a_SendLevel, float a_AttenuationRange, float a_Directivity, FName a_AuxGroupName)
		: BusAkGameObject(a_BusAkGameObject)
		, PreSendAkGameObject(a_PreSendAkGameObject)
		, PostSendAkGameObject(a_PostSendAkGameObject)
		, AuxSoundEmitters(a_AuxSoundEmitters)
		, GlobalSoundObjects(a_GlobalSoundObjects)
		, SendLevel(a_SendLevel)
		, AttenuationRange(a_AttenuationRange)
		, Directivity(FMath::Clamp(a_Directivity, 0.f, 1.f))
		, AuxGroupName(a_AuxGroupName)
	{}
};

USTRUCT(BlueprintType)
struct WWISERR_API FAuxBusComps
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) UAkGameObject* PreSendAkGameObject;
	UPROPERTY(BlueprintReadOnly) UAkGameObject* PostSendAkGameObject;

	UPROPERTY(BlueprintReadOnly) TSet<UAuxSoundEmitterComponent*> AuxSoundEmitters{};
	UPROPERTY(BlueprintReadOnly) TSet<UAkGameObject*> GlobalSoundObjects;

	FAuxBusComps() {}
	FAuxBusComps(UAkGameObject* a_PreSendAkGameObject, UAkGameObject* a_PostSendAkGameObject,
		TSet<UAuxSoundEmitterComponent*>  a_AuxSoundEmitters, TSet<UAkGameObject*> a_GlobalSoundObjects)
		: PreSendAkGameObject(a_PreSendAkGameObject)
		, PostSendAkGameObject(a_PostSendAkGameObject)
		, AuxSoundEmitters(a_AuxSoundEmitters)
		, GlobalSoundObjects(a_GlobalSoundObjects)
	{}
};
#pragma endregion

#pragma region WorldSoundListener
/**
 * WorldSoundListener: spatialized in-world 'microphone' with customizable directivity
 **/
UCLASS(ClassGroup = "WwiserR", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent),
	hidecategories = (Transform, Rendering, Mobility, LOD, Component, Activation))
class WWISERR_API UWorldSoundListener : public UAkComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldSoundListener")
	float MaxSpeed{};

#if WITH_EDITOR // non-static in editor to avoid problems when running under one process
	USoundListenerManager* s_listenerManager;
#else
	inline static USoundListenerManager* s_listenerManager = nullptr;
#endif

	inline static TMap<UWorldSoundListener*, TMap<UAkAuxBus*, FAuxBusParams>> s_worldListenersAuxBusParams{};

	TMap<UAkAuxBus*, FAuxBusParams> m_auxConfigurations;
	TSet<TWeakObjectPtr<UAuxSoundEmitterComponent>> m_connectedAuxSoundEmitters{};
	TSet<TWeakObjectPtr<UAkGameObject>> m_connectedGlobalEmitters{};

public:
	explicit UWorldSoundListener(const class FObjectInitializer& ObjectInitializer);

protected:
	void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()	void UpdatePosition();
	void UpdateWorldListener();

public:
	static void UpdateSoundEmitterSendLevels(UAkComponent* AkComponent);
	void UpdateAuxEmitterCompConnections(UAkAuxBus* AuxBus);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager|WorldSoundListener", meta = (WorldContext = "Context"))
	static UPARAM(DisplayName = "WorldSoundListener")UWorldSoundListener* AddWorldListener(
		const UObject* Context, float MaximumSpeed, USceneComponent* AttachToComponent, const FName Socket = NAME_None);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager|WorldSoundListener")
	static UPARAM(DisplayName = "Listener Removed") bool RemoveWorldListener(UWorldSoundListener* WorldListener);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager|WorldSoundListener")
	UPARAM(DisplayName = "SoundEmitters")FAuxBusComps RouteToAuxBus(UAkAuxBus* AuxBus, const TSet<USceneComponent*>& AttachToComponents,
		const FName Socket = NAME_None,	const FName AuxBusGroupName = NAME_None, const float AuxSendLevelInPercent = 100.f,
		const float AttenuationRange = 0.f,	const float MaximumSpeed = 0.f, const float Directivity = 0.f);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager|WorldSoundListener")
	UPARAM(DisplayName = "WasRemoved") bool RemoveAuxBus(UAkAuxBus* AuxBus);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager|WorldSoundListener")
	void SetAuxLevel(UAkAuxBus* AuxBus, const float AuxSendPercent = 100.f);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager|WorldSoundListener")
	void SetMaxSpeed(const float a_MaxSpeed);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager|WorldSoundListener")
	UAkGameObject* GetBusSendAkComp(const UAkAuxBus* AuxBus, bool& WasFound) const;

	FORCEINLINE float GetMaxSpeed() { return MaxSpeed; }
};
#pragma endregion

#pragma region AuxFeedComponent
UCLASS(ClassGroup = "WwiserR", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent),
	hidecategories = (Transform, Rendering, Mobility, LOD, Component, Activation))
	class WWISERR_API UAuxFeedComponent : public USoundEmitterComponentBase
{
	GENERATED_BODY()
};
#pragma endregion
