#include "WBCardLifecycle.h"

#include "WBCardZoneMutation.h"
#include "WBCardZoneState.h"

namespace
{
bool IsSuccessCode(const EWBCardLifecycleResultCode Code)
{
	return Code == EWBCardLifecycleResultCode::Success
		|| Code == EWBCardLifecycleResultCode::FirstPlayerFirstTurnDrawSkipped;
}

FWBCardLifecycleResult MakeResult(
	const EWBCardLifecycleResultCode Code,
	const int32 PlayerId,
	const FString& Reason = FString())
{
	FWBCardLifecycleResult Result;
	Result.bOk = IsSuccessCode(Code);
	Result.Code = Code;
	Result.Reason = Reason.IsEmpty()
		? WBCardLifecycle::ResultCodeToString(Code)
		: Reason;
	Result.PlayerId = PlayerId;
	return Result;
}

void SortOrderedZones(FWBCardZoneState& ZoneState)
{
	WBCardZoneState::SortOrderedZonesDeterministically(ZoneState);
}

int32 MaxZoneIndex(const TArray<FWBZoneCardEntry>& Entries)
{
	int32 MaxIndex = INDEX_NONE;
	for (const FWBZoneCardEntry& Entry : Entries)
	{
		MaxIndex = FMath::Max(MaxIndex, Entry.ZoneIndex);
	}
	return MaxIndex;
}

FWBCardLifecycleResult ValidatePlayerAndZones(
	FWBGameStateData& State,
	const int32 PlayerId,
	FWBCardZoneState*& OutZoneState,
	FWBPlayerCardZoneState*& OutPlayerZones)
{
	OutZoneState = nullptr;
	OutPlayerZones = nullptr;

	if (!FWBGameStateData::IsValidPlayerId(PlayerId)
		|| State.GetPlayerById(PlayerId) == nullptr)
	{
		return MakeResult(EWBCardLifecycleResultCode::InvalidPlayer, PlayerId);
	}

	FWBCardZoneState& ZoneState = State.GetMutableCardZoneStateForTest();

	FString DuplicateInstanceId;
	if (WBCardZoneState::HasDuplicateCardInstanceIds(ZoneState, DuplicateInstanceId))
	{
		FWBCardLifecycleResult Result = MakeResult(
			EWBCardLifecycleResultCode::DuplicateInstanceId,
			PlayerId);
		Result.CardInstanceId = DuplicateInstanceId;
		return Result;
	}

	FString ValidationReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(ZoneState, ValidationReason))
	{
		return MakeResult(EWBCardLifecycleResultCode::InvalidZoneState, PlayerId);
	}

	SortOrderedZones(ZoneState);

	FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindMutablePlayerZones(ZoneState, PlayerId);
	if (PlayerZones == nullptr)
	{
		return MakeResult(EWBCardLifecycleResultCode::PlayerZonesMissing, PlayerId);
	}

	OutZoneState = &ZoneState;
	OutPlayerZones = PlayerZones;
	return MakeResult(EWBCardLifecycleResultCode::Success, PlayerId);
}

FWBCardLifecycleResult MakeSuccessfulMoveResult(
	const int32 PlayerId,
	const FWBZoneCardEntry& Card,
	const int32 SourceCountAfter,
	const int32 DestinationCountAfter)
{
	FWBCardLifecycleResult Result = MakeResult(EWBCardLifecycleResultCode::Success, PlayerId);
	Result.CardInstanceId = Card.Card.InstanceId;
	Result.CardId = Card.Card.CardId;
	Result.SourceZoneCountAfter = SourceCountAfter;
	Result.DestinationZoneCountAfter = DestinationCountAfter;
	return Result;
}

