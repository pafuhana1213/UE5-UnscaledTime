#include "DemoComparisonActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "UnscaledTickComponent.h"
#include "UnscaledTimeSubsystem.h"

ADemoComparisonActor::ADemoComparisonActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VanillaCounterText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VanillaCounterText"));
	VanillaCounterText->SetupAttachment(SceneRoot);
	// デモ全体の読み比べは Y=-160 を vanilla、Y=+160 を unscaled とする配置規約に依存している。
	// 対になるテキストとキューブは同じ Y 側へ置き、個別の数値には意味を持たせない。
	VanillaCounterText->SetRelativeLocation(FVector(0.0f, -160.0f, 160.0f));
	VanillaCounterText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	VanillaCounterText->SetWorldSize(48.0f);
	VanillaCounterText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	VanillaCounterText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);

	UnscaledCounterText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("UnscaledCounterText"));
	UnscaledCounterText->SetupAttachment(SceneRoot);
	UnscaledCounterText->SetRelativeLocation(FVector(0.0f, 160.0f, 160.0f));
	UnscaledCounterText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	UnscaledCounterText->SetWorldSize(48.0f);
	UnscaledCounterText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	UnscaledCounterText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);

	PhaseText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PhaseText"));
	PhaseText->SetupAttachment(SceneRoot);
	PhaseText->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	PhaseText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	PhaseText->SetWorldSize(48.0f);
	PhaseText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	PhaseText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);

	VanillaCube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VanillaCube"));
	VanillaCube->SetupAttachment(SceneRoot);
	VanillaCube->SetRelativeLocation(FVector(0.0f, -160.0f, 30.0f));
	VanillaCube->SetRelativeScale3D(FVector(0.5f));

	UnscaledCube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnscaledCube"));
	UnscaledCube->SetupAttachment(SceneRoot);
	UnscaledCube->SetRelativeLocation(FVector(0.0f, 160.0f, 30.0f));
	UnscaledCube->SetRelativeScale3D(FVector(0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		VanillaCube->SetStaticMesh(CubeMesh.Object);
		UnscaledCube->SetStaticMesh(CubeMesh.Object);
	}

	UnscaledTicker = CreateDefaultSubobject<UUnscaledTickComponent>(TEXT("UnscaledTicker"));
	UnscaledTicker->bTickWhilePaused = true;
}

void ADemoComparisonActor::BeginPlay()
{
	Super::BeginPlay();

	VanillaCount = 0;
	UnscaledCount = 0;
	UpdateCounterText();
	PhaseText->SetText(FText::FromString(TEXT("Phase 1: Normal")));

	GetWorldTimerManager().SetTimer(VanillaTimerHandle, this, &ThisClass::HandleVanillaTimer, 1.0f, true);
	UnscaledTicker->OnUnscaledTick.AddDynamic(this, &ThisClass::HandleUnscaledTick);

	UUnscaledTimeSubsystem* UnscaledTimeSubsystem = UUnscaledTimeSubsystem::Get(this);
	if (!UnscaledTimeSubsystem)
	{
		return;
	}

	FTimerManager& RealTimeTimerManager = UnscaledTimeSubsystem->GetTimerManager(EUnscaledTimeClock::RealTime);
	RealTimeTimerManager.SetTimer(UnscaledTimerHandle, FTimerDelegate::CreateUObject(this, &ThisClass::HandleUnscaledTimer), 1.0f, true);

	if (bAutoRunDemoCycle)
	{
		// The demo phase changes use the RealTime clock so they continue through global dilation and pause.
		RealTimeTimerManager.SetTimer(DilationPhaseTimerHandle, FTimerDelegate::CreateUObject(this, &ThisClass::StartDilationPhase), 5.0f, false);
		RealTimeTimerManager.SetTimer(PausedPhaseTimerHandle, FTimerDelegate::CreateUObject(this, &ThisClass::StartPausedPhase), 10.0f, false);
		RealTimeTimerManager.SetTimer(ResumePhaseTimerHandle, FTimerDelegate::CreateUObject(this, &ThisClass::FinishDemoCycle), 15.0f, false);
	}
	else
	{
		PhaseText->SetText(FText::FromString(TEXT("Use HUD: dilation slider / pause")));
	}
}

void ADemoComparisonActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (VanillaCube)
	{
		// A/B 比較の vanilla 側は world Tick の DeltaSeconds を使うため、
		// global time dilation と pause の影響をそのまま受ける。
		VanillaCube->AddLocalRotation(FRotator(0.0f, 90.0f * DeltaSeconds, 0.0f));
	}
}

void ADemoComparisonActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(VanillaTimerHandle);

	if (UUnscaledTimeSubsystem* UnscaledTimeSubsystem = UUnscaledTimeSubsystem::Get(this))
	{
		FTimerManager& RealTimeTimerManager = UnscaledTimeSubsystem->GetTimerManager(EUnscaledTimeClock::RealTime);
		RealTimeTimerManager.ClearTimer(UnscaledTimerHandle);
		RealTimeTimerManager.ClearTimer(DilationPhaseTimerHandle);
		RealTimeTimerManager.ClearTimer(PausedPhaseTimerHandle);
		RealTimeTimerManager.ClearTimer(ResumePhaseTimerHandle);
	}

	if (UnscaledTicker)
	{
		UnscaledTicker->OnUnscaledTick.RemoveDynamic(this, &ThisClass::HandleUnscaledTick);
	}

	Super::EndPlay(EndPlayReason);
}

void ADemoComparisonActor::HandleVanillaTimer()
{
	++VanillaCount;
	UpdateCounterText();
}

void ADemoComparisonActor::HandleUnscaledTimer()
{
	++UnscaledCount;
	UpdateCounterText();
}

void ADemoComparisonActor::StartDilationPhase()
{
	UGameplayStatics::SetGlobalTimeDilation(this, 0.1f);
	PhaseText->SetText(FText::FromString(TEXT("Phase 2: Dilation 0.1")));
}

void ADemoComparisonActor::StartPausedPhase()
{
	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	UGameplayStatics::SetGamePaused(this, true);
	PhaseText->SetText(FText::FromString(TEXT("Phase 3: Paused")));
}

void ADemoComparisonActor::FinishDemoCycle()
{
	UGameplayStatics::SetGamePaused(this, false);
	PhaseText->SetText(FText::FromString(TEXT("Phase 4: Normal (loop done)")));
}

void ADemoComparisonActor::UpdateCounterText() const
{
	VanillaCounterText->SetText(FText::FromString(FString::Printf(TEXT("Vanilla: %d"), VanillaCount)));
	UnscaledCounterText->SetText(FText::FromString(FString::Printf(TEXT("Unscaled: %d"), UnscaledCount)));
}

void ADemoComparisonActor::HandleUnscaledTick(float RealDeltaSeconds)
{
	if (UnscaledCube)
	{
		// A/B 比較の unscaled 側は subsystem から届く実 delta で回し、
		// Tick() 側との差だけで dilation/pause 耐性を見せる。
		UnscaledCube->AddLocalRotation(FRotator(0.0f, 90.0f * RealDeltaSeconds, 0.0f));
	}
}
