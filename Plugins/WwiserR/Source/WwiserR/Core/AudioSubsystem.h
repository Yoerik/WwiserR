// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioUtils.h"

#if WITH_EDITOR
#include "WwiserR_Editor/EditorAudioUtils.h"
#endif

#include "AudioSubsystem.generated.h"

/*
 * Audio Subsystem
 * ---------------
 *
 * - manages persistent global audio systems and functions
 *
 */
UCLASS(ClassGroup = "WwiserR")
class WWISERR_API UAudioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAppInForegroundChanged, bool);

public:
	FOnAppInForegroundChanged OnAppHasAudioFocusChanged;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Audio Systems")
	USoundListenerManager* ListenerManager = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Audio Systems")
	UMusicManager* MusicManager = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Audio Systems")
	UAkAudioEvent* MuteAllEvent = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Audio Systems")
	UAkAudioEvent* UnmuteAllEvent = nullptr;

	/*UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio Globals")
	class UDA_AudioMaterials* AudioMaterialProperties;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Audio Systems")
	class UAmbientSeedManager* AmbientSeedManager;*/

private:
	uint32 m_lastTickFrame = INDEX_NONE;

#if WITH_EDITOR
	inline static TArray<UAudioSubsystem*> s_allAudioSubsystems{};
#endif

protected:
	UPROPERTY(Transient) class UGlobalSoundEmitterManager* m_globalSoundEmitterManager = nullptr;
	UPROPERTY(Transient) class UStaticSoundEmitterManager* m_staticSoundEmitterManager = nullptr;
	UPROPERTY(Transient) class UAmbientBedManager* m_ambientBedManager = nullptr;
	//UPROPERTY(Transient) class UPooledSoundEmitterManager* m_pooledSoundEmitterManager{};

	bool m_isAppForeground = true;

#if WITH_EDITOR
public:	// creates a circular dependency which is probably okay with editor, but not in build
#endif
	bool m_muteWhenGameNotInForeground = false;

public:
	bool m_isMuted = false;

#if WITH_EDITOR
private:
	inline static bool s_soloAudioInFirstPIEClient = false;
	EShouldInstancePlayAudio m_ShouldInstancePlayAudio{ EShouldInstancePlayAudio::Undefined };

public:
	uint8 m_PIEInstance = -1;
#endif

#pragma region PIE instance log macro definitions
#if WITH_EDITOR
private:
	FORCEINLINE FString GetPieInstMsg()
	{
		FString pieInstance{};

		if (GetWorld()->WorldType == EWorldType::PIE && m_PIEInstance != 255)
		{
			pieInstance.Append(TEXT("PIE_")).AppendInt(m_PIEInstance);
			pieInstance.Append(TEXT(" ")); \
		}

		return pieInstance;
	}

public:
#define WR_DBG_INST_NET(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s%s%s : %s"), NETMODE_WORLD, *GetPieInstMsg(), *GetNameSafe(this), *Msg); \
}

#define WR_DBG_INST_FUNC(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s%s - %s : %s"), *GetPieInstMsg(), *GetNameSafe(this), FUNC_NAME, *Msg); \
}

#define WR_DBG_INST_NET_FUNC(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s%s%s - %s() : %s"), NETMODE_WORLD, *GetPieInstMsg(), *GetNameSafe(this), FUNC_NAME, *Msg); \
}
#else
#define WR_DBG_INST_NET(Verbosity, Format, ...) WR_DBG_NET(Verbosity, Format, ##__VA_ARGS__)
#define WR_DBG_INST_FUNC(Verbosity, Format, ...) WR_DBG_FUNC(Verbosity, Format, ##__VA_ARGS__)
#define WR_DBG_INST_NET_FUNC(Verbosity, Format, ...) WR_DBG_NET_FUNC(Verbosity, Format, ##__VA_ARGS__)
#endif
#pragma endregion

#pragma region Tick
public:
	void Tick(float DeltaTime) override;

	bool IsAllowedToTick() const override;
	FORCEINLINE bool IsTickable() const override
	{
		if (const UWorld* world = GetWorld())
		{
			return GEngine->GetNetMode(world) != ENetMode::NM_DedicatedServer;
		}

		return false;
	}
	FORCEINLINE ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	FORCEINLINE TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAmbientSeedManager, STATGROUP_Tickables); }
	FORCEINLINE bool IsTickableWhenPaused() const override { return false; }
	FORCEINLINE bool IsTickableInEditor() const override { return false; }
#pragma endregion

#pragma region Initialization
public:
	void Initialize(FSubsystemCollectionBase& a_collection) override;
	void Deinitialize() override;

protected:
	void InitializeOnServer();
	void InitializeOnClient();
	void InitializeMembers(const class UWwiserRGameSettings* audioConfig);
	void DeinitializeOnServer();
	void DeinitializeOnClient();

	void InitializeGlobalEmitterManager();
	void DeinitializeGlobalEmitterManager();
	//void InitializePooledEmitterManager();
	//void DeinitializePooledEmitterManager();
	void InitializeListenerManager(const UWwiserRGameSettings* AudioConfig);
	void DeinitializeListenerManager();
	void InitializeMusicManager(const UWwiserRGameSettings* AudioConfig);
	void DeinitializeMusicManager();
	void InitializeStaticSoundEmitterManager();
	void DeinitializeStaticSoundEmitterManager();
	void InitializeAmbientBedManager();
	void DeinitializeAmbientBedManager();

	void ClientBindDelegates();

	void ClientUnbindDelegates();
#pragma endregion

	void ClientBeginPlay(UWorld* World, const FWorldInitializationValues IVS);
	UFUNCTION() void OnMuteAllCVarChanged();

#if WITH_EDITOR
	UFUNCTION()	void OnMuteInstanceCVarChanged();
	UFUNCTION() void OnUnmuteInstanceCVarChanged();
#endif

	void UpdateAppHasAudioFocus();

#if WITH_EDITOR
	void InitializeGlobalListenerConnection();
public:
	bool ShouldInstancePlayAudio();
#endif

#pragma region Public Methods
public:
	FORCEINLINE static UAudioSubsystem* Get(const UObject* const a_worldContext)
	{
		const UWorld* world = IsValid(a_worldContext) ? a_worldContext->GetWorld() : nullptr;

		return IsValid(world)
			? world->GetGameInstance()->GetSubsystem<UAudioSubsystem>()
			: nullptr;
	}

	FORCEINLINE UGlobalSoundEmitterManager* GetGlobalSoundEmitterManager() const { return m_globalSoundEmitterManager; }
	FORCEINLINE UStaticSoundEmitterManager* GetStaticSoundEmitterManager() const { return m_staticSoundEmitterManager; }
	FORCEINLINE UAmbientBedManager* GetAmbientSoundManager() const { return m_ambientBedManager; }
	//FORCEINLINE UPooledSoundEmitterManager* GetPooledSoundEmitterManager() const { return m_pooledSoundEmitterManager; }

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, BlueprintPure, Category = "WwiserR|Audio Subsystem")
	USoundListenerManager* GetListenerManager();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, BlueprintPure, Category = "WwiserR|Audio Subsystem")
	UMusicManager* GetMusicManager();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Audio Subsystem")
	UPARAM(DisplayName = "HasChanged") bool MuteAllAudio(const bool bMute);

	bool PostWwiseMuteEvent(const bool bMute);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Audio Subsystem")
	void SetMuteWhenAppNotInForeground(const bool bMute);
#pragma endregion
};
