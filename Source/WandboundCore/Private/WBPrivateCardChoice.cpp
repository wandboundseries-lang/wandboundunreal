#include "WBPrivateCardChoice.h"

#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"

namespace
{
const TArray<FWBZoneCardEntry>* FindZone(
	const FWBPlayerCardZoneState& Zones,
	const EWBCardZone Zone)
{
	switch (Zone)
	{
	case EWBCardZone::Deck: return &Zones.Deck;
	case EWBCardZone::Hand: return &Zones.Hand;
	case EWBCardZone::Discard: return &Zones.Discard;
	default: return nullptr;
	}
}

bool MatchesFilter(
	const FWBZoneCardEntry& Entry,
	const FWBCardDefinitionRepository& Repository,
	const FWBPrivateCardChoiceFilter& Filter)
{
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, Entry.Card.CardId);
	if (!Lookup.bFound) return false;
	if (Filter.RequiredKind != EWBCardDefinitionKind::Unknown
		&& Lookup.Definition.Kind != Filter.RequiredKind)
	{
		return false;
	}
	if (!Filter.RequiredFaction.IsEmpty()
		&& !Lookup.Definition.PublicFactions.Contains(Filter.RequiredFaction))
	{
		return false;
	}
	return Filter.RequiredCardId.IsEmpty()
		|| Entry.Card.CardId == Filter.RequiredCardId;
}

bool CandidateLess(
	const FWBPrivateCardChoiceCandidate& A,
	const FWBPrivateCardChoiceCandidate& B)
{
	if (A.ZoneIndex != B.ZoneIndex) return A.ZoneIndex < B.ZoneIndex;
	if (A.CardInstanceId != B.CardInstanceId)
	{
		return A.CardInstanceId < B.CardInstanceId;
	}
	return A.CardId < B.CardId;
}

FWBPrivateCardChoiceCandidateResult FailCandidates(const FString& Reason)
{
	FWBPrivateCardChoiceCandidateResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBPrivateCardChoiceSelectionResult FailSelection(const FString& Reason)
{
	FWBPrivateCardChoiceSelectionResult Result;
	Result.Reason = Reason;
	return Result;
}
}

bool WBPrivateCardChoice::IsSupportedPrivateZone(const EWBCardZone Zone)
{
	return Zone == EWBCardZone::Deck
		|| Zone == EWBCardZone::Hand
		|| Zone == EWBCardZone::Discard;
}

FWBPrivateCardChoiceCandidateResult WBPrivateCardChoice::EnumerateCandidates(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBPrivateCardChoiceDescriptor& Descriptor)
{
	if (!FWBGameStateData::IsValidPlayerId(Descriptor.ChoosingPlayerId))
	{
		return FailCandidates(TEXT("private_choice_player_invalid"));
	}
	if (!IsSupportedPrivateZone(Descriptor.SourceZone))
	{
		return FailCandidates(TEXT("private_choice_zone_unsupported"));
	}
	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), Descriptor.ChoosingPlayerId);
	if (PlayerZones == nullptr)
	{
		return FailCandidates(TEXT("private_choice_player_zones_missing"));
	}
	const TArray<FWBZoneCardEntry>* Entries = FindZone(
		*PlayerZones, Descriptor.SourceZone);
	if (Entries == nullptr)
	{
		return FailCandidates(TEXT("private_choice_zone_unsupported"));
	}

	FWBPrivateCardChoiceCandidateResult Result;
	for (const FWBZoneCardEntry& Entry : *Entries)
	{
		if (Entry.Zone != Descriptor.SourceZone
			|| Entry.Card.OwnerPlayerId != Descriptor.ChoosingPlayerId
			|| Entry.Card.InstanceId.IsEmpty()
			|| !MatchesFilter(Entry, Repository, Descriptor.Filter))
		{
			continue;
		}
		FWBPrivateCardChoiceCandidate Candidate;
		Candidate.CardInstanceId = Entry.Card.InstanceId;
		Candidate.CardId = Entry.Card.CardId;
		Candidate.ZoneIndex = Entry.ZoneIndex;
		Result.Candidates.Add(MoveTemp(Candidate));
	}
	Result.Candidates.Sort(CandidateLess);
	Result.bOk = true;
	return Result;
}

FWBPrivateCardChoiceCandidateResult WBPrivateCardChoice::FreezeCandidates(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBPrivateCardChoiceDescriptor& InOutDescriptor)
{
	FWBPrivateCardChoiceCandidateResult Result = EnumerateCandidates(
		State, Repository, InOutDescriptor);
	if (!Result.bOk) return Result;
	InOutDescriptor.FrozenCandidateInstanceIds.Reset(Result.Candidates.Num());
	for (const FWBPrivateCardChoiceCandidate& Candidate : Result.Candidates)
	{
		InOutDescriptor.FrozenCandidateInstanceIds.Add(Candidate.CardInstanceId);
	}
	return Result;
}

FWBPrivateCardChoiceSelectionResult WBPrivateCardChoice::ValidateSelection(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBPrivateCardChoiceDescriptor& Descriptor,
	const FString& CardInstanceId,
	const bool bRequireFrozenCandidate)
{
	if (CardInstanceId.IsEmpty())
	{
		return FailSelection(TEXT("private_choice_instance_missing"));
	}
	if (bRequireFrozenCandidate
		&& !Descriptor.FrozenCandidateInstanceIds.Contains(CardInstanceId))
	{
		return FailSelection(TEXT("private_choice_instance_not_frozen"));
	}
	const FWBPrivateCardChoiceCandidateResult Current = EnumerateCandidates(
		State, Repository, Descriptor);
	if (!Current.bOk) return FailSelection(Current.Reason);
	const FWBPrivateCardChoiceCandidate* Selected =
		Current.Candidates.FindByPredicate(
			[&CardInstanceId](const FWBPrivateCardChoiceCandidate& Candidate)
			{
				return Candidate.CardInstanceId == CardInstanceId;
			});
	if (Selected == nullptr)
	{
		return FailSelection(TEXT("private_choice_instance_unavailable_or_ineligible"));
	}
	FWBPrivateCardChoiceSelectionResult Result;
	Result.bOk = true;
	Result.Selected = *Selected;
	return Result;
}
