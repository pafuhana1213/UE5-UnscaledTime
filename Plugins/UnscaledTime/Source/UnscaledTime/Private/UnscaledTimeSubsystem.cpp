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

		// 0x5554 の固定 prefix と world pointer を組み合わせ、PIE の複数 world が
		// 同じ on-screen message を上書きしないようにする。
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

	// Tick の DeltaTime は dilation 済みなので時間計測には使わない。
	// world ごとの real delta を使うと PIE 複数 world と test world の制御に乗せられる。
	float RealDelta = World->GetTime().GetDeltaRealTimeSeconds();
	const float MaxDelta = GetDefault<UUnscaledTimeSettings>()->MaxRealDeltaSeconds;
	if (MaxDelta > 0.f)
	{
		// editor break や stall 復帰直後の大きな real delta で、loop timer の catch-up や
		// callback が 1 frame に集中しないよう実時間側も上限を設ける。
		RealDelta = FMath::Min(RealDelta, MaxDelta);
	}

	LastRealDelta = RealDelta;
	AccumulatedRealTime += RealDelta;

	// RealTime は常に進め、Unpaused は world pause guard の外側で明示的に止める。
	// 2 本の manager を同じ real delta で tick することで、pause policy だけを分離する。
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
		LatentInfo.UUID
	};

	const EUnscaledTimeClock Clock = bTickWhilePaused ? EUnscaledTimeClock::RealTime : EUnscaledTimeClock::RealTimeUnpaused;

	if (FPendingUnscaledDelay* ExistingDelay = PendingDelays.Find(Key))
	{
		// latent Delay と同じく同一 key の非リトリガ呼び出しは無視し、
		// retriggerable だけ timer と resume 情報を張り直す。
		if (!bRetriggerable)
		{
			return;
		}

		const FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &UUnscaledTimeSubsystem::OnDelayExpired, Key);
		FTimerManager& OldTimerManager = GetTimerManager(ExistingDelay->Clock);
		FTimerManager& TimerManager = GetTimerManager(Clock);

		if (ExistingDelay->Clock != Clock)
		{
			// retrigger 時に pause policy が変わる場合は、旧 manager の timer を消してから
			// handle を無効化し、新しい clock の manager で張り直す。
			OldTimerManager.ClearTimer(ExistingDelay->TimerHandle);
			ExistingDelay->TimerHandle.Invalidate();
			ExistingDelay->Clock = Clock;
		}

		if (DurationSeconds <= 0.f)
		{
			// TimerManager の next-tick セマンティクスに合わせ、次の unscaled timer tick で再開する。
			if (ExistingDelay->TimerHandle.IsValid())
			{
				TimerManager.ClearTimer(ExistingDelay->TimerHandle);
			}
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

	FTimerManager& TimerManager = GetTimerManager(Clock);
	const FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &UUnscaledTimeSubsystem::OnDelayExpired, Key);

	FPendingUnscaledDelay PendingDelay;
	PendingDelay.Clock = Clock;
	PendingDelay.ExecutionFunction = LatentInfo.ExecutionFunction;
	PendingDelay.Linkage = LatentInfo.Linkage;
	PendingDelay.CallbackTarget = LatentInfo.CallbackTarget;

	if (DurationSeconds <= 0.f)
	{
		// TimerManager の next-tick セマンティクスに合わせ、次の unscaled timer tick で再開する。
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

	// 標準 latent Delay は world の latent action 経路が pause guard 内で止まる。
	// ここでは manager 満了後に engine の latent manager と同じ ProcessEvent で再開点を直接発火する。
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
