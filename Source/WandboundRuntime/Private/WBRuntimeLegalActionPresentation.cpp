#include "WBRuntimeLegalActionPresentation.h"

#include "WBActionCodec.h"

namespace
{
EWBRuntimeActionPresentationType PresentationTypeForAction(const EWBActionType Type)
{
	switch (Type)
	{
	case EWBActionType::Move:
		return EWBRuntimeActionPresentationType::Move;
	case EWBActionType::Attack:
		return EWBRuntimeActionPresentationType::Attack;
	case EWBActionType::EndTurn:
		return EWBRuntimeActionPresentationType::EndTurn;
	case EWBActionType::Pass:
		return EWBRuntimeActionPresentationType::Pass;
	case EWBActionType::PassResponse:
		return EWBRuntimeActionPresentationType::PassResponse;
	default:
		return EWBRuntimeActionPresentationType::Unknown;
	}
}

FString PublicLabelForPresentationType(const EWBRuntimeActionPresentationType Type)
{
	switch (Type)
	{
	case EWBRuntimeActionPresentationType::Move:
		return TEXT("Move");
	case EWBRuntimeActionPresentationType::Attack:
		return TEXT("Attack");
	case EWBRuntimeActionPresentationType::EndTurn:
		return TEXT("End Turn");
	case EWBRuntimeActionPresentationType::Pass:
	case EWBRuntimeActionPresentationType::PassResponse:
		return TEXT("Pass");
	default:
		return TEXT("Action");
	}
}

const FWBPublicUnitBoardSummary* FindPublicUnitById(
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

FWBRuntimeActionPresentationEntry MakeEntry(
	const FWBAction& Action,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	FWBRuntimeActionPresentationEntry Entry;
	Entry.ActionId = WBActionCodec::MakeActionId(Action);
	Entry.Type = PresentationTypeForAction(Action.Type);
	Entry.PublicLabel = PublicLabelForPresentationType(Entry.Type);
	Entry.PlayerId = Action.PlayerId;
	Entry.SourceUnitId = Action.SourceUnitId;
	Entry.TargetUnitId = Action.TargetUnitId;
	Entry.FromTile = Action.FromTile;
	Entry.ToTile = Action.ToTile;

	if (const FWBPublicUnitBoardSummary* SourceUnit = FindPublicUnitById(PublicBoardSummary, Action.SourceUnitId))
	{
		Entry.bHasSourcePublicUnit = true;
		Entry.SourcePublicCardId = SourceUnit->CardId;
	}

	if (const FWBPublicUnitBoardSummary* TargetUnit = FindPublicUnitById(PublicBoardSummary, Action.TargetUnitId))
	{
		Entry.bHasTargetPublicUnit = true;
		Entry.TargetPublicCardId = TargetUnit->CardId;
	}

	return Entry;
}
}

FWBRuntimeLegalActionPresentationSnapshot WBRuntimeLegalActionPresentation::BuildSnapshot(
	const TArray<FWBAction>& PrecomputedLegalActions,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	FWBRuntimeLegalActionPresentationSnapshot Snapshot;
	Snapshot.Entries.Reserve(PrecomputedLegalActions.Num());

	for (const FWBAction& Action : PrecomputedLegalActions)
	{
		Snapshot.Entries.Add(MakeEntry(Action, PublicBoardSummary));
	}

	return Snapshot;
}

bool WBRuntimeLegalActionPresentation::TryFindEntryByActionId(
	const FWBRuntimeLegalActionPresentationSnapshot& Snapshot,
	const FString& ActionId,
	FWBRuntimeActionPresentationEntry& OutEntry)
{
	int32 MatchCount = 0;
	FWBRuntimeActionPresentationEntry MatchedEntry;

	for (const FWBRuntimeActionPresentationEntry& Entry : Snapshot.Entries)
	{
		if (Entry.ActionId == ActionId)
		{
			++MatchCount;
			MatchedEntry = Entry;
		}
	}

	if (MatchCount != 1)
	{
		return false;
	}

	OutEntry = MatchedEntry;
	return true;
}
