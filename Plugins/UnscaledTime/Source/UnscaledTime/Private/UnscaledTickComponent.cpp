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
	UnregisterTickDelegate();

	Super::Deactivate();
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
		return;
	}

	UWorld* World = GetWorld();
	UUnscaledTimeSubsystem* Subsystem = World ? World->GetSubsystem<UUnscaledTimeSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	RegisteredClock = bTickWhilePaused ? EUnscaledTimeClock::RealTime : EUnscaledTimeClock::RealTimeUnpaused;
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
