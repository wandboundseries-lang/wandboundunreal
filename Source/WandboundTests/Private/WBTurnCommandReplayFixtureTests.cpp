#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FString TurnCommandReplayFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

bool LoadTurnCommandReplayFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *TurnCommandReplayFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

bool TryGetExpectedObject(const TSharedPtr<FJsonObject>& Fixture, TSharedPtr<FJsonObject>& OutExpectedObject)
{
	const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expected"), ExpectedObject)
		|| ExpectedObject == nullptr
		|| !ExpectedObject->IsValid())
	{
		return false;
	}

	OutExpectedObject = *ExpectedObject;
	return true;
}

bool ReadExpectedResult(const TSharedPtr<FJsonObject>& Fixture, bool& bOutExpectedOk, FString& OutExpectedReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject))
	{
		return false;
	}

	if (!ExpectedObject->TryGetBoolField(TEXT("ok"), bOutExpectedOk))
	{
		return false;
	}

	if (!ExpectedObject->TryGetStringField(TEXT("reason"), OutExpectedReason))
	{
		OutExpectedReason.Reset();
	}

	return true;
}

bool TraceContainsKind(const TArray<FWBTraceEvent>& TraceEvents, const FName Kind)
{
	for (const FWBTraceEvent& TraceEvent : TraceEvents)
	{
		if (TraceEvent.Kind == Kind)
		{
			return true;
		}
	}

	return false;
}

bool TraceContainsStatusTick(const TArray<FWBTraceEvent>& TraceEvents, const FName StatusId)
{
	for (const FWBTraceEvent& TraceEvent : TraceEvents)
	{
		if (TraceEvent.Kind == FName(TEXT("status_tick")) && TraceEvent.StatusId == StatusId)
		{
			return true;
		}
	}

	return false;
}

bool RunFixtureOperation(
	const FString& FixtureName,
	FAutomationTestBase& Test,
	TSharedPtr<FJsonObject>& OutFixture,
	FWBGameStateData& OutState,
	FWBApplyActionResult& OutResult,
	EWBFixtureOperationKind& OutOperationKind)
{
	Test.TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadTurnCommandReplayFixture(FixtureName, OutFixture));
	if (!OutFixture.IsValid())
	{
		return false;
	}

	FString StateReason;
	const bool bStateBuilt = BuildGameStateFromFixture(OutFixture, OutState, StateReason);
	Test.TestTrue(*FString::Printf(TEXT("%s initial state builds: %s"), *FixtureName, *StateReason), bStateBuilt);
	if (!bStateBuilt)
	{
		return false;
	}

	FString ApplyReason;
	const bool bApplied = ApplyFixtureOperation(OutFixture, OutState, OutResult, OutOperationKind, ApplyReason);
	Test.TestTrue(*FString::Printf(TEXT("%s operation applies/parses: %s"), *FixtureName, *ApplyReason), bApplied);
	return bApplied;
}

