#include "UnscaledTimeSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Stats/Stats.h"
#include "UnscaledTimeLog.h"
#include "UnscaledTimeSettings.h"
#include "UnscaledTimeStats.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#endif

DEFINE_STAT(STAT_UnscaledTimeSubsystemTick);
DEFINE_STAT(STAT_UnscaledTimePendingDelays);

#if !UE_BUILD_SHIPPING
namespace
{
	bool IsUnscaledDebugEnabled()
	{
		const IConsoleVariable* DebugCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("UnscaledTime.Debug"));
		return DebugCVar && DebugCVar->GetInt() > 0;
	}

	bool ShouldSuspendUnscaledTimeWhileDebugPaused()
	{
		const IConsoleVariable* SuspendCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("UnscaledTime.SuspendWhileDebugPaused"));
		return SuspendCVar && SuspendCVar->GetInt() != 0;
	}

	void DisplayUnscaledDebugMessage(const UUnscaledTimeSubsystem* Subsystem, const UWorld* World)
	{
		if (!GEngine || !Subsystem || !World || !IsUnscaledDebugEnabled())
		{
			return;
		}

		const uint64 MessageKey = (uint64(0x5554) << 32) | PointerHash(World);
		const FString Message = FString::Printf(TEXT("UnscaledTime [%s]: RealTime=%.2f Unpaused=%.2f Delta=%.4f PendingDelays=%d Paused=%s"),
			*World->GetName(),
			Subsystem->GetUnscaledTimeSeconds(EUnscaledTimeClock::RealTime),
			Subsystem->GetUnscaledTimeSeconds(EUnscaledTimeClock::RealTimeUnpaused),
			Subsystem->GetLastRealDeltaSeconds(),
			Subsystem->GetPendingDelayCount(),
			World->IsPaused() ? TEXT("true") : TEXT("false"));

		GEngine->AddOnScreenDebugMessage(MessageKey, 0.f, FColor::Cyan, Message, false);
	}
}
#endif

UUnscaledTimeSubsystem* UUnscaledTimeSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? World->GetSubsystem<UUnscaledTimeSubsystem>() : nullptr;
}

void UUnscaledTimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	RealTimeManager = MakeUnique<FTimerManager>(GameInstance);
	UnpausedManager = MakeUnique<FTimerManager>(GameInstance);

	AccumulatedRealTime = 0.0;
	AccumulatedUnpausedRealTime = 0.0;
	LastRealDelta = 0.0f;

	UE_LOG(LogUnscaledTime, Verbose, TEXT("Initialized UnscaledTime subsystem for world %s."), World ? *World->GetName() : TEXT("<null>"));
}

void UUnscaledTimeSubsystem::Deinitialize()
{
	UE_LOG(LogUnscaledTime, Verbose, TEXT("Deinitialized UnscaledTime subsystem for world %s."), GetWorld() ? *GetWorld()->GetName() : TEXT("<null>"));

	PendingDelays.Empty();
	RealTimeManager.Reset();
	UnpausedManager.Reset();

	Super::Deinitialize();
}

bool UUnscaledTimeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UUnscaledTimeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnscaledTimeSubsystem, STATGROUP_UnscaledTime);
}

void UUnscaledTimeSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_UnscaledTimeSubsystemTick);

	Super::Tick(DeltaTime);
	SET_DWORD_STAT(STAT_UnscaledTimePendingDelays, PendingDelays.Num());

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (World->bDebugPauseExecution && ShouldSuspendUnscaledTimeWhileDebugPaused())
	{
		DisplayUnscaledDebugMessage(this, World);
		return;
	}
#endif

	float RealDelta = World->GetTime().GetDeltaRealTimeSeconds();
	const float MaxDelta = GetDefault<UUnscaledTimeSettings>()->MaxRealDeltaSeconds;
	if (MaxDelta > 0.f)
	{
		RealDelta = FMath::Min(RealDelta, MaxDelta);
	}

	LastRealDelta = RealDelta;
	AccumulatedRealTime += RealDelta;

	checkf(RealTimeManager.IsValid(), TEXT("RealTimeManager must be valid while UUnscaledTimeSubsystem is initialized."));
	RealTimeManager->Tick(RealDelta);
	OnTickRealTime.Broadcast(RealDelta);

	if (!World->IsPaused())
	{
		AccumulatedUnpausedRealTime += RealDelta;

		checkf(UnpausedManager.IsValid(), TEXT("UnpausedManager must be valid while UUnscaledTimeSubsystem is initialized."));
		UnpausedManager->Tick(RealDelta);
		OnTickUnpaused.Broadcast(RealDelta);
	}

#if !UE_BUILD_SHIPPING
	DisplayUnscaledDebugMessage(this, World);
#endif
}

