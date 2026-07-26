#include "UnscaledTimeSubsystem.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UnscaledTimeLog.h"

// Debug 専用の member 実装をこの TU に分け、shipping 除外と CVar/console command 登録を同じ #if に閉じ込める。
namespace
{
	TAutoConsoleVariable<int32> CVarUnscaledTimeDebug(
		TEXT("UnscaledTime.Debug"),
		0,
		TEXT("Displays per-world UnscaledTime subsystem state on screen when nonzero."),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarUnscaledTimeSuspendWhileDebugPaused(
		TEXT("UnscaledTime.SuspendWhileDebugPaused"),
		0,
		TEXT("Suspends UnscaledTime clocks while the world is debug-paused when nonzero."),
		ECVF_Cheat);

	const TCHAR* GetUnscaledClockName(EUnscaledTimeClock Clock)
	{
		switch (Clock)
		{
		case EUnscaledTimeClock::RealTimeUnpaused:
			return TEXT("RealTimeUnpaused");

		case EUnscaledTimeClock::RealTime:
		default:
			return TEXT("RealTime");
		}
	}

	void DumpUnscaledTimersCommand(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;

		if (!World)
		{
			UE_LOG(LogUnscaledTime, Warning, TEXT("UnscaledTime.DumpTimers could not resolve a world."));
			return;
		}

		UUnscaledTimeSubsystem* Subsystem = World->GetSubsystem<UUnscaledTimeSubsystem>();
		if (!Subsystem)
		{
			UE_LOG(LogUnscaledTime, Warning, TEXT("UnscaledTime.DumpTimers could not find UUnscaledTimeSubsystem for world %s."), *World->GetName());
			return;
		}

		Subsystem->DumpDebugInfo();
	}

	FAutoConsoleCommandWithWorldAndArgs GDumpUnscaledTimersCommand(
		TEXT("UnscaledTime.DumpTimers"),
		TEXT("Dumps UnscaledTime subsystem clocks, pending latent delays, and both timer managers for the current world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpUnscaledTimersCommand),
		ECVF_Cheat);
}

void UUnscaledTimeSubsystem::DumpDebugInfo() const
{
	UWorld* World = GetWorld();
	UE_LOG(LogUnscaledTime, Log, TEXT("UnscaledTime dump for world %s"), World ? *World->GetName() : TEXT("<null>"));
	UE_LOG(LogUnscaledTime, Log, TEXT("  RealTime=%.6f Unpaused=%.6f LastRealDelta=%.6f PendingDelays=%d Paused=%s"),
		AccumulatedRealTime,
		AccumulatedUnpausedRealTime,
		LastRealDelta,
		PendingDelays.Num(),
		World && World->IsPaused() ? TEXT("true") : TEXT("false"));

	if (PendingDelays.Num() == 0)
	{
		UE_LOG(LogUnscaledTime, Log, TEXT("  PendingDelays: none"));
	}
	else
	{
		for (const TPair<FUnscaledDelayKey, FPendingUnscaledDelay>& PendingDelayPair : PendingDelays)
		{
			const FUnscaledDelayKey& Key = PendingDelayPair.Key;
			const FPendingUnscaledDelay& PendingDelay = PendingDelayPair.Value;
			const UObject* Target = PendingDelay.CallbackTarget.Get();
			const FTimerManager* TimerManager = PendingDelay.Clock == EUnscaledTimeClock::RealTimeUnpaused ? UnpausedManager.Get() : RealTimeManager.Get();
			const float RemainingTime = TimerManager ? TimerManager->GetTimerRemaining(PendingDelay.TimerHandle) : -1.f;
			const FString TargetPathName = Target ? Target->GetPathName() : TEXT("<stale>");

			UE_LOG(LogUnscaledTime, Log, TEXT("  PendingDelay: Target=%s UUID=%d Function=%s Linkage=%d Clock=%s Remaining=%.6f"),
				*TargetPathName,
				Key.UUID,
				*PendingDelay.ExecutionFunction.ToString(),
				PendingDelay.Linkage,
				GetUnscaledClockName(PendingDelay.Clock),
				RemainingTime);
		}
	}

	UE_LOG(LogUnscaledTime, Log, TEXT("  RealTime manager timers:"));
	if (RealTimeManager.IsValid())
	{
		RealTimeManager->ListTimers();
	}
	else
	{
		UE_LOG(LogUnscaledTime, Log, TEXT("    <not initialized>"));
	}

	UE_LOG(LogUnscaledTime, Log, TEXT("  RealTimeUnpaused manager timers:"));
	if (UnpausedManager.IsValid())
	{
		UnpausedManager->ListTimers();
	}
	else
	{
		UE_LOG(LogUnscaledTime, Log, TEXT("    <not initialized>"));
	}
}

#endif
