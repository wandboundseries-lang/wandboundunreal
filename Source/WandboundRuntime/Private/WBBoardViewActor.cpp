#include "WBBoardViewActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "WBRuntimeUnitPresentationActor.h"

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
	TileInstances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TileInstances->SetCollisionResponseToAllChannels(ECR_Ignore);
	TileInstances->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	UnitInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("UnitInstances"));
	UnitInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(UnitInstances);

	WallInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallInstances"));
	WallInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(WallInstances);

	TerrainInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TerrainInstances"));
	TerrainInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(TerrainInstances);

	MarkerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MarkerInstances"));
	MarkerInstances->SetupAttachment(SceneRoot);
	ConfigureInstanceComponent(MarkerInstances);

	UnitPresentationActorClass = AWBRuntimeUnitPresentationActor::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CubeMeshAsset.Succeeded())
	{
		TileMesh = CubeMeshAsset.Object;
		WallMesh = CubeMeshAsset.Object;
		TerrainMesh = CubeMeshAsset.Object;
	}
	if (CylinderMeshAsset.Succeeded())
	{
		UnitMesh = CylinderMeshAsset.Object;
		MarkerMesh = CylinderMeshAsset.Object;
	}
}

void AWBBoardViewActor::ClearBoardView()
{
	ClearInstanceComponent(TileInstances);
	ClearInstanceComponent(UnitInstances);
	ClearInstanceComponent(WallInstances);
	ClearInstanceComponent(TerrainInstances);
	ClearInstanceComponent(MarkerInstances);

	LastRenderedTileCount = 0;
	LastRenderedUnitCount = 0;
	LastRenderedWallCount = 0;
	LastRenderedTerrainCount = 0;
	AppliedPresentationRevision = 0;
	TilePresentations.Reset();
	UnitPresentations.Reset();
	PendingSummary = FWBPublicBoardSummary();
	PendingTilePresentations.Reset();
	PendingUnitPresentations.Reset();
	bPresentationSequenceActive = false;
	for (TPair<int32, TObjectPtr<AWBRuntimeUnitPresentationActor>>& Pair : UnitPresentationActors)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->Destroy();
		}
	}
	UnitPresentationActors.Reset();
}

void AWBBoardViewActor::ApplyRuntimePresentation(
	const FWBPublicBoardSummary& Summary,
	const TArray<FWBRuntimeBoardTilePresentation>& Tiles,
	const TArray<FWBRuntimeUnitPresentation>& Units)
{
	if (bPresentationSequenceActive)
	{
		PendingSummary = Summary;
		PendingTilePresentations = Tiles;
		PendingUnitPresentations = Units;
		return;
	}

	RenderPublicBoardSummary(Summary);
	// Stable unit Actors own the production unit visuals; the instance path remains for the legacy summary demo.
	ClearInstanceComponent(UnitInstances);
	TilePresentations = Tiles;
	UnitPresentations = Units;
	SynchronizeUnitActors(Units);
	RenderMarkers();
}

void AWBBoardViewActor::BeginPresentationSequence()
{
	bPresentationSequenceActive = true;
	PendingSummary = FWBPublicBoardSummary();
	PendingTilePresentations.Reset();
	PendingUnitPresentations.Reset();
}

void AWBBoardViewActor::CompletePresentationSequence()
{
	SnapToAuthoritativePresentation();
}

void AWBBoardViewActor::CancelPresentationSequence()
{
	SnapToAuthoritativePresentation();
}

