// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Tickable.h"
#include "AkComponent.h"
#include "Kismet/KismetMathLibrary.h" // for EEasingFunc
#include "SoundListenerManager.generated.h"

#pragma region Enums
UENUM(BlueprintType, Category = "WwiserR")
enum class EListenerRotation : uint8
{
	Camera				UMETA(DisplayName = "Camera"),
	Target				UMETA(DisplayName = "Target"),
	CameraToTarget		UMETA(DisplayName = "CameraToTarget")
};

UENUM(BlueprintType, Category = "WwiserR")
enum class EListenerTarget : uint8
{
	CameraViewTarget	UMETA(DisplayName = "CameraViewTarget"),
	Player				UMETA(DisplayName = "Player"),
	CustomTarget		UMETA(DisplayName = "CustomTarget", Hidden),
	CustomPosition		UMETA(DisplayName = "CustomPosition", Hidden)
};
#pragma endregion

#pragma region Structs

USTRUCT(BlueprintType)
struct WWISERR_API FListenerManagerComponentProperties
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial Audio Listener")
	EListenerRotation ListenerRotation = EListenerRotation::Camera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial Audio Listener")
	EListenerTarget ListenerTarget = EListenerTarget::CameraViewTarget;

	/** Lerps the listener position between the camera and the listener target. 0 = Camera, 1 = Listener Target (e.g. player head) **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio Listener", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float ListenerPositionLerp = 0.0f;

	/** Lerps the listener position between the camera and the listener target. 0 = Camera, 1 = Listener Target (e.g. player head) **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio Listener", meta = (ClampMin = 0.f, ClampMax = 1.f))
	float DistanceProbePositionLerp = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Audio Listener")
	bool bKeepListenerInSameRoomAsTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial Audio Listener")
	class UAkRtpc* ListenerSpeedRtpc = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Attachment")
	FName TargetAttachmentPoint = TEXT("head");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Attachment")
	FVector TargetAttachmentOffsetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Attachment")
	FRotator TargetAttachmentOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Culling")
	float SpatialAudioListenerMaxSpeed = 2000.f;

	/** automatically set the attenuation reference max speed from the character movement component if the listener target is an ACharacter **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance Culling")
	bool bAutoAttenuationReferenceSpeedForCharacters = true;

	/** slower movement modes will be ignored, to reduce sound emitter reculling. Can be useful if there are many loops posted on SoundEmitterComponents **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance Culling", meta = (EditCondition = "bAutoAttenuationReferenceSpeedForCharacters"))
	TEnumAsByte<EMovementMode> MinimumCharacterReferenceMovementModeMaxSpeed = EMovementMode::MOVE_None;
};

USTRUCT()
struct WWISERR_API FCustomTargetSettings
{
	GENERATED_BODY()

	UPROPERTY() USceneComponent* m_attachToComponent{};
	FName					m_attachPointName{};
	FTransform				m_attachPointOffset{};
	EAttachLocation::Type	m_locationType{};

	FCustomTargetSettings() {}
	FCustomTargetSettings(USceneComponent* AttachToComponent, const FName AttachPointName = NAME_None,
		const FTransform& AttachPointOffset = FTransform(), EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset)
		: m_attachToComponent(AttachToComponent)
		, m_attachPointName(AttachPointName)
		, m_attachPointOffset(AttachPointOffset)
		, m_locationType(LocationType)
	{}
};

USTRUCT()
struct WWISERR_API FGlideData
{
	GENERATED_BODY()

public:
	bool	bIsGlidingForward{ false };
	bool	bIsGlidingReverse{ false };
	float	StartPositionLerp{ 0.f };
	float	EndPositionLerp{ 0.f };
	float	Duration{ 0.f };
	float	InterpolationEasingFunctionBlendExponent{ 1.75f };
	EEasingFunc::Type InterpolationEasingFunction{ EEasingFunc::Linear };

	float	Progress{ 0.f };
	double	LastTimeStarted{ 0.f };

public:
	FGlideData() {}
	FGlideData(bool	a_bIsGlidingForward, float a_StartPositionLerp, float a_EndPositionLerp, float a_Duration, double a_LastTimeStarted,
		float a_InterpolationEasingFunctionBlendExponent, EEasingFunc::Type a_InterpolationEasingFunction)
		: bIsGlidingForward(a_bIsGlidingForward)
		, bIsGlidingReverse(!a_bIsGlidingForward)
		, StartPositionLerp(a_StartPositionLerp)
		, EndPositionLerp(a_EndPositionLerp)
		, Duration(a_Duration)
		, InterpolationEasingFunctionBlendExponent(a_InterpolationEasingFunctionBlendExponent)
		, InterpolationEasingFunction(a_InterpolationEasingFunction)
		, LastTimeStarted(a_LastTimeStarted)
	{
		if (bIsGlidingReverse)
		{
			Duration *= Progress;
		}
	}
};
#pragma endregion

#pragma region Data Asset
/**
 * Data Asset to configure the Listener Manager component settings
 **/
