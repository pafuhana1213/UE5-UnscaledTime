#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnscaledTimeSettings.generated.h"

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Unscaled Time"))
class UNSCALEDTIME_API UUnscaledTimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Conversion base fps for frame-count APIs. Seconds = Frames / ReferenceFrameRate. */
	UPROPERTY(EditAnywhere, Config, Category="Frames", meta=(ClampMin="1.0"))
	float ReferenceFrameRate = 60.0f;

	/** Clamp for real delta seconds to prevent large catch-up jumps after hitches or debugger breaks. 0 disables clamping. */
	UPROPERTY(EditAnywhere, Config, Category="Clock", meta=(ClampMin="0.0"))
	float MaxRealDeltaSeconds = 0.5f;

	virtual FName GetCategoryName() const override;
};
