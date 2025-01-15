// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AK\SoundEngine\Common\AkTypes.h"
#include "AmbientBedManager.generated.h"

class USoundListenerManager;
class UDA_AmbientBed;
class UAkComponent;
class UAkRoomComponent;
class UAkAuxBus;
class UAkRtpc;

#define WEIGHTCOLORSTARTSEED 18;

struct FAmbientWeightOctreeElement
{
	UAmbientBedWeightComponent* AmbientSoundWeightComponent{};
	FBoxCenterAndExtent BoundingBox{};

	explicit FAmbientWeightOctreeElement(UAmbientBedWeightComponent* a_AmbientSoundWeightComponent);
};

struct FAmbientWeightOctreeSemantics
{
	typedef TOctree2<FAmbientWeightOctreeElement, FAmbientWeightOctreeSemantics> FOctree;

	enum { MaxElementsPerLeaf = 12 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 16 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(const FAmbientWeightOctreeElement& Element) { return Element.BoundingBox; }

	FORCEINLINE static bool AreElementsEqual(const FAmbientWeightOctreeElement& A, const FAmbientWeightOctreeElement& B)
	{
		return (A.AmbientSoundWeightComponent == B.AmbientSoundWeightComponent);
	}

	static void SetElementId(FOctree& OctreeOwner, const FAmbientWeightOctreeElement& Element, FOctreeElementId2 Id);
};

class TAmbientWeightOctree : public TOctree2<FAmbientWeightOctreeElement, FAmbientWeightOctreeSemantics>
{
public:
	TMap<uint32, FOctreeElementId2> ObjectToOctreeId{};
	uint32 NumElements = 0;

public:
	TAmbientWeightOctree()
		: TOctree2<FAmbientWeightOctreeElement, FAmbientWeightOctreeSemantics>(FVector::ZeroVector, HALF_WORLD_MAX) {}

	void AddWeight(UAmbientBedWeightComponent* AmbientSoundWeightComponent);
	void RemoveWeight(UAmbientBedWeightComponent* AmbientSoundWeightComponent);

#if !UE_BUILD_SHIPPING
protected:
	void DebugConsoleStats(UAmbientBedWeightComponent* AmbientSoundWeightComponent) const;
#endif
};

USTRUCT() struct FAmbientBedGroup
{
	GENERATED_BODY()

	UPROPERTY() UDA_AmbientBed* AmbientBed = nullptr;
	int32 GroupID = -1;

#if !UE_BUILD_SHIPPING
	FColor GroupColor{};
	inline static int32 s_colorSeed = WEIGHTCOLORSTARTSEED;
#endif

	FAmbientBedGroup() {}
	FAmbientBedGroup(UDA_AmbientBed* a_AmbientSoundLoop, int32 a_GroupID)
		: AmbientBed(a_AmbientSoundLoop), GroupID(a_GroupID), GroupColor(FColor::/*Red*/MakeRandomSeededColor(s_colorSeed))
	{
		s_colorSeed++;
	}

	bool operator==(const FAmbientBedGroup& Other) const
	{
		return AmbientBed == Other.AmbientBed && GroupID == Other.GroupID;
	}
};

FORCEINLINE uint32 GetTypeHash(const FAmbientBedGroup& AmbientLoopGroup)
{
	uint32 Hash = FCrc::MemCrc32(&AmbientLoopGroup, sizeof(FAmbientBedGroup));
	return Hash;
}

UCLASS(ClassGroup = "WwiserR", meta=(BlueprintSpawnableComponent))
class WWISERR_API UAmbientBedWeightComponent : public USceneComponent
{
	GENERATED_BODY()

	friend class TAmbientWeightOctree;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient Bed Weight")
	UDA_AmbientBed* AmbientBed = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ambient Bed Weight")
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grouping")
	bool bOverrideAmbientBedGroup = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grouping",
		meta = (EditCondition = "bOverrideAmbientBedGroup"))
	uint8 GroupId = 0;

protected:
	bool m_isInWeightOctree = false;

public:
	UAmbientBedWeightComponent();
	void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|AmbientBed")
	void SetWeight(float NewWeight);
};

UCLASS(ClassGroup = "WwiserR")
class WWISERR_API UAmbientBedEmitterComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	inline static UAkAuxBus* s_passthroughAuxBus{};

	AkGameObjectID m_emitterId{};
	AkGameObjectID m_listenerId{};
	AkGameObjectID m_roomId{};
	AkGameObjectID m_auxBusID{};
	AkGameObjectID m_passthroughAuxBusID{};

	UPROPERTY() UAkComponent* m_ambientEmitter{};
	UPROPERTY() UAkRtpc* m_distanceRtpc{};
	UPROPERTY() UAkRtpc* m_weightRtpc{};

	FVector m_localPosition{};
	float m_avgWeightedDistance = 0.f;
	float m_summedWeight = 0.f;
	float m_maxAccumWeight = 0.f;
	uint32 m_numWeights = 0;

