#include "WBCardZoneMutation.h"

#include "WBGameStateData.h"

namespace
{
struct FWBKnownOrderedCardLocation
{
	bool bFound = false;
	int32 ContainerPlayerId = -1;
	EWBCardZone ContainerZone = EWBCardZone::Unknown;
	FWBCardInstanceRef Card;
};

const TArray<FWBZoneCardEntry>* GetZoneMutationEntries(
	const FWBPlayerCardZoneState& PlayerZones,
	const EWBCardZone Zone)
{
	switch (Zone)
	{
	case EWBCardZone::Deck:
		return &PlayerZones.Deck;
	case EWBCardZone::Hand:
		return &PlayerZones.Hand;
	case EWBCardZone::Discard:
		return &PlayerZones.Discard;
	default:
		return nullptr;
	}
}

TArray<FWBZoneCardEntry>* GetMutableZoneMutationEntries(
	FWBPlayerCardZoneState& PlayerZones,
	const EWBCardZone Zone)
{
	switch (Zone)
	{
	case EWBCardZone::Deck:
		return &PlayerZones.Deck;
	case EWBCardZone::Hand:
		return &PlayerZones.Hand;
	case EWBCardZone::Discard:
		return &PlayerZones.Discard;
	default:
		return nullptr;
	}
}

FWBCardZoneMutationResult MakeZoneMutationResult(
	const EWBCardZoneMutationResultCode Code,
	const int32 PlayerId,
	const EWBCardZone SourceZone,
	const EWBCardZone DestinationZone)
{
	FWBCardZoneMutationResult Result;
	Result.bOk = Code == EWBCardZoneMutationResultCode::Success;
	Result.Code = Code;
	Result.Reason = WBCardZoneMutation::ResultCodeToString(Code);
	Result.PlayerId = PlayerId;
	Result.SourceZone = SourceZone;
	Result.DestinationZone = DestinationZone;
	return Result;
}

bool FindKnownOrderedCardLocation(
	const FWBCardZoneState& ZoneState,
	const FString& CardInstanceId,
	FWBKnownOrderedCardLocation& OutLocation)
{
	for (const FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		const EWBCardZone OrderedZones[] = {
			EWBCardZone::Deck,
			EWBCardZone::Hand,
			EWBCardZone::Discard
		};
		for (const EWBCardZone Zone : OrderedZones)
		{
			const TArray<FWBZoneCardEntry>* Entries =
				GetZoneMutationEntries(PlayerZones, Zone);
			const FWBZoneCardEntry* Match = Entries != nullptr
				? Entries->FindByPredicate(
					[&CardInstanceId](const FWBZoneCardEntry& Entry)
					{
						return Entry.Card.InstanceId == CardInstanceId;
					})
				: nullptr;
			if (Match != nullptr)
			{
				OutLocation.bFound = true;
				OutLocation.ContainerPlayerId = PlayerZones.PlayerId;
				OutLocation.ContainerZone = Zone;
				OutLocation.Card = Match->Card;
				return true;
			}
		}
	}

	for (const FWBEquippedCardEntry& Entry : ZoneState.EquippedCards)
	{
		if (Entry.Card.InstanceId == CardInstanceId)
		{
			OutLocation.bFound = true;
			OutLocation.ContainerPlayerId = Entry.Card.OwnerPlayerId;
			OutLocation.ContainerZone = EWBCardZone::Equipped;
			OutLocation.Card = Entry.Card;
			return true;
		}
	}

	return false;
}

bool ValidateZoneMutationIndexes(
	const TArray<FWBZoneCardEntry>& Entries,
	const bool bRequireContiguous,
	FString& OutReason)
{
	TSet<int32> SeenIndexes;
	for (int32 ArrayIndex = 0; ArrayIndex < Entries.Num(); ++ArrayIndex)
	{
		const FWBZoneCardEntry& Entry = Entries[ArrayIndex];
		if (Entry.Card.InstanceId.IsEmpty())
		{
			OutReason = TEXT("card_instance_id_empty");
			return false;
		}
		if (Entry.ZoneIndex < 0 || SeenIndexes.Contains(Entry.ZoneIndex))
		{
			OutReason = TEXT("zone_index_invalid");
			return false;
		}
		SeenIndexes.Add(Entry.ZoneIndex);
		if (bRequireContiguous && Entry.ZoneIndex != ArrayIndex)
		{
			OutReason = TEXT("zone_index_not_contiguous");
			return false;
		}
	}
	return true;
}

bool ValidateZoneMutationState(
	const FWBCardZoneState& ZoneState,
	const bool bRequireContiguous,
	FString& OutReason)
{
	if (!WBCardZoneState::ValidateZoneStateForTest(ZoneState, OutReason))
	{
		return false;
	}
	for (const FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		if (!ValidateZoneMutationIndexes(
				PlayerZones.Deck, bRequireContiguous, OutReason)
			|| !ValidateZoneMutationIndexes(
				PlayerZones.Hand, bRequireContiguous, OutReason)
			|| !ValidateZoneMutationIndexes(
				PlayerZones.Discard, bRequireContiguous, OutReason))
		{
			return false;
		}
	}
	return true;
}

void NormalizeZoneMutationEntries(TArray<FWBZoneCardEntry>& Entries)
{
	Entries.Sort([](const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
	{
		if (A.ZoneIndex != B.ZoneIndex)
		{
			return A.ZoneIndex < B.ZoneIndex;
		}
		return A.Card.InstanceId < B.Card.InstanceId;
	});
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index].ZoneIndex = Index;
	}
}

