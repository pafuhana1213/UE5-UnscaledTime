#pragma once

// Note: no WITH_DEV_AUTOMATION_TESTS guard here — UHT does not allow reflection
// macros (UCLASS/UPROPERTY/UFUNCTION) inside preprocessor blocks.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnscaledTimeTestObjects.generated.h"

UCLASS()
class UUnscaledTimeTestObject : public UObject
{
	GENERATED_BODY()

public:
	void Init(UWorld* InWorld);

	virtual UWorld* GetWorld() const override;

	UPROPERTY()
	int32 FireCount = 0;

	UPROPERTY()
	int32 LatentResumeCount = 0;

	UPROPERTY()
	int32 LastLatentLinkage = INDEX_NONE;

	UPROPERTY()
	int32 TickCount = 0;

	UPROPERTY()
	float AccumulatedRealDeltaSeconds = 0.f;

	UPROPERTY()
	float LastTickDeltaSeconds = 0.f;

	UFUNCTION()
	void HandleTimerFired();

	UFUNCTION()
	void LatentResume(int32 Linkage);

	UFUNCTION()
	void HandleUnscaledTick(float RealDeltaSeconds);

private:
	TWeakObjectPtr<UWorld> TestWorld;
};
