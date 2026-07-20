#include "WBBoardViewTypes.h"

namespace
{
constexpr float DefaultCubeSize = 100.0f;

float BoardCenterOffset(const int32 Size)
{
	return (FMath::Max(Size, 1) - 1) * 0.5f;
}

FVector MeshScale(const float X, const float Y, const float Z)
{
	return FVector(X / DefaultCubeSize, Y / DefaultCubeSize, Z / DefaultCubeSize);
}

FWBTile TileFromPublicPoint(const int32 X, const int32 Y)
{
	return FWBTile(X, Y);
}
}

FVector WBBoardViewTypes::TileCenterToWorld(const FWBTile& Tile, const FWBBoardViewSettings& Settings)
{
	const float WorldX = (static_cast<float>(Tile.X) - BoardCenterOffset(Settings.BoardWidth)) * Settings.TileSize;
	const float WorldY = (static_cast<float>(Tile.Y) - BoardCenterOffset(Settings.BoardHeight)) * Settings.TileSize;
	return FVector(WorldX, WorldY, 0.0f);
}

bool WBBoardViewTypes::LocalPositionToTile(
	const FVector& LocalPosition,
	const FWBBoardViewSettings& Settings,
	FWBTile& OutTile)
{
	OutTile = FWBTile();
	if (Settings.BoardWidth <= 0 || Settings.BoardHeight <= 0 || Settings.TileSize <= 0.0f)
	{
		return false;
	}

	const float TileX = LocalPosition.X / Settings.TileSize + BoardCenterOffset(Settings.BoardWidth);
	const float TileY = LocalPosition.Y / Settings.TileSize + BoardCenterOffset(Settings.BoardHeight);
	const int32 X = FMath::RoundToInt(TileX);
	const int32 Y = FMath::RoundToInt(TileY);
	const FWBTile Candidate(X, Y);
	if (!IsTileInBounds(Candidate, Settings)
		|| FMath::Abs(TileX - static_cast<float>(X)) > 0.5f
		|| FMath::Abs(TileY - static_cast<float>(Y)) > 0.5f)
	{
		return false;
	}

	OutTile = Candidate;
	return true;
}

bool WBBoardViewTypes::IsTileInBounds(const FWBTile& Tile, const FWBBoardViewSettings& Settings)
{
	return Tile.X >= 0 && Tile.Y >= 0
		&& Tile.X < Settings.BoardWidth && Tile.Y < Settings.BoardHeight;
}

FTransform WBBoardViewTypes::TileTransform(const FWBTile& Tile, const FWBBoardViewSettings& Settings)
{
	return FTransform(
		FQuat::Identity,
		TileCenterToWorld(Tile, Settings),
		MeshScale(Settings.TileSize, Settings.TileSize, Settings.TileThickness));
}

FTransform WBBoardViewTypes::UnitTransform(const FWBPublicUnitBoardSummary& Unit, const FWBBoardViewSettings& Settings)
{
	FVector Location = TileCenterToWorld(TileFromPublicPoint(Unit.X, Unit.Y), Settings);
	Location.Z = Settings.UnitHeight;
	return FTransform(
		FQuat::Identity,
		Location,
		MeshScale(Settings.TileSize * 0.6f, Settings.TileSize * 0.6f, Settings.UnitHeight));
}

FTransform WBBoardViewTypes::WallTransform(const FWBPublicWallEdgeSummary& Wall, const FWBBoardViewSettings& Settings)
{
	const FVector A = TileCenterToWorld(TileFromPublicPoint(Wall.AX, Wall.AY), Settings);
	const FVector B = TileCenterToWorld(TileFromPublicPoint(Wall.BX, Wall.BY), Settings);
	FVector Location = (A + B) * 0.5f;
	Location.Z = Settings.WallHeight * 0.5f;

	const FString Orientation = Wall.Orientation.GetPlainNameString().ToLower();
	const bool bVertical = Orientation == TEXT("vertical");
	const FVector Scale = bVertical
		? MeshScale(Settings.WallThickness, Settings.TileSize, Settings.WallHeight)
		: MeshScale(Settings.TileSize, Settings.WallThickness, Settings.WallHeight);

	return FTransform(FQuat::Identity, Location, Scale);
}

FTransform WBBoardViewTypes::TerrainTransform(const FWBPublicTerrainTileSummary& Terrain, const FWBBoardViewSettings& Settings)
{
	FVector Location = TileCenterToWorld(TileFromPublicPoint(Terrain.X, Terrain.Y), Settings);
	Location.Z = Settings.TileThickness;
	return FTransform(
		FQuat::Identity,
		Location,
		MeshScale(Settings.TileSize, Settings.TileSize, FMath::Max(Settings.TileThickness * 0.5f, 1.0f)));
}
