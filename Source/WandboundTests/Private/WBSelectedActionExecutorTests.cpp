#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBSelectedActionExecutor.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeSelectedActionPlayer(
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

FWBUnitState MakeSelectedActionUnit(
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

FWBGameStateData MakeSelectedActionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeSelectedActionPlayer(0, 2, 2));
	State.Players.Add(MakeSelectedActionPlayer(1));
	return State;
}

FWBGameStateData MakeSelectedActionStatusState()
{
	FWBGameStateData State = MakeSelectedActionState();
	FWBUnitState BurnUnit = MakeSelectedActionUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeSelectedActionUnit(2, 1, FWBTile(6, 4), 5, 5, 0, 2);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(BurnUnit);
	State.AddUnitForTest(PoisonUnit);
	return State;
}

FWBAction MakeSelectedMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeSelectedTurnAction(const EWBActionType Type, const int32 PlayerId)
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

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	return true;
}

int32 ReadIntegerFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const int32 DefaultValue)
{
	int32 Value = DefaultValue;
	TryReadIntegerField(Object, FieldName, Value);
	return Value;
}

FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Object)
{
	FString Output;
	if (Object.IsValid())
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	}
	return Output;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString SelectedActionFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadSelectedActionFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *SelectedActionFixturePath(FileName)))
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

bool ParseSelectedActionFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FWBAction& OutAction,
	FString& OutReason)
{
	FString Operation;
	if (!Fixture.IsValid() || !Fixture->TryGetStringField(TEXT("operation"), Operation))
	{
		OutReason = TEXT("missing_operation");
		return false;
	}

	if (Operation != TEXT("apply_selected_action"))
	{
		OutReason = TEXT("fixture_operation_is_not_apply_selected_action");
		return false;
	}

	const TSharedPtr<FJsonObject>* ActionObject = nullptr;
	if (!Fixture->TryGetObjectField(TEXT("action"), ActionObject)
		|| ActionObject == nullptr
		|| !ActionObject->IsValid())
	{
		OutReason = TEXT("missing_selected_action");
		return false;
	}

	TSharedPtr<FJsonObject> NormalizedActionObject = MakeShared<FJsonObject>();
	NormalizedActionObject->Values = (*ActionObject)->Values;

	FString Type;
	if (!NormalizedActionObject->HasField(TEXT("kind")) && NormalizedActionObject->TryGetStringField(TEXT("type"), Type))
	{
		NormalizedActionObject->SetStringField(TEXT("kind"), Type);
	}

	int32 SourceUnitId = -1;
	if (!NormalizedActionObject->HasField(TEXT("unit_id"))
		&& TryReadIntegerField(NormalizedActionObject, TEXT("source_unit_id"), SourceUnitId))
	{
		NormalizedActionObject->SetNumberField(TEXT("unit_id"), SourceUnitId);
	}

	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(SerializeJsonObject(NormalizedActionObject), State);
	if (!DecodeResult.bOk)
	{
		OutReason = DecodeResult.Reason;
		return false;
	}

	OutAction = DecodeResult.Action;
	OutReason.Reset();
	return true;
}

bool ParseSelectedActionContextFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBSelectedActionExecutionContext& OutContext,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ContextObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("context"), ContextObject)
		|| ContextObject == nullptr
		|| !ContextObject->IsValid())
	{
		OutReason = TEXT("missing_selected_action_context");
		return false;
	}

	if (!(*ContextObject)->TryGetBoolField(
		TEXT("use_full_turn_transition_for_end_turn"),
		OutContext.bUseFullTurnTransitionForEndTurn))
	{
		OutReason = TEXT("missing_selected_action_context_full_transition_flag");
		return false;
	}

	OutContext.NextPlayerExplicitMPRoll = ReadIntegerFieldOrDefault(
		*ContextObject,
		TEXT("next_player_explicit_mp_roll"),
		0);

	OutReason.Reset();
	return true;
}

