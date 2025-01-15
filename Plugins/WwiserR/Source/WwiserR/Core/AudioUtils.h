// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "AK/SoundEngine/Common/AkTypes.h"
#include "AudioUtils.generated.h"

#pragma region Log Macros

DECLARE_LOG_CATEGORY_EXTERN(LogWwiserR, Log, All);

namespace WR_Log
{
#define NETMODE_WORLD \
	(GEngine == nullptr || GetWorld() == nullptr) ? TEXT("") \
	: (GEngine->GetNetMode(GetWorld()) == NM_Client) ? TEXT("[Client] ") \
	: (GEngine->GetNetMode(GetWorld()) == NM_ListenServer) ? TEXT("[ListenServer] ") \
	: (GEngine->GetNetMode(GetWorld()) == NM_DedicatedServer) ? TEXT("[DedicatedServer] ") \
	: TEXT("[Standalone] ")

#define NETMODE_STATIC \
	(GEngine == nullptr || GEngine->GetWorld() == nullptr) ? TEXT("") \
	: (GEngine->GetNetMode(GEngine->GetWorld()) == NM_Client) ? TEXT("[Client] ") \
	: (GEngine->GetNetMode(GEngine->GetWorld()) == NM_ListenServer) ? TEXT("[ListenServer] ") \
	: (GEngine->GetNetMode(GEngine->GetWorld()) == NM_DedicatedServer) ? TEXT("[DedicatedServer] ") \
	: TEXT("[Standalone] ")

#define FUNC_NAME *FString(__FUNCTION__)
} // WR_Log


#define WR_DBG_SHORT(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s"), *Msg); \
}

#define WR_DBG(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s : %s"), *UAudioUtils::GetFullObjectName(this), *Msg); \
}

#define WR_DBG_FUNC(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s - %s() : %s"), *UAudioUtils::GetFullObjectName(this), FUNC_NAME, *Msg); \
}

#define WR_DBG_NET(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s%s : %s"), NETMODE_WORLD, *UAudioUtils::GetFullObjectName(this), *Msg); \
}

#define WR_DBG_NET_FUNC(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s%s - %s() : %s"), NETMODE_WORLD, *UAudioUtils::GetFullObjectName(this), FUNC_NAME, *Msg); \
}

#define WR_DBG_STATIC(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	const FString className = FString(StaticClass()->GetPrefixCPP()).Append(StaticClass()->GetName()); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s : %s"), *className, *Msg); \
}

#define WR_DBG_STATIC_NET(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	const FString className = FString(StaticClass()->GetPrefixCPP()).Append(StaticClass()->GetName()); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s%s : %s"), NETMODE_STATIC, *className, *Msg); \
}

#define WR_DBG_STATIC_FUNC(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s() : %s"), FUNC_NAME, *Msg); \
}

#define WR_DBG_STATIC_NET_FUNC(Verbosity, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	UE_LOG(LogWwiserR, Verbosity, TEXT("%s%s() : %s"), NETMODE_STATIC, FUNC_NAME, *Msg); \
}

#define WR_ASSERT(Expr, Format, ...) \
{ \
	const FString Msg = FString::Printf(TEXT(Format), ##__VA_ARGS__); \
	checkf(Expr, TEXT("%s() : %s"), FUNC_NAME, *Msg); \
}
#pragma endregion

UENUM(BlueprintType, Category = "WwiserR")
enum class EFoot : uint8
{
	Left,
	Right
};

// keeps track of Rtpcs posted on sound emitters
USTRUCT() struct WWISERR_API FActiveRtpc
{
	GENERATED_BODY()

public:
	class UAkRtpc*	Rtpc{};
	float			Value{};

public:
	FActiveRtpc() {}
	FActiveRtpc(UAkRtpc* in_Rtpc, float in_Value) : Rtpc(in_Rtpc), Value(in_Value) {}

};

FORCEINLINE uint32 GetTypeHash(const FActiveRtpc& ActiveRtpc)
{
	uint32 Hash = FCrc::MemCrc32(&ActiveRtpc, sizeof(ActiveRtpc));
	return Hash;
}

inline bool operator==(const FActiveRtpc& a, const FActiveRtpc& b)
{
	return GetTypeHash(a) == GetTypeHash(b);
}

// keeps track of all looping events posted on sound emitters
USTRUCT()
struct WWISERR_API FPlayingAudioLoop
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	class UAkAudioEvent*		AkEvent{};

	AkPlayingID					InitialPlayingID{};
	AkPlayingID					LastPlayingID{};
	float						AttenuationRangeBuffer{};
	bool						bQueryAndPostEnvironmentSwitches{};
	TMap<class UAkRtpc*, float>	RtpcsOnPlayingID{};

	bool	bIsVirtual{};
	float	NextCullTime{};

public:
	FPlayingAudioLoop(UAkAudioEvent* a_AkEvent, bool a_bQueryAndPostEnvironmentSwitches, AkPlayingID a_PlayingID, bool a_bIsVirtual,
		float a_AttenuationRangeBuffer, float a_NextCullTime)
		: AkEvent(a_AkEvent)
		, LastPlayingID(a_PlayingID)
		, AttenuationRangeBuffer(a_AttenuationRangeBuffer)
		, bQueryAndPostEnvironmentSwitches(a_bQueryAndPostEnvironmentSwitches)
		, bIsVirtual(a_bIsVirtual)
		, NextCullTime(a_NextCullTime)
	{
	}

	FPlayingAudioLoop() {}
};

