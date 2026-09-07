#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneMutation.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneTransition.h"
#include "WBProductionMatchReplay.h"
#include "WBPublicBoardSummary.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WBCardZoneTransitionEventTestsPrivate
{
FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 7;
	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		FWBPlayerStateData Player;
		Player.PlayerId = PlayerId;
		State.Players.Add(Player);
		FWBPlayerCardZoneState Zones;
		Zones.PlayerId = PlayerId;
		State.GetMutableCardZoneStateForTest().PlayerZones.Add(Zones);
	}
	return State;
}

TArray<FWBZoneCardEntry>* GetMutableEntries(
	FWBGameStateData& State,
	const int32 PlayerId,
	const EWBCardZone Zone)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), PlayerId);
	if (Zones == nullptr)
	{
		return nullptr;
	}
	if (Zone == EWBCardZone::Deck)
	{
		return &Zones->Deck;
	}
	if (Zone == EWBCardZone::Hand)
	{
		return &Zones->Hand;
	}
	if (Zone == EWBCardZone::Discard)
	{
		return &Zones->Discard;
	}
	return nullptr;
}

const TArray<FWBZoneCardEntry>* GetEntries(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const EWBCardZone Zone)
{
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), PlayerId);
	if (Zones == nullptr)
	{
		return nullptr;
	}
	if (Zone == EWBCardZone::Deck)
	{
		return &Zones->Deck;
	}
	if (Zone == EWBCardZone::Hand)
	{
		return &Zones->Hand;
	}
	if (Zone == EWBCardZone::Discard)
	{
		return &Zones->Discard;
	}
	return nullptr;
}

void AddCard(
	FWBGameStateData& State,
	const EWBCardZone Zone,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex,
	const int32 PlayerId = 0)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = PlayerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	GetMutableEntries(State, PlayerId, Zone)->Add(MoveTemp(Entry));
}

FWBCardZoneTransferRequest MakeTransfer(
	const EWBCardZone Source,
	const EWBCardZone Destination,
	const FString& InstanceId)
{
	FWBCardZoneTransferRequest Request;
	Request.PlayerId = 0;
	Request.SourceZone = Source;
	Request.DestinationZone = Destination;
	Request.CardInstanceId = InstanceId;
	Request.DestinationPlacement = EWBOrderedZonePlacement::Append;
	return Request;
}

FWBCardZoneTransitionContext MakeContext(
	const EWBCardZoneTransitionCause Cause,
	const FString& ActionId,
	const int32 Order = 0)
{
	FWBCardZoneTransitionContext Context;
	Context.Cause = Cause;
	Context.SourceActionId = ActionId;
	Context.ContinuationId = TEXT("continuation:zone_transition");
	Context.ActionDeclaration =
		EWBDeclarationProvenance::PlayerDeclared;
	Context.ResolutionOrder = Order;
	return Context;
}