EWBCardLifecycleResultCode ToLifecycleResultCode(
	const EWBCardZoneMutationResultCode Code)
{
	switch (Code)
	{
	case EWBCardZoneMutationResultCode::Success:
		return EWBCardLifecycleResultCode::Success;
	case EWBCardZoneMutationResultCode::InvalidPlayer:
		return EWBCardLifecycleResultCode::InvalidPlayer;
	case EWBCardZoneMutationResultCode::PlayerZonesMissing:
		return EWBCardLifecycleResultCode::PlayerZonesMissing;
	case EWBCardZoneMutationResultCode::CardInstanceMissing:
		return EWBCardLifecycleResultCode::CardInstanceMissing;
	case EWBCardZoneMutationResultCode::CardNotInExpectedZone:
	case EWBCardZoneMutationResultCode::OwnerMismatch:
	case EWBCardZoneMutationResultCode::CardIdentityMismatch:
		return EWBCardLifecycleResultCode::CardNotInExpectedZone;
	case EWBCardZoneMutationResultCode::DuplicateInstanceId:
		return EWBCardLifecycleResultCode::DuplicateInstanceId;
	case EWBCardZoneMutationResultCode::InvalidZoneState:
		return EWBCardLifecycleResultCode::InvalidZoneState;
	case EWBCardZoneMutationResultCode::UnsupportedSourceZone:
	case EWBCardZoneMutationResultCode::UnsupportedDestinationZone:
	case EWBCardZoneMutationResultCode::SameZoneTransferUnsupported:
	case EWBCardZoneMutationResultCode::InvalidDestinationPlacement:
	default:
		return EWBCardLifecycleResultCode::UnsupportedLifecycleOperation;
	}
}

FWBCardLifecycleResult FromZoneMutation(
	const FWBCardZoneMutationResult& Mutation)
{
	FWBCardLifecycleResult Result = MakeResult(
		ToLifecycleResultCode(Mutation.Code),
		Mutation.PlayerId,
		Mutation.Reason);
	Result.CardInstanceId = Mutation.Card.InstanceId;
	Result.CardId = Mutation.Card.CardId;
	Result.SourceZoneCountAfter = Mutation.SourceZoneCountAfter;
	Result.DestinationZoneCountAfter =
		Mutation.DestinationZoneCountAfter;
	return Result;
}
}

FWBCardLifecycleResult WBCardLifecycle::DrawOneCard(
	FWBGameStateData& State,
	const int32 PlayerId)
{
	FWBGameStateData OrderedState = State;
	FWBCardZoneState* ZoneState = nullptr;
	FWBPlayerCardZoneState* PlayerZones = nullptr;
	FWBCardLifecycleResult ValidationResult = ValidatePlayerAndZones(
		OrderedState, PlayerId, ZoneState, PlayerZones);
	if (!ValidationResult.bOk)
	{
		return ValidationResult;
	}

	if (PlayerZones->Deck.Num() <= 0)
	{
		FWBCardLifecycleResult Result = MakeResult(EWBCardLifecycleResultCode::DeckEmpty, PlayerId);
		Result.SourceZoneCountAfter = 0;
		Result.DestinationZoneCountAfter = PlayerZones->Hand.Num();
		return Result;
	}

	FWBCardZoneTransferRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceZone = EWBCardZone::Deck;
	Request.DestinationZone = EWBCardZone::Hand;
	Request.CardInstanceId = PlayerZones->Deck[0].Card.InstanceId;
	Request.ExpectedCardId = PlayerZones->Deck[0].Card.CardId;
	Request.DestinationPlacement = EWBOrderedZonePlacement::Append;
	return FromZoneMutation(
		WBCardZoneMutation::TransferExact(State, Request));
}