FTimerManager& UUnscaledTimeSubsystem::GetTimerManager(EUnscaledTimeClock Clock)
{
	switch (Clock)
	{
	case EUnscaledTimeClock::RealTimeUnpaused:
		checkf(UnpausedManager.IsValid(), TEXT("UnpausedManager must be valid while UUnscaledTimeSubsystem is initialized."));
		return *UnpausedManager;

	case EUnscaledTimeClock::RealTime:
	default:
		checkf(RealTimeManager.IsValid(), TEXT("RealTimeManager must be valid while UUnscaledTimeSubsystem is initialized."));
		return *RealTimeManager;
	}
}

void UUnscaledTimeSubsystem::RegisterUnscaledDelay(float DurationSeconds, bool bRetriggerable, bool bTickWhilePaused, const FLatentActionInfo& LatentInfo)
{
	if (!LatentInfo.CallbackTarget || LatentInfo.Linkage == INDEX_NONE)
	{
		return;
	}

	const FUnscaledDelayKey Key
	{
		FWeakObjectPtr(LatentInfo.CallbackTarget),
		LatentInfo.CallbackTarget,
		LatentInfo.UUID
	};

	if (FPendingUnscaledDelay* ExistingDelay = PendingDelays.Find(Key))
	{
		if (!bRetriggerable)
		{
			return;
		}

		FTimerManager& TimerManager = GetTimerManager(ExistingDelay->Clock);
		const FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &UUnscaledTimeSubsystem::OnDelayExpired, Key);

		if (DurationSeconds <= 0.f)
		{
			TimerManager.ClearTimer(ExistingDelay->TimerHandle);
			ExistingDelay->TimerHandle = TimerManager.SetTimerForNextTick(TimerDelegate);
		}
		else
		{
			TimerManager.SetTimer(ExistingDelay->TimerHandle, TimerDelegate, DurationSeconds, false);
		}

		ExistingDelay->ExecutionFunction = LatentInfo.ExecutionFunction;
		ExistingDelay->Linkage = LatentInfo.Linkage;
		ExistingDelay->CallbackTarget = LatentInfo.CallbackTarget;
		return;
	}

	const EUnscaledTimeClock Clock = bTickWhilePaused ? EUnscaledTimeClock::RealTime : EUnscaledTimeClock::RealTimeUnpaused;
	FTimerManager& TimerManager = GetTimerManager(Clock);
	const FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &UUnscaledTimeSubsystem::OnDelayExpired, Key);

	FPendingUnscaledDelay PendingDelay;
	PendingDelay.Clock = Clock;
	PendingDelay.ExecutionFunction = LatentInfo.ExecutionFunction;
	PendingDelay.Linkage = LatentInfo.Linkage;
	PendingDelay.CallbackTarget = LatentInfo.CallbackTarget;

	if (DurationSeconds <= 0.f)
	{
		PendingDelay.TimerHandle = TimerManager.SetTimerForNextTick(TimerDelegate);
	}
	else
	{
		TimerManager.SetTimer(PendingDelay.TimerHandle, TimerDelegate, DurationSeconds, false);
	}

	PendingDelays.Add(Key, PendingDelay);
}

double UUnscaledTimeSubsystem::GetUnscaledTimeSeconds(EUnscaledTimeClock Clock) const
{
	return Clock == EUnscaledTimeClock::RealTimeUnpaused ? AccumulatedUnpausedRealTime : AccumulatedRealTime;
}

float UUnscaledTimeSubsystem::GetLastRealDeltaSeconds() const
{
	return LastRealDelta;
}

int32 UUnscaledTimeSubsystem::GetPendingDelayCount() const
{
	return PendingDelays.Num();
}

UUnscaledTimeSubsystem::FUnscaledTickDelegate& UUnscaledTimeSubsystem::GetOnUnscaledTick(EUnscaledTimeClock Clock)
{
	return Clock == EUnscaledTimeClock::RealTimeUnpaused ? OnTickUnpaused : OnTickRealTime;
}

void UUnscaledTimeSubsystem::OnDelayExpired(FUnscaledDelayKey Key)
{
	FPendingUnscaledDelay* FoundDelay = PendingDelays.Find(Key);
	if (!FoundDelay)
	{
		return;
	}

	const FPendingUnscaledDelay Delay = *FoundDelay;
	PendingDelays.Remove(Key);

	UObject* Target = Delay.CallbackTarget.Get();
	if (!Target)
	{
		UE_LOG(LogUnscaledTime, Warning, TEXT("UUnscaledTimeSubsystem::OnDelayExpired: CallbackTarget is None."));
		return;
	}

	UFunction* Func = Target->FindFunction(Delay.ExecutionFunction);
	if (!Func)
	{
		UE_LOG(LogUnscaledTime, Warning, TEXT("UUnscaledTimeSubsystem::OnDelayExpired: Could not find latent action resume point named '%s' on '%s'."),
			*Delay.ExecutionFunction.ToString(), *Target->GetPathName());
		return;
	}

	int32 LinkId = Delay.Linkage;
	Target->ProcessEvent(Func, &LinkId);
}
