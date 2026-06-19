#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WBBoardViewTypes.h"
#include "WBPublicBoardSummary.h"
#include "WBBoardViewActor.generated.h"

class USceneComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;

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

	void RenderPublicBoardSummary(const FWBPublicBoardSummary& Summary);
	void ClearBoardView();

	int32 GetRenderedTileCount() const;
	int32 GetRenderedUnitCount() const;
	int32 GetRenderedWallCount() const;
	int32 GetRenderedTerrainCount() const;

private:
	int32 LastRenderedTileCount = 0;
	int32 LastRenderedUnitCount = 0;
	int32 LastRenderedWallCount = 0;
	int32 LastRenderedTerrainCount = 0;
};
