#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnscaledTimeTypes.h"
#include "UnscaledTickComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnscaledTickSignature, float, RealDeltaSeconds);

UCLASS(ClassGroup=(UnscaledTime), meta=(BlueprintSpawnableComponent))
class UNSCALEDTIME_API UUnscaledTickComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Creates an unscaled tick component that relies on the subsystem delegate instead of ActorComponent ticking. */
	UUnscaledTickComponent();

	/** Broadcasts once per unscaled subsystem tick. */
	UPROPERTY(BlueprintAssignable, Category="UnscaledTime")
	FOnUnscaledTickSignature OnUnscaledTick;

	/** If true, broadcasts while paused; otherwise broadcasts only while the world is unpaused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UnscaledTime")
	bool bTickWhilePaused = true;

	virtual void BeginPlay() override;
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual void OnUnregister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	/** Active subsystem delegate subscription, used to prevent duplicate registration and remove the exact binding. */
	FDelegateHandle TickDelegateHandle;

	/** Clock used by the active subscription, cached so teardown removes from the same delegate even if settings changed. */
	EUnscaledTimeClock RegisteredClock = EUnscaledTimeClock::RealTime;

	/** Subscribes to the selected unscaled tick delegate when the component is active and a subsystem exists. */
	void RegisterTickDelegate();

	/** Clears any active unscaled tick delegate subscription. */
	void UnregisterTickDelegate();

	/** Relays subsystem tick deltas to Blueprint listeners. */
	void HandleUnscaledTick(float RealDeltaSeconds);
};
