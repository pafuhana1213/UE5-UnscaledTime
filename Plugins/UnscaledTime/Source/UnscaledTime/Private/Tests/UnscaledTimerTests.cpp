#if WITH_DEV_AUTOMATION_TESTS

// この suite は Blueprint/C++ timer API が選択した unscaled clock へ正しくルーティングされ、
// dilation、pause、handle 操作、frame 変換、delta clamp の境界を保つことを保証する。

#include "Tests/UnscaledTimeTestHelpers.h"
#include "Tests/UnscaledTimeTestObjects.h"

#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UnscaledTimeBlueprintLibrary.h"

namespace UnscaledTimeTimerTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	UUnscaledTimeTestObject* NewTestObject(UWorld* World)
	{
		UUnscaledTimeTestObject* TestObject = NewObject<UUnscaledTimeTestObject>(World);
		TestObject->Init(World);
		return TestObject;
	}

	FTimerDynamicDelegate MakeTimerDelegate(UUnscaledTimeTestObject* TestObject)
	{
		FTimerDynamicDelegate Delegate;
		Delegate.BindUFunction(TestObject, GET_FUNCTION_NAME_CHECKED(UUnscaledTimeTestObject, HandleTimerFired));
		return Delegate;
	}

	FUnscaledTimerHandle SetCppTimer(UUnscaledTimeSubsystem* Subsystem, EUnscaledTimeClock Clock, UUnscaledTimeTestObject* TestObject, float Time)
	{
		FUnscaledTimerHandle Handle;
		Handle.Clock = Clock;
		Subsystem->GetTimerManager(Clock).SetTimer(
			Handle.Handle,
			FTimerDelegate::CreateUObject(TestObject, &UUnscaledTimeTestObject::HandleTimerFired),
			Time,
			false);
		return Handle;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTimerFiresRealTimeUnderDilationTest, "UnscaledTime.Timer.FiresRealTimeUnderDilation", UnscaledTimeTimerTests::TestFlags)

bool FUnscaledTimerFiresRealTimeUnderDilationTest::RunTest(const FString& Parameters)
{
	for (float Dilation : { 0.01f, 100.f })
	{
		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		Fixture.SetDilation(Dilation);
		UUnscaledTimeTestObject* TestObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
		const FUnscaledTimerHandle Handle = UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerDelegate(
			UnscaledTimeTimerTests::MakeTimerDelegate(TestObject),
			1.0f,
			false,
			true);

		if (!TestTrue(TEXT("Blueprint timer handle is valid"), Handle.IsValid()))
		{
			return false;
		}

		Fixture.ArmPendingTimers();
		// 1.0s 締切の直前へ着地させ、dilation 値に関係なく早期発火していないことを検証する。
		Fixture.PumpFrames(9, 0.1f);
		Fixture.PumpFrames(1, 0.05f);
		if (!TestEqual(TEXT("Timer does not fire before 1.0 seconds"), TestObject->FireCount, 0))
		{
			return false;
		}

		Fixture.PumpFrames(1, 0.1f);
		if (!TestEqual(FString::Printf(TEXT("Timer fires once after 1.0 seconds at dilation %.2f"), Dilation), TestObject->FireCount, 1))
		{
			return false;
		}

		Fixture.PumpFrames(10, 0.1f);
		if (!TestEqual(TEXT("One-shot timer does not fire again"), TestObject->FireCount, 1))
		{
			return false;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTimerFiresWhilePausedTest, "UnscaledTime.Timer.FiresWhilePaused", UnscaledTimeTimerTests::TestFlags)

bool FUnscaledTimerFiresWhilePausedTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	UUnscaledTimeTestObject* RealTimeObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
	UUnscaledTimeTestObject* VanillaObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());

	FTimerHandle VanillaHandle;
	Fixture.Subsystem()->GetTimerManager(EUnscaledTimeClock::RealTime).SetTimer(
		VanillaHandle,
		FTimerDelegate::CreateUObject(RealTimeObject, &UUnscaledTimeTestObject::HandleTimerFired),
		1.0f,
		false);

	FTimerHandle WorldTimerHandle;
	Fixture.World()->GetTimerManager().SetTimer(
		WorldTimerHandle,
		FTimerDelegate::CreateUObject(VanillaObject, &UUnscaledTimeTestObject::HandleTimerFired),
		1.0f,
		false);

	Fixture.ArmPendingTimers();
	Fixture.SetPaused(true);
	Fixture.PumpFrames(11, 0.1f);

	if (!TestEqual(TEXT("RealTime timer fires while paused"), RealTimeObject->FireCount, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("World timer does not fire while paused"), VanillaObject->FireCount, 0))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTimerUnpausedClockStopsWhilePausedTest, "UnscaledTime.Timer.UnpausedClockStopsWhilePaused", UnscaledTimeTimerTests::TestFlags)

