// Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "StaticSoundEmitterManager.h"
#include "AkAudioEvent.h"
#include "SoundEmitters/StaticSoundEmitterComponent.h"
#include "Managers/SoundListenerManager.h"
#include "Core/AudioUtils.h"
#include "Config/AudioConfig.h"
#include "Engine/World.h"


namespace Private_StaticSoundEmitterManager
{
static TAutoConsoleVariable<bool> CVar_StaticSoundEmitter_DebugConsole(
	TEXT("WwiserR.StaticSoundEmitter.DebugToConsole"), false, TEXT("Log non-critical messages to console. (0 = off, 1 = on)"), ECVF_Cheat);
static TAutoConsoleVariable<bool> CVar_StaticSoundEmitter_DebugConsoleOctreeStats(
	TEXT("WwiserR.StaticSoundEmitter.DebugToConsoleOctreeStats"), false, TEXT("Include octree stats in console logging. (0 = off, 1 = on)"), ECVF_Cheat);
static TAutoConsoleVariable<bool> CVar_StaticSoundEmitter_DebugDraw(
	TEXT("WwiserR.StaticSoundEmitter.DebugDraw"), false, TEXT("Visual debugging. (0 = off, 1 = on)"), ECVF_Cheat);
static TAutoConsoleVariable<bool> CVar_StaticSoundEmitter_ViewportStats(
	TEXT("WwiserR.StaticSoundEmitter.ViewportStats"), false, TEXT("Show stats in viewport. (0 = off, 1 = on)"), ECVF_Cheat);

bool bDebugConsole = false;
bool bDebugConsoleOctreeStats = false;
bool bDebugDraw = false;
bool bViewPortStats = false;

static void OnStaticSoundEmitterManagerUpdate()
{
	bDebugConsole = CVar_StaticSoundEmitter_DebugConsole.GetValueOnGameThread();
	bDebugConsoleOctreeStats = CVar_StaticSoundEmitter_DebugConsoleOctreeStats.GetValueOnGameThread();
	bDebugDraw = CVar_StaticSoundEmitter_DebugDraw.GetValueOnGameThread();

#if !UE_BUILD_SHIPPING
	if (bViewPortStats != CVar_StaticSoundEmitter_ViewportStats.GetValueOnGameThread())
	{
		if (AStaticSoundEmitterWorldManager::OnDebugViewportStatsChanged.IsBound())
		{
			AStaticSoundEmitterWorldManager::OnDebugViewportStatsChanged.Broadcast();
		}
	}
#endif
	bViewPortStats = CVar_StaticSoundEmitter_ViewportStats.GetValueOnGameThread();
}

FAutoConsoleVariableSink CStaticSoundEmitterManagerConsoleSink(FConsoleCommandDelegate::CreateStatic(&OnStaticSoundEmitterManagerUpdate));
} // namespace Private_StaticSoundEmitterManager
#pragma endregion

uint8 FStaticSoundEmittersInRange::s_maxEmittersPerQuadrantIfNotDefinedByUser = 16;
uint8 FStaticSoundEmittersInRange::s_maxEmittersTotalIfNotDefinedByUser = 32;
float AStaticSoundEmitterWorldManager::s_stableListenerPositionToleratedDistanceSquared = 100.f;

#if !UE_BUILD_SHIPPING
FOnDebugViewportStatsChanged AStaticSoundEmitterWorldManager::OnDebugViewportStatsChanged = FOnDebugViewportStatsChanged();
#endif

#pragma region Structs
bool FStaticSoundEmittersInRange::TryToAddEmitter(UStaticSoundEmitterComponent* StaticSoundEmitterComponent,
	const FVector a_StableListenerPosition, const float a_DistanceToListenerSquared, const EQuadrant a_Quadrant)
{
	FStaticEmitterQuadrant& currentQuadrant = m_StaticSoundEmitterQuadrants[(uint8)a_Quadrant];

	if (m_Count >= m_MaxEmittersTotal)
	{
		// if the current quadrant is full, replace the farthest emitter if the new emitter is closer
		if (currentQuadrant.m_Count >= currentQuadrant.m_Emitters.Num())
		{
			if (a_DistanceToListenerSquared < currentQuadrant.m_MaxDistanceSquared)
			{
				currentQuadrant.m_Emitters[currentQuadrant.m_MaxDistanceIndex] = StaticSoundEmitterComponent;
				currentQuadrant.FindAndUpdateQuadrantMaxDistance(a_StableListenerPosition);

				return true;
			}

			return false;
		}

		// find the farthest emitter across all quadrants
		float maxDistSquaredInAllQuadrants = -1.f;
		uint8 maxDistSquaredQuadrantIndex = 0;

		for (uint8 i = 0; i < 4; ++i)
		{
			const float quadrantMaxDistSquared = m_StaticSoundEmitterQuadrants[i].m_MaxDistanceSquared;

			if (quadrantMaxDistSquared > maxDistSquaredInAllQuadrants)
			{
				maxDistSquaredInAllQuadrants = quadrantMaxDistSquared;
				maxDistSquaredQuadrantIndex = i;
			}
		}

		// remove the farthest emitter if the new one is closer
		if (a_DistanceToListenerSquared < maxDistSquaredInAllQuadrants
			&& m_StaticSoundEmitterQuadrants[maxDistSquaredQuadrantIndex].RemoveFarthestEmitter(a_StableListenerPosition))
		{
			m_Count--;
		}
		else
		{
			return false;
		}
	}

	// try to add new emitter to current quadrant
	bool countIncreased;
	bool wasAdded = currentQuadrant.TryToAddEmitter(StaticSoundEmitterComponent,
		a_StableListenerPosition, a_DistanceToListenerSquared, countIncreased);

	if (countIncreased)	{ m_Count++; }
	return wasAdded;
}

