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

	/** Broadcast unscaled ticks while this ability task is active. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="TRUE"))
	static UE_API UAbilityTask_UnscaledTick* UnscaledTick(UGameplayAbility* OwningAbility, bool bTickWhilePaused = true);

protected:
	UE_API virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleUnscaledTick(float RealDeltaSeconds);

	FDelegateHandle TickDelegateHandle;
	EUnscaledTimeClock RegisteredClock;
	bool bTickWhilePaused;
};

#undef UE_API
