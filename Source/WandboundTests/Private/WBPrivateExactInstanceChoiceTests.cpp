#include "Misc/AutomationTest.h"

#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBGameStateData.h"
#include "WBMandatoryDeckChoice.h"
#include "WBPrivateCardChoice.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardDefinition MakeDefinition(
	const FString& CardId,
	const EWBCardDefinitionKind Kind,
	const FString& Faction = FString())
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = Kind;
	if (!Faction.IsEmpty()) Definition.PublicFactions.Add(Faction);
	if (Kind == EWBCardDefinitionKind::Character)
	{
		Definition.CharacterStats.HP = 3;
		Definition.CharacterStats.ATK = 1;
		Definition.CharacterStats.AR = 1;
		Definition.CharacterStats.RL = 2;
	}
	else if (Kind == EWBCardDefinitionKind::Wand)
	{
		Definition.WandStats.RR = 1;
	}
	return Definition;
}

FWBCardDefinitionRepository MakeRepository()
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("private_choice_tests");
	Repository.SourceVersion = TEXT("1");
	Repository.Definitions = {
		MakeDefinition(TEXT("csn_character"), EWBCardDefinitionKind::Character, TEXT("CSN")),
		MakeDefinition(TEXT("other_character"), EWBCardDefinitionKind::Character, TEXT("Other")),
		MakeDefinition(TEXT("csn_wand"), EWBCardDefinitionKind::Wand, TEXT("CSN")),
		MakeDefinition(TEXT("exact_character"), EWBCardDefinitionKind::Character, TEXT("CSN"))
	};
	return Repository;
}

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
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

void AddCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const EWBCardZone Zone,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = PlayerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), PlayerId);
	if (Zone == EWBCardZone::Deck) Zones->Deck.Add(Entry);
	else if (Zone == EWBCardZone::Hand) Zones->Hand.Add(Entry);
	else if (Zone == EWBCardZone::Discard) Zones->Discard.Add(Entry);
}

FWBPrivateCardChoiceDescriptor MakeDescriptor(
	const EWBCardZone Zone,
	const int32 PlayerId = 0)
{
	FWBPrivateCardChoiceDescriptor Descriptor;
	Descriptor.ChoiceId = TEXT("choice:1");
	Descriptor.ChoosingPlayerId = PlayerId;
	Descriptor.SourceZone = Zone;
	Descriptor.Timing = EWBPrivateCardChoiceTiming::ResolutionContinuation;
	Descriptor.Requirement = EWBPrivateCardChoiceRequirement::Mandatory;
	Descriptor.TargetDeclaration = EWBDeclarationProvenance::PlayerDeclared;
	Descriptor.ContinuationKind =
		EWBPrivateCardChoiceContinuationKind::ActivatedEffectContinuation;
	Descriptor.Filter.RequiredKind = EWBCardDefinitionKind::Character;
	return Descriptor;
}

#define WB_PRIVATE_CHOICE_TEST(ClassName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceDeckEnumeration,
	"Wandbound.PrivateExactInstanceChoice.Candidates.DeckDeterministic")
bool FWBPrivateChoiceDeckEnumeration::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	AddCard(State, 0, EWBCardZone::Deck, TEXT("a2"), TEXT("csn_character"), 2);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("a0"), TEXT("csn_character"), 0);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("a1"), TEXT("csn_character"), 1);
	const FWBPrivateCardChoiceCandidateResult Result =
		WBPrivateCardChoice::EnumerateCandidates(
			State, Repository, MakeDescriptor(EWBCardZone::Deck));
	TestTrue(TEXT("01 Deck enumeration succeeds"), Result.bOk);
	TestTrue(TEXT("02 Failure reason empty"), Result.Reason.IsEmpty());
	TestEqual(TEXT("03 Three candidates"), Result.Candidates.Num(), 3);
	TestEqual(TEXT("04 First follows zone order"), Result.Candidates[0].CardInstanceId, FString(TEXT("a0")));
	TestEqual(TEXT("05 Second follows zone order"), Result.Candidates[1].CardInstanceId, FString(TEXT("a1")));
	TestEqual(TEXT("06 Third follows zone order"), Result.Candidates[2].CardInstanceId, FString(TEXT("a2")));
	TestEqual(TEXT("07 First index retained"), Result.Candidates[0].ZoneIndex, 0);
	TestEqual(TEXT("08 Third index retained"), Result.Candidates[2].ZoneIndex, 2);
	TestEqual(TEXT("09 Exact CardId retained"), Result.Candidates[1].CardId, FString(TEXT("csn_character")));
	TestTrue(TEXT("10 Deck is supported"), WBPrivateCardChoice::IsSupportedPrivateZone(EWBCardZone::Deck));
	return true;
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceHandDuplicates,
	"Wandbound.PrivateExactInstanceChoice.Candidates.HandDuplicateInstances")
