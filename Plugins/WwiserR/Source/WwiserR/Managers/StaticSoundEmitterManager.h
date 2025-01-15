// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Math/GenericOctree.h"
#include "EngineDefines.h"
#include "DataAssets/DA_StaticSoundLoop.h"
#include "StaticSoundEmitterManager.generated.h"

class UDA_StaticSoundLoop;
class UStaticSoundEmitterComponent;

UENUM()
enum class EQuadrant : uint8
{
	NorthEast,
	SouthEast,
	SouthWest,
	NorthWest
};

USTRUCT()
struct FStaticEmitterQuadrant
{
	GENERATED_BODY()

public:
	TArray<UStaticSoundEmitterComponent*> m_Emitters;

	float m_MaxDistanceSquared = -1.f;
	uint8 m_MaxDistanceIndex = 0;
	uint8 m_Count = 0;

public:
	FStaticEmitterQuadrant(
		TArray<UStaticSoundEmitterComponent*> a_Emitters, float a_MaxDistanceSquared, uint8 a_MaxDistanceIndex, uint8 a_Count)
		: m_Emitters(a_Emitters)
		, m_MaxDistanceSquared(a_MaxDistanceSquared)
		, m_MaxDistanceIndex(a_MaxDistanceIndex)
		, m_Count(a_Count)
	{}
	explicit FStaticEmitterQuadrant(uint8 a_EmittersPerQuadrant)
		: m_MaxDistanceSquared(-1.f)
		, m_MaxDistanceIndex(0)
		, m_Count(0)
	{
		m_Emitters.Init(nullptr, a_EmittersPerQuadrant);
	}
	FStaticEmitterQuadrant()
		: m_Emitters(TArray<UStaticSoundEmitterComponent*>())
		, m_MaxDistanceSquared(-1.f)
		, m_MaxDistanceIndex(0)
		, m_Count(0)
	{}

public:
	bool TryToAddEmitter(UStaticSoundEmitterComponent* a_StaticSoundEmitterComponent,
		const FVector a_StableListenerPosition,	const float a_DistanceToListenerSquared, bool& bCountIncreased);
	bool RemoveFarthestEmitter(FVector StableListenerPosition);
	void FindAndUpdateQuadrantMaxDistance(FVector StableListenerPosition);
};

USTRUCT()
struct FStaticSoundEmittersInRange
{
	GENERATED_BODY()

public:
	static uint8 s_maxEmittersPerQuadrantIfNotDefinedByUser;
	static uint8 s_maxEmittersTotalIfNotDefinedByUser;

public:
	UPROPERTY()
	TArray<FStaticEmitterQuadrant> m_StaticSoundEmitterQuadrants;

	uint8 m_MaxEmittersTotal {};
	uint8 m_Count {};

public:
	FStaticSoundEmittersInRange(uint8 a_MaxEmittersPerQuadrant, uint8 a_MaxEmittersTotal)
		: m_MaxEmittersTotal(a_MaxEmittersTotal > 0 ? a_MaxEmittersTotal : s_maxEmittersTotalIfNotDefinedByUser)
		, m_Count(0)
	{
		m_StaticSoundEmitterQuadrants.Init(FStaticEmitterQuadrant(a_MaxEmittersPerQuadrant > 0 ?
			a_MaxEmittersPerQuadrant : a_MaxEmittersTotal > 0 ?
			a_MaxEmittersTotal : s_maxEmittersPerQuadrantIfNotDefinedByUser),
			4);
	}
	FStaticSoundEmittersInRange()
		: m_MaxEmittersTotal(0)
		, m_Count(0)
	{
		m_StaticSoundEmitterQuadrants.Init(FStaticEmitterQuadrant(), 4);
	}

	bool TryToAddEmitter(UStaticSoundEmitterComponent* StaticSoundEmitterComponent,
		const FVector a_StableListenerPosition, const float a_DistanceToListenerSquared, const EQuadrant a_Quadrant);

