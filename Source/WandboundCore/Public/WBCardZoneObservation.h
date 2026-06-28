#pragma once

#include "CoreMinimal.h"
#include "WBCardZoneState.h"

struct FWBGameStateData;

enum class EWBZoneObservationVisibility : uint8
{
	Unknown,
	Hidden,
	CountOnly,
	OwnPrivate,
	Public
};

struct WANDBOUNDCORE_API FWBObservedCardRef
{
	FString InstanceId;
	FString CardId;
	int32 OwnerPlayerId = -1;
};

struct WANDBOUNDCORE_API FWBObservedZoneSummary
{
	EWBCardZone Zone = EWBCardZone::Unknown;
	EWBZoneObservationVisibility Visibility = EWBZoneObservationVisibility::Unknown;
	int32 OwnerPlayerId = -1;
	int32 Count = 0;
	TArray<FWBObservedCardRef> Cards;
};

struct WANDBOUNDCORE_API FWBObservedEquippedSummary
{
	EWBZoneObservationVisibility Visibility = EWBZoneObservationVisibility::Unknown;
	int32 Count = 0;
	TArray<FWBEquippedCardEntry> OwnVisibleEquippedCards;
};

struct WANDBOUNDCORE_API FWBObservedMarkerSummary
{
	int32 MarkerId = -1;
	int32 OwnerPlayerId = -1;
	FWBTile Tile;
	EWBMarkerPublicState PublicState = EWBMarkerPublicState::Hidden;
};

struct WANDBOUNDCORE_API FWBCardZonePublicSummary
{
	TArray<FWBObservedZoneSummary> PlayerDecks;
	TArray<FWBObservedZoneSummary> PlayerHands;
	TArray<FWBObservedZoneSummary> PlayerDiscards;
	FWBObservedEquippedSummary Equipped;
	TArray<FWBObservedMarkerSummary> Markers;
	TArray<FWBBoardCardReference> BoardCardReferences;
};

struct WANDBOUNDCORE_API FWBCardZonePlayerObservation
{
	int32 ViewerPlayerId = -1;
	FWBCardZonePublicSummary PublicSummary;
	FWBObservedZoneSummary OwnHand;
	FWBObservedZoneSummary OwnDiscard;
	FWBObservedZoneSummary OwnDeck;
};

class WANDBOUNDCORE_API WBCardZoneObservation
{
public:
	static FWBCardZonePublicSummary BuildPublicSummary(const FWBGameStateData& State);
	static FWBCardZonePlayerObservation BuildObservationForPlayer(const FWBGameStateData& State, int32 ViewerPlayerId);
	static bool IsValidViewerPlayerId(int32 ViewerPlayerId);
	static FString VisibilityToString(EWBZoneObservationVisibility Visibility);

	static bool PublicSummaryContainsForbiddenSubstringForTest(
		const FWBCardZonePublicSummary& Summary,
		const FString& ForbiddenSubstring);

	static bool PlayerObservationContainsForbiddenSubstringForTest(
		const FWBCardZonePlayerObservation& Observation,
		const FString& ForbiddenSubstring);
};
