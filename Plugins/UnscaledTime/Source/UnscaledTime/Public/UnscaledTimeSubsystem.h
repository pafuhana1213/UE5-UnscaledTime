#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "UnscaledTimeTypes.h"
#include "UnscaledTimeSubsystem.generated.h"

UCLASS()
class UNSCALEDTIME_API UUnscaledTimeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FUnscaledTickDelegate, float /*RealDeltaSeconds*/);

	static UUnscaledTimeSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;

	FTimerManager& GetTimerManager(EUnscaledTimeClock Clock = EUnscaledTimeClock::RealTime);
	void RegisterUnscaledDelay(float DurationSeconds, bool bRetriggerable, bool bTickWhilePaused, const FLatentActionInfo& LatentInfo);
	double GetUnscaledTimeSeconds(EUnscaledTimeClock Clock) const;
	float GetLastRealDeltaSeconds() const;
	int32 GetPendingDelayCount() const;
	FUnscaledTickDelegate& GetOnUnscaledTick(EUnscaledTimeClock Clock);
#if !UE_BUILD_SHIPPING
	void DumpDebugInfo() const;
#endif

private:
	struct FUnscaledDelayKey
	{
		FWeakObjectPtr CallbackTarget;
		int32 UUID = INDEX_NONE;

		bool operator==(const FUnscaledDelayKey& Other) const
		{
			return CallbackTarget.HasSameIndexAndSerialNumber(Other.CallbackTarget) && UUID == Other.UUID;
		}

		friend uint32 GetTypeHash(const FUnscaledDelayKey& Key)
		{
			return HashCombine(GetTypeHash(Key.CallbackTarget), ::GetTypeHash(Key.UUID));
		}
	};

	struct FPendingUnscaledDelay
	{
		FTimerHandle TimerHandle;
		EUnscaledTimeClock Clock = EUnscaledTimeClock::RealTime;
		FName ExecutionFunction;
		int32 Linkage = INDEX_NONE;
		TWeakObjectPtr<UObject> CallbackTarget;
	};

	TUniquePtr<FTimerManager> RealTimeManager;
	TUniquePtr<FTimerManager> UnpausedManager;
	TMap<FUnscaledDelayKey, FPendingUnscaledDelay> PendingDelays;

	double AccumulatedRealTime = 0.0;
	double AccumulatedUnpausedRealTime = 0.0;
	float LastRealDelta = 0.0f;

	FUnscaledTickDelegate OnTickRealTime;
	FUnscaledTickDelegate OnTickUnpaused;

	void OnDelayExpired(FUnscaledDelayKey Key);
};