bool FStaticEmitterQuadrant::TryToAddEmitter(UStaticSoundEmitterComponent* StaticSoundEmitterComponent,
	const FVector a_StableListenerPosition, const float a_DistanceToListenerSquared, bool& bCountIncreased)
{
	bCountIncreased = false;
	if (m_Emitters.Num() < 1) {	return false; }

	if (m_Count < m_Emitters.Num())
	{
		m_Emitters[m_Count] = StaticSoundEmitterComponent;

		if (a_DistanceToListenerSquared > m_MaxDistanceSquared)
		{
			m_MaxDistanceSquared = a_DistanceToListenerSquared;
			m_MaxDistanceIndex = m_Count;
		}

		m_Count++;
		bCountIncreased = true;
		return true;
	}

	if (a_DistanceToListenerSquared < m_MaxDistanceSquared)
	{
		m_Emitters[m_MaxDistanceIndex] = StaticSoundEmitterComponent;
		FindAndUpdateQuadrantMaxDistance(a_StableListenerPosition);
		return true;
	}

	return false;
}

bool FStaticEmitterQuadrant::RemoveFarthestEmitter(FVector StableListenerPosition)
{
	if (m_Count == 0)
	{
		return false;
	}

	if (m_MaxDistanceIndex < m_Count - 1)
	{
		m_Emitters[m_MaxDistanceIndex] = m_Emitters[m_Count - 1];
	}

	m_Emitters[m_Count - 1] = nullptr;
	m_Count--;
	FindAndUpdateQuadrantMaxDistance(StableListenerPosition);

	return true;
}

void FStaticEmitterQuadrant::FindAndUpdateQuadrantMaxDistance(FVector StableListenerPosition)
{
	m_MaxDistanceSquared = 0.f;
	m_MaxDistanceIndex = 0;

	for (int i = 0; i < m_Count; i++)
	{
		const float distanceSquared = FVector::DistSquared(m_Emitters[i]->GetComponentLocation(), StableListenerPosition);

		if (distanceSquared > m_MaxDistanceSquared)
		{
			m_MaxDistanceSquared = distanceSquared;
			m_MaxDistanceIndex = i;
		}
	}
}
#pragma endregion

#pragma region TStaticSoundEmitterOctree
FStaticSoundEmitterOctreeElement::FStaticSoundEmitterOctreeElement(UStaticSoundEmitterComponent* a_StaticSoundEmitterComponent)
	: StaticSoundEmitterComponent(a_StaticSoundEmitterComponent)
	, BoundingBox(FBoxCenterAndExtent(StaticSoundEmitterComponent->GetComponentLocation(), FVector(1.f, 1.f, 1.f)))
{}

void FStaticSoundEmitterOctreeSemantics::SetElementId(FOctree& OctreeOwner, const FStaticSoundEmitterOctreeElement& Element, FOctreeElementId2 Id)
{
	static_cast<TStaticSoundEmitterOctree&>(OctreeOwner).ObjectToOctreeId.Add(Element.StaticSoundEmitterComponent->GetUniqueID(), Id);
}

void TStaticSoundEmitterOctree::AddEmitter(UStaticSoundEmitterComponent* StaticSoundEmitterComponent)
{
	const uint32 emitterID = StaticSoundEmitterComponent->GetUniqueID();
	//if (ObjectToOctreeId.Contains(emitterID) && IsValidElementId(ObjectToOctreeId[emitterID])) { return; }

	AddElement(FStaticSoundEmitterOctreeElement{ StaticSoundEmitterComponent });
	NumElements++;

	if (Private_StaticSoundEmitterManager::bDebugConsoleOctreeStats)
	{ 
		WR_DBG_STATIC_FUNC(Log, "%s_%s - elements: %i", 
			*StaticSoundEmitterComponent->GetAttachParentActor()->GetActorNameOrLabel(),
			*StaticSoundEmitterComponent->GetName(), NumElements);
		//DumpStats();
	}
}