FWBCardLifecycleResult WBCardLifecycle::DrawCards(
	FWBGameStateData& State,
	const int32 PlayerId,
	const int32 Count)
{
	if (Count <= 0)
	{
		FWBCardZoneState* ZoneState = nullptr;
		FWBPlayerCardZoneState* PlayerZones = nullptr;
		FWBCardLifecycleResult ValidationResult = ValidatePlayerAndZones(State, PlayerId, ZoneState, PlayerZones);
		if (!ValidationResult.bOk)
		{
			return ValidationResult;
		}

		ValidationResult.SourceZoneCountAfter = PlayerZones->Deck.Num();
		ValidationResult.DestinationZoneCountAfter = PlayerZones->Hand.Num();
		return ValidationResult;
	}

	FWBCardLifecycleResult LastResult = MakeResult(EWBCardLifecycleResultCode::Success, PlayerId);
	for (int32 DrawIndex = 0; DrawIndex < Count; ++DrawIndex)
	{
		LastResult = DrawOneCard(State, PlayerId);
		if (!LastResult.bOk)
		{
			return LastResult;
		}
	}

	return LastResult;
}

FWBCardLifecycleResult WBCardLifecycle::MoveHandCardToDiscard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& CardInstanceId)
{
	FWBCardZoneTransferRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceZone = EWBCardZone::Hand;
	Request.DestinationZone = EWBCardZone::Discard;
	Request.CardInstanceId = CardInstanceId;
	Request.DestinationPlacement = EWBOrderedZonePlacement::Append;
	return FromZoneMutation(
		WBCardZoneMutation::TransferExact(State, Request));
}

FWBCardLifecycleResult WBCardLifecycle::MoveEquippedCardToDiscard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& CardInstanceId)
{
	FWBCardZoneState* ZoneState = nullptr;
	FWBPlayerCardZoneState* PlayerZones = nullptr;
	FWBCardLifecycleResult ValidationResult = ValidatePlayerAndZones(State, PlayerId, ZoneState, PlayerZones);
	if (!ValidationResult.bOk)
	{
		return ValidationResult;
	}

	if (CardInstanceId.IsEmpty())
	{
		return MakeResult(EWBCardLifecycleResultCode::CardInstanceMissing, PlayerId);
	}

	int32 EquippedIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ZoneState->EquippedCards.Num(); ++Index)
	{
		const FWBEquippedCardEntry& Entry = ZoneState->EquippedCards[Index];
		if (Entry.Card.InstanceId == CardInstanceId)
		{
			if (Entry.Card.OwnerPlayerId != PlayerId)
			{
				FWBCardLifecycleResult Result = MakeResult(
					EWBCardLifecycleResultCode::CardNotInExpectedZone,
					PlayerId);
				Result.CardInstanceId = CardInstanceId;
				Result.CardId = Entry.Card.CardId;
				Result.SourceZoneCountAfter = ZoneState->EquippedCards.Num();
				Result.DestinationZoneCountAfter = PlayerZones->Discard.Num();
				return Result;
			}

			EquippedIndex = Index;
			break;
		}
	}

	if (EquippedIndex == INDEX_NONE)
	{
		FWBZoneCardEntry ExistingEntry;
		if (WBCardZoneState::FindCardByInstanceId(*ZoneState, CardInstanceId, ExistingEntry))
		{
			FWBCardLifecycleResult Result = MakeResult(
				EWBCardLifecycleResultCode::CardNotInExpectedZone,
				PlayerId);
			Result.CardInstanceId = CardInstanceId;
			Result.CardId = ExistingEntry.Card.CardId;
			Result.SourceZoneCountAfter = ZoneState->EquippedCards.Num();
			Result.DestinationZoneCountAfter = PlayerZones->Discard.Num();
			return Result;
		}

		FWBCardLifecycleResult Result = MakeResult(EWBCardLifecycleResultCode::CardInstanceMissing, PlayerId);
		Result.CardInstanceId = CardInstanceId;
		Result.SourceZoneCountAfter = ZoneState->EquippedCards.Num();
		Result.DestinationZoneCountAfter = PlayerZones->Discard.Num();
		return Result;
	}

	const FWBEquippedCardEntry EquippedCard = ZoneState->EquippedCards[EquippedIndex];
	ZoneState->EquippedCards.RemoveAt(EquippedIndex, 1, EAllowShrinking::No);

	FWBZoneCardEntry DiscardedCard;
	DiscardedCard.Card = EquippedCard.Card;
	DiscardedCard.Card.OwnerPlayerId = PlayerId;
	DiscardedCard.Zone = EWBCardZone::Discard;
	DiscardedCard.ZoneIndex = MaxZoneIndex(PlayerZones->Discard) + 1;
	PlayerZones->Discard.Add(DiscardedCard);

	const int32 SourceCountAfter = ZoneState->EquippedCards.Num();
	const int32 DestinationCountAfter = PlayerZones->Discard.Num();
	SortOrderedZones(*ZoneState);
	return MakeSuccessfulMoveResult(
		PlayerId,
		DiscardedCard,
		SourceCountAfter,
		DestinationCountAfter);
}

