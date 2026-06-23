#include "WBCardActivationLegalAction.h"

FWBCardActivationLegalActionPresentationSnapshot WBCardActivationLegalActionPresentation::BuildSnapshot(
	const FWBCardActivationLegalActionSet& ActionSet)
{
	FWBCardActivationLegalActionPresentationSnapshot Snapshot;
	Snapshot.Entries.Reserve(ActionSet.Actions.Num());

	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		FWBCardActivationLegalActionPresentationEntry Entry;
		Entry.ActivationActionId = Action.ActivationActionId;
		Entry.PublicLabel = Action.PublicLabel;
		Entry.PlayerId = Action.PlayerId;
		Entry.SourceUnitId = Action.SourceUnitId;
		Entry.TargetUnitId = Action.Target.TargetUnitId;
		Entry.SourceCardId = Action.Candidate.SourceCardId;
		Entry.SourceEffectId = Action.Candidate.SourceEffectId;
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
