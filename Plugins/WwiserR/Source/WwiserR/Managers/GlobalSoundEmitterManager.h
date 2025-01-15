// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Misc/EnumRange.h"
#include "AkGameplayTypes.h"
#include "GlobalSoundEmitterManager.generated.h"

/**
*	- edit enum values before Count to add, remove, or rename global emitters
*   - this is deliberately set in code to avoid accidentally changing these in blueprints
*/
UENUM(BlueprintType, Category = "WwiserR")
enum class EGlobalSoundEmitter : uint8
{
	Music			UMETA(DisplayName = "Music"),
	GUI				UMETA(DisplayName = "GUI"),
	Narration		UMETA(DisplayName = "Narration"),
	Comms			UMETA(DisplayName = "Comms"),
	Count			UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EGlobalSoundEmitter, EGlobalSoundEmitter::Count);

/**
 * GlobalSoundEmitterManager
 * - creates and owns the global sound emitters
 */
UCLASS(ClassGroup = "WwiserR")
class WWISERR_API UGlobalSoundEmitterManager : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
private:
	inline static TArray<UGlobalSoundEmitterManager*> s_allConnectedGlobalEmitterManagers{};

public:
	FORCEINLINE static TArray<UGlobalSoundEmitterManager*> GetAllConnectedListenerManagers() { return s_allConnectedGlobalEmitterManagers; }
#endif

protected:
	UPROPERTY() class UAkGameObject*	m_globalListener = nullptr;
	AkGameObjectID						m_globalListenerID{};
	TArray<UAkGameObject*>				m_globalEmitters;
	TArray<AkGameObjectID>				m_globalEmitterIDs;

public:
	~UGlobalSoundEmitterManager();

	virtual void Initialize();
	virtual void Deinitialize();

protected:
	bool GetIndexOf(const UAkGameObject* Emitter, uint8& Index) const;

public:
	virtual void ConnectGlobalListener(const bool bConnect);
	virtual void ConnectGlobalEmitter(UAkGameObject* Emitter, const bool bConnect);

	FORCEINLINE TSet<UAkGameObject*> GetAllGlobalSoundObjects() const
	{
		TSet<UAkGameObject*> allGlobalSoundObjects = TSet<UAkGameObject*>(m_globalEmitters);
		allGlobalSoundObjects.Emplace(m_globalListener);

		return allGlobalSoundObjects;
	}

	FORCEINLINE TSet<UAkGameObject*> GetAllGlobalEmitters() const
	{
		return TSet<UAkGameObject*>(m_globalEmitters);
	}

	FORCEINLINE UAkGameObject* GetGlobalListener() const
	{
		return m_globalListener;
	}

	FORCEINLINE UAkGameObject* GetGlobalSoundEmitter(const EGlobalSoundEmitter GlobalSoundEmitter) const
	{
		return m_globalEmitters.IsValidIndex((uint8)GlobalSoundEmitter) ? m_globalEmitters[(uint8)GlobalSoundEmitter] : nullptr;
	}

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = "WwiserR|Global Listener", meta = (WorldContext = "Context"))
	static UPARAM(DisplayName = "Global Sound Listener") UAkGameObject* GetGlobalListener(const UObject* Context);

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = "WwiserR|Global Sound Emitters", meta = (WorldContext = "Context"))
	static UPARAM(DisplayName = "Global Sound Emitter") UAkGameObject* GetGlobalSoundEmitter(
		const UObject* Context, EGlobalSoundEmitter GlobalSoundEmitter);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Global Sound Emitters")
	static void ConnectListenerToGlobalSoundObject(UAkComponent* Listener, UAkGameObject* GlobalSoundObject, bool bResetConnections = true);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Global Sound Emitters")
	static void DisconnectListenerFromGlobalSoundObject(UAkComponent* Listener, UAkGameObject* GlobalSoundObject);
};
