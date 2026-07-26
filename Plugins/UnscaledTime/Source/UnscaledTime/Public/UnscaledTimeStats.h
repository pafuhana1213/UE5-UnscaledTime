#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("UnscaledTime"), STATGROUP_UnscaledTime, STATCAT_Advanced);
// Measures subsystem tick cost and the current number of pending latent delays.
DECLARE_CYCLE_STAT_EXTERN(TEXT("Subsystem Tick"), STAT_UnscaledTimeSubsystemTick, STATGROUP_UnscaledTime, UNSCALEDTIME_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Pending Delays"), STAT_UnscaledTimePendingDelays, STATGROUP_UnscaledTime, UNSCALEDTIME_API);
