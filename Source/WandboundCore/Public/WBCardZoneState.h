#pragma once

#include "CoreMinimal.h"
#include "WBTypes.h"

struct FWBGameStateData;

enum class EWBCardZone : uint8
{
	Unknown,
	Deck,
	Hand,
	Discard,
	Equipped,
	Board,
	Marker
};

struct WANDBOUNDCORE_API FWBCardInstanceRef
{
	FString InstanceId;
	FString CardId;
	int32 OwnerPlayerId = -1;
};

struct WANDBOUNDCORE_API FWBZoneCardEntry
{
	FWBCardInstanceRef Card;
	EWBCardZone Zone = EWBCardZone::Unknown;
	int32 ZoneIndex = INDEX_NONE;
};

struct WANDBOUNDCORE_API FWBEquippedCardEntry
{
	FWBCardInstanceRef Card;
	int32 EquippedToUnitId = -1;
	FString SlotId;
	int32 EquipOrder = INDEX_NONE;
};

struct WANDBOUNDCORE_API FWBBoardCardReference
{
	int32 UnitId = -1;
	FString CardId;
	int32 OwnerPlayerId = -1;
};

enum class EWBMarkerPublicState : uint8
{
	Unknown,
	Hidden,
	Revealed
};

enum class EWBMarkerType : uint8
{
	Unknown,
	Trap,
	NPC
};

struct WANDBOUNDCORE_API FWBMarkerPlaceholderEntry
{
	int32 MarkerId = -1;
	int32 OwnerPlayerId = -1;
	EWBMarkerType Type = EWBMarkerType::Unknown;
	FWBTile Tile;
	int32 PlacementOrder = INDEX_NONE;
	EWBMarkerPublicState PublicState = EWBMarkerPublicState::Hidden;

	// Authoritative hidden identity. Player and public observations must not expose this.
	FString InternalMarkerCardId;
};

struct WANDBOUNDCORE_API FWBPlayerCardZoneState
{
	int32 PlayerId = -1;
	TArray<FWBZoneCardEntry> Deck;
	TArray<FWBZoneCardEntry> Hand;
	TArray<FWBZoneCardEntry> Discard;
};

struct WANDBOUNDCORE_API FWBCardZoneState
{
	TArray<FWBPlayerCardZoneState> PlayerZones;
	TArray<FWBEquippedCardEntry> EquippedCards;
	TArray<FWBMarkerPlaceholderEntry> MarkerPlaceholders;
};

class WANDBOUNDCORE_API WBCardZoneState
{
public:
	static FString ZoneToString(EWBCardZone Zone);
	static EWBCardZone ZoneFromString(const FString& ZoneString);
	static bool IsPrivateZone(EWBCardZone Zone);
	static bool IsOrderedZone(EWBCardZone Zone);

	static const FWBPlayerCardZoneState* FindPlayerZones(
		const FWBCardZoneState& ZoneState,
		int32 PlayerId);

	static FWBPlayerCardZoneState* FindMutablePlayerZones(
		FWBCardZoneState& ZoneState,
		int32 PlayerId);

	static int32 CountCardsInZoneForPlayer(
		const FWBCardZoneState& ZoneState,
		int32 PlayerId,
		EWBCardZone Zone);

	static bool FindCardByInstanceId(
		const FWBCardZoneState& ZoneState,
		const FString& InstanceId,
		FWBZoneCardEntry& OutEntry);

	static bool HasDuplicateCardInstanceIds(
		const FWBCardZoneState& ZoneState,
		FString& OutDuplicateInstanceId);

	static void SortOrderedZonesDeterministically(FWBCardZoneState& ZoneState);

	static bool ValidateZoneStateForTest(
		const FWBCardZoneState& ZoneState,
		FString& OutReason);

	static TArray<FWBBoardCardReference> BuildBoardCardReferencesForTest(
		const FWBGameStateData& State);

	static bool FindMarkerAtTileForTest(
		const FWBCardZoneState& ZoneState,
		const FWBTile& Tile,
		FWBMarkerPlaceholderEntry& OutMarker);
};
