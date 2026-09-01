#include "WBBoardGeometry.h"

namespace
{
constexpr int32 BoardSize = 9;

bool IsValidOrthogonalEdge(const FWBTile& A, const FWBTile& B)
{
	return WBBoardGeometry::IsTileInBounds(A)
		&& WBBoardGeometry::IsTileInBounds(B)
		&& WBBoardGeometry::AreOrthogonallyAdjacent(A, B);
}
}

bool WBBoardGeometry::IsTileInBounds(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.X < BoardSize
		&& Tile.Y >= 0 && Tile.Y < BoardSize;
}

bool WBBoardGeometry::AreOrthogonallyAdjacent(
	const FWBTile& A,
	const FWBTile& B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) == 1;
}

bool WBBoardGeometry::AreDiagonallyAdjacent(
	const FWBTile& A,
	const FWBTile& B)
{
	return FMath::Abs(A.X - B.X) == 1
		&& FMath::Abs(A.Y - B.Y) == 1;
}

bool WBBoardGeometry::AreAdjacent(
	const FWBGridGeometryProfile& Profile,
	const FWBTile& A,
	const FWBTile& B)
{
	return (Profile.bOrthogonal && AreOrthogonallyAdjacent(A, B))
		|| (Profile.bDiagonal && AreDiagonallyAdjacent(A, B));
}

bool WBBoardGeometry::AreOrthogonallyAligned(
	const FWBTile& A,
	const FWBTile& B)
{
	return A != B && (A.X == B.X || A.Y == B.Y);
}

bool WBBoardGeometry::AreDiagonallyAligned(
	const FWBTile& A,
	const FWBTile& B)
{
	const int32 DeltaX = FMath::Abs(A.X - B.X);
	const int32 DeltaY = FMath::Abs(A.Y - B.Y);
	return DeltaX > 0 && DeltaX == DeltaY;
}

int32 WBBoardGeometry::OrthogonalDistance(
	const FWBTile& A,
	const FWBTile& B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

int32 WBBoardGeometry::DiagonalDistance(
	const FWBTile& A,
	const FWBTile& B)
{
	return AreDiagonallyAligned(A, B)
		? FMath::Abs(A.X - B.X)
		: MAX_int32;
}

EWBGridGeometry WBBoardGeometry::ClassifyLine(
	const FWBGridGeometryProfile& Profile,
	const FWBTile& From,
	const FWBTile& To)
{
	if (Profile.bOrthogonal && AreOrthogonallyAligned(From, To))
	{
		return EWBGridGeometry::Orthogonal;
	}
	if (Profile.bDiagonal && AreDiagonallyAligned(From, To))
	{
		return EWBGridGeometry::Diagonal;
	}
	return EWBGridGeometry::None;
}

int32 WBBoardGeometry::LineDistance(
	const EWBGridGeometry Geometry,
	const FWBTile& From,
	const FWBTile& To)
{
	if (Geometry == EWBGridGeometry::Orthogonal)
	{
		return OrthogonalDistance(From, To);
	}
	if (Geometry == EWBGridGeometry::Diagonal)
	{
		return DiagonalDistance(From, To);
	}
	return MAX_int32;
}

bool WBBoardGeometry::HasOrthogonalWallBetween(
	const FWBGameStateData& State,
	const FWBTile& A,
	const FWBTile& B)
{
	if (!IsValidOrthogonalEdge(A, B))
	{
		return false;
	}
	const FWBWallEdge Candidate(A, B);
	for (const FWBWallEdge& Wall : State.Walls)
	{
		if (Wall.GetNormalized().IsSameUndirectedEdge(
			Candidate.GetNormalized()))
		{
			return true;
		}
	}
	return false;
}

bool WBBoardGeometry::IsDiagonalStepBlockedByWalls(
	const FWBGameStateData& State,
	const FWBTile& From,
	const FWBTile& To)
{
	if (!AreDiagonallyAdjacent(From, To))
	{
		return false;
	}
	const FWBTile HorizontalSide(To.X, From.Y);
	const FWBTile VerticalSide(From.X, To.Y);
	const bool bHorizontalFirstRouteBlocked =
		HasOrthogonalWallBetween(State, From, HorizontalSide)
		|| HasOrthogonalWallBetween(State, HorizontalSide, To);
	const bool bVerticalFirstRouteBlocked =
		HasOrthogonalWallBetween(State, From, VerticalSide)
		|| HasOrthogonalWallBetween(State, VerticalSide, To);
	return bHorizontalFirstRouteBlocked && bVerticalFirstRouteBlocked;
}

bool WBBoardGeometry::IsStepBlockedByWalls(
	const FWBGameStateData& State,
	const FWBTile& From,
	const FWBTile& To)
{
	if (AreOrthogonallyAdjacent(From, To))
	{
		return HasOrthogonalWallBetween(State, From, To);
	}
	return IsDiagonalStepBlockedByWalls(State, From, To);
}

FWBGeometryLineQueryResult WBBoardGeometry::QueryLine(
	const FWBGameStateData& State,
	const FWBGridGeometryProfile& Profile,
	const FWBTile& From,
	const FWBTile& To,
	const bool bIgnoreWalls,
	const bool bBlockedByUnits)
{
	FWBGeometryLineQueryResult Result;
	if (!IsTileInBounds(From) || !IsTileInBounds(To))
	{
		Result.Reason = TEXT("out_of_bounds");
		return Result;
	}
	Result.Geometry = ClassifyLine(Profile, From, To);
	if (Result.Geometry == EWBGridGeometry::None)
	{
		Result.Reason = TEXT("not_in_line");
		return Result;
	}
	Result.Distance = LineDistance(Result.Geometry, From, To);
	const int32 StepX = From.X == To.X ? 0 : (To.X > From.X ? 1 : -1);
	const int32 StepY = From.Y == To.Y ? 0 : (To.Y > From.Y ? 1 : -1);
	FWBTile Current = From;
	while (Current != To)
	{
		const FWBTile Next(Current.X + StepX, Current.Y + StepY);
		if (!bIgnoreWalls && IsStepBlockedByWalls(State, Current, Next))
		{
			Result.Reason = TEXT("blocked_by_wall");
			return Result;
		}
		if (bBlockedByUnits && Next != To && State.UnitIdAt(Next) != INDEX_NONE)
		{
			Result.Reason = TEXT("blocked_by_unit");
			return Result;
		}
		Current = Next;
	}
	Result.bOk = true;
	return Result;
}

TArray<FWBTile> WBBoardGeometry::GetStepDirections(
	const FWBGridGeometryProfile& Profile)
{
	TArray<FWBTile> Result;
	if (Profile.bOrthogonal)
	{
		Result.Append({
			FWBTile(1, 0), FWBTile(-1, 0),
			FWBTile(0, 1), FWBTile(0, -1)
		});
	}
	if (Profile.bDiagonal)
	{
		Result.Append({
			FWBTile(1, 1), FWBTile(-1, 1),
			FWBTile(1, -1), FWBTile(-1, -1)
		});
	}
	return Result;
}
