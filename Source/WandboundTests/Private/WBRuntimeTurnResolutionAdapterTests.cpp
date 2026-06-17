#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBMPRollSource.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRuntimeTurnResolutionAdapter.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeRuntimePlayer(
	const int32 PlayerId,
	const int32 RemainingMP = 0,
	const int32 LastMPRoll = 0,
	const int32 WallsLeft = 0,
	const int32 WallRemovalsLeft = 0)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = LastMPRoll;
	Player.WallsLeft = WallsLeft;
	Player.WallRemovalsLeft = WallRemovalsLeft;
	return Player;
}

FWBUnitState MakeRuntimeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 AttacksLeft = 0,
	const int32 MaxAttacksPerTurn = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = MaxAttacksPerTurn;
	return Unit;
}

FWBGameStateData MakeRuntimeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeRuntimePlayer(0, 2, 2));
	State.Players.Add(MakeRuntimePlayer(1));
	return State;
}

FWBGameStateData MakeRuntimeStatusState()
{
	FWBGameStateData State = MakeRuntimeState();
	FWBUnitState BurnUnit = MakeRuntimeUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeRuntimeUnit(2, 1, FWBTile(6, 4), 5, 5, 0, 2);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(BurnUnit);
	State.AddUnitForTest(PoisonUnit);
	return State;
}

FWBAction MakeRuntimeMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeRuntimeAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
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

bool ContainsActionType(const TArray<FWBAction>& Actions, const EWBActionType Type)
{
	for (const FWBAction& Action : Actions)
	{
		if (Action.Type == Type)
		{
			return true;
		}
	}

	return false;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString RuntimeFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadRuntimeFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *RuntimeFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

bool ReadExpectedResult(const TSharedPtr<FJsonObject>& Fixture, bool& bOutExpectedOk, FString& OutExpectedReason)
{
	const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expected"), ExpectedObject)
		|| ExpectedObject == nullptr
		|| !ExpectedObject->IsValid())
	{
		return false;
	}

	if (!(*ExpectedObject)->TryGetBoolField(TEXT("ok"), bOutExpectedOk))
	{
		return false;
	}

	if (!(*ExpectedObject)->TryGetStringField(TEXT("reason"), OutExpectedReason))
	{
		OutExpectedReason.Reset();
	}

	return true;
}

bool RunRuntimeFixture(
	const FString& FixtureName,
	FAutomationTestBase& Test,
	TSharedPtr<FJsonObject>& OutFixture,
	FWBGameStateData& OutState,
	FWBApplyActionResult& OutResult,
	int32& OutRollSourceRemainingCount)
{
	Test.TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadRuntimeFixture(FixtureName, OutFixture));
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
	const bool bApplied = ApplyRuntimeSelectedActionFixture(
		OutFixture,
		OutState,
		OutResult,
		OutRollSourceRemainingCount,
		ApplyReason);
	Test.TestTrue(*FString::Printf(TEXT("%s runtime operation applies/parses: %s"), *FixtureName, *ApplyReason), bApplied);
	return bApplied;
}