	// clears all counters, "resetting" all counters, without reallocating memory or clearing elements
	FORCEINLINE void Clear()
	{
		for (FStaticEmitterQuadrant& quadrant : m_StaticSoundEmitterQuadrants)
		{
			quadrant.m_Emitters.Init(nullptr, quadrant.m_Emitters.Num());
			quadrant.m_MaxDistanceSquared = -1.f;
			quadrant.m_MaxDistanceIndex = 0;
			quadrant.m_Count = 0;
		}

		m_Count = 0;
	}
};

struct FStaticSoundEmitterOctreeElement
{
	UStaticSoundEmitterComponent* StaticSoundEmitterComponent{};
	FBoxCenterAndExtent BoundingBox{};

	explicit FStaticSoundEmitterOctreeElement(UStaticSoundEmitterComponent* a_StaticSoundEmitterComponent);
};

struct FStaticSoundEmitterOctreeSemantics
{
	typedef TOctree2<FStaticSoundEmitterOctreeElement, FStaticSoundEmitterOctreeSemantics> FOctree;

	enum { MaxElementsPerLeaf = 12 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 16 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(const FStaticSoundEmitterOctreeElement& Element) { return Element.BoundingBox; }

	FORCEINLINE static bool AreElementsEqual(const FStaticSoundEmitterOctreeElement& A, const FStaticSoundEmitterOctreeElement& B)
	{
		return (A.StaticSoundEmitterComponent == B.StaticSoundEmitterComponent);
	}

	static void SetElementId(FOctree& OctreeOwner, const FStaticSoundEmitterOctreeElement& Element, FOctreeElementId2 Id);
};

class TStaticSoundEmitterOctree : public TOctree2<FStaticSoundEmitterOctreeElement, FStaticSoundEmitterOctreeSemantics>
{
public:
	TMap<uint32, FOctreeElementId2> ObjectToOctreeId{};
	uint8 RangeIndex = 0;
	uint32 NumElements = 0;

public:
	TStaticSoundEmitterOctree()
		: TOctree2<FStaticSoundEmitterOctreeElement, FStaticSoundEmitterOctreeSemantics>(FVector::ZeroVector, HALF_WORLD_MAX) {}
	TStaticSoundEmitterOctree(uint8 a_RangeIndex)
		: TOctree2<FStaticSoundEmitterOctreeElement, FStaticSoundEmitterOctreeSemantics>(FVector::ZeroVector, HALF_WORLD_MAX)
		, RangeIndex(a_RangeIndex)
	{}

	void AddEmitter(UStaticSoundEmitterComponent* StaticSoundEmitterComponent);
	void RemoveEmitter(UStaticSoundEmitterComponent* StaticSoundEmitterComponent);
};

/**
 * StaticSoundEmitterWorldManager
 * ------------------------------
 *
 * - stores StaticSoundEmitters within its world in 3 octrees, depending on their range (close, medium, far)
 * - selects the StaticSoundEmitters and events that need to play
 * - handles starting and stopping audio playback
 */
#if !UE_BUILD_SHIPPING
DECLARE_MULTICAST_DELEGATE(FOnDebugViewportStatsChanged);
#endif

UCLASS(ClassGroup = "WwiserR")
class WWISERR_API AStaticSoundEmitterWorldManager : public AActor
{
	GENERATED_BODY()

protected:
	/* we only update this position if the listener has moved than a tolerated distance,
	** to avoid sounds jumping around due to small movements e.g. of the player's head when the player is standing still*/
	static float s_stableListenerPositionToleratedDistanceSquared;
	FVector m_stableDistanceProbePosition{};
	UPROPERTY() class USoundListenerManager* m_listenerManager{};

private:
	TArray<TMap<UDA_StaticSoundLoop*, TSharedPtr<TStaticSoundEmitterOctree>>> m_postedLoops{};
	TMap<UDA_StaticSoundLoop*, float> m_postedLoopRanges{};
	uint8 m_rangeIndex = 0;
	bool m_spreadOverMultipleFrames = false;
	TArray<float> m_rangeTresholds{};
	uint8 m_numDistanceRanges = 1;

