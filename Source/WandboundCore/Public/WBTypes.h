#pragma once

#include "CoreMinimal.h"

struct WANDBOUNDCORE_API FWBTile
{
	int32 X = -1;
	int32 Y = -1;

	FWBTile() = default;

	FWBTile(const int32 InX, const int32 InY)
		: X(InX)
		, Y(InY)
	{
	}

	bool operator==(const FWBTile& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	bool operator!=(const FWBTile& Other) const
	{
		return !(*this == Other);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(%d,%d)"), X, Y);
	}
};

struct WANDBOUNDCORE_API FWBWallEdge
{
	FWBTile A;
	FWBTile B;

	FWBWallEdge() = default;

	FWBWallEdge(const FWBTile& InA, const FWBTile& InB)
		: A(InA)
		, B(InB)
	{
	}

	FWBWallEdge GetNormalized() const
	{
		if (B.X < A.X || (B.X == A.X && B.Y < A.Y))
		{
			return FWBWallEdge(B, A);
		}

		return *this;
	}

	bool IsSameUndirectedEdge(const FWBWallEdge& Other) const
	{
		const FWBWallEdge NormalizedThis = GetNormalized();
		const FWBWallEdge NormalizedOther = Other.GetNormalized();
		return NormalizedThis.A == NormalizedOther.A && NormalizedThis.B == NormalizedOther.B;
	}

	bool operator==(const FWBWallEdge& Other) const
	{
		return IsSameUndirectedEdge(Other);
	}

	bool operator!=(const FWBWallEdge& Other) const
	{
		return !(*this == Other);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s-%s"), *A.ToString(), *B.ToString());
	}
};
