#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionFixtureLoader.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneTransitionTrigger.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCardZoneTransitionTriggerSmoke.h"
#include "WBProductionMatchReplay.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WBCardZoneTransitionTriggerTestsPrivate
{
constexpr int32 PlayerA = 0;
constexpr int32 PlayerB = 1;

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = PlayerA;
	State.PriorityPlayer = PlayerA;
	State.TurnNumber = 3;
	for (int32 PlayerId = PlayerA; PlayerId <= PlayerB; ++PlayerId)
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

TArray<FWBZoneCardEntry>* Entries(
	FWBGameStateData& State, int32 PlayerId, EWBCardZone Zone)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), PlayerId);
	if (Zones == nullptr) return nullptr;
	if (Zone == EWBCardZone::Deck) return &Zones->Deck;
	if (Zone == EWBCardZone::Hand) return &Zones->Hand;
	if (Zone == EWBCardZone::Discard) return &Zones->Discard;
	return nullptr;
}

void AddCard(
	FWBGameStateData& State, int32 PlayerId, EWBCardZone Zone,
	const FString& InstanceId, const FString& CardId)
{
	TArray<FWBZoneCardEntry>* ZoneEntries = Entries(State, PlayerId, Zone);
	check(ZoneEntries != nullptr);
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = PlayerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneEntries->Num();
	ZoneEntries->Add(MoveTemp(Entry));
}

FWBCardDefinition MakeDefinition(const FString& CardId)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	return Definition;
}

FWBCardZoneTransitionTriggerDefinition MakeTrigger(
	const FString& TriggerId,
	EWBCardZoneTransitionTriggerSourceScope Scope,
	int32 DrawCount = 1)
{
	FWBCardZoneTransitionTriggerDefinition Trigger;
	Trigger.TriggerId = TriggerId;
	Trigger.SourceScope = Scope;
	Trigger.DrawCount = DrawCount;
	return Trigger;
}

FWBCardDefinitionRepository MakeRepository(
	const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("zone_transition_trigger_tests");
	Repository.SourceVersion = TEXT("synthetic_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBCardLifecycleResult Transfer(
	FWBGameStateData& State, int32 PlayerId,
	EWBCardZone Source, EWBCardZone Destination,
	const FString& InstanceId, EWBCardZoneTransitionCause Cause,
	int32 Order = 0)
{
	FWBCardZoneTransferRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceZone = Source;
	Request.DestinationZone = Destination;
	Request.CardInstanceId = InstanceId;
	Request.DestinationPlacement = EWBOrderedZonePlacement::Append;
	FWBCardZoneTransitionContext Context;
	Context.Cause = Cause;
	Context.SourceActionId = TEXT("synthetic_root");
	Context.ContinuationId = TEXT("synthetic_continuation");
	Context.ResolutionOrder = Order;
	return WBCardLifecycle::TransferExactCard(State, Request, Context);
}

const FWBZoneCardEntry* FindExact(
	const FWBGameStateData& State, int32 PlayerId,
	EWBCardZone Zone, const FString& InstanceId)
{
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), PlayerId);
	const TArray<FWBZoneCardEntry>* ZoneEntries = nullptr;
	if (Zones != nullptr && Zone == EWBCardZone::Deck) ZoneEntries = &Zones->Deck;
	if (Zones != nullptr && Zone == EWBCardZone::Hand) ZoneEntries = &Zones->Hand;
	if (Zones != nullptr && Zone == EWBCardZone::Discard) ZoneEntries = &Zones->Discard;
	return ZoneEntries == nullptr ? nullptr : ZoneEntries->FindByPredicate(
		[&InstanceId](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == InstanceId;
		});
}

int32 CountTraceKind(const TArray<FWBTraceEvent>& Traces, FName Kind)
{
	return Traces.FilterByPredicate([Kind](const FWBTraceEvent& Trace)
	{
		return Trace.Kind == Kind;
	}).Num();
}
}

using namespace WBCardZoneTransitionTriggerTestsPrivate;

#define WB_ZONE_TRIGGER_TEST(ClassName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_ZONE_TRIGGER_TEST(FWBCardZoneTransitionTriggerDefinitionTest,
	"Wandbound.CardZoneTransitionTrigger.Definition.TypedFilterAndParser")
