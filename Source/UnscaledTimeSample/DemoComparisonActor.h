#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "DemoComparisonActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UTextRenderComponent;
class UUnscaledTickComponent;

UCLASS()
class UNSCALEDTIMESAMPLE_API ADemoComparisonActor : public AActor
{
	GENERATED_BODY()

public:
	ADemoComparisonActor();

	/**
	 * Rotates the vanilla side using the world's scaled tick delta.
	 * HandleUnscaledTick rotates the paired side from real delta to make the A/B contrast visible.
	 */
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

	// Off by default so the interactive WBP_DemoHUD owns dilation/pause;
	// enable per-instance to run the 15s automated dilation->pause->resume showcase.
	UPROPERTY(EditAnywhere, Category="UnscaledTime Demo")
	bool bAutoRunDemoCycle = false;

private:
	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	TObjectPtr<UTextRenderComponent> VanillaCounterText;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	TObjectPtr<UTextRenderComponent> UnscaledCounterText;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	TObjectPtr<UTextRenderComponent> PhaseText;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	TObjectPtr<UStaticMeshComponent> VanillaCube;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	TObjectPtr<UStaticMeshComponent> UnscaledCube;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	TObjectPtr<UUnscaledTickComponent> UnscaledTicker;

	FTimerHandle VanillaTimerHandle;
	FTimerHandle UnscaledTimerHandle;
	FTimerHandle DilationPhaseTimerHandle;
	FTimerHandle PausedPhaseTimerHandle;
	FTimerHandle ResumePhaseTimerHandle;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	int32 VanillaCount = 0;

	UPROPERTY(VisibleAnywhere, Category="UnscaledTime Demo")
	int32 UnscaledCount = 0;

	void HandleVanillaTimer();
	void HandleUnscaledTimer();
	void StartDilationPhase();
	void StartPausedPhase();
	void FinishDemoCycle();
	void UpdateCounterText() const;

	UFUNCTION()
	void HandleUnscaledTick(float RealDeltaSeconds);
};