void TStaticSoundEmitterOctree::RemoveEmitter(UStaticSoundEmitterComponent* StaticSoundEmitterComponent)
{
	const FOctreeElementId2 elementID = ObjectToOctreeId[StaticSoundEmitterComponent->GetUniqueID()];
	if (!IsValidElementId(elementID)) { return; }

	RemoveElement(ObjectToOctreeId[StaticSoundEmitterComponent->GetUniqueID()]);
	NumElements--;

	if (Private_StaticSoundEmitterManager::bDebugConsoleOctreeStats)
	{
		WR_DBG_STATIC_FUNC(Log, "%s_%s - elements: %i", 
			*StaticSoundEmitterComponent->GetAttachParentActor()->GetActorNameOrLabel(),
			*StaticSoundEmitterComponent->GetName(), NumElements);
		//DumpStats();
	}
}
#pragma endregion

#pragma region AStaticSoundEmitterWorldManager
AStaticSoundEmitterWorldManager::AStaticSoundEmitterWorldManager()
{
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AStaticSoundEmitterWorldManager::Initialize(USoundListenerManager* SoundListenerManager)
{
	m_listenerManager = SoundListenerManager;

#if !UE_BUILD_SHIPPING
	AStaticSoundEmitterWorldManager::OnDebugViewportStatsChanged.AddUObject(
		this, &AStaticSoundEmitterWorldManager::UpdateDbgNumLoopsAndEmitters);
#endif

	const UWwiserRGameSettings* gameSettings = GetDefault<UWwiserRGameSettings>();
	m_spreadOverMultipleFrames = gameSettings->bSpreadOverMultipleFrames && !gameSettings->DistanceTresholds.IsEmpty();

	if (m_spreadOverMultipleFrames)
	{
		m_rangeTresholds = gameSettings->DistanceTresholds;

		// sort and remove duplicate and <= 0.f values
		m_rangeTresholds.Sort();

		for (int i = m_rangeTresholds.Num() - 1; i > 0; i--)
		{
			if (m_rangeTresholds[i] <= 0.f || m_rangeTresholds[i] == m_rangeTresholds[i - 1])
			{
				m_rangeTresholds.RemoveAt(i);
			}
		}

		if (m_rangeTresholds[0] <= 0.f)
		{
			m_rangeTresholds.RemoveAt(0);
		}


		// initialize distance ranges range tresholds are valid
		if (m_rangeTresholds.IsEmpty())
		{
			m_spreadOverMultipleFrames = false;
		}
		else
		{
			m_numDistanceRanges = m_rangeTresholds.Num() + 1;

			m_rangeTexts.Init(FString{}, m_numDistanceRanges);
			m_rangeTexts[0] = FString::Printf(TEXT("0 - %.0f m"), m_rangeTresholds[0] / 100.f);
			for (int i = 1; i < m_rangeTresholds.Num(); i++)
			{
				m_rangeTexts[i] = FString::Printf(TEXT("%.0f - %.0f m"), m_rangeTresholds[i - 1] / 100.f, m_rangeTresholds[i] / 100.f);
			}
			m_rangeTexts[m_rangeTresholds.Num()] = FString::Printf(TEXT(">= %.0f m"), m_rangeTresholds[m_rangeTresholds.Num() - 1] / 100.f);

			m_rangeTextLengths.Init(0, m_numDistanceRanges);

			for (int j = 0; j < m_numDistanceRanges; j++)
			{
				m_rangeTextLengths[j] = m_rangeTexts[j].Len();
			}
		}
	}

	m_postedLoops.Init(TMap<UDA_StaticSoundLoop*, TSharedPtr<TStaticSoundEmitterOctree>>{}, m_numDistanceRanges);
	m_loopsToPlayPerRange.Init(TMap<UDA_StaticSoundLoop*, FStaticSoundEmittersInRange>{}, m_numDistanceRanges);
	m_playingEventsPerRange.Init(TMap<UDA_StaticSoundLoop*, TSet<UStaticSoundEmitterComponent*>>{}, m_numDistanceRanges);

#if !UE_BUILD_SHIPPING
	m_dbgNumLoops.Init(0, m_numDistanceRanges + 1);
	m_dbgNumPosted.Init(0, m_numDistanceRanges + 1);
	m_dbgNumInRange.Init(0, m_numDistanceRanges + 1);
	m_dbgNumPlaying.Init(0, m_numDistanceRanges + 1);
#endif
}

void AStaticSoundEmitterWorldManager::Deinitialize()
{
#if !UE_BUILD_SHIPPING
	AStaticSoundEmitterWorldManager::OnDebugViewportStatsChanged.RemoveAll(this);
#endif

	m_postedLoops.Empty();
	m_loopsToPlayPerRange.Empty();
	m_playingEventsPerRange.Empty();
	m_listenerManager = nullptr;
}

void AStaticSoundEmitterWorldManager::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Deinitialize();
}

void AStaticSoundEmitterWorldManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const FVector listenerPosition = m_listenerManager->GetSpatialAudioListenerPosition();
	const FVector distanceProbePosition = m_listenerManager->GetDistanceProbePosition();

	if (FVector::DistSquared(m_stableDistanceProbePosition, distanceProbePosition) > s_stableListenerPositionToleratedDistanceSquared)
	{
		m_stableDistanceProbePosition = distanceProbePosition;
	}