bool FWBPrivateChoiceHandDuplicates::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	AddCard(State, 0, EWBCardZone::Hand, TEXT("copy_b"), TEXT("csn_character"), 1);
	AddCard(State, 0, EWBCardZone::Hand, TEXT("copy_a"), TEXT("csn_character"), 0);
	FWBPrivateCardChoiceDescriptor Descriptor = MakeDescriptor(EWBCardZone::Hand);
	const FWBPrivateCardChoiceCandidateResult Result =
		WBPrivateCardChoice::FreezeCandidates(State, Repository, Descriptor);
	TestTrue(TEXT("11 Hand enumeration succeeds"), Result.bOk);
	TestEqual(TEXT("12 Duplicate CardIds remain two"), Result.Candidates.Num(), 2);
	TestEqual(TEXT("13 First exact instance"), Result.Candidates[0].CardInstanceId, FString(TEXT("copy_a")));
	TestEqual(TEXT("14 Second exact instance"), Result.Candidates[1].CardInstanceId, FString(TEXT("copy_b")));
	TestEqual(TEXT("15 Same CardId first"), Result.Candidates[0].CardId, Result.Candidates[1].CardId);
	TestEqual(TEXT("16 Frozen set has two"), Descriptor.FrozenCandidateInstanceIds.Num(), 2);
	TestTrue(TEXT("17 Frozen set contains A"), Descriptor.FrozenCandidateInstanceIds.Contains(TEXT("copy_a")));
	TestTrue(TEXT("18 Frozen set contains B"), Descriptor.FrozenCandidateInstanceIds.Contains(TEXT("copy_b")));
	TestTrue(TEXT("19 Hand is supported"), WBPrivateCardChoice::IsSupportedPrivateZone(EWBCardZone::Hand));
	return true;
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceDiscardEnumeration,
	"Wandbound.PrivateExactInstanceChoice.Candidates.DiscardSynthetic")
bool FWBPrivateChoiceDiscardEnumeration::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	AddCard(State, 0, EWBCardZone::Discard, TEXT("discard_1"), TEXT("csn_character"), 0);
	AddCard(State, 0, EWBCardZone::Discard, TEXT("discard_2"), TEXT("other_character"), 1);
	FWBPrivateCardChoiceDescriptor Descriptor = MakeDescriptor(EWBCardZone::Discard);
	Descriptor.Filter.RequiredFaction = TEXT("CSN");
	const FWBPrivateCardChoiceCandidateResult Result =
		WBPrivateCardChoice::EnumerateCandidates(State, Repository, Descriptor);
	TestTrue(TEXT("20 Discard enumeration succeeds"), Result.bOk);
	TestEqual(TEXT("21 Faction leaves one"), Result.Candidates.Num(), 1);
	TestEqual(TEXT("22 Exact discard instance"), Result.Candidates[0].CardInstanceId, FString(TEXT("discard_1")));
	TestEqual(TEXT("23 Exact discard CardId"), Result.Candidates[0].CardId, FString(TEXT("csn_character")));
	TestEqual(TEXT("24 Discard order retained"), Result.Candidates[0].ZoneIndex, 0);
	TestTrue(TEXT("25 Discard is supported"), WBPrivateCardChoice::IsSupportedPrivateZone(EWBCardZone::Discard));
	TestFalse(TEXT("26 Equipped is not speculative private choice support"), WBPrivateCardChoice::IsSupportedPrivateZone(EWBCardZone::Equipped));
	TestFalse(TEXT("27 Board is not a private card choice zone"), WBPrivateCardChoice::IsSupportedPrivateZone(EWBCardZone::Board));
	return true;
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceTypedFilters,
	"Wandbound.PrivateExactInstanceChoice.Candidates.TypedFilters")
