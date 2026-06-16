#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeStatusPlayer(const int32 PlayerId, const int32 RemainingMP = 1)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	return Player;
}

FWBUnitState MakeStatusUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	return Unit;
}

FWBGameStateData MakeStatusTickState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeStatusPlayer(0));
	State.Players.Add(MakeStatusPlayer(1));
	return State;
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

bool ReadBoolFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const bool bDefaultValue)
{
	bool bValue = bDefaultValue;
	if (Object.IsValid())
	{
		Object->TryGetBoolField(FieldName, bValue);
	}
	return bValue;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString StatusTickFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadStatusTickFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *StatusTickFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

EWBGamePhase ReadPhaseOrDefault(const TSharedPtr<FJsonObject>& Object, const EWBGamePhase DefaultPhase)
{
	FString PhaseString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("phase"), PhaseString))
	{
		return DefaultPhase;
	}

	return PhaseString == TEXT("response") ? EWBGamePhase::Response : EWBGamePhase::NormalTurn;
}

bool ApplyFixturePlayers(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState)
{
	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("players"), Players) || Players == nullptr)
	{
		OutState.Players.Add(MakeStatusPlayer(0));
		OutState.Players.Add(MakeStatusPlayer(1));
		return true;
	}

	for (const TSharedPtr<FJsonValue>& PlayerValue : *Players)
	{
		const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
		if (!PlayerObject.IsValid())
		{
			return false;
		}

		FWBPlayerStateData Player;
		if (!TryReadIntegerField(PlayerObject, TEXT("player_id"), Player.PlayerId))
		{
			return false;
		}

		Player.RemainingMP = ReadIntegerFieldOrDefault(PlayerObject, TEXT("remaining_mp"), 0);
		OutState.Players.Add(Player);
	}

	return true;
}

bool ApplyFixtureUnits(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState)
{
	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("units"), Units) || Units == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& UnitValue : *Units)
	{
		const TSharedPtr<FJsonObject> UnitObject = UnitValue.IsValid() ? UnitValue->AsObject() : nullptr;
		if (!UnitObject.IsValid())
		{
			return false;
		}

		FWBUnitState Unit;
		if (!TryReadIntegerField(UnitObject, TEXT("id"), Unit.UnitId)
			|| !TryReadIntegerField(UnitObject, TEXT("owner_id"), Unit.OwnerId)
			|| !TryReadIntegerField(UnitObject, TEXT("x"), Unit.X)
			|| !TryReadIntegerField(UnitObject, TEXT("y"), Unit.Y))
		{
			return false;
		}

		Unit.HP = ReadIntegerFieldOrDefault(UnitObject, TEXT("hp"), Unit.HP);
		Unit.MaxHP = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_hp"), Unit.MaxHP);
		Unit.MPRemaining = ReadIntegerFieldOrDefault(UnitObject, TEXT("mp_remaining"), 0);
		if (!OutState.AddUnitForTest(Unit))
		{
			return false;
		}
	}

	return true;
}

void ApplyFixtureStatuses(FWBGameStateData& State, const TSharedPtr<FJsonObject>& StateObject)
{
	const TSharedPtr<FJsonObject>* StatusesByUnit = nullptr;
	if (!StateObject->TryGetObjectField(TEXT("statuses_by_unit"), StatusesByUnit)
		|| StatusesByUnit == nullptr
		|| !StatusesByUnit->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& UnitStatusesPair : (*StatusesByUnit)->Values)
	{
		FWBUnitState* Unit = State.GetMutableUnitById(FCString::Atoi(*UnitStatusesPair.Key));
		const TSharedPtr<FJsonObject> UnitStatuses = UnitStatusesPair.Value.IsValid()
			? UnitStatusesPair.Value->AsObject()
			: nullptr;
		if (Unit == nullptr || !UnitStatuses.IsValid())
		{
			continue;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& StatusPair : UnitStatuses->Values)
		{
			const TSharedPtr<FJsonObject> StatusObject = StatusPair.Value.IsValid()
				? StatusPair.Value->AsObject()
				: nullptr;
			FString StatusId = StatusPair.Key;
			int32 TurnsRemaining = 0;
			if (StatusObject.IsValid())
			{
				StatusObject->TryGetStringField(TEXT("status_id"), StatusId);
				TurnsRemaining = ReadIntegerFieldOrDefault(
					StatusObject,
					TEXT("turns_remaining"),
					ReadIntegerFieldOrDefault(StatusObject, TEXT("ticks_remaining"), 0));
			}
			Unit->AddStatus(FName(*StatusId), TurnsRemaining);
		}
	}
}

