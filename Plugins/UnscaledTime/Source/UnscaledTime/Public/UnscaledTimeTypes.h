#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UnscaledTimeTypes.generated.h"

UENUM(BlueprintType)
enum class EUnscaledTimeClock : uint8
{
	/** Advances from real delta time even while the world is paused. */
	RealTime UMETA(DisplayName = "Real Time (ticks while paused)"),

	/** Advances from real delta time only while the world is not paused. */
	RealTimeUnpaused UMETA(DisplayName = "Real Time (stops while paused)")
};

// 素の FTimerHandle だけでは、2 本の standalone FTimerManager のどちらに
// 問い合わせるべきか復元できないため、clock を handle と一緒に保持する。
/** Identifies an unscaled timer together with the clock that owns it. */
USTRUCT(BlueprintType)
struct UNSCALEDTIME_API FUnscaledTimerHandle
{
	GENERATED_BODY()

	/** Timer handle stored in the selected unscaled timer manager. */
	UPROPERTY()
	FTimerHandle Handle;

	/** Clock that owns Handle and must be used for future operations on it. */
	UPROPERTY()
	EUnscaledTimeClock Clock = EUnscaledTimeClock::RealTime;

	/** Returns true when the underlying timer handle is valid, regardless of the stored clock. */
	bool IsValid() const
	{
		return Handle.IsValid();
	}

	/** Invalidates the underlying timer handle and resets the stored clock to RealTime. */
	void Invalidate()
	{
		Handle.Invalidate();
		Clock = EUnscaledTimeClock::RealTime;
	}

	/** Compares both the underlying timer handle and its owning clock. */
	bool operator==(const FUnscaledTimerHandle& Other) const
	{
		return Handle == Other.Handle && Clock == Other.Clock;
	}
};