void NormalizeAllZoneMutationEntries(FWBCardZoneState& ZoneState)
{
	for (FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		NormalizeZoneMutationEntries(PlayerZones.Deck);
		NormalizeZoneMutationEntries(PlayerZones.Hand);
		NormalizeZoneMutationEntries(PlayerZones.Discard);
	}
}

bool ValidateZoneMutationRequest(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const EWBCardZone SourceZone,
	const FString& CardInstanceId,
	const FString& ExpectedCardId,
	FWBKnownOrderedCardLocation& OutLocation,
	FWBCardZoneMutationResult& OutFailure)
{
	if (!FWBGameStateData::IsValidPlayerId(PlayerId)
		|| State.GetPlayerById(PlayerId) == nullptr)
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidPlayer,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		return false;
	}
	if (!WBCardZoneState::IsOrderedZone(SourceZone))
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::UnsupportedSourceZone,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		return false;
	}
	if (CardInstanceId.IsEmpty())
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::CardInstanceMissing,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		return false;
	}

	const FWBCardZoneState& ZoneState = State.GetCardZoneState();
	const FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindPlayerZones(ZoneState, PlayerId);
	if (PlayerZones == nullptr)
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::PlayerZonesMissing,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		return false;
	}

	FString DuplicateInstanceId;
	if (WBCardZoneState::HasDuplicateCardInstanceIds(
		ZoneState, DuplicateInstanceId))
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::DuplicateInstanceId,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		OutFailure.Card.InstanceId = DuplicateInstanceId;
		return false;
	}

	if (!FindKnownOrderedCardLocation(
		ZoneState, CardInstanceId, OutLocation))
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::CardInstanceMissing,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		OutFailure.Card.InstanceId = CardInstanceId;
		return false;
	}
	OutFailure.Card = OutLocation.Card;
	if (OutLocation.ContainerPlayerId != PlayerId
		|| OutLocation.Card.OwnerPlayerId != PlayerId)
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::OwnerMismatch,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		OutFailure.Card = OutLocation.Card;
		return false;
	}
	if (OutLocation.ContainerZone != SourceZone)
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::CardNotInExpectedZone,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		OutFailure.Card = OutLocation.Card;
		return false;
	}
	if (!ExpectedCardId.IsEmpty()
		&& OutLocation.Card.CardId != ExpectedCardId)
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::CardIdentityMismatch,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		OutFailure.Card = OutLocation.Card;
		return false;
	}

	FString ValidationReason;
	if (!ValidateZoneMutationState(
		ZoneState, false, ValidationReason))
	{
		OutFailure = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidZoneState,
			PlayerId,
			SourceZone,
			EWBCardZone::Unknown);
		OutFailure.Reason = ValidationReason;
		OutFailure.Card = OutLocation.Card;
		return false;
	}
	return true;
}
}

