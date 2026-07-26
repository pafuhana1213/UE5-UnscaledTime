#include "Tests/UnscaledTimeTestObjects.h"

#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UnscaledTimeTestObjects)

void UUnscaledTimeTestObject::Init(UWorld* InWorld)
{
	TestWorld = InWorld;
}

UWorld* UUnscaledTimeTestObject::GetWorld() const
{
	return TestWorld.Get();
}

void UUnscaledTimeTestObject::HandleTimerFired()
{
	++FireCount;
}

void UUnscaledTimeTestObject::LatentResume(int32 Linkage)
{
	++LatentResumeCount;
	LastLatentLinkage = Linkage;
}

void UUnscaledTimeTestObject::HandleUnscaledTick(float RealDeltaSeconds)
{
	++TickCount;
	LastTickDeltaSeconds = RealDeltaSeconds;
	AccumulatedRealDeltaSeconds += RealDeltaSeconds;
}