bool RunSelectedActionFixture(
	const FString& FixtureName,
	FAutomationTestBase& Test,
	TSharedPtr<FJsonObject>& OutFixture,
	FWBGameStateData& OutState,
	FWBApplyActionResult& OutResult)
{
	Test.TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadSelectedActionFixture(FixtureName, OutFixture));
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

	FWBAction Action;
	FString ActionReason;
	const bool bActionParsed = ParseSelectedActionFromFixture(OutFixture, OutState, Action, ActionReason);
	Test.TestTrue(*FString::Printf(TEXT("%s selected action parses: %s"), *FixtureName, *ActionReason), bActionParsed);
	if (!bActionParsed)
	{
		return false;
	}

	FWBSelectedActionExecutionContext Context;
	FString ContextReason;
	const bool bContextParsed = ParseSelectedActionContextFromFixture(OutFixture, Context, ContextReason);
	Test.TestTrue(*FString::Printf(TEXT("%s context parses: %s"), *FixtureName, *ContextReason), bContextParsed);
	if (!bContextParsed)
	{
		return false;
	}

	OutResult = WBSelectedActionExecutor::ApplySelectedAction(OutState, Action, Context);
	return true;
}

bool CheckSelectedActionFixtureExpectations(
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionMoveDelegatesToApplyMoveTest, "Wandbound.Core.SelectedAction.MoveDelegatesToApplyMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionMoveDelegatesToApplyMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionState();
	TestTrue(TEXT("Move unit added"), State.AddUnitForTest(MakeSelectedActionUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1)));

	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = true;
	Context.NextPlayerExplicitMPRoll = 4;
	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(State, MakeSelectedMoveAction(), Context);

	TestTrue(TEXT("Selected Move succeeds"), Result.bOk);
	TestEqual(TEXT("Unit moves to destination x"), State.GetUnitById(1)->X, 5);
	TestEqual(TEXT("Unit moves to destination y"), State.GetUnitById(1)->Y, 4);
	TestEqual(TEXT("Player MP decrements"), State.GetPlayerById(0)->RemainingMP, 1);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("move")));
	}
	TestFalse(TEXT("Move does not emit turn transition"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionEndTurnBasicTest, "Wandbound.Core.SelectedAction.EndTurnBasic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionEndTurnBasicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionStatusState();
	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = false;

	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(
		State,
		MakeSelectedTurnAction(EWBActionType::EndTurn, 0),
		Context);

	TestTrue(TEXT("Selected EndTurn basic succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Priority follows current player"), State.PriorityPlayer, 1);
	TestEqual(TEXT("Turn number increments"), State.TurnNumber, 4);
	TestEqual(TEXT("Burn does not tick"), State.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("Poison does not tick"), State.GetUnitById(2)->MaxHP, 5);
	TestEqual(TEXT("Next player MP is not setup"), State.GetPlayerById(1)->RemainingMP, 0);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("end_turn")));
	}
	TestFalse(TEXT("Basic selected EndTurn has no transition trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestFalse(TEXT("Basic selected EndTurn has no status tick"), TraceContainsKind(Result.TraceEvents, FName(TEXT("status_tick"))));
	TestFalse(TEXT("Basic selected EndTurn has no resource setup"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_start_resource_setup"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionEndTurnFullTransitionTest, "Wandbound.Core.SelectedAction.EndTurnFullTransition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionEndTurnFullTransitionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionStatusState();
	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = true;
	Context.NextPlayerExplicitMPRoll = 4;

	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(
		State,
		MakeSelectedTurnAction(EWBActionType::EndTurn, 0),
		Context);

	TestTrue(TEXT("Selected EndTurn full transition succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Burn applies"), State.GetUnitById(1)->HP, 4);
	TestEqual(TEXT("Poison MaxHP applies"), State.GetUnitById(2)->MaxHP, 4);
	TestEqual(TEXT("Poison HP clamps"), State.GetUnitById(2)->HP, 4);
	TestEqual(TEXT("Next player MP is setup"), State.GetPlayerById(1)->RemainingMP, 4);
	TestEqual(TEXT("Next player roll recorded"), State.GetPlayerById(1)->LastMPRoll, 4);
	TestEqual(TEXT("Next player attacks reset"), State.GetUnitById(2)->AttacksLeft, 2);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 7);
	if (Result.TraceEvents.Num() == 7)
	{
		TestEqual(TEXT("Trace 0 transition"), Result.TraceEvents[0].Kind, FName(TEXT("turn_transition")));
		TestEqual(TEXT("Trace 1 end status"), Result.TraceEvents[1].Kind, FName(TEXT("end_turn_status_ticks")));
		TestEqual(TEXT("Trace 2 Burn"), Result.TraceEvents[2].StatusId, FName(TEXT("Burn")));
		TestEqual(TEXT("Trace 3 end turn"), Result.TraceEvents[3].Kind, FName(TEXT("end_turn")));
		TestEqual(TEXT("Trace 4 start status"), Result.TraceEvents[4].Kind, FName(TEXT("start_turn_status_ticks")));
		TestEqual(TEXT("Trace 5 Poison"), Result.TraceEvents[5].StatusId, FName(TEXT("Poison")));
		TestEqual(TEXT("Trace 6 resources"), Result.TraceEvents[6].Kind, FName(TEXT("turn_start_resource_setup")));
	}
	TestTrue(TEXT("Full selected EndTurn has Burn tick"), TraceContainsStatusTick(Result.TraceEvents, FName(TEXT("Burn"))));
	TestTrue(TEXT("Full selected EndTurn has Poison tick"), TraceContainsStatusTick(Result.TraceEvents, FName(TEXT("Poison"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionEndTurnFullTransitionInvalidRollTest, "Wandbound.Core.SelectedAction.EndTurnFullTransitionInvalidRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionEndTurnFullTransitionInvalidRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionStatusState();
	const FWBGameStateData Before = State;
	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = true;
	Context.NextPlayerExplicitMPRoll = 7;

	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(
		State,
		MakeSelectedTurnAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Invalid full selected EndTurn fails"), Result.bOk);
	TestEqual(TEXT("Invalid roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestEqual(TEXT("Burn HP unchanged"), State.GetUnitById(1)->HP, Before.GetUnitById(1)->HP);
	TestEqual(TEXT("Poison MaxHP unchanged"), State.GetUnitById(2)->MaxHP, Before.GetUnitById(2)->MaxHP);
	TestEqual(TEXT("Next player MP unchanged"), State.GetPlayerById(1)->RemainingMP, Before.GetPlayerById(1)->RemainingMP);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionEndTurnFullTransitionMissingRollTest, "Wandbound.Core.SelectedAction.EndTurnFullTransitionMissingRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionEndTurnFullTransitionMissingRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionStatusState();
	const FWBGameStateData Before = State;
	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = true;

	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(
		State,
		MakeSelectedTurnAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Missing roll fails"), Result.bOk);
	TestEqual(TEXT("Missing roll uses invalid roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionEndTurnBasicIgnoresRollTest, "Wandbound.Core.SelectedAction.EndTurnBasicIgnoresRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionEndTurnBasicIgnoresRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionStatusState();
	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = false;
	Context.NextPlayerExplicitMPRoll = 4;

	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(
		State,
		MakeSelectedTurnAction(EWBActionType::EndTurn, 0),
		Context);

	TestTrue(TEXT("Basic selected EndTurn succeeds"), Result.bOk);
	TestEqual(TEXT("Roll is ignored"), State.GetPlayerById(1)->LastMPRoll, 0);
	TestEqual(TEXT("MP is not setup"), State.GetPlayerById(1)->RemainingMP, 0);
	TestEqual(TEXT("Only basic trace"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("end_turn")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionPassResponseDelegatesTest, "Wandbound.Core.SelectedAction.PassResponseDelegates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionPassResponseDelegatesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionState();
	State.PriorityPlayer = 1;
	State.TurnNumber = 6;
	State.Phase = EWBGamePhase::Response;
	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = true;
	Context.NextPlayerExplicitMPRoll = 4;

	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(
		State,
		MakeSelectedTurnAction(EWBActionType::PassResponse, 1),
		Context);

	TestTrue(TEXT("Selected PassResponse succeeds"), Result.bOk);
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, 0);
	TestEqual(TEXT("Priority returns to current player"), State.PriorityPlayer, 0);
	TestTrue(TEXT("Phase returns to normal"), State.IsNormalTurnPhase());
	TestEqual(TEXT("Turn number unchanged"), State.TurnNumber, 6);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("pass_response")));
	}
	TestFalse(TEXT("PassResponse has no transition trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionLegalGenerationUnchangedTest, "Wandbound.Core.SelectedAction.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionState();
	TestTrue(TEXT("Current unit added"), State.AddUnitForTest(MakeSelectedActionUnit(1, 0, FWBTile(4, 4))));

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActions(State);
	const TArray<FString> ActionIds = WBActionCodec::MakeActionIds(Actions);

	TestTrue(TEXT("EndTurn action remains generated"), ContainsActionType(Actions, EWBActionType::EndTurn));
	TestTrue(TEXT("Movement remains generated before EndTurn"), Actions.Num() > 1 && Actions[0].Type == EWBActionType::Move);
	TestTrue(TEXT("EndTurn remains last"), Actions.Num() > 0 && Actions.Last().Type == EWBActionType::EndTurn);
	for (const FString& ActionId : ActionIds)
	{
		TestFalse(*FString::Printf(TEXT("%s is not a full transition player action"), *ActionId), ActionId.Contains(TEXT("full_transition")));
		TestFalse(*FString::Printf(TEXT("%s is not a turn transition player action"), *ActionId), ActionId.Contains(TEXT("turn_transition")));
		TestFalse(*FString::Printf(TEXT("%s is not a roll action"), *ActionId), ActionId.Contains(TEXT("roll")));
		TestFalse(*FString::Printf(TEXT("%s is not a draw action"), *ActionId), ActionId.Contains(TEXT("draw")));
		TestFalse(*FString::Printf(TEXT("%s is not a status tick action"), *ActionId), ActionId.Contains(TEXT("status_tick")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionCodecUnchangedTest, "Wandbound.Core.SelectedAction.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSelectedActionStatusState();
	FWBAction EndTurnAction = MakeSelectedTurnAction(EWBActionType::EndTurn, 0);
	const FString ActionIdBefore = WBActionCodec::MakeActionId(EndTurnAction);
	TestEqual(TEXT("EndTurn action ID remains stable before selected execution"), ActionIdBefore, FString(TEXT("end_turn:p0")));

	FWBSelectedActionExecutionContext Context;
	Context.bUseFullTurnTransitionForEndTurn = true;
	Context.NextPlayerExplicitMPRoll = 4;
	const FWBApplyActionResult Result = WBSelectedActionExecutor::ApplySelectedAction(State, EndTurnAction, Context);
	TestTrue(TEXT("Selected full transition succeeds"), Result.bOk);

	const FString ActionIdAfter = WBActionCodec::MakeActionId(EndTurnAction);
	TestEqual(TEXT("EndTurn action ID remains stable after selected execution"), ActionIdAfter, FString(TEXT("end_turn:p0")));
	const FString SerializedAction = WBActionCodec::SerializeAction(EndTurnAction);
	TestTrue(TEXT("Serialized EndTurn has no command mode"), !SerializedAction.Contains(TEXT("deterministic_full_transition")));
	TestTrue(TEXT("Serialized EndTurn has no MP roll"), !SerializedAction.Contains(TEXT("next_player_explicit_mp_roll")));
	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(SerializedAction, State);
	TestTrue(TEXT("EndTurn action still decodes through WBActionCodec"), DecodeResult.bOk);
	TestTrue(TEXT("Decoded action remains EndTurn"), DecodeResult.Action.Type == EWBActionType::EndTurn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionReplayApplyActionUnchangedTest, "Wandbound.Core.SelectedAction.ReplayApplyActionUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionReplayApplyActionUnchangedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Action replay fixture loads"), LoadSelectedActionFixture(TEXT("replay_turn_command_does_not_change_action_replay.json"), Fixture));
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
	TestTrue(TEXT("Replay apply_action EndTurn succeeds"), Result.bOk);
	TestEqual(TEXT("Replay apply_action emits one basic trace"), Result.TraceEvents.Num(), 1);
	TestFalse(TEXT("Replay apply_action has no turn transition trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_transition"))));
	TestFalse(TEXT("Replay apply_action has no status trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("status_tick"))));
	TestFalse(TEXT("Replay apply_action has no resource setup trace"), TraceContainsKind(Result.TraceEvents, FName(TEXT("turn_start_resource_setup"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionFixtureScenariosTest, "Wandbound.Core.SelectedAction.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("selected_action_end_turn_basic.json"),
		TEXT("selected_action_end_turn_full_transition.json"),
		TEXT("selected_action_end_turn_full_transition_invalid_roll.json"),
		TEXT("selected_action_move_unchanged.json"),
		TEXT("selected_action_pass_response_unchanged.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		FWBGameStateData State;
		FWBApplyActionResult Result;
		if (!RunSelectedActionFixture(FixtureName, *this, Fixture, State, Result))
		{
			continue;
		}

		CheckSelectedActionFixtureExpectations(FixtureName, *this, Fixture, State, Result);
	}

	return true;
}

#endif
