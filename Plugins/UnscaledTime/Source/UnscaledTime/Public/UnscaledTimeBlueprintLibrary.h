#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TimerManager.h"
#include "UnscaledTimeTypes.h"
#include "UnscaledTimeBlueprintLibrary.generated.h"

UCLASS()
class UNSCALEDTIME_API UUnscaledTimeBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Performs a latent delay using unscaled real time, ignoring later calls while it is counting down. If bTickWhilePaused is true, the delay continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Unscaled Delay", Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject", Duration="0.2", KeyWords="sleep delay realtime unscaled", UnsafeDuringActorConstruction="true"), Category="UnscaledTime|FlowControl")
	static void UnscaledDelay(const UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo, bool bTickWhilePaused = true);

	/** Performs a retriggerable latent delay using unscaled real time, resetting the countdown to Duration when called again while counting down. If bTickWhilePaused is true, the delay continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Unscaled Retriggerable Delay", Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject", Duration="0.2", KeyWords="sleep delay realtime unscaled", UnsafeDuringActorConstruction="true"), Category="UnscaledTime|FlowControl")
	static void UnscaledRetriggerableDelay(const UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo, bool bTickWhilePaused = true);

	/** Performs a frame-converted latent delay using unscaled real time, ignoring later calls while it is counting down. If bTickWhilePaused is true, the delay continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Unscaled Delay (Frames)", Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject", FrameDuration="12", KeyWords="sleep delay realtime unscaled", UnsafeDuringActorConstruction="true"), Category="UnscaledTime|FlowControl")
	static void UnscaledDelayByFrames(const UObject* WorldContextObject, int32 FrameDuration, FLatentActionInfo LatentInfo, bool bTickWhilePaused = true);

	/** Performs a retriggerable frame-converted latent delay using unscaled real time, resetting the countdown to FrameDuration when called again while counting down. If bTickWhilePaused is true, the delay continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Unscaled Retriggerable Delay (Frames)", Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject", FrameDuration="12", KeyWords="sleep delay realtime unscaled", UnsafeDuringActorConstruction="true"), Category="UnscaledTime|FlowControl")
	static void UnscaledRetriggerableDelayByFrames(const UObject* WorldContextObject, int32 FrameDuration, FLatentActionInfo LatentInfo, bool bTickWhilePaused = true);

	/** Sets a timer using unscaled real time. If bTickWhilePaused is true, the timer continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Set Unscaled Timer by Event", ScriptName="SetUnscaledTimerDelegate", AdvancedDisplay="bMaxOncePerFrame, InitialStartDelay, InitialStartDelayVariance"), Category="UnscaledTime|Timer")
	static FUnscaledTimerHandle K2_SetUnscaledTimerDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate, float Time, bool bLooping, bool bTickWhilePaused = true, bool bMaxOncePerFrame = false, float InitialStartDelay = 0.f, float InitialStartDelayVariance = 0.f);

	/** Sets a timer by function name using unscaled real time. If bTickWhilePaused is true, the timer continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Set Unscaled Timer by Function Name", ScriptName="SetUnscaledTimer", DefaultToSelf="Object", AdvancedDisplay="bMaxOncePerFrame, InitialStartDelay, InitialStartDelayVariance"), Category="UnscaledTime|Timer")
	static FUnscaledTimerHandle K2_SetUnscaledTimer(UObject* Object, FString FunctionName, float Time, bool bLooping, bool bTickWhilePaused = true, bool bMaxOncePerFrame = false, float InitialStartDelay = 0.f, float InitialStartDelayVariance = 0.f);

	/** Sets a frame-converted timer using unscaled real time. If bTickWhilePaused is true, the timer continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Set Unscaled Timer by Event (Frames)", ScriptName="SetUnscaledTimerDelegateByFrames"), Category="UnscaledTime|Timer")
	static FUnscaledTimerHandle K2_SetUnscaledTimerDelegateByFrames(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate, int32 FrameInterval, bool bLooping, bool bTickWhilePaused = true, bool bMaxOncePerFrame = false);

	/** Sets a frame-converted timer by function name using unscaled real time. If bTickWhilePaused is true, the timer continues while the world is paused; otherwise it stops while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Set Unscaled Timer by Function Name (Frames)", ScriptName="SetUnscaledTimerByFrames", DefaultToSelf="Object"), Category="UnscaledTime|Timer")
	static FUnscaledTimerHandle K2_SetUnscaledTimerByFrames(UObject* Object, FString FunctionName, int32 FrameInterval, bool bLooping, bool bTickWhilePaused = true, bool bMaxOncePerFrame = false);

	/** Clears an unscaled real-time timer and invalidates its handle. The handle's clock selects whether pause should affect the timer manager used. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Clear and Invalidate Unscaled Timer by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static void ClearAndInvalidateUnscaledTimer(const UObject* WorldContextObject, UPARAM(ref) FUnscaledTimerHandle& Handle);

	/** Pauses an unscaled real-time timer. The handle's clock selects whether pause should affect the timer manager used. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Pause Unscaled Timer by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static void PauseUnscaledTimer(const UObject* WorldContextObject, FUnscaledTimerHandle Handle);

	/** Resumes a paused unscaled real-time timer. The handle's clock selects whether pause should affect the timer manager used. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Unpause Unscaled Timer by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static void UnPauseUnscaledTimer(const UObject* WorldContextObject, FUnscaledTimerHandle Handle);

	/** Returns true when an unscaled real-time timer exists and is active. The handle's clock selects whether pause should affect the timer manager used. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Unscaled Timer Active by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static bool IsUnscaledTimerActive(const UObject* WorldContextObject, FUnscaledTimerHandle Handle);

	/** Returns true when an unscaled real-time timer exists and is paused. The handle's clock selects whether pause should affect the timer manager used. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Unscaled Timer Paused by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static bool IsUnscaledTimerPaused(const UObject* WorldContextObject, FUnscaledTimerHandle Handle);

	/** Returns true when an unscaled real-time timer exists. The handle's clock selects whether pause should affect the timer manager used. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Does Unscaled Timer Exist by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static bool UnscaledTimerExists(const UObject* WorldContextObject, FUnscaledTimerHandle Handle);

	/** Returns elapsed unscaled real time for a timer, or 0 if the handle is invalid or no subsystem exists. The handle's clock selects the timer manager; a valid handle with no timer follows FTimerManager and returns -1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Get Unscaled Timer Elapsed Time by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static float GetUnscaledTimerElapsed(const UObject* WorldContextObject, FUnscaledTimerHandle Handle);

	/** Returns remaining unscaled real time for a timer, or 0 if the handle is invalid or no subsystem exists. The handle's clock selects the timer manager; a valid handle with no timer follows FTimerManager and returns -1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Get Unscaled Timer Remaining Time by Handle", WorldContext="WorldContextObject"), Category="UnscaledTime|Timer")
	static float GetUnscaledTimerRemaining(const UObject* WorldContextObject, FUnscaledTimerHandle Handle);

	/** Returns whether an unscaled real-time timer handle once referenced a timer. This does not require a world and does not check pause behavior. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Valid Unscaled Timer Handle"), Category="UnscaledTime|Timer")
	static bool IsUnscaledTimerHandleValid(FUnscaledTimerHandle Handle);

	/** Clears an unscaled real-time timer by event. Searches timers that tick while paused before timers that stop while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Clear Unscaled Timer by Event", ScriptName="ClearUnscaledTimerDelegate"), Category="UnscaledTime|Timer")
	static void ClearAndInvalidateUnscaledTimerByEvent(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/** Pauses an unscaled real-time timer by event. Searches timers that tick while paused before timers that stop while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Pause Unscaled Timer by Event", ScriptName="PauseUnscaledTimerDelegate"), Category="UnscaledTime|Timer")
	static void PauseUnscaledTimerByEvent(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/** Resumes an unscaled real-time timer by event. Searches timers that tick while paused before timers that stop while paused. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Unpause Unscaled Timer by Event", ScriptName="UnPauseUnscaledTimerDelegate"), Category="UnscaledTime|Timer")
	static void UnPauseUnscaledTimerByEvent(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/** Returns true when an unscaled real-time timer exists and is active for the event. Searches timers that tick while paused before timers that stop while paused. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Unscaled Timer Active by Event", ScriptName="IsUnscaledTimerActiveDelegate"), Category="UnscaledTime|Timer")
	static bool IsUnscaledTimerActiveByEvent(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/** Returns true when an unscaled real-time timer exists and is paused for the event. Searches timers that tick while paused before timers that stop while paused. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Unscaled Timer Paused by Event", ScriptName="IsUnscaledTimerPausedDelegate"), Category="UnscaledTime|Timer")
	static bool IsUnscaledTimerPausedByEvent(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/** Returns true when an unscaled real-time timer exists for the event. Searches timers that tick while paused before timers that stop while paused. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Does Unscaled Timer Exist by Event", ScriptName="UnscaledTimerExistsDelegate"), Category="UnscaledTime|Timer")
	static bool UnscaledTimerExistsByEvent(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/** Returns the last unscaled real-time delta seconds, or 0 if no subsystem exists. */
	UFUNCTION(BlueprintPure, meta=(WorldContext="WorldContextObject"), Category="UnscaledTime|Clock")
	static float GetUnscaledDeltaSeconds(const UObject* WorldContextObject);

	/** Returns accumulated unscaled time seconds for the selected clock, or 0 if no subsystem exists. */
	UFUNCTION(BlueprintPure, meta=(WorldContext="WorldContextObject"), Category="UnscaledTime|Clock")
	static double GetUnscaledTimeSeconds(const UObject* WorldContextObject, EUnscaledTimeClock Clock = EUnscaledTimeClock::RealTime);

	/** Converts frame counts to seconds using the unscaled timer reference frame rate. */
	UFUNCTION(BlueprintPure, meta=(), Category="UnscaledTime|Frames")
	static float UnscaledFramesToSeconds(int32 Frames);

	/** Converts seconds to frame counts using the unscaled timer reference frame rate, rounded to the nearest frame. */
	UFUNCTION(BlueprintPure, meta=(), Category="UnscaledTime|Frames")
	static int32 UnscaledSecondsToFrames(float Seconds);

private:
	/** Shared implementation for Blueprint dynamic-delegate timer creation across seconds and frame APIs. */
	static FUnscaledTimerHandle SetUnscaledTimerDelegateInternal(FTimerDynamicDelegate Delegate, float Time, bool bLooping, bool bTickWhilePaused, bool bMaxOncePerFrame, float InitialStartDelay, float InitialStartDelayVariance);

	/** Validates that a Blueprint timer function exists and has no parameters before binding by name. */
	static bool ValidateTimerFunction(UObject* Object, const FString& FunctionName, const TCHAR* CallerName);
};