bool AWBBoardViewActor::PlayPresentationEvent(
	const FWBRuntimePresentationEvent& Event,
	const float EffectiveDurationSeconds)
{
	int32 UnitId = Event.SourceUnitId;
	if (Event.Type == EWBRuntimePresentationEventType::DamageApplied
		|| Event.Type == EWBRuntimePresentationEventType::HPChanged
		|| Event.Type == EWBRuntimePresentationEventType::ArmorChanged
		|| Event.Type == EWBRuntimePresentationEventType::AttackImpact
		|| Event.Type == EWBRuntimePresentationEventType::TrapTriggered
		|| Event.Type == EWBRuntimePresentationEventType::WandEquipped
		|| Event.Type == EWBRuntimePresentationEventType::UnitDefeated
		|| Event.Type == EWBRuntimePresentationEventType::HeroDefeated)
	{
		UnitId = Event.TargetUnitId;
	}

	if (Event.Type == EWBRuntimePresentationEventType::MarkerConsumed)
	{
		for (FWBRuntimeBoardTilePresentation& Tile : TilePresentations)
		{
			if (Tile.PublicMarkerId == Event.MarkerId)
			{
				Tile.bHasConcealedMarker = false;
				Tile.PublicMarkerId = -1;
			}
		}
		RenderMarkers();
		return true;
	}

	if (Event.Type == EWBRuntimePresentationEventType::TurnStarted
		|| Event.Type == EWBRuntimePresentationEventType::TurnEnded
		|| Event.Type == EWBRuntimePresentationEventType::NPCPhaseStarted
		|| Event.Type == EWBRuntimePresentationEventType::NPCPhaseCompleted
		|| Event.Type == EWBRuntimePresentationEventType::MatchInitialized
		|| Event.Type == EWBRuntimePresentationEventType::GameOver
		|| Event.Type == EWBRuntimePresentationEventType::MarkerRevealed
		|| Event.Type == EWBRuntimePresentationEventType::EquipmentDiscarded)
	{
		return true;
	}

	AWBRuntimeUnitPresentationActor* PresentationActor = FindUnitPresentationActor(UnitId);
	if (PresentationActor == nullptr
		&& (Event.Type == EWBRuntimePresentationEventType::UnitSummoned
			|| Event.Type == EWBRuntimePresentationEventType::NPCSpawned))
	{
		if (const FWBRuntimeUnitPresentation* Unit = FindPendingUnit(UnitId))
		{
			PresentationActor = EnsureUnitPresentationActor(UnitId);
			if (PresentationActor != nullptr)
			{
				PresentationActor->ApplyPresentation(*Unit, UnitWorldLocation(Unit->Tile));
			}
		}
	}
	if (PresentationActor == nullptr)
	{
		return false;
	}

	FVector SourceLocation = PresentationActor->GetActorLocation();
	FVector DestinationLocation = SourceLocation;
	if (Event.SourceTile.X >= 0 && Event.SourceTile.Y >= 0)
	{
		SourceLocation = UnitWorldLocation(Event.SourceTile);
	}
	if (Event.DestinationTile.X >= 0 && Event.DestinationTile.Y >= 0)
	{
		DestinationLocation = UnitWorldLocation(Event.DestinationTile);
	}
	PresentationActor->BeginPresentationEvent(
		Event,
		SourceLocation,
		DestinationLocation,
		EffectiveDurationSeconds);
	return true;
}

void AWBBoardViewActor::CompletePresentationEvent(const FWBRuntimePresentationEvent& Event)
{
	int32 UnitId = Event.SourceUnitId;
	if (Event.Type == EWBRuntimePresentationEventType::DamageApplied
		|| Event.Type == EWBRuntimePresentationEventType::HPChanged
		|| Event.Type == EWBRuntimePresentationEventType::ArmorChanged
		|| Event.Type == EWBRuntimePresentationEventType::AttackImpact
		|| Event.Type == EWBRuntimePresentationEventType::TrapTriggered
		|| Event.Type == EWBRuntimePresentationEventType::WandEquipped
		|| Event.Type == EWBRuntimePresentationEventType::UnitDefeated
		|| Event.Type == EWBRuntimePresentationEventType::HeroDefeated)
	{
		UnitId = Event.TargetUnitId;
	}
	AWBRuntimeUnitPresentationActor* PresentationActor = FindUnitPresentationActor(UnitId);
	if (PresentationActor == nullptr)
	{
		return;
	}

	FVector FinalLocation = PresentationActor->GetActorLocation();
	if (Event.DestinationTile.X >= 0 && Event.DestinationTile.Y >= 0)
	{
		FinalLocation = UnitWorldLocation(Event.DestinationTile);
	}
	else if (const FWBRuntimeUnitPresentation* Unit = FindPendingUnit(UnitId))
	{
		FinalLocation = UnitWorldLocation(Unit->Tile);
	}
	PresentationActor->CompletePresentationEvent(Event, FinalLocation);

	if (Event.Type == EWBRuntimePresentationEventType::UnitDefeated)
	{
		PresentationActor->Destroy();
		UnitPresentationActors.Remove(UnitId);
	}
}

