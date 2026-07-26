#include "AbilityTask_UnscaledTick.h"

#include "Engine/World.h"
#include "UnscaledTimeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_UnscaledTick)

DEFINE_LOG_CATEGORY_STATIC(LogUnscaledTimeGAS, Log, All);

UAbilityTask_UnscaledTick::UAbilityTask_UnscaledTick(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegisteredClock = EUnscaledTimeClock::RealTime;
	bTickWhilePaused = true;
}

UAbilityTask_UnscaledTick* UAbilityTask_UnscaledTick::UnscaledTick(UGameplayAbility* OwningAbility, bool bTickWhilePaused)
{
	UAbilityTask_UnscaledTick* MyObj = NewAbilityTask<UAbilityTask_UnscaledTick>(OwningAbility);
	MyObj->bTickWhilePaused = bTickWhilePaused;
	return MyObj;
}

void UAbilityTask_UnscaledTick::Activate()
{
	UWorld* World = GetWorld();
	UUnscaledTimeSubsystem* Subsystem = World ? UUnscaledTimeSubsystem::Get(World) : nullptr;
	if (!Subsystem)
	{
		// 継続 tick は subsystem delegate が無いと unscaled delta を生成できないため終了する。
		// WaitUnscaledDelay は一回完了なら world の scaled timer にフォールバックできる、という非対称性は意図的。
		// fallback timer を持たないので、このタスクには bUsingUnscaledTimer 相当の所有者フラグは不要。
		UE_LOG(LogUnscaledTimeGAS, Warning, TEXT("UnscaledTick could not find UUnscaledTimeSubsystem for world %s. This ability task will not tick."), *GetNameSafe(World));
		EndTask();
		return;
	}

	RegisteredClock = bTickWhilePaused ? EUnscaledTimeClock::RealTime : EUnscaledTimeClock::RealTimeUnpaused;
	// 登録時のクロックを保存し、破棄時に同じ delegate から購読を解除する。
	TickDelegateHandle = Subsystem->GetOnUnscaledTick(RegisteredClock).AddUObject(this, &ThisClass::HandleUnscaledTick);
}

void UAbilityTask_UnscaledTick::OnDestroy(bool bInOwnerFinished)
{
	if (TickDelegateHandle.IsValid())
	{
		UWorld* World = GetWorld();
		UUnscaledTimeSubsystem* Subsystem = World ? UUnscaledTimeSubsystem::Get(World) : nullptr;
		if (Subsystem)
		{
			Subsystem->GetOnUnscaledTick(RegisteredClock).Remove(TickDelegateHandle);
		}

		TickDelegateHandle.Reset();
		RegisteredClock = EUnscaledTimeClock::RealTime;
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_UnscaledTick::HandleUnscaledTick(float RealDeltaSeconds)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnTick.Broadcast(RealDeltaSeconds);
	}
}
