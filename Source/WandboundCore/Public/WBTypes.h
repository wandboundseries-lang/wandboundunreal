#pragma once

#include "CoreMinimal.h"

enum class EWBCombatCapability : uint8
{
	AttacksCannotBeCountered,
	AttacksDiagonally,
	ImmuneToEnemyEffects
};

enum class EWBGridGeometry : uint8
{
	None,
	Orthogonal,
	Diagonal
};

struct WANDBOUNDCORE_API FWBGridGeometryProfile
{
	bool bOrthogonal = true;
	bool bDiagonal = false;

	bool Allows(const EWBGridGeometry Geometry) const
	{
		return (Geometry == EWBGridGeometry::Orthogonal && bOrthogonal)
			|| (Geometry == EWBGridGeometry::Diagonal && bDiagonal);
	}

	bool IsValid() const
	{
		return bOrthogonal || bDiagonal;
	}

	static FWBGridGeometryProfile OrthogonalOnly()
	{
		return FWBGridGeometryProfile();
	}

	static FWBGridGeometryProfile DiagonalOnly()
	{
		FWBGridGeometryProfile Result;
		Result.bOrthogonal = false;
		Result.bDiagonal = true;
		return Result;
	}

	static FWBGridGeometryProfile OrthogonalAndDiagonal()
	{
		FWBGridGeometryProfile Result;
		Result.bDiagonal = true;
		return Result;
	}
};

enum class EWBDeclarationProvenance : uint8
{
	Automatic,
	PlayerDeclared
};

enum class EWBActivationProvenance : uint8
{
	ResolutionOnly,
	AutomaticActivation,
	PlayerDeclared
};

inline bool WBIsPlayerDeclared(const EWBDeclarationProvenance Provenance)
{
	return Provenance == EWBDeclarationProvenance::PlayerDeclared;
}

inline bool WBIsActivation(const EWBActivationProvenance Provenance)
{
	return Provenance != EWBActivationProvenance::ResolutionOnly;
}

inline bool WBIsPlayerDeclaredActivation(
	const EWBActivationProvenance Provenance)
{
	return Provenance == EWBActivationProvenance::PlayerDeclared;
}

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
