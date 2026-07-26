#include "Tests/UnscaledGASTestClasses.h"

#include "AbilityTask_UnscaledTick.h"
#include "AbilityTask_WaitUnscaledDelay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UnscaledGASTestClasses)

bool UUnscaledTimeWaitTestAbility::bActivated = false;
bool UUnscaledTimeWaitTestAbility::bFinished = false;
bool UUnscaledTimeWaitTestAbility::bEnded = false;

UUnscaledTimeWaitTestAbility::UUnscaledTimeWaitTestAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UUnscaledTimeWaitTestAbility::Reset()
{
	bActivated = false;
	bFinished = false;
	bEnded = false;
}

void UUnscaledTimeWaitTestAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bActivated = true;

	UAbilityTask_WaitUnscaledDelay* WaitTask = UAbilityTask_WaitUnscaledDelay::WaitUnscaledDelay(this, 1.0f, true);
	WaitTask->OnFinish.AddDynamic(this, &UUnscaledTimeWaitTestAbility::HandleWaitFinished);
	WaitTask->ReadyForActivation();
}

void UUnscaledTimeWaitTestAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	bEnded = true;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UUnscaledTimeWaitTestAbility::HandleWaitFinished()
{
	bFinished = true;
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
}

bool UUnscaledTimeTickTestAbility::bActivated = false;
bool UUnscaledTimeTickTestAbility::bEnded = false;
int32 UUnscaledTimeTickTestAbility::TickCount = 0;
float UUnscaledTimeTickTestAbility::AccumulatedRealDeltaSeconds = 0.f;
float UUnscaledTimeTickTestAbility::LastRealDeltaSeconds = 0.f;
TWeakObjectPtr<UUnscaledTimeTickTestAbility> UUnscaledTimeTickTestAbility::LastInstance;

UUnscaledTimeTickTestAbility::UUnscaledTimeTickTestAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UUnscaledTimeTickTestAbility::Reset()
{
	bActivated = false;
	bEnded = false;
	TickCount = 0;
	AccumulatedRealDeltaSeconds = 0.f;
	LastRealDeltaSeconds = 0.f;
	LastInstance.Reset();
}

void UUnscaledTimeTickTestAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bActivated = true;
	LastInstance = this;

	TickTask = UAbilityTask_UnscaledTick::UnscaledTick(this, true);
	TickTask->OnTick.AddDynamic(this, &UUnscaledTimeTickTestAbility::HandleUnscaledTick);
	TickTask->ReadyForActivation();
}

void UUnscaledTimeTickTestAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	bEnded = true;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UUnscaledTimeTickTestAbility::EndFromTest()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
}

bool UUnscaledTimeTickTestAbility::IsTickTaskActiveForTest() const
{
	return TickTask && TickTask->IsActive();
}

bool UUnscaledTimeTickTestAbility::IsTickTaskFinishedForTest() const
{
	return TickTask && TickTask->IsFinished();
}

void UUnscaledTimeTickTestAbility::HandleUnscaledTick(float RealDeltaSeconds)
{
	++TickCount;
	LastRealDeltaSeconds = RealDeltaSeconds;
	AccumulatedRealDeltaSeconds += RealDeltaSeconds;
}