UCLASS(ClassGroup = "WwiserR", BlueprintType, Blueprintable, AutoExpandCategories = "Listener Manager Component")
class WWISERR_API UDAListenerManagerSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Listener Manager Component")
	FListenerManagerComponentProperties ListenerManagerComponentProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Listeners")
	float DefaultListenerMaxSpeed = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play In Editor")
	bool bAutoAudition = false;

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Play In Editor")
	void Audition();

	void PostEditChangeProperty(struct FPropertyChangedEvent& e) override;
#endif
};
#pragma endregion

#pragma region SpatialProbeComponent
/**
 * ListenerTarget (similar to ViewTarget) helper component which must be an AkComponent to be set as the Wwise 'distance probe'
 **/
UCLASS(ClassGroup = "WwiserR")
class WWISERR_API USpatialProbeComponent : public UAkComponent
{
	GENERATED_BODY()

	explicit USpatialProbeComponent(const FObjectInitializer& ObjectInitializer);
};
#pragma endregion


UCLASS(Blueprintable, ClassGroup = "WwiserR")
class WWISERR_API USoundListenerManagerComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	// properties
	UPROPERTY(Transient) UAkRtpc* m_listenerSpeedRtpc {};
	FListenerManagerComponentProperties m_listManCompProperties;

	// members
	FAkAudioDevice* m_akAudioDevice{};
	UPROPERTY(Transient)	UWorld*					m_currentWorld{};
	UPROPERTY(Transient)	USoundListenerManager*	m_ListenerManager{};
	UPROPERTY(Transient)	class UAkComponent*		m_spatialAudioListener{};
	UPROPERTY(Transient)	APlayerController*		m_playerController {};
	UPROPERTY(Transient)	APlayerCameraManager*	m_playerCameraManager{};
	UPROPERTY(Transient)	APawn*					m_playerPawn{};
	UPROPERTY(Transient)	AActor*					m_currentViewTarget {};
	UPROPERTY(Transient)	USceneComponent*		m_targetComponent{};
	UPROPERTY(Transient)	USpatialProbeComponent* m_distanceProbe{};

	float					m_spatialAudioListenerMaxSpeed{};
	FTransform				m_customTargetTransform{};
	bool m_shouldAdjustForPawnEyeHeight = false;
	FCustomTargetSettings m_lastCustomTargetSettings{};
	class UCharacterMovementComponent* m_characterMovementComp{};
	float m_characterMovementSpeed{};
	FGlideData m_listenerGlideData{};
	FGlideData m_probeGlideData{};
	FVector m_lastListenerPosition{};

	//bool m_listenerLeftTargetRoomViaConnectingPortal = false;
	//bool m_listenerReturnedToTargetRoomViaConnectingPortal = false;
	//bool m_freezeListenerTransform = false;

	//FRotator m_lastListenerRotation{};

protected:
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // -> start ticking manager again

public:
	USoundListenerManagerComponent();
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void Initialize(USoundListenerManager* SoundListenerManager);
	void ImportListenerManagerComponentProperties(const FListenerManagerComponentProperties& ListenerManagerComponentProperties);
	void SetListenerRotation(const EListenerRotation NewListenerRotation);
	void SetListenerTarget(const EListenerTarget NewListenerTarget);
	void SetTargetTransform(FVector Location, FQuat Rotation);
	void SetListenerPositionLerp(const float NewPositionLerp, const bool bTriggerEmitterRecull);
	void SetDistanceProbeMaxSpeed(float MaxSpeed);
	float GetDistanceProbeMaxSpeed() const;

	bool AttachTargetComponent(bool isCustomTarget, USceneComponent* AttachToComponent, const FName AttachPointName = NAME_None,
		const FTransform& AttachPointOffset = FTransform(), EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset);
	bool AttachTargetComponent(bool isCustomTarget, AActor* AttachToActor, const FName AttachPointName = NAME_None,
		const FTransform& AttachPointOffset = FTransform(), EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset);
	void GlideListenerPositionLerp(const float EndPositionLerp, const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition = false,
		const EEasingFunc::Type InterpolationEasingFunction = EEasingFunc::Linear, const float InterpolationEasingFunctionBlendExponent = 1.75f);
	void GlideDistanceProbePositionLerp(const float EndPositionLerp, const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition = false,
		const EEasingFunc::Type InterpolationEasingFunction = EEasingFunc::Linear, const float InterpolationEasingFunctionBlendExponent = 1.75f);

	FORCEINLINE UAkComponent* GetDistanceProbe() const { return m_distanceProbe; }
	FORCEINLINE USceneComponent* GetListenerTargetComponent() const { return m_targetComponent; }