void AWBBoardViewActor::SnapToAuthoritativePresentation()
{
	if (!bPresentationSequenceActive)
	{
		return;
	}
	bPresentationSequenceActive = false;
	if (PendingSummary.BoardWidth > 0 && PendingSummary.BoardHeight > 0)
	{
		ApplyRuntimePresentation(PendingSummary, PendingTilePresentations, PendingUnitPresentations);
	}
	PendingSummary = FWBPublicBoardSummary();
	PendingTilePresentations.Reset();
	PendingUnitPresentations.Reset();
}

FVector AWBBoardViewActor::TileToWorld(const FIntPoint Tile) const
{
	const FVector Local = WBBoardViewTypes::TileCenterToWorld(FWBTile(Tile.X, Tile.Y), ViewSettings);
	return GetActorTransform().TransformPosition(Local);
}

bool AWBBoardViewActor::WorldToTile(const FVector WorldLocation, FIntPoint& OutTile) const
{
	FWBTile Tile;
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (!WBBoardViewTypes::LocalPositionToTile(Local, ViewSettings, Tile))
	{
		OutTile = FIntPoint(-1, -1);
		return false;
	}
	OutTile = FIntPoint(Tile.X, Tile.Y);
	return true;
}

bool AWBBoardViewActor::IsTileInBounds(const FIntPoint Tile) const
{
	return WBBoardViewTypes::IsTileInBounds(FWBTile(Tile.X, Tile.Y), ViewSettings);
}

void AWBBoardViewActor::ClearHighlights()
{
	for (FWBRuntimeBoardTilePresentation& Tile : TilePresentations)
	{
		Tile.Highlight = EWBRuntimeBoardHighlight::None;
		Tile.bSelected = false;
	}
}

void AWBBoardViewActor::SetTileSelected(const FIntPoint Tile, const bool bSelected)
{
	for (FWBRuntimeBoardTilePresentation& Entry : TilePresentations)
	{
		if (Entry.Tile == Tile)
		{
			Entry.bSelected = bSelected;
			return;
		}
	}
}

TArray<FWBRuntimeBoardTilePresentation> AWBBoardViewActor::GetTilePresentations() const
{
	return TilePresentations;
}

TArray<FWBRuntimeUnitPresentation> AWBBoardViewActor::GetUnitPresentations() const
{
	return UnitPresentations;
}

int32 AWBBoardViewActor::GetUnitPresentationActorCount() const
{
	return UnitPresentationActors.Num();
}

int32 AWBBoardViewActor::GetAppliedPresentationRevision() const
{
	return AppliedPresentationRevision;
}

void AWBBoardViewActor::SetAppliedPresentationRevision(const int32 InPresentationRevision)
{
	AppliedPresentationRevision = InPresentationRevision;
}

AWBRuntimeUnitPresentationActor* AWBBoardViewActor::FindUnitPresentationActor(const int32 UnitId) const
{
	const TObjectPtr<AWBRuntimeUnitPresentationActor>* Found = UnitPresentationActors.Find(UnitId);
	return Found != nullptr ? Found->Get() : nullptr;
}