FWBCardLifecycleResult WBCardLifecycle::RemoveExactCardFromDeck(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& CardInstanceId)
{
	FWBCardZoneExtractRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceZone = EWBCardZone::Deck;
	Request.CardInstanceId = CardInstanceId;
	return FromZoneMutation(
		WBCardZoneMutation::ExtractExact(State, Request));
}

FWBCardLifecycleResult WBCardLifecycle::RemoveExactCardFromHand(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& CardInstanceId)
{
	FWBCardZoneExtractRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceZone = EWBCardZone::Hand;
	Request.CardInstanceId = CardInstanceId;
	return FromZoneMutation(
		WBCardZoneMutation::ExtractExact(State, Request));
}

FWBCardLifecycleResult WBCardLifecycle::ApplySetupDraw(
	FWBGameStateData& State,
	const int32 PlayerId,
	const int32 Count)
{
	return DrawCards(State, PlayerId, Count);
}

FWBCardLifecycleResult WBCardLifecycle::ApplyTurnStartDraw(
	FWBGameStateData& State,
	const int32 ActivePlayerId,
	const int32 TurnNumber,
	const int32 FirstPlayerId)
{
	if (!FWBGameStateData::IsValidPlayerId(ActivePlayerId)
		|| State.GetPlayerById(ActivePlayerId) == nullptr)
	{
		return MakeResult(EWBCardLifecycleResultCode::InvalidPlayer, ActivePlayerId);
	}

	if (ActivePlayerId == FirstPlayerId && TurnNumber == 1)
	{
		return MakeResult(
			EWBCardLifecycleResultCode::FirstPlayerFirstTurnDrawSkipped,
			ActivePlayerId);
	}

	return DrawOneCard(State, ActivePlayerId);
}

FString WBCardLifecycle::ResultCodeToString(const EWBCardLifecycleResultCode Code)
{
	switch (Code)
	{
	case EWBCardLifecycleResultCode::Success:
		return TEXT("success");
	case EWBCardLifecycleResultCode::GameStateMissing:
		return TEXT("game_state_missing");
	case EWBCardLifecycleResultCode::InvalidPlayer:
		return TEXT("invalid_player");
	case EWBCardLifecycleResultCode::PlayerZonesMissing:
		return TEXT("player_zones_missing");
	case EWBCardLifecycleResultCode::DeckEmpty:
		return TEXT("deck_empty");
	case EWBCardLifecycleResultCode::CardInstanceMissing:
		return TEXT("card_instance_missing");
	case EWBCardLifecycleResultCode::CardNotInExpectedZone:
		return TEXT("card_not_in_expected_zone");
	case EWBCardLifecycleResultCode::DuplicateInstanceId:
		return TEXT("duplicate_instance_id");
	case EWBCardLifecycleResultCode::InvalidZoneState:
		return TEXT("invalid_zone_state");
	case EWBCardLifecycleResultCode::UnsupportedLifecycleOperation:
		return TEXT("unsupported_lifecycle_operation");
	case EWBCardLifecycleResultCode::FirstPlayerFirstTurnDrawSkipped:
		return TEXT("first_player_first_turn_draw_skipped");
	default:
		return TEXT("unsupported_lifecycle_operation");
	}
}
