#include "WBCardZoneState.h"

#include "WBGameStateData.h"
#include "WBRules.h"

namespace
{
bool IsValidPlayerOwner(const int32 PlayerId)
{
	return FWBGameStateData::IsValidPlayerId(PlayerId);
}

bool IsKnownMarkerPublicState(const EWBMarkerPublicState PublicState)
{
	switch (PublicState)
	{
	case EWBMarkerPublicState::Unknown:
	case EWBMarkerPublicState::Hidden:
	case EWBMarkerPublicState::Revealed:
		return true;
	default:
		return false;
	}
}

const TArray<FWBZoneCardEntry>* GetOrderedZoneEntries(
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

TArray<FWBZoneCardEntry>* GetMutableOrderedZoneEntries(
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

void SortZoneEntries(TArray<FWBZoneCardEntry>& Entries)
{
	Entries.Sort([](const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
	{
		if (A.ZoneIndex != B.ZoneIndex)
		{
			return A.ZoneIndex < B.ZoneIndex;
		}

		if (A.Card.InstanceId != B.Card.InstanceId)
		{
			return A.Card.InstanceId < B.Card.InstanceId;
		}

		return A.Card.CardId < B.Card.CardId;
	});
}

bool TrackInstanceId(
	TSet<FString>& SeenInstanceIds,
	const FString& InstanceId,
	FString& OutDuplicateInstanceId)
{
	if (InstanceId.IsEmpty())
	{
		return true;
	}

	if (SeenInstanceIds.Contains(InstanceId))
	{
		OutDuplicateInstanceId = InstanceId;
		return false;
	}

	SeenInstanceIds.Add(InstanceId);
	return true;
}

bool ValidateCardRef(
	const FWBCardInstanceRef& Card,
	const int32 ExpectedOwnerPlayerId,
	const TCHAR* Context,
	FString& OutReason)
{
	if (Card.CardId.IsEmpty())
	{
		OutReason = FString::Printf(TEXT("%s_card_id_empty"), Context);
		return false;
	}

	if (!IsValidPlayerOwner(Card.OwnerPlayerId))
	{
		OutReason = FString::Printf(TEXT("%s_owner_invalid"), Context);
		return false;
	}

	if (ExpectedOwnerPlayerId != -1 && Card.OwnerPlayerId != ExpectedOwnerPlayerId)
	{
		OutReason = FString::Printf(TEXT("%s_owner_mismatch"), Context);
		return false;
	}

	return true;
}

bool ValidateOrderedEntries(
	const TArray<FWBZoneCardEntry>& Entries,
	const int32 ExpectedOwnerPlayerId,
	const EWBCardZone ExpectedZone,
	const TCHAR* Context,
	TSet<FString>& SeenInstanceIds,
	FString& OutReason)
{
	for (const FWBZoneCardEntry& Entry : Entries)
	{
		if (Entry.Zone != ExpectedZone)
		{
			OutReason = FString::Printf(TEXT("%s_zone_mismatch"), Context);
			return false;
		}

		if (!ValidateCardRef(Entry.Card, ExpectedOwnerPlayerId, Context, OutReason))
		{
			return false;
		}

		FString DuplicateInstanceId;
		if (!TrackInstanceId(SeenInstanceIds, Entry.Card.InstanceId, DuplicateInstanceId))
		{
			OutReason = FString::Printf(TEXT("duplicate_instance_id:%s"), *DuplicateInstanceId);
			return false;
		}
	}

	return true;
}
}

FString WBCardZoneState::ZoneToString(const EWBCardZone Zone)
{
	switch (Zone)
	{
	case EWBCardZone::Deck:
		return TEXT("Deck");
	case EWBCardZone::Hand:
		return TEXT("Hand");
	case EWBCardZone::Discard:
		return TEXT("Discard");
	case EWBCardZone::Equipped:
		return TEXT("Equipped");
	case EWBCardZone::Board:
		return TEXT("Board");
	case EWBCardZone::Marker:
		return TEXT("Marker");
	case EWBCardZone::Unknown:
	default:
		return TEXT("Unknown");
	}
}

EWBCardZone WBCardZoneState::ZoneFromString(const FString& ZoneString)
{
	const FString NormalizedZone = ZoneString.TrimStartAndEnd().ToLower();
	if (NormalizedZone == TEXT("deck"))
	{
		return EWBCardZone::Deck;
	}

	if (NormalizedZone == TEXT("hand"))
	{
		return EWBCardZone::Hand;
	}

	if (NormalizedZone == TEXT("discard"))
	{
		return EWBCardZone::Discard;
	}

	if (NormalizedZone == TEXT("equipped"))
	{
		return EWBCardZone::Equipped;
	}

	if (NormalizedZone == TEXT("board"))
	{
		return EWBCardZone::Board;
	}

	if (NormalizedZone == TEXT("marker"))
	{
		return EWBCardZone::Marker;
	}

	return EWBCardZone::Unknown;
}

bool WBCardZoneState::IsPrivateZone(const EWBCardZone Zone)
{
	switch (Zone)
	{
	case EWBCardZone::Deck:
	case EWBCardZone::Hand:
	case EWBCardZone::Discard:
	case EWBCardZone::Equipped:
	case EWBCardZone::Marker:
		return true;
	case EWBCardZone::Board:
	case EWBCardZone::Unknown:
	default:
		return false;
	}
}

bool WBCardZoneState::IsOrderedZone(const EWBCardZone Zone)
{
	return Zone == EWBCardZone::Deck
		|| Zone == EWBCardZone::Hand
		|| Zone == EWBCardZone::Discard;
}

const FWBPlayerCardZoneState* WBCardZoneState::FindPlayerZones(
	const FWBCardZoneState& ZoneState,
	const int32 PlayerId)
{
	for (const FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		if (PlayerZones.PlayerId == PlayerId)
		{
			return &PlayerZones;
		}
	}

	return nullptr;
}

FWBPlayerCardZoneState* WBCardZoneState::FindMutablePlayerZones(
	FWBCardZoneState& ZoneState,
	const int32 PlayerId)
{
	for (FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		if (PlayerZones.PlayerId == PlayerId)
		{
			return &PlayerZones;
		}
	}

	return nullptr;
}

int32 WBCardZoneState::CountCardsInZoneForPlayer(
	const FWBCardZoneState& ZoneState,
	const int32 PlayerId,
	const EWBCardZone Zone)
{
	if (Zone == EWBCardZone::Equipped)
	{
		int32 Count = 0;
		for (const FWBEquippedCardEntry& Entry : ZoneState.EquippedCards)
		{
			if (Entry.Card.OwnerPlayerId == PlayerId)
			{
				++Count;
			}
		}
		return Count;
	}

	const FWBPlayerCardZoneState* PlayerZones = FindPlayerZones(ZoneState, PlayerId);
	if (PlayerZones == nullptr)
	{
		return 0;
	}

	const TArray<FWBZoneCardEntry>* Entries = GetOrderedZoneEntries(*PlayerZones, Zone);
	return Entries != nullptr ? Entries->Num() : 0;
}

bool WBCardZoneState::FindCardByInstanceId(
	const FWBCardZoneState& ZoneState,
	const FString& InstanceId,
	FWBZoneCardEntry& OutEntry)
{
	if (InstanceId.IsEmpty())
	{
		return false;
	}

	for (const FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		const TArray<const TArray<FWBZoneCardEntry>*> OrderedZones = {
			&PlayerZones.Deck,
			&PlayerZones.Hand,
			&PlayerZones.Discard
		};

		for (const TArray<FWBZoneCardEntry>* Entries : OrderedZones)
		{
			for (const FWBZoneCardEntry& Entry : *Entries)
			{
				if (Entry.Card.InstanceId == InstanceId)
				{
					OutEntry = Entry;
					return true;
				}
			}
		}
	}

	return false;
}

bool WBCardZoneState::HasDuplicateCardInstanceIds(
	const FWBCardZoneState& ZoneState,
	FString& OutDuplicateInstanceId)
{
	TSet<FString> SeenInstanceIds;
	for (const FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		const TArray<const TArray<FWBZoneCardEntry>*> OrderedZones = {
			&PlayerZones.Deck,
			&PlayerZones.Hand,
			&PlayerZones.Discard
		};

		for (const TArray<FWBZoneCardEntry>* Entries : OrderedZones)
		{
			for (const FWBZoneCardEntry& Entry : *Entries)
			{
				if (!TrackInstanceId(SeenInstanceIds, Entry.Card.InstanceId, OutDuplicateInstanceId))
				{
					return true;
				}
			}
		}
	}

	for (const FWBEquippedCardEntry& Entry : ZoneState.EquippedCards)
	{
		if (!TrackInstanceId(SeenInstanceIds, Entry.Card.InstanceId, OutDuplicateInstanceId))
		{
			return true;
		}
	}

	OutDuplicateInstanceId.Reset();
	return false;
}

void WBCardZoneState::SortOrderedZonesDeterministically(FWBCardZoneState& ZoneState)
{
	ZoneState.PlayerZones.Sort([](const FWBPlayerCardZoneState& A, const FWBPlayerCardZoneState& B)
	{
		return A.PlayerId < B.PlayerId;
	});

	for (FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		SortZoneEntries(PlayerZones.Deck);
		SortZoneEntries(PlayerZones.Hand);
		SortZoneEntries(PlayerZones.Discard);
	}

	ZoneState.EquippedCards.Sort([](const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
	{
		if (A.EquippedToUnitId != B.EquippedToUnitId)
		{
			return A.EquippedToUnitId < B.EquippedToUnitId;
		}

		if (A.SlotId != B.SlotId)
		{
			return A.SlotId < B.SlotId;
		}

		return A.Card.InstanceId < B.Card.InstanceId;
	});

	ZoneState.MarkerPlaceholders.Sort([](const FWBMarkerPlaceholderEntry& A, const FWBMarkerPlaceholderEntry& B)
	{
		if (A.Tile.Y != B.Tile.Y)
		{
			return A.Tile.Y < B.Tile.Y;
		}

		if (A.Tile.X != B.Tile.X)
		{
			return A.Tile.X < B.Tile.X;
		}

		return A.MarkerId < B.MarkerId;
	});
}

bool WBCardZoneState::ValidateZoneStateForTest(
	const FWBCardZoneState& ZoneState,
	FString& OutReason)
{
	TSet<int32> SeenPlayerIds;
	TSet<FString> SeenInstanceIds;
	for (const FWBPlayerCardZoneState& PlayerZones : ZoneState.PlayerZones)
	{
		if (!IsValidPlayerOwner(PlayerZones.PlayerId))
		{
			OutReason = TEXT("player_id_invalid");
			return false;
		}

		if (SeenPlayerIds.Contains(PlayerZones.PlayerId))
		{
			OutReason = FString::Printf(TEXT("duplicate_player_zones:%d"), PlayerZones.PlayerId);
			return false;
		}
		SeenPlayerIds.Add(PlayerZones.PlayerId);

		if (!ValidateOrderedEntries(PlayerZones.Deck, PlayerZones.PlayerId, EWBCardZone::Deck, TEXT("deck"), SeenInstanceIds, OutReason))
		{
			return false;
		}

		if (!ValidateOrderedEntries(PlayerZones.Hand, PlayerZones.PlayerId, EWBCardZone::Hand, TEXT("hand"), SeenInstanceIds, OutReason))
		{
			return false;
		}

		if (!ValidateOrderedEntries(PlayerZones.Discard, PlayerZones.PlayerId, EWBCardZone::Discard, TEXT("discard"), SeenInstanceIds, OutReason))
		{
			return false;
		}
	}

	for (const FWBEquippedCardEntry& Entry : ZoneState.EquippedCards)
	{
		if (!ValidateCardRef(Entry.Card, -1, TEXT("equipped"), OutReason))
		{
			return false;
		}

		if (Entry.EquippedToUnitId < 0)
		{
			OutReason = TEXT("equipped_unit_invalid");
			return false;
		}

		FString DuplicateInstanceId;
		if (!TrackInstanceId(SeenInstanceIds, Entry.Card.InstanceId, DuplicateInstanceId))
		{
			OutReason = FString::Printf(TEXT("duplicate_instance_id:%s"), *DuplicateInstanceId);
			return false;
		}
	}

	TSet<int32> SeenMarkerIds;
	for (int32 MarkerIndex = 0; MarkerIndex < ZoneState.MarkerPlaceholders.Num(); ++MarkerIndex)
	{
		const FWBMarkerPlaceholderEntry& Marker = ZoneState.MarkerPlaceholders[MarkerIndex];
		if (!IsValidPlayerOwner(Marker.OwnerPlayerId))
		{
			OutReason = TEXT("marker_owner_invalid");
			return false;
		}

		if (Marker.MarkerId >= 0)
		{
			if (SeenMarkerIds.Contains(Marker.MarkerId))
			{
				OutReason = FString::Printf(TEXT("duplicate_marker_id:%d"), Marker.MarkerId);
				return false;
			}
			SeenMarkerIds.Add(Marker.MarkerId);
		}

		if (!IsKnownMarkerPublicState(Marker.PublicState))
		{
			OutReason = TEXT("marker_public_state_invalid");
			return false;
		}

		if (!WBRules::IsTileInBounds(Marker.Tile))
		{
			OutReason = TEXT("marker_tile_out_of_bounds");
			return false;
		}

		for (int32 OtherIndex = MarkerIndex + 1; OtherIndex < ZoneState.MarkerPlaceholders.Num(); ++OtherIndex)
		{
			if (Marker.Tile == ZoneState.MarkerPlaceholders[OtherIndex].Tile)
			{
				OutReason = FString::Printf(TEXT("duplicate_marker_tile:%s"), *Marker.Tile.ToString());
				return false;
			}
		}
	}

	OutReason.Reset();
	return true;
}

TArray<FWBBoardCardReference> WBCardZoneState::BuildBoardCardReferencesForTest(
	const FWBGameStateData& State)
{
	TArray<FWBBoardCardReference> References;
	References.Reserve(State.Units.Num());
	for (const FWBUnitState& Unit : State.Units)
	{
		if (!Unit.IsUnitOnBoard())
		{
			continue;
		}

		FWBBoardCardReference Reference;
		Reference.UnitId = Unit.UnitId;
		Reference.CardId = Unit.CardId;
		Reference.OwnerPlayerId = Unit.OwnerId;
		References.Add(Reference);
	}

	References.Sort([&State](const FWBBoardCardReference& A, const FWBBoardCardReference& B)
	{
		const FWBUnitState* UnitA = State.GetUnitById(A.UnitId);
		const FWBUnitState* UnitB = State.GetUnitById(B.UnitId);
		const int32 AY = UnitA != nullptr ? UnitA->Y : MAX_int32;
		const int32 BY = UnitB != nullptr ? UnitB->Y : MAX_int32;
		if (AY != BY)
		{
			return AY < BY;
		}

		const int32 AX = UnitA != nullptr ? UnitA->X : MAX_int32;
		const int32 BX = UnitB != nullptr ? UnitB->X : MAX_int32;
		if (AX != BX)
		{
			return AX < BX;
		}

		return A.UnitId < B.UnitId;
	});

	return References;
}

bool WBCardZoneState::FindMarkerAtTileForTest(
	const FWBCardZoneState& ZoneState,
	const FWBTile& Tile,
	FWBMarkerPlaceholderEntry& OutMarker)
{
	for (const FWBMarkerPlaceholderEntry& Marker : ZoneState.MarkerPlaceholders)
	{
		if (Marker.Tile == Tile)
		{
			OutMarker = Marker;
			return true;
		}
	}

	return false;
}
