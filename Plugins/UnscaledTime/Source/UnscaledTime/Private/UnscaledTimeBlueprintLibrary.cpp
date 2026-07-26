#include "UnscaledTimeBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "TimerManager.h"
#include "UObject/Stack.h"
#include "UnscaledTimeLog.h"
#include "UnscaledTimeSettings.h"
#include "UnscaledTimeSubsystem.h"

namespace
{
	float GetUnscaledReferenceFrameRate()
	{
		return FMath::Max(1.f, GetDefault<UUnscaledTimeSettings>()->ReferenceFrameRate);
	}

	UUnscaledTimeSubsystem* GetUnscaledTimerSubsystem(const UObject* WorldContextObject, const TCHAR* CallerName)
	{
		UUnscaledTimeSubsystem* Subsystem = UUnscaledTimeSubsystem::Get(WorldContextObject);
		if (!Subsystem)
		{
			UE_LOG(LogUnscaledTime, Warning, TEXT("%s could not find UUnscaledTimeSubsystem for object %s. This world type may not support unscaled timers."), CallerName, *GetNameSafe(WorldContextObject));
		}

		return Subsystem;
	}

	bool IsValidDynamicTimerDelegate(FTimerDynamicDelegate Delegate, const TCHAR* CallerName)
	{
		if (Delegate.IsBound())
		{
			return true;
		}

		UE_LOGF(LogBlueprintUserMessages, Warning,
			"%ls passed a bad function (%ls) or object (%ls)",
			CallerName, *Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));

		return false;
	}

	struct FFoundUnscaledTimer
	{
		FTimerManager* TimerManager = nullptr;
		FTimerHandle Handle;
	};

	bool FindDynamicUnscaledTimer(UUnscaledTimeSubsystem* Subsystem, FTimerDynamicDelegate Delegate, FFoundUnscaledTimer& OutTimer)
	{
		if (!Subsystem)
		{
			return false;
		}

		FTimerManager& RealTimeManager = Subsystem->GetTimerManager(EUnscaledTimeClock::RealTime);
		FTimerHandle RealTimeHandle = RealTimeManager.K2_FindDynamicTimerHandle(Delegate);
		if (RealTimeHandle.IsValid())
		{
			OutTimer.TimerManager = &RealTimeManager;
			OutTimer.Handle = RealTimeHandle;
			return true;
		}

		FTimerManager& UnpausedManager = Subsystem->GetTimerManager(EUnscaledTimeClock::RealTimeUnpaused);
		FTimerHandle UnpausedHandle = UnpausedManager.K2_FindDynamicTimerHandle(Delegate);
		if (UnpausedHandle.IsValid())
		{
			OutTimer.TimerManager = &UnpausedManager;
			OutTimer.Handle = UnpausedHandle;
			return true;
		}

		return false;
	}

	UUnscaledTimeSubsystem* GetSubsystemForDynamicDelegate(FTimerDynamicDelegate Delegate, const TCHAR* CallerName)
	{
		if (!GEngine)
		{
			UE_LOG(LogUnscaledTime, Warning, TEXT("%s could not resolve a world because GEngine is unavailable."), CallerName);
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if (!World)
		{
			return nullptr;
		}

		return GetUnscaledTimerSubsystem(Delegate.GetUObject(), CallerName);
	}
}

void UUnscaledTimeBlueprintLibrary::UnscaledDelay(const UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo, bool bTickWhilePaused)
{
	if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("UnscaledDelay")))
	{
		Subsystem->RegisterUnscaledDelay(Duration, false, bTickWhilePaused, LatentInfo);
	}
}

void UUnscaledTimeBlueprintLibrary::UnscaledRetriggerableDelay(const UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo, bool bTickWhilePaused)
{
	if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("UnscaledRetriggerableDelay")))
	{
		Subsystem->RegisterUnscaledDelay(Duration, true, bTickWhilePaused, LatentInfo);
	}
}

void UUnscaledTimeBlueprintLibrary::UnscaledDelayByFrames(const UObject* WorldContextObject, int32 FrameDuration, FLatentActionInfo LatentInfo, bool bTickWhilePaused)
{
	if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("UnscaledDelayByFrames")))
	{
		Subsystem->RegisterUnscaledDelay(UnscaledFramesToSeconds(FrameDuration), false, bTickWhilePaused, LatentInfo);
	}
}

void UUnscaledTimeBlueprintLibrary::UnscaledRetriggerableDelayByFrames(const UObject* WorldContextObject, int32 FrameDuration, FLatentActionInfo LatentInfo, bool bTickWhilePaused)
{
	if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("UnscaledRetriggerableDelayByFrames")))
	{
		Subsystem->RegisterUnscaledDelay(UnscaledFramesToSeconds(FrameDuration), true, bTickWhilePaused, LatentInfo);
	}
}