bool CheckCommonFixtureExpectations(
	const FString& FixtureName,
	FAutomationTestBase& Test,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	const FWBApplyActionResult& Result)
{
	bool bExpectedOk = false;
	FString ExpectedReason;
	Test.TestTrue(*FString::Printf(TEXT("%s expected result reads"), *FixtureName), ReadExpectedResult(Fixture, bExpectedOk, ExpectedReason));
	Test.TestEqual(*FString::Printf(TEXT("%s result ok"), *FixtureName), Result.bOk, bExpectedOk);
	Test.TestEqual(*FString::Printf(TEXT("%s result reason"), *FixtureName), Result.Reason, ExpectedReason);

	FString TurnStateReason;
	const bool bTurnStateMatches = ExpectFinalTurnState(Fixture, State, TurnStateReason);
	Test.TestTrue(*FString::Printf(TEXT("%s turn state matches: %s"), *FixtureName, *TurnStateReason), bTurnStateMatches);

	FString ResourceReason;
	const bool bResourcesMatch = ExpectPlayerResources(Fixture, State, ResourceReason);
	Test.TestTrue(*FString::Printf(TEXT("%s resources match: %s"), *FixtureName, *ResourceReason), bResourcesMatch);

	FString UnitReason;
	const bool bUnitsMatch = ExpectUnitStatusSummary(Fixture, State, UnitReason);
	Test.TestTrue(*FString::Printf(TEXT("%s unit summary matches: %s"), *FixtureName, *UnitReason), bUnitsMatch);

	FString TraceReason;
	const bool bTraceOrderMatches = ExpectTraceOrder(Fixture, Result.TraceEvents, TraceReason);
	Test.TestTrue(*FString::Printf(TEXT("%s trace order matches: %s"), *FixtureName, *TraceReason), bTraceOrderMatches);

	return bTurnStateMatches && bResourcesMatch && bUnitsMatch && bTraceOrderMatches;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandReplayFixtureBasicEndTurnOnlyTest, "Wandbound.Core.TurnCommandReplay.BasicEndTurnOnlyFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandReplayFixtureBasicEndTurnOnlyTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("replay_turn_command_basic_end_turn_only.json");
	TSharedPtr<FJsonObject> Fixture;
	FWBGameStateData State;
	FWBApplyActionResult Result;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	if (!RunFixtureOperation(FixtureName, *this, Fixture, State, Result, OperationKind))
	{
		return false;
	}

	TestTrue(TEXT("Fixture uses turn command path"), OperationKind == EWBFixtureOperationKind::ApplyTurnCommand);
	CheckCommonFixtureExpectations(FixtureName, *this, Fixture, State, Result);
	TestEqual(TEXT("Basic command emits one trace"), Result.TraceEvents.Num(), 1);
	TestTrue(TEXT("Basic command has no turn transition trace"), !TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestTrue(TEXT("Basic command has no status trace"), !TraceContainsKind(Result.TraceEvents, FName(TEXT("status_tick"))));
	TestTrue(TEXT("Basic command has no resource setup trace"), !TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_start_resource_setup"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandReplayFixtureFullTransitionTest, "Wandbound.Core.TurnCommandReplay.FullTransitionFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandReplayFixtureFullTransitionTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("replay_turn_command_full_transition_burn_poison_setup.json");
	TSharedPtr<FJsonObject> Fixture;
	FWBGameStateData State;
	FWBApplyActionResult Result;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	if (!RunFixtureOperation(FixtureName, *this, Fixture, State, Result, OperationKind))
	{
		return false;
	}

	TestTrue(TEXT("Fixture uses turn command path"), OperationKind == EWBFixtureOperationKind::ApplyTurnCommand);
	CheckCommonFixtureExpectations(FixtureName, *this, Fixture, State, Result);
	TestEqual(TEXT("Full command trace count"), Result.TraceEvents.Num(), 7);
	TestTrue(TEXT("Full command has Burn tick"), TraceContainsStatusTick(Result.TraceEvents, FName(TEXT("Burn"))));
	TestTrue(TEXT("Full command has Poison tick"), TraceContainsStatusTick(Result.TraceEvents, FName(TEXT("Poison"))));
	TestEqual(TEXT("Next player explicit MP applied"), State.GetPlayerById(1)->RemainingMP, 4);
	TestEqual(TEXT("Next player explicit roll recorded"), State.GetPlayerById(1)->LastMPRoll, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandReplayFixtureInvalidRollNoMutationTest, "Wandbound.Core.TurnCommandReplay.InvalidRollNoMutationFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandReplayFixtureInvalidRollNoMutationTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("replay_turn_command_full_transition_invalid_roll_no_mutation.json");
	TSharedPtr<FJsonObject> Fixture;
	FWBGameStateData State;
	FWBApplyActionResult Result;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	if (!RunFixtureOperation(FixtureName, *this, Fixture, State, Result, OperationKind))
	{
		return false;
	}

	TestTrue(TEXT("Fixture uses turn command path"), OperationKind == EWBFixtureOperationKind::ApplyTurnCommand);
	CheckCommonFixtureExpectations(FixtureName, *this, Fixture, State, Result);
	TestFalse(TEXT("Invalid roll fails"), Result.bOk);
	TestEqual(TEXT("Invalid roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Invalid roll emits no traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandReplayFixtureActionReplayStillBasicTest, "Wandbound.Core.TurnCommandReplay.ActionReplayStillBasic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandReplayFixtureActionReplayStillBasicTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("replay_turn_command_does_not_change_action_replay.json");
	TSharedPtr<FJsonObject> Fixture;
	FWBGameStateData State;
	FWBApplyActionResult Result;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	if (!RunFixtureOperation(FixtureName, *this, Fixture, State, Result, OperationKind))
	{
		return false;
	}

	FWBAction EndTurnAction;
	EndTurnAction.Type = EWBActionType::EndTurn;
	EndTurnAction.PlayerId = 0;
	TestEqual(TEXT("EndTurn action ID remains stable"), WBActionCodec::MakeActionId(EndTurnAction), FString(TEXT("end_turn:p0")));
	FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(WBActionCodec::SerializeAction(EndTurnAction), State);
	TestTrue(TEXT("EndTurn action still decodes through WBActionCodec"), DecodeResult.bOk);
	TestTrue(TEXT("Decoded EndTurn action remains EndTurn"), DecodeResult.Action.Type == EWBActionType::EndTurn);

	TestTrue(TEXT("Fixture uses player action path"), OperationKind == EWBFixtureOperationKind::ApplyAction);
	CheckCommonFixtureExpectations(FixtureName, *this, Fixture, State, Result);
	TestTrue(TEXT("Action replay has no turn transition trace"), !TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestTrue(TEXT("Action replay has no status trace"), !TraceContainsKind(Result.TraceEvents, FName(TEXT("status_tick"))));
	TestTrue(TEXT("Action replay has no resource setup trace"), !TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_start_resource_setup"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandReplayLegalActionsUnaffectedTest, "Wandbound.Core.TurnCommandReplay.LegalActionsUnaffected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandReplayLegalActionsUnaffectedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Action replay fixture loads"), LoadTurnCommandReplayFixture(TEXT("replay_turn_command_does_not_change_action_replay.json"), Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(*FString::Printf(TEXT("Fixture state builds: %s"), *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));

	const TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(State);
	const TArray<FString> LegalActionIds = WBActionCodec::MakeActionIds(LegalActions);
	TestTrue(TEXT("Legal actions include EndTurn"), LegalActionIds.Contains(TEXT("end_turn:p0")));
	for (const FString& ActionId : LegalActionIds)
	{
		TestFalse(*FString::Printf(TEXT("%s is not a full transition player action"), *ActionId), ActionId.Contains(TEXT("full_transition")));
		TestFalse(*FString::Printf(TEXT("%s is not a turn transition player action"), *ActionId), ActionId.Contains(TEXT("turn_transition")));
		TestFalse(*FString::Printf(TEXT("%s is not a status tick player action"), *ActionId), ActionId.Contains(TEXT("status_tick")));
		TestFalse(*FString::Printf(TEXT("%s is not a resource setup player action"), *ActionId), ActionId.Contains(TEXT("turn_start_resource_setup")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandReplayTraceSerializationTest, "Wandbound.Core.TurnCommandReplay.TraceSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandReplayTraceSerializationTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("replay_turn_command_full_transition_burn_poison_setup.json");
	TSharedPtr<FJsonObject> Fixture;
	FWBGameStateData State;
	FWBApplyActionResult Result;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	if (!RunFixtureOperation(FixtureName, *this, Fixture, State, Result, OperationKind))
	{
		return false;
	}

	const FString TraceJson = WBReplayTrace::SerializeEvents(Result.TraceEvents);
	TestTrue(TEXT("Trace JSON includes turn transition"), TraceJson.Contains(TEXT("turn_transition")));
	TestTrue(TEXT("Trace JSON includes end-turn status ticks"), TraceJson.Contains(TEXT("end_turn_status_ticks")));
	TestTrue(TEXT("Trace JSON includes Burn status id"), TraceJson.Contains(TEXT("Burn")));
	TestTrue(TEXT("Trace JSON includes Poison status id"), TraceJson.Contains(TEXT("Poison")));
	TestTrue(TEXT("Trace JSON includes resource setup"), TraceJson.Contains(TEXT("turn_start_resource_setup")));
	TestTrue(TEXT("Trace JSON includes MP roll"), TraceJson.Contains(TEXT("mp_roll")));
	TestTrue(TEXT("Trace JSON includes remaining MP"), TraceJson.Contains(TEXT("remaining_mp")));
	TestFalse(TEXT("Trace JSON does not serialize deck contents"), TraceJson.Contains(TEXT("deck")));
	TestFalse(TEXT("Trace JSON does not serialize hand contents"), TraceJson.Contains(TEXT("hand")));
	return true;
}

#endif