#if !UE_BUILD_SHIPPING
	if (Private_StaticSoundEmitterManager::bViewPortStats) { m_dbgNumInRange[m_rangeIndex] = 0; }
#endif

	// reset or reinitialize existing loops instead, minimizing memory reallocation
	for (TPair<UDA_StaticSoundLoop*, FStaticSoundEmittersInRange>& loop : m_loopsToPlayPerRange[m_rangeIndex])
	{
		if (loop.Key->MaxInstances == loop.Value.m_MaxEmittersTotal
			&& loop.Key->MaxInstancesPerQuadrant == loop.Value.m_StaticSoundEmitterQuadrants[0].m_Emitters.Num())
		{
			loop.Value.Clear();
		}
		else
		{
			loop.Value = FStaticSoundEmittersInRange(loop.Key->MaxInstancesPerQuadrant, loop.Key->MaxInstances);
		}
	}

	// convert TMap to TArray for efficient parallel access
	TMap<UDA_StaticSoundLoop*, TSharedPtr<TStaticSoundEmitterOctree>>& PostedLoopsMap = m_postedLoops[m_rangeIndex];
	TArray<TPair<UDA_StaticSoundLoop*, TSharedPtr<TStaticSoundEmitterOctree>>> postedLoopsArray;
	postedLoopsArray.Reserve(PostedLoopsMap.Num());
	for (const auto& postedLoop : PostedLoopsMap)
	{
		postedLoopsArray.Emplace(postedLoop);
	}

	TArray<FStaticSoundEmittersInRange> tempEmittersToPlayArray;
	tempEmittersToPlayArray.SetNum(postedLoopsArray.Num());

	ParallelFor(postedLoopsArray.Num(), [&](int32 Index)
		{
			const auto& pair = postedLoopsArray[Index];
			UDA_StaticSoundLoop* staticSoundLoop = pair.Key;
			const TSharedPtr<TStaticSoundEmitterOctree> emitterOctree = pair.Value;

			WR_ASSERT(emitterOctree->RangeIndex == m_rangeIndex,
				"emitterOctree->RangeIndex (%i) != m_rangeIndex (%i)", emitterOctree->RangeIndex, m_rangeIndex);

			const float activationRange = GetActivationRange(staticSoundLoop);
			const float activationRangeSquared = activationRange * activationRange;
			const FVector referencePosition =
				staticSoundLoop->ReferencePositionLerp * m_stableDistanceProbePosition +
				(1.f - staticSoundLoop->ReferencePositionLerp) * listenerPosition;

			const FBox Box = FBox(referencePosition + FVector(-activationRange),
				referencePosition + FVector(activationRange));

			// local emitters for this loop
			FStaticSoundEmittersInRange localEmittersToPlay(staticSoundLoop->MaxInstancesPerQuadrant, staticSoundLoop->MaxInstances);

			emitterOctree->FindElementsWithBoundsTest(FBoxCenterAndExtent(Box),
				[&](const FStaticSoundEmitterOctreeElement& EmitterElement)
				{
#if !UE_BUILD_SHIPPING
					if (Private_StaticSoundEmitterManager::bViewPortStats)
					{
						FPlatformAtomics::InterlockedIncrement(&m_dbgNumInRange[m_rangeIndex]);
					}
#endif
					const float distanceToListenerSquared = FVector::DistSquared(referencePosition, EmitterElement.BoundingBox.Center);

					if (distanceToListenerSquared < activationRangeSquared)
					{
						// Determine quadrant
						EQuadrant quadrant{};
						FVector emitterLocation = EmitterElement.StaticSoundEmitterComponent->GetComponentLocation();
						if (emitterLocation.Y > referencePosition.Y)
						{
							quadrant = (emitterLocation.X > referencePosition.X) ? EQuadrant::NorthEast : EQuadrant::NorthWest;
						}
						else
						{
							quadrant = (emitterLocation.X > referencePosition.X) ? EQuadrant::SouthEast : EQuadrant::SouthWest;
						}

						// Add emitter to local emitters
						localEmittersToPlay.TryToAddEmitter(EmitterElement.StaticSoundEmitterComponent,
							referencePosition, distanceToListenerSquared, quadrant);
					}
				});

			tempEmittersToPlayArray[Index] = MoveTemp(localEmittersToPlay);
		}); // ParallelFor()

	for (int32 i = 0; i < postedLoopsArray.Num(); ++i)
	{
		UDA_StaticSoundLoop* postedLoop = postedLoopsArray[i].Key;
		m_loopsToPlayPerRange[m_rangeIndex].FindOrAdd(postedLoop) = MoveTemp(tempEmittersToPlayArray[i]);
	}

	PlayAndStopAudioEvents(m_rangeIndex);

#if !UE_BUILD_SHIPPING
	Debug();
#endif

	if (m_spreadOverMultipleFrames)
	{
		m_rangeIndex = (m_rangeIndex + 1) % m_numDistanceRanges;
	}
}

