#pragma once

// Note: no WITH_DEV_AUTOMATION_TESTS guard here — UHT does not allow reflection
// macros (UCLASS/UPROPERTY/UFUNCTION) inside preprocessor blocks.

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "UnscaledGASTestClasses.generated.h"

class UAbilityTask_UnscaledTick;

UCLASS(Hidden, NotBlueprintType, NotBlueprintable)
class UUnscaledTimeWaitTestAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UUnscaledTimeWaitTestAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static void Reset();

	static bool bActivated;
	static bool bFinished;
	static bool bEnded;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleWaitFinished();
};

UCLASS(Hidden, NotBlueprintType, NotBlueprintable)
class UUnscaledTimeTickTestAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UUnscaledTimeTickTestAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static void Reset();

	static bool bActivated;
	static bool bEnded;
	static int32 TickCount;
	static float AccumulatedRealDeltaSeconds;
	static float LastRealDeltaSeconds;
	static TWeakObjectPtr<UUnscaledTimeTickTestAbility> LastInstance;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	void EndFromTest();
	bool IsTickTaskActiveForTest() const;
	bool IsTickTaskFinishedForTest() const;

private:
	UPROPERTY()
	TObjectPtr<UAbilityTask_UnscaledTick> TickTask;

	UFUNCTION()
	void HandleUnscaledTick(float RealDeltaSeconds);
};
