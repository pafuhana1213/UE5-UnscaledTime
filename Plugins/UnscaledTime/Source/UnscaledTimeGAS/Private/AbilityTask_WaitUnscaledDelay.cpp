#include "AbilityTask_WaitUnscaledDelay.h"

#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnscaledTimeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_WaitUnscaledDelay)

DEFINE_LOG_CATEGORY_STATIC(LogUnscaledTimeGAS, Log, All);

UAbilityTask_WaitUnscaledDelay::UAbilityTask_WaitUnscaledDelay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Time = 0.f;
	TimeStarted = 0.0;
	bTickWhilePaused = true;
	bUsingUnscaledTimer = false;
	RegisteredClock = EUnscaledTimeClock::RealTime;
}

UAbilityTask_WaitUnscaledDelay* UAbilityTask_WaitUnscaledDelay::WaitUnscaledDelay(UGameplayAbility* OwningAbility, float Time, bool bTickWhilePaused)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Time);

	UAbilityTask_WaitUnscaledDelay* MyObj = NewAbilityTask<UAbilityTask_WaitUnscaledDelay>(OwningAbility);
	MyObj->Time = Time;
	MyObj->bTickWhilePaused = bTickWhilePaused;
	return MyObj;
}

void UAbilityTask_WaitUnscaledDelay::Activate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUnscaledTimeGAS, Warning, TEXT("WaitUnscaledDelay could not resolve a world."));
		EndTask();
		return;
	}

	RegisteredClock = bTickWhilePaused ? EUnscaledTimeClock::RealTime : EUnscaledTimeClock::RealTimeUnpaused;

	if (UUnscaledTimeSubsystem* Subsystem = UUnscaledTimeSubsystem::Get(World))
	{
		bUsingUnscaledTimer = true;
		TimeStarted = Subsystem->GetUnscaledTimeSeconds(RegisteredClock);
		FTimerManager& TimerManager = Subsystem->GetTimerManager(RegisteredClock);
		if (Time <= 0.0f)
		{
			TimerHandle = TimerManager.SetTimerForNextTick(this, &ThisClass::OnTimeFinish);
		}
		else
		{
			TimerManager.SetTimer(TimerHandle, this, &ThisClass::OnTimeFinish, Time, false);
		}
	}
	else
	{
		bUsingUnscaledTimer = false;
		TimeStarted = World->GetTimeSeconds();
		UE_LOG(LogUnscaledTimeGAS, Warning, TEXT("WaitUnscaledDelay could not find UUnscaledTimeSubsystem for world %s. Falling back to the world timer manager."), *GetNameSafe(World));

		FTimerManager& TimerManager = World->GetTimerManager();
		if (Time <= 0.0f)
		{
			TimerHandle = TimerManager.SetTimerForNextTick(this, &ThisClass::OnTimeFinish);
		}
		else
		{
			TimerManager.SetTimer(TimerHandle, this, &ThisClass::OnTimeFinish, Time, false);
		}
	}
}

void UAbilityTask_WaitUnscaledDelay::OnDestroy(bool bInOwnerFinished)
{
	if (TimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (bUsingUnscaledTimer)
			{
				if (UUnscaledTimeSubsystem* Subsystem = UUnscaledTimeSubsystem::Get(World))
				{
					Subsystem->GetTimerManager(RegisteredClock).ClearTimer(TimerHandle);
				}
				else
				{
					TimerHandle.Invalidate();
				}
			}
			else
			{
				World->GetTimerManager().ClearTimer(TimerHandle);
			}
		}
		else
		{
			TimerHandle.Invalidate();
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_WaitUnscaledDelay::OnTimeFinish()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}
	EndTask();
}

FString UAbilityTask_WaitUnscaledDelay::GetDebugString() const
{
	if (UWorld* World = GetWorld())
	{
		float TimeLeft = 0.f;
		if (bUsingUnscaledTimer)
		{
			if (UUnscaledTimeSubsystem* Subsystem = UUnscaledTimeSubsystem::Get(World))
			{
				TimeLeft = Time - static_cast<float>(Subsystem->GetUnscaledTimeSeconds(RegisteredClock) - TimeStarted);
			}
		}
		else
		{
			TimeLeft = Time - World->TimeSince(static_cast<float>(TimeStarted));
		}

		return FString::Printf(TEXT("WaitUnscaledDelay. Time: %.2f. TimeLeft: %.2f"), Time, TimeLeft);
	}
	else
	{
		return FString::Printf(TEXT("WaitUnscaledDelay. Time: %.2f. Time Started: %.2f"), Time, TimeStarted);
	}
}
