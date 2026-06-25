#include "WBCardActivationLegalAction.h"

#include "WBPublicBoardSummary.h"

namespace
{
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
}

FWBCardActivationLegalActionPresentationSnapshot WBCardActivationLegalActionPresentation::BuildSnapshot(
	const FWBCardActivationLegalActionSet& ActionSet,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	FWBCardActivationLegalActionPresentationSnapshot Snapshot;
	Snapshot.Entries.Reserve(ActionSet.Actions.Num());

	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		FWBCardActivationLegalActionPresentationEntry Entry;
		Entry.ActivationActionId = Action.ActivationActionId;
		Entry.PublicLabel = Action.PublicLabel.IsEmpty() ? FString(TEXT("Activate")) : Action.PublicLabel;
		Entry.PlayerId = Action.PlayerId;
		Entry.SourceUnitId = Action.SourceUnitId;
		Entry.TargetUnitId = Action.Target.TargetUnitId;

		if (const FWBPublicUnitBoardSummary* SourceUnit = FindPublicUnitById(PublicBoardSummary, Action.SourceUnitId))
		{
			Entry.bHasPublicSourceUnit = true;
			Entry.SourcePublicCardId = SourceUnit->CardId;
		}

		if (const FWBPublicUnitBoardSummary* TargetUnit = FindPublicUnitById(PublicBoardSummary, Action.Target.TargetUnitId))
		{
			Entry.bHasPublicTargetUnit = true;
			Entry.TargetPublicCardId = TargetUnit->CardId;
		}

		Snapshot.Entries.Add(Entry);
	}

	return Snapshot;
}

bool WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(
	const FWBCardActivationLegalActionPresentationSnapshot& Snapshot,
	const FString& ActivationActionId,
	FWBCardActivationLegalActionPresentationEntry& OutEntry)
{
	bool bFound = false;
	FWBCardActivationLegalActionPresentationEntry FoundEntry;
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		if (Entry.ActivationActionId != ActivationActionId)
		{
			continue;
		}

		if (bFound)
		{
			OutEntry = FWBCardActivationLegalActionPresentationEntry();
			return false;
		}

		bFound = true;
		FoundEntry = Entry;
	}

	if (!bFound)
	{
		OutEntry = FWBCardActivationLegalActionPresentationEntry();
		return false;
	}

	OutEntry = FoundEntry;
	return true;
}
