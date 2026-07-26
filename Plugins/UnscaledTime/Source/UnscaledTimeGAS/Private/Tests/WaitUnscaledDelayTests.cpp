#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnscaledGASTestClasses.h"
#include "../../../UnscaledTime/Private/Tests/UnscaledTimeTestHelpers.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "Misc/AutomationTest.h"

namespace UnscaledTimeGASTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	UAbilitySystemComponent* AddAbilitySystemComponent(AActor* Actor)
	{
		UAbilitySystemComponent* AbilitySystemComponent = NewObject<UAbilitySystemComponent>(Actor, TEXT("UnscaledTimeTestASC"));
		Actor->AddInstanceComponent(AbilitySystemComponent);
		AbilitySystemComponent->RegisterComponent();
		AbilitySystemComponent->InitAbilityActorInfo(Actor, Actor);
		return AbilitySystemComponent;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTaskWaitUnscaledDelayRealTimeTest, "UnscaledTime.GAS.WaitUnscaledDelay.RealTimeUnderDilation", UnscaledTimeGASTests::TestFlags)

bool FAbilityTaskWaitUnscaledDelayRealTimeTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	Fixture.SetDilation(0.01f);

	AActor* Actor = Fixture.World()->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystemComponent = UnscaledTimeGASTests::AddAbilitySystemComponent(Actor);

	UUnscaledTimeWaitTestAbility::Reset();
	const FGameplayAbilitySpecHandle AbilityHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UUnscaledTimeWaitTestAbility::StaticClass(), 1, INDEX_NONE));

	if (!TestTrue(TEXT("WaitUnscaledDelay ability activates"), AbilitySystemComponent->TryActivateAbility(AbilityHandle)))
	{
		return false;
	}
	if (!TestTrue(TEXT("WaitUnscaledDelay ability reached ActivateAbility"), UUnscaledTimeWaitTestAbility::bActivated))
	{
		return false;
	}

	Fixture.ArmPendingTimers();
	Fixture.PumpFrames(9, 0.1f);
	Fixture.PumpFrames(1, 0.05f);
	if (!TestFalse(TEXT("WaitUnscaledDelay does not finish before 1.0 real seconds"), UUnscaledTimeWaitTestAbility::bFinished))
	{
		return false;
	}

	Fixture.PumpFrames(1, 0.1f);
	if (!TestTrue(TEXT("WaitUnscaledDelay finishes after 1.0 real seconds under dilation"), UUnscaledTimeWaitTestAbility::bFinished))
	{
		return false;
	}
	if (!TestTrue(TEXT("WaitUnscaledDelay ability ended"), UUnscaledTimeWaitTestAbility::bEnded))
	{
		return false;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
	if (!TestTrue(TEXT("WaitUnscaledDelay ability spec remains findable"), AbilitySpec != nullptr))
	{
		return false;
	}
	if (!TestFalse(TEXT("WaitUnscaledDelay ability is no longer active"), AbilitySpec->IsActive()))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTaskUnscaledTickRealDeltaTest, "UnscaledTime.GAS.UnscaledTick.RealDeltaWhilePaused", UnscaledTimeGASTests::TestFlags)

