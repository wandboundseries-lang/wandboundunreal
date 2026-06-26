#include "WBCardActivationTargetPresentation.h"

#include "WBPublicBoardSummary.h"

namespace
{
const FWBPublicUnitBoardSummary* FindTargetPresentationPublicUnitById(
	const FWBPublicBoardSummary& PublicBoardSummary,
	const int32 UnitId)
{
	if (UnitId == -1)
	{
		return nullptr;
	}

	for (const FWBPublicUnitBoardSummary& Unit : PublicBoardSummary.Units)
	{
		if (Unit.UnitId == UnitId)
		{
			return &Unit;
		}
	}

	return nullptr;
}

bool IsUnsetTile(const FWBTile& Tile)
{
	return Tile.X == -1 && Tile.Y == -1;
}

bool IsValidTilePresentationTarget(const FWBTile& Tile)
{
	return Tile.X != -1 && Tile.Y != -1;
}

bool IsUnsetWallEdge(const FWBWallEdge& WallEdge)
{
	return IsUnsetTile(WallEdge.A) && IsUnsetTile(WallEdge.B);
}

bool IsValidWallEdgePresentationTarget(const FWBWallEdge& WallEdge)
{
	return IsValidTilePresentationTarget(WallEdge.A) && IsValidTilePresentationTarget(WallEdge.B);
}

bool HasPartialTargetData(const FWBEffectTargetRef& Target)
{
	return !IsUnsetTile(Target.TargetTile) || !IsUnsetWallEdge(Target.TargetWallEdge);
}

EWBCardActivationTargetPresentationKind DetermineTargetKind(const FWBEffectTargetRef& Target)
{
	if (Target.TargetUnitId != -1)
	{
		return EWBCardActivationTargetPresentationKind::Unit;
	}

	if (IsValidTilePresentationTarget(Target.TargetTile))
	{
		return EWBCardActivationTargetPresentationKind::Tile;
	}

	if (IsValidWallEdgePresentationTarget(Target.TargetWallEdge))
	{
		return EWBCardActivationTargetPresentationKind::WallEdge;
	}

	if (!HasPartialTargetData(Target))
	{
		return EWBCardActivationTargetPresentationKind::None;
	}

	return EWBCardActivationTargetPresentationKind::Unknown;
}

FString PublicTargetLabelForKind(const EWBCardActivationTargetPresentationKind TargetKind)
{
	switch (TargetKind)
	{
	case EWBCardActivationTargetPresentationKind::None:
		return TEXT("Activate");
	case EWBCardActivationTargetPresentationKind::Unit:
		return TEXT("Choose Unit");
	case EWBCardActivationTargetPresentationKind::Tile:
		return TEXT("Choose Tile");
	case EWBCardActivationTargetPresentationKind::WallEdge:
		return TEXT("Choose Wall");
	default:
		return TEXT("Choose Target");
	}
}
}

FWBCardActivationTargetPresentationSnapshot WBCardActivationTargetPresentation::BuildSnapshot(
	const FWBCardActivationLegalActionSet& ActionSet,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	FWBCardActivationTargetPresentationSnapshot Snapshot;
	Snapshot.Entries.Reserve(ActionSet.Actions.Num());

	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		FWBCardActivationTargetPresentationEntry Entry;
		Entry.ActivationActionId = Action.ActivationActionId;
		Entry.PublicActivationLabel = Action.PublicLabel.IsEmpty() ? FString(TEXT("Activate")) : Action.PublicLabel;
		Entry.PlayerId = Action.PlayerId;
		Entry.SourceUnitId = Action.SourceUnitId;
		Entry.TargetUnitId = Action.Target.TargetUnitId;
		Entry.TargetTile = Action.Target.TargetTile;
		Entry.TargetWallEdge = Action.Target.TargetWallEdge;
		Entry.TargetKind = DetermineTargetKind(Action.Target);
		Entry.PublicTargetLabel = PublicTargetLabelForKind(Entry.TargetKind);

		if (const FWBPublicUnitBoardSummary* SourceUnit = FindTargetPresentationPublicUnitById(PublicBoardSummary, Action.SourceUnitId))
		{
			Entry.bHasPublicSourceUnit = true;
			Entry.SourcePublicCardId = SourceUnit->CardId;
		}

		if (Entry.TargetKind == EWBCardActivationTargetPresentationKind::Unit)
		{
			if (const FWBPublicUnitBoardSummary* TargetUnit = FindTargetPresentationPublicUnitById(PublicBoardSummary, Action.Target.TargetUnitId))
			{
				Entry.bHasPublicTargetUnit = true;
				Entry.TargetPublicCardId = TargetUnit->CardId;
			}
		}

		Snapshot.Entries.Add(Entry);
	}

	return Snapshot;
}

bool WBCardActivationTargetPresentation::TryFindEntryByActivationActionId(
	const FWBCardActivationTargetPresentationSnapshot& Snapshot,
	const FString& ActivationActionId,
	FWBCardActivationTargetPresentationEntry& OutEntry)
{
	bool bFound = false;
	FWBCardActivationTargetPresentationEntry FoundEntry;
	for (const FWBCardActivationTargetPresentationEntry& Entry : Snapshot.Entries)
	{
		if (Entry.ActivationActionId != ActivationActionId)
		{
			continue;
		}

		if (bFound)
		{
			OutEntry = FWBCardActivationTargetPresentationEntry();
			return false;
		}

		bFound = true;
		FoundEntry = Entry;
	}

	if (!bFound)
	{
		OutEntry = FWBCardActivationTargetPresentationEntry();
		return false;
	}

	OutEntry = FoundEntry;
	return true;
}