#if !UE_BUILD_SHIPPING
	FColor m_dbgColor{};
#endif

protected:
	bool m_isInListenerRoom = false;
	bool m_isCrossfading = false;

	// 0.f = same room, 1.f = different room
	float m_FadePos = 0.f;
	
public:
	UAmbientBedEmitterComponent();

	void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	void Initialize(UDA_AmbientBed* AmbientBed, const FString& Name);
	void StartPlay(UDA_AmbientBed* AmbientBed);
	void Stop();
	void AccumulatePositionAndDistance(UDA_AmbientBed* AmbientBed);
	void SetEmitterListenerRelations();

protected:
	bool IsInListenerRoom();
};

#if !UE_BUILD_SHIPPING
struct FDebugValues
{
	float DistanceRtpc{};
	float WeightRtpc{};
	float MaxWeight{};

	FDebugValues(float a_DistanceRtpc, float a_WeightRtpc, float a_MaxWeight) :
		DistanceRtpc(a_DistanceRtpc), WeightRtpc(a_WeightRtpc), MaxWeight(a_MaxWeight) {}
};
#endif

UCLASS(ClassGroup = "WwiserR")
class WWISERR_API AAmbientBedWorldManager : public AActor
{
	GENERATED_BODY()

protected:
	UPROPERTY() USoundListenerManager* m_listenerManager {};
	TMap<FAmbientBedGroup, TSharedPtr<TAmbientWeightOctree>> m_weightComps{};
	TMap<UAkRoomComponent*, UAkComponent*> m_roomListeners{};
	
private:
	TMap<FAmbientBedGroup, TMap<UAkRoomComponent*, UAmbientBedEmitterComponent*>> m_playingAmbientBedEmitters{};
	FCriticalSection m_critSectAmbientEmitters;
	FCriticalSection m_critSectRoomListeners;

#if !UE_BUILD_SHIPPING
	TSet<UDA_AmbientBed*> m_postedBedsWithoutValidRange;		// so we can log warnings only once per UDA_StaticSoundLoop
	TMap<UAmbientBedWeightComponent*, FAmbientBedGroup*> m_dbgWeightComps;
	FCriticalSection m_critSectDbgWeightComps;
public:
	inline static TMap<UAmbientBedEmitterComponent*, FDebugValues> s_dbgViewportValues{};
	inline static FCriticalSection s_critSectDbgValues;
#endif

public:	
	AAmbientBedWorldManager();

	void Initialize(USoundListenerManager* SoundListenerManager);
	void Deinitialize();
	void EndPlay(EEndPlayReason::Type EndPlayReason);
	void Tick(float DeltaTime) override;

protected:
	void CreateAmbientEmitter(const FAmbientBedGroup& AmbientLoopGroup,
		UAkRoomComponent* RoomComp, UDA_AmbientBed* SoundLoop);
	UAkComponent* GetOrCreateRoomListener(UAkRoomComponent* RoomComp, const FString& BaseName);
	void UpdateEmitterPosition(const FAmbientBedGroup& LoopGroup, UAkRoomComponent* RoomComp,
		const FAmbientWeightOctreeElement& WeightElement, const FVector& ListenerPosition, float Range);
	void CleanupUnusedEmitters(const FAmbientBedGroup& LoopGroup,
		const TSet<UAkRoomComponent*>& FoundRooms, const TSet<FAmbientBedGroup*>& LoopGroupsToKeep);
	void CleanupRoomListeners();

#if !UE_BUILD_SHIPPING
	void DebugDrawOnTick(UWorld* World);
#endif

public:
	void AddWeight(UAmbientBedWeightComponent* AmbientBedWeightComponent, UDA_AmbientBed* AmbientBed);
	void RemoveWeight(UAmbientBedWeightComponent* AmbientBedWeightComponent, UDA_AmbientBed* AmbientBed);
};

UCLASS(ClassGroup = "WwiserR")
class WWISERR_API UAmbientBedManager : public UObject
{
	GENERATED_BODY()

	friend class AAmbientBedWorldManager;

protected:
	TMap<UWorld*, AAmbientBedWorldManager*> m_worldManagers{};
	UPROPERTY() USoundListenerManager* m_listenerManager {};
	bool m_isInitialized = false;

#if !UE_BUILD_SHIPPING
private:
	TSet<UDA_AmbientBed*> m_postedBedsWithoutValidAkEvent;	// so we can log warnings only once per UDA_StaticSoundLoop
#endif

public:
	void Initialize(USoundListenerManager* SoundListenerManager);
	void Deinitialize();

	void AddWeight(UWorld* World, UAmbientBedWeightComponent* AmbientSoundWeightComponent, UDA_AmbientBed* AmbientBed);
	void RemoveWeight(UWorld* World, UAmbientBedWeightComponent* AmbientSoundWeightComponent, UDA_AmbientBed* AmbientBed);

protected:
	void OnSpatialAudioListenerChanged(UWorld* NewWorld, UAkComponent* SpatialAudioListener);
	void OnPostWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
};