bool LoadSource(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(
		OutSource,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

#define WB_ZONE_TRANSITION_TEST(ClassName, Path) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, Path, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

using namespace WBCardZoneTransitionEventTestsPrivate;

WB_ZONE_TRANSITION_TEST(
	FWBCardZoneTransitionSnapshotTest,
	"Wandbound.CardZoneTransitionEvent.Snapshot.ImmutableExactInstance")

bool FWBCardZoneTransitionSnapshotTest::RunTest(const FString&)
{
	FWBGameStateData First = MakeState();
	AddCard(First, EWBCardZone::Hand, TEXT("A1"), TEXT("card_x"), 0);
	AddCard(First, EWBCardZone::Hand, TEXT("A2"), TEXT("card_x"), 1);
	FWBGameStateData Second = First;
	const FWBCardZoneTransitionContext Context = MakeContext(
		EWBCardZoneTransitionCause::Rule,
		TEXT("discard:p0:iA2"),
		3);
	const FWBCardLifecycleResult Result =
		WBCardLifecycle::MoveHandCardToDiscard(
			First, 0, TEXT("A2"), Context);
	const FWBCardLifecycleResult Repeat =
		WBCardLifecycle::MoveHandCardToDiscard(
			Second, 0, TEXT("A2"), Context);

	TestTrue(TEXT("Successful Hand to Discard"), Result.bOk);
	TestEqual(TEXT("Exactly one transition"), Result.TransitionEvents.Num(), 1);
	const FWBCardZoneTransitionSnapshot& Snapshot =
		Result.TransitionEvents[0];
	TestTrue(TEXT("Snapshot valid"), Snapshot.IsValid());
	TestEqual(TEXT("Exact instance A2"), Snapshot.CardInstanceId,
		FString(TEXT("A2")));
	TestEqual(TEXT("CardId captured"), Snapshot.CardId,
		FString(TEXT("card_x")));
	TestEqual(TEXT("Owner captured"), Snapshot.OwnerPlayerId, 0);
	TestEqual(TEXT("Source captured"), Snapshot.SourceZone,
		EWBCardZone::Hand);
	TestEqual(TEXT("Destination captured"), Snapshot.DestinationZone,
		EWBCardZone::Discard);
	TestEqual(TEXT("Cause captured"), Snapshot.Cause,
		EWBCardZoneTransitionCause::Rule);
	TestEqual(TEXT("Resolution order captured"), Snapshot.ResolutionOrder, 3);
	TestEqual(TEXT("Turn captured"), Snapshot.EventIdentity.TurnNumber, 7);
	TestEqual(TEXT("Action identity captured"),
		Snapshot.EventIdentity.SourceActionId,
		FString(TEXT("discard:p0:iA2")));
	TestEqual(TEXT("Continuation captured"),
		Snapshot.EventIdentity.ContinuationId,
		FString(TEXT("continuation:zone_transition")));
	TestEqual(TEXT("Declaration provenance captured"),
		Snapshot.EventIdentity.ActionDeclaration,
		EWBDeclarationProvenance::PlayerDeclared);
	TestEqual(TEXT("Typed event kind"),
		Snapshot.EventIdentity.Kind,
		EWBEventKind::CardZoneTransition);
	TestEqual(TEXT("Kind appended after Activation"),
		static_cast<int32>(EWBEventKind::CardZoneTransition),
		static_cast<int32>(EWBEventKind::Activation) + 1);
	TestEqual(TEXT("Deterministic EventId"),
		Snapshot.EventIdentity.EventId,
		Repeat.TransitionEvents[0].EventIdentity.EventId);
	TestEqual(TEXT("Deterministic state digest"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));

	const TArray<FWBZoneCardEntry>* Hand =
		GetEntries(First, 0, EWBCardZone::Hand);
	const TArray<FWBZoneCardEntry>* Discard =
		GetEntries(First, 0, EWBCardZone::Discard);
	TestEqual(TEXT("A1 remains in Hand"), Hand->Num(), 1);
	TestEqual(TEXT("A1 exact instance untouched"),
		(*Hand)[0].Card.InstanceId, FString(TEXT("A1")));
	TestEqual(TEXT("A2 enters Discard"), Discard->Num(), 1);
	TestEqual(TEXT("A2 exact destination"),
		(*Discard)[0].Card.InstanceId, FString(TEXT("A2")));

	const FString HistoricalEventId = Snapshot.EventIdentity.EventId;
	const EWBCardZone HistoricalSource = Snapshot.SourceZone;
	const EWBCardZone HistoricalDestination = Snapshot.DestinationZone;
	const FString HistoricalInstance = Snapshot.CardInstanceId;
	const FWBCardLifecycleResult Return =
		WBCardLifecycle::TransferExactCard(
			First,
			MakeTransfer(
				EWBCardZone::Discard,
				EWBCardZone::Hand,
				TEXT("A2")),
			MakeContext(
				EWBCardZoneTransitionCause::Effect,
				TEXT("return:p0:iA2"),
				4));
	TestTrue(TEXT("Later return succeeds"), Return.bOk);
	TestEqual(TEXT("Historical EventId immutable"),
		Snapshot.EventIdentity.EventId, HistoricalEventId);
	TestEqual(TEXT("Historical source immutable"),
		Snapshot.SourceZone, HistoricalSource);
	TestEqual(TEXT("Historical destination immutable"),
		Snapshot.DestinationZone, HistoricalDestination);
	TestEqual(TEXT("Historical instance immutable"),
		Snapshot.CardInstanceId, HistoricalInstance);
	return true;
}

WB_ZONE_TRANSITION_TEST(
	FWBCardZoneTransitionSupportedDirectionsTest,
	"Wandbound.CardZoneTransitionEvent.Transfer.AllOrderedPrivateDirections")

bool FWBCardZoneTransitionSupportedDirectionsTest::RunTest(const FString&)
{
	struct FCase
	{
		EWBCardZone Source;
		EWBCardZone Destination;
		const TCHAR* Name;
	};
	const FCase Cases[] = {
		{ EWBCardZone::Deck, EWBCardZone::Hand, TEXT("DeckToHand") },
		{ EWBCardZone::Deck, EWBCardZone::Discard, TEXT("DeckToDiscard") },
		{ EWBCardZone::Hand, EWBCardZone::Deck, TEXT("HandToDeck") },
		{ EWBCardZone::Hand, EWBCardZone::Discard, TEXT("HandToDiscard") },
		{ EWBCardZone::Discard, EWBCardZone::Deck, TEXT("DiscardToDeck") },
		{ EWBCardZone::Discard, EWBCardZone::Hand, TEXT("DiscardToHand") }
	};

	for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
	{
		const FCase& Case = Cases[CaseIndex];
		FWBGameStateData State = MakeState();
		AddCard(State, Case.Source, TEXT("source_other"),
			TEXT("other_card"), 4);
		AddCard(State, Case.Source, TEXT("selected"),
			TEXT("selected_card"), 9);
		AddCard(State, Case.Destination, TEXT("destination_existing"),
			TEXT("existing_card"), 6);
		const FWBCardLifecycleResult Result =
			WBCardLifecycle::TransferExactCard(
				State,
				MakeTransfer(Case.Source, Case.Destination, TEXT("selected")),
				MakeContext(
					EWBCardZoneTransitionCause::Effect,
					FString::Printf(TEXT("effect:%s"), Case.Name),
					CaseIndex));
		const FString Prefix(Case.Name);
		TestTrue(*FString::Printf(TEXT("%s succeeds"), Case.Name), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s transition count"), Case.Name),
			Result.TransitionEvents.Num(), 1);
		const FWBCardZoneTransitionSnapshot& Event =
			Result.TransitionEvents[0];
		TestTrue(*FString::Printf(TEXT("%s event valid"), Case.Name),
			Event.IsValid());
		TestEqual(*FString::Printf(TEXT("%s source"), Case.Name),
			Event.SourceZone, Case.Source);
		TestEqual(*FString::Printf(TEXT("%s destination"), Case.Name),
			Event.DestinationZone, Case.Destination);
		TestEqual(*FString::Printf(TEXT("%s exact instance"), Case.Name),
			Event.CardInstanceId, FString(TEXT("selected")));
		TestEqual(*FString::Printf(TEXT("%s CardId"), Case.Name),
			Event.CardId, FString(TEXT("selected_card")));
		TestEqual(*FString::Printf(TEXT("%s owner"), Case.Name),
			Event.OwnerPlayerId, 0);
		TestEqual(*FString::Printf(TEXT("%s cause"), Case.Name),
			Event.Cause, EWBCardZoneTransitionCause::Effect);
		TestEqual(*FString::Printf(TEXT("%s order"), Case.Name),
			Event.ResolutionOrder, CaseIndex);
		const TArray<FWBZoneCardEntry>* Source =
			GetEntries(State, 0, Case.Source);
		const TArray<FWBZoneCardEntry>* Destination =
			GetEntries(State, 0, Case.Destination);
		TestEqual(*FString::Printf(TEXT("%s source count"), Case.Name),
			Source->Num(), 1);
		TestEqual(*FString::Printf(TEXT("%s source preserved"), Case.Name),
			(*Source)[0].Card.InstanceId, FString(TEXT("source_other")));
		TestEqual(*FString::Printf(TEXT("%s source normalized"), Case.Name),
			(*Source)[0].ZoneIndex, 0);
		TestEqual(*FString::Printf(TEXT("%s destination count"), Case.Name),
			Destination->Num(), 2);
		TestEqual(*FString::Printf(TEXT("%s destination preserved"), Case.Name),
			(*Destination)[0].Card.InstanceId,
			FString(TEXT("destination_existing")));
		TestEqual(*FString::Printf(TEXT("%s append exact"), Case.Name),
			Destination->Last().Card.InstanceId, FString(TEXT("selected")));
		TestEqual(*FString::Printf(TEXT("%s append normalized"), Case.Name),
			Destination->Last().ZoneIndex, 1);

		const FWBTraceEvent PublicTrace =
			WBCardZoneTransition::MakeRedactedTrace(Event);
		const FString Serialized = WBReplayTrace::SerializeEvent(PublicTrace);
		TestEqual(*FString::Printf(TEXT("%s public kind"), Case.Name),
			PublicTrace.Kind, FName(TEXT("card_zone_transition")));
		TestTrue(*FString::Printf(TEXT("%s public source zone"), Case.Name),
			Serialized.Contains(WBCardZoneState::ZoneToString(Case.Source)));
		TestTrue(*FString::Printf(TEXT("%s public destination zone"), Case.Name),
			Serialized.Contains(WBCardZoneState::ZoneToString(Case.Destination)));
		TestFalse(*FString::Printf(TEXT("%s private instance redacted"), Case.Name),
			Serialized.Contains(TEXT("selected")));
		TestFalse(*FString::Printf(TEXT("%s private CardId redacted"), Case.Name),
			Serialized.Contains(TEXT("selected_card")));
		TestFalse(*FString::Printf(TEXT("%s no zone index"), Case.Name),
			Serialized.Contains(TEXT("zone_index")));
		TestFalse(*FString::Printf(TEXT("%s no reaction"), Case.Name),
			State.HasOpenReactionWindow());
	}
	return true;
}

WB_ZONE_TRANSITION_TEST(
	FWBCardZoneTransitionDrawOrderingTest,
	"Wandbound.CardZoneTransitionEvent.Draw.SequentialAndPartialCommit")

bool FWBCardZoneTransitionDrawOrderingTest::RunTest(const FString&)
{
	FWBGameStateData Single = MakeState();
	AddCard(Single, EWBCardZone::Deck, TEXT("draw_1"), TEXT("card_1"), 3);
	AddCard(Single, EWBCardZone::Deck, TEXT("draw_2"), TEXT("card_2"), 8);
	const FWBCardLifecycleResult DrawOne =
		WBCardLifecycle::DrawOneCard(Single, 0);
	TestTrue(TEXT("DrawOne succeeds"), DrawOne.bOk);
	TestEqual(TEXT("DrawOne one event"), DrawOne.TransitionEvents.Num(), 1);
	TestEqual(TEXT("DrawOne source Deck"),
		DrawOne.TransitionEvents[0].SourceZone, EWBCardZone::Deck);
	TestEqual(TEXT("DrawOne destination Hand"),
		DrawOne.TransitionEvents[0].DestinationZone, EWBCardZone::Hand);
	TestEqual(TEXT("DrawOne cause Draw"),
		DrawOne.TransitionEvents[0].Cause,
		EWBCardZoneTransitionCause::Draw);
	TestEqual(TEXT("Top ordered card drawn"),
		DrawOne.TransitionEvents[0].CardInstanceId,
		FString(TEXT("draw_1")));
	TestEqual(TEXT("Remaining Deck order preserved"),
		GetEntries(Single, 0, EWBCardZone::Deck)->Last().Card.InstanceId,
		FString(TEXT("draw_2")));
	TestEqual(TEXT("Hand append preserved"),
		GetEntries(Single, 0, EWBCardZone::Hand)->Last().Card.InstanceId,
		FString(TEXT("draw_1")));

	FWBGameStateData Three = MakeState();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		AddCard(
			Three,
			EWBCardZone::Deck,
			FString::Printf(TEXT("multi_%d"), Index),
			FString::Printf(TEXT("card_%d"), Index),
			Index);
	}
	const FWBCardLifecycleResult DrawThree =
		WBCardLifecycle::DrawCards(Three, 0, 3);
	TestTrue(TEXT("DrawCards succeeds"), DrawThree.bOk);
	TestEqual(TEXT("Three transition events"),
		DrawThree.TransitionEvents.Num(), 3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Draw event %d identity"), Index),
			DrawThree.TransitionEvents[Index].CardInstanceId,
			FString::Printf(TEXT("multi_%d"), Index));
		TestEqual(*FString::Printf(TEXT("Draw event %d order"), Index),
			DrawThree.TransitionEvents[Index].ResolutionOrder, Index);
		TestEqual(*FString::Printf(TEXT("Hand %d order"), Index),
			(*GetEntries(Three, 0, EWBCardZone::Hand))[Index].Card.InstanceId,
			FString::Printf(TEXT("multi_%d"), Index));
	}

	FWBGameStateData Partial = MakeState();
	AddCard(Partial, EWBCardZone::Deck, TEXT("partial_0"),
		TEXT("partial_card_0"), 0);
	AddCard(Partial, EWBCardZone::Deck, TEXT("partial_1"),
		TEXT("partial_card_1"), 1);
	const FWBCardLifecycleResult PartialDraw =
		WBCardLifecycle::DrawCards(Partial, 0, 3);
	TestFalse(TEXT("Third draw fails"), PartialDraw.bOk);
	TestEqual(TEXT("Third draw reports DeckEmpty"), PartialDraw.Code,
		EWBCardLifecycleResultCode::DeckEmpty);
	TestEqual(TEXT("Only successful events retained"),
		PartialDraw.TransitionEvents.Num(), 2);
	TestEqual(TEXT("First partial event"),
		PartialDraw.TransitionEvents[0].CardInstanceId,
		FString(TEXT("partial_0")));
	TestEqual(TEXT("Second partial event"),
		PartialDraw.TransitionEvents[1].CardInstanceId,
		FString(TEXT("partial_1")));
	TestEqual(TEXT("Two partial draws committed"),
		GetEntries(Partial, 0, EWBCardZone::Hand)->Num(), 2);
	TestEqual(TEXT("Deck exhausted"), GetEntries(
		Partial, 0, EWBCardZone::Deck)->Num(), 0);

	const FWBCardLifecycleResult Failed =
		WBCardLifecycle::DrawOneCard(Partial, 0);
	TestFalse(TEXT("Empty draw fails"), Failed.bOk);
	TestEqual(TEXT("Failed draw emits zero events"),
		Failed.TransitionEvents.Num(), 0);

	FWBGameStateData SetupState = MakeState();
	AddCard(SetupState, EWBCardZone::Deck,
		TEXT("setup_draw"), TEXT("setup_card"), 0);
	const FWBCardLifecycleResult SetupDraw =
		WBCardLifecycle::ApplySetupDraw(SetupState, 0, 1);
	TestTrue(TEXT("Setup draw still succeeds"), SetupDraw.bOk);
	TestEqual(TEXT("Setup draw publishes no transition event"),
		SetupDraw.TransitionEvents.Num(), 0);
	return true;
}

