// // Copyright Yoerik Roevens. All Rights Reserved.(c)


#include "SoundEmitters/StaticSoundEmitterComponent.h"
#include "Managers/StaticSoundEmitterManager.h"
#include "DataAssets/DA_StaticSoundLoop.h"
#include "Core/AudioSubsystem.h"
#include "Core/AudioUtils.h"
#include "AkComponent.h"
#include "AkAudioEvent.h"

UStaticSoundEmitterComponent::UStaticSoundEmitterComponent()
{
	SetIsReplicated(false);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UStaticSoundEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed || EndPlayReason == EEndPlayReason::RemovedFromWorld)
	{
		UWorld* world = GetWorld();

		if (UStaticSoundEmitterManager* staticSoundEmitterManager = UAudioSubsystem::Get(world)->GetStaticSoundEmitterManager())
		{
			for (UDA_StaticSoundLoop* postedLoop : m_postedLoops)
			{
				staticSoundEmitterManager->StopLoop(world, this, postedLoop);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
void UStaticSoundEmitterComponent::DebugDrawOnTick(UWorld* World)
{
	if (!IsValid(World)) { return; }

	// culled loop event names
	for (const TPair<UAkAudioEvent*, AkPlayingID> Loop : m_playingLoops)
	{
		WR_ASSERT(IsValid(Loop.Key), "invalid AkEvent found in m_playingLoops");
		m_msgEmitterDebug.Append(Loop.Key->GetName().Append("\n"));
	}

	Super::DebugDrawOnTick(World);
}
#endif

void UStaticSoundEmitterComponent::StartPlayAudio(UDA_StaticSoundLoop* StaticSoundLoop)
{
	// already checked in UStaticSoundEmitterManager::PostLoop()
	WR_ASSERT(IsValid(StaticSoundLoop->LoopEvent), "missing Wwise event in %s", *StaticSoundLoop->GetName());

	if (m_playingLoops.Contains(StaticSoundLoop->LoopEvent) && !(m_playingLoops[StaticSoundLoop->LoopEvent] == AK_INVALID_PLAYING_ID))
	{
		WR_DBG_FUNC(Error, "%s already playing on %s", *StaticSoundLoop->LoopEvent->GetName(), *UAudioUtils::GetFullObjectName(this));
		return;
	}

	CreateAkComponentIfNeeded();

	const AkPlayingID playingID = m_AkComp->PostAkEvent(StaticSoundLoop->LoopEvent);

	if (playingID != AK_INVALID_PLAYING_ID)
	{
		OnPlayingStateChanged.Broadcast(true, StaticSoundLoop);
		m_playingLoops.Emplace(StaticSoundLoop->LoopEvent, playingID);
		
		if (s_debugToConsole && s_logEvents)
		{
			WR_DBG_FUNC(Log, "AkEvent posted : %s - PlayingID = %i", *StaticSoundLoop->LoopEvent->GetName(), playingID);
		}
	}
	else
	{
		WR_DBG_FUNC(Warning, "failed to post Wwise event [%s]", *StaticSoundLoop->LoopEvent->GetName());
	}
}

void UStaticSoundEmitterComponent::StopPlayAudio(UDA_StaticSoundLoop* StaticSoundLoop)
{
	// already checked in UStaticSoundEmitterManager::PostLoop()
	WR_ASSERT(IsValid(StaticSoundLoop->LoopEvent), "missing Wwise event in %s", *StaticSoundLoop->GetName());
	WR_ASSERT(m_playingLoops.Contains(StaticSoundLoop->LoopEvent), "AkEvent wasn't playing")

	if (FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get())
	{
		AkAudioDevice->StopPlayingID(m_playingLoops[StaticSoundLoop->LoopEvent],
			StaticSoundLoop->FadeOutTimeInMs, (AkCurveInterpolation)StaticSoundLoop->FadeOutCurve);
	}

	OnPlayingStateChanged.Broadcast(false, StaticSoundLoop);
	m_playingLoops.Remove(StaticSoundLoop->LoopEvent);

	if (s_debugToConsole && s_logEvents)
	{
		WR_DBG_FUNC(Log, "AkEvent stopped : %s - PlayingID = %i",
			*StaticSoundLoop->LoopEvent->GetName(), m_playingLoops[StaticSoundLoop->LoopEvent]);
	}
}

void UStaticSoundEmitterComponent::PostStaticSoundLoop(UDA_StaticSoundLoop* StaticSoundLoop)
{
	if (IsValid(StaticSoundLoop) && IsValid(StaticSoundLoop->LoopEvent))
	{
		UWorld* world = GetWorld();

		if (UStaticSoundEmitterManager* staticSoundEmitterManager = UAudioSubsystem::Get(world)->GetStaticSoundEmitterManager())
		{
			staticSoundEmitterManager->PostLoop(world, this, StaticSoundLoop);
			m_postedLoops.Add(StaticSoundLoop);
		}
	}
}

void UStaticSoundEmitterComponent::StopStaticSoundLoop(UDA_StaticSoundLoop* StaticSoundLoop)
{
	if (!m_postedLoops.Contains(StaticSoundLoop)) { return; }

	UWorld* world = GetWorld();

	if (UStaticSoundEmitterManager* staticSoundEmitterManager = UAudioSubsystem::Get(world)->GetStaticSoundEmitterManager())
	{
		staticSoundEmitterManager->StopLoop(world, this, StaticSoundLoop);
	}

	m_postedLoops.Remove(StaticSoundLoop);

	if (m_playingLoops.Contains(StaticSoundLoop->LoopEvent))
	{
		StopPlayAudio(StaticSoundLoop);
	}
}

bool UStaticSoundEmitterComponent::HasActiveEvents() const
{
	return !m_playingLoops.IsEmpty() || Super::HasActiveEvents();
}
