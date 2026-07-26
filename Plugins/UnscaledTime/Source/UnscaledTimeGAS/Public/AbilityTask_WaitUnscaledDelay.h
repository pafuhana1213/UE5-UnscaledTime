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

	/** Return debug string describing task */
	UE_API virtual FString GetDebugString() const override;

	/** Wait specified unscaled time. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="TRUE"))
	static UE_API UAbilityTask_WaitUnscaledDelay* WaitUnscaledDelay(UGameplayAbility* OwningAbility, float Time, bool bTickWhilePaused = true);

protected:
	UE_API virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void OnTimeFinish();

	float Time;
	double TimeStarted;
	bool bTickWhilePaused;
	bool bUsingUnscaledTimer;
	EUnscaledTimeClock RegisteredClock;
	FTimerHandle TimerHandle;
};

#undef UE_API