// WwiserR - helper functions
UCLASS(ClassGroup = "WwiserR") class WWISERR_API UAudioUtils : public UObject
{
	GENERATED_BODY()

public:
	static bool IsServer(const UWorld* world);
	static bool IsClient(const UWorld* world);
	static FString GetClientOrServerString(const UWorld* world);

	FORCEINLINE static FString GetFullObjectName(UObject* object)
	{
		FString objectName{};

		if (object->IsA<UActorComponent>())
		{
			if (AActor* owner = Cast<UActorComponent>(object)->GetOwner())
			{
				objectName = owner->GetActorNameOrLabel();
				objectName.Append(TEXT("->"));
			}
		}

		return objectName.Append(object->GetName());
	}
	template<typename T>
	FORCEINLINE static void WeakObjectPtrTSetToTSet(const TSet<TWeakObjectPtr<T>>& WeakObjectPtrSet, TSet<T*>& Set)
	{
		Set.Empty();

		for (TWeakObjectPtr<T> element : WeakObjectPtrSet)
		{
			Set.Emplace(element.Get());
		}
	}

	template<typename T>
	FORCEINLINE static void WeakObjectPtrSetToArray(const TSet<TWeakObjectPtr<T>>& WeakObjectPtrSet, TArray<T*>& Array)
	{
		TSet<T> newSet{};
		WeakObjectPtrTSetToTSet(WeakObjectPtrSet, newSet);
		Array = newSet.Array();

		/*Array.Init(nullptr, AkComponentSet.Num());

		int i = 0;
		for (TWeakObjectPtr<UAkComponent> comp : AkComponentSet)
		{
			Array[i] = comp.Get();
			i++;
		}*/
	}

	template<typename T>
	FORCEINLINE static bool AreTSetsEqual(const TSet<T>& A, const TSet<T>& B)
	{
		if (A.Num() != B.Num()) { return false; }

		for (const T& element : A)
		{
			if (!B.Contains(element)) { return false; }
		}

		for (const T& element : B)
		{
			if (!A.Contains(element)) { return false; }
		}

		return true;
	}

	static void DrawDebugGizmo(UWorld* world, const FVector& Origin, const FRotator& Rotation, float Size);
	static void DrawDebugArrow(UWorld* world, const FVector& Start, const FVector& Direction, float Length, FColor Color);

	static constexpr char* WorldTypeToString(EWorldType::Type worldtype)
	{
		switch (worldtype)
		{
		case EWorldType::Editor: return "Editor";
		case EWorldType::EditorPreview: return "EditorPreview";
		case EWorldType::Game: return "Game";
		case EWorldType::GamePreview: return "GamePreview";
		case EWorldType::GameRPC: return "GameRPC";
		case EWorldType::Inactive: return "Inactive";
		case EWorldType::None: return "None";
		case EWorldType::PIE: return "PIE";
		}

		return "unknown";
	}

	// interpolate values in [0.f, 1.f] logarithmically (offset to avoid log(0))
	static float LogaritmicInterpolation(float Input, float Base = 3.f, float Offset = 0.001f);
};
