#include "WBCardZoneObservation.h"

#include "WBGameStateData.h"

namespace
{
TArray<FWBPlayerCardZoneState> GetSortedPlayerZones(const FWBCardZoneState& ZoneState)
{
	TArray<FWBPlayerCardZoneState> SortedPlayerZones = ZoneState.PlayerZones;
	SortedPlayerZones.Sort([](const FWBPlayerCardZoneState& A, const FWBPlayerCardZoneState& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	return SortedPlayerZones;
}

TArray<FWBZoneCardEntry> GetSortedZoneEntries(const TArray<FWBZoneCardEntry>& Entries)
{
	TArray<FWBZoneCardEntry> SortedEntries = Entries;
	SortedEntries.Sort([](const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
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
	return SortedEntries;
}

FWBObservedCardRef MakeObservedCardRef(const FWBCardInstanceRef& Card)
{
	FWBObservedCardRef ObservedCard;
	ObservedCard.InstanceId = Card.InstanceId;
	ObservedCard.CardId = Card.CardId;
	ObservedCard.OwnerPlayerId = Card.OwnerPlayerId;
	return ObservedCard;
}

FWBObservedZoneSummary MakeCountOnlyZoneSummary(
	const EWBCardZone Zone,
	const int32 OwnerPlayerId,
	const int32 Count)
{
	FWBObservedZoneSummary Summary;
	Summary.Zone = Zone;
	Summary.Visibility = EWBZoneObservationVisibility::CountOnly;
	Summary.OwnerPlayerId = OwnerPlayerId;
	Summary.Count = Count;
	return Summary;
}

FWBObservedZoneSummary MakeOwnPrivateZoneSummary(
	const EWBCardZone Zone,
	const int32 OwnerPlayerId,
	const TArray<FWBZoneCardEntry>& Entries)
{
	FWBObservedZoneSummary Summary;
	Summary.Zone = Zone;
	Summary.Visibility = EWBZoneObservationVisibility::OwnPrivate;
	Summary.OwnerPlayerId = OwnerPlayerId;
	Summary.Count = Entries.Num();
	for (const FWBZoneCardEntry& Entry : GetSortedZoneEntries(Entries))
	{
		Summary.Cards.Add(MakeObservedCardRef(Entry.Card));
	}
	return Summary;
}

FWBObservedZoneSummary MakeHiddenZoneSummary(
	const EWBCardZone Zone,
	const int32 OwnerPlayerId)
{
	FWBObservedZoneSummary Summary;
	Summary.Zone = Zone;
	Summary.Visibility = EWBZoneObservationVisibility::Hidden;
	Summary.OwnerPlayerId = OwnerPlayerId;
	Summary.Count = 0;
	return Summary;
}

TArray<FWBObservedMarkerSummary> BuildMarkerSummaries(const FWBCardZoneState& ZoneState)
{
	TArray<FWBMarkerPlaceholderEntry> SortedMarkers = ZoneState.MarkerPlaceholders;
	SortedMarkers.Sort([](const FWBMarkerPlaceholderEntry& A, const FWBMarkerPlaceholderEntry& B)
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

	TArray<FWBObservedMarkerSummary> Summaries;
	Summaries.Reserve(SortedMarkers.Num());
	for (const FWBMarkerPlaceholderEntry& Marker : SortedMarkers)
	{
		FWBObservedMarkerSummary Summary;
		Summary.MarkerId = Marker.MarkerId;
		Summary.OwnerPlayerId = Marker.OwnerPlayerId;
		Summary.Tile = Marker.Tile;
		Summary.PublicState = Marker.PublicState;
		Summaries.Add(Summary);
	}
	return Summaries;
}

void AppendZoneSummaryForScan(FString& OutText, const FWBObservedZoneSummary& Summary)
{
	OutText += FString::Printf(
		TEXT("zone=%s|visibility=%s|owner=%d|count=%d;"),
		*WBCardZoneState::ZoneToString(Summary.Zone),
		*WBCardZoneObservation::VisibilityToString(Summary.Visibility),
		Summary.OwnerPlayerId,
		Summary.Count);

	for (const FWBObservedCardRef& Card : Summary.Cards)
	{
		OutText += FString::Printf(
			TEXT("card=%s|instance=%s|owner=%d;"),
			*Card.CardId,
			*Card.InstanceId,
			Card.OwnerPlayerId);
	}
}

FString BuildPublicSummaryScanText(const FWBCardZonePublicSummary& Summary)
{
	FString Text;
	for (const FWBObservedZoneSummary& Deck : Summary.PlayerDecks)
	{
		AppendZoneSummaryForScan(Text, Deck);
	}
	for (const FWBObservedZoneSummary& Hand : Summary.PlayerHands)
	{
		AppendZoneSummaryForScan(Text, Hand);
	}
	for (const FWBObservedZoneSummary& Discard : Summary.PlayerDiscards)
	{
		AppendZoneSummaryForScan(Text, Discard);
	}

	Text += FString::Printf(
		TEXT("equipped_visibility=%s|equipped_count=%d;"),
		*WBCardZoneObservation::VisibilityToString(Summary.Equipped.Visibility),
		Summary.Equipped.Count);
	for (const FWBEquippedCardEntry& Equipped : Summary.Equipped.OwnVisibleEquippedCards)
	{
		Text += FString::Printf(
			TEXT("equipped=%s|instance=%s|owner=%d|unit=%d|slot=%s;"),
			*Equipped.Card.CardId,
			*Equipped.Card.InstanceId,
			Equipped.Card.OwnerPlayerId,
			Equipped.EquippedToUnitId,
			*Equipped.SlotId);
	}

	for (const FWBObservedMarkerSummary& Marker : Summary.Markers)
	{
		Text += FString::Printf(
			TEXT("marker=%d|owner=%d|tile=%s|state=%d;"),
			Marker.MarkerId,
			Marker.OwnerPlayerId,
			*Marker.Tile.ToString(),
			static_cast<int32>(Marker.PublicState));
	}

	for (const FWBBoardCardReference& Reference : Summary.BoardCardReferences)
	{
		Text += FString::Printf(
			TEXT("board_unit=%d|owner=%d|card=%s;"),
			Reference.UnitId,
			Reference.OwnerPlayerId,
			*Reference.CardId);
	}

	return Text;
}
}

FWBCardZonePublicSummary WBCardZoneObservation::BuildPublicSummary(const FWBGameStateData& State)
{
	const FWBCardZoneState& ZoneState = State.GetCardZoneState();
	FWBCardZonePublicSummary Summary;

	const TArray<FWBPlayerCardZoneState> SortedPlayerZones = GetSortedPlayerZones(ZoneState);
	for (const FWBPlayerCardZoneState& PlayerZones : SortedPlayerZones)
	{
		Summary.PlayerDecks.Add(MakeCountOnlyZoneSummary(EWBCardZone::Deck, PlayerZones.PlayerId, PlayerZones.Deck.Num()));
		Summary.PlayerHands.Add(MakeCountOnlyZoneSummary(EWBCardZone::Hand, PlayerZones.PlayerId, PlayerZones.Hand.Num()));
		Summary.PlayerDiscards.Add(MakeCountOnlyZoneSummary(EWBCardZone::Discard, PlayerZones.PlayerId, PlayerZones.Discard.Num()));
	}

	Summary.Equipped.Visibility = EWBZoneObservationVisibility::CountOnly;
	Summary.Equipped.Count = ZoneState.EquippedCards.Num();
	Summary.Equipped.OwnVisibleEquippedCards.Reset();
	Summary.Markers = BuildMarkerSummaries(ZoneState);
	Summary.BoardCardReferences = WBCardZoneState::BuildBoardCardReferencesForTest(State);

	return Summary;
}

FWBCardZonePlayerObservation WBCardZoneObservation::BuildObservationForPlayer(
	const FWBGameStateData& State,
	const int32 ViewerPlayerId)
{
	FWBCardZonePlayerObservation Observation;
	Observation.ViewerPlayerId = ViewerPlayerId;
	Observation.PublicSummary = BuildPublicSummary(State);

	if (!IsValidViewerPlayerId(ViewerPlayerId))
	{
		Observation.OwnDeck = MakeHiddenZoneSummary(EWBCardZone::Deck, ViewerPlayerId);
		Observation.OwnHand = MakeHiddenZoneSummary(EWBCardZone::Hand, ViewerPlayerId);
		Observation.OwnDiscard = MakeHiddenZoneSummary(EWBCardZone::Discard, ViewerPlayerId);
		return Observation;
	}

	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), ViewerPlayerId);
	if (PlayerZones == nullptr)
	{
		Observation.OwnDeck = MakeCountOnlyZoneSummary(EWBCardZone::Deck, ViewerPlayerId, 0);
		Observation.OwnHand = MakeOwnPrivateZoneSummary(EWBCardZone::Hand, ViewerPlayerId, TArray<FWBZoneCardEntry>());
		Observation.OwnDiscard = MakeOwnPrivateZoneSummary(EWBCardZone::Discard, ViewerPlayerId, TArray<FWBZoneCardEntry>());
		return Observation;
	}

	Observation.OwnDeck = MakeCountOnlyZoneSummary(EWBCardZone::Deck, ViewerPlayerId, PlayerZones->Deck.Num());
	Observation.OwnHand = MakeOwnPrivateZoneSummary(EWBCardZone::Hand, ViewerPlayerId, PlayerZones->Hand);
	Observation.OwnDiscard = MakeOwnPrivateZoneSummary(EWBCardZone::Discard, ViewerPlayerId, PlayerZones->Discard);
	return Observation;
}

bool WBCardZoneObservation::IsValidViewerPlayerId(const int32 ViewerPlayerId)
{
	return FWBGameStateData::IsValidPlayerId(ViewerPlayerId);
}

FString WBCardZoneObservation::VisibilityToString(const EWBZoneObservationVisibility Visibility)
{
	switch (Visibility)
	{
	case EWBZoneObservationVisibility::Hidden:
		return TEXT("Hidden");
	case EWBZoneObservationVisibility::CountOnly:
		return TEXT("CountOnly");
	case EWBZoneObservationVisibility::OwnPrivate:
		return TEXT("OwnPrivate");
	case EWBZoneObservationVisibility::Public:
		return TEXT("Public");
	case EWBZoneObservationVisibility::Unknown:
	default:
		return TEXT("Unknown");
	}
}

bool WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(
	const FWBCardZonePublicSummary& Summary,
	const FString& ForbiddenSubstring)
{
	return !ForbiddenSubstring.IsEmpty() && BuildPublicSummaryScanText(Summary).Contains(ForbiddenSubstring);
}

bool WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
	const FWBCardZonePlayerObservation& Observation,
	const FString& ForbiddenSubstring)
{
	if (ForbiddenSubstring.IsEmpty())
	{
		return false;
	}

	FString Text = BuildPublicSummaryScanText(Observation.PublicSummary);
	AppendZoneSummaryForScan(Text, Observation.OwnDeck);
	AppendZoneSummaryForScan(Text, Observation.OwnHand);
	AppendZoneSummaryForScan(Text, Observation.OwnDiscard);
	return Text.Contains(ForbiddenSubstring);
}
