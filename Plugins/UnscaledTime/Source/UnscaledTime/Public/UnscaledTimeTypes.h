#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UnscaledTimeTypes.generated.h"

UENUM(BlueprintType)
enum class EUnscaledTimeClock : uint8
{
	RealTime UMETA(DisplayName = "Real Time (ticks while paused)"),
	RealTimeUnpaused UMETA(DisplayName = "Real Time (stops while paused)")
};

USTRUCT(BlueprintType)
struct UNSCALEDTIME_API FUnscaledTimerHandle
{
	GENERATED_BODY()

	UPROPERTY()
	FTimerHandle Handle;

	UPROPERTY()
	EUnscaledTimeClock Clock = EUnscaledTimeClock::RealTime;

	bool IsValid() const
	{
		return Handle.IsValid();
	}

	void Invalidate()
	{
		Handle.Invalidate();
		Clock = EUnscaledTimeClock::RealTime;
	}

	bool operator==(const FUnscaledTimerHandle& Other) const
	{
		return Handle == Other.Handle && Clock == Other.Clock;
	}
};