bool FWBPrivateChoiceTypedFilters::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	AddCard(State, 0, EWBCardZone::Deck, TEXT("csn"), TEXT("csn_character"), 0);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("other"), TEXT("other_character"), 1);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("wand"), TEXT("csn_wand"), 2);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("exact"), TEXT("exact_character"), 3);
	AddCard(State, 1, EWBCardZone::Deck, TEXT("opponent"), TEXT("csn_character"), 0);
	FWBPrivateCardChoiceDescriptor Descriptor = MakeDescriptor(EWBCardZone::Deck);
	Descriptor.Filter.RequiredFaction = TEXT("CSN");
	const FWBPrivateCardChoiceCandidateResult Faction =
		WBPrivateCardChoice::EnumerateCandidates(State, Repository, Descriptor);
	TestTrue(TEXT("28 Filter enumeration succeeds"), Faction.bOk);
	TestEqual(TEXT("29 Character and faction filter"), Faction.Candidates.Num(), 2);
	TestFalse(TEXT("30 Other faction excluded"), Faction.Candidates.ContainsByPredicate([](const auto& C){ return C.CardInstanceId == TEXT("other"); }));
	TestFalse(TEXT("31 Wand excluded"), Faction.Candidates.ContainsByPredicate([](const auto& C){ return C.CardInstanceId == TEXT("wand"); }));
	TestFalse(TEXT("32 Opponent excluded"), Faction.Candidates.ContainsByPredicate([](const auto& C){ return C.CardInstanceId == TEXT("opponent"); }));
	Descriptor.Filter.RequiredCardId = TEXT("exact_character");
	const FWBPrivateCardChoiceCandidateResult Exact =
		WBPrivateCardChoice::EnumerateCandidates(State, Repository, Descriptor);
	TestEqual(TEXT("33 Exact CardId leaves one"), Exact.Candidates.Num(), 1);
	TestEqual(TEXT("34 Exact CardId identity"), Exact.Candidates[0].CardInstanceId, FString(TEXT("exact")));
	Descriptor.ChoosingPlayerId = 1;
	Descriptor.Filter.RequiredCardId.Reset();
	const FWBPrivateCardChoiceCandidateResult Opponent =
		WBPrivateCardChoice::EnumerateCandidates(State, Repository, Descriptor);
	TestEqual(TEXT("35 Explicit other player sees own zone"), Opponent.Candidates.Num(), 1);
	TestEqual(TEXT("36 Other player exact instance"), Opponent.Candidates[0].CardInstanceId, FString(TEXT("opponent")));
	TestFalse(TEXT("37 No cross-player candidate"), Opponent.Candidates.ContainsByPredicate([](const auto& C){ return C.CardInstanceId == TEXT("csn"); }));
	return true;
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceFreezeRevalidation,
	"Wandbound.PrivateExactInstanceChoice.State.FreezeAndRevalidate")