bool FWBCardZoneTransitionTriggerDefinitionTest::RunTest(const FString&)
{
	FWBCardZoneTransitionTriggerDefinition Trigger = MakeTrigger(
		TEXT("self_enter"),
		EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf);
	TestEqual(TEXT("typed self scope"), Trigger.SourceScope,
		EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf);
	TestFalse(TEXT("source filter disabled"), Trigger.Filter.bRequireSourceZone);
	TestFalse(TEXT("destination filter disabled"), Trigger.Filter.bRequireDestinationZone);
	TestFalse(TEXT("cause filter disabled"), Trigger.Filter.bRequireCause);
	TestEqual(TEXT("disabled source is Unknown"),
		Trigger.Filter.RequiredSourceZone, EWBCardZone::Unknown);
	TestEqual(TEXT("disabled destination is Unknown"),
		Trigger.Filter.RequiredDestinationZone, EWBCardZone::Unknown);
	TestEqual(TEXT("disabled cause is Unknown"),
		Trigger.Filter.RequiredCause, EWBCardZoneTransitionCause::Unknown);
	TestTrue(TEXT("mandatory by default"), Trigger.bMandatory);

	const FString Json = TEXT(R"JSON({
	 "repository_id":"zone_fixture","source_version":"v1","cards":[
	 {"card_id":"fixture_card","public_name":"Fixture Card",
	  "card_zone_transition_triggers":[
	   {"trigger_id":"leave_discard","source_scope":"moved_card_self",
	    "filter":{"source_zone":"discard","destination_zone":"hand","cause":"effect"},
	    "draw_count":2,"mandatory":true},
	   {"trigger_id":"observe_discard","source_scope":"resident_discard_observer",
	    "filter":{"destination_zone":"discard"},"draw_count":1,"mandatory":true}
	  ]}]})JSON");
	const FWBCardDefinitionFixtureLoadResult Loaded =
		WBCardDefinitionFixtureLoader::LoadRepositoryFromJsonString(
			Json, TEXT("inline_zone_trigger.json"));
	TestTrue(TEXT("typed fixture loads"), Loaded.bOk);
	TestEqual(TEXT("one card parsed"), Loaded.Repository.Definitions.Num(), 1);
	const FWBCardDefinition& Parsed = Loaded.Repository.Definitions[0];
	TestEqual(TEXT("two triggers parsed"), Parsed.CardZoneTransitionTriggers.Num(), 2);
	const FWBCardZoneTransitionTriggerDefinition& Leave =
		Parsed.CardZoneTransitionTriggers[0];
	TestTrue(TEXT("source filter enabled"), Leave.Filter.bRequireSourceZone);
	TestEqual(TEXT("source parsed"), Leave.Filter.RequiredSourceZone, EWBCardZone::Discard);
	TestTrue(TEXT("destination filter enabled"), Leave.Filter.bRequireDestinationZone);
	TestEqual(TEXT("destination parsed"), Leave.Filter.RequiredDestinationZone, EWBCardZone::Hand);
	TestTrue(TEXT("cause filter enabled"), Leave.Filter.bRequireCause);
	TestEqual(TEXT("cause parsed"), Leave.Filter.RequiredCause, EWBCardZoneTransitionCause::Effect);
	TestEqual(TEXT("draw count parsed"), Leave.DrawCount, 2);
	TestEqual(TEXT("observer scope parsed"), Parsed.CardZoneTransitionTriggers[1].SourceScope,
		EWBCardZoneTransitionTriggerSourceScope::ResidentDiscardObserver);

	FWBCardDefinition Valid = MakeDefinition(TEXT("valid"));
	Valid.CardZoneTransitionTriggers.Add(Trigger);
	TestTrue(TEXT("valid repository accepted"),
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ Valid })).bOk);
	FWBCardDefinition ZeroDraw = Valid;
	ZeroDraw.CardZoneTransitionTriggers[0].DrawCount = 0;
	TestTrue(TEXT("zero draw definition accepted"),
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ ZeroDraw })).bOk);
	FWBGameStateData ZeroState = MakeState();
	AddCard(ZeroState, PlayerA, EWBCardZone::Hand, TEXT("zero_source"), TEXT("valid"));
	const FWBCardLifecycleResult ZeroRoot = Transfer(ZeroState, PlayerA,
		EWBCardZone::Hand, EWBCardZone::Discard, TEXT("zero_source"),
		EWBCardZoneTransitionCause::Effect);
	const FWBCardZoneTransitionTriggerResult ZeroResult =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			ZeroState, MakeRepository({ ZeroDraw }), ZeroRoot.TransitionEvents);
	TestTrue(TEXT("explicit zero draw resolves with empty Deck"), ZeroResult.bOk);
	TestEqual(TEXT("zero draw resolves one trigger"), ZeroResult.ResolvedTriggerCount, 1);
	TestEqual(TEXT("zero draw produces no draw"), ZeroResult.DrawnCardCount, 0);
	TestTrue(TEXT("zero draw creates no nested transitions"), ZeroResult.NestedTransitionEvents.IsEmpty());
	ZeroDraw.CardZoneTransitionTriggers[0].DrawCount = -1;
	TestFalse(TEXT("negative draw rejected"),
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ ZeroDraw })).bOk);
	const FString ZeroJson = Json.Replace(TEXT("\"draw_count\":2"), TEXT("\"draw_count\":0"));
	TestTrue(TEXT("fixture parser accepts zero draw"),
		WBCardDefinitionFixtureLoader::LoadRepositoryFromJsonString(
			ZeroJson, TEXT("zero_draw.json")).bOk);
	FWBCardDefinition Optional = Valid;
	Optional.CardZoneTransitionTriggers[0].bMandatory = false;
	TestEqual(TEXT("optional rejected"),
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ Optional })).Reason,
		FString(TEXT("optional_card_zone_transition_trigger_unsupported")));
	FWBCardDefinition BadScope = Valid;
	BadScope.CardZoneTransitionTriggers[0].SourceScope =
		EWBCardZoneTransitionTriggerSourceScope::Unknown;
	TestEqual(TEXT("unknown scope rejected"),
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ BadScope })).Reason,
		FString(TEXT("card_zone_transition_trigger_source_scope_unsupported")));
	FWBCardDefinition BadCause = Valid;
	BadCause.CardZoneTransitionTriggers[0].Filter.bRequireCause = true;
	TestEqual(TEXT("Unknown not wildcard when required"),
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ BadCause })).Reason,
		FString(TEXT("card_zone_transition_trigger_filter_invalid")));
	FWBCardDefinition Duplicate = Valid;
	Duplicate.CardZoneTransitionTriggers.Add(Trigger);
	TestEqual(TEXT("duplicate TriggerId rejected"),
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ Duplicate })).Reason,
		FString(TEXT("duplicate_card_zone_transition_trigger_id")));
	return true;
}

