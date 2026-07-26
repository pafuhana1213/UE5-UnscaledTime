#if WITH_DEV_AUTOMATION_TESTS

// この suite は latent delay の重複登録、retrigger、対象破棄、zero duration、
// および tick component の real-delta 購読が scaled world 経路へ戻らないことを保証する。

#include "Tests/UnscaledTimeTestHelpers.h"
#include "Tests/UnscaledTimeTestObjects.h"

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UnscaledTickComponent.h"

namespace UnscaledTimeDelayTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	UUnscaledTimeTestObject* NewTestObject(UWorld* World)
	{
		UUnscaledTimeTestObject* TestObject = NewObject<UUnscaledTimeTestObject>(World);
		TestObject->Init(World);
		return TestObject;
	}

	FLatentActionInfo MakeLatentInfo(UUnscaledTimeTestObject* TestObject, int32 UUID, int32 Linkage)
	{
		return FLatentActionInfo(
			Linkage,
			UUID,
			TEXT("LatentResume"),
			TestObject);
	}

	UUnscaledTickComponent* AddUnscaledTickComponent(AActor* Actor, bool bTickWhilePaused)
	{
		UUnscaledTickComponent* Component = NewObject<UUnscaledTickComponent>(Actor);
		Component->bTickWhilePaused = bTickWhilePaused;
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->Activate(true);
		return Component;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledDelayCompatibilityTest, "UnscaledTime.Delay.Compatibility", UnscaledTimeDelayTests::TestFlags)

bool FUnscaledDelayCompatibilityTest::RunTest(const FString& Parameters)
{
	{
		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* TestObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
		Fixture.Subsystem()->RegisterUnscaledDelay(1.0f, false, true, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 101, 1001));
		Fixture.ArmPendingTimers();
		Fixture.PumpFrames(5, 0.1f);
		Fixture.Subsystem()->RegisterUnscaledDelay(2.0f, false, true, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 101, 1002));
		Fixture.ArmPendingTimers();
		// 元の 1.0s 締切の直前まで進め、非 retriggerable の再登録が deadline を延ばしていないことを確認する。
		Fixture.PumpFrames(4, 0.1f);
		Fixture.PumpFrames(1, 0.05f);

		if (!TestEqual(TEXT("Non-retriggerable duplicate does not reset the original deadline"), TestObject->LatentResumeCount, 0))
		{
			return false;
		}

		Fixture.PumpFrames(1, 0.1f);
		if (!TestEqual(TEXT("Non-retriggerable duplicate fires once"), TestObject->LatentResumeCount, 1))
		{
			return false;
		}
		if (!TestEqual(TEXT("Non-retriggerable duplicate preserves original linkage"), TestObject->LastLatentLinkage, 1001))
		{
			return false;
		}
		if (!TestEqual(TEXT("Non-retriggerable delay removes pending entry"), Fixture.Subsystem()->GetPendingDelayCount(), 0))
		{
			return false;
		}
	}

	{
		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* TestObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
		Fixture.Subsystem()->RegisterUnscaledDelay(1.0f, true, true, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 202, 2001));
		Fixture.ArmPendingTimers();
		Fixture.PumpFrames(5, 0.1f);
		Fixture.Subsystem()->RegisterUnscaledDelay(1.0f, true, true, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 202, 2002));
		Fixture.ArmPendingTimers();
		// reset 後の 1.0s 締切直前へ着地させ、元の deadline だけでは発火しないことを切り分ける。
		Fixture.PumpFrames(9, 0.1f);
		Fixture.PumpFrames(1, 0.05f);

		if (!TestEqual(TEXT("Retriggerable delay stays pending past the original deadline but before the reset deadline"), TestObject->LatentResumeCount, 0))
		{
			return false;
		}

		Fixture.PumpFrames(1, 0.1f);
		if (!TestEqual(TEXT("Retriggerable delay fires once after re-armed deadline"), TestObject->LatentResumeCount, 1))
		{
			return false;
		}
		if (!TestEqual(TEXT("Retriggerable delay uses latest linkage"), TestObject->LastLatentLinkage, 2002))
		{
			return false;
		}
		if (!TestEqual(TEXT("Retriggerable delay removes pending entry"), Fixture.Subsystem()->GetPendingDelayCount(), 0))
		{
			return false;
		}
	}

	{
		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* DeadTarget = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
		TWeakObjectPtr<UUnscaledTimeTestObject> DeadTargetWeak = DeadTarget;
		Fixture.Subsystem()->RegisterUnscaledDelay(0.5f, false, true, UnscaledTimeDelayTests::MakeLatentInfo(DeadTarget, 303, 3001));
		Fixture.ArmPendingTimers();
		DeadTarget->MarkAsGarbage();
		DeadTarget = nullptr;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		UUnscaledTimeTestObject* FreshObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
		Fixture.PumpFrames(6, 0.1f);

		if (!TestFalse(TEXT("Callback target was collected"), DeadTargetWeak.IsValid()))
		{
			return false;
		}
		if (!TestEqual(TEXT("Collected callback target does not fire a fresh object"), FreshObject->FireCount, 0))
		{
			return false;
		}
		if (!TestEqual(TEXT("Collected callback target pending delay is cleaned"), Fixture.Subsystem()->GetPendingDelayCount(), 0))
		{
			return false;
		}
	}

	{
		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* TestObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
		Fixture.Subsystem()->RegisterUnscaledDelay(0.0f, false, true, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 404, 4001));
		Fixture.ArmPendingTimers();

		if (!TestEqual(TEXT("Zero duration delay is pending until next pumped frame"), TestObject->LatentResumeCount, 0))
		{
			return false;
		}

		Fixture.PumpFrames(1, 0.001f);
		if (!TestEqual(TEXT("Zero duration delay fires on next pumped frame"), TestObject->LatentResumeCount, 1))
		{
			return false;
		}
		if (!TestEqual(TEXT("Zero duration delay removes pending entry"), Fixture.Subsystem()->GetPendingDelayCount(), 0))
		{
			return false;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTickComponentAccumulatesRealDeltaTest, "UnscaledTime.TickComponent.AccumulatesRealDelta", UnscaledTimeDelayTests::TestFlags)

