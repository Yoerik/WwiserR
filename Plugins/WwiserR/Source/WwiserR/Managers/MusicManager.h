// Copyright Yoerik Roevens. All Rights Reserved.(c)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AkGameplayTypes.h"
#include "Core/AudioUtils.h"
#include "MusicManager.generated.h"

/**
 * Music Manager
 * *************
 * - handles starting/stopping the global music event (from the very start of the game)
 * - additional logic might be added later if a more complex music system is desired
 **/
UCLASS(Abstract, ClassGroup = "WwiserR", Blueprintable)
class WWISERR_API UMusicManager : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Music Manager")
	class UAkGameObject* GlobalMusicSoundEmitter;

	UPROPERTY(EditDefaultsOnly, Category = "Music Manager")
	class UAkAudioEvent* MainMusicEvent;

	UPROPERTY(EditDefaultsOnly, Category = "Music Manager")
	TArray <class UAkStateValue*> InitialMusicStates;

	UPROPERTY(EditDefaultsOnly, Category = "Music Manager")
	TArray <class UAkSwitchValue*> InitialMusicSwitches;

	UPROPERTY(EditDefaultsOnly, Category = "Music Manager")
	UAkAudioEvent* MuteMusicEvent;

	UPROPERTY(EditDefaultsOnly, Category = "Music Manager")
	UAkAudioEvent* UnmuteMusicEvent;

protected:
	AkPlayingID MainMusicPlayingID = AK_INVALID_PLAYING_ID;

public:
	void Initialize(class UAkGameObject* GlobalMusicSoundEmitter);
	void Deinitialize();

	UFUNCTION()
	void OnMuteMusicCVarChanged();

	UFUNCTION()
	void OnMainMusicCallback(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Music Manager")
	UPARAM(DisplayName = "Music PlayingID") int32 MainMusicStartIfNotPlaying();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Music Manager")
	UPARAM(DisplayName = "Music Stopped") bool MainMusicStop();

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "WwiserR|Music Manager")
	void MuteMusic(bool bMute);
};