#if !UE_BUILD_SHIPPING
void AStaticSoundEmitterWorldManager::Debug()
{
	if (Private_StaticSoundEmitterManager::bDebugDraw)
	{
		const FThemeStaticSoundEmitters& themeSettings = GetDefault<UWwiserRThemeSettings>()->ThemeStaticSoundEmitters;

		UWorld* world = GetWorld();
		for (int i = 0; i < 3; i++)
		{
			for (const TPair<UDA_StaticSoundLoop*, TSet<UStaticSoundEmitterComponent*>>& playingEvents : m_playingEventsPerRange[i])
			{
				for (const UStaticSoundEmitterComponent* playingEmitter : playingEvents.Value)
				{
					DrawDebugSphere(world, playingEmitter->GetComponentLocation(),
						themeSettings.Radius, 12, themeSettings.StaticSoundEmitterColor);
				}
			}
		}
	}

	if (Private_StaticSoundEmitterManager::bViewPortStats && GEngine)
	{
		// whitespace
		static const FString msgWhiteSpaceTop = TEXT("\n\n\n\n");
		
		// labels
		static const FString msgLabels = TEXT("\n\nRange                             Loops                     Posted            In Range          Playing");

		// accumulate emitters in range stats
		m_dbgNumInRange[m_numDistanceRanges] = 0;
		for (int i = 0; i < m_dbgNumInRange.Num(); i++)
		{
			m_dbgNumInRange[m_numDistanceRanges] += m_dbgNumInRange[i];
		}

		// ranged stats
		TArray<FString> msgRangedStatsArray{};
		msgRangedStatsArray.Init(FString{}, m_numDistanceRanges);

		for (int i = 0; i < m_numDistanceRanges; i++)
		{
			// posted events
			msgRangedStatsArray[i] = m_rangeTexts[i];
			uint8 currentLength = msgRangedStatsArray[i].Len();

			if (m_spreadOverMultipleFrames)
			{
				for (int j = 0; j < 24 - 2 * (m_rangeTextLengths[i] - m_rangeTextLengths[0]) - i; j++) // very ugly, but Unreal discards .AppendChars() etc...
				{
					msgRangedStatsArray[i].Append(" ");
				}
				msgRangedStatsArray[i].Append(FString::Printf(TEXT("%8i"), m_dbgNumLoops[i]));

				// posted emitters
				AddSpacing(msgRangedStatsArray[i], m_dbgNumLoops[i], 47);
				msgRangedStatsArray[i].Append(FString::Printf(TEXT("%8i"), m_dbgNumPosted[i]));

				// emitters in range
				AddSpacing(msgRangedStatsArray[i], m_dbgNumPosted[i], 59);
				msgRangedStatsArray[i].Append(FString::Printf(TEXT("%8i"), m_dbgNumInRange[i]));

				// playing emitters
				AddSpacing(msgRangedStatsArray[i], m_dbgNumInRange[i], 69);
				msgRangedStatsArray[i].Append(FString::Printf(TEXT("%8i\n"), m_dbgNumPlaying[i]));
			}
		}

		FString msgRangedStats{};
		for (int i = 0; i < m_numDistanceRanges; i++)
		{
			msgRangedStats.Append(msgRangedStatsArray[i]);
		}

		// total stats
		FString msgTotalStats{ TEXT("Total:") };

		// total stats - posted events
		uint8 currentLength = msgTotalStats.Len();
		for (int j = 0; j < 34 - currentLength; j++) // very ugly, but Unreal discards .AppendChars() etc...
		{
			msgTotalStats.Append(" ");
		}
		msgTotalStats.Append(FString::Printf(TEXT("%8i"), m_dbgNumLoops[m_numDistanceRanges]));

		// posted emitters
		AddSpacing(msgTotalStats, m_dbgNumLoops[m_numDistanceRanges], 50);
		msgTotalStats.Append(FString::Printf(TEXT("%8i"), m_dbgNumPosted[m_numDistanceRanges]));

		// emitters in range
		AddSpacing(msgTotalStats, m_dbgNumPosted[m_numDistanceRanges], 61);
		msgTotalStats.Append(FString::Printf(TEXT("%8i"), m_dbgNumInRange[m_numDistanceRanges]));

		// playing emitters
		AddSpacing(msgTotalStats, m_dbgNumInRange[m_numDistanceRanges], 71);
		msgTotalStats.Append(FString::Printf(TEXT("%8i\n"), m_dbgNumPlaying[m_numDistanceRanges]));

		GEngine->AddOnScreenDebugMessage(1, 1.f, FColor::White, msgWhiteSpaceTop);
		GEngine->AddOnScreenDebugMessage(2, 1.f, FColor::White, msgLabels);

		if (m_numDistanceRanges > 1)
		{
			GEngine->AddOnScreenDebugMessage(3, 1.f, FColor::Cyan, msgRangedStats);
		}

		GEngine->AddOnScreenDebugMessage(4, 1.f, FColor::Yellow, msgTotalStats);
	}
}

