#include "Misc/AutomationTest.h"

#include "WBGameStateData.h"
#include "WBResonanceLoad.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeRLPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = PlayerId == 0 ? 1 : 2;
	return Player;
}

FWBUnitState MakeRLUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 RLTotal,
	const int32 RLUsed)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("rl_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 1;
	Unit.AR = 1;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	return Unit;
}

FWBGameStateData MakeRLState(
	const int32 RLTotal = 5,
	const int32 RLUsed = 2)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeRLPlayer(0));
	State.Players.Add(MakeRLPlayer(1));
	State.AddUnitForTest(MakeRLUnit(1, 0, FWBTile(4, 4), RLTotal, RLUsed));
	return State;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadSummaryTest, "Wandbound.Core.ResonanceLoad.BuildUnitLoadSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadSummaryTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeRLState(5, 2);
	const FWBResonanceLoadSummary Summary = WBResonanceLoad::BuildUnitLoadSummary(State, 1);

	TestTrue(TEXT("Summary ok"), Summary.bOk);
	TestEqual(TEXT("Reason"), Summary.Reason, FString(TEXT("success")));
	TestEqual(TEXT("Unit id"), Summary.UnitId, 1);
	TestEqual(TEXT("Owner"), Summary.OwnerPlayerId, 0);
	TestEqual(TEXT("Current RL"), Summary.CurrentRL, 5);
	TestEqual(TEXT("RL used"), Summary.RLUsed, 2);
	TestEqual(TEXT("Available RL"), Summary.AvailableRL, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadCanPayWithinAvailableTest, "Wandbound.Core.ResonanceLoad.CanPayRRWithinAvailable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadCanPayWithinAvailableTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeRLState(5, 2);
	FWBResonanceLoadSummary Summary;

	TestTrue(TEXT("Can pay exact available"), WBResonanceLoad::CanPayRR(State, 1, 3, Summary));
	TestEqual(TEXT("Available RL"), Summary.AvailableRL, 3);
	TestEqual(TEXT("Reason"), Summary.Reason, FString(TEXT("success")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadCannotPayAboveAvailableTest, "Wandbound.Core.ResonanceLoad.CanPayRRExceedsAvailable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadCannotPayAboveAvailableTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeRLState(5, 2);
	FWBResonanceLoadSummary Summary;

	TestFalse(TEXT("Cannot pay above available"), WBResonanceLoad::CanPayRR(State, 1, 4, Summary));
	TestTrue(TEXT("Summary still ok"), Summary.bOk);
	TestEqual(TEXT("Available RL"), Summary.AvailableRL, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadInvalidRRTest, "Wandbound.Core.ResonanceLoad.InvalidRRFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadInvalidRRTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeRLState();
	FWBResonanceLoadSummary Summary;

	TestFalse(TEXT("Negative RR fails"), WBResonanceLoad::CanPayRR(State, 1, -1, Summary));
	TestFalse(TEXT("Summary not ok"), Summary.bOk);
	TestEqual(TEXT("Reason"), Summary.Reason, FString(TEXT("invalid_rr")));
	TestEqual(TEXT("Stable string"), WBResonanceLoad::ResultReasonToString(TEXT("invalid_rr")), FString(TEXT("invalid_rr")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadMissingUnitTest, "Wandbound.Core.ResonanceLoad.MissingUnitFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadMissingUnitTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeRLState();
	const FWBResonanceLoadSummary Summary = WBResonanceLoad::BuildUnitLoadSummary(State, 99);

	TestFalse(TEXT("Summary not ok"), Summary.bOk);
	TestEqual(TEXT("Reason"), Summary.Reason, FString(TEXT("unit_not_found")));
	TestEqual(TEXT("Unit id preserved"), Summary.UnitId, 99);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadInvalidStateTest, "Wandbound.Core.ResonanceLoad.InvalidNegativeStateFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadInvalidStateTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeRLState(-1, 0);
	const FWBResonanceLoadSummary Summary = WBResonanceLoad::BuildUnitLoadSummary(State, 1);

	TestFalse(TEXT("Summary not ok"), Summary.bOk);
	TestEqual(TEXT("Reason"), Summary.Reason, FString(TEXT("invalid_rl_state")));
	TestEqual(TEXT("Raw current RL"), Summary.CurrentRL, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadOverflowPendingTest, "Wandbound.Core.ResonanceLoad.OverflowPendingFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadOverflowPendingTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeRLState(2, 3);
	const FWBResonanceLoadSummary Summary = WBResonanceLoad::BuildUnitLoadSummary(State, 1);

	TestFalse(TEXT("Summary not ok"), Summary.bOk);
	TestEqual(TEXT("Reason"), Summary.Reason, FString(TEXT("overflow_pending")));
	TestEqual(TEXT("Raw RL used"), Summary.RLUsed, 3);
	TestEqual(TEXT("Available clamps to zero"), Summary.AvailableRL, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceLoadNoMutationTest, "Wandbound.Core.ResonanceLoad.DoesNotMutateGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceLoadNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLState(6, 1);
	const FWBUnitState* BeforeUnit = State.GetUnitById(1);
	TestNotNull(TEXT("Before unit"), BeforeUnit);
	if (BeforeUnit == nullptr)
	{
		return false;
	}

	const int32 BeforeRLTotal = BeforeUnit->RLTotal;
	const int32 BeforeRLUsed = BeforeUnit->RLUsed;
	const int32 BeforeUnitCount = State.Units.Num();
	FWBResonanceLoadSummary Summary;

	WBResonanceLoad::BuildUnitLoadSummary(State, 1);
	WBResonanceLoad::CanPayRR(State, 1, 2, Summary);

	const FWBUnitState* AfterUnit = State.GetUnitById(1);
	TestNotNull(TEXT("After unit"), AfterUnit);
	if (AfterUnit == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("Unit count unchanged"), State.Units.Num(), BeforeUnitCount);
	TestEqual(TEXT("RL total unchanged"), AfterUnit->RLTotal, BeforeRLTotal);
	TestEqual(TEXT("RL used unchanged"), AfterUnit->RLUsed, BeforeRLUsed);
	return true;
}

#endif
