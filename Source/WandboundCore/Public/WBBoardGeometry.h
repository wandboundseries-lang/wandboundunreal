#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBTypes.h"

struct WANDBOUNDCORE_API FWBGeometryLineQueryResult
{
	bool bOk = false;
	FString Reason;
	EWBGridGeometry Geometry = EWBGridGeometry::None;
	int32 Distance = 0;
};

class WANDBOUNDCORE_API WBBoardGeometry
{
public:
	static bool IsTileInBounds(const FWBTile& Tile);
	static bool AreOrthogonallyAdjacent(const FWBTile& A, const FWBTile& B);
	static bool AreDiagonallyAdjacent(const FWBTile& A, const FWBTile& B);
	static bool AreAdjacent(
		const FWBGridGeometryProfile& Profile,
		const FWBTile& A,
		const FWBTile& B);
	static bool AreOrthogonallyAligned(const FWBTile& A, const FWBTile& B);
	static bool AreDiagonallyAligned(const FWBTile& A, const FWBTile& B);
	static int32 OrthogonalDistance(const FWBTile& A, const FWBTile& B);
	static int32 DiagonalDistance(const FWBTile& A, const FWBTile& B);
	static EWBGridGeometry ClassifyLine(
		const FWBGridGeometryProfile& Profile,
		const FWBTile& From,
		const FWBTile& To);
	static int32 LineDistance(
		EWBGridGeometry Geometry,
		const FWBTile& From,
		const FWBTile& To);
	static bool HasOrthogonalWallBetween(
		const FWBGameStateData& State,
		const FWBTile& A,
		const FWBTile& B);
	static bool IsDiagonalStepBlockedByWalls(
		const FWBGameStateData& State,
		const FWBTile& From,
		const FWBTile& To);
	static bool IsStepBlockedByWalls(
		const FWBGameStateData& State,
		const FWBTile& From,
		const FWBTile& To);
	static FWBGeometryLineQueryResult QueryLine(
		const FWBGameStateData& State,
		const FWBGridGeometryProfile& Profile,
		const FWBTile& From,
		const FWBTile& To,
		bool bIgnoreWalls,
		bool bBlockedByUnits);
	static TArray<FWBTile> GetStepDirections(
		const FWBGridGeometryProfile& Profile);
};
