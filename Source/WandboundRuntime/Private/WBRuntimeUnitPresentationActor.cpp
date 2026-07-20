#include "WBRuntimeUnitPresentationActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AWBRuntimeUnitPresentationActor::AWBRuntimeUnitPresentationActor()
{
	PrimaryActorTick.bCanEverTick = false;
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
}

int32 AWBRuntimeUnitPresentationActor::GetStableUnitId() const
{
	return Presentation.UnitId;
}

FWBRuntimeUnitPresentation AWBRuntimeUnitPresentationActor::GetPresentation() const
{
	return Presentation;
}
