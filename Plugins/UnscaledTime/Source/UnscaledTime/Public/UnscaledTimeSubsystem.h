#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "UnscaledTimeTypes.h"
#include "UnscaledTimeSubsystem.generated.h"

/**
 * Drives unscaled timers, latent delays, and tick delegates for a world using real delta time.
 * It maintains separate real-time clocks for work that continues while paused and work that stops while paused.
 */
UCLASS()
class UNSCALEDTIME_API UUnscaledTimeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Broadcasts once per unscaled subsystem tick with the real delta seconds used by the selected clock. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FUnscaledTickDelegate, float /*RealDeltaSeconds*/);

	/** Returns the UnscaledTime subsystem for the world that owns the context object, or nullptr if none is available. */
	static UUnscaledTimeSubsystem* Get(const UObject* WorldContextObject);

	/** Creates the independent timer managers used by the two unscaled clocks. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Clears pending delay state and releases the subsystem-owned timer managers. */
	virtual void Deinitialize() override;

	// 対応範囲を Game / PIE の world lifecycle に限定し、editor などの非実行 world に
	// runtime 用の unscaled timer / tick 前提を持ち込まない。
	/** Enables the subsystem only for runtime game worlds and PIE worlds. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// pause 中も RealTime clock を tick し続けるため、Subsystem 自身は
	// world pause で止まらない tickable として登録しておく必要がある。
	/** Allows the subsystem tick to run while the world is paused. */
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Returns the stat id used for subsystem tick profiling. */
	virtual TStatId GetStatId() const override;

	/** Advances the unscaled clocks and dispatches due timers and tick delegates. */
	virtual void Tick(float DeltaTime) override;

	/** Returns the timer manager that owns timers for the requested unscaled clock. */
	FTimerManager& GetTimerManager(EUnscaledTimeClock Clock = EUnscaledTimeClock::RealTime);

	/** Registers or refreshes a latent Blueprint delay driven by an unscaled clock. */
	void RegisterUnscaledDelay(float DurationSeconds, bool bRetriggerable, bool bTickWhilePaused, const FLatentActionInfo& LatentInfo);

	/** Returns the accumulated real time for the requested unscaled clock. */
	double GetUnscaledTimeSeconds(EUnscaledTimeClock Clock) const;

	/** Returns the last real delta seconds consumed by the subsystem tick. */
	float GetLastRealDeltaSeconds() const;

	/** Returns the number of latent delays currently waiting on subsystem timers. */
	int32 GetPendingDelayCount() const;

	/** Returns the multicast tick delegate for the requested unscaled clock. */
	FUnscaledTickDelegate& GetOnUnscaledTick(EUnscaledTimeClock Clock);
#if !UE_BUILD_SHIPPING
	/** Logs debug information about the subsystem state in non-shipping builds. */
	void DumpDebugInfo() const;
#endif

private:
	// latent UUID は callback target と組で重複判定する。FWeakObjectPtr の
	// index/serial 比較なら、GC 済み object を再解決せず別 object との衝突を避けられる。
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

	// pending delay は timer 満了後に ProcessEvent で latent resume point を
	// 呼ぶために必要な execution link と target を保持する。
	struct FPendingUnscaledDelay
	{
		FTimerHandle TimerHandle;
		EUnscaledTimeClock Clock = EUnscaledTimeClock::RealTime;
		FName ExecutionFunction;
		int32 Linkage = INDEX_NONE;
		TWeakObjectPtr<UObject> CallbackTarget;
	};

	// FTimerManager は timer 単位の pause policy を持たないため、
	// RealTime / Unpaused の選択は manager 単位で分ける。
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