FWBCardZoneMutationResult WBCardZoneMutation::TransferExact(
	FWBGameStateData& State,
	const FWBCardZoneTransferRequest& Request)
{
	FWBKnownOrderedCardLocation Location;
	FWBCardZoneMutationResult Failure;
	if (!ValidateZoneMutationRequest(
		State,
		Request.PlayerId,
		Request.SourceZone,
		Request.CardInstanceId,
		Request.ExpectedCardId,
		Location,
		Failure))
	{
		Failure.DestinationZone = Request.DestinationZone;
		return Failure;
	}
	if (!WBCardZoneState::IsOrderedZone(Request.DestinationZone))
	{
		return MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::UnsupportedDestinationZone,
			Request.PlayerId,
			Request.SourceZone,
			Request.DestinationZone);
	}
	if (Request.SourceZone == Request.DestinationZone)
	{
		return MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::SameZoneTransferUnsupported,
			Request.PlayerId,
			Request.SourceZone,
			Request.DestinationZone);
	}
	if (Request.DestinationPlacement != EWBOrderedZonePlacement::Append)
	{
		return MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidDestinationPlacement,
			Request.PlayerId,
			Request.SourceZone,
			Request.DestinationZone);
	}

	FWBGameStateData WorkingState = State;
	FWBCardZoneState& WorkingZones =
		WorkingState.GetMutableCardZoneStateForTest();
	NormalizeAllZoneMutationEntries(WorkingZones);
	FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindMutablePlayerZones(
			WorkingZones, Request.PlayerId);
	TArray<FWBZoneCardEntry>* SourceEntries = PlayerZones != nullptr
		? GetMutableZoneMutationEntries(*PlayerZones, Request.SourceZone)
		: nullptr;
	TArray<FWBZoneCardEntry>* DestinationEntries = PlayerZones != nullptr
		? GetMutableZoneMutationEntries(*PlayerZones, Request.DestinationZone)
		: nullptr;
	if (SourceEntries == nullptr || DestinationEntries == nullptr)
	{
		return MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidZoneState,
			Request.PlayerId,
			Request.SourceZone,
			Request.DestinationZone);
	}
	const int32 SourceIndex = SourceEntries->IndexOfByPredicate(
		[&Request](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == Request.CardInstanceId;
		});
	if (SourceIndex == INDEX_NONE)
	{
		return MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidZoneState,
			Request.PlayerId,
			Request.SourceZone,
			Request.DestinationZone);
	}

	FWBZoneCardEntry Moved = (*SourceEntries)[SourceIndex];
	SourceEntries->RemoveAt(SourceIndex, 1, EAllowShrinking::No);
	NormalizeZoneMutationEntries(*SourceEntries);
	Moved.Zone = Request.DestinationZone;
	Moved.ZoneIndex = DestinationEntries->Num();
	DestinationEntries->Add(Moved);
	NormalizeZoneMutationEntries(*DestinationEntries);

	FString FinalReason;
	if (!ValidateZoneMutationState(WorkingZones, true, FinalReason))
	{
		FWBCardZoneMutationResult Result = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidZoneState,
			Request.PlayerId,
			Request.SourceZone,
			Request.DestinationZone);
		Result.Reason = FinalReason;
		Result.Card = Location.Card;
		return Result;
	}

	FWBCardZoneMutationResult Result = MakeZoneMutationResult(
		EWBCardZoneMutationResultCode::Success,
		Request.PlayerId,
		Request.SourceZone,
		Request.DestinationZone);
	Result.Card = Moved.Card;
	Result.SourceZoneCountAfter = SourceEntries->Num();
	Result.DestinationZoneCountAfter = DestinationEntries->Num();
	State = MoveTemp(WorkingState);
	return Result;
}

