#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WBBoardViewTypes.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeMatchPresentation.h"
#include "WBBoardViewActor.generated.h"

class USceneComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class AWBRuntimeUnitPresentationActor;

UCLASS()
class WANDBOUNDRUNTIME_API AWBBoardViewActor : public AActor
{
	GENERATED_BODY()

public:
	AWBBoardViewActor();

	UPROPERTY(VisibleAnywhere, Category = "Board View")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Board View")
	TObjectPtr<UInstancedStaticMeshComponent> TileInstances;

	UPROPERTY(VisibleAnywhere, Category = "Board View")
	TObjectPtr<UInstancedStaticMeshComponent> UnitInstances;

	UPROPERTY(VisibleAnywhere, Category = "Board View")
	TObjectPtr<UInstancedStaticMeshComponent> WallInstances;

	UPROPERTY(VisibleAnywhere, Category = "Board View")
	TObjectPtr<UInstancedStaticMeshComponent> TerrainInstances;

	UPROPERTY(VisibleAnywhere, Category = "Board View")
	TObjectPtr<UInstancedStaticMeshComponent> MarkerInstances;

	UPROPERTY(EditAnywhere, Category = "Board View")
	FWBBoardViewSettings ViewSettings;

	UPROPERTY(EditAnywhere, Category = "Board View")
	TObjectPtr<UStaticMesh> TileMesh;

	UPROPERTY(EditAnywhere, Category = "Board View")
	TObjectPtr<UStaticMesh> UnitMesh;

	UPROPERTY(EditAnywhere, Category = "Board View")
	TObjectPtr<UStaticMesh> WallMesh;

	UPROPERTY(EditAnywhere, Category = "Board View")
	TObjectPtr<UStaticMesh> TerrainMesh;

	UPROPERTY(EditAnywhere, Category = "Board View")
	TObjectPtr<UStaticMesh> MarkerMesh;

	UPROPERTY(EditAnywhere, Category = "Board View")
	TSubclassOf<AWBRuntimeUnitPresentationActor> UnitPresentationActorClass;

	void RenderPublicBoardSummary(const FWBPublicBoardSummary& Summary);
	void ApplyRuntimePresentation(
		const FWBPublicBoardSummary& Summary,
		const TArray<FWBRuntimeBoardTilePresentation>& Tiles,
		const TArray<FWBRuntimeUnitPresentation>& Units);
	void ClearBoardView();

	UFUNCTION(BlueprintPure, Category = "Wandbound|Board")
	FVector TileToWorld(FIntPoint Tile) const;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Board")
	bool WorldToTile(FVector WorldLocation, FIntPoint& OutTile) const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Board")
	bool IsTileInBounds(FIntPoint Tile) const;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Board")
	void ClearHighlights();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Board")
	void SetTileSelected(FIntPoint Tile, bool bSelected);

	UFUNCTION(BlueprintPure, Category = "Wandbound|Board")
	TArray<FWBRuntimeBoardTilePresentation> GetTilePresentations() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Board")
	TArray<FWBRuntimeUnitPresentation> GetUnitPresentations() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Board")
	int32 GetUnitPresentationActorCount() const;

	AWBRuntimeUnitPresentationActor* FindUnitPresentationActor(int32 UnitId) const;

	int32 GetRenderedTileCount() const;
	int32 GetRenderedUnitCount() const;
	int32 GetRenderedWallCount() const;
	int32 GetRenderedTerrainCount() const;

private:
	int32 LastRenderedTileCount = 0;
	int32 LastRenderedUnitCount = 0;
	int32 LastRenderedWallCount = 0;
	int32 LastRenderedTerrainCount = 0;

	UPROPERTY(Transient)
	TArray<FWBRuntimeBoardTilePresentation> TilePresentations;

	UPROPERTY(Transient)
	TArray<FWBRuntimeUnitPresentation> UnitPresentations;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AWBRuntimeUnitPresentationActor>> UnitPresentationActors;

	void SynchronizeUnitActors(const TArray<FWBRuntimeUnitPresentation>& Units);
	void RenderMarkers();
};
