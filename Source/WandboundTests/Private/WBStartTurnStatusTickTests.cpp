#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBAction.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeStatusPlayer(const int32 PlayerId, const int32 RemainingMP = 0)
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
	State.Players.Add(MakeStatusPlayer(0, 1));
	State.Players.Add(MakeStatusPlayer(1, 1));
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
		Player.LastMPRoll = ReadIntegerFieldOrDefault(PlayerObject, TEXT("last_mp_roll"), 0);
		Player.WallsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("walls_left"), 0);
		Player.WallRemovalsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("wall_removals_left"), 0);
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
		|| Kind != TEXT("apply_start_turn_status_ticks"))
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

				const int32 ExpectedTurns = static_cast<int32>(StatusTurnsPair.Value->AsNumber());
				if (Unit->GetStatusTurnsRemaining(FName(*StatusTurnsPair.Key)) != ExpectedTurns)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusHelperAliasesTest, "Wandbound.Core.StatusTicks.StatusHelperAliases", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusHelperAliasesTest::RunTest(const FString& Parameters)
{
	FWBUnitState Unit;
	Unit.Statuses.Add(FName(TEXT("root")));
	TestTrue(TEXT("Lowercase root aliases Rooted"), Unit.HasStatus(FName(TEXT("Rooted"))));

	Unit.AddStatus(FName(TEXT("Stunned")), 2);
	TestTrue(TEXT("Canonical Stunned aliases stun"), Unit.HasStatus(FName(TEXT("stun"))));
	TestEqual(TEXT("Stunned turns recorded"), Unit.GetStatusTurnsRemaining(FName(TEXT("stun"))), 2);

	Unit.SetStatusTurnsRemaining(FName(TEXT("stun")), 1);
	TestEqual(TEXT("Stunned turns update through alias"), Unit.GetStatusTurnsRemaining(FName(TEXT("Stunned"))), 1);

	Unit.RemoveStatus(FName(TEXT("stun")));
	TestFalse(TEXT("Stunned removed through alias"), Unit.HasStatus(FName(TEXT("Stunned"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPoisonReducesMaxHPTest, "Wandbound.Core.StatusTicks.PoisonReducesMaxHP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPoisonReducesMaxHPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Poisoned unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestEqual(TEXT("MaxHP reduced"), After->MaxHP, 4);
	TestEqual(TEXT("HP clamped to MaxHP"), After->HP, 4);
	TestTrue(TEXT("Poison remains"), After->HasStatus(FName(TEXT("Poison"))));
	TestEqual(TEXT("Poison duration decremented"), After->GetStatusTurnsRemaining(FName(TEXT("Poison"))), 1);
	TestEqual(TEXT("Parent and poison trace emitted"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("Parent trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("start_turn_status_ticks")));
		TestEqual(TEXT("Poison trace kind"), Result.TraceEvents[1].Kind, FName(TEXT("status_tick")));
		TestEqual(TEXT("Poison trace status"), Result.TraceEvents[1].StatusId, FName(TEXT("Poison")));
		TestEqual(TEXT("Poison trace target"), Result.TraceEvents[1].TargetUnitId, 1);
		TestEqual(TEXT("Poison trace previous MaxHP"), Result.TraceEvents[1].PreviousMaxHP, 5);
		TestEqual(TEXT("Poison trace new MaxHP"), Result.TraceEvents[1].NewMaxHP, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPoisonKeepsHPUnderNewMaxTest, "Wandbound.Core.StatusTicks.PoisonKeepsHPUnderNewMax", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPoisonKeepsHPUnderNewMaxTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 3, 5);
	Unit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Poisoned unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestEqual(TEXT("MaxHP reduced"), After->MaxHP, 4);
	TestEqual(TEXT("HP remains below MaxHP"), After->HP, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPoisonMinMaxHPIsOneTest, "Wandbound.Core.StatusTicks.PoisonMinMaxHPIsOne", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPoisonMinMaxHPIsOneTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 1, 1);
	Unit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Poisoned unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestEqual(TEXT("MaxHP remains at minimum one"), After->MaxHP, 1);
	TestEqual(TEXT("HP remains one"), After->HP, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBurnDoesNotTickAtStartTest, "Wandbound.Core.StatusTicks.BurnDoesNotTickAtStart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBurnDoesNotTickAtStartTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 4, 5);
	Unit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Burning unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), After->HP, 4);
	TestEqual(TEXT("MaxHP unchanged"), After->MaxHP, 5);
	TestTrue(TEXT("Burn remains"), After->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Burn duration unchanged at start"), After->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 2);
	TestEqual(TEXT("Only parent trace emitted"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStartTurnStatusTicksAllBoardPoisonTest, "Wandbound.Core.StatusTicks.AllBoardPoisonTicks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStartTurnStatusTicksAllBoardPoisonTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState PlayerUnit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	PlayerUnit.AddStatus(FName(TEXT("Poison")), 2);
	FWBUnitState OpponentUnit = MakeStatusUnit(2, 1, FWBTile(5, 4), 5, 5);
	OpponentUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Player unit added"), State.AddUnitForTest(PlayerUnit));
	TestTrue(TEXT("Opponent unit added"), State.AddUnitForTest(OpponentUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestEqual(TEXT("Player unit MaxHP ticks"), State.GetUnitById(1)->MaxHP, 4);
	TestEqual(TEXT("Opponent unit MaxHP also ticks per Godot all-board status pass"), State.GetUnitById(2)->MaxHP, 4);
	TestEqual(TEXT("Parent plus two poison traces emitted"), Result.TraceEvents.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPoisonPausedByFrozenTest, "Wandbound.Core.StatusTicks.PoisonPausedByFrozen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPoisonPausedByFrozenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Poison")), 2);
	Unit.AddStatus(FName(TEXT("Frozen")), 2);
	TestTrue(TEXT("Frozen poisoned unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), After->HP, 5);
	TestEqual(TEXT("MaxHP unchanged"), After->MaxHP, 5);
	TestEqual(TEXT("Poison duration unchanged while Frozen"), After->GetStatusTurnsRemaining(FName(TEXT("Poison"))), 2);
	TestEqual(TEXT("Only parent trace emitted"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStartTurnPoisonDurationExpiresTest, "Wandbound.Core.StatusTicks.PoisonDurationExpires", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStartTurnPoisonDurationExpiresTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Poison")), 1);
	TestTrue(TEXT("Poisoned unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	const FWBUnitState* After = State.GetUnitById(1);
	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestEqual(TEXT("MaxHP reduced before expiration"), After->MaxHP, 4);
	TestFalse(TEXT("Poison expired"), After->HasStatus(FName(TEXT("Poison"))));
	TestEqual(TEXT("Parent, poison tick, and expiration traces emitted"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Expiration trace kind"), Result.TraceEvents[2].Kind, FName(TEXT("status_expired")));
		TestEqual(TEXT("Expiration status"), Result.TraceEvents[2].StatusId, FName(TEXT("Poison")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRootedStunnedDoNotExpireAtStartTest, "Wandbound.Core.StatusTicks.RootedStunnedDoNotExpireAtStart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRootedStunnedDoNotExpireAtStartTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState RootedUnit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	RootedUnit.AddStatus(FName(TEXT("Rooted")), 1);
	FWBUnitState StunnedUnit = MakeStatusUnit(2, 0, FWBTile(5, 4), 5, 5);
	StunnedUnit.AddStatus(FName(TEXT("Stunned")), 1);
	TestTrue(TEXT("Rooted unit added"), State.AddUnitForTest(RootedUnit));
	TestTrue(TEXT("Stunned unit added"), State.AddUnitForTest(StunnedUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	TestTrue(TEXT("Status tick succeeds"), Result.bOk);
	TestTrue(TEXT("Rooted remains active at start"), State.GetUnitById(1)->HasStatus(FName(TEXT("Rooted"))));
	TestTrue(TEXT("Stunned remains active at start"), State.GetUnitById(2)->HasStatus(FName(TEXT("Stunned"))));
	TestEqual(TEXT("Rooted movement remains blocked"), WBRules::GenerateLegalMoveActions(State, 0, 1).Num(), 0);
	TestEqual(TEXT("Stunned movement remains blocked"), WBRules::GenerateLegalMoveActions(State, 0, 2).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStartTurnStatusTicksInvalidPlayerFailsTest, "Wandbound.Core.StatusTicks.InvalidPlayerFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStartTurnStatusTicksInvalidPlayerFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Poisoned unit added"), State.AddUnitForTest(Unit));
	const FWBGameStateData Before = State;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 2);

	TestFalse(TEXT("Invalid player fails"), Result.bOk);
	TestEqual(TEXT("Invalid player reason"), Result.Reason, FString(TEXT("bad_player")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(1)->HP, Before.GetUnitById(1)->HP);
	TestEqual(TEXT("MaxHP unchanged"), State.GetUnitById(1)->MaxHP, Before.GetUnitById(1)->MaxHP);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStartTurnStatusTicksGameOverFailsTest, "Wandbound.Core.StatusTicks.GameOverFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStartTurnStatusTicksGameOverFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusTickState();
	State.bGameOver = true;
	FWBUnitState Unit = MakeStatusUnit(1, 0, FWBTile(4, 4), 5, 5);
	Unit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Poisoned unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);

	TestFalse(TEXT("Game over fails"), Result.bOk);
	TestEqual(TEXT("Game over reason"), Result.Reason, FString(TEXT("game_over")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("MaxHP unchanged"), State.GetUnitById(1)->MaxHP, 5);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStartTurnStatusTickFixtureScenariosTest, "Wandbound.Core.StatusTicks.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStartTurnStatusTickFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("start_turn_poison_reduces_max_hp.json"),
		TEXT("start_turn_poison_clamps_hp_to_max.json"),
		TEXT("start_turn_burn_does_not_tick.json"),
		TEXT("start_turn_status_ticks_invalid_player_fails.json"),
		TEXT("start_turn_status_duration_expires.json")
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

		const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, PlayerId);

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
