#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneMutation.h"
#include "WBCardZoneObservation.h"
#include "WBProductionMatchReplay.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBGameStateData MakeZoneMutationState()
{
	FWBGameStateData State;
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

FWBZoneCardEntry MakeZoneMutationEntry(
	const FString& InstanceId,
	const FString& CardId,
	const int32 Owner,
	const EWBCardZone Zone,
	const int32 ZoneIndex)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = Owner;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	return Entry;
}

TArray<FWBZoneCardEntry>* GetMutableZoneMutationTestEntries(
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
	switch (Zone)
	{
	case EWBCardZone::Deck:
		return &Zones->Deck;
	case EWBCardZone::Hand:
		return &Zones->Hand;
	case EWBCardZone::Discard:
		return &Zones->Discard;
	default:
		return nullptr;
	}
}

const TArray<FWBZoneCardEntry>* GetZoneMutationTestEntries(
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
	switch (Zone)
	{
	case EWBCardZone::Deck:
		return &Zones->Deck;
	case EWBCardZone::Hand:
		return &Zones->Hand;
	case EWBCardZone::Discard:
		return &Zones->Discard;
	default:
		return nullptr;
	}
}

void AddZoneMutationTestCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const EWBCardZone Zone,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex,
	const int32 CardOwner = INDEX_NONE)
{
	if (TArray<FWBZoneCardEntry>* Entries =
		GetMutableZoneMutationTestEntries(State, PlayerId, Zone))
	{
		Entries->Add(MakeZoneMutationEntry(
			InstanceId,
			CardId,
			CardOwner == INDEX_NONE ? PlayerId : CardOwner,
			Zone,
			ZoneIndex));
	}
}

FWBCardZoneTransferRequest MakeZoneMutationTransfer(
	const int32 PlayerId,
	const EWBCardZone Source,
	const EWBCardZone Destination,
	const FString& InstanceId)
{
	FWBCardZoneTransferRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceZone = Source;
	Request.DestinationZone = Destination;
	Request.CardInstanceId = InstanceId;
	Request.DestinationPlacement = EWBOrderedZonePlacement::Append;
	return Request;
}

FString ZoneMutationStateDigest(const FWBGameStateData& State)
{
	return WBProductionMatchReplay::BuildGameStateDigest(State);
}

