#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WBRuntimeMatchPresentation.h"
#include "WBRuntimeUnitPresentationActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class WANDBOUNDRUNTIME_API AWBRuntimeUnitPresentationActor : public AActor
{
	GENERATED_BODY()

public:
	AWBRuntimeUnitPresentationActor();

	void ApplyPresentation(const FWBRuntimeUnitPresentation& InPresentation, const FVector& WorldLocation);

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	int32 GetStableUnitId() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	FWBRuntimeUnitPresentation GetPresentation() const;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere)
	FWBRuntimeUnitPresentation Presentation;
};
