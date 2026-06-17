#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBMPRollSource.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRuntimeTurnResolutionAdapter.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeEnvelopePlayer(
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

FWBUnitState MakeEnvelopeUnit(
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

FWBGameStateData MakeEnvelopeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeEnvelopePlayer(0, 2, 2));
	State.Players.Add(MakeEnvelopePlayer(1));
	return State;
}

FWBGameStateData MakeEnvelopeStatusState()
{
	FWBGameStateData State = MakeEnvelopeState();
	FWBUnitState BurnUnit = MakeEnvelopeUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeEnvelopeUnit(2, 1, FWBTile(6, 4), 5, 5, 0, 2);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(BurnUnit);
	State.AddUnitForTest(PoisonUnit);
	return State;
}

FWBAction MakeEnvelopeAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

FWBAction MakeEnvelopeMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

const FWBPublicPlayerTurnSummary* FindPublicPlayerSummary(const FWBPublicTurnSummary& Summary, const int32 PlayerId)
{
	for (const FWBPublicPlayerTurnSummary& Player : Summary.Players)
	{
		if (Player.PlayerId == PlayerId)
		{
			return &Player;
		}
	}

	return nullptr;
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

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString RuntimeResultFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadRuntimeResultFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *RuntimeResultFixturePath(FileName)))
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

bool RunRuntimeResultFixture(
	const FString& FixtureName,
	FAutomationTestBase& Test,
	TSharedPtr<FJsonObject>& OutFixture,
	FWBGameStateData& OutState,
	FWBRuntimeSelectedActionResult& OutEnvelope,
	int32& OutRollSourceRemainingCount)
{
	Test.TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadRuntimeResultFixture(FixtureName, OutFixture));
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
	const bool bApplied = ApplyRuntimeSelectedActionWithResultFixture(
		OutFixture,
		OutState,
		OutEnvelope,
		OutRollSourceRemainingCount,
		ApplyReason);
	Test.TestTrue(*FString::Printf(TEXT("%s envelope operation applies/parses: %s"), *FixtureName, *ApplyReason), bApplied);
	return bApplied;
}