WB_ZONE_TRANSITION_TEST(
	FWBCardZoneTransitionTransactionAndExclusionTest,
	"Wandbound.CardZoneTransitionEvent.Transaction.RollbackAndExclusions")

bool FWBCardZoneTransitionTransactionAndExclusionTest::RunTest(const FString&)
{
	FWBGameStateData Original = MakeState();
	AddCard(Original, EWBCardZone::Hand, TEXT("rollback"),
		TEXT("rollback_card"), 0);
	const FString BeforeDigest =
		WBProductionMatchReplay::BuildGameStateDigest(Original);
	FWBGameStateData Working = Original;
	const FWBCardLifecycleResult WorkingMove =
		WBCardLifecycle::MoveHandCardToDiscard(
			Working,
			0,
			TEXT("rollback"),
			MakeContext(
				EWBCardZoneTransitionCause::Effect,
				TEXT("effect:rollback")));
	TArray<FWBTraceEvent> Published;
	TestTrue(TEXT("Working mutation succeeds"), WorkingMove.bOk);
	TestEqual(TEXT("Working fact exists"),
		WorkingMove.TransitionEvents.Num(), 1);
	TestEqual(TEXT("Original state remains exact"),
		WBProductionMatchReplay::BuildGameStateDigest(Original),
		BeforeDigest);
	TestEqual(TEXT("Failed parent publishes zero"), Published.Num(), 0);

	FWBGameStateData Committed = Original;
	const FWBCardLifecycleResult CommittedMove =
		WBCardLifecycle::MoveHandCardToDiscard(
			Committed,
			0,
			TEXT("rollback"),
			MakeContext(
				EWBCardZoneTransitionCause::Effect,
				TEXT("effect:commit")));
	WBCardZoneTransition::AppendRedactedTraces(
		CommittedMove.TransitionEvents, Published);
	TestTrue(TEXT("Committed mutation succeeds"), CommittedMove.bOk);
	TestEqual(TEXT("Successful parent publishes once"), Published.Num(), 1);
	TestEqual(TEXT("No duplicate publication"), Published.Num(),
		CommittedMove.TransitionEvents.Num());

	FWBGameStateData Raw = MakeState();
	AddCard(Raw, EWBCardZone::Hand, TEXT("raw"), TEXT("raw_card"), 0);
	TArray<FWBTraceEvent> RawTrace;
	const FWBCardZoneMutationResult RawMove =
		WBCardZoneMutation::TransferExact(
			Raw,
			MakeTransfer(
				EWBCardZone::Hand,
				EWBCardZone::Discard,
				TEXT("raw")));
	TestTrue(TEXT("Raw structural mutation succeeds"), RawMove.bOk);
	TestEqual(TEXT("Raw mutation publishes no event"), RawTrace.Num(), 0);

	FWBGameStateData Extract = MakeState();
	AddCard(Extract, EWBCardZone::Deck, TEXT("extract_deck"),
		TEXT("deck_card"), 0);
	AddCard(Extract, EWBCardZone::Hand, TEXT("extract_hand"),
		TEXT("hand_card"), 0);
	const FWBCardLifecycleResult DeckExtract =
		WBCardLifecycle::RemoveExactCardFromDeck(
			Extract, 0, TEXT("extract_deck"));
	const FWBCardLifecycleResult HandExtract =
		WBCardLifecycle::RemoveExactCardFromHand(
			Extract, 0, TEXT("extract_hand"));
	TestTrue(TEXT("Deck extract succeeds"), DeckExtract.bOk);
	TestEqual(TEXT("Deck extract has no transition"),
		DeckExtract.TransitionEvents.Num(), 0);
	TestTrue(TEXT("Hand extract succeeds"), HandExtract.bOk);
	TestEqual(TEXT("Hand extract has no transition"),
		HandExtract.TransitionEvents.Num(), 0);

	FWBGameStateData Equipped = MakeState();
	FWBEquippedCardEntry EquippedCard;
	EquippedCard.Card.InstanceId = TEXT("equipped");
	EquippedCard.Card.CardId = TEXT("wand");
	EquippedCard.Card.OwnerPlayerId = 0;
	EquippedCard.EquippedToUnitId = 10;
	EquippedCard.SlotId = TEXT("wand_0");
	EquippedCard.EquipOrder = 0;
	Equipped.GetMutableCardZoneStateForTest().EquippedCards.Add(
		EquippedCard);
	const FWBCardLifecycleResult Unequipped =
		WBCardLifecycle::MoveEquippedCardToDiscard(
			Equipped, 0, TEXT("equipped"));
	TestTrue(TEXT("Specialized Equipped to Discard succeeds"),
		Unequipped.bOk);
	TestEqual(TEXT("Equipped path has no generic transition"),
		Unequipped.TransitionEvents.Num(), 0);

	const FString PublishedJson = WBReplayTrace::SerializeEvents(Published);
	TestFalse(TEXT("Hand discard is not destruction"),
		PublishedJson.Contains(TEXT("destruction")));
	TestFalse(TEXT("Hand discard is not sacrifice"),
		PublishedJson.Contains(TEXT("sacrifice")));
	TestFalse(TEXT("Transition does not activate effect"),
		PublishedJson.Contains(TEXT("effect_activation")));
	TestFalse(TEXT("Transition opens no reaction"),
		Committed.HasOpenReactionWindow());

	FString MutationSource;
	FString GameStateSource;
	FString CharacterSummonSource;
	FString HybridSource;
	TestTrue(TEXT("Mutation source loads"), LoadSource(
		TEXT("Source/WandboundCore/Private/WBCardZoneMutation.cpp"),
		MutationSource));
	TestFalse(TEXT("Mutation remains event silent"),
		MutationSource.Contains(TEXT("WBCardZoneTransition")));
	TestTrue(TEXT("GameState source loads"), LoadSource(
		TEXT("Source/WandboundCore/Public/WBGameStateData.h"),
		GameStateSource));
	TestFalse(TEXT("No permanent transition history in state"),
		GameStateSource.Contains(TEXT("PendingCardZoneTransition")));
	TestTrue(TEXT("Character summon source loads"), LoadSource(
		TEXT("Source/WandboundCore/Private/WBCharacterSummon.cpp"),
		CharacterSummonSource));
	TestFalse(TEXT("Character summon gains no generic publication"),
		CharacterSummonSource.Contains(TEXT("AppendRedactedTraces")));
	TestTrue(TEXT("Hybrid source loads"), LoadSource(
		TEXT("Source/WandboundCore/Private/WBHybridSummon.cpp"),
		HybridSource));
	TestFalse(TEXT("Hybrid payment gains no generic publication"),
		HybridSource.Contains(TEXT("AppendRedactedTraces")));
	return true;
}

