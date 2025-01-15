// Copyright Yoerik Roevens. All Rights Reserved.(c)

#include "AudioUtils.h"
#include "AkComponent.h"

DEFINE_LOG_CATEGORY(LogWwiserR);

bool UAudioUtils::IsServer(const UWorld* world)
{
    return world != nullptr && world->GetNetMode() != NM_Client;
}

bool UAudioUtils::IsClient(const UWorld* world)
{
    return world != nullptr && world->GetNetMode() != NM_DedicatedServer;
}

FString UAudioUtils::GetClientOrServerString(const UWorld* world)
{
	if (!IsValid(world))
	{
		static const FString msgClient = FString(TEXT("undefined"));
		return msgClient;
	}

	static const FString msgClient = FString(TEXT("client"));
	static const FString msgServer = FString(TEXT("server"));

	return IsClient(world) ? msgClient : msgServer;
}

void UAudioUtils::DrawDebugGizmo(UWorld* world, const FVector& Origin, const FRotator& Rotation, float Size)
{
	const FQuat rotQuat = Rotation.Quaternion();

	DrawDebugArrow(world, Origin, rotQuat.GetAxisX(), Size, FColor::Red);
	DrawDebugArrow(world, Origin, rotQuat.GetAxisY(), Size, FColor::Green);
	DrawDebugArrow(world, Origin, rotQuat.GetAxisZ(), Size, FColor::Blue);
}

void UAudioUtils::DrawDebugArrow(UWorld* world, const FVector& Start, const FVector& Direction, float Length, FColor Color)
{
	const FVector End = Start + Length * Direction;

	DrawDebugLine(world, Start, End, Color);
	DrawDebugCone(world, End, -Direction, Length / 10.f, 0.25f, .25f, 6, Color);
}