FUnscaledTimerHandle UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerDelegate(FTimerDynamicDelegate Delegate, float Time, bool bLooping, bool bTickWhilePaused, bool bMaxOncePerFrame, float InitialStartDelay, float InitialStartDelayVariance)
{
	return SetUnscaledTimerDelegateInternal(Delegate, Time, bLooping, bTickWhilePaused, bMaxOncePerFrame, InitialStartDelay, InitialStartDelayVariance);
}

FUnscaledTimerHandle UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimer(UObject* Object, FString FunctionName, float Time, bool bLooping, bool bTickWhilePaused, bool bMaxOncePerFrame, float InitialStartDelay, float InitialStartDelayVariance)
{
	if (!ValidateTimerFunction(Object, FunctionName, TEXT("SetUnscaledTimer")))
	{
		return FUnscaledTimerHandle();
	}

	FName const FunctionFName(*FunctionName);

	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, FunctionFName);

	// UKismetSystemLibrary::K2_SetTimer does not forward InitialStartDelayVariance; this unscaled variant intentionally does.
	return SetUnscaledTimerDelegateInternal(Delegate, Time, bLooping, bTickWhilePaused, bMaxOncePerFrame, InitialStartDelay, InitialStartDelayVariance);
}

FUnscaledTimerHandle UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerDelegateByFrames(FTimerDynamicDelegate Delegate, int32 FrameInterval, bool bLooping, bool bTickWhilePaused, bool bMaxOncePerFrame)
{
	const float Seconds = UnscaledFramesToSeconds(FrameInterval);
	return SetUnscaledTimerDelegateInternal(Delegate, Seconds, bLooping, bTickWhilePaused, bMaxOncePerFrame, 0.f, 0.f);
}

FUnscaledTimerHandle UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerByFrames(UObject* Object, FString FunctionName, int32 FrameInterval, bool bLooping, bool bTickWhilePaused, bool bMaxOncePerFrame)
{
	if (!ValidateTimerFunction(Object, FunctionName, TEXT("SetUnscaledTimerByFrames")))
	{
		return FUnscaledTimerHandle();
	}

	FName const FunctionFName(*FunctionName);

	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, FunctionFName);

	const float Seconds = UnscaledFramesToSeconds(FrameInterval);
	return SetUnscaledTimerDelegateInternal(Delegate, Seconds, bLooping, bTickWhilePaused, bMaxOncePerFrame, 0.f, 0.f);
}

void UUnscaledTimeBlueprintLibrary::ClearAndInvalidateUnscaledTimer(const UObject* WorldContextObject, FUnscaledTimerHandle& Handle)
{
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("ClearAndInvalidateUnscaledTimer")))
		{
			Subsystem->GetTimerManager(Handle.Clock).ClearTimer(Handle.Handle);
		}
	}

	Handle.Invalidate();
}

void UUnscaledTimeBlueprintLibrary::PauseUnscaledTimer(const UObject* WorldContextObject, FUnscaledTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("PauseUnscaledTimer")))
		{
			Subsystem->GetTimerManager(Handle.Clock).PauseTimer(Handle.Handle);
		}
	}
}

void UUnscaledTimeBlueprintLibrary::UnPauseUnscaledTimer(const UObject* WorldContextObject, FUnscaledTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("UnPauseUnscaledTimer")))
		{
			Subsystem->GetTimerManager(Handle.Clock).UnPauseTimer(Handle.Handle);
		}
	}
}

bool UUnscaledTimeBlueprintLibrary::IsUnscaledTimerActive(const UObject* WorldContextObject, FUnscaledTimerHandle Handle)
{
	bool bIsActive = false;
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("IsUnscaledTimerActive")))
		{
			bIsActive = Subsystem->GetTimerManager(Handle.Clock).IsTimerActive(Handle.Handle);
		}
	}

	return bIsActive;
}

bool UUnscaledTimeBlueprintLibrary::IsUnscaledTimerPaused(const UObject* WorldContextObject, FUnscaledTimerHandle Handle)
{
	bool bIsPaused = false;
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("IsUnscaledTimerPaused")))
		{
			bIsPaused = Subsystem->GetTimerManager(Handle.Clock).IsTimerPaused(Handle.Handle);
		}
	}

	return bIsPaused;
}

bool UUnscaledTimeBlueprintLibrary::UnscaledTimerExists(const UObject* WorldContextObject, FUnscaledTimerHandle Handle)
{
	bool bTimerExists = false;
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("UnscaledTimerExists")))
		{
			bTimerExists = Subsystem->GetTimerManager(Handle.Clock).TimerExists(Handle.Handle);
		}
	}

	return bTimerExists;
}