bool FUnscaledTickComponentAccumulatesRealDeltaTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	Fixture.SetDilation(0.01f);

	AActor* PausedTickActor = Fixture.World()->SpawnActor<AActor>();
	UUnscaledTickComponent* PausedTickComponent = UnscaledTimeDelayTests::AddUnscaledTickComponent(PausedTickActor, true);
	UUnscaledTimeTestObject* PausedTickObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
	PausedTickComponent->OnUnscaledTick.AddDynamic(PausedTickObject, &UUnscaledTimeTestObject::HandleUnscaledTick);

	Fixture.PumpFrames(5, 0.1f);
	if (!TestEqual(TEXT("Tick-while-paused component ticks once per pumped frame under dilation"), PausedTickObject->TickCount, 5))
	{
		return false;
	}
	if (!TestTrue(TEXT("Tick-while-paused component accumulates real delta under dilation"), FMath::IsNearlyEqual(PausedTickObject->AccumulatedRealDeltaSeconds, 0.5f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	Fixture.SetPaused(true);
	Fixture.PumpFrames(5, 0.1f);
	if (!TestEqual(TEXT("Tick-while-paused component keeps ticking while paused"), PausedTickObject->TickCount, 10))
	{
		return false;
	}
	if (!TestTrue(TEXT("Tick-while-paused component accumulates paused real delta"), FMath::IsNearlyEqual(PausedTickObject->AccumulatedRealDeltaSeconds, 1.0f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	Fixture.SetPaused(false);

	AActor* UnpausedOnlyActor = Fixture.World()->SpawnActor<AActor>();
	UUnscaledTickComponent* UnpausedOnlyComponent = UnscaledTimeDelayTests::AddUnscaledTickComponent(UnpausedOnlyActor, false);
	UUnscaledTimeTestObject* UnpausedOnlyObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
	UnpausedOnlyComponent->OnUnscaledTick.AddDynamic(UnpausedOnlyObject, &UUnscaledTimeTestObject::HandleUnscaledTick);

	Fixture.PumpFrames(3, 0.1f);
	if (!TestEqual(TEXT("Unpaused-only component ticks while unpaused"), UnpausedOnlyObject->TickCount, 3))
	{
		return false;
	}
	if (!TestTrue(TEXT("Unpaused-only component accumulates unpaused real delta"), FMath::IsNearlyEqual(UnpausedOnlyObject->AccumulatedRealDeltaSeconds, 0.3f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	Fixture.SetPaused(true);
	Fixture.PumpFrames(3, 0.1f);
	if (!TestEqual(TEXT("Unpaused-only component stops ticking while paused"), UnpausedOnlyObject->TickCount, 3))
	{
		return false;
	}
	if (!TestTrue(TEXT("Unpaused-only component accumulation is unchanged while paused"), FMath::IsNearlyEqual(UnpausedOnlyObject->AccumulatedRealDeltaSeconds, 0.3f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTickComponentRegistrationCyclingTest, "UnscaledTime.TickComponent.RegistrationCycling", UnscaledTimeDelayTests::TestFlags)

bool FUnscaledTickComponentRegistrationCyclingTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	AActor* TickActor = Fixture.World()->SpawnActor<AActor>();
	UUnscaledTickComponent* TickComponent = UnscaledTimeDelayTests::AddUnscaledTickComponent(TickActor, true);
	UUnscaledTimeTestObject* TickObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
	TickComponent->OnUnscaledTick.AddDynamic(TickObject, &UUnscaledTimeTestObject::HandleUnscaledTick);

	Fixture.PumpFrames(3, 0.1f);
	if (!TestEqual(TEXT("Registered tick component ticks once per pump"), TickObject->TickCount, 3))
	{
		return false;
	}
	if (!TestTrue(TEXT("Registered tick component accumulates per-pump delta"), FMath::IsNearlyEqual(TickObject->AccumulatedRealDeltaSeconds, 0.3f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	TickComponent->UnregisterComponent();
	Fixture.PumpFrames(3, 0.1f);
	if (!TestEqual(TEXT("Unregistered tick component stops receiving subsystem ticks"), TickObject->TickCount, 3))
	{
		return false;
	}
	if (!TestTrue(TEXT("Unregistered tick component accumulation does not change"), FMath::IsNearlyEqual(TickObject->AccumulatedRealDeltaSeconds, 0.3f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	TickComponent->RegisterComponent();
	TickComponent->Activate(true);
	Fixture.PumpFrames(3, 0.1f);
	if (!TestEqual(TEXT("Re-registered tick component resumes once per pump"), TickObject->TickCount, 6))
	{
		return false;
	}
	if (!TestTrue(TEXT("Re-registered tick component accumulation resumes"), FMath::IsNearlyEqual(TickObject->AccumulatedRealDeltaSeconds, 0.6f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	TickComponent->Deactivate();
	Fixture.PumpFrames(2, 0.1f);
	if (!TestEqual(TEXT("Deactivated tick component stops receiving subsystem ticks"), TickObject->TickCount, 6))
	{
		return false;
	}

	TickComponent->Activate(true);
	TickComponent->Activate(true);
	Fixture.PumpFrames(2, 0.1f);
	if (!TestEqual(TEXT("Repeated activation does not double-subscribe"), TickObject->TickCount, 8))
	{
		return false;
	}
	if (!TestTrue(TEXT("Repeated activation accumulates each pump exactly once"), FMath::IsNearlyEqual(TickObject->AccumulatedRealDeltaSeconds, 0.8f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledDelayRetriggerClockChangeTest, "UnscaledTime.Delay.RetriggerClockChange", UnscaledTimeDelayTests::TestFlags)

bool FUnscaledDelayRetriggerClockChangeTest::RunTest(const FString& Parameters)
{
	{
		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* TestObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
		Fixture.Subsystem()->RegisterUnscaledDelay(1.0f, true, true, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 505, 5001));
		Fixture.ArmPendingTimers();
		Fixture.PumpFrames(4, 0.1f);
		Fixture.Subsystem()->RegisterUnscaledDelay(1.0f, true, false, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 505, 5002));
		Fixture.ArmPendingTimers();

		Fixture.SetPaused(true);
		Fixture.PumpFrames(11, 0.1f);
		if (!TestEqual(TEXT("RealTime to unpaused retrigger does not fire while paused"), TestObject->LatentResumeCount, 0))
		{
			return false;
		}

		Fixture.SetPaused(false);
		// unpause 後の unpaused clock を deadline 直前まで進め、paused 中の pump と混ざらないことを確認する。
		Fixture.PumpFrames(9, 0.1f);
		Fixture.PumpFrames(1, 0.05f);
		if (!TestEqual(TEXT("RealTime to unpaused retrigger does not fire before reset unpaused deadline"), TestObject->LatentResumeCount, 0))
		{
			return false;
		}

		Fixture.PumpFrames(1, 0.1f);
		if (!TestEqual(TEXT("RealTime to unpaused retrigger fires after unpause at reset deadline"), TestObject->LatentResumeCount, 1))
		{
			return false;
		}
		if (!TestEqual(TEXT("RealTime to unpaused retrigger uses latest linkage"), TestObject->LastLatentLinkage, 5002))
		{
			return false;
		}
	}

	{
		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* TestObject = UnscaledTimeDelayTests::NewTestObject(Fixture.World());
		Fixture.Subsystem()->RegisterUnscaledDelay(1.0f, true, false, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 606, 6001));
		Fixture.ArmPendingTimers();
		Fixture.PumpFrames(4, 0.1f);

		Fixture.SetPaused(true);
		Fixture.Subsystem()->RegisterUnscaledDelay(1.0f, true, true, UnscaledTimeDelayTests::MakeLatentInfo(TestObject, 606, 6002));
		Fixture.ArmPendingTimers();
		// RealTime へ移した後の paused 中 deadline 直前へ着地させ、clock 移行後の締切だけを検証する。
		Fixture.PumpFrames(9, 0.1f);
		Fixture.PumpFrames(1, 0.05f);
		if (!TestEqual(TEXT("Unpaused to RealTime retrigger does not fire before reset paused deadline"), TestObject->LatentResumeCount, 0))
		{
			return false;
		}

		Fixture.PumpFrames(1, 0.1f);
		if (!TestEqual(TEXT("Unpaused to RealTime retrigger fires while paused"), TestObject->LatentResumeCount, 1))
		{
			return false;
		}
		if (!TestEqual(TEXT("Unpaused to RealTime retrigger uses latest linkage"), TestObject->LastLatentLinkage, 6002))
		{
			return false;
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
