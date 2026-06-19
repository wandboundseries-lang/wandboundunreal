#include "WBBoardViewActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
void ConfigureInstanceComponent(UInstancedStaticMeshComponent* Component)
{
	if (Component == nullptr)
	{
		return;
	}

	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
}

void ClearInstanceComponent(UInstancedStaticMeshComponent* Component)
{
	if (Component != nullptr)
	{
		Component->ClearInstances();
	}
}

void AddInstanceIfMeshAvailable(
	UInstancedStaticMeshComponent* Component,
	UStaticMesh* Mesh,
	const FTransform& Transform)
{
	if (Component == nullptr || Mesh == nullptr)
	{
		return;
	}

	if (Component->GetStaticMesh() != Mesh)
	{
		Component->SetStaticMesh(Mesh);
	}

	Component->AddInstance(Transform);
}

FWBBoardViewSettings MakeRenderSettings(
	const FWBBoardViewSettings& ViewSettings,
	const FWBPublicBoardSummary& Summary)
{
	FWBBoardViewSettings RenderSettings = ViewSettings;
	RenderSettings.BoardWidth = Summary.BoardWidth > 0 ? Summary.BoardWidth : ViewSettings.BoardWidth;
	RenderSettings.BoardHeight = Summary.BoardHeight > 0 ? Summary.BoardHeight : ViewSettings.BoardHeight;
	return RenderSettings;
}
}

AWBBoardViewActor::AWBBoardViewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TileInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TileInstances"));
	TileInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(TileInstances);

	UnitInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("UnitInstances"));
	UnitInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(UnitInstances);

	WallInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallInstances"));
	WallInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(WallInstances);

	TerrainInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TerrainInstances"));
	TerrainInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(TerrainInstances);
}

void AWBBoardViewActor::ClearBoardView()
{
	ClearInstanceComponent(TileInstances);
	ClearInstanceComponent(UnitInstances);
	ClearInstanceComponent(WallInstances);
	ClearInstanceComponent(TerrainInstances);

	LastRenderedTileCount = 0;
	LastRenderedUnitCount = 0;
	LastRenderedWallCount = 0;
	LastRenderedTerrainCount = 0;
}

void AWBBoardViewActor::RenderPublicBoardSummary(const FWBPublicBoardSummary& Summary)
{
	ClearBoardView();

	const FWBBoardViewSettings RenderSettings = MakeRenderSettings(ViewSettings, Summary);

	LastRenderedTileCount = RenderSettings.BoardWidth * RenderSettings.BoardHeight;
	for (int32 Y = 0; Y < RenderSettings.BoardHeight; ++Y)
	{
		for (int32 X = 0; X < RenderSettings.BoardWidth; ++X)
		{
			AddInstanceIfMeshAvailable(
				TileInstances,
				TileMesh,
				WBBoardViewTypes::TileTransform(FWBTile(X, Y), RenderSettings));
		}
	}

	LastRenderedUnitCount = Summary.Units.Num();
	for (const FWBPublicUnitBoardSummary& Unit : Summary.Units)
	{
		AddInstanceIfMeshAvailable(UnitInstances, UnitMesh, WBBoardViewTypes::UnitTransform(Unit, RenderSettings));
	}

	LastRenderedWallCount = Summary.Walls.Num();
	for (const FWBPublicWallEdgeSummary& Wall : Summary.Walls)
	{
		AddInstanceIfMeshAvailable(WallInstances, WallMesh, WBBoardViewTypes::WallTransform(Wall, RenderSettings));
	}

	LastRenderedTerrainCount = Summary.TerrainTiles.Num();
	for (const FWBPublicTerrainTileSummary& Terrain : Summary.TerrainTiles)
	{
		AddInstanceIfMeshAvailable(TerrainInstances, TerrainMesh, WBBoardViewTypes::TerrainTransform(Terrain, RenderSettings));
	}
}

int32 AWBBoardViewActor::GetRenderedTileCount() const
{
	return LastRenderedTileCount;
}

int32 AWBBoardViewActor::GetRenderedUnitCount() const
{
	return LastRenderedUnitCount;
}

int32 AWBBoardViewActor::GetRenderedWallCount() const
{
	return LastRenderedWallCount;
}

int32 AWBBoardViewActor::GetRenderedTerrainCount() const
{
	return LastRenderedTerrainCount;
}