WB_ZONE_TRIGGER_TEST(FWBCardZoneTransitionTriggerSelfTest,
	"Wandbound.CardZoneTransitionTrigger.Self.EnterAndLeaveHistorical")
bool FWBCardZoneTransitionTriggerSelfTest::RunTest(const FString&)
{
	auto RunEnter = []()
	{
		FWBGameStateData State = MakeState();
		AddCard(State, PlayerA, EWBCardZone::Hand, TEXT("self_a1"), TEXT("self_a"));
		AddCard(State, PlayerA, EWBCardZone::Hand, TEXT("self_a2"), TEXT("self_a"));
		AddCard(State, PlayerA, EWBCardZone::Deck, TEXT("draw_1"), TEXT("filler"));
		FWBCardDefinition Self = MakeDefinition(TEXT("self_a"));
		FWBCardZoneTransitionTriggerDefinition Trigger = MakeTrigger(
			TEXT("when_sent"), EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf);
		Trigger.Filter.bRequireDestinationZone = true;
		Trigger.Filter.RequiredDestinationZone = EWBCardZone::Discard;
		Self.CardZoneTransitionTriggers.Add(Trigger);
		const FWBCardDefinitionRepository Repository = MakeRepository(
			{ Self, MakeDefinition(TEXT("filler")) });
		const FWBCardLifecycleResult Root = Transfer(State, PlayerA,
			EWBCardZone::Hand, EWBCardZone::Discard, TEXT("self_a1"),
			EWBCardZoneTransitionCause::Rule);
		const FWBCardZoneTransitionTriggerResult Result =
			WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
				State, Repository, Root.TransitionEvents);
		return TTuple<FWBGameStateData, FWBCardLifecycleResult,
			FWBCardZoneTransitionTriggerResult>(State, Root, Result);
	};
	const auto First = RunEnter();
	const auto Second = RunEnter();
	const FWBGameStateData& State = First.Get<0>();
	const FWBCardZoneTransitionTriggerResult& Result = First.Get<2>();
	TestTrue(TEXT("root succeeds"), First.Get<1>().bOk);
	TestTrue(TEXT("trigger succeeds"), Result.bOk);
	TestEqual(TEXT("one trigger"), Result.ResolvedTriggerCount, 1);
	TestEqual(TEXT("one draw"), Result.DrawnCardCount, 1);
	TestEqual(TEXT("root plus nested processed"), Result.ProcessedTransitionCount, 2);
	TestEqual(TEXT("exact source"),
		Result.CollectedTriggers[0].SourceSnapshot.SourceCardInstanceId,
		FString(TEXT("self_a1")));
	TestNotEqual(TEXT("duplicate not substituted"),
		Result.CollectedTriggers[0].SourceSnapshot.SourceCardInstanceId,
		FString(TEXT("self_a2")));
	TestEqual(TEXT("self scope"), Result.CollectedTriggers[0].SourceSnapshot.SourceScope,
		EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf);
	TestEqual(TEXT("snapshot eligibility"),
		Result.CollectedTriggers[0].SourceSnapshot.EligibilityPolicy,
		EWBTriggerEligibilityPolicy::SnapshotAtCollection);
	TestEqual(TEXT("no fake residence"),
		Result.CollectedTriggers[0].SourceSnapshot.ResidenceZoneAtCollection,
		EWBCardZone::Unknown);
	TestTrue(TEXT("source remains Discard"),
		FindExact(State, PlayerA, EWBCardZone::Discard, TEXT("self_a1")) != nullptr);
	TestTrue(TEXT("duplicate remains Hand"),
		FindExact(State, PlayerA, EWBCardZone::Hand, TEXT("self_a2")) != nullptr);
	TestTrue(TEXT("draw reaches Hand"),
		FindExact(State, PlayerA, EWBCardZone::Hand, TEXT("draw_1")) != nullptr);
	TestEqual(TEXT("nested source Deck"), Result.NestedTransitionEvents[0].SourceZone,
		EWBCardZone::Deck);
	TestEqual(TEXT("nested destination Hand"),
		Result.NestedTransitionEvents[0].DestinationZone, EWBCardZone::Hand);
	TestEqual(TEXT("nested cause Effect"), Result.NestedTransitionEvents[0].Cause,
		EWBCardZoneTransitionCause::Effect);
	TestEqual(TEXT("trigger trace count"), CountTraceKind(Result.TraceEvents,
		FName(TEXT("card_zone_transition_triggered"))), 1);
	TestEqual(TEXT("resolved trace count"), CountTraceKind(Result.TraceEvents,
		FName(TEXT("card_zone_transition_trigger_resolved"))), 1);
	const FString PublicTrace = WBReplayTrace::SerializeEvents(Result.TraceEvents);
	for (const FString& Token : { FString(TEXT("self_a1")), FString(TEXT("draw_1")),
		FString(TEXT("card_zone_trigger:")), FString(TEXT("zone_index")),
		FString(TEXT("card_instance_id")), FString(TEXT("card_id")) })
	{
		TestFalse(FString::Printf(TEXT("public trace redacts %s"), *Token),
			PublicTrace.Contains(Token));
	}
	TestEqual(TEXT("state digest deterministic"),
		WBProductionMatchReplay::BuildGameStateDigest(State),
		WBProductionMatchReplay::BuildGameStateDigest(Second.Get<0>()));
	TestEqual(TEXT("trace digest deterministic"),
		WBProductionMatchReplay::BuildTraceDigest(Result.TraceEvents),
		WBProductionMatchReplay::BuildTraceDigest(Second.Get<2>().TraceEvents));
	TestEqual(TEXT("stable trigger deterministic"),
		Result.CollectedTriggers[0].StableTriggerId,
		Second.Get<2>().CollectedTriggers[0].StableTriggerId);

	FWBCardDefinition LeaveDefinition = MakeDefinition(TEXT("self_leave"));
	FWBCardZoneTransitionTriggerDefinition LeaveTrigger = MakeTrigger(
		TEXT("when_leave"), EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf);
	LeaveTrigger.Filter.bRequireSourceZone = true;
	LeaveTrigger.Filter.RequiredSourceZone = EWBCardZone::Discard;
	LeaveDefinition.CardZoneTransitionTriggers.Add(LeaveTrigger);
	const FWBCardDefinitionRepository LeaveRepository = MakeRepository(
		{ LeaveDefinition, MakeDefinition(TEXT("filler")) });
	for (const EWBCardZone Destination : { EWBCardZone::Hand, EWBCardZone::Deck })
	{
		FWBGameStateData LeaveState = MakeState();
		AddCard(LeaveState, PlayerA, EWBCardZone::Discard,
			TEXT("leave_source"), TEXT("self_leave"));
		AddCard(LeaveState, PlayerA, EWBCardZone::Deck,
			TEXT("draw_first"), TEXT("filler"));
		const FWBCardLifecycleResult LeaveRoot = Transfer(LeaveState, PlayerA,
			EWBCardZone::Discard, Destination, TEXT("leave_source"),
			EWBCardZoneTransitionCause::Effect);
		const FString HistoricalId = LeaveRoot.TransitionEvents[0].EventIdentity.EventId;
		const FWBCardZoneTransitionTriggerResult LeaveResult =
			WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
				LeaveState, LeaveRepository, LeaveRoot.TransitionEvents);
		TestTrue(TEXT("self leave root succeeds"), LeaveRoot.bOk);
		TestTrue(TEXT("self leave resolves"), LeaveResult.bOk);
		TestEqual(TEXT("self leave count"), LeaveResult.ResolvedTriggerCount, 1);
		TestEqual(TEXT("historical source is Discard"),
			LeaveResult.CollectedTriggers[0].TransitionSnapshot.SourceZone,
			EWBCardZone::Discard);
		TestEqual(TEXT("historical destination fixed"),
			LeaveResult.CollectedTriggers[0].TransitionSnapshot.DestinationZone,
			Destination);
		TestEqual(TEXT("historical id fixed"),
			LeaveResult.CollectedTriggers[0].TransitionSnapshot.EventIdentity.EventId,
			HistoricalId);
		TestEqual(TEXT("exact leave source fixed"),
			LeaveResult.CollectedTriggers[0].SourceSnapshot.SourceCardInstanceId,
			FString(TEXT("leave_source")));
		TestTrue(TEXT("source no longer required in Discard"),
			FindExact(LeaveState, PlayerA, EWBCardZone::Discard,
				TEXT("leave_source")) == nullptr);
		TestFalse(TEXT("no reaction window"), LeaveState.HasOpenReactionWindow());
		TestFalse(TEXT("no pending choice"), LeaveState.HasPendingPrivateCardChoice());
	}
	return true;
}