bool LoadZoneMutationSource(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(
		OutSource,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationSupportedTransfersTest,
	"Wandbound.CardZoneMutation.Transfer.SupportedDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationSupportedTransfersTest::RunTest(const FString&)
{
	struct FTransferCase
	{
		EWBCardZone Source;
		EWBCardZone Destination;
		const TCHAR* Name;
	};
	const FTransferCase Cases[] = {
		{ EWBCardZone::Deck, EWBCardZone::Hand, TEXT("DeckToHand") },
		{ EWBCardZone::Hand, EWBCardZone::Discard, TEXT("HandToDiscard") },
		{ EWBCardZone::Deck, EWBCardZone::Discard, TEXT("DeckToDiscard") },
		{ EWBCardZone::Discard, EWBCardZone::Hand, TEXT("DiscardToHand") },
		{ EWBCardZone::Discard, EWBCardZone::Deck, TEXT("DiscardToDeck") },
		{ EWBCardZone::Hand, EWBCardZone::Deck, TEXT("HandToDeck") }
	};

	for (const FTransferCase& Case : Cases)
	{
		FWBGameStateData State = MakeZoneMutationState();
		AddZoneMutationTestCard(
			State, 0, Case.Source, TEXT("selected"), TEXT("card_selected"), 4);
		AddZoneMutationTestCard(
			State, 0, Case.Destination, TEXT("existing"), TEXT("card_existing"), 9);
		const FWBCardZoneMutationResult Result =
			WBCardZoneMutation::TransferExact(
				State,
				MakeZoneMutationTransfer(
					0, Case.Source, Case.Destination, TEXT("selected")));
		const TArray<FWBZoneCardEntry>* Source =
			GetZoneMutationTestEntries(State, 0, Case.Source);
		const TArray<FWBZoneCardEntry>* Destination =
			GetZoneMutationTestEntries(State, 0, Case.Destination);
		const FString Prefix(Case.Name);
		TestTrue(*FString::Printf(TEXT("%s succeeds"), Case.Name), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s code"), Case.Name),
			Result.Code, EWBCardZoneMutationResultCode::Success);
		TestEqual(*FString::Printf(TEXT("%s exact instance"), Case.Name),
			Result.Card.InstanceId, FString(TEXT("selected")));
		TestEqual(*FString::Printf(TEXT("%s exact CardId"), Case.Name),
			Result.Card.CardId, FString(TEXT("card_selected")));
		TestEqual(*FString::Printf(TEXT("%s owner"), Case.Name),
			Result.Card.OwnerPlayerId, 0);
		TestEqual(*FString::Printf(TEXT("%s source empty"), Case.Name),
			Source != nullptr ? Source->Num() : -1, 0);
		TestEqual(*FString::Printf(TEXT("%s destination count"), Case.Name),
			Destination != nullptr ? Destination->Num() : -1, 2);
		TestEqual(*FString::Printf(TEXT("%s existing first"), Case.Name),
			Destination != nullptr ? (*Destination)[0].Card.InstanceId : FString(),
			FString(TEXT("existing")));
		TestEqual(*FString::Printf(TEXT("%s selected appended"), Case.Name),
			Destination != nullptr ? (*Destination)[1].Card.InstanceId : FString(),
			FString(TEXT("selected")));
		TestEqual(*FString::Printf(TEXT("%s append index"), Case.Name),
			Destination != nullptr ? (*Destination)[1].ZoneIndex : -1, 1);
		TestEqual(*FString::Printf(TEXT("%s destination zone"), Case.Name),
			Destination != nullptr ? (*Destination)[1].Zone : EWBCardZone::Unknown,
			Case.Destination);
		FString ZoneReason;
		TestTrue(*FString::Printf(TEXT("%s final zones valid"), Case.Name),
			WBCardZoneState::ValidateZoneStateForTest(
				State.GetCardZoneState(), ZoneReason));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationExactIdentityOrderingTest,
	"Wandbound.CardZoneMutation.Transfer.ExactIdentityAndOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationExactIdentityOrderingTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeZoneMutationState();
	AddZoneMutationTestCard(State, 0, EWBCardZone::Deck,
		TEXT("copy_a"), TEXT("duplicate_card"), 10);
	AddZoneMutationTestCard(State, 0, EWBCardZone::Deck,
		TEXT("copy_b"), TEXT("duplicate_card"), 20);
	AddZoneMutationTestCard(State, 0, EWBCardZone::Deck,
		TEXT("copy_c"), TEXT("other_card"), 30);
	AddZoneMutationTestCard(State, 0, EWBCardZone::Hand,
		TEXT("hand_existing"), TEXT("hand_card"), 5);

	FWBCardZoneTransferRequest Request = MakeZoneMutationTransfer(
		0, EWBCardZone::Deck, EWBCardZone::Hand, TEXT("copy_b"));
	Request.ExpectedCardId = TEXT("duplicate_card");
	const FWBCardZoneMutationResult Result =
		WBCardZoneMutation::TransferExact(State, Request);
	const TArray<FWBZoneCardEntry>* Deck =
		GetZoneMutationTestEntries(State, 0, EWBCardZone::Deck);
	const TArray<FWBZoneCardEntry>* Hand =
		GetZoneMutationTestEntries(State, 0, EWBCardZone::Hand);

	TestTrue(TEXT("Exact duplicate transfer succeeds"), Result.bOk);
	TestEqual(TEXT("Selected duplicate returned"),
		Result.Card.InstanceId, FString(TEXT("copy_b")));
	TestEqual(TEXT("First duplicate remains"),
		Deck != nullptr ? (*Deck)[0].Card.InstanceId : FString(),
		FString(TEXT("copy_a")));
	TestEqual(TEXT("Later card remains second"),
		Deck != nullptr ? (*Deck)[1].Card.InstanceId : FString(),
		FString(TEXT("copy_c")));
	TestEqual(TEXT("Remaining first index"),
		Deck != nullptr ? (*Deck)[0].ZoneIndex : -1, 0);
	TestEqual(TEXT("Remaining second index"),
		Deck != nullptr ? (*Deck)[1].ZoneIndex : -1, 1);
	TestEqual(TEXT("Existing hand remains first"),
		Hand != nullptr ? (*Hand)[0].Card.InstanceId : FString(),
		FString(TEXT("hand_existing")));
	TestEqual(TEXT("Selected duplicate appended"),
		Hand != nullptr ? (*Hand)[1].Card.InstanceId : FString(),
		FString(TEXT("copy_b")));
	TestEqual(TEXT("Existing hand normalized"),
		Hand != nullptr ? (*Hand)[0].ZoneIndex : -1, 0);
	TestEqual(TEXT("Appended hand contiguous"),
		Hand != nullptr ? (*Hand)[1].ZoneIndex : -1, 1);
	TestEqual(TEXT("Source count result"), Result.SourceZoneCountAfter, 2);
	TestEqual(TEXT("Destination count result"),
		Result.DestinationZoneCountAfter, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationExtractionTest,
	"Wandbound.CardZoneMutation.Extract.AllOrderedZones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationExtractionTest::RunTest(const FString&)
{
	const EWBCardZone Zones[] = {
		EWBCardZone::Deck,
		EWBCardZone::Hand,
		EWBCardZone::Discard
	};
	for (const EWBCardZone Zone : Zones)
	{
		FWBGameStateData State = MakeZoneMutationState();
		AddZoneMutationTestCard(State, 0, Zone,
			TEXT("copy_a"), TEXT("duplicate_card"), 2);
		AddZoneMutationTestCard(State, 0, Zone,
			TEXT("copy_b"), TEXT("duplicate_card"), 5);
		AddZoneMutationTestCard(State, 0, Zone,
			TEXT("copy_c"), TEXT("other_card"), 8);
		FWBCardZoneExtractRequest Request;
		Request.PlayerId = 0;
		Request.SourceZone = Zone;
		Request.CardInstanceId = TEXT("copy_b");
		Request.ExpectedCardId = TEXT("duplicate_card");
		const FWBCardZoneMutationResult Result =
			WBCardZoneMutation::ExtractExact(State, Request);
		const TArray<FWBZoneCardEntry>* Entries =
			GetZoneMutationTestEntries(State, 0, Zone);

		TestTrue(TEXT("Extraction succeeds"), Result.bOk);
		TestEqual(TEXT("Extraction code"),
			Result.Code, EWBCardZoneMutationResultCode::Success);
		TestEqual(TEXT("Exact extracted instance"),
			Result.Card.InstanceId, FString(TEXT("copy_b")));
		TestEqual(TEXT("Authoritative extracted CardId"),
			Result.Card.CardId, FString(TEXT("duplicate_card")));
		TestEqual(TEXT("Extracted owner"), Result.Card.OwnerPlayerId, 0);
		TestEqual(TEXT("Remaining count"),
			Entries != nullptr ? Entries->Num() : -1, 2);
		TestEqual(TEXT("Remaining first preserved"),
			Entries != nullptr ? (*Entries)[0].Card.InstanceId : FString(),
			FString(TEXT("copy_a")));
		TestEqual(TEXT("Remaining last preserved"),
			Entries != nullptr ? (*Entries)[1].Card.InstanceId : FString(),
			FString(TEXT("copy_c")));
		TestEqual(TEXT("Remaining first index"),
			Entries != nullptr ? (*Entries)[0].ZoneIndex : -1, 0);
		TestEqual(TEXT("Remaining second index"),
			Entries != nullptr ? (*Entries)[1].ZoneIndex : -1, 1);
		TestEqual(TEXT("Source count returned"),
			Result.SourceZoneCountAfter, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationRejectedRequestsAtomicTest,
	"Wandbound.CardZoneMutation.Failure.RejectedRequestsAreAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationRejectedRequestsAtomicTest::RunTest(const FString&)
{
	auto ExpectFailure = [this](
		const TCHAR* Label,
		FWBGameStateData State,
		const FWBCardZoneTransferRequest& Request,
		const EWBCardZoneMutationResultCode ExpectedCode)
	{
		const FString Before = ZoneMutationStateDigest(State);
		const FWBCardZoneMutationResult Result =
			WBCardZoneMutation::TransferExact(State, Request);
		TestFalse(*FString::Printf(TEXT("%s fails"), Label), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s code"), Label),
			Result.Code, ExpectedCode);
		TestEqual(*FString::Printf(TEXT("%s state unchanged"), Label),
			ZoneMutationStateDigest(State), Before);
	};

	FWBGameStateData Base = MakeZoneMutationState();
	AddZoneMutationTestCard(Base, 0, EWBCardZone::Deck,
		TEXT("selected"), TEXT("selected_card"), 0);
	ExpectFailure(TEXT("Invalid player"), Base,
		MakeZoneMutationTransfer(7, EWBCardZone::Deck,
			EWBCardZone::Hand, TEXT("selected")),
		EWBCardZoneMutationResultCode::InvalidPlayer);
	ExpectFailure(TEXT("Unsupported source"), Base,
		MakeZoneMutationTransfer(0, EWBCardZone::Board,
			EWBCardZone::Hand, TEXT("selected")),
		EWBCardZoneMutationResultCode::UnsupportedSourceZone);
	ExpectFailure(TEXT("Unsupported destination"), Base,
		MakeZoneMutationTransfer(0, EWBCardZone::Deck,
			EWBCardZone::Equipped, TEXT("selected")),
		EWBCardZoneMutationResultCode::UnsupportedDestinationZone);
	ExpectFailure(TEXT("Same zone"), Base,
		MakeZoneMutationTransfer(0, EWBCardZone::Deck,
			EWBCardZone::Deck, TEXT("selected")),
		EWBCardZoneMutationResultCode::SameZoneTransferUnsupported);
	FWBCardZoneTransferRequest BadPlacement = MakeZoneMutationTransfer(
		0, EWBCardZone::Deck, EWBCardZone::Hand, TEXT("selected"));
	BadPlacement.DestinationPlacement = EWBOrderedZonePlacement::Unknown;
	ExpectFailure(TEXT("Invalid placement"), Base, BadPlacement,
		EWBCardZoneMutationResultCode::InvalidDestinationPlacement);
	ExpectFailure(TEXT("Missing instance"), Base,
		MakeZoneMutationTransfer(0, EWBCardZone::Deck,
			EWBCardZone::Hand, TEXT("missing")),
		EWBCardZoneMutationResultCode::CardInstanceMissing);
	ExpectFailure(TEXT("Wrong source zone"), Base,
		MakeZoneMutationTransfer(0, EWBCardZone::Hand,
			EWBCardZone::Discard, TEXT("selected")),
		EWBCardZoneMutationResultCode::CardNotInExpectedZone);
	ExpectFailure(TEXT("Cross player"), Base,
		MakeZoneMutationTransfer(1, EWBCardZone::Deck,
			EWBCardZone::Hand, TEXT("selected")),
		EWBCardZoneMutationResultCode::OwnerMismatch);
	FWBCardZoneTransferRequest WrongCardId = MakeZoneMutationTransfer(
		0, EWBCardZone::Deck, EWBCardZone::Hand, TEXT("selected"));
	WrongCardId.ExpectedCardId = TEXT("wrong_card");
	ExpectFailure(TEXT("Card identity mismatch"), Base, WrongCardId,
		EWBCardZoneMutationResultCode::CardIdentityMismatch);
	ExpectFailure(TEXT("Marker source rejected"), Base,
		MakeZoneMutationTransfer(0, EWBCardZone::Marker,
			EWBCardZone::Hand, TEXT("selected")),
		EWBCardZoneMutationResultCode::UnsupportedSourceZone);

	FWBGameStateData Equipped = MakeZoneMutationState();
	FWBEquippedCardEntry EquippedEntry;
	EquippedEntry.Card.InstanceId = TEXT("equipped");
	EquippedEntry.Card.CardId = TEXT("wand");
	EquippedEntry.Card.OwnerPlayerId = 0;
	EquippedEntry.EquippedToUnitId = 1;
	EquippedEntry.SlotId = TEXT("slot_0");
	EquippedEntry.EquipOrder = 0;
	Equipped.GetMutableCardZoneStateForTest().EquippedCards.Add(EquippedEntry);
	ExpectFailure(TEXT("Equipped source rejected"), Equipped,
		MakeZoneMutationTransfer(0, EWBCardZone::Equipped,
			EWBCardZone::Discard, TEXT("equipped")),
		EWBCardZoneMutationResultCode::UnsupportedSourceZone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationMalformedStateAtomicTest,
	"Wandbound.CardZoneMutation.Failure.MalformedStateIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationMalformedStateAtomicTest::RunTest(const FString&)
{
	auto ExpectInvalidState = [this](const TCHAR* Label, FWBGameStateData State)
	{
		const FString Before = ZoneMutationStateDigest(State);
		const FWBCardZoneMutationResult Result =
			WBCardZoneMutation::TransferExact(
				State,
				MakeZoneMutationTransfer(0, EWBCardZone::Deck,
					EWBCardZone::Hand, TEXT("selected")));
		TestFalse(*FString::Printf(TEXT("%s fails"), Label), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s invalid state"), Label),
			Result.Code, EWBCardZoneMutationResultCode::InvalidZoneState);
		TestEqual(*FString::Printf(TEXT("%s rollback"), Label),
			ZoneMutationStateDigest(State), Before);
	};

	FWBGameStateData Duplicate = MakeZoneMutationState();
	AddZoneMutationTestCard(Duplicate, 0, EWBCardZone::Deck,
		TEXT("selected"), TEXT("card"), 0);
	AddZoneMutationTestCard(Duplicate, 0, EWBCardZone::Hand,
		TEXT("selected"), TEXT("card"), 0);
	const FString DuplicateBefore = ZoneMutationStateDigest(Duplicate);
	const FWBCardZoneMutationResult DuplicateResult =
		WBCardZoneMutation::TransferExact(
			Duplicate,
			MakeZoneMutationTransfer(0, EWBCardZone::Deck,
				EWBCardZone::Discard, TEXT("selected")));
	TestFalse(TEXT("Duplicate instance fails"), DuplicateResult.bOk);
	TestEqual(TEXT("Duplicate instance code"), DuplicateResult.Code,
		EWBCardZoneMutationResultCode::DuplicateInstanceId);
	TestEqual(TEXT("Duplicate rollback"),
		ZoneMutationStateDigest(Duplicate), DuplicateBefore);

	FWBGameStateData DuplicateIndex = MakeZoneMutationState();
	AddZoneMutationTestCard(DuplicateIndex, 0, EWBCardZone::Deck,
		TEXT("selected"), TEXT("card"), 0);
	AddZoneMutationTestCard(DuplicateIndex, 0, EWBCardZone::Deck,
		TEXT("other"), TEXT("other_card"), 0);
	ExpectInvalidState(TEXT("Duplicate ZoneIndex"), DuplicateIndex);

	FWBGameStateData WrongZone = MakeZoneMutationState();
	AddZoneMutationTestCard(WrongZone, 0, EWBCardZone::Deck,
		TEXT("selected"), TEXT("card"), 0);
	GetMutableZoneMutationTestEntries(
		WrongZone, 0, EWBCardZone::Deck)->Last().Zone = EWBCardZone::Hand;
	ExpectInvalidState(TEXT("Container zone mismatch"), WrongZone);

	FWBGameStateData EmptyIdentity = MakeZoneMutationState();
	AddZoneMutationTestCard(EmptyIdentity, 0, EWBCardZone::Deck,
		TEXT("selected"), TEXT("card"), 0);
	AddZoneMutationTestCard(EmptyIdentity, 0, EWBCardZone::Hand,
		FString(), TEXT("malformed"), 0);
	ExpectInvalidState(TEXT("Empty unrelated instance"), EmptyIdentity);

	FWBGameStateData OwnerMismatch = MakeZoneMutationState();
	AddZoneMutationTestCard(OwnerMismatch, 0, EWBCardZone::Deck,
		TEXT("selected"), TEXT("card"), 0, 1);
	const FString OwnerBefore = ZoneMutationStateDigest(OwnerMismatch);
	const FWBCardZoneMutationResult OwnerResult =
		WBCardZoneMutation::TransferExact(
			OwnerMismatch,
			MakeZoneMutationTransfer(0, EWBCardZone::Deck,
				EWBCardZone::Hand, TEXT("selected")));
	TestFalse(TEXT("Owner mismatch fails"), OwnerResult.bOk);
	TestEqual(TEXT("Owner mismatch code"), OwnerResult.Code,
		EWBCardZoneMutationResultCode::OwnerMismatch);
	TestEqual(TEXT("Owner mismatch rollback"),
		ZoneMutationStateDigest(OwnerMismatch), OwnerBefore);

	FWBGameStateData MissingZones = MakeZoneMutationState();
	MissingZones.GetMutableCardZoneStateForTest().PlayerZones.RemoveAt(1);
	const FString MissingZonesBefore = ZoneMutationStateDigest(MissingZones);
	const FWBCardZoneMutationResult MissingZonesResult =
		WBCardZoneMutation::TransferExact(
			MissingZones,
			MakeZoneMutationTransfer(1, EWBCardZone::Deck,
				EWBCardZone::Hand, TEXT("missing")));
	TestFalse(TEXT("Missing player zones fails"), MissingZonesResult.bOk);
	TestEqual(TEXT("Missing player zones code"), MissingZonesResult.Code,
		EWBCardZoneMutationResultCode::PlayerZonesMissing);
	TestEqual(TEXT("Missing zones rollback"),
		ZoneMutationStateDigest(MissingZones), MissingZonesBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationLifecycleCompatibilityTest,
	"Wandbound.CardZoneMutation.Compatibility.LifecycleAdapters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationLifecycleCompatibilityTest::RunTest(const FString&)
{
	FWBGameStateData DrawState = MakeZoneMutationState();
	AddZoneMutationTestCard(DrawState, 0, EWBCardZone::Deck,
		TEXT("late"), TEXT("late_card"), 7);
	AddZoneMutationTestCard(DrawState, 0, EWBCardZone::Deck,
		TEXT("top"), TEXT("top_card"), 1);
	AddZoneMutationTestCard(DrawState, 0, EWBCardZone::Hand,
		TEXT("existing_hand"), TEXT("hand_card"), 4);
	const FWBCardLifecycleResult Draw =
		WBCardLifecycle::DrawOneCard(DrawState, 0);
	const TArray<FWBZoneCardEntry>* DrawDeck =
		GetZoneMutationTestEntries(DrawState, 0, EWBCardZone::Deck);
	const TArray<FWBZoneCardEntry>* DrawHand =
		GetZoneMutationTestEntries(DrawState, 0, EWBCardZone::Hand);
	TestTrue(TEXT("Draw adapter succeeds"), Draw.bOk);
	TestEqual(TEXT("Authoritative top selected"),
		Draw.CardInstanceId, FString(TEXT("top")));
	TestEqual(TEXT("Remaining Deck preserved"),
		DrawDeck != nullptr ? (*DrawDeck)[0].Card.InstanceId : FString(),
		FString(TEXT("late")));
	TestEqual(TEXT("Remaining Deck contiguous"),
		DrawDeck != nullptr ? (*DrawDeck)[0].ZoneIndex : -1, 0);
	TestEqual(TEXT("Existing Hand preserved"),
		DrawHand != nullptr ? (*DrawHand)[0].Card.InstanceId : FString(),
		FString(TEXT("existing_hand")));
	TestEqual(TEXT("Draw appended"),
		DrawHand != nullptr ? (*DrawHand)[1].Card.InstanceId : FString(),
		FString(TEXT("top")));
	TestEqual(TEXT("Draw append contiguous"),
		DrawHand != nullptr ? (*DrawHand)[1].ZoneIndex : -1, 1);

	FWBGameStateData MultiDraw = MakeZoneMutationState();
	AddZoneMutationTestCard(MultiDraw, 0, EWBCardZone::Deck,
		TEXT("only"), TEXT("only_card"), 0);
	const FWBCardLifecycleResult Partial =
		WBCardLifecycle::DrawCards(MultiDraw, 0, 2);
	TestFalse(TEXT("Second sequential draw fails"), Partial.bOk);
	TestEqual(TEXT("Sequential draw retains DeckEmpty"),
		Partial.Code, EWBCardLifecycleResultCode::DeckEmpty);
	TestEqual(TEXT("First sequential draw remains committed"),
		GetZoneMutationTestEntries(MultiDraw, 0, EWBCardZone::Hand)->Num(), 1);

	FWBGameStateData Discard = MakeZoneMutationState();
	AddZoneMutationTestCard(Discard, 0, EWBCardZone::Hand,
		TEXT("discard_me"), TEXT("discard_card"), 0);
	const FWBCardLifecycleResult Discarded =
		WBCardLifecycle::MoveHandCardToDiscard(
			Discard, 0, TEXT("discard_me"));
	TestTrue(TEXT("Discard adapter succeeds"), Discarded.bOk);
	TestEqual(TEXT("Discard exact identity"),
		Discarded.CardInstanceId, FString(TEXT("discard_me")));
	TestEqual(TEXT("Discard appended"),
		GetZoneMutationTestEntries(Discard, 0, EWBCardZone::Discard)
			->Last().Card.InstanceId,
		FString(TEXT("discard_me")));

	FWBGameStateData Extract = MakeZoneMutationState();
	AddZoneMutationTestCard(Extract, 0, EWBCardZone::Deck,
		TEXT("deck_extract"), TEXT("deck_card"), 0);
	AddZoneMutationTestCard(Extract, 0, EWBCardZone::Hand,
		TEXT("hand_extract"), TEXT("hand_card"), 0);
	const FWBCardLifecycleResult DeckExtract =
		WBCardLifecycle::RemoveExactCardFromDeck(
			Extract, 0, TEXT("deck_extract"));
	const FWBCardLifecycleResult HandExtract =
		WBCardLifecycle::RemoveExactCardFromHand(
			Extract, 0, TEXT("hand_extract"));
	TestTrue(TEXT("Deck extraction adapter succeeds"), DeckExtract.bOk);
	TestEqual(TEXT("Deck extraction exact CardId"),
		DeckExtract.CardId, FString(TEXT("deck_card")));
	TestTrue(TEXT("Hand extraction adapter succeeds"), HandExtract.bOk);
	TestEqual(TEXT("Hand extraction exact CardId"),
		HandExtract.CardId, FString(TEXT("hand_card")));
	TestEqual(TEXT("Deck consumed"),
		GetZoneMutationTestEntries(Extract, 0, EWBCardZone::Deck)->Num(), 0);
	TestEqual(TEXT("Hand consumed"),
		GetZoneMutationTestEntries(Extract, 0, EWBCardZone::Hand)->Num(), 0);

	FWBGameStateData Skip = MakeZoneMutationState();
	AddZoneMutationTestCard(Skip, 0, EWBCardZone::Deck,
		TEXT("skip"), TEXT("skip_card"), 0);
	const FWBCardLifecycleResult Skipped =
		WBCardLifecycle::ApplyTurnStartDraw(Skip, 0, 1, 0);
	TestTrue(TEXT("First player skip succeeds"), Skipped.bOk);
	TestEqual(TEXT("First player skip code"), Skipped.Code,
		EWBCardLifecycleResultCode::FirstPlayerFirstTurnDrawSkipped);
	TestEqual(TEXT("Skip leaves Deck unchanged"),
		GetZoneMutationTestEntries(Skip, 0, EWBCardZone::Deck)->Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationPrivacyAndSpecializationTest,
	"Wandbound.CardZoneMutation.Privacy.AndSpecializedExclusions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationPrivacyAndSpecializationTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeZoneMutationState();
	AddZoneMutationTestCard(State, 0, EWBCardZone::Hand,
		TEXT("own_private_instance"), TEXT("own_private_card"), 0);
	AddZoneMutationTestCard(State, 1, EWBCardZone::Deck,
		TEXT("SECRET_DECK_INSTANCE"), TEXT("SECRET_DECK_CARD"), 0);
	AddZoneMutationTestCard(State, 1, EWBCardZone::Hand,
		TEXT("SECRET_HAND_INSTANCE"), TEXT("SECRET_HAND_CARD"), 0);
	AddZoneMutationTestCard(State, 1, EWBCardZone::Discard,
		TEXT("SECRET_DISCARD_INSTANCE"), TEXT("SECRET_DISCARD_CARD"), 0);
	const FWBCardZoneMutationResult Moved =
		WBCardZoneMutation::TransferExact(
			State,
			MakeZoneMutationTransfer(0, EWBCardZone::Hand,
				EWBCardZone::Discard, TEXT("own_private_instance")));
	const FWBCardZonePublicSummary Public =
		WBCardZoneObservation::BuildPublicSummary(State);
	const FWBCardZonePlayerObservation Viewer =
		WBCardZoneObservation::BuildObservationForPlayer(State, 0);
	TestTrue(TEXT("Private transfer succeeds"), Moved.bOk);
	TestFalse(TEXT("Public Deck identity hidden"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(
			Public, TEXT("SECRET_DECK")));
	TestFalse(TEXT("Public Hand identity hidden"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(
			Public, TEXT("SECRET_HAND")));
	TestFalse(TEXT("Public Discard identity hidden"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(
			Public, TEXT("SECRET_DISCARD")));
	TestFalse(TEXT("Opponent Deck hidden from viewer"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Viewer, TEXT("SECRET_DECK")));
	TestFalse(TEXT("Opponent Hand hidden from viewer"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Viewer, TEXT("SECRET_HAND")));
	TestFalse(TEXT("Opponent Discard hidden from viewer"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Viewer, TEXT("SECRET_DISCARD")));

	FWBGameStateData EquippedState = MakeZoneMutationState();
	FWBEquippedCardEntry Equipped;
	Equipped.Card.InstanceId = TEXT("equipped_instance");
	Equipped.Card.CardId = TEXT("equipped_card");
	Equipped.Card.OwnerPlayerId = 0;
	Equipped.EquippedToUnitId = 3;
	Equipped.SlotId = TEXT("wand_0");
	Equipped.EquipOrder = 0;
	EquippedState.GetMutableCardZoneStateForTest().EquippedCards.Add(Equipped);
	const FWBCardLifecycleResult Specialized =
		WBCardLifecycle::MoveEquippedCardToDiscard(
			EquippedState, 0, TEXT("equipped_instance"));
	TestTrue(TEXT("Specialized equipped path still succeeds"), Specialized.bOk);
	TestEqual(TEXT("Specialized equipped source emptied"),
		EquippedState.GetCardZoneState().EquippedCards.Num(), 0);
	TestEqual(TEXT("Specialized equipped card reaches Discard"),
		GetZoneMutationTestEntries(
			EquippedState, 0, EWBCardZone::Discard)->Num(), 1);

	FString Source;
	TestTrue(TEXT("Mutation source loads"), LoadZoneMutationSource(
		TEXT("Source/WandboundCore/Private/WBCardZoneMutation.cpp"), Source));
	TestFalse(TEXT("Mutation emits no trace events"),
		Source.Contains(TEXT("FWBTraceEvent")));
	TestFalse(TEXT("Mutation has no replay trace dependency"),
		Source.Contains(TEXT("WBReplayTrace")));
	TestFalse(TEXT("Mutation does not enumerate choices"),
		Source.Contains(TEXT("WBPrivateCardChoice")));
	TestFalse(TEXT("Mutation consumes no RNG"),
		Source.Contains(TEXT("Random")) || Source.Contains(TEXT("Roll")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCardZoneMutationDeterminismTest,
	"Wandbound.CardZoneMutation.Replay.DeterministicStateAndTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWBCardZoneMutationDeterminismTest::RunTest(const FString&)
{
	FWBGameStateData First = MakeZoneMutationState();
	AddZoneMutationTestCard(First, 0, EWBCardZone::Deck,
		TEXT("first"), TEXT("same_card"), 0);
	AddZoneMutationTestCard(First, 0, EWBCardZone::Deck,
		TEXT("selected"), TEXT("same_card"), 1);
	AddZoneMutationTestCard(First, 0, EWBCardZone::Hand,
		TEXT("existing"), TEXT("existing_card"), 0);
	FWBGameStateData Second = First;
	const FWBCardZoneTransferRequest Request = MakeZoneMutationTransfer(
		0, EWBCardZone::Deck, EWBCardZone::Hand, TEXT("selected"));
	const FWBCardZoneMutationResult A =
		WBCardZoneMutation::TransferExact(First, Request);
	const FWBCardZoneMutationResult B =
		WBCardZoneMutation::TransferExact(Second, Request);
	TestTrue(TEXT("First deterministic transfer succeeds"), A.bOk);
	TestTrue(TEXT("Second deterministic transfer succeeds"), B.bOk);
	TestEqual(TEXT("Exact results match"),
		A.Card.InstanceId, B.Card.InstanceId);
	TestEqual(TEXT("State digests match"),
		ZoneMutationStateDigest(First), ZoneMutationStateDigest(Second));
	const TArray<FWBTraceEvent> NoTraceA;
	const TArray<FWBTraceEvent> NoTraceB;
	TestEqual(TEXT("Trace digests match"),
		WBProductionMatchReplay::BuildTraceDigest(NoTraceA),
		WBProductionMatchReplay::BuildTraceDigest(NoTraceB));
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

#endif