float UUnscaledTimeBlueprintLibrary::GetUnscaledTimerElapsed(const UObject* WorldContextObject, FUnscaledTimerHandle Handle)
{
	float ElapsedTime = 0.f;
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("GetUnscaledTimerElapsed")))
		{
			ElapsedTime = Subsystem->GetTimerManager(Handle.Clock).GetTimerElapsed(Handle.Handle);
		}
	}

	return ElapsedTime;
}

float UUnscaledTimeBlueprintLibrary::GetUnscaledTimerRemaining(const UObject* WorldContextObject, FUnscaledTimerHandle Handle)
{
	float RemainingTime = 0.f;
	if (Handle.IsValid())
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetUnscaledTimerSubsystem(WorldContextObject, TEXT("GetUnscaledTimerRemaining")))
		{
			RemainingTime = Subsystem->GetTimerManager(Handle.Clock).GetTimerRemaining(Handle.Handle);
		}
	}

	return RemainingTime;
}

bool UUnscaledTimeBlueprintLibrary::IsUnscaledTimerHandleValid(FUnscaledTimerHandle Handle)
{
	return Handle.IsValid();
}

void UUnscaledTimeBlueprintLibrary::ClearAndInvalidateUnscaledTimerByEvent(FTimerDynamicDelegate Delegate)
{
	if (IsValidDynamicTimerDelegate(Delegate, TEXT("ClearUnscaledTimer")))
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetSubsystemForDynamicDelegate(Delegate, TEXT("ClearUnscaledTimer")))
		{
			FFoundUnscaledTimer FoundTimer;
			if (FindDynamicUnscaledTimer(Subsystem, Delegate, FoundTimer))
			{
				FoundTimer.TimerManager->ClearTimer(FoundTimer.Handle);
			}
		}
	}
}

void UUnscaledTimeBlueprintLibrary::PauseUnscaledTimerByEvent(FTimerDynamicDelegate Delegate)
{
	if (IsValidDynamicTimerDelegate(Delegate, TEXT("PauseUnscaledTimer")))
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetSubsystemForDynamicDelegate(Delegate, TEXT("PauseUnscaledTimer")))
		{
			FFoundUnscaledTimer FoundTimer;
			if (FindDynamicUnscaledTimer(Subsystem, Delegate, FoundTimer))
			{
				FoundTimer.TimerManager->PauseTimer(FoundTimer.Handle);
			}
		}
	}
}

void UUnscaledTimeBlueprintLibrary::UnPauseUnscaledTimerByEvent(FTimerDynamicDelegate Delegate)
{
	if (IsValidDynamicTimerDelegate(Delegate, TEXT("UnPauseUnscaledTimer")))
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetSubsystemForDynamicDelegate(Delegate, TEXT("UnPauseUnscaledTimer")))
		{
			FFoundUnscaledTimer FoundTimer;
			if (FindDynamicUnscaledTimer(Subsystem, Delegate, FoundTimer))
			{
				FoundTimer.TimerManager->UnPauseTimer(FoundTimer.Handle);
			}
		}
	}
}

bool UUnscaledTimeBlueprintLibrary::IsUnscaledTimerActiveByEvent(FTimerDynamicDelegate Delegate)
{
	bool bIsActive = false;
	if (IsValidDynamicTimerDelegate(Delegate, TEXT("IsUnscaledTimerActive")))
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetSubsystemForDynamicDelegate(Delegate, TEXT("IsUnscaledTimerActive")))
		{
			FFoundUnscaledTimer FoundTimer;
			if (FindDynamicUnscaledTimer(Subsystem, Delegate, FoundTimer))
			{
				bIsActive = FoundTimer.TimerManager->IsTimerActive(FoundTimer.Handle);
			}
		}
	}

	return bIsActive;
}

bool UUnscaledTimeBlueprintLibrary::IsUnscaledTimerPausedByEvent(FTimerDynamicDelegate Delegate)
{
	bool bIsPaused = false;
	if (IsValidDynamicTimerDelegate(Delegate, TEXT("IsUnscaledTimerPaused")))
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetSubsystemForDynamicDelegate(Delegate, TEXT("IsUnscaledTimerPaused")))
		{
			FFoundUnscaledTimer FoundTimer;
			if (FindDynamicUnscaledTimer(Subsystem, Delegate, FoundTimer))
			{
				bIsPaused = FoundTimer.TimerManager->IsTimerPaused(FoundTimer.Handle);
			}
		}
	}

	return bIsPaused;
}

