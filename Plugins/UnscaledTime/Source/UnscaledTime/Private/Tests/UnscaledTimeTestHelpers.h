#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationCommon.h"
#include "UnscaledTimeSettings.h"
#include "UnscaledTimeSubsystem.h"

struct FScopedUnscaledTimeSettings
{
	FScopedUnscaledTimeSettings()
	{
		UUnscaledTimeSettings* Settings = GetMutableDefault<UUnscaledTimeSettings>();
		OriginalReferenceFrameRate = Settings->ReferenceFrameRate;
		OriginalMaxRealDeltaSeconds = Settings->MaxRealDeltaSeconds;
	}

	~FScopedUnscaledTimeSettings()
	{
		UUnscaledTimeSettings* Settings = GetMutableDefault<UUnscaledTimeSettings>();
		Settings->ReferenceFrameRate = OriginalReferenceFrameRate;
		Settings->MaxRealDeltaSeconds = OriginalMaxRealDeltaSeconds;
	}

	void SetReferenceFrameRate(float ReferenceFrameRate) const
	{
		GetMutableDefault<UUnscaledTimeSettings>()->ReferenceFrameRate = ReferenceFrameRate;
	}

	void SetMaxRealDeltaSeconds(float MaxRealDeltaSeconds) const
	{
		GetMutableDefault<UUnscaledTimeSettings>()->MaxRealDeltaSeconds = MaxRealDeltaSeconds;
	}

private:
	float OriginalReferenceFrameRate = 60.f;
	float OriginalMaxRealDeltaSeconds = 0.5f;
};

struct FUnscaledTimeTestFixture
{
	~FUnscaledTimeTestFixture()
	{
		SetPaused(false);
	}

	bool CreateAndBeginPlay(FAutomationTestBase& Test, EWorldType::Type WorldType = EWorldType::Game)
	{
		const bool bCreated = WorldWrapper.CreateTestWorld(WorldType);
		const bool bBegunPlay = bCreated && WorldWrapper.BeginPlayInTestWorld();
		// Warm-up tick with zero delta: the subsystem's FTimerManagers can already have
		// LastTickedFrame == GFrameCounter for the engine frame that created the world,
		// so their first Tick that frame is dropped by HasBeenTickedThisFrame(). Consume
		// that poisoned frame here so subsequent pumped deltas all reach the managers.
		if (bBegunPlay)
		{
			WorldWrapper.TickTestWorld(0.f);
		}
		ForwardErrors(Test);
		const bool bRequiresSubsystem = WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
		return bCreated && bBegunPlay && World() && (!bRequiresSubsystem || Subsystem());
	}

	UWorld* World() const
	{
		return WorldWrapper.GetTestWorld();
	}

	UUnscaledTimeSubsystem* Subsystem() const
	{
		UWorld* TestWorld = World();
		return TestWorld ? TestWorld->GetSubsystem<UUnscaledTimeSubsystem>() : nullptr;
	}

	void SetDilation(float Dilation) const
	{
		if (AWorldSettings* WorldSettings = World() ? World()->GetWorldSettings() : nullptr)
		{
			WorldSettings->SetTimeDilation(Dilation);
		}
	}

	void SetPaused(bool bPaused)
	{
		UWorld* TestWorld = World();
		AWorldSettings* WorldSettings = TestWorld ? TestWorld->GetWorldSettings() : nullptr;
		if (!WorldSettings)
		{
			return;
		}

		if (bPaused)
		{
			if (!PauserPlayerState)
			{
				PauserPlayerState = TestWorld->SpawnActor<APlayerState>();
			}
			WorldSettings->SetPauserPlayerState(PauserPlayerState);
		}
		else
		{
			WorldSettings->SetPauserPlayerState(nullptr);
		}
	}

	void PumpFrames(int32 NumFrames, float DeltaSeconds)
	{
		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			WorldWrapper.TickTestWorld(DeltaSeconds);
		}
	}

	void ArmPendingTimers()
	{
		PumpFrames(1, 0.f);
	}

	void ForwardErrors(FAutomationTestBase& Test)
	{
		WorldWrapper.ForwardErrorMessages(&Test);
		WorldWrapper.ClearFailureState();
	}

private:
	FTestWorldWrapper WorldWrapper;
	APlayerState* PauserPlayerState = nullptr;
};

#endif // WITH_DEV_AUTOMATION_TESTS
