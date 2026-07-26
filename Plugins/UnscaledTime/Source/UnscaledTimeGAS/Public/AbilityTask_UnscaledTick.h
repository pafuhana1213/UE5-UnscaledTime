#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "UnscaledTimeTypes.h"
#include "AbilityTask_UnscaledTick.generated.h"

#define UE_API UNSCALEDTIMEGAS_API

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnscaledTickTaskSignature, float, RealDeltaSeconds);

UCLASS(MinimalAPI)
class UAbilityTask_UnscaledTick : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	/** Broadcasts once per unscaled subsystem tick. */
	UPROPERTY(BlueprintAssignable)
	FOnUnscaledTickTaskSignature OnTick;

	UE_API virtual void Activate() override;

	/** Broadcasts unscaled tick deltas while this ability task is active. If bTickWhilePaused is true, ticks continue while the world is paused. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="TRUE"))
	static UE_API UAbilityTask_UnscaledTick* UnscaledTick(UGameplayAbility* OwningAbility, bool bTickWhilePaused = true);

protected:
	UE_API virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleUnscaledTick(float RealDeltaSeconds);

	/** Active subsystem delegate subscription, used to remove the exact binding on destroy. */
	FDelegateHandle TickDelegateHandle;

	/** Clock used by the active subscription, cached so teardown removes from the same delegate. */
	EUnscaledTimeClock RegisteredClock;

	/** Selects whether the task subscribes to the real-time or unpaused tick stream. */
	bool bTickWhilePaused;
};

#undef UE_API