bool UUnscaledTimeBlueprintLibrary::UnscaledTimerExistsByEvent(FTimerDynamicDelegate Delegate)
{
	bool bTimerExists = false;
	if (IsValidDynamicTimerDelegate(Delegate, TEXT("UnscaledTimerExists")))
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetSubsystemForDynamicDelegate(Delegate, TEXT("UnscaledTimerExists")))
		{
			FFoundUnscaledTimer FoundTimer;
			if (FindDynamicUnscaledTimer(Subsystem, Delegate, FoundTimer))
			{
				bTimerExists = FoundTimer.TimerManager->TimerExists(FoundTimer.Handle);
			}
		}
	}

	return bTimerExists;
}

float UUnscaledTimeBlueprintLibrary::GetUnscaledDeltaSeconds(const UObject* WorldContextObject)
{
	if (UUnscaledTimeSubsystem* Subsystem = UUnscaledTimeSubsystem::Get(WorldContextObject))
	{
		return Subsystem->GetLastRealDeltaSeconds();
	}

	return 0.f;
}

double UUnscaledTimeBlueprintLibrary::GetUnscaledTimeSeconds(const UObject* WorldContextObject, EUnscaledTimeClock Clock)
{
	if (UUnscaledTimeSubsystem* Subsystem = UUnscaledTimeSubsystem::Get(WorldContextObject))
	{
		return Subsystem->GetUnscaledTimeSeconds(Clock);
	}

	return 0.0;
}

float UUnscaledTimeBlueprintLibrary::UnscaledFramesToSeconds(int32 Frames)
{
	return static_cast<float>(Frames) / GetUnscaledReferenceFrameRate();
}

int32 UUnscaledTimeBlueprintLibrary::UnscaledSecondsToFrames(float Seconds)
{
	return FMath::RoundToInt(Seconds * GetUnscaledReferenceFrameRate());
}

FUnscaledTimerHandle UUnscaledTimeBlueprintLibrary::SetUnscaledTimerDelegateInternal(FTimerDynamicDelegate Delegate, float Time, bool bLooping, bool bTickWhilePaused, bool bMaxOncePerFrame, float InitialStartDelay, float InitialStartDelayVariance)
{
	FUnscaledTimerHandle UnscaledHandle;
	if (IsValidDynamicTimerDelegate(Delegate, TEXT("SetUnscaledTimer")))
	{
		if (UUnscaledTimeSubsystem* Subsystem = GetSubsystemForDynamicDelegate(Delegate, TEXT("SetUnscaledTimer")))
		{
			InitialStartDelay += FMath::FRandRange(-InitialStartDelayVariance, InitialStartDelayVariance);
			if (Time <= 0.f || (Time + InitialStartDelay) < 0.f)
			{
				FString ObjectName = GetNameSafe(Delegate.GetUObject());
				FString FunctionName = Delegate.GetFunctionName().ToString();
				FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s %s SetUnscaledTimer passed a negative or zero time. The associated timer may fail to be created/fire! If using InitialStartDelayVariance, be sure it is smaller than (Time + InitialStartDelay)."), *ObjectName, *FunctionName), ELogVerbosity::Warning);
			}

			const EUnscaledTimeClock Clock = bTickWhilePaused ? EUnscaledTimeClock::RealTime : EUnscaledTimeClock::RealTimeUnpaused;
			const EUnscaledTimeClock OppositeClock = bTickWhilePaused ? EUnscaledTimeClock::RealTimeUnpaused : EUnscaledTimeClock::RealTime;
			FTimerManager& OppositeTimerManager = Subsystem->GetTimerManager(OppositeClock);
			// Dynamic delegate timers are unique across both unscaled clocks, matching vanilla timer behavior.
			FTimerHandle OppositeHandle = OppositeTimerManager.K2_FindDynamicTimerHandle(Delegate);
			if (OppositeHandle.IsValid())
			{
				OppositeTimerManager.ClearTimer(OppositeHandle);
			}

			FTimerManager& TimerManager = Subsystem->GetTimerManager(Clock);
			UnscaledHandle.Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			UnscaledHandle.Clock = Clock;
			TimerManager.SetTimer(UnscaledHandle.Handle, Delegate, Time, FTimerManagerTimerParameters { .bLoop = bLooping, .bMaxOncePerFrame = bMaxOncePerFrame, .FirstDelay = Time + InitialStartDelay });
		}
	}

	return UnscaledHandle;
}

bool UUnscaledTimeBlueprintLibrary::ValidateTimerFunction(UObject* Object, const FString& FunctionName, const TCHAR* CallerName)
{
	FName const FunctionFName(*FunctionName);

	if (Object)
	{
		UFunction* const Func = Object->FindFunction(FunctionFName);
		if (Func && (Func->ParmsSize > 0))
		{
			UE_LOGF(LogBlueprintUserMessages, Warning, "%ls passed a function (%ls) that expects parameters.", CallerName, *FunctionName);
			return false;
		}
	}

	return true;
}