bool FUnscaledTimerUnpausedClockStopsWhilePausedTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	UUnscaledTimeTestObject* TestObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
	FUnscaledTimerHandle Handle = UnscaledTimeTimerTests::SetCppTimer(Fixture.Subsystem(), EUnscaledTimeClock::RealTimeUnpaused, TestObject, 1.0f);

	Fixture.ArmPendingTimers();
	Fixture.SetPaused(true);
	const float RemainingBeforePausePump = Fixture.Subsystem()->GetTimerManager(EUnscaledTimeClock::RealTimeUnpaused).GetTimerRemaining(Handle.Handle);
	Fixture.PumpFrames(5, 0.1f);
	const float RemainingAfterPausePump = Fixture.Subsystem()->GetTimerManager(EUnscaledTimeClock::RealTimeUnpaused).GetTimerRemaining(Handle.Handle);

	if (!TestTrue(TEXT("Unpaused timer remaining is unchanged while paused"), FMath::IsNearlyEqual(RemainingBeforePausePump, RemainingAfterPausePump, KINDA_SMALL_NUMBER)))
	{
		return false;
	}
	if (!TestEqual(TEXT("Unpaused timer does not fire while paused"), TestObject->FireCount, 0))
	{
		return false;
	}

	Fixture.SetPaused(false);
	// unpaused clock の経過だけで 1.0s 締切直前へ進め、paused 中の pump が残り時間を削っていないことを確認する。
	Fixture.PumpFrames(9, 0.1f);
	Fixture.PumpFrames(1, 0.05f);
	if (!TestEqual(TEXT("Unpaused timer does not fire before 1.0 unpaused seconds"), TestObject->FireCount, 0))
	{
		return false;
	}

	Fixture.PumpFrames(1, 0.1f);
	if (!TestEqual(TEXT("Unpaused timer fires after unpaused real time elapses"), TestObject->FireCount, 1))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTimerHandleRoutesOperationsTest, "UnscaledTime.Timer.HandleRoutesOperations", UnscaledTimeTimerTests::TestFlags)