private:
	float GetMovementModeMaxSpeed(const UCharacterMovementComponent* Comp, const EMovementMode MovementMode) const;
	void UpdateGlidingPositionLerp(FGlideData& GlideData, float& PositionLerp);

protected:
	bool UpdateTarget();
	FVector GetListenerPosition();
	FRotator GetListenerRotation() const;

#if !UE_BUILD_SHIPPING
	mutable FVector m_lastProbeLocation;
	void DebugDraw() const;
#endif

};
#pragma region SoundListenerManager
/**
 * ListenerManager
 * ---------------
 *
 * - handles the spatial audio listener's position, rotation and (Wwise) distance probe in relation to the camera and player/viewtarget or
 *   custom target actor or component
 * - configured by a data asset that can be exchanged at runtime and/or individual parameters that can be updated at runtime
 * - (for different scenario's, e.g. in vehicle, eavesdropping, aiming, ...). Note: smooth transitioning between presets might be added
 *   later if needed
 * - exposes information relevant to audio emitters (e.g. for distance culling) and audio systems
 * - must be added on the PlayerController
 **/
UCLASS(Blueprintable, ClassGroup = "WwiserR")
class WWISERR_API USoundListenerManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

	friend class USoundListenerManagerComponent;

#pragma region SoundListenerManager - Delegates
	// notify 3d emitters when the potential maximum speed of the distance probe has increased, to update their distance culling timings
	DECLARE_MULTICAST_DELEGATE(FOnMaxSpeedIncreased)
	// notify 3d emitters when the distance probe has changed/teleported, to update their distance culling timings
	DECLARE_MULTICAST_DELEGATE(FOnAttenuationReferenceChanged)

	// notify 3d emitters when a world listener was added
	DECLARE_MULTICAST_DELEGATE(FOnListenersUpdated)
	// notify 3d emitters when a world listener was added
	DECLARE_MULTICAST_DELEGATE(FOnAllWorldListenersRemoved)

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSpatialAudioListenerChanged, UWorld* NewWorld, UAkComponent* SpatialAudioListener)
#pragma endregion

#pragma region SoundListenerManager - Properties
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Listener Manager")
	UDAListenerManagerSettings* ListenerManagerSettings;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener Configuration")
	UPARAM(DisplayName = "Settings Imported") bool ImportListenerManagerComponentProperties(UDAListenerManagerSettings* DAListenerManagerSettings);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener Configuration")
	void SetListenerRotation(const EListenerRotation NewListenerRotation);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener Configuration")
	void SetListenerTarget(const EListenerTarget NewListenerTarget);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener Configuration",
		meta = (ClampMin = 0.f, ClampMax = 1.f))
	void SetListenerPositionLerp(const float NewPositionLerp, const bool bTriggerEmitterRecull);

	/** / (in Centimeters per Second) */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener Configuration")
	void SetDistanceProbeMaxSpeed(float MaxSpeed);

#pragma endregion

#pragma region SoundListenerManager - Member Variables
public:
	FOnMaxSpeedIncreased				OnMaxSpeedIncreased;
	FOnAttenuationReferenceChanged		OnAttenuationReferenceChanged;
	FOnListenersUpdated					OnListenersUpdated;
	FOnAllWorldListenersRemoved			OnAllWorldListenersRemoved;
	FOnSpatialAudioListenerChanged		OnSpatialAudioListenerChanged;

protected:
	UPROPERTY(Transient) USoundListenerManagerComponent* m_SoundListenerManagerComponent {};
	UPROPERTY(Transient) APlayerController* m_playerController {};
	/*UPROPERTY(Transient) class APlayerCameraManager* m_playerCameraManager{};
	UPROPERTY(Transient) class APawn* m_playerPawn{};
	UPROPERTY(Transient) UWorld* m_currentWorld {};*/
	UPROPERTY(Transient) class UAkComponent* m_spatialAudioListener{};
	AkGameObjectID m_spatialAudioListenerID = AK_INVALID_GAME_OBJECT;

	TSet<TWeakObjectPtr<class UWorldSoundListener>> m_worldListeners{};
	TSet<AkGameObjectID> m_currentListenerIds{};

	FListenerManagerComponentProperties m_listenerManagerComponentProperties{};
	float	m_defaultListenerMaxSpeed{};

private:
	uint32 m_lastTickFrame = INDEX_NONE;

#if WITH_EDITOR
	inline static TArray<USoundListenerManager*> s_allConnectedListenerManagers{};
	inline static TSet<USoundListenerManager*> s_activeListenerManagers{};
	friend class UDAListenerManagerSettings;

	void SetActiveListenerManager(USoundListenerManager* ListenerManager);
	void AddActiveListenerManager(USoundListenerManager* ListenerManager);
	void RemoveActiveListenerManager(USoundListenerManager* ListenerManager);
