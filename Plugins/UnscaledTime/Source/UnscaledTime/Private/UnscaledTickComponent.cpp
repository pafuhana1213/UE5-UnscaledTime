#include "UnscaledTickComponent.h"

#include "Engine/World.h"
#include "UnscaledTimeSubsystem.h"

UUnscaledTickComponent::UUnscaledTickComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UUnscaledTickComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UUnscaledTickComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	if (IsActive())
	{
		RegisterTickDelegate();
	}
}

void UUnscaledTickComponent::Deactivate()
{
	// 無効化・unregister・EndPlay・破棄のどの経路でも同じ解除処理に寄せ、二重解除や漏れを防ぐ。
	UnregisterTickDelegate();

	Super::Deactivate();
}

void UUnscaledTickComponent::OnUnregister()
{
	UnregisterTickDelegate();

	Super::OnUnregister();
}

void UUnscaledTickComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterTickDelegate();

	Super::EndPlay(EndPlayReason);
}

void UUnscaledTickComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	UnregisterTickDelegate();

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UUnscaledTickComponent::RegisterTickDelegate()
{
	if (TickDelegateHandle.IsValid())
	{
		// Activate が複数回呼ばれても購読は 1 本に保つ。
		return;
	}

	UWorld* World = GetWorld();
	UUnscaledTimeSubsystem* Subsystem = World ? World->GetSubsystem<UUnscaledTimeSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	RegisteredClock = bTickWhilePaused ? EUnscaledTimeClock::RealTime : EUnscaledTimeClock::RealTimeUnpaused;
	// 登録時のクロックを保存しておき、実行中に bTickWhilePaused が変わっても同じ delegate から解除する。
	TickDelegateHandle = Subsystem->GetOnUnscaledTick(RegisteredClock).AddUObject(this, &UUnscaledTickComponent::HandleUnscaledTick);
}

void UUnscaledTickComponent::UnregisterTickDelegate()
{
	if (!TickDelegateHandle.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	UUnscaledTimeSubsystem* Subsystem = World ? World->GetSubsystem<UUnscaledTimeSubsystem>() : nullptr;
	if (Subsystem)
	{
		Subsystem->GetOnUnscaledTick(RegisteredClock).Remove(TickDelegateHandle);
	}

	TickDelegateHandle.Reset();
	RegisteredClock = EUnscaledTimeClock::RealTime;
}

void UUnscaledTickComponent::HandleUnscaledTick(float RealDeltaSeconds)
{
	OnUnscaledTick.Broadcast(RealDeltaSeconds);
}
