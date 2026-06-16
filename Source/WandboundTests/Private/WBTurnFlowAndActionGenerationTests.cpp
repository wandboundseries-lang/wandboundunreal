#include "Misc/AutomationTest.h"
#include "WBAction.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBUnitState MakeTurnFlowUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile, const int32 MPRemaining)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.MPRemaining = MPRemaining;
	return Unit;
}

FWBAction MakeTurnFlowAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

FWBAction MakeTurnFlowMove(const int32 PlayerId, const int32 UnitId, const FWBTile& FromTile, const FWBTile& ToTile)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = UnitId;
	Action.FromTile = FromTile;
	Action.ToTile = ToTile;
	return Action;
}

bool ContainsTurnFlowMoveFromUnitTo(const TArray<FWBAction>& Actions, const int32 UnitId, const FWBTile& Tile)
{
	for (const FWBAction& Action : Actions)
	{
		if (Action.Type == EWBActionType::Move && Action.SourceUnitId == UnitId && Action.ToTile == Tile)
		{
			return true;
		}
	}

	return false;
}

bool ActionIdsEqual(const TArray<FWBAction>& A, const TArray<FWBAction>& B)
{
	return WBActionCodec::MakeActionIds(A) == WBActionCodec::MakeActionIds(B);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnFlowStateHelpersTest, "Wandbound.Core.TurnFlow.StateHelpers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnFlowStateHelpersTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestEqual(TEXT("Default current player"), State.CurrentPlayer, 0);
	TestEqual(TEXT("Default priority player"), State.PriorityPlayer, 0);
	TestEqual(TEXT("Default turn number"), State.TurnNumber, 1);
	TestTrue(TEXT("Default phase is normal turn"), State.IsNormalTurnPhase());
	TestFalse(TEXT("Default phase is not response"), State.IsResponsePhase());
	TestFalse(TEXT("Default game is not over"), State.bGameOver);
	TestTrue(TEXT("Player 0 is valid"), FWBGameStateData::IsValidPlayerId(0));
	TestTrue(TEXT("Player 1 is valid"), FWBGameStateData::IsValidPlayerId(1));
	TestFalse(TEXT("Neutral owner is not an acting player"), FWBGameStateData::IsValidPlayerId(-1));

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.RemainingMP = 5;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.RemainingMP = 7;
	State.Players.Add(Player0);
	State.Players.Add(Player1);

	TestTrue(TEXT("Current player data resolves"), State.GetCurrentPlayer() != nullptr);
	TestTrue(TEXT("Mutable current player data resolves"), State.GetMutableCurrentPlayer() != nullptr);
	if (const FWBPlayerStateData* CurrentPlayer = State.GetCurrentPlayer())
	{
		TestEqual(TEXT("Current player id resolves"), CurrentPlayer->PlayerId, 0);
	}

	State.AdvanceTurnBasic();

	TestEqual(TEXT("AdvanceTurnBasic switches current player"), State.CurrentPlayer, 1);
	TestEqual(TEXT("AdvanceTurnBasic moves priority to new current player"), State.PriorityPlayer, 1);
	TestEqual(TEXT("AdvanceTurnBasic increments turn number"), State.TurnNumber, 2);
	TestTrue(TEXT("AdvanceTurnBasic leaves phase in normal turn"), State.IsNormalTurnPhase());
	TestEqual(TEXT("AdvanceTurnBasic preserves player 0 MP baseline"), State.GetPlayerById(0)->RemainingMP, 5);
	TestEqual(TEXT("AdvanceTurnBasic preserves player 1 MP baseline"), State.GetPlayerById(1)->RemainingMP, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnFlowEndTurnRulesTest, "Wandbound.Core.TurnFlow.EndTurnRulesAndApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnFlowEndTurnRulesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 4;

	const FWBAction EndTurn = MakeTurnFlowAction(EWBActionType::EndTurn, 0);
	TestTrue(TEXT("Current player can end turn"), WBRules::CanEndTurn(State, EndTurn).bOk);
	TestFalse(TEXT("Non-current player cannot end turn"), WBRules::CanEndTurn(State, MakeTurnFlowAction(EWBActionType::EndTurn, 1)).bOk);

	FWBGameStateData ResponseState = State;
	ResponseState.Phase = EWBGamePhase::Response;
	const FWBActionQueryResult ResponseEndTurn = WBRules::CanEndTurn(ResponseState, EndTurn);
	TestFalse(TEXT("End turn is illegal in response phase"), ResponseEndTurn.bOk);
	TestEqual(TEXT("Response phase end turn reason"), ResponseEndTurn.Reason, FString(TEXT("not_normal_turn")));

	FWBGameStateData GameOverState = State;
	GameOverState.bGameOver = true;
	const FWBActionQueryResult GameOverEndTurn = WBRules::CanEndTurn(GameOverState, EndTurn);
	TestFalse(TEXT("End turn is illegal after game over"), GameOverEndTurn.bOk);
	TestEqual(TEXT("Game over end turn reason"), GameOverEndTurn.Reason, FString(TEXT("game_over")));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndTurn(State, EndTurn);
	TestTrue(TEXT("End turn applies"), Result.bOk);
	TestEqual(TEXT("Current player switches"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Priority follows current player"), State.PriorityPlayer, 1);
	TestEqual(TEXT("Turn number increments"), State.TurnNumber, 5);
	TestTrue(TEXT("Phase remains normal"), State.IsNormalTurnPhase());
	TestEqual(TEXT("End turn emits one trace"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("end_turn")));
		TestEqual(TEXT("Trace player"), Result.TraceEvents[0].PlayerId, 0);
		TestEqual(TEXT("Trace from player"), Result.TraceEvents[0].FromPlayer, 0);
		TestEqual(TEXT("Trace to player"), Result.TraceEvents[0].ToPlayer, 1);
		TestEqual(TEXT("Trace turn number"), Result.TraceEvents[0].TurnNumber, 5);
		TestTrue(TEXT("Trace ok"), Result.TraceEvents[0].bOk);
	}

	FWBGameStateData FailedState;
	FailedState.CurrentPlayer = 0;
	FailedState.PriorityPlayer = 0;
	FailedState.TurnNumber = 9;
	const FWBApplyActionResult FailedResult = WBEffectRunner::ApplyEndTurn(FailedState, MakeTurnFlowAction(EWBActionType::EndTurn, 1));
	TestFalse(TEXT("Failed end turn returns failure"), FailedResult.bOk);
	TestEqual(TEXT("Failed end turn leaves current player"), FailedState.CurrentPlayer, 0);
	TestEqual(TEXT("Failed end turn leaves priority"), FailedState.PriorityPlayer, 0);
	TestEqual(TEXT("Failed end turn leaves turn number"), FailedState.TurnNumber, 9);
	TestEqual(TEXT("Failed end turn emits no traces"), FailedResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnFlowLegalActionGenerationTest, "Wandbound.Core.TurnFlow.LegalActionGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnFlowLegalActionGenerationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	TestTrue(TEXT("Mover added"), State.AddUnitForTest(MakeTurnFlowUnit(1, 0, FWBTile(4, 4), 1)));
	TestTrue(TEXT("Enemy blocker added"), State.AddUnitForTest(MakeTurnFlowUnit(2, 1, FWBTile(5, 4), 1)));
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 3)));

	FWBUnitState RootedUnit = MakeTurnFlowUnit(3, 0, FWBTile(2, 2), 1);
	RootedUnit.Statuses.Add(FName(TEXT("root")));
	TestTrue(TEXT("Rooted unit added"), State.AddUnitForTest(RootedUnit));

	FWBUnitState StunnedUnit = MakeTurnFlowUnit(4, 0, FWBTile(3, 3), 1);
	StunnedUnit.Statuses.Add(FName(TEXT("stun")));
	TestTrue(TEXT("Stunned unit added"), State.AddUnitForTest(StunnedUnit));

	TestTrue(TEXT("No-MP unit added"), State.AddUnitForTest(MakeTurnFlowUnit(5, 0, FWBTile(6, 6), 0)));

	const FWBGameStateData BeforeGeneration = State;
	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(State, 0);
	const TArray<FWBAction> RepeatedActions = WBRules::GenerateLegalActionsForPlayer(State, 0);

	TestTrue(TEXT("Legal action generation is deterministic"), ActionIdsEqual(Actions, RepeatedActions));
	TestEqual(TEXT("Current player gets two moves plus end turn"), Actions.Num(), 3);
	TestEqual(TEXT("Non-current player gets no normal actions"), WBRules::GenerateLegalActionsForPlayer(State, 1).Num(), 0);
	TestTrue(TEXT("West move generated"), ContainsTurnFlowMoveFromUnitTo(Actions, 1, FWBTile(3, 4)));
	TestTrue(TEXT("South move generated"), ContainsTurnFlowMoveFromUnitTo(Actions, 1, FWBTile(4, 5)));
	TestFalse(TEXT("Occupied east move omitted"), ContainsTurnFlowMoveFromUnitTo(Actions, 1, FWBTile(5, 4)));
	TestFalse(TEXT("Walled north move omitted"), ContainsTurnFlowMoveFromUnitTo(Actions, 1, FWBTile(4, 3)));
	TestFalse(TEXT("Rooted unit move omitted"), ContainsTurnFlowMoveFromUnitTo(Actions, 3, FWBTile(1, 2)));
	TestFalse(TEXT("Stunned unit move omitted"), ContainsTurnFlowMoveFromUnitTo(Actions, 4, FWBTile(2, 3)));
	TestFalse(TEXT("No-MP unit move omitted"), ContainsTurnFlowMoveFromUnitTo(Actions, 5, FWBTile(5, 6)));

	if (Actions.Num() == 3)
	{
		TestTrue(TEXT("First generated action is movement"), Actions[0].Type == EWBActionType::Move);
		TestTrue(TEXT("Second generated action is movement"), Actions[1].Type == EWBActionType::Move);
		TestTrue(TEXT("End turn is last"), Actions.Last().Type == EWBActionType::EndTurn);
	}

	TArray<FWBAction> OutActions;
	WBRules::GenerateLegalActions(State, 0, OutActions);
	TestTrue(TEXT("Out-array legal generation matches return API"), ActionIdsEqual(Actions, OutActions));

	TestEqual(TEXT("Generation leaves current player unchanged"), State.CurrentPlayer, BeforeGeneration.CurrentPlayer);
	TestEqual(TEXT("Generation leaves priority unchanged"), State.PriorityPlayer, BeforeGeneration.PriorityPlayer);
	TestEqual(TEXT("Generation leaves turn unchanged"), State.TurnNumber, BeforeGeneration.TurnNumber);
	TestEqual(TEXT("Generation leaves unit 1 X unchanged"), State.GetUnitById(1)->X, BeforeGeneration.GetUnitById(1)->X);
	TestEqual(TEXT("Generation leaves unit 1 Y unchanged"), State.GetUnitById(1)->Y, BeforeGeneration.GetUnitById(1)->Y);

	FWBGameStateData GameOverState = State;
	GameOverState.bGameOver = true;
	TestEqual(TEXT("Game over state generates no actions"), WBRules::GenerateLegalActionsForPlayer(GameOverState, 0).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnFlowPassResponseRulesTest, "Wandbound.Core.TurnFlow.PassResponseRulesAndApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnFlowPassResponseRulesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData NormalState;
	NormalState.CurrentPlayer = 0;
	NormalState.PriorityPlayer = 0;
	NormalState.Phase = EWBGamePhase::NormalTurn;
	const FWBAction Player0PassResponse = MakeTurnFlowAction(EWBActionType::PassResponse, 0);

	const FWBActionQueryResult NormalPass = WBRules::CanPassResponse(NormalState, Player0PassResponse);
	TestFalse(TEXT("PassResponse is illegal in normal phase"), NormalPass.bOk);
	TestEqual(TEXT("Normal phase pass response reason"), NormalPass.Reason, FString(TEXT("not_response_phase")));

	FWBGameStateData ResponseState;
	ResponseState.CurrentPlayer = 0;
	ResponseState.PriorityPlayer = 1;
	ResponseState.Phase = EWBGamePhase::Response;
	ResponseState.TurnNumber = 6;
	const FWBAction PriorityPassResponse = MakeTurnFlowAction(EWBActionType::PassResponse, 1);

	TestTrue(TEXT("Priority player can pass response"), WBRules::CanPassResponse(ResponseState, PriorityPassResponse).bOk);
	TestFalse(TEXT("Non-priority player cannot pass response"), WBRules::CanPassResponse(ResponseState, Player0PassResponse).bOk);
	TestEqual(TEXT("Non-priority player gets no response actions"), WBRules::GenerateLegalActionsForPlayer(ResponseState, 0).Num(), 0);

	const TArray<FWBAction> PriorityActions = WBRules::GenerateLegalActionsForPlayer(ResponseState, 1);
	TestEqual(TEXT("Priority player gets one response action"), PriorityActions.Num(), 1);
	if (PriorityActions.Num() == 1)
	{
		TestTrue(TEXT("Generated response action is PassResponse"), PriorityActions[0].Type == EWBActionType::PassResponse);
		TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(PriorityActions[0]), FString(TEXT("pass_response:p1")));
	}

	FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(WBActionCodec::SerializeAction(PriorityPassResponse), ResponseState);
	TestTrue(TEXT("PassResponse decodes"), DecodeResult.bOk);
	TestEqual(TEXT("Decoded PassResponse type"), static_cast<int32>(DecodeResult.Action.Type), static_cast<int32>(EWBActionType::PassResponse));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPassResponse(ResponseState, PriorityPassResponse);
	TestTrue(TEXT("PassResponse applies"), Result.bOk);
	TestEqual(TEXT("PassResponse returns priority to current player"), ResponseState.PriorityPlayer, 0);
	TestTrue(TEXT("PassResponse returns state to normal phase"), ResponseState.IsNormalTurnPhase());
	TestEqual(TEXT("PassResponse leaves turn number unchanged"), ResponseState.TurnNumber, 6);
	TestEqual(TEXT("PassResponse emits one trace"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("PassResponse trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("pass_response")));
		TestEqual(TEXT("PassResponse trace player"), Result.TraceEvents[0].PlayerId, 1);
		TestEqual(TEXT("PassResponse trace from player"), Result.TraceEvents[0].FromPlayer, 1);
		TestEqual(TEXT("PassResponse trace to player"), Result.TraceEvents[0].ToPlayer, 0);
		TestEqual(TEXT("PassResponse trace turn number"), Result.TraceEvents[0].TurnNumber, 6);
		TestTrue(TEXT("PassResponse trace ok"), Result.TraceEvents[0].bOk);
	}

	FWBGameStateData FailedState;
	FailedState.CurrentPlayer = 0;
	FailedState.PriorityPlayer = 1;
	FailedState.Phase = EWBGamePhase::Response;
	FailedState.TurnNumber = 8;
	const FWBApplyActionResult FailedResult = WBEffectRunner::ApplyPassResponse(FailedState, Player0PassResponse);
	TestFalse(TEXT("Failed PassResponse returns failure"), FailedResult.bOk);
	TestEqual(TEXT("Failed PassResponse leaves current player"), FailedState.CurrentPlayer, 0);
	TestEqual(TEXT("Failed PassResponse leaves priority"), FailedState.PriorityPlayer, 1);
	TestTrue(TEXT("Failed PassResponse leaves response phase"), FailedState.IsResponsePhase());
	TestEqual(TEXT("Failed PassResponse leaves turn number"), FailedState.TurnNumber, 8);
	TestEqual(TEXT("Failed PassResponse emits no traces"), FailedResult.TraceEvents.Num(), 0);
	return true;
}

#endif