bool BuildStatusTickStateFromFixture(const TSharedPtr<FJsonObject>& Fixture, FWBGameStateData& OutState)
{
	const TSharedPtr<FJsonObject>* StateObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("initial_state"), StateObject)
		|| StateObject == nullptr
		|| !StateObject->IsValid())
	{
		return false;
	}

	OutState = FWBGameStateData();
	OutState.CurrentPlayer = ReadIntegerFieldOrDefault(*StateObject, TEXT("current_player"), 0);
	OutState.PriorityPlayer = ReadIntegerFieldOrDefault(*StateObject, TEXT("priority_player"), OutState.CurrentPlayer);
	OutState.TurnNumber = ReadIntegerFieldOrDefault(*StateObject, TEXT("turn_number"), 1);
	OutState.Phase = ReadPhaseOrDefault(*StateObject, EWBGamePhase::NormalTurn);
	OutState.bGameOver = ReadBoolFieldOrDefault(*StateObject, TEXT("game_over"), false);

	if (!ApplyFixturePlayers(*StateObject, OutState) || !ApplyFixtureUnits(*StateObject, OutState))
	{
		return false;
	}

	ApplyFixtureStatuses(OutState, *StateObject);
	return true;
}

bool ReadFixtureOperation(const TSharedPtr<FJsonObject>& Fixture, int32& OutPlayerId)
{
	const TSharedPtr<FJsonObject>* OperationObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("operation"), OperationObject)
		|| OperationObject == nullptr
		|| !OperationObject->IsValid())
	{
		return false;
	}

	FString Kind;
	if (!(*OperationObject)->TryGetStringField(TEXT("kind"), Kind)
		|| Kind != TEXT("apply_end_turn_status_ticks"))
	{
		return false;
	}

	return TryReadIntegerField(*OperationObject, TEXT("player_id"), OutPlayerId);
}