FWBCardZoneMutationResult WBCardZoneMutation::ExtractExact(
	FWBGameStateData& State,
	const FWBCardZoneExtractRequest& Request)
{
	FWBKnownOrderedCardLocation Location;
	FWBCardZoneMutationResult Failure;
	if (!ValidateZoneMutationRequest(
		State,
		Request.PlayerId,
		Request.SourceZone,
		Request.CardInstanceId,
		Request.ExpectedCardId,
		Location,
		Failure))
	{
		return Failure;
	}

	FWBGameStateData WorkingState = State;
	FWBCardZoneState& WorkingZones =
		WorkingState.GetMutableCardZoneStateForTest();
	NormalizeAllZoneMutationEntries(WorkingZones);
	FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindMutablePlayerZones(
			WorkingZones, Request.PlayerId);
	TArray<FWBZoneCardEntry>* SourceEntries = PlayerZones != nullptr
		? GetMutableZoneMutationEntries(*PlayerZones, Request.SourceZone)
		: nullptr;
	if (SourceEntries == nullptr)
	{
		return MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidZoneState,
			Request.PlayerId,
			Request.SourceZone,
			EWBCardZone::Unknown);
	}
	const int32 SourceIndex = SourceEntries->IndexOfByPredicate(
		[&Request](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == Request.CardInstanceId;
		});
	if (SourceIndex == INDEX_NONE)
	{
		return MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidZoneState,
			Request.PlayerId,
			Request.SourceZone,
			EWBCardZone::Unknown);
	}

	const FWBZoneCardEntry Extracted = (*SourceEntries)[SourceIndex];
	SourceEntries->RemoveAt(SourceIndex, 1, EAllowShrinking::No);
	NormalizeZoneMutationEntries(*SourceEntries);
	FString FinalReason;
	if (!ValidateZoneMutationState(WorkingZones, true, FinalReason))
	{
		FWBCardZoneMutationResult Result = MakeZoneMutationResult(
			EWBCardZoneMutationResultCode::InvalidZoneState,
			Request.PlayerId,
			Request.SourceZone,
			EWBCardZone::Unknown);
		Result.Reason = FinalReason;
		Result.Card = Location.Card;
		return Result;
	}

	FWBCardZoneMutationResult Result = MakeZoneMutationResult(
		EWBCardZoneMutationResultCode::Success,
		Request.PlayerId,
		Request.SourceZone,
		EWBCardZone::Unknown);
	Result.Card = Extracted.Card;
	Result.SourceZoneCountAfter = SourceEntries->Num();
	State = MoveTemp(WorkingState);
	return Result;
}

FString WBCardZoneMutation::ResultCodeToString(
	const EWBCardZoneMutationResultCode Code)
{
	switch (Code)
	{
	case EWBCardZoneMutationResultCode::Success:
		return TEXT("success");
	case EWBCardZoneMutationResultCode::InvalidPlayer:
		return TEXT("invalid_player");
	case EWBCardZoneMutationResultCode::PlayerZonesMissing:
		return TEXT("player_zones_missing");
	case EWBCardZoneMutationResultCode::CardInstanceMissing:
		return TEXT("card_instance_missing");
	case EWBCardZoneMutationResultCode::CardNotInExpectedZone:
		return TEXT("card_not_in_expected_zone");
	case EWBCardZoneMutationResultCode::DuplicateInstanceId:
		return TEXT("duplicate_instance_id");
	case EWBCardZoneMutationResultCode::OwnerMismatch:
		return TEXT("owner_mismatch");
	case EWBCardZoneMutationResultCode::CardIdentityMismatch:
		return TEXT("card_identity_mismatch");
	case EWBCardZoneMutationResultCode::InvalidZoneState:
		return TEXT("invalid_zone_state");
	case EWBCardZoneMutationResultCode::UnsupportedSourceZone:
		return TEXT("unsupported_source_zone");
	case EWBCardZoneMutationResultCode::UnsupportedDestinationZone:
		return TEXT("unsupported_destination_zone");
	case EWBCardZoneMutationResultCode::SameZoneTransferUnsupported:
		return TEXT("same_zone_transfer_unsupported");
	case EWBCardZoneMutationResultCode::InvalidDestinationPlacement:
		return TEXT("invalid_destination_placement");
	default:
		return TEXT("invalid_zone_state");
	}
}
