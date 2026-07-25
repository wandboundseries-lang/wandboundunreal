#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WBRuntimeMatchPresentation.h"
#include "WBRuntimePresentationEvent.h"
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
	void BeginPresentationEvent(
		const FWBRuntimePresentationEvent& Event,
		const FVector& SourceWorldLocation,
		const FVector& DestinationWorldLocation,
		float DurationSeconds);
	void CompletePresentationEvent(
		const FWBRuntimePresentationEvent& Event,
		const FVector& FinalWorldLocation);
	void SnapToWorldLocation(const FVector& WorldLocation);

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	int32 GetStableUnitId() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	FWBRuntimeUnitPresentation GetPresentation() const;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere)
	FWBRuntimeUnitPresentation Presentation;

	FVector PresentationStartLocation = FVector::ZeroVector;
	FVector PresentationEndLocation = FVector::ZeroVector;
	FVector PresentationBaseScale = FVector(0.45f, 0.45f, 0.75f);
	float PresentationElapsedSeconds = 0.0f;
	float PresentationDurationSeconds = 0.0f;
	EWBRuntimePresentationEventType ActivePresentationType =
		EWBRuntimePresentationEventType::MatchInitialized;
	bool bPresentationActive = false;
};