bool FWBPrivateChoiceFreezeRevalidation::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	AddCard(State, 0, EWBCardZone::Deck, TEXT("old_a"), TEXT("csn_character"), 0);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("old_b"), TEXT("csn_character"), 1);
	FWBPrivateCardChoiceDescriptor Descriptor = MakeDescriptor(EWBCardZone::Deck);
	Descriptor.Filter.RequiredFaction = TEXT("CSN");
	const FWBPrivateCardChoiceCandidateResult Frozen =
		WBPrivateCardChoice::FreezeCandidates(State, Repository, Descriptor);
	TestTrue(TEXT("38 Freeze succeeds"), Frozen.bOk);
	TestEqual(TEXT("39 Two frozen"), Descriptor.FrozenCandidateInstanceIds.Num(), 2);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("new_card"), TEXT("csn_character"), 2);
	TestFalse(TEXT("40 New entry is not frozen"), Descriptor.FrozenCandidateInstanceIds.Contains(TEXT("new_card")));
	const auto NewSelection = WBPrivateCardChoice::ValidateSelection(
		State, Repository, Descriptor, TEXT("new_card"), true);
	TestFalse(TEXT("41 New entry rejected"), NewSelection.bOk);
	TestEqual(TEXT("42 New entry reason"), NewSelection.Reason, FString(TEXT("private_choice_instance_not_frozen")));
	const auto OldB = WBPrivateCardChoice::ValidateSelection(
		State, Repository, Descriptor, TEXT("old_b"), true);
	TestTrue(TEXT("43 Frozen B valid"), OldB.bOk);
	TestEqual(TEXT("44 Frozen B exact"), OldB.Selected.CardInstanceId, FString(TEXT("old_b")));
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), 0);
	Zones->Deck.RemoveAt(1);
	const auto Removed = WBPrivateCardChoice::ValidateSelection(
		State, Repository, Descriptor, TEXT("old_b"), true);
	TestFalse(TEXT("45 Removed frozen entry rejected"), Removed.bOk);
	TestEqual(TEXT("46 Removed reason"), Removed.Reason, FString(TEXT("private_choice_instance_unavailable_or_ineligible")));
	const auto Wrong = WBPrivateCardChoice::ValidateSelection(
		State, Repository, Descriptor, TEXT("missing"), true);
	TestFalse(TEXT("47 Wrong exact instance rejected"), Wrong.bOk);
	return true;
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceDescriptorSemantics,
	"Wandbound.PrivateExactInstanceChoice.State.TypedSemantics")
bool FWBPrivateChoiceDescriptorSemantics::RunTest(const FString& Parameters)
{
	const FWBPrivateCardChoiceDescriptor Descriptor = MakeDescriptor(EWBCardZone::Deck);
	TestEqual(TEXT("48 Deterministic ChoiceId"), Descriptor.ChoiceId, FString(TEXT("choice:1")));
	TestEqual(TEXT("49 Choosing player explicit"), Descriptor.ChoosingPlayerId, 0);
	TestEqual(TEXT("50 Zone typed"), Descriptor.SourceZone, EWBCardZone::Deck);
	TestEqual(TEXT("51 Resolution timing typed"), Descriptor.Timing, EWBPrivateCardChoiceTiming::ResolutionContinuation);
	TestEqual(TEXT("52 Mandatory distinct"), Descriptor.Requirement, EWBPrivateCardChoiceRequirement::Mandatory);
	TestEqual(TEXT("53 Declared target explicit"), Descriptor.TargetDeclaration, EWBDeclarationProvenance::PlayerDeclared);
	TestEqual(TEXT("54 Continuation typed"), Descriptor.ContinuationKind, EWBPrivateCardChoiceContinuationKind::ActivatedEffectContinuation);
	TestEqual(TEXT("55 Character filter typed"), Descriptor.Filter.RequiredKind, EWBCardDefinitionKind::Character);
	return true;
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceViewerFirewall,
	"Wandbound.PrivateExactInstanceChoice.Privacy.ViewerScopedActions")