bool CheckRuntimeFixtureExpectations(
	const FString& FixtureName,
	FAutomationTestBase& Test,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	const FWBApplyActionResult& Result,
	const int32 RollSourceRemainingCount)
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

	FString RollSourceReason;
	const bool bRollSourceMatches = ExpectRuntimeRollSourceRemainingCount(Fixture, RollSourceRemainingCount, RollSourceReason);
	Test.TestTrue(*FString::Printf(TEXT("%s roll source count matches: %s"), *FixtureName, *RollSourceReason), bRollSourceMatches);

	return bTurnStateMatches && bResourcesMatch && bUnitsMatch && bTraceOrderMatches && bRollSourceMatches;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionFullEndTurnFixedRollTest, "Wandbound.Core.RuntimeTurnResolution.FullEndTurnFixedRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionFullEndTurnFixedRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeAction(EWBActionType::EndTurn, 0),
		Context);

	TestTrue(TEXT("Runtime full EndTurn succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Next player MP setup"), State.GetPlayerById(1)->RemainingMP, 4);
	TestEqual(TEXT("Next player roll recorded"), State.GetPlayerById(1)->LastMPRoll, 4);
	TestTrue(TEXT("Full transition trace appears"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestTrue(TEXT("Burn ticks"), TraceContainsStatusTick(Result.TraceEvents, FName(TEXT("Burn"))));
	TestTrue(TEXT("Poison ticks"), TraceContainsStatusTick(Result.TraceEvents, FName(TEXT("Poison"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionBasicEndTurnNoRollTest, "Wandbound.Core.RuntimeTurnResolution.BasicEndTurnNoRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionBasicEndTurnNoRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeStatusState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = false;
	Context.MPRollSource = nullptr;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeAction(EWBActionType::EndTurn, 0),
		Context);

	TestTrue(TEXT("Runtime basic EndTurn succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Burn does not tick"), State.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("Poison does not tick"), State.GetUnitById(2)->MaxHP, 5);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("end_turn")));
	}
	TestFalse(TEXT("No transition trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestFalse(TEXT("No resource setup trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_start_resource_setup"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionMissingRollSourceFailsTest, "Wandbound.Core.RuntimeTurnResolution.MissingRollSourceFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionMissingRollSourceFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeStatusState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Missing roll source fails"), Result.bOk);
	TestEqual(TEXT("Missing roll source reason"), Result.Reason, FString(TEXT("missing_mp_roll_source")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionInvalidFixedRollFailsTest, "Wandbound.Core.RuntimeTurnResolution.InvalidFixedRollFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionInvalidFixedRollFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeStatusState();
	const FWBGameStateData Before = State;
	FWBFixedMPRollSource RollSource(7);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Invalid fixed roll fails"), Result.bOk);
	TestEqual(TEXT("Invalid roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestEqual(TEXT("Burn HP unchanged"), State.GetUnitById(1)->HP, Before.GetUnitById(1)->HP);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionQueuedInvalidRollDoesNotPopTest, "Wandbound.Core.RuntimeTurnResolution.QueuedInvalidRollDoesNotPop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionQueuedInvalidRollDoesNotPopTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeStatusState();
	const FWBGameStateData Before = State;
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(0);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Invalid queued roll fails"), Result.bOk);
	TestEqual(TEXT("Invalid queued roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Invalid queued roll remains queued"), RollSource.NumRemainingRolls(), 1);
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionQueuedRollConsumedByFullEndTurnOnlyTest, "Wandbound.Core.RuntimeTurnResolution.QueuedRollConsumedByFullEndTurnOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionQueuedRollConsumedByFullEndTurnOnlyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeState();
	TestTrue(TEXT("Move unit added"), State.AddUnitForTest(MakeRuntimeUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1)));
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBApplyActionResult MoveResult = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeMoveAction(),
		Context);
	TestTrue(TEXT("Move succeeds"), MoveResult.bOk);
	TestEqual(TEXT("Move does not consume roll"), RollSource.NumRemainingRolls(), 1);

	const FWBApplyActionResult EndTurnResult = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeAction(EWBActionType::EndTurn, 0),
		Context);
	TestTrue(TEXT("Full EndTurn succeeds"), EndTurnResult.bOk);
	TestEqual(TEXT("Full EndTurn consumes roll"), RollSource.NumRemainingRolls(), 0);
	TestEqual(TEXT("Next player MP setup"), State.GetPlayerById(1)->RemainingMP, 4);
	TestTrue(TEXT("Transition trace appears"), TraceContainsKind(EndTurnResult.TraceEvents, FName(TEXT("turn_transition"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionMoveDoesNotRequireRollSourceTest, "Wandbound.Core.RuntimeTurnResolution.MoveDoesNotRequireRollSource", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionMoveDoesNotRequireRollSourceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeState();
	TestTrue(TEXT("Move unit added"), State.AddUnitForTest(MakeRuntimeUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1)));
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeMoveAction(),
		Context);

	TestTrue(TEXT("Move succeeds without roll source"), Result.bOk);
	TestEqual(TEXT("Unit moved"), State.GetUnitById(1)->X, 5);
	TestEqual(TEXT("Player MP decrements"), State.GetPlayerById(0)->RemainingMP, 1);
	TestEqual(TEXT("Move trace only"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionPassResponseDoesNotConsumeRollTest, "Wandbound.Core.RuntimeTurnResolution.PassResponseDoesNotConsumeRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionPassResponseDoesNotConsumeRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeState();
	State.PriorityPlayer = 1;
	State.TurnNumber = 6;
	State.Phase = EWBGamePhase::Response;
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeRuntimeAction(EWBActionType::PassResponse, 1),
		Context);

	TestTrue(TEXT("PassResponse succeeds"), Result.bOk);
	TestEqual(TEXT("Roll remains queued"), RollSource.NumRemainingRolls(), 1);
	TestEqual(TEXT("Priority returns to current player"), State.PriorityPlayer, 0);
	TestTrue(TEXT("Phase returns to normal"), State.IsNormalTurnPhase());
	TestEqual(TEXT("PassResponse trace only"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionLegalGenerationUnchangedTest, "Wandbound.Core.RuntimeTurnResolution.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeState();
	TestTrue(TEXT("Current unit added"), State.AddUnitForTest(MakeRuntimeUnit(1, 0, FWBTile(4, 4))));

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActions(State);
	const TArray<FString> ActionIds = WBActionCodec::MakeActionIds(Actions);

	TestTrue(TEXT("EndTurn remains generated"), ContainsActionType(Actions, EWBActionType::EndTurn));
	for (const FString& ActionId : ActionIds)
	{
		TestFalse(*FString::Printf(TEXT("%s is not a full transition action"), *ActionId), ActionId.Contains(TEXT("full_transition")));
		TestFalse(*FString::Printf(TEXT("%s is not a turn transition action"), *ActionId), ActionId.Contains(TEXT("turn_transition")));
		TestFalse(*FString::Printf(TEXT("%s is not a roll action"), *ActionId), ActionId.Contains(TEXT("roll")));
		TestFalse(*FString::Printf(TEXT("%s is not a draw action"), *ActionId), ActionId.Contains(TEXT("draw")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionActionCodecUnchangedTest, "Wandbound.Core.RuntimeTurnResolution.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeStatusState();
	FWBAction EndTurnAction = MakeRuntimeAction(EWBActionType::EndTurn, 0);
	TestEqual(TEXT("EndTurn action id before runtime execution"), WBActionCodec::MakeActionId(EndTurnAction), FString(TEXT("end_turn:p0")));

	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(State, EndTurnAction, Context);
	TestTrue(TEXT("Runtime full EndTurn succeeds"), Result.bOk);

	TestEqual(TEXT("EndTurn action id after runtime execution"), WBActionCodec::MakeActionId(EndTurnAction), FString(TEXT("end_turn:p0")));
	const FString SerializedAction = WBActionCodec::SerializeAction(EndTurnAction);
	TestFalse(TEXT("Serialized action has no runtime context"), SerializedAction.Contains(TEXT("runtime_context")));
	TestFalse(TEXT("Serialized action has no roll source"), SerializedAction.Contains(TEXT("mp_roll")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionReplayApplyActionUnchangedTest, "Wandbound.Core.RuntimeTurnResolution.ReplayApplyActionUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionReplayApplyActionUnchangedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Action replay fixture loads"), LoadRuntimeFixture(TEXT("replay_turn_command_does_not_change_action_replay.json"), Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(*FString::Printf(TEXT("Fixture state builds: %s"), *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));

	FWBApplyActionResult Result;
	FString ApplyReason;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	TestTrue(
		*FString::Printf(TEXT("apply_action fixture uses existing path: %s"), *ApplyReason),
		ApplyFixtureOperation(Fixture, State, Result, OperationKind, ApplyReason));

	TestTrue(TEXT("Replay fixture remains apply_action"), OperationKind == EWBFixtureOperationKind::ApplyAction);
	TestTrue(TEXT("Replay apply_action succeeds"), Result.bOk);
	TestEqual(TEXT("Replay apply_action emits one trace"), Result.TraceEvents.Num(), 1);
	TestFalse(TEXT("Replay apply_action has no transition trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestFalse(TEXT("Replay apply_action has no resource setup trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_start_resource_setup"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResolutionFixtureScenariosTest, "Wandbound.Core.RuntimeTurnResolution.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResolutionFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("runtime_selected_end_turn_full_transition_roll_4.json"),
		TEXT("runtime_selected_end_turn_basic_no_roll.json"),
		TEXT("runtime_selected_end_turn_missing_roll_source_fails.json"),
		TEXT("runtime_selected_end_turn_invalid_roll_no_mutation.json"),
		TEXT("runtime_selected_move_does_not_consume_roll.json"),
		TEXT("runtime_selected_pass_response_does_not_consume_roll.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		FWBGameStateData State;
		FWBApplyActionResult Result;
		int32 RollSourceRemainingCount = -1;
		if (!RunRuntimeFixture(FixtureName, *this, Fixture, State, Result, RollSourceRemainingCount))
		{
			continue;
		}

		CheckRuntimeFixtureExpectations(FixtureName, *this, Fixture, State, Result, RollSourceRemainingCount);
	}

	return true;
}

#endif