bool CheckRuntimeResultFixtureExpectations(
	const FString& FixtureName,
	FAutomationTestBase& Test,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBRuntimeSelectedActionResult& Envelope,
	const int32 RollSourceRemainingCount)
{
	bool bExpectedOk = false;
	FString ExpectedReason;
	Test.TestTrue(*FString::Printf(TEXT("%s expected result reads"), *FixtureName), ReadExpectedResult(Fixture, bExpectedOk, ExpectedReason));
	Test.TestEqual(*FString::Printf(TEXT("%s result ok"), *FixtureName), Envelope.ApplyResult.bOk, bExpectedOk);
	Test.TestEqual(*FString::Printf(TEXT("%s result reason"), *FixtureName), Envelope.ApplyResult.Reason, ExpectedReason);

	FString EnvelopeReason;
	const bool bEnvelopeMatches = ExpectRuntimeSelectedActionEnvelope(
		Fixture,
		Envelope,
		RollSourceRemainingCount,
		EnvelopeReason);
	Test.TestTrue(*FString::Printf(TEXT("%s envelope matches: %s"), *FixtureName, *EnvelopeReason), bEnvelopeMatches);
	return bEnvelopeMatches;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeFullEndTurnTest, "Wandbound.Core.RuntimeTurnResultEnvelope.FullEndTurnReportsActionIdAndRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeFullEndTurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEnvelopeStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FWBAction EndTurnAction = MakeEnvelopeAction(EWBActionType::EndTurn, 0);

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		EndTurnAction,
		Context);

	TestTrue(TEXT("Full EndTurn envelope succeeds"), Envelope.ApplyResult.bOk);
	TestEqual(TEXT("Selected action type"), Envelope.SelectedActionType, FName(TEXT("end_turn")));
	TestEqual(TEXT("Selected action id"), Envelope.SelectedActionId, WBActionCodec::MakeActionId(EndTurnAction));
	TestTrue(TEXT("MP roll consumed"), Envelope.bConsumedMPRoll);
	TestEqual(TEXT("Consumed roll"), Envelope.ConsumedMPRoll, 4);
	TestEqual(TEXT("Summary current player"), Envelope.FinalPublicTurnSummary.CurrentPlayerId, 1);
	TestEqual(TEXT("Summary priority player"), Envelope.FinalPublicTurnSummary.PriorityPlayerId, 1);
	TestEqual(TEXT("Summary phase"), Envelope.FinalPublicTurnSummary.Phase, FName(TEXT("normal_turn")));
	const FWBPublicPlayerTurnSummary* Player1 = FindPublicPlayerSummary(Envelope.FinalPublicTurnSummary, 1);
	TestTrue(TEXT("Player 1 summary exists"), Player1 != nullptr);
	if (Player1 != nullptr)
	{
		TestEqual(TEXT("Player 1 remaining MP"), Player1->RemainingMP, 4);
		TestEqual(TEXT("Player 1 last roll"), Player1->LastMPRoll, 4);
		TestEqual(TEXT("Player 1 walls left"), Player1->WallsLeft, 1);
	}
	TestTrue(TEXT("Transition trace appears"), TraceContainsKind(Envelope.ApplyResult.TraceEvents, FName(TEXT("turn_transition"))));
	TestTrue(TEXT("Burn trace appears"), TraceContainsStatusTick(Envelope.ApplyResult.TraceEvents, FName(TEXT("Burn"))));
	TestTrue(TEXT("Poison trace appears"), TraceContainsStatusTick(Envelope.ApplyResult.TraceEvents, FName(TEXT("Poison"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeBasicEndTurnTest, "Wandbound.Core.RuntimeTurnResultEnvelope.BasicEndTurnReportsNoRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeBasicEndTurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEnvelopeStatusState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = false;
	const FWBAction EndTurnAction = MakeEnvelopeAction(EWBActionType::EndTurn, 0);

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		EndTurnAction,
		Context);

	TestTrue(TEXT("Basic EndTurn envelope succeeds"), Envelope.ApplyResult.bOk);
	TestEqual(TEXT("Selected action id"), Envelope.SelectedActionId, WBActionCodec::MakeActionId(EndTurnAction));
	TestFalse(TEXT("No MP roll consumed"), Envelope.bConsumedMPRoll);
	TestEqual(TEXT("Consumed roll is zero"), Envelope.ConsumedMPRoll, 0);
	TestEqual(TEXT("Summary current player"), Envelope.FinalPublicTurnSummary.CurrentPlayerId, 1);
	TestEqual(TEXT("Trace count"), Envelope.ApplyResult.TraceEvents.Num(), 1);
	if (Envelope.ApplyResult.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Basic trace kind"), Envelope.ApplyResult.TraceEvents[0].Kind, FName(TEXT("end_turn")));
	}
	TestFalse(TEXT("No full transition trace"), TraceContainsKind(Envelope.ApplyResult.TraceEvents, FName(TEXT("turn_transition"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeMoveTest, "Wandbound.Core.RuntimeTurnResultEnvelope.MoveReportsNoRollAndFinalMP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEnvelopeState();
	TestTrue(TEXT("Move unit added"), State.AddUnitForTest(MakeEnvelopeUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1)));
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FWBAction MoveAction = MakeEnvelopeMoveAction();

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MoveAction,
		Context);

	TestTrue(TEXT("Move envelope succeeds"), Envelope.ApplyResult.bOk);
	TestEqual(TEXT("Selected move id"), Envelope.SelectedActionId, WBActionCodec::MakeActionId(MoveAction));
	TestFalse(TEXT("No MP roll consumed"), Envelope.bConsumedMPRoll);
	TestEqual(TEXT("Queued roll remains"), RollSource.NumRemainingRolls(), 1);
	const FWBPublicPlayerTurnSummary* Player0 = FindPublicPlayerSummary(Envelope.FinalPublicTurnSummary, 0);
	TestTrue(TEXT("Player 0 summary exists"), Player0 != nullptr);
	if (Player0 != nullptr)
	{
		TestEqual(TEXT("Player 0 MP decremented"), Player0->RemainingMP, 1);
	}
	TestEqual(TEXT("Move trace count"), Envelope.ApplyResult.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopePassResponseTest, "Wandbound.Core.RuntimeTurnResultEnvelope.PassResponseReportsNoRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopePassResponseTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEnvelopeState();
	State.PriorityPlayer = 1;
	State.TurnNumber = 6;
	State.Phase = EWBGamePhase::Response;
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FWBAction PassResponseAction = MakeEnvelopeAction(EWBActionType::PassResponse, 1);

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		PassResponseAction,
		Context);

	TestTrue(TEXT("PassResponse envelope succeeds"), Envelope.ApplyResult.bOk);
	TestEqual(TEXT("Selected pass response id"), Envelope.SelectedActionId, WBActionCodec::MakeActionId(PassResponseAction));
	TestFalse(TEXT("No MP roll consumed"), Envelope.bConsumedMPRoll);
	TestEqual(TEXT("Queued roll remains"), RollSource.NumRemainingRolls(), 1);
	TestEqual(TEXT("Summary priority player"), Envelope.FinalPublicTurnSummary.PriorityPlayerId, 0);
	TestEqual(TEXT("Summary phase"), Envelope.FinalPublicTurnSummary.Phase, FName(TEXT("normal_turn")));
	TestEqual(TEXT("PassResponse trace count"), Envelope.ApplyResult.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeMissingRollSourceFailureTest, "Wandbound.Core.RuntimeTurnResultEnvelope.MissingRollSourceFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeMissingRollSourceFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEnvelopeStatusState();
	const FWBPublicTurnSummary BeforeSummary = WBPublicTurnSummary::Build(State);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeEnvelopeAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Missing roll source fails"), Envelope.ApplyResult.bOk);
	TestEqual(TEXT("Failure reason"), Envelope.ApplyResult.Reason, FString(TEXT("missing_mp_roll_source")));
	TestFalse(TEXT("No MP roll consumed"), Envelope.bConsumedMPRoll);
	TestEqual(TEXT("Summary current unchanged"), Envelope.FinalPublicTurnSummary.CurrentPlayerId, BeforeSummary.CurrentPlayerId);
	TestEqual(TEXT("Summary turn unchanged"), Envelope.FinalPublicTurnSummary.TurnNumber, BeforeSummary.TurnNumber);
	TestEqual(TEXT("No traces"), Envelope.ApplyResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeInvalidQueuedRollFailureTest, "Wandbound.Core.RuntimeTurnResultEnvelope.InvalidQueuedRollFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeInvalidQueuedRollFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEnvelopeStatusState();
	const FWBPublicTurnSummary BeforeSummary = WBPublicTurnSummary::Build(State);
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(7);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeEnvelopeAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Invalid queued roll fails"), Envelope.ApplyResult.bOk);
	TestEqual(TEXT("Failure reason"), Envelope.ApplyResult.Reason, FString(TEXT("invalid_mp_roll")));
	TestFalse(TEXT("No MP roll consumed"), Envelope.bConsumedMPRoll);
	TestEqual(TEXT("Consumed roll remains zero"), Envelope.ConsumedMPRoll, 0);
	TestEqual(TEXT("Invalid roll not popped"), RollSource.NumRemainingRolls(), 1);
	TestEqual(TEXT("Summary current unchanged"), Envelope.FinalPublicTurnSummary.CurrentPlayerId, BeforeSummary.CurrentPlayerId);
	TestEqual(TEXT("Summary turn unchanged"), Envelope.FinalPublicTurnSummary.TurnNumber, BeforeSummary.TurnNumber);
	TestEqual(TEXT("No traces"), Envelope.ApplyResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeActionIdNoContextTest, "Wandbound.Core.RuntimeTurnResultEnvelope.SelectedActionIdHasNoRuntimeContext", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeActionIdNoContextTest::RunTest(const FString& Parameters)
{
	FWBGameStateData FullState = MakeEnvelopeStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext FullContext;
	FullContext.bResolveEndTurnAsFullTransition = true;
	FullContext.MPRollSource = &RollSource;
	const FWBAction EndTurnAction = MakeEnvelopeAction(EWBActionType::EndTurn, 0);

	const FWBRuntimeSelectedActionResult FullEnvelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		FullState,
		EndTurnAction,
		FullContext);

	FWBGameStateData BasicState = MakeEnvelopeStatusState();
	FWBRuntimeTurnResolutionContext BasicContext;
	BasicContext.bResolveEndTurnAsFullTransition = false;
	const FWBRuntimeSelectedActionResult BasicEnvelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		BasicState,
		EndTurnAction,
		BasicContext);

	TestTrue(TEXT("Full EndTurn succeeds"), FullEnvelope.ApplyResult.bOk);
	TestTrue(TEXT("Basic EndTurn succeeds"), BasicEnvelope.ApplyResult.bOk);
	TestEqual(TEXT("Full and basic selected action ids match"), FullEnvelope.SelectedActionId, BasicEnvelope.SelectedActionId);
	TestEqual(TEXT("Selected action id remains codec id"), FullEnvelope.SelectedActionId, FString(TEXT("end_turn:p0")));
	TestFalse(TEXT("Action id has no roll value"), FullEnvelope.SelectedActionId.Contains(TEXT("4")));
	TestFalse(TEXT("Action id has no full transition wording"), FullEnvelope.SelectedActionId.Contains(TEXT("full_transition")));
	TestFalse(TEXT("Action id has no turn transition wording"), FullEnvelope.SelectedActionId.Contains(TEXT("turn_transition")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeLegacyAdapterStillWorksTest, "Wandbound.Core.RuntimeTurnResultEnvelope.LegacyAdapterStillWorks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeLegacyAdapterStillWorksTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEnvelopeStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBApplyActionResult Result = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		State,
		MakeEnvelopeAction(EWBActionType::EndTurn, 0),
		Context);

	TestTrue(TEXT("Legacy adapter succeeds"), Result.bOk);
	TestTrue(TEXT("Legacy adapter still emits transition trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnResultEnvelopeFixtureScenariosTest, "Wandbound.Core.RuntimeTurnResultEnvelope.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnResultEnvelopeFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("runtime_result_end_turn_full_transition_roll_4.json"),
		TEXT("runtime_result_end_turn_basic_no_roll.json"),
		TEXT("runtime_result_move_summary.json"),
		TEXT("runtime_result_pass_response_summary.json"),
		TEXT("runtime_result_invalid_roll_no_mutation.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		FWBGameStateData State;
		FWBRuntimeSelectedActionResult Envelope;
		int32 RollSourceRemainingCount = -1;
		if (!RunRuntimeResultFixture(FixtureName, *this, Fixture, State, Envelope, RollSourceRemainingCount))
		{
			continue;
		}

		CheckRuntimeResultFixtureExpectations(FixtureName, *this, Fixture, Envelope, RollSourceRemainingCount);
	}

	return true;
}

#endif
