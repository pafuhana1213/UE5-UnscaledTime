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
	FDelegateHandle TickDelegateHandle;
	EUnscaledTimeClock RegisteredClock = EUnscaledTimeClock::RealTime;

	void RegisterTickDelegate();
	void UnregisterTickDelegate();
	void HandleUnscaledTick(float RealDeltaSeconds);
};