	TArray<TMap<UDA_StaticSoundLoop*, FStaticSoundEmittersInRange>> m_loopsToPlayPerRange{};
	TArray<TMap<UDA_StaticSoundLoop*, TSet<UStaticSoundEmitterComponent*>>> m_playingEventsPerRange{};
	//FCriticalSection CriticalSection;

#if !UE_BUILD_SHIPPING
	TArray<uint32> m_dbgNumLoops{};		// posted loops per range
	TArray<uint32> m_dbgNumPosted{};	// posted emitters per range
	TArray<uint32> m_dbgNumPlaying{};	// playing emitters per range
	TArray<int32> m_dbgNumInRange{};	// emitters in range per range

	TArray<FString> m_rangeTexts{ TEXT("none") };
	TArray<int8> m_rangeTextLengths;

public:
	static FOnDebugViewportStatsChanged OnDebugViewportStatsChanged;
#endif

protected:
	void PlayAndStopAudioEvents(const uint8 a_distanceIndex);
	float GetActivationRange(const UDA_StaticSoundLoop* StaticSoundLoop) const;

	FORCEINLINE uint8 GetDistanceRangeIndex(const float Distance) const
	{
		if (!m_spreadOverMultipleFrames) { return 0; }

		for (int i = 0; i < m_numDistanceRanges - 1; i++)
		{
			if (Distance < m_rangeTresholds[i]) { return i; }
		}

		return m_numDistanceRanges - 1;
	}

public:
	AStaticSoundEmitterWorldManager();

	void Initialize(USoundListenerManager* SoundListenerManager);
	void Deinitialize();
	void EndPlay(EEndPlayReason::Type EndPlayReason);
	void Tick(float DeltaTime) override;

	//static AStaticSoundEmitterWorldManager* Get(const UObject* const a_Context);
	void PostLoop(UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop);
	void StopLoop(UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop);

#if !UE_BUILD_SHIPPING
protected:
	void Debug();
	void UpdateDbgNumLoopsAndEmitters();
	FORCEINLINE void AddSpacing(FString& currentMsgRanged, const int lastStat, const int startPos)
	{
		const uint8 currentLength = currentMsgRanged.Len();
		const uint8 lastDbgNumLen = FString::Printf(TEXT("%i"), lastStat).Len();

		uint8 numSpaces = 0; // all very, very ugly, but Unreal discards any sensible standard formatting
		for (char c : currentMsgRanged)
		{
			if (c == ' ' || c == '.') { numSpaces++; }
		}

		const uint8 spacing = currentLength + lastDbgNumLen - numSpaces / 2;

		for (int j = 0; j < startPos - spacing; j++) // even more verier uglier, (-sigh-) ...
		{
			currentMsgRanged.Append(" ");
		}

		// seriously, what's wrong with \t or .AppendChars() ??? Epic ???
		// consistent character spacing = V boomer, can't argue with that (-sigh again-) ...
	}
#endif
};

UCLASS(ClassGroup = "WwiserR")
class WWISERR_API UStaticSoundEmitterManager : public UObject
{
	GENERATED_BODY()

	friend class AStaticSoundEmitterWorldManager;

protected:
	TMap<UWorld*, AStaticSoundEmitterWorldManager*> m_worldManagers{};
	UPROPERTY() USoundListenerManager* m_listenerManager {};

#if !UE_BUILD_SHIPPING
	TSet<UDA_StaticSoundLoop*> m_postedBedsWithoutValidAkEvent{};	// so we can log warnings only once per UDA_StaticSoundLoop
#endif

public:
	void Initialize(USoundListenerManager* SoundListenerManager);
	void Deinitialize();

	void PostLoop(UWorld* World, UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop);
	void StopLoop(UWorld* World, UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop);

protected:
	void OnPostWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
};
