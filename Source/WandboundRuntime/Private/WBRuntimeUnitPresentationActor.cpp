#include "WBRuntimeUnitPresentationActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AWBRuntimeUnitPresentationActor::AWBRuntimeUnitPresentationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(SceneRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VisualMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	VisualMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	VisualMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CylinderMesh.Object);
	}
	VisualMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.75f));
}

void AWBRuntimeUnitPresentationActor::ApplyPresentation(
	const FWBRuntimeUnitPresentation& InPresentation,
	const FVector& WorldLocation)
{
	Presentation = InPresentation;
	SetActorLocation(WorldLocation);
	VisualMesh->SetRelativeScale3D(PresentationBaseScale);
}

void AWBRuntimeUnitPresentationActor::BeginPresentationEvent(
	const FWBRuntimePresentationEvent& Event,
	const FVector& SourceWorldLocation,
	const FVector& DestinationWorldLocation,
	const float DurationSeconds)
{
	ActivePresentationType = Event.Type;
	PresentationStartLocation = SourceWorldLocation;
	PresentationEndLocation = DestinationWorldLocation;
	PresentationElapsedSeconds = 0.0f;
	PresentationDurationSeconds = FMath::Max(DurationSeconds, KINDA_SMALL_NUMBER);
	bPresentationActive = true;
	SetActorLocation(SourceWorldLocation);

	if (Event.Type == EWBRuntimePresentationEventType::UnitSummoned
		|| Event.Type == EWBRuntimePresentationEventType::NPCSpawned)
	{
		VisualMesh->SetRelativeScale3D(FVector::ZeroVector);
	}
	else
	{
		VisualMesh->SetRelativeScale3D(PresentationBaseScale);
	}
	SetActorTickEnabled(true);
}

void AWBRuntimeUnitPresentationActor::CompletePresentationEvent(
	const FWBRuntimePresentationEvent& Event,
	const FVector& FinalWorldLocation)
{
	bPresentationActive = false;
	SetActorTickEnabled(false);
	SetActorLocation(FinalWorldLocation);
	VisualMesh->SetRelativeScale3D(
		Event.Type == EWBRuntimePresentationEventType::UnitDefeated
			|| Event.Type == EWBRuntimePresentationEventType::HeroDefeated
			? FVector::ZeroVector
			: PresentationBaseScale);
}

void AWBRuntimeUnitPresentationActor::SnapToWorldLocation(const FVector& WorldLocation)
{
	bPresentationActive = false;
	SetActorTickEnabled(false);
	SetActorLocation(WorldLocation);
	VisualMesh->SetRelativeScale3D(PresentationBaseScale);
}

int32 AWBRuntimeUnitPresentationActor::GetStableUnitId() const
{
	return Presentation.UnitId;
}

FWBRuntimeUnitPresentation AWBRuntimeUnitPresentationActor::GetPresentation() const
{
	return Presentation;
}

void AWBRuntimeUnitPresentationActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPresentationActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	PresentationElapsedSeconds += DeltaSeconds;
	const float Alpha = FMath::Clamp(
		PresentationElapsedSeconds / PresentationDurationSeconds,
		0.0f,
		1.0f);
	if (ActivePresentationType == EWBRuntimePresentationEventType::UnitMoved
		|| ActivePresentationType == EWBRuntimePresentationEventType::NPCMoved)
	{
		SetActorLocation(FMath::Lerp(PresentationStartLocation, PresentationEndLocation, Alpha));
	}
	else if (ActivePresentationType == EWBRuntimePresentationEventType::UnitSummoned
		|| ActivePresentationType == EWBRuntimePresentationEventType::NPCSpawned)
	{
		SetActorLocation(PresentationEndLocation);
		VisualMesh->SetRelativeScale3D(PresentationBaseScale * Alpha);
	}
	else if (ActivePresentationType == EWBRuntimePresentationEventType::UnitDefeated
		|| ActivePresentationType == EWBRuntimePresentationEventType::HeroDefeated)
	{
		VisualMesh->SetRelativeScale3D(PresentationBaseScale * (1.0f - Alpha));
	}
	else
	{
		const float Pulse = 1.0f + FMath::Sin(Alpha * PI) * 0.18f;
		VisualMesh->SetRelativeScale3D(PresentationBaseScale * Pulse);
	}

	if (Alpha >= 1.0f)
	{
		bPresentationActive = false;
		SetActorTickEnabled(false);
	}
}