#endif
	void OnSpatialAudioListenerChanged_Internal(UAkComponent* NewSpatialAudioListener);
	//bool m_IsPieInstanceConnected = false;
	//bool m_isSpatialListenerConnected = true;

public:
#if WITH_EDITOR
	FORCEINLINE static TArray<USoundListenerManager*> GetAllConnectedListenerManagers() { return s_allConnectedListenerManagers; }
#endif
	FORCEINLINE TSet<TWeakObjectPtr<UWorldSoundListener>> GetWorldListeners() { return m_worldListeners; }
	void AddWorldListener(UWorldSoundListener* WorldListener);
	void RemoveWorldListener(UWorldSoundListener* WorldListener);
	//void UpdateListeners(UAkComponent* AkComponent);
#pragma endregion

#pragma region SoundListenerManager - Internal Methods
private:

protected:
	//void OnPostWorldCreation(UWorld* World);
	void BeginPlay(UWorld* World);
	void EndPlay(UWorld* World);

	void OnTeleported(USceneComponent* UpdatedComponent);

	void RemoveSpatialAudioListener();

#pragma region SoundListenerManager - Tick
public:
	void Tick(float DeltaTime) override;

#if WITH_EDITOR
	FORCEINLINE bool IsAllowedToTick() const override
	{
		return s_activeListenerManagers.Contains(this);
	}
#endif

	FORCEINLINE bool IsTickable() const override
	{
		if (const UWorld* world = GetWorld())
		{
			if (GEngine->GetNetMode(world) == ENetMode::NM_DedicatedServer) { return false; }

			if (APlayerController* playerController = world->GetFirstPlayerController())
			{
				return playerController != m_playerController;
			}
		}

		return false;
	}
	FORCEINLINE ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	FORCEINLINE TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAmbientSeedManager, STATGROUP_Tickables); }
	FORCEINLINE bool IsTickableWhenPaused() const override { return false; }
	FORCEINLINE bool IsTickableInEditor() const override { return false; }
#pragma endregion

public:
	void Initialize();
	void Deinitialize();

#pragma endregion

public:
#pragma region Public Getters
	// static as they are frequently consulted by audio emitters and systems, to avoid too many casts when getting the ListenManager instance
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener")
	UAkComponent*		GetSpatialAudioListener() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener")
	FTransform			GetSpatialAudioListenerTransform() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener")
	FVector				GetSpatialAudioListenerPosition() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener")
	FRotator			GetSpatialAudioListenerRotation() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener")
	USceneComponent*	GetListenerTargetComp() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener Distance Probe")
	FVector				GetDistanceProbePosition() const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener Distance Probe")
	float				GetSquaredDistanceToDistanceProbe(const FVector& Location) const;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager|Spatial Audio Listener")
	float				GetDistanceProbeMaxSpeed() const;

	FORCEINLINE float	GetDefaultListenerMaxSpeed() const { return m_defaultListenerMaxSpeed; }
#pragma endregion

#pragma region Public Methods
public:
#if WITH_EDITOR
	void ConnectSpatialAudioListener();

	void MuteInstance(bool bMute);

private:
	UFUNCTION()
	void ConnectSpatialAudioListener_Internal();
#endif

public:
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager", meta = (AdvancedDisplay = "1"))
	void AttachTargetToComponent(USceneComponent* AttachToComponent,
		const FName AttachPointName = NAME_None, const FTransform& AttachPointOffset = FTransform(),
		EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager", meta = (AdvancedDisplay = "1"))
	void AttachTargetToActor(AActor* AttachToActor,
		const FName AttachPointName = NAME_None, const FTransform& AttachPointOffset = FTransform(),
		EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager", meta = (AdvancedDisplay = "1"))
	void SetTargetTransform(FVector Location, FQuat Rotation);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Listener Manager")
	void ResetToDefaultSettings();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager", meta = (AdvancedDisplay = "4"))
	void GlideListenerPositionLerp(const float EndPositionLerp,	const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition = false,
		const EEasingFunc::Type InterpolationEasingFunction = EEasingFunc::Linear, const float InterpolationEasingFunctionBlendExponent = 1.75f);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "DW|Audio|Listener Manager", meta = (AdvancedDisplay = "4"))
	void GlideDistanceProbePositionLerp(const float EndPositionLerp, const float TargetVerticalOffset, float Duration, bool bReturnToStartPosition = false,
		const EEasingFunc::Type InterpolationEasingFunction = EEasingFunc::Linear, const float InterpolationEasingFunctionBlendExponent = 1.75f);
#pragma endregion
};

#pragma endregion
