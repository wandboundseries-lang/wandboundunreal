#include "WBPublicBoardSummary.h"

namespace
{
constexpr int32 PublicBoardWidth = 9;
constexpr int32 PublicBoardHeight = 9;

FString CanonicalPublicStatusIdString(const FName StatusId)
{
	const FString LowerStatusId = StatusId.GetPlainNameString().ToLower();
	if (LowerStatusId == TEXT("burn") || LowerStatusId.StartsWith(TEXT("burn_")))
	{
		return TEXT("Burn");
	}

	if (LowerStatusId == TEXT("poison") || LowerStatusId.StartsWith(TEXT("poison_")))
	{
		return TEXT("Poison");
	}

	if (LowerStatusId == TEXT("root")
		|| LowerStatusId.StartsWith(TEXT("root_"))
		|| LowerStatusId == TEXT("rooted")
		|| LowerStatusId.StartsWith(TEXT("rooted_")))
	{
		return TEXT("Rooted");
	}

	if (LowerStatusId == TEXT("stun")
		|| LowerStatusId.StartsWith(TEXT("stun_"))
		|| LowerStatusId == TEXT("stunned")
		|| LowerStatusId.StartsWith(TEXT("stunned_")))
	{
		return TEXT("Stunned");
	}

	if (LowerStatusId == TEXT("frozen") || LowerStatusId.StartsWith(TEXT("frozen_")))
	{
		return TEXT("Frozen");
	}

	return FString();
}

TArray<FWBPublicUnitStatusSummary> BuildPublicStatusSummaries(const FWBUnitState& Unit)
{
	TMap<FString, int32> TurnsByStatusId;
	for (const FName& RawStatusId : Unit.Statuses)
	{
		const FString PublicStatusId = CanonicalPublicStatusIdString(RawStatusId);
		if (PublicStatusId.IsEmpty())
		{
			continue;
		}

		const int32 TurnsRemaining = Unit.GetStatusTurnsRemaining(RawStatusId);
		if (int32* ExistingTurns = TurnsByStatusId.Find(PublicStatusId))
		{
			*ExistingTurns = FMath::Max(*ExistingTurns, TurnsRemaining);
		}
		else
		{
			TurnsByStatusId.Add(PublicStatusId, TurnsRemaining);
		}
	}

	TArray<FWBPublicUnitStatusSummary> Statuses;
	Statuses.Reserve(TurnsByStatusId.Num());
	for (const TPair<FString, int32>& Pair : TurnsByStatusId)
	{
		FWBPublicUnitStatusSummary Status;
		Status.StatusId = FName(*Pair.Key);
		Status.TurnsRemaining = Pair.Value;
		Statuses.Add(Status);
	}

	Statuses.Sort([](const FWBPublicUnitStatusSummary& A, const FWBPublicUnitStatusSummary& B)
	{
		const FString AId = A.StatusId.GetPlainNameString();
		const FString BId = B.StatusId.GetPlainNameString();
		if (AId != BId)
		{
			return AId < BId;
		}

		return A.TurnsRemaining < B.TurnsRemaining;
	});

	return Statuses;
}

FWBPublicUnitBoardSummary BuildPublicUnitSummary(const FWBUnitState& Unit)
{
	FWBPublicUnitBoardSummary Summary;
	Summary.UnitId = Unit.UnitId;
	Summary.OwnerId = Unit.OwnerId;
	Summary.CardId = Unit.CardId;
	Summary.X = Unit.X;
	Summary.Y = Unit.Y;
	Summary.HP = Unit.HP;
	Summary.MaxHP = Unit.MaxHP;
	Summary.ATK = Unit.ATK;
	Summary.AR = Unit.AR;
	Summary.RLTotal = Unit.RLTotal;
	Summary.RLUsed = Unit.RLUsed;
	Summary.AttacksLeft = Unit.AttacksLeft;
	Summary.Statuses = BuildPublicStatusSummaries(Unit);
	return Summary;
}
}

FWBPublicBoardSummary WBPublicBoardSummary::Build(const FWBGameStateData& State)
{
	FWBPublicBoardSummary Summary;
	Summary.BoardWidth = PublicBoardWidth;
	Summary.BoardHeight = PublicBoardHeight;
	Summary.Units.Reserve(State.Units.Num());
	for (const FWBUnitState& Unit : State.Units)
	{
		Summary.Units.Add(BuildPublicUnitSummary(Unit));
	}

	Summary.Units.Sort([](const FWBPublicUnitBoardSummary& A, const FWBPublicUnitBoardSummary& B)
	{
		if (A.Y != B.Y)
		{
			return A.Y < B.Y;
		}

		if (A.X != B.X)
		{
			return A.X < B.X;
		}

		return A.UnitId < B.UnitId;
	});

	return Summary;
}