WB_ZONE_TRIGGER_TEST(FWBCardZoneTransitionTriggerResidentTest,
	"Wandbound.CardZoneTransitionTrigger.Resident.ExactOwnedOrdering")
bool FWBCardZoneTransitionTriggerResidentTest::RunTest(const FString&)
{
	FWBCardDefinition Observer = MakeDefinition(TEXT("observer"));
	FWBCardZoneTransitionTriggerDefinition Observe = MakeTrigger(
		TEXT("observe_enter"),
		EWBCardZoneTransitionTriggerSourceScope::ResidentDiscardObserver);
	Observe.Filter.bRequireDestinationZone = true;
	Observe.Filter.RequiredDestinationZone = EWBCardZone::Discard;
	Observer.CardZoneTransitionTriggers.Add(Observe);
	const FWBCardDefinitionRepository Repository = MakeRepository(
		{ Observer, MakeDefinition(TEXT("subject")), MakeDefinition(TEXT("filler")) });
	FWBGameStateData State = MakeState();
	AddCard(State, PlayerA, EWBCardZone::Discard, TEXT("observer_2"), TEXT("observer"));
	AddCard(State, PlayerA, EWBCardZone::Discard, TEXT("observer_1"), TEXT("observer"));
	AddCard(State, PlayerB, EWBCardZone::Discard, TEXT("opponent_observer"), TEXT("observer"));
	AddCard(State, PlayerA, EWBCardZone::Hand, TEXT("subject_1"), TEXT("subject"));
	AddCard(State, PlayerA, EWBCardZone::Deck, TEXT("draw_a"), TEXT("filler"));
	AddCard(State, PlayerA, EWBCardZone::Deck, TEXT("draw_b"), TEXT("filler"));
	const FWBCardLifecycleResult Root = Transfer(State, PlayerA,
		EWBCardZone::Hand, EWBCardZone::Discard, TEXT("subject_1"),
		EWBCardZoneTransitionCause::Cost);
	const FWBCardZoneTransitionTriggerResult Result =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			State, Repository, Root.TransitionEvents);
	TestTrue(TEXT("resident resolution succeeds"), Result.bOk);
	TestEqual(TEXT("two observers fire"), Result.ResolvedTriggerCount, 2);
	TestEqual(TEXT("two draws"), Result.DrawnCardCount, 2);
	TestEqual(TEXT("two exact sources"), Result.CollectedTriggers.Num(), 2);
	TestEqual(TEXT("first source by ZoneIndex"),
		Result.CollectedTriggers[0].SourceSnapshot.SourceCardInstanceId,
		FString(TEXT("observer_2")));
	TestEqual(TEXT("second source by ZoneIndex"),
		Result.CollectedTriggers[1].SourceSnapshot.SourceCardInstanceId,
		FString(TEXT("observer_1")));
	TestNotEqual(TEXT("duplicate CardIds remain exact"),
		Result.CollectedTriggers[0].SourceSnapshot.SourceCardInstanceId,
		Result.CollectedTriggers[1].SourceSnapshot.SourceCardInstanceId);
	for (int32 Index = 0; Index < Result.CollectedTriggers.Num(); ++Index)
	{
		const FWBCardZoneTransitionTriggerSourceSnapshot& Source =
			Result.CollectedTriggers[Index].SourceSnapshot;
		TestTrue(FString::Printf(TEXT("source %d valid"), Index), Source.IsValid());
		TestEqual(FString::Printf(TEXT("source %d owner"), Index),
			Source.OwnerPlayerId, PlayerA);
		TestEqual(FString::Printf(TEXT("source %d residence"), Index),
			Source.ResidenceZoneAtCollection, EWBCardZone::Discard);
		TestEqual(FString::Printf(TEXT("source %d index"), Index),
			Source.ZoneIndexAtCollection, Index);
		TestEqual(FString::Printf(TEXT("source %d TriggerId"), Index),
			Source.TriggerId, FString(TEXT("observe_enter")));
		TestEqual(FString::Printf(TEXT("source %d scope"), Index), Source.SourceScope,
			EWBCardZoneTransitionTriggerSourceScope::ResidentDiscardObserver);
	}
	TestTrue(TEXT("first observer remains"),
		FindExact(State, PlayerA, EWBCardZone::Discard, TEXT("observer_1")) != nullptr);
	TestTrue(TEXT("second observer remains"),
		FindExact(State, PlayerA, EWBCardZone::Discard, TEXT("observer_2")) != nullptr);
	TestTrue(TEXT("opponent remains"),
		FindExact(State, PlayerB, EWBCardZone::Discard, TEXT("opponent_observer")) != nullptr);
	TestFalse(TEXT("opponent source not collected"),
		Result.CollectedTriggers.ContainsByPredicate([](
			const FWBCardZoneTransitionTriggerInstance& Item)
		{
			return Item.SourceSnapshot.SourceCardInstanceId == TEXT("opponent_observer");
		}));
	TestEqual(TEXT("event subject preserved"),
		Result.CollectedTriggers[0].TransitionSnapshot.CardInstanceId,
		FString(TEXT("subject_1")));
	TestNotEqual(TEXT("event subject differs from observer"),
		Result.CollectedTriggers[0].TransitionSnapshot.CardInstanceId,
		Result.CollectedTriggers[0].SourceSnapshot.SourceCardInstanceId);
	TestEqual(TEXT("historical Cost cause preserved"),
		Result.CollectedTriggers[0].TransitionSnapshot.Cause,
		EWBCardZoneTransitionCause::Cost);
	TestEqual(TEXT("root and nested events processed"), Result.ProcessedTransitionCount, 3);
	TestEqual(TEXT("two nested transition events"), Result.NestedTransitionEvents.Num(), 2);
	TestEqual(TEXT("two triggered traces"), CountTraceKind(Result.TraceEvents,
		FName(TEXT("card_zone_transition_triggered"))), 2);
	TestEqual(TEXT("two resolved traces"), CountTraceKind(Result.TraceEvents,
		FName(TEXT("card_zone_transition_trigger_resolved"))), 2);

	FWBCardDefinition Both = MakeDefinition(TEXT("both"));
	FWBCardZoneTransitionTriggerDefinition Self = MakeTrigger(
		TEXT("a_self"), EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf);
	Self.Filter.bRequireDestinationZone = true;
	Self.Filter.RequiredDestinationZone = EWBCardZone::Discard;
	FWBCardZoneTransitionTriggerDefinition Resident = MakeTrigger(
		TEXT("b_resident"),
		EWBCardZoneTransitionTriggerSourceScope::ResidentDiscardObserver);
	Resident.Filter = Self.Filter;
	Both.CardZoneTransitionTriggers = { Self, Resident };
	const FWBCardDefinitionRepository BothRepository = MakeRepository(
		{ Both, MakeDefinition(TEXT("filler")) });
	FWBGameStateData BothState = MakeState();
	AddCard(BothState, PlayerA, EWBCardZone::Hand, TEXT("both_1"), TEXT("both"));
	AddCard(BothState, PlayerA, EWBCardZone::Deck, TEXT("both_draw_1"), TEXT("filler"));
	AddCard(BothState, PlayerA, EWBCardZone::Deck, TEXT("both_draw_2"), TEXT("filler"));
	const FWBCardLifecycleResult BothRoot = Transfer(BothState, PlayerA,
		EWBCardZone::Hand, EWBCardZone::Discard, TEXT("both_1"),
		EWBCardZoneTransitionCause::Rule);
	const FWBCardZoneTransitionTriggerResult BothResult =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			BothState, BothRepository, BothRoot.TransitionEvents);
	TestTrue(TEXT("newly entered observer resolves"), BothResult.bOk);
	TestEqual(TEXT("self and resident are separate"), BothResult.ResolvedTriggerCount, 2);
	TestEqual(TEXT("self first"), BothResult.CollectedTriggers[0].SourceSnapshot.SourceScope,
		EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf);
	TestEqual(TEXT("resident second"), BothResult.CollectedTriggers[1].SourceSnapshot.SourceScope,
		EWBCardZoneTransitionTriggerSourceScope::ResidentDiscardObserver);
	TestEqual(TEXT("same exact source allowed"),
		BothResult.CollectedTriggers[0].SourceSnapshot.SourceCardInstanceId,
		BothResult.CollectedTriggers[1].SourceSnapshot.SourceCardInstanceId);
	TestNotEqual(TEXT("separate TriggerIds"),
		BothResult.CollectedTriggers[0].SourceSnapshot.TriggerId,
		BothResult.CollectedTriggers[1].SourceSnapshot.TriggerId);
	TestEqual(TEXT("self fixed slot"), BothResult.CollectedTriggers[0].CollectionOrder, 0);
	TestEqual(TEXT("resident fixed slot"), BothResult.CollectedTriggers[1].CollectionOrder, 1);

	FWBGameStateData LegacyState = MakeState();
	AddCard(LegacyState, PlayerA, EWBCardZone::Hand,
		TEXT("legacy_1"), TEXT("legacy_definition_not_loaded"));
	const FWBCardLifecycleResult LegacyRoot = Transfer(LegacyState, PlayerA,
		EWBCardZone::Hand, EWBCardZone::Discard, TEXT("legacy_1"),
		EWBCardZoneTransitionCause::Effect);
	const FWBCardZoneTransitionTriggerResult LegacyResult =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			LegacyState, BothRepository, LegacyRoot.TransitionEvents);
	TestTrue(TEXT("legacy card without loaded definition is ignored"),
		LegacyResult.bOk);
	TestEqual(TEXT("legacy card collects no trigger"),
		LegacyResult.ResolvedTriggerCount, 0);
	TestEqual(TEXT("legacy card emits no trigger trace"),
		LegacyResult.TraceEvents.Num(), 0);
	return true;
}