WB_ZONE_TRANSITION_TEST(
	FWBCardZoneTransitionPrivacyTest,
	"Wandbound.CardZoneTransitionEvent.Privacy.InternalExactPublicRedacted")

bool FWBCardZoneTransitionPrivacyTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	AddCard(State, EWBCardZone::Hand, TEXT("SECRET_INSTANCE"),
		TEXT("SECRET_CARD"), 0);
	AddCard(State, EWBCardZone::Deck, TEXT("SECRET_DECK_OTHER"),
		TEXT("SECRET_DECK_CARD"), 0, 1);
	AddCard(State, EWBCardZone::Hand, TEXT("SECRET_HAND_OTHER"),
		TEXT("SECRET_HAND_CARD"), 0, 1);
	AddCard(State, EWBCardZone::Discard, TEXT("SECRET_DISCARD_OTHER"),
		TEXT("SECRET_DISCARD_CARD"), 0, 1);
	const FWBPublicBoardSummary BeforeBoard =
		WBPublicBoardSummary::Build(State);
	const FWBCardLifecycleResult Result =
		WBCardLifecycle::MoveHandCardToDiscard(
			State,
			0,
			TEXT("SECRET_INSTANCE"),
			MakeContext(
				EWBCardZoneTransitionCause::Rule,
				TEXT("SECRET_ACTION")));
	TestTrue(TEXT("Private transition succeeds"), Result.bOk);
	TestEqual(TEXT("Internal exact instance retained"),
		Result.TransitionEvents[0].CardInstanceId,
		FString(TEXT("SECRET_INSTANCE")));
	TestEqual(TEXT("Internal CardId retained"),
		Result.TransitionEvents[0].CardId,
		FString(TEXT("SECRET_CARD")));
	const FWBTraceEvent Trace =
		WBCardZoneTransition::MakeRedactedTrace(
			Result.TransitionEvents[0]);
	const FString PublicTrace = WBReplayTrace::SerializeEvent(Trace);
	TestFalse(TEXT("Public trace hides instance"),
		PublicTrace.Contains(TEXT("SECRET_INSTANCE")));
	TestFalse(TEXT("Public trace hides CardId"),
		PublicTrace.Contains(TEXT("SECRET_CARD")));
	TestFalse(TEXT("Public trace hides source action"),
		PublicTrace.Contains(TEXT("SECRET_ACTION")));
	TestFalse(TEXT("Public trace hides EventId"),
		PublicTrace.Contains(TEXT("card_zone_transition:t")));
	TestFalse(TEXT("Public trace omits source ZoneIndex"),
		PublicTrace.Contains(TEXT("source_zone_index")));
	TestFalse(TEXT("Public trace omits destination ZoneIndex"),
		PublicTrace.Contains(TEXT("destination_zone_index")));
	TestFalse(TEXT("Public trace omits generic ZoneIndex"),
		PublicTrace.Contains(TEXT("zone_index")));
	TestTrue(TEXT("Public trace preserves occurrence"),
		PublicTrace.Contains(TEXT("card_zone_transition")));
	TestTrue(TEXT("Public trace preserves owner"),
		PublicTrace.Contains(TEXT("\"player_id\": 0")));
	TestTrue(TEXT("Public trace preserves source zone"),
		PublicTrace.Contains(TEXT("\"source_card_zone\": \"hand\"")));
	TestTrue(TEXT("Public trace preserves destination zone"),
		PublicTrace.Contains(TEXT("\"destination_card_zone\": \"discard\"")));

	const FWBCardZonePublicSummary Public =
		WBCardZoneObservation::BuildPublicSummary(State);
	const FWBCardZonePlayerObservation Viewer =
		WBCardZoneObservation::BuildObservationForPlayer(State, 0);
	TestFalse(TEXT("Public summary hides opponent Deck"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(
			Public, TEXT("SECRET_DECK")));
	TestFalse(TEXT("Public summary hides opponent Hand"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(
			Public, TEXT("SECRET_HAND")));
	TestFalse(TEXT("Public summary hides opponent Discard"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(
			Public, TEXT("SECRET_DISCARD")));
	TestFalse(TEXT("Viewer hides opponent Deck alternative"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Viewer, TEXT("SECRET_DECK_OTHER")));
	TestFalse(TEXT("Viewer hides opponent Hand alternative"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Viewer, TEXT("SECRET_HAND_OTHER")));
	TestFalse(TEXT("Viewer hides opponent Discard alternative"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Viewer, TEXT("SECRET_DISCARD_OTHER")));
	const FWBPublicBoardSummary AfterBoard =
		WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Public board unit summary unchanged"),
		AfterBoard.Units.Num(), BeforeBoard.Units.Num());
	TestEqual(TEXT("Public board wall summary unchanged"),
		AfterBoard.Walls.Num(), BeforeBoard.Walls.Num());
	TestEqual(TEXT("Public board terrain summary unchanged"),
		AfterBoard.TerrainTiles.Num(), BeforeBoard.TerrainTiles.Num());

	FWBProductionMatchReplayReceipt Receipt;
	Receipt.bAvailable = true;
	Receipt.OpaqueMatchId = TEXT("zone_transition_smoke");
	Receipt.RecordCount = 1;
	Receipt.bComplete = true;
	Receipt.FinalReplayDigest = TEXT("opaque_digest");
	const FString ReceiptJson =
		WBProductionMatchReplay::SerializeReceipt(Receipt);
	TSharedPtr<FJsonObject> ReceiptObject;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(ReceiptJson);
	TestTrue(TEXT("Receipt parses"),
		FJsonSerializer::Deserialize(Reader, ReceiptObject)
			&& ReceiptObject.IsValid());
	TestEqual(TEXT("Receipt remains exactly eight fields"),
		ReceiptObject.IsValid() ? ReceiptObject->Values.Num() : 0, 8);
	TestFalse(TEXT("Receipt omits exact instance"),
		ReceiptJson.Contains(TEXT("SECRET_INSTANCE")));
	TestFalse(TEXT("Receipt omits protected state digest"),
		ReceiptJson.Contains(TEXT("state_digest")));
	TestFalse(TEXT("Receipt omits protected trace digest"),
		ReceiptJson.Contains(TEXT("trace_digest")));
	return true;
}

WB_ZONE_TRANSITION_TEST(
	FWBCardZoneTransitionDeterminismTest,
	"Wandbound.CardZoneTransitionEvent.Replay.DeterministicStateTraceAndOrder")

bool FWBCardZoneTransitionDeterminismTest::RunTest(const FString&)
{
	auto Run = [](
		FWBGameStateData& OutState,
		TArray<FWBCardZoneTransitionSnapshot>& OutEvents,
		TArray<FWBTraceEvent>& OutTrace)
	{
		OutState = MakeState();
		AddCard(OutState, EWBCardZone::Deck, TEXT("d0"), TEXT("c0"), 0);
		AddCard(OutState, EWBCardZone::Deck, TEXT("d1"), TEXT("c1"), 1);
		AddCard(OutState, EWBCardZone::Hand, TEXT("h0"), TEXT("c2"), 0);
		const FWBCardLifecycleResult Draw =
			WBCardLifecycle::DrawCards(OutState, 0, 2);
		OutEvents.Append(Draw.TransitionEvents);
		WBCardZoneTransition::AppendRedactedTraces(
			Draw.TransitionEvents, OutTrace);
		const FWBCardLifecycleResult Discard =
			WBCardLifecycle::MoveHandCardToDiscard(
				OutState,
				0,
				TEXT("h0"),
				MakeContext(
					EWBCardZoneTransitionCause::Rule,
					TEXT("discard:p0:ih0"),
					2));
		OutEvents.Append(Discard.TransitionEvents);
		WBCardZoneTransition::AppendRedactedTraces(
			Discard.TransitionEvents, OutTrace);
	};

	FWBGameStateData First;
	FWBGameStateData Second;
	TArray<FWBCardZoneTransitionSnapshot> FirstEvents;
	TArray<FWBCardZoneTransitionSnapshot> SecondEvents;
	TArray<FWBTraceEvent> FirstTrace;
	TArray<FWBTraceEvent> SecondTrace;
	Run(First, FirstEvents, FirstTrace);
	Run(Second, SecondEvents, SecondTrace);
	TestEqual(TEXT("Same event count"),
		FirstEvents.Num(), SecondEvents.Num());
	TestEqual(TEXT("Three exact transitions"), FirstEvents.Num(), 3);
	for (int32 Index = 0; Index < FirstEvents.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Event %d deterministic id"), Index),
			FirstEvents[Index].EventIdentity.EventId,
			SecondEvents[Index].EventIdentity.EventId);
		TestEqual(*FString::Printf(TEXT("Event %d deterministic order"), Index),
			FirstEvents[Index].ResolutionOrder,
			SecondEvents[Index].ResolutionOrder);
		TestEqual(*FString::Printf(TEXT("Event %d exact identity"), Index),
			FirstEvents[Index].CardInstanceId,
			SecondEvents[Index].CardInstanceId);
	}
	TestEqual(TEXT("Repeated state digest identical"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));
	TestEqual(TEXT("Repeated trace digest identical"),
		WBProductionMatchReplay::BuildTraceDigest(FirstTrace),
		WBProductionMatchReplay::BuildTraceDigest(SecondTrace));
	TestEqual(TEXT("Repeated trace bytes identical"),
		WBReplayTrace::SerializeEvents(FirstTrace),
		WBReplayTrace::SerializeEvents(SecondTrace));
	TestEqual(TEXT("No duplicate transition traces"),
		FirstTrace.Num(), FirstEvents.Num());
	TestFalse(TEXT("Terminal continuation cannot remain queued"),
		First.HasPendingAttack()
			|| First.HasPendingSummon()
			|| First.HasPendingPrivateCardChoice());
	TestFalse(TEXT("No reaction window introduced"),
		First.HasOpenReactionWindow());
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);

	FString TransitionSource;
	TestTrue(TEXT("Transition source loads"), LoadSource(
		TEXT("Source/WandboundCore/Private/WBCardZoneTransition.cpp"),
		TransitionSource));
	TestFalse(TEXT("Transition consumes no RNG"),
		TransitionSource.Contains(TEXT("Random"))
			|| TransitionSource.Contains(TEXT("Guid"))
			|| TransitionSource.Contains(TEXT("Clock"))
			|| TransitionSource.Contains(TEXT("Time::")));
	return true;
}

#undef WB_ZONE_TRANSITION_TEST

#endif