void AStaticSoundEmitterWorldManager::UpdateDbgNumLoopsAndEmitters()
{
	m_dbgNumLoops[m_numDistanceRanges] = 0;
	m_dbgNumPosted[m_numDistanceRanges] = 0;

	for (int i = 0; i < m_numDistanceRanges; i++)
	{
		m_dbgNumLoops[i] = m_postedLoops[i].Num();
		m_dbgNumLoops[m_numDistanceRanges] += m_dbgNumLoops[i];

		m_dbgNumPosted[i] = 0;
		for (TPair<UDA_StaticSoundLoop*, TSharedPtr<TStaticSoundEmitterOctree>> loop : m_postedLoops[i])
		{
			m_dbgNumPosted[i] += loop.Value->NumElements;
		}
		m_dbgNumPosted[m_numDistanceRanges] += m_dbgNumPosted[i];
	}
}
#endif

void AStaticSoundEmitterWorldManager::PlayAndStopAudioEvents(const uint8 a_distanceIndex)
{
	TMap<UDA_StaticSoundLoop*, FStaticSoundEmittersInRange>& eventsToPlay = m_loopsToPlayPerRange[a_distanceIndex];
	TMap<UDA_StaticSoundLoop*, TSet<UStaticSoundEmitterComponent*>>& playingEvents = m_playingEventsPerRange[a_distanceIndex];
	TSet<UDA_StaticSoundLoop*> eventsToRemove = TSet<UDA_StaticSoundLoop*>();
	
	// identify and remove events to stop playing
	for (TPair<UDA_StaticSoundLoop*, TSet<UStaticSoundEmitterComponent*>>& playingEvent : playingEvents)
	{
		WR_ASSERT(IsValid(playingEvent.Key), "!IsValid(playingEvent.Key)");
		
		if (!eventsToPlay.Contains(playingEvent.Key))
		{
			// stop playing all sound emitters for this event
			for (UStaticSoundEmitterComponent* StaticSoundEmitterComponent : playingEvent.Value)
			{
				if (IsValid(StaticSoundEmitterComponent))
				{
					StaticSoundEmitterComponent->StopPlayAudio(playingEvent.Key);
				}
				else
				{
					WR_DBG_FUNC(Warning, "Trying to stop %s on invalid or destroyed StaticSoundEmitterComponent. Current World: %s",
						*playingEvent.Key->GetName(), IsValid(GetWorld()) ? *GetWorld()->GetName() : TEXT("none"));
				}
			}

			eventsToRemove.Emplace(playingEvent.Key);
		}
	}

	for (UDA_StaticSoundLoop* eventToRemove : eventsToRemove)
	{
		playingEvents.Remove(eventToRemove);
	}

	// process the events to play
	for (TPair<UDA_StaticSoundLoop*, FStaticSoundEmittersInRange>& eventToPlay : eventsToPlay)
	{
		WR_ASSERT(IsValid(eventToPlay.Key), "!IsValid(eventToPlay.Key)");

		UDA_StaticSoundLoop* loop = eventToPlay.Key;
		FStaticSoundEmittersInRange& emittersInRange = eventToPlay.Value;

		TSet<UStaticSoundEmitterComponent*>* playingEmittersPtr = playingEvents.Find(loop);

		if (playingEmittersPtr != nullptr)
		{
			TSet<UStaticSoundEmitterComponent*>& playingEmitters = *playingEmittersPtr;

			// gather emitters that need to play
			TSet<UStaticSoundEmitterComponent*> emittersToPlay;
			emittersToPlay.Reserve(emittersInRange.m_Count);

			for (const FStaticEmitterQuadrant& quadrant : eventToPlay.Value.m_StaticSoundEmitterQuadrants)
			{
				emittersToPlay.Append(quadrant.m_Emitters.FilterByPredicate([](UStaticSoundEmitterComponent* emitter)
					{
						return IsValid(emitter);
					}));
			}

			// stop emitters that must no longer play
			TSet<UStaticSoundEmitterComponent*> emittersToRemove;
			for (UStaticSoundEmitterComponent* emitter : playingEmitters)
			{
				if (!emittersToPlay.Contains(emitter))
				{
					emitter->StopPlayAudio(loop);
					emittersToRemove.Emplace(emitter);
				}
			}
			playingEmitters = playingEmitters.Difference(emittersToRemove);

			// start emitters that must begin playing
			for (UStaticSoundEmitterComponent* emitter : emittersToPlay)
			{
				if (!playingEmitters.Contains(emitter))
				{
					emitter->StartPlayAudio(loop);
					playingEmitters.Emplace(emitter);
				}
			}
		}
		else // staticSoundLoop not yet playing
		{
			TSet<UStaticSoundEmitterComponent*>& newEmitters = playingEvents.Emplace(loop);
			newEmitters.Reserve(emittersInRange.m_Count);

			for (const FStaticEmitterQuadrant& quadrant : emittersInRange.m_StaticSoundEmitterQuadrants)
			{
				for (UStaticSoundEmitterComponent* emitter : quadrant.m_Emitters)
				{
					if (IsValid(emitter))
					{
						newEmitters.Emplace(emitter);
						emitter->StartPlayAudio(loop);
					}
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (Private_StaticSoundEmitterManager::bViewPortStats)
	{
		m_dbgNumPlaying[a_distanceIndex] = 0;
		m_dbgNumPlaying[m_numDistanceRanges] = 0;

		for (const auto& playingEvent : playingEvents)
		{
			m_dbgNumPlaying[a_distanceIndex] += playingEvent.Value.Num();
		}

		for (int i = 0; i < m_numDistanceRanges; i++)
		{
			m_dbgNumPlaying[m_numDistanceRanges] += m_dbgNumPlaying[i];
		}
	}
#endif
}


float AStaticSoundEmitterWorldManager::GetActivationRange(const UDA_StaticSoundLoop* StaticSoundLoop) const
{
	return StaticSoundLoop->ActivationRangeOverride > 0.f
		&& StaticSoundLoop->ActivationRangeOverride < StaticSoundLoop->LoopEvent->MaxAttenuationRadius
		? StaticSoundLoop->ActivationRangeOverride : StaticSoundLoop->LoopEvent->MaxAttenuationRadius;
}

void AStaticSoundEmitterWorldManager::PostLoop(
	UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop)
{
	float activationRange;

	if (m_postedLoopRanges.Contains(StaticSoundLoop))
	{
		activationRange = m_postedLoopRanges[StaticSoundLoop];
	}
	else
	{
		activationRange = GetActivationRange(StaticSoundLoop);

		if (activationRange <= 0.f)
		{
			WR_DBG_FUNC(Warning, "no attenuation range set on %s. Loop was not posted", *StaticSoundLoop->LoopEvent->GetName());
			return;
		}

		m_postedLoopRanges.Add(TPair<UDA_StaticSoundLoop*, float>(StaticSoundLoop, activationRange));
	}

	uint8 distRangeIdx = GetDistanceRangeIndex(activationRange);

	if (!m_postedLoops[distRangeIdx].Contains(StaticSoundLoop))
	{
		TSharedPtr<TStaticSoundEmitterOctree> emitterOctree = MakeShared<TStaticSoundEmitterOctree>(distRangeIdx);

		m_postedLoops[distRangeIdx].Add(TPair<UDA_StaticSoundLoop*, TSharedPtr<TStaticSoundEmitterOctree>>(
			StaticSoundLoop, emitterOctree));
	}

	m_postedLoops[distRangeIdx][StaticSoundLoop]->AddEmitter(StaticSoundEmitterComponent);

	if (Private_StaticSoundEmitterManager::bDebugConsole)
	{
		WR_DBG_FUNC(Log, "[%s] posted on %s. Activation Range: %.2f, Range: %s",
			*StaticSoundLoop->LoopEvent->GetName(), *UAudioUtils::GetFullObjectName(StaticSoundEmitterComponent),
			activationRange, *m_rangeTexts[distRangeIdx]);

		if (Private_StaticSoundEmitterManager::bDebugConsoleOctreeStats)
		{
			WR_DBG_FUNC(Log, "Dump Octree Stats - %s", *StaticSoundLoop->GetName());
			m_postedLoops[distRangeIdx][StaticSoundLoop]->DumpStats();
		}
	}

#if !UE_BUILD_SHIPPING
	if (Private_StaticSoundEmitterManager::bViewPortStats) { UpdateDbgNumLoopsAndEmitters(); }
#endif
}

void AStaticSoundEmitterWorldManager::StopLoop(
	UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop)
{
	if (!m_postedLoopRanges.Contains(StaticSoundLoop)) { return; }

	const float activationRange = m_postedLoopRanges[StaticSoundLoop];
	const uint8 distRangeIdx = GetDistanceRangeIndex(activationRange);

	// remove from m_postedLoops
	if (m_postedLoops[distRangeIdx][StaticSoundLoop]->ObjectToOctreeId.Contains(StaticSoundEmitterComponent->GetUniqueID()))
	{
		m_postedLoops[distRangeIdx][StaticSoundLoop]->RemoveEmitter(StaticSoundEmitterComponent);
	}

	if (m_postedLoops[distRangeIdx][StaticSoundLoop]->NumElements <= 0)
	{
		m_postedLoops[distRangeIdx].Remove(StaticSoundLoop);
		m_postedLoopRanges.Remove(StaticSoundLoop);
	}

	// remove from m_playingEventsPerRange
	if (m_playingEventsPerRange[distRangeIdx].Contains(StaticSoundLoop)
		&& m_playingEventsPerRange[distRangeIdx][StaticSoundLoop].Contains(StaticSoundEmitterComponent))
	{
		m_playingEventsPerRange[distRangeIdx][StaticSoundLoop].Remove(StaticSoundEmitterComponent);

		if (m_playingEventsPerRange[distRangeIdx][StaticSoundLoop].IsEmpty())
		{
			const bool wasRemoved = m_playingEventsPerRange[distRangeIdx].Remove(StaticSoundLoop) > 0;
		}
	}

	// remove from m_loopsToPlayPerRange
	if (m_loopsToPlayPerRange[distRangeIdx].Contains(StaticSoundLoop))
	{
		for (int i = 0; i < 4; i++)
		{
			if (m_loopsToPlayPerRange[distRangeIdx][StaticSoundLoop].m_StaticSoundEmitterQuadrants[i].m_Emitters.Contains(StaticSoundEmitterComponent))
			{
				m_loopsToPlayPerRange[distRangeIdx][StaticSoundLoop].m_StaticSoundEmitterQuadrants[i].m_Emitters.Remove(
					StaticSoundEmitterComponent);
				m_loopsToPlayPerRange[distRangeIdx][StaticSoundLoop].m_StaticSoundEmitterQuadrants[i].m_Count--;
				m_loopsToPlayPerRange[distRangeIdx][StaticSoundLoop].m_Count--;

				break;
			}
		}

		if (m_loopsToPlayPerRange[distRangeIdx][StaticSoundLoop].m_Count == 0)
		{
			m_loopsToPlayPerRange[distRangeIdx].Remove(StaticSoundLoop);
		}
	}

	if (Private_StaticSoundEmitterManager::bDebugConsole)
	{
		WR_DBG_FUNC(Log, "[%s] stopped on %s. Activation Range: %.2f, Range: %s",
			*StaticSoundLoop->LoopEvent->GetName(), *UAudioUtils::GetFullObjectName(StaticSoundEmitterComponent),
			activationRange, *m_rangeTexts[distRangeIdx]);

		if (Private_StaticSoundEmitterManager::bDebugConsoleOctreeStats)
		{
			WR_DBG_FUNC(Log, "Dump Octree Stats - %s", *StaticSoundLoop->GetName());
			m_postedLoops[distRangeIdx][StaticSoundLoop]->DumpStats();
		}
	}

#if !UE_BUILD_SHIPPING
	if (Private_StaticSoundEmitterManager::bViewPortStats) { UpdateDbgNumLoopsAndEmitters(); }
#endif
}
#pragma endregion

#pragma region UStaticSoundEmitterManager
void UStaticSoundEmitterManager::Initialize(USoundListenerManager* SoundListenerManager)
{
	m_listenerManager = SoundListenerManager;
	FWorldDelegates::OnPostWorldCleanup.AddUObject(this, &UStaticSoundEmitterManager::OnPostWorldCleanup);

	WR_DBG_NET(Log, "initialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}

void UStaticSoundEmitterManager::Deinitialize()
{
	FWorldDelegates::OnPostWorldCleanup.RemoveAll(this);

	for (TPair<UWorld*, AStaticSoundEmitterWorldManager*> worldManager : m_worldManagers)
	{
		if (IsValid(worldManager.Value))
		{
			worldManager.Value->Deinitialize();
		}
	}

	m_worldManagers.Empty();
	m_listenerManager = nullptr;

	WR_DBG_NET(Log, "deinitialized (%s)", *UAudioUtils::GetClientOrServerString(GetWorld()));
}

void UStaticSoundEmitterManager::PostLoop(UWorld* World,
	UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop)
{
	if (!(IsValid(StaticSoundEmitterComponent) && IsValid(StaticSoundLoop))) { return; }

	if (!IsValid(StaticSoundLoop->LoopEvent))
	{
#if !UE_BUILD_SHIPPING
		if (!m_postedBedsWithoutValidAkEvent.Contains(StaticSoundLoop))
		{
			WR_DBG_FUNC(Warning, "no valid AkAudioEvent assigned in %s. Static sound loop cannot be posted.", *StaticSoundLoop->GetName());
			m_postedBedsWithoutValidAkEvent.Add(StaticSoundLoop);
		}
#endif

		return;
	}

	if (!m_worldManagers.Contains(World))
	{
		m_worldManagers.Add(TPair<UWorld*, AStaticSoundEmitterWorldManager*>
			(World, World->SpawnActor<AStaticSoundEmitterWorldManager>(FActorSpawnParameters())));

		m_worldManagers[World]->Initialize(m_listenerManager);
	}

	m_worldManagers[World]->PostLoop(StaticSoundEmitterComponent, StaticSoundLoop);
}

void UStaticSoundEmitterManager::StopLoop(UWorld* World,
	UStaticSoundEmitterComponent* StaticSoundEmitterComponent, UDA_StaticSoundLoop* StaticSoundLoop)
{
	if (!IsValid(StaticSoundLoop) /* || !IsValid(StaticSoundEmitterComponent)*/) { return; }

	if (m_worldManagers.Contains(World))
	{
		m_worldManagers[World]->StopLoop(StaticSoundEmitterComponent, StaticSoundLoop);
	}
}

void UStaticSoundEmitterManager::OnPostWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	m_worldManagers.Remove(World);
}
#pragma endregion
