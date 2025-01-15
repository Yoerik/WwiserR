// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "SoundEmitterComponent.h"
#include "AuxSoundEmitterComponent.generated.h"

USTRUCT()
struct WWISERR_API FAuxBusConfig
{
	GENERATED_BODY()

	UPROPERTY() UWorldSoundListener* WorldSoundListener {};
	UPROPERTY() class UAkGameObject* BusAkGameObject {};
	FName AuxBusGroupName {};
	float SendLevel = 0.f;
	float SquaredAttenuationRange = 0.f;
	//float MaxSpeed = 0.f;
	float NextCullTime = -1.f;

	FAuxBusConfig() {}
	FAuxBusConfig(UWorldSoundListener* a_WorldSoundListener, UAkGameObject* a_BusAkGameObject, FName a_AuxBusGroupName,
		float a_SendLevel = 0.f, float a_AttenuationRange = 0.f/*, float a_MaxSpeed = 0.f*/)
		: WorldSoundListener(a_WorldSoundListener)
		, BusAkGameObject(a_BusAkGameObject)
		, AuxBusGroupName(a_AuxBusGroupName)
		, SendLevel(a_SendLevel)
		, SquaredAttenuationRange(a_AttenuationRange* a_AttenuationRange)
		//, MaxSpeed(a_MaxSpeed)
	{}
};

/**
 * UAuxSoundEmitterComponent
 *  - SoundEmitterComponent that receives spatialized aux busses
 */
UCLASS()
class WWISERR_API UAuxSoundEmitterComponent : public USoundEmitterComponent
{
	GENERATED_BODY()

protected:
	TMap<class UAkAuxBus*, FAuxBusConfig> m_auxBusses{};
	//TSet<UAkAuxBus*> m_playingAuxBusses;
	bool m_cullAuxBusses = true;

private:
	bool m_neverUnregisterParent;
	bool m_isAuxActive = false;
	FTimerHandle m_auxCullingTimerHandle;
	FTimerDelegate m_auxCullingTimerDelegate;
	//float m_auxEmitterMaxSpeed = 0.f;
	float m_squaredAuxAttRange = 0.f;

public:
	UAuxSoundEmitterComponent();

	void AddAuxBus(UAkAuxBus* AuxBus, const FAuxBusConfig& AuxBusConfig);
	void RemoveAuxBus(UAkAuxBus* AuxBus);
	//void SetNeverUnregister(bool bMustNeverUnregister) override;

	void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION()	void UpdateListenerConnections();
	void InitializeAkComponent() override;

	// invalidate any of the listeners that are feeding this aux bus, to prevent feedback loops
	TSet<TWeakObjectPtr<UWorldSoundListener>> GetWorldListeners() override;

	UFUNCTION() void CullAuxBus();
	void RecullAllLoops() override;
	void UpdateDistanceCullingRelativeMaxSpeed() override;

	float CalculateNextAuxBusCullTime();
	bool IsInAuxListenerRange();
	void UpdateAuxEmitterAttenuationRange();
	//void UpdateAuxEmitterMaxSpeed_Internal();

#if !UE_BUILD_SHIPPING
	bool TryToStopTicking() override;
	void DebugDrawOnTick(UWorld* World) override;
#endif

public:
	static UAuxSoundEmitterComponent* GetAttachedAuxSoundEmitterComponent(USceneComponent* AttachToComponent, bool& ComponentCreated,
		FName Socket = NAME_None, EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Aux Sound Emitter")
	virtual void SetCullingForAuxBusses(bool bMustCullAuxBusses);
};