bool FAbilityTaskUnscaledTickRealDeltaTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	Fixture.SetDilation(0.01f);

	AActor* Actor = Fixture.World()->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystemComponent = UnscaledTimeGASTests::AddAbilitySystemComponent(Actor);

	UUnscaledTimeTickTestAbility::Reset();
	const FGameplayAbilitySpecHandle AbilityHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UUnscaledTimeTickTestAbility::StaticClass(), 1, INDEX_NONE));

	if (!TestTrue(TEXT("UnscaledTick ability activates"), AbilitySystemComponent->TryActivateAbility(AbilityHandle)))
	{
		return false;
	}
	if (!TestTrue(TEXT("UnscaledTick ability reached ActivateAbility"), UUnscaledTimeTickTestAbility::bActivated))
	{
		return false;
	}

	Fixture.ArmPendingTimers();
	UUnscaledTimeTickTestAbility::TickCount = 0;
	UUnscaledTimeTickTestAbility::AccumulatedRealDeltaSeconds = 0.f;
	UUnscaledTimeTickTestAbility::LastRealDeltaSeconds = 0.f;

	Fixture.SetPaused(true);
	Fixture.PumpFrames(4, 0.1f);

	if (!TestEqual(TEXT("UnscaledTick task ticks once per pumped paused frame"), UUnscaledTimeTickTestAbility::TickCount, 4))
	{
		return false;
	}
	if (!TestTrue(TEXT("UnscaledTick task receives real delta under dilation and pause"), FMath::IsNearlyEqual(UUnscaledTimeTickTestAbility::AccumulatedRealDeltaSeconds, 0.4f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}
	if (!TestTrue(TEXT("UnscaledTick task receives per-pump delta"), FMath::IsNearlyEqual(UUnscaledTimeTickTestAbility::LastRealDeltaSeconds, 0.1f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	if (!TestTrue(TEXT("UnscaledTick ability instance is available"), UUnscaledTimeTickTestAbility::LastInstance.IsValid()))
	{
		return false;
	}

	UUnscaledTimeTickTestAbility::LastInstance->EndFromTest();
	if (!TestTrue(TEXT("UnscaledTick ability ended via EndAbility"), UUnscaledTimeTickTestAbility::bEnded))
	{
		return false;
	}

	const int32 TickCountAfterEnd = UUnscaledTimeTickTestAbility::TickCount;
	const float AccumulatedAfterEnd = UUnscaledTimeTickTestAbility::AccumulatedRealDeltaSeconds;
	Fixture.PumpFrames(3, 0.1f);

	if (!TestEqual(TEXT("UnscaledTick task delegate is removed after EndAbility"), UUnscaledTimeTickTestAbility::TickCount, TickCountAfterEnd))
	{
		return false;
	}
	if (!TestTrue(TEXT("UnscaledTick task accumulation stops after EndAbility"), FMath::IsNearlyEqual(UUnscaledTimeTickTestAbility::AccumulatedRealDeltaSeconds, AccumulatedAfterEnd, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
	if (!TestTrue(TEXT("UnscaledTick ability spec remains findable"), AbilitySpec != nullptr))
	{
		return false;
	}
	if (!TestFalse(TEXT("UnscaledTick ability is no longer active"), AbilitySpec->IsActive()))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTaskUnscaledTickNoSubsystemEndsTaskTest, "UnscaledTime.GAS.UnscaledTick.NoSubsystemEndsTask", UnscaledTimeGASTests::TestFlags)

bool FAbilityTaskUnscaledTickNoSubsystemEndsTaskTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this, EWorldType::GamePreview))
	{
		return false;
	}

	if (!TestNull(TEXT("GamePreview world does not create UUnscaledTimeSubsystem"), Fixture.World()->GetSubsystem<UUnscaledTimeSubsystem>()))
	{
		return false;
	}

	AActor* Actor = Fixture.World()->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystemComponent = UnscaledTimeGASTests::AddAbilitySystemComponent(Actor);

	UUnscaledTimeTickTestAbility::Reset();
	const FGameplayAbilitySpecHandle AbilityHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UUnscaledTimeTickTestAbility::StaticClass(), 1, INDEX_NONE));

	AddExpectedError(TEXT("UnscaledTick could not find UUnscaledTimeSubsystem"), EAutomationExpectedErrorFlags::Contains, 1);
	if (!TestTrue(TEXT("UnscaledTick ability activates without subsystem"), AbilitySystemComponent->TryActivateAbility(AbilityHandle)))
	{
		return false;
	}
	if (!TestTrue(TEXT("UnscaledTick no-subsystem ability reached ActivateAbility"), UUnscaledTimeTickTestAbility::bActivated))
	{
		return false;
	}
	if (!TestTrue(TEXT("UnscaledTick no-subsystem ability instance is available"), UUnscaledTimeTickTestAbility::LastInstance.IsValid()))
	{
		return false;
	}
	if (!TestFalse(TEXT("UnscaledTick no-subsystem task is not active"), UUnscaledTimeTickTestAbility::LastInstance->IsTickTaskActiveForTest()))
	{
		return false;
	}
	if (!TestTrue(TEXT("UnscaledTick no-subsystem task is finished"), UUnscaledTimeTickTestAbility::LastInstance->IsTickTaskFinishedForTest()))
	{
		return false;
	}

	Fixture.PumpFrames(3, 0.1f);
	if (!TestEqual(TEXT("UnscaledTick no-subsystem task does not register a tick delegate"), UUnscaledTimeTickTestAbility::TickCount, 0))
	{
		return false;
	}

	UUnscaledTimeTickTestAbility::LastInstance->EndFromTest();
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
	if (!TestTrue(TEXT("UnscaledTick no-subsystem ability spec remains findable"), AbilitySpec != nullptr))
	{
		return false;
	}
	if (!TestFalse(TEXT("UnscaledTick no-subsystem ability is no longer active after cleanup"), AbilitySpec->IsActive()))
	{
		return false;
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