WB_ZONE_TRIGGER_TEST(FWBCardZoneTransitionTriggerQueueTest,
	"Wandbound.CardZoneTransitionTrigger.Queue.TransactionTerminalAndExclusions")
bool FWBCardZoneTransitionTriggerQueueTest::RunTest(const FString&)
{
	FWBCardDefinition Observer = MakeDefinition(TEXT("draw_observer"));
	Observer.CardZoneTransitionTriggers.Add(MakeTrigger(
		TEXT("observe_all"),
		EWBCardZoneTransitionTriggerSourceScope::ResidentDiscardObserver));
	const FWBCardDefinitionRepository Repository = MakeRepository(
		{ Observer, MakeDefinition(TEXT("subject")), MakeDefinition(TEXT("filler")) });
	FWBGameStateData State = MakeState();
	AddCard(State, PlayerA, EWBCardZone::Discard,
		TEXT("observer"), TEXT("draw_observer"));
	AddCard(State, PlayerA, EWBCardZone::Hand,
		TEXT("subject"), TEXT("subject"));
	for (int32 Index = 0; Index < 8; ++Index)
	{
		AddCard(State, PlayerA, EWBCardZone::Deck,
			FString::Printf(TEXT("draw_%d"), Index), TEXT("filler"));
	}
	const FWBCardLifecycleResult Root = Transfer(State, PlayerA,
		EWBCardZone::Hand, EWBCardZone::Discard, TEXT("subject"),
		EWBCardZoneTransitionCause::Rule);
	const FString StateAfterRoot = WBProductionMatchReplay::BuildGameStateDigest(State);
	const FWBCardZoneTransitionTriggerResult Guarded =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			State, Repository, Root.TransitionEvents, 3);
	TestFalse(TEXT("guard fails closed"), Guarded.bOk);
	TestEqual(TEXT("guard reason"), Guarded.Reason,
		FString(TEXT("card_zone_transition_trigger_guard_exceeded")));
	TestEqual(TEXT("guard state rollback"),
		WBProductionMatchReplay::BuildGameStateDigest(State), StateAfterRoot);
	TestEqual(TEXT("guard traces cleared"), Guarded.TraceEvents.Num(), 0);
	TestEqual(TEXT("guard nested events cleared"), Guarded.NestedTransitionEvents.Num(), 0);
	TestEqual(TEXT("guard sources cleared"), Guarded.CollectedTriggers.Num(), 0);
	TestEqual(TEXT("guard resolved count cleared"), Guarded.ResolvedTriggerCount, 0);
	TestEqual(TEXT("guard draw count cleared"), Guarded.DrawnCardCount, 0);

	FWBGameStateData EmptyDeck = MakeState();
	AddCard(EmptyDeck, PlayerA, EWBCardZone::Discard,
		TEXT("observer"), TEXT("draw_observer"));
	AddCard(EmptyDeck, PlayerA, EWBCardZone::Hand,
		TEXT("subject"), TEXT("subject"));
	const FWBCardLifecycleResult EmptyRoot = Transfer(EmptyDeck, PlayerA,
		EWBCardZone::Hand, EWBCardZone::Discard, TEXT("subject"),
		EWBCardZoneTransitionCause::Rule);
	const FString EmptyDigest = WBProductionMatchReplay::BuildGameStateDigest(EmptyDeck);
	const FWBCardZoneTransitionTriggerResult EmptyResult =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			EmptyDeck, Repository, EmptyRoot.TransitionEvents);
	TestFalse(TEXT("impossible mandatory draw fails"), EmptyResult.bOk);
	TestEqual(TEXT("empty Deck semantics preserved"), EmptyResult.Reason,
		FString(TEXT("deck_empty")));
	TestEqual(TEXT("failed draw state rollback"),
		WBProductionMatchReplay::BuildGameStateDigest(EmptyDeck), EmptyDigest);
	TestEqual(TEXT("failed draw traces cleared"), EmptyResult.TraceEvents.Num(), 0);
	TestEqual(TEXT("failed draw sources cleared"), EmptyResult.CollectedTriggers.Num(), 0);

	FWBGameStateData Terminal = EmptyDeck;
	Terminal.bGameOver = true;
	const FWBCardZoneTransitionTriggerResult TerminalResult =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			Terminal, Repository, EmptyRoot.TransitionEvents);
	TestTrue(TEXT("terminal suppression succeeds"), TerminalResult.bOk);
	TestEqual(TEXT("terminal collects zero"), TerminalResult.CollectedTriggers.Num(), 0);
	TestEqual(TEXT("terminal draws zero"), TerminalResult.DrawnCardCount, 0);
	TestEqual(TEXT("terminal traces zero"), TerminalResult.TraceEvents.Num(), 0);

	FWBGameStateData ExtractState = MakeState();
	AddCard(ExtractState, PlayerA, EWBCardZone::Hand,
		TEXT("extract"), TEXT("subject"));
	const FWBCardLifecycleResult Extract = WBCardLifecycle::RemoveExactCardFromHand(
		ExtractState, PlayerA, TEXT("extract"));
	TestTrue(TEXT("ExtractExact succeeds"), Extract.bOk);
	TestEqual(TEXT("ExtractExact excluded"), Extract.TransitionEvents.Num(), 0);
	TestFalse(TEXT("ExtractExact cannot enter trigger queue"),
		Extract.TransitionEvents.Num() > 0);

	FWBGameStateData CauseState = MakeState();
	AddCard(CauseState, PlayerA, EWBCardZone::Hand,
		TEXT("cause_subject"), TEXT("subject"));
	const FWBCardLifecycleResult CauseRoot = Transfer(CauseState, PlayerA,
		EWBCardZone::Hand, EWBCardZone::Discard, TEXT("cause_subject"),
		EWBCardZoneTransitionCause::Cost);
	for (const EWBCardZoneTransitionCause Cause : {
		EWBCardZoneTransitionCause::Cost,
		EWBCardZoneTransitionCause::Rule,
		EWBCardZoneTransitionCause::Draw })
	{
		FWBCardZoneTransitionTriggerFilter Filter;
		Filter.bRequireCause = true;
		Filter.RequiredCause = Cause;
		TestEqual(TEXT("explicit cause filter is historical"),
			WBCardZoneTransitionTrigger::MatchesFilter(Filter, CauseRoot.TransitionEvents[0]),
			Cause == EWBCardZoneTransitionCause::Cost);
	}
	return true;
}