bool ExpectedUnitsMatch(const TSharedPtr<FJsonObject>& Fixture, const FWBGameStateData& State, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		|| ExpectObject == nullptr
		|| !ExpectObject->IsValid())
	{
		OutReason = TEXT("missing_expect");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* FinalUnits = nullptr;
	if (!(*ExpectObject)->TryGetArrayField(TEXT("final_units"), FinalUnits) || FinalUnits == nullptr)
	{
		OutReason = TEXT("missing_expect_final_units");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& UnitValue : *FinalUnits)
	{
		const TSharedPtr<FJsonObject> UnitObject = UnitValue.IsValid() ? UnitValue->AsObject() : nullptr;
		int32 UnitId = -1;
		if (!UnitObject.IsValid() || !TryReadIntegerField(UnitObject, TEXT("id"), UnitId))
		{
			OutReason = TEXT("malformed_expected_unit");
			return false;
		}

		const FWBUnitState* Unit = State.GetUnitById(UnitId);
		if (Unit == nullptr)
		{
			OutReason = FString::Printf(TEXT("missing_unit_%d"), UnitId);
			return false;
		}

		if (Unit->HP != ReadIntegerFieldOrDefault(UnitObject, TEXT("hp"), Unit->HP)
			|| Unit->MaxHP != ReadIntegerFieldOrDefault(UnitObject, TEXT("max_hp"), Unit->MaxHP))
		{
			OutReason = FString::Printf(TEXT("unit_%d_hp_mismatch"), UnitId);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Statuses = nullptr;
		if (UnitObject->TryGetArrayField(TEXT("statuses"), Statuses) && Statuses != nullptr)
		{
			if (Unit->Statuses.Num() != Statuses->Num())
			{
				OutReason = FString::Printf(TEXT("unit_%d_status_count_mismatch"), UnitId);
				return false;
			}

			for (const TSharedPtr<FJsonValue>& StatusValue : *Statuses)
			{
				if (!StatusValue.IsValid() || StatusValue->Type != EJson::String || !Unit->HasStatus(FName(*StatusValue->AsString())))
				{
					OutReason = FString::Printf(TEXT("unit_%d_status_mismatch"), UnitId);
					return false;
				}
			}
		}

		const TSharedPtr<FJsonObject>* StatusTurns = nullptr;
		if (UnitObject->TryGetObjectField(TEXT("status_turns"), StatusTurns)
			&& StatusTurns != nullptr
			&& StatusTurns->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& StatusTurnsPair : (*StatusTurns)->Values)
			{
				if (!StatusTurnsPair.Value.IsValid() || StatusTurnsPair.Value->Type != EJson::Number)
				{
					OutReason = FString::Printf(TEXT("unit_%d_malformed_status_turns"), UnitId);
					return false;
				}

				if (Unit->GetStatusTurnsRemaining(FName(*StatusTurnsPair.Key)) != static_cast<int32>(StatusTurnsPair.Value->AsNumber()))
				{
					OutReason = FString::Printf(TEXT("unit_%d_status_turns_mismatch"), UnitId);
					return false;
				}
			}
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectedTraceMatches(const TSharedPtr<FJsonObject>& Fixture, const TArray<FWBTraceEvent>& TraceEvents, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		|| ExpectObject == nullptr
		|| !ExpectObject->IsValid())
	{
		OutReason = TEXT("missing_expect");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedEvents = nullptr;
	if (!(*ExpectObject)->TryGetArrayField(TEXT("trace_events"), ExpectedEvents) || ExpectedEvents == nullptr)
	{
		OutReason = TEXT("missing_expect_trace_events");
		return false;
	}

	if (TraceEvents.Num() != ExpectedEvents->Num())
	{
		OutReason = FString::Printf(TEXT("trace_count_expected_%d_got_%d"), ExpectedEvents->Num(), TraceEvents.Num());
		return false;
	}

	for (int32 Index = 0; Index < ExpectedEvents->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> ExpectedEvent = (*ExpectedEvents)[Index].IsValid() ? (*ExpectedEvents)[Index]->AsObject() : nullptr;
		if (!ExpectedEvent.IsValid())
		{
			OutReason = TEXT("malformed_expected_trace_event");
			return false;
		}

		const FWBTraceEvent& Event = TraceEvents[Index];
		FString ExpectedKind;
		if (!ExpectedEvent->TryGetStringField(TEXT("kind"), ExpectedKind) || Event.Kind.ToString() != ExpectedKind)
		{
			OutReason = TEXT("trace_kind_mismatch");
			return false;
		}

		FString ExpectedStatusId;
		if (ExpectedEvent->TryGetStringField(TEXT("status_id"), ExpectedStatusId) && Event.StatusId.ToString() != ExpectedStatusId)
		{
			OutReason = TEXT("trace_status_id_mismatch");
			return false;
		}

		bool bExpectedExpired = false;
		if (ExpectedEvent->TryGetBoolField(TEXT("expired_status"), bExpectedExpired) && Event.bExpiredStatus != bExpectedExpired)
		{
			OutReason = TEXT("trace_expired_status_mismatch");
			return false;
		}

		bool bExpectedAtOrBelowZero = false;
		if (ExpectedEvent->TryGetBoolField(TEXT("at_or_below_zero_hp"), bExpectedAtOrBelowZero)
			&& Event.bAtOrBelowZeroHP != bExpectedAtOrBelowZero)
		{
			OutReason = TEXT("trace_zero_hp_mismatch");
			return false;
		}

		if (Event.PlayerId != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("player_id"), Event.PlayerId)
			|| Event.TurnNumber != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("turn_number"), Event.TurnNumber)
			|| Event.TargetUnitId != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("target_unit_id"), Event.TargetUnitId)
			|| Event.PreviousHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("previous_hp"), Event.PreviousHP)
			|| Event.NewHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("new_hp"), Event.NewHP)
			|| Event.PreviousMaxHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("previous_max_hp"), Event.PreviousMaxHP)
			|| Event.NewMaxHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("new_max_hp"), Event.NewMaxHP)
			|| Event.PreviousStatusTurns != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("previous_status_turns"), Event.PreviousStatusTurns)
			|| Event.NewStatusTurns != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("new_status_turns"), Event.NewStatusTurns))
		{
			OutReason = TEXT("trace_field_mismatch");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnBurnDealsDamageTest, "Wandbound.Core.EndTurnStatusTicks.BurnDealsDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnBurnDealsDamageTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Burning unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestEqual(TEXT("Burn deals one damage"), After->HP, 4);
	TestEqual(TEXT("Burn does not change MaxHP"), After->MaxHP, 5);
	TestTrue(TEXT("Burn remains"), After->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Burn duration decremented"), After->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 1);
	TestEqual(TEXT("Parent plus burn trace emitted"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("Parent trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("end_turn_status_ticks")));
		TestEqual(TEXT("Burn trace kind"), Result.TraceEvents[1].Kind, FName(TEXT("status_tick")));
		TestEqual(TEXT("Burn trace status"), Result.TraceEvents[1].StatusId, FName(TEXT("Burn")));
		TestEqual(TEXT("Burn previous HP"), Result.TraceEvents[1].PreviousHP, 5);
		TestEqual(TEXT("Burn new HP"), Result.TraceEvents[1].NewHP, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnBurnCanDropHPToZeroTest, "Wandbound.Core.EndTurnStatusTicks.BurnCanDropHPToZero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnBurnCanDropHPToZeroTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 1, 5);
	Unit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Burning unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestTrue(TEXT("Unit remains because death handling is out of scope"), After != nullptr);
	TestEqual(TEXT("Burn clamps HP at zero"), After->HP, 0);
	TestEqual(TEXT("Trace marks zero HP"), Result.TraceEvents[1].bAtOrBelowZeroHP, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnBurnDurationExpiresTest, "Wandbound.Core.EndTurnStatusTicks.BurnDurationExpires", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnBurnDurationExpiresTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Burn")), 1);
	TestTrue(TEXT("Burning unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestEqual(TEXT("Burn damage applies before expiration"), After->HP, 4);
	TestFalse(TEXT("Burn expired"), After->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Parent, burn tick, and expiration traces emitted"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Expiration trace kind"), Result.TraceEvents[2].Kind, FName(TEXT("status_expired")));
		TestTrue(TEXT("Expiration trace flag"), Result.TraceEvents[2].bExpiredStatus);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnPoisonDoesNotTickTest, "Wandbound.Core.EndTurnStatusTicks.PoisonDoesNotTick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnPoisonDoesNotTickTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Poison")), 1);
	TestTrue(TEXT("Poisoned unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), After->HP, 5);
	TestEqual(TEXT("MaxHP unchanged"), After->MaxHP, 5);
	TestTrue(TEXT("Poison remains"), After->HasStatus(FName(TEXT("Poison"))));
	TestEqual(TEXT("Poison duration unchanged"), After->GetStatusTurnsRemaining(FName(TEXT("Poison"))), 1);
	TestEqual(TEXT("Only parent trace emitted"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnRootedDurationExpiresTest, "Wandbound.Core.EndTurnStatusTicks.RootedDurationExpires", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnRootedDurationExpiresTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Rooted")), 1);
	TestTrue(TEXT("Rooted unit added"), State.AddUnitForTest(Unit));
	TestEqual(TEXT("Rooted unit cannot move before expiration"), WBRules::GenerateLegalMoveActions(State, 0, 1).Num(), 0);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestFalse(TEXT("Rooted expired"), After->HasStatus(FName(TEXT("Rooted"))));
	TestTrue(TEXT("Legal movement returns after Rooted expires"), WBRules::GenerateLegalMoveActions(State, 0, 1).Num() > 0);
	TestEqual(TEXT("Parent plus expiration trace emitted"), Result.TraceEvents.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnStunnedDurationExpiresTest, "Wandbound.Core.EndTurnStatusTicks.StunnedDurationExpires", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnStunnedDurationExpiresTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Stunned")), 1);
	TestTrue(TEXT("Stunned unit added"), State.AddUnitForTest(Unit));
	TestEqual(TEXT("Stunned unit cannot move before expiration"), WBRules::GenerateLegalMoveActions(State, 0, 1).Num(), 0);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestFalse(TEXT("Stunned expired"), After->HasStatus(FName(TEXT("Stunned"))));
	TestTrue(TEXT("Legal movement returns after Stunned expires"), WBRules::GenerateLegalMoveActions(State, 0, 1).Num() > 0);
	TestEqual(TEXT("Parent plus expiration trace emitted"), Result.TraceEvents.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnPermanentStatusesDoNotExpireTest, "Wandbound.Core.EndTurnStatusTicks.PermanentStatusesDoNotExpire", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnPermanentStatusesDoNotExpireTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState RootedUnit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	RootedUnit.AddStatus(FName(TEXT("Rooted")));
	FWBUnitState StunnedUnit = MakeStatusUnit(2, 0, FWBTile(5, 4), 5, 5);
	StunnedUnit.AddStatus(FName(TEXT("Stunned")));
	TestTrue(TEXT("Rooted unit added"), State.AddUnitForTest(RootedUnit));
	TestTrue(TEXT("Stunned unit added"), State.AddUnitForTest(StunnedUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestTrue(TEXT("Untimed Rooted remains"), State.GetUnitById(1)->HasStatus(FName(TEXT("Rooted"))));
	TestTrue(TEXT("Untimed Stunned remains"), State.GetUnitById(2)->HasStatus(FName(TEXT("Stunned"))));
	TestEqual(TEXT("Only parent trace emitted"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnFrozenDurationExpiresTest, "Wandbound.Core.EndTurnStatusTicks.FrozenDurationExpires", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnFrozenDurationExpiresTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Frozen")), 1);
	TestTrue(TEXT("Frozen unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestFalse(TEXT("Frozen expired"), State.GetUnitById(1)->HasStatus(FName(TEXT("Frozen"))));
	TestEqual(TEXT("Parent plus expiration trace emitted"), Result.TraceEvents.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnStatusTicksAllBoardBurnTest, "Wandbound.Core.EndTurnStatusTicks.AllBoardBurnTicks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnStatusTicksAllBoardBurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState PlayerUnit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	PlayerUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState OpponentUnit = MakeStatusUnit(2, 1, FWBTile(5, 4), 5, 5);
	OpponentUnit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Player unit added"), State.AddUnitForTest(PlayerUnit));
	TestTrue(TEXT("Opponent unit added"), State.AddUnitForTest(OpponentUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	TestTrue(TEXT("End-turn status tick succeeds"), Result.bOk);
	TestEqual(TEXT("Player unit takes Burn"), State.GetUnitById(1)->HP, 4);
	TestEqual(TEXT("Opponent unit also takes Burn per Godot all-board status pass"), State.GetUnitById(2)->HP, 4);
	TestEqual(TEXT("Parent plus two Burn traces emitted"), Result.TraceEvents.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnStatusTicksInvalidPlayerFailsTest, "Wandbound.Core.EndTurnStatusTicks.InvalidPlayerFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnStatusTicksInvalidPlayerFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Burning unit added"), State.AddUnitForTest(Unit));
	const FWBGameStateData Before = State;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 2);

	TestFalse(TEXT("Invalid player fails"), Result.bOk);
	TestEqual(TEXT("Invalid player reason"), Result.Reason, FString(TEXT("bad_player")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(1)->HP, Before.GetUnitById(1)->HP);
	TestEqual(TEXT("MaxHP unchanged"), State.GetUnitById(1)->MaxHP, Before.GetUnitById(1)->MaxHP);
	TestTrue(TEXT("Burn remains"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnStatusTicksGameOverFailsTest, "Wandbound.Core.EndTurnStatusTicks.GameOverFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnStatusTicksGameOverFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	State.bGameOver = true;
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Burning unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);

	TestFalse(TEXT("Game over fails"), Result.bOk);
	TestEqual(TEXT("Game over reason"), Result.Reason, FString(TEXT("game_over")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("MaxHP unchanged"), State.GetUnitById(1)->MaxHP, 5);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnStatusTickFixtureScenariosTest, "Wandbound.Core.EndTurnStatusTicks.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnStatusTickFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("end_turn_burn_deals_damage.json"),
		TEXT("end_turn_burn_duration_expires.json"),
		TEXT("end_turn_poison_does_not_tick.json"),
		TEXT("end_turn_rooted_duration_expires.json"),
		TEXT("end_turn_stunned_duration_expires.json"),
		TEXT("end_turn_status_ticks_invalid_player_fails.json"),
		TEXT("end_turn_frozen_duration_expires.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadStatusTickFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FWBGameStateData State;
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), BuildStatusTickStateFromFixture(Fixture, State));

		int32 PlayerId = -1;
		TestTrue(*FString::Printf(TEXT("%s operation reads"), *FixtureName), ReadFixtureOperation(Fixture, PlayerId));

		const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, PlayerId);

		const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
		bool bExpectedOk = false;
		FString ExpectedReason;
		TestTrue(*FString::Printf(TEXT("%s expect object exists"), *FixtureName), Fixture->TryGetObjectField(TEXT("expect"), ExpectObject));
		if (ExpectObject != nullptr && ExpectObject->IsValid())
		{
			(*ExpectObject)->TryGetBoolField(TEXT("ok"), bExpectedOk);
			(*ExpectObject)->TryGetStringField(TEXT("reason"), ExpectedReason);
		}

		TestEqual(*FString::Printf(TEXT("%s result ok"), *FixtureName), Result.bOk, bExpectedOk);
		TestEqual(*FString::Printf(TEXT("%s result reason"), *FixtureName), Result.Reason, ExpectedReason);

		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s final units match: %s"), *FixtureName, *StateReason), ExpectedUnitsMatch(Fixture, State, StateReason));

		FString TraceReason;
		TestTrue(*FString::Printf(TEXT("%s trace matches: %s"), *FixtureName, *TraceReason), ExpectedTraceMatches(Fixture, Result.TraceEvents, TraceReason));
	}

	return true;
}

#endif