void AWBBoardViewActor::SynchronizeUnitActors(const TArray<FWBRuntimeUnitPresentation>& Units)
{
	UWorld* World = GetWorld();
	if (World == nullptr || !IsValid(this))
	{
		return;
	}

	TSet<int32> DesiredIds;
	for (const FWBRuntimeUnitPresentation& Unit : Units)
	{
		DesiredIds.Add(Unit.UnitId);
		AWBRuntimeUnitPresentationActor* PresentationActor = EnsureUnitPresentationActor(Unit.UnitId);
		if (PresentationActor != nullptr)
		{
			FVector Location = TileToWorld(Unit.Tile);
			Location.Z += ViewSettings.UnitHeight;
			PresentationActor->ApplyPresentation(Unit, Location);
		}
	}

	TArray<int32> ExistingIds;
	UnitPresentationActors.GetKeys(ExistingIds);
	ExistingIds.Sort();
	for (const int32 UnitId : ExistingIds)
	{
		if (!DesiredIds.Contains(UnitId))
		{
			if (AWBRuntimeUnitPresentationActor* Actor = FindUnitPresentationActor(UnitId))
			{
				Actor->Destroy();
			}
			UnitPresentationActors.Remove(UnitId);
		}
	}
}

AWBRuntimeUnitPresentationActor* AWBBoardViewActor::EnsureUnitPresentationActor(const int32 UnitId)
{
	if (AWBRuntimeUnitPresentationActor* Existing = FindUnitPresentationActor(UnitId))
	{
		return Existing;
	}
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	TSubclassOf<AWBRuntimeUnitPresentationActor> ActorClass = UnitPresentationActorClass;
	if (ActorClass == nullptr)
	{
		ActorClass = AWBRuntimeUnitPresentationActor::StaticClass();
	}
	AWBRuntimeUnitPresentationActor* PresentationActor =
		World->SpawnActor<AWBRuntimeUnitPresentationActor>(ActorClass);
	if (PresentationActor != nullptr)
	{
		UnitPresentationActors.Add(UnitId, PresentationActor);
	}
	return PresentationActor;
}

const FWBRuntimeUnitPresentation* AWBBoardViewActor::FindPendingUnit(const int32 UnitId) const
{
	return PendingUnitPresentations.FindByPredicate([UnitId](const FWBRuntimeUnitPresentation& Unit)
	{
		return Unit.UnitId == UnitId;
	});
}

FVector AWBBoardViewActor::UnitWorldLocation(const FIntPoint Tile) const
{
	FVector Location = TileToWorld(Tile);
	Location.Z += ViewSettings.UnitHeight;
	return Location;
}

void AWBBoardViewActor::RenderMarkers()
{
	ClearInstanceComponent(MarkerInstances);
	for (const FWBRuntimeBoardTilePresentation& Tile : TilePresentations)
	{
		if (!Tile.bHasConcealedMarker)
		{
			continue;
		}
		FTransform Transform = WBBoardViewTypes::TileTransform(FWBTile(Tile.Tile.X, Tile.Tile.Y), ViewSettings);
		FVector Location = Transform.GetLocation();
		Location.Z = ViewSettings.TileThickness * 3.0f;
		Transform.SetLocation(Location);
		Transform.SetScale3D(FVector(0.25f, 0.25f, 0.1f));
		AddInstanceIfMeshAvailable(MarkerInstances, MarkerMesh, Transform);
	}
}

void AWBBoardViewActor::RenderPublicBoardSummary(const FWBPublicBoardSummary& Summary)
{
	ClearInstanceComponent(TileInstances);
	ClearInstanceComponent(UnitInstances);
	ClearInstanceComponent(WallInstances);
	ClearInstanceComponent(TerrainInstances);
	ClearInstanceComponent(MarkerInstances);
	LastRenderedTileCount = 0;
	LastRenderedUnitCount = 0;
	LastRenderedWallCount = 0;
	LastRenderedTerrainCount = 0;

	const FWBBoardViewSettings RenderSettings = MakeRenderSettings(ViewSettings, Summary);
	ViewSettings.BoardWidth = RenderSettings.BoardWidth;
	ViewSettings.BoardHeight = RenderSettings.BoardHeight;

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