bool FWBPrivateChoiceViewerFirewall::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	AddCard(State, 0, EWBCardZone::Deck, TEXT("private_a"), TEXT("csn_character"), 0);
	AddCard(State, 0, EWBCardZone::Deck, TEXT("private_b"), TEXT("csn_character"), 1);
	FWBPendingPrivateCardChoiceState Choice;
	Choice.bActive = true;
	Choice.Descriptor = MakeDescriptor(EWBCardZone::Deck);
	WBPrivateCardChoice::FreezeCandidates(State, Repository, Choice.Descriptor);
	State.PendingMandatoryDeckChoice = Choice;
	const TArray<FString> OwnerActions =
		WBMandatoryDeckChoice::EnumerateLegalActionIds(State, Repository, 0);
	const TArray<FString> OpponentActions =
		WBMandatoryDeckChoice::EnumerateLegalActionIds(State, Repository, 1);
	TestEqual(TEXT("56 Owner receives both private actions"), OwnerActions.Num(), 2);
	TestTrue(TEXT("57 Owner action A exact"), FString::Join(OwnerActions, TEXT("|")).Contains(TEXT("private_a")));
	TestTrue(TEXT("58 Owner action B exact"), FString::Join(OwnerActions, TEXT("|")).Contains(TEXT("private_b")));
	TestEqual(TEXT("59 Opponent receives no actions"), OpponentActions.Num(), 0);
	const FWBCardZonePublicSummary Public = WBCardZoneObservation::BuildPublicSummary(State);
	TestFalse(TEXT("60 Public summary hides A"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Public, TEXT("private_a")));
	TestFalse(TEXT("61 Public summary hides B"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Public, TEXT("private_b")));
	const FWBCardZonePlayerObservation Opponent =
		WBCardZoneObservation::BuildObservationForPlayer(State, 1);
	TestFalse(TEXT("62 Opponent observation hides A"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Opponent, TEXT("private_a")));
	TestFalse(TEXT("63 Opponent observation hides B"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Opponent, TEXT("private_b")));
	State.bGameOver = true;
	TestEqual(TEXT("64 Terminal suppresses owner choices"), WBMandatoryDeckChoice::EnumerateLegalActionIds(State, Repository, 0).Num(), 0);
	TestTrue(TEXT("65 Pending state remains one authority"), State.HasPendingPrivateCardChoice() && State.HasPendingMandatoryDeckChoice());
	return true;
}

WB_PRIVATE_CHOICE_TEST(FWBPrivateChoiceDeterminism,
	"Wandbound.PrivateExactInstanceChoice.Determinism.IdenticalInputs")
bool FWBPrivateChoiceDeterminism::RunTest(const FString& Parameters)
{
	FWBGameStateData First = MakeState();
	FWBGameStateData Second = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	for (FWBGameStateData* State : { &First, &Second })
	{
		AddCard(*State, 0, EWBCardZone::Hand, TEXT("z"), TEXT("csn_character"), 1);
		AddCard(*State, 0, EWBCardZone::Hand, TEXT("a"), TEXT("csn_character"), 0);
	}
	FWBPrivateCardChoiceDescriptor FirstDescriptor = MakeDescriptor(EWBCardZone::Hand);
	FWBPrivateCardChoiceDescriptor SecondDescriptor = MakeDescriptor(EWBCardZone::Hand);
	const auto FirstResult = WBPrivateCardChoice::FreezeCandidates(
		First, Repository, FirstDescriptor);
	const auto SecondResult = WBPrivateCardChoice::FreezeCandidates(
		Second, Repository, SecondDescriptor);
	TestTrue(TEXT("66 First run succeeds"), FirstResult.bOk);
	TestTrue(TEXT("67 Second run succeeds"), SecondResult.bOk);
	TestEqual(TEXT("68 Candidate counts match"), FirstResult.Candidates.Num(), SecondResult.Candidates.Num());
	TestEqual(TEXT("69 Frozen sets match"), FirstDescriptor.FrozenCandidateInstanceIds, SecondDescriptor.FrozenCandidateInstanceIds);
	TestEqual(TEXT("70 First identities match"), FirstResult.Candidates[0].CardInstanceId, SecondResult.Candidates[0].CardInstanceId);
	TestEqual(TEXT("71 Second identities match"), FirstResult.Candidates[1].CardInstanceId, SecondResult.Candidates[1].CardInstanceId);
	TestEqual(TEXT("72 Choice IDs match"), FirstDescriptor.ChoiceId, SecondDescriptor.ChoiceId);
	FWBPrivateCardChoiceDescriptor ActivationDescriptor = MakeDescriptor(EWBCardZone::Hand);
	ActivationDescriptor.Timing = EWBPrivateCardChoiceTiming::ActivationDeclaration;
	TestEqual(TEXT("73 Activation timing remains representable"), ActivationDescriptor.Timing, EWBPrivateCardChoiceTiming::ActivationDeclaration);
	return true;
}

#endif