WB_ZONE_TRIGGER_TEST(FWBCardZoneTransitionTriggerBoundaryTest,
	"Wandbound.CardZoneTransitionTrigger.Boundary.PrivacyAndNoParallelSystems")
bool FWBCardZoneTransitionTriggerBoundaryTest::RunTest(const FString&)
{
	auto LoadSource = [](const FString& RelativePath)
	{
		FString Text;
		FFileHelper::LoadFileToString(
			Text, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
		return Text;
	};
	const FString Authority = LoadSource(
		TEXT("Source/WandboundCore/Private/WBCardZoneTransitionTrigger.cpp"));
	const FString Mutation = LoadSource(
		TEXT("Source/WandboundCore/Private/WBCardZoneMutation.cpp"));
	const FString Coordinator = LoadSource(
		TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp"));
	const FString ActionCodec = LoadSource(
		TEXT("Source/WandboundCore/Private/WBActionCodec.cpp"));
	TestTrue(TEXT("authority source loads"), !Authority.IsEmpty());
	for (const FString& Token : { FString(TEXT("WBCardLifecycle::DrawOneCard")),
		FString(TEXT("FWBCardZoneTransitionSnapshot")),
		FString(TEXT("ResidentDiscardObserver")),
		FString(TEXT("SourceCardInstanceId")),
		FString(TEXT("SnapshotAtCollection")),
		FString(TEXT("card_zone_transition_trigger_guard_exceeded")) })
	{
		TestTrue(FString::Printf(TEXT("authority contains %s"), *Token),
			Authority.Contains(Token));
	}
	for (const FString& Token : { FString(TEXT("PassResponse")),
		FString(TEXT("OpenReactionWindow")), FString(TEXT("Caster")),
		FString(TEXT("WBActionCodec")), FString(TEXT("FGuid")),
		FString(TEXT("FDateTime")), FString(TEXT(".Deck.Remove")),
		FString(TEXT(".Hand.Add")), FString(TEXT(".Discard.Add")) })
	{
		TestFalse(FString::Printf(TEXT("authority excludes %s"), *Token),
			Authority.Contains(Token));
	}
	TestFalse(TEXT("structural mutation scans no triggers"),
		Mutation.Contains(TEXT("CardZoneTransitionTrigger")));
	TestTrue(TEXT("coordinator consumes authority"),
		Coordinator.Contains(TEXT("ResolveCardZoneTransitionTriggers")));
	TestFalse(TEXT("ActionCodec has no trigger type"),
		ActionCodec.Contains(TEXT("CardZoneTransitionTrigger")));
	TestFalse(TEXT("ActionCodec has no trigger id"),
		ActionCodec.Contains(TEXT("card_zone_trigger")));
	TestEqual(TEXT("replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_ZONE_TRIGGER_TEST(FWBCardZoneTransitionTriggerProductionFixtureTest,
	"Wandbound.CardZoneTransitionTrigger.Production.ParserAndSmoke")
bool FWBCardZoneTransitionTriggerProductionFixtureTest::RunTest(const FString&)
{
	const FString ManifestPath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/Replay/CardZoneTransitionTriggerFixture/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Database =
		WBProductionCardDatabase::LoadManifestSuite(ManifestPath);
	TestTrue(TEXT("production fixture parses"), Database.bOk);
	TestTrue(TEXT("production snapshot exists"), Database.Snapshot.IsValid());
	if (!Database.bOk || !Database.Snapshot.IsValid())
	{
		AddError(Database.Reason);
		return false;
	}
	TestEqual(TEXT("six synthetic definitions"), Database.Snapshot->Records.Num(), 6);
	TestEqual(TEXT("replay schema unchanged"), WBProductionMatchReplay::SchemaVersion, 1);
	for (const FString& CardId : { FString(TEXT("zone_trigger_self_enter")),
		FString(TEXT("zone_trigger_self_leave")),
		FString(TEXT("zone_trigger_observer")) })
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Database.Snapshot->CoreRepository, CardId);
		TestTrue(FString::Printf(TEXT("%s parsed"), *CardId), Lookup.bFound);
		TestEqual(FString::Printf(TEXT("%s has one trigger"), *CardId),
			Lookup.Definition.CardZoneTransitionTriggers.Num(), 1);
	}
	const FWBProductionCardZoneTransitionTriggerSmokeResult Smoke =
		WBProductionCardZoneTransitionTriggerSmoke::Run();
	TestTrue(TEXT("production smoke succeeds"), Smoke.bOk);
	TestEqual(TEXT("six triggers resolved"), Smoke.TriggerCount, 6);
	TestEqual(TEXT("nine transitions processed"), Smoke.TransitionCount, 9);
	TestTrue(TEXT("state digest produced"), !Smoke.FinalStateDigest.IsEmpty());
	TestTrue(TEXT("trace digest produced"), !Smoke.FinalTraceDigest.IsEmpty());
	return true;
}

#undef WB_ZONE_TRIGGER_TEST

#endif
