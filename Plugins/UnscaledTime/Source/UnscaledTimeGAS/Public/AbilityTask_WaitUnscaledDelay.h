#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Engine/TimerHandle.h"
#include "UnscaledTimeTypes.h"
#include "AbilityTask_WaitUnscaledDelay.generated.h"

#define UE_API UNSCALEDTIMEGAS_API

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitUnscaledDelayDelegate);

UCLASS(MinimalAPI)
class UAbilityTask_WaitUnscaledDelay : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	/** Called when the unscaled delay finishes. */
	UPROPERTY(BlueprintAssignable)
	FWaitUnscaledDelayDelegate OnFinish;

	UE_API virtual void Activate() override;

	/** Returns timing state for GAS debug output. */
	UE_API virtual FString GetDebugString() const override;

	/** Waits for the specified duration using unscaled time when available. If bTickWhilePaused is true, the delay continues while the world is paused. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="TRUE"))
	static UE_API UAbilityTask_WaitUnscaledDelay* WaitUnscaledDelay(UGameplayAbility* OwningAbility, float Time, bool bTickWhilePaused = true);

protected:
	UE_API virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void OnTimeFinish();

	/** Requested delay duration in seconds after GAS applies its global duration scaler. */
	float Time;

	/** Clock timestamp used to compute debug time remaining. */
	double TimeStarted;

	/** Selects whether the unscaled delay uses the real-time or unpaused clock. */
	bool bTickWhilePaused;

	/** True when TimerHandle belongs to the unscaled subsystem; false when it belongs to the world timer fallback. */
	bool bUsingUnscaledTimer;

	/** Clock used by the active unscaled timer, cached for teardown and debug calculations. */
	EUnscaledTimeClock RegisteredClock;

	/** Timer handle owned either by the unscaled timer manager or the world timer manager. */
	FTimerHandle TimerHandle;
};

#undef UE_API