bool FUnscaledTimerHandleRoutesOperationsTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	UUnscaledTimeTestObject* RealTimeObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
	UUnscaledTimeTestObject* UnpausedObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
	FUnscaledTimerHandle RealTimeHandle = UnscaledTimeTimerTests::SetCppTimer(Fixture.Subsystem(), EUnscaledTimeClock::RealTime, RealTimeObject, 2.0f);
	FUnscaledTimerHandle UnpausedHandle = UnscaledTimeTimerTests::SetCppTimer(Fixture.Subsystem(), EUnscaledTimeClock::RealTimeUnpaused, UnpausedObject, 2.0f);
	Fixture.ArmPendingTimers();

	Fixture.PumpFrames(5, 0.1f);
	if (!TestTrue(TEXT("RealTime elapsed routes to RealTime manager"), FMath::IsNearlyEqual(UUnscaledTimeBlueprintLibrary::GetUnscaledTimerElapsed(Fixture.World(), RealTimeHandle), 0.5f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Unpaused remaining routes to Unpaused manager"), FMath::IsNearlyEqual(UUnscaledTimeBlueprintLibrary::GetUnscaledTimerRemaining(Fixture.World(), UnpausedHandle), 1.5f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	UUnscaledTimeBlueprintLibrary::PauseUnscaledTimer(Fixture.World(), RealTimeHandle);
	if (!TestTrue(TEXT("Pause routes to RealTime manager"), UUnscaledTimeBlueprintLibrary::IsUnscaledTimerPaused(Fixture.World(), RealTimeHandle)))
	{
		return false;
	}

	Fixture.PumpFrames(5, 0.1f);
	if (!TestTrue(TEXT("Paused RealTime timer does not advance"), FMath::IsNearlyEqual(UUnscaledTimeBlueprintLibrary::GetUnscaledTimerElapsed(Fixture.World(), RealTimeHandle), 0.5f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Unpaused timer keeps advancing through its own handle"), FMath::IsNearlyEqual(UUnscaledTimeBlueprintLibrary::GetUnscaledTimerElapsed(Fixture.World(), UnpausedHandle), 1.0f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	UUnscaledTimeBlueprintLibrary::UnPauseUnscaledTimer(Fixture.World(), RealTimeHandle);
	if (!TestFalse(TEXT("UnPause routes to RealTime manager"), UUnscaledTimeBlueprintLibrary::IsUnscaledTimerPaused(Fixture.World(), RealTimeHandle)))
	{
		return false;
	}

	Fixture.PumpFrames(3, 0.1f);
	if (!TestTrue(TEXT("Unpaused RealTime timer resumes advancing"), UUnscaledTimeBlueprintLibrary::GetUnscaledTimerElapsed(Fixture.World(), RealTimeHandle) > 0.5f))
	{
		return false;
	}

	UUnscaledTimeBlueprintLibrary::ClearAndInvalidateUnscaledTimer(Fixture.World(), RealTimeHandle);
	if (!TestFalse(TEXT("ClearAndInvalidate invalidates handle"), UUnscaledTimeBlueprintLibrary::IsUnscaledTimerHandleValid(RealTimeHandle)))
	{
		return false;
	}

	Fixture.PumpFrames(10, 0.1f);
	Fixture.PumpFrames(1, 0.001f);
	if (!TestEqual(TEXT("Cleared RealTime timer does not fire"), RealTimeObject->FireCount, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("Unpaused timer fires through its routed manager"), UnpausedObject->FireCount, 1))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTimerByEventUniqueAcrossClocksTest, "UnscaledTime.Timer.ByEventUniqueAcrossClocks", UnscaledTimeTimerTests::TestFlags)

bool FUnscaledTimerByEventUniqueAcrossClocksTest::RunTest(const FString& Parameters)
{
	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	UUnscaledTimeTestObject* TestObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
	FTimerDynamicDelegate Delegate = UnscaledTimeTimerTests::MakeTimerDelegate(TestObject);

	const FUnscaledTimerHandle FirstHandle = UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerDelegate(
		Delegate,
		1.0f,
		true,
		true);
	Fixture.ArmPendingTimers();
	const FUnscaledTimerHandle SecondHandle = UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerDelegate(
		Delegate,
		1.0f,
		true,
		false);
	Fixture.ArmPendingTimers();

	if (!TestFalse(TEXT("Re-arming by event clears the opposite RealTime clock timer"), Fixture.Subsystem()->GetTimerManager(EUnscaledTimeClock::RealTime).TimerExists(FirstHandle.Handle)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Re-arming by event creates one timer on the selected unpaused clock"), Fixture.Subsystem()->GetTimerManager(EUnscaledTimeClock::RealTimeUnpaused).TimerExists(SecondHandle.Handle)))
	{
		return false;
	}

	Fixture.SetPaused(true);
	Fixture.PumpFrames(11, 0.1f);
	if (!TestEqual(TEXT("By-event timer follows second unpaused registration and does not fire while paused"), TestObject->FireCount, 0))
	{
		return false;
	}

	Fixture.SetPaused(false);
	// 2 回目の by-event 登録を基準に deadline 直前へ進め、古い clock 側の登録が残っていないことを切り分ける。
	Fixture.PumpFrames(9, 0.1f);
	Fixture.PumpFrames(1, 0.05f);
	if (!TestEqual(TEXT("By-event timer does not fire before second registration deadline"), TestObject->FireCount, 0))
	{
		return false;
	}

	Fixture.PumpFrames(1, 0.1f);
	if (!TestEqual(TEXT("By-event timer fires once on the second registration clock"), TestObject->FireCount, 1))
	{
		return false;
	}

	UUnscaledTimeBlueprintLibrary::ClearAndInvalidateUnscaledTimerByEvent(Delegate);
	if (!TestFalse(TEXT("By-event clear removes RealTime timer"), Fixture.Subsystem()->GetTimerManager(EUnscaledTimeClock::RealTime).K2_FindDynamicTimerHandle(Delegate).IsValid()))
	{
		return false;
	}
	if (!TestFalse(TEXT("By-event clear removes unpaused timer"), Fixture.Subsystem()->GetTimerManager(EUnscaledTimeClock::RealTimeUnpaused).K2_FindDynamicTimerHandle(Delegate).IsValid()))
	{
		return false;
	}

	const int32 FireCountAfterClear = TestObject->FireCount;
	Fixture.PumpFrames(12, 0.1f);
	if (!TestEqual(TEXT("By-event clear prevents further looping timer fires"), TestObject->FireCount, FireCountAfterClear))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTimerClampRealDeltaTest, "UnscaledTime.Timer.ClampRealDelta", UnscaledTimeTimerTests::TestFlags)

bool FUnscaledTimerClampRealDeltaTest::RunTest(const FString& Parameters)
{
	FScopedUnscaledTimeSettings Settings;
	Settings.SetMaxRealDeltaSeconds(0.5f);

	FUnscaledTimeTestFixture Fixture;
	if (!Fixture.CreateAndBeginPlay(*this))
	{
		return false;
	}

	Fixture.PumpFrames(1, 10.0f);

	if (!TestTrue(TEXT("Last real delta is clamped"), FMath::IsNearlyEqual(Fixture.Subsystem()->GetLastRealDeltaSeconds(), 0.5f, KINDA_SMALL_NUMBER)))
	{
		return false;
	}
	if (!TestTrue(TEXT("RealTime clock advances by clamped delta"), FMath::IsNearlyEqual(Fixture.Subsystem()->GetUnscaledTimeSeconds(EUnscaledTimeClock::RealTime), 0.5, KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnscaledTimerFramesUseReferenceFrameRateTest, "UnscaledTime.Timer.FramesUseReferenceFrameRate", UnscaledTimeTimerTests::TestFlags)

bool FUnscaledTimerFramesUseReferenceFrameRateTest::RunTest(const FString& Parameters)
{
	FScopedUnscaledTimeSettings Settings;

	{
		Settings.SetReferenceFrameRate(60.0f);
		if (!TestTrue(TEXT("30 frames at 60fps converts to 0.5 seconds"), FMath::IsNearlyEqual(UUnscaledTimeBlueprintLibrary::UnscaledFramesToSeconds(30), 0.5f, KINDA_SMALL_NUMBER)))
		{
			return false;
		}
		if (!TestEqual(TEXT("0.5 seconds at 60fps converts to 30 frames"), UUnscaledTimeBlueprintLibrary::UnscaledSecondsToFrames(0.5f), 30))
		{
			return false;
		}

		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* TestObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
		UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerDelegateByFrames(
			UnscaledTimeTimerTests::MakeTimerDelegate(TestObject),
			30,
			false,
			true);

		Fixture.ArmPendingTimers();
		// 30 frames @ 60fps の 0.5s 締切直前へ着地させ、変換後の timer duration を境界で見る。
		Fixture.PumpFrames(4, 0.1f);
		Fixture.PumpFrames(1, 0.05f);
		if (!TestEqual(TEXT("30-frame timer at 60fps does not fire before 0.5s"), TestObject->FireCount, 0))
		{
			return false;
		}
		Fixture.PumpFrames(1, 0.1f);
		if (!TestEqual(TEXT("30-frame timer at 60fps fires after 0.5s"), TestObject->FireCount, 1))
		{
			return false;
		}
	}

	{
		Settings.SetReferenceFrameRate(30.0f);
		if (!TestTrue(TEXT("30 frames at 30fps converts to 1.0 seconds"), FMath::IsNearlyEqual(UUnscaledTimeBlueprintLibrary::UnscaledFramesToSeconds(30), 1.0f, KINDA_SMALL_NUMBER)))
		{
			return false;
		}
		if (!TestEqual(TEXT("1.0 second at 30fps converts to 30 frames"), UUnscaledTimeBlueprintLibrary::UnscaledSecondsToFrames(1.0f), 30))
		{
			return false;
		}

		FUnscaledTimeTestFixture Fixture;
		if (!Fixture.CreateAndBeginPlay(*this))
		{
			return false;
		}

		UUnscaledTimeTestObject* TestObject = UnscaledTimeTimerTests::NewTestObject(Fixture.World());
		UUnscaledTimeBlueprintLibrary::K2_SetUnscaledTimerDelegateByFrames(
			UnscaledTimeTimerTests::MakeTimerDelegate(TestObject),
			30,
			false,
			true);

		Fixture.ArmPendingTimers();
		// 30 frames @ 30fps の 1.0s 締切直前へ着地させ、reference rate 変更が timer に反映されたことを見る。
		Fixture.PumpFrames(9, 0.1f);
		Fixture.PumpFrames(1, 0.05f);
		if (!TestEqual(TEXT("30-frame timer at 30fps does not fire before 1.0s"), TestObject->FireCount, 0))
		{
			return false;
		}
		Fixture.PumpFrames(1, 0.1f);
		if (!TestEqual(TEXT("30-frame timer at 30fps fires after 1.0s"), TestObject->FireCount, 1))
		{
			return false;
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
