#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBAction.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBUnitState MakeUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile, const int32 MPRemaining)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.MPRemaining = MPRemaining;
	return Unit;
}

FWBAction MakeMoveAction(const int32 PlayerId, const int32 SourceUnitId, const FWBTile& FromTile, const FWBTile& ToTile)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = SourceUnitId;
	Action.FromTile = FromTile;
	Action.ToTile = ToTile;
	return Action;
}

FWBAction MakeTurnControlAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

bool ContainsMoveTo(const TArray<FWBAction>& Actions, const FWBTile& Tile)
{
	for (const FWBAction& Action : Actions)
	{
		if (Action.Type == EWBActionType::Move && Action.ToTile == Tile)
		{
			return true;
		}
	}

	return false;
}

bool ContainsMoveFromUnitTo(const TArray<FWBAction>& Actions, const int32 UnitId, const FWBTile& Tile)
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

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

TArray<TSharedPtr<FJsonValue>> ParseJsonArray(const FString& Json)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Values);
	return Values;
}

FString GoldenScenarioPath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadGoldenScenarioFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString FixtureJson;
	if (!FFileHelper::LoadFileToString(FixtureJson, *GoldenScenarioPath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(FixtureJson);
	return OutFixture.IsValid();
}

TArray<FString> GetReplayFixtureNames()
{
	TArray<FString> FixtureNames;
	FixtureNames.Add(TEXT("replay_movement_end_turn.json"));
	FixtureNames.Add(TEXT("replay_pass_response.json"));
	FixtureNames.Add(TEXT("replay_two_moves_end_turn.json"));
	FixtureNames.Add(TEXT("replay_multi_unit_order_end_turn.json"));
	FixtureNames.Add(TEXT("replay_player_one_movement_end_turn.json"));
	FixtureNames.Add(TEXT("replay_player_zero_pass_response.json"));
	return FixtureNames;
}

TArray<FString> GetMovementFixtureNames()
{
	TArray<FString> FixtureNames;
	FixtureNames.Add(TEXT("movement_valid_adjacent.json"));
	FixtureNames.Add(TEXT("movement_blocked_by_wall.json"));
	FixtureNames.Add(TEXT("movement_blocked_by_unit.json"));
	FixtureNames.Add(TEXT("movement_rooted_fails.json"));
	FixtureNames.Add(TEXT("movement_stunned_fails.json"));
	return FixtureNames;
}

TArray<FString> GetLegalActionFamilyFixtureNames()
{
	TArray<FString> FixtureNames;
	for (const FString& MovementFixtureName : GetMovementFixtureNames())
	{
		FixtureNames.Add(MovementFixtureName);
	}
	FixtureNames.Add(TEXT("action_generation_active_player_movement_first.json"));
	for (const FString& ReplayFixtureName : GetReplayFixtureNames())
	{
		FixtureNames.Add(ReplayFixtureName);
	}
	return FixtureNames;
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

bool ParseFixtureTile(const TSharedPtr<FJsonObject>& Object, FWBTile& OutTile)
{
	return TryReadIntegerField(Object, TEXT("x"), OutTile.X)
		&& TryReadIntegerField(Object, TEXT("y"), OutTile.Y);
}

void ApplyFixtureStatuses(FWBGameStateData& State, const TSharedPtr<FJsonObject>& InitialStateObject)
{
	const TSharedPtr<FJsonObject>* StatusesByUnit = nullptr;
	if (!InitialStateObject->TryGetObjectField(TEXT("statuses_by_unit"), StatusesByUnit)
		|| StatusesByUnit == nullptr
		|| !StatusesByUnit->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& UnitStatusesPair : (*StatusesByUnit)->Values)
	{
		const int32 UnitId = FCString::Atoi(*UnitStatusesPair.Key);
		FWBUnitState* Unit = State.GetMutableUnitById(UnitId);
		const TSharedPtr<FJsonObject> UnitStatuses = UnitStatusesPair.Value.IsValid()
			? UnitStatusesPair.Value->AsObject()
			: nullptr;
		if (Unit == nullptr || !UnitStatuses.IsValid())
		{
			continue;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& StatusPair : UnitStatuses->Values)
		{
			Unit->Statuses.Add(FName(*StatusPair.Key));
		}
	}
}

bool BuildGameStateFromFixture(const TSharedPtr<FJsonObject>& Fixture, FWBGameStateData& OutState)
{
	const TSharedPtr<FJsonObject>* InitialStateObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("initial_state"), InitialStateObject)
		|| InitialStateObject == nullptr
		|| !InitialStateObject->IsValid())
	{
		return false;
	}

	OutState = FWBGameStateData();
	int32 CurrentPlayer = 0;
	if (!TryReadIntegerField(*InitialStateObject, TEXT("current_player"), CurrentPlayer))
	{
		TryReadIntegerField(*InitialStateObject, TEXT("active_player"), CurrentPlayer);
	}
	OutState.CurrentPlayer = CurrentPlayer;
	OutState.PriorityPlayer = ReadIntegerFieldOrDefault(*InitialStateObject, TEXT("priority_player"), CurrentPlayer);
	OutState.TurnNumber = ReadIntegerFieldOrDefault(*InitialStateObject, TEXT("turn_number"), 1);

	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if ((*InitialStateObject)->TryGetArrayField(TEXT("units"), Units) && Units != nullptr)
	{
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
			Unit.CardId = UnitObject->GetStringField(TEXT("card_id"));
			Unit.MPRemaining = ReadIntegerFieldOrDefault(UnitObject, TEXT("mp_remaining"), 1);

			if (!OutState.AddUnitForTest(Unit))
			{
				return false;
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
	if ((*InitialStateObject)->TryGetArrayField(TEXT("walls"), Walls) && Walls != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& WallValue : *Walls)
		{
			const TSharedPtr<FJsonObject> WallObject = WallValue.IsValid() ? WallValue->AsObject() : nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Between = nullptr;
			if (!WallObject.IsValid()
				|| !WallObject->TryGetArrayField(TEXT("between"), Between)
				|| Between == nullptr
				|| Between->Num() != 2)
			{
				return false;
			}

			FWBTile A;
			FWBTile B;
			if (!ParseFixtureTile((*Between)[0]->AsObject(), A) || !ParseFixtureTile((*Between)[1]->AsObject(), B))
			{
				return false;
			}
			OutState.AddWallForTest(FWBWallEdge(A, B));
		}
	}

	ApplyFixtureStatuses(OutState, *InitialStateObject);
	return true;
}

FString SerializeFixtureObject(const TSharedPtr<FJsonObject>& Object)
{
	FString Output;
	if (!Object.IsValid())
	{
		return Output;
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Output;
}

bool ExtractFixtureActionJson(const TSharedPtr<FJsonObject>& Fixture, FString& OutActionJson)
{
	const TSharedPtr<FJsonObject>* ActionObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("action"), ActionObject)
		|| ActionObject == nullptr
		|| !ActionObject->IsValid())
	{
		return false;
	}

	OutActionJson = SerializeFixtureObject(*ActionObject);
	return !OutActionJson.IsEmpty();
}

bool TryGetExpectedObject(const TSharedPtr<FJsonObject>& Fixture, TSharedPtr<FJsonObject>& OutExpectObject)
{
	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		|| ExpectObject == nullptr
		|| !ExpectObject->IsValid())
	{
		return false;
	}

	OutExpectObject = *ExpectObject;
	return true;
}

bool TryReadBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Boolean)
	{
		return false;
	}

	OutValue = (*Value)->AsBool();
	return true;
}

bool StringArraysEqual(const TArray<FString>& A, const TArray<FString>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		if (A[Index] != B[Index])
		{
			return false;
		}
	}

	return true;
}

FString JoinStringsForTest(const TArray<FString>& Values)
{
	return FString::Join(Values, TEXT(", "));
}

bool ReadStringArrayValuesForTest(
	const TArray<TSharedPtr<FJsonValue>>& Values,
	const FString& FieldPath,
	TArray<FString>& OutStrings,
	FString& OutReason)
{
	OutStrings.Reset();
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = Values[Index];
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			OutReason = FString::Printf(TEXT("malformed_%s[%d]"), *FieldPath, Index);
			return false;
		}

		OutStrings.Add(Value->AsString());
	}

	OutReason.Reset();
	return true;
}

bool ReadMovementFixtureExpectations(
	const TSharedPtr<FJsonObject>& Fixture,
	bool& bOutExpectedLegal,
	FString& OutExpectedReason,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectObject;
	if (!TryGetExpectedObject(Fixture, ExpectObject))
	{
		OutReason = TEXT("missing_expect");
		return false;
	}

	if (!TryReadBoolField(ExpectObject, TEXT("legal"), bOutExpectedLegal))
	{
		OutReason = TEXT("missing_expect_legal");
		return false;
	}

	if (!ExpectObject->TryGetStringField(TEXT("reason"), OutExpectedReason))
	{
		OutReason = TEXT("missing_expect_reason");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ReadExpectedStringArrayField(
	const TSharedPtr<FJsonObject>& Fixture,
	const TCHAR* FieldName,
	TArray<FString>& OutActionIds,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectObject;
	if (!TryGetExpectedObject(Fixture, ExpectObject))
	{
		OutReason = TEXT("missing_expect");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ActionIdValues = nullptr;
	if (!ExpectObject->TryGetArrayField(FieldName, ActionIdValues) || ActionIdValues == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_expect_%s"), FieldName);
		return false;
	}

	OutActionIds.Reset();
	return ReadStringArrayValuesForTest(*ActionIdValues, FString::Printf(TEXT("expect_%s"), FieldName), OutActionIds, OutReason);
}

bool ReadExpectedLegalMoveActionIds(
	const TSharedPtr<FJsonObject>& Fixture,
	TArray<FString>& OutActionIds,
	FString& OutReason)
{
	return ReadExpectedStringArrayField(Fixture, TEXT("legal_move_action_ids"), OutActionIds, OutReason);
}

bool ReadExpectedLegalActionIds(
	const TSharedPtr<FJsonObject>& Fixture,
	TArray<FString>& OutActionIds,
	FString& OutReason)
{
	return ReadExpectedStringArrayField(Fixture, TEXT("legal_action_ids"), OutActionIds, OutReason);
}

bool MovementFixtureFinalUnitsMatch(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectObject;
	if (!TryGetExpectedObject(Fixture, ExpectObject))
	{
		OutReason = TEXT("missing_expect");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* FinalUnits = nullptr;
	if (!ExpectObject->TryGetArrayField(TEXT("final_units"), FinalUnits) || FinalUnits == nullptr)
	{
		OutReason = TEXT("missing_expect_final_units");
		return false;
	}

	if (State.Units.Num() != FinalUnits->Num())
	{
		OutReason = FString::Printf(TEXT("unit count expected %d got %d"), FinalUnits->Num(), State.Units.Num());
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FinalUnitValue : *FinalUnits)
	{
		const TSharedPtr<FJsonObject> FinalUnitObject = FinalUnitValue.IsValid() ? FinalUnitValue->AsObject() : nullptr;
		int32 UnitId = -1;
		int32 ExpectedX = -1;
		int32 ExpectedY = -1;
		if (!TryReadIntegerField(FinalUnitObject, TEXT("id"), UnitId)
			|| !TryReadIntegerField(FinalUnitObject, TEXT("x"), ExpectedX)
			|| !TryReadIntegerField(FinalUnitObject, TEXT("y"), ExpectedY))
		{
			OutReason = TEXT("malformed_expect_final_unit");
			return false;
		}

		const FWBUnitState* Unit = State.GetUnitById(UnitId);
		if (Unit == nullptr)
		{
			OutReason = FString::Printf(TEXT("missing_unit:%d"), UnitId);
			return false;
		}

		if (Unit->X != ExpectedX || Unit->Y != ExpectedY)
		{
			OutReason = FString::Printf(TEXT("unit %d tile expected (%d,%d) got (%d,%d)"), UnitId, ExpectedX, ExpectedY, Unit->X, Unit->Y);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

FString JsonTypeName(const EJson Type)
{
	switch (Type)
	{
	case EJson::None:
		return TEXT("none");
	case EJson::Null:
		return TEXT("null");
	case EJson::String:
		return TEXT("string");
	case EJson::Number:
		return TEXT("number");
	case EJson::Boolean:
		return TEXT("boolean");
	case EJson::Array:
		return TEXT("array");
	case EJson::Object:
		return TEXT("object");
	default:
		return TEXT("unknown");
	}
}

void AddSchemaError(TArray<FString>& Errors, const FString& Path, const FString& Message)
{
	Errors.Add(FString::Printf(TEXT("%s: %s"), *Path, *Message));
}

bool ValidateTypedField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Path,
	const TCHAR* FieldName,
	const EJson ExpectedType,
	TArray<FString>& Errors)
{
	if (!Object.IsValid())
	{
		AddSchemaError(Errors, Path, TEXT("parent is not an object"));
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	const FString FieldPath = FString::Printf(TEXT("%s.%s"), *Path, FieldName);
	if (Value == nullptr || !Value->IsValid())
	{
		AddSchemaError(Errors, FieldPath, TEXT("missing required field"));
		return false;
	}

	if ((*Value)->Type != ExpectedType)
	{
		AddSchemaError(
			Errors,
			FieldPath,
			FString::Printf(TEXT("expected %s, got %s"), *JsonTypeName(ExpectedType), *JsonTypeName((*Value)->Type)));
		return false;
	}

	return true;
}

void ValidateOptionalTypedField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Path,
	const TCHAR* FieldName,
	const EJson ExpectedType,
	TArray<FString>& Errors)
{
	if (!Object.IsValid())
	{
		AddSchemaError(Errors, Path, TEXT("parent is not an object"));
		return;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr)
	{
		return;
	}

	const FString FieldPath = FString::Printf(TEXT("%s.%s"), *Path, FieldName);
	if (!Value->IsValid())
	{
		AddSchemaError(Errors, FieldPath, TEXT("missing value"));
		return;
	}

	if ((*Value)->Type != ExpectedType)
	{
		AddSchemaError(
			Errors,
			FieldPath,
			FString::Printf(TEXT("expected %s, got %s"), *JsonTypeName(ExpectedType), *JsonTypeName((*Value)->Type)));
	}
}

bool ValidateObjectField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Path,
	const TCHAR* FieldName,
	TSharedPtr<FJsonObject>& OutObject,
	TArray<FString>& Errors)
{
	if (!ValidateTypedField(Object, Path, FieldName, EJson::Object, Errors))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ObjectField = nullptr;
	if (!Object->TryGetObjectField(FieldName, ObjectField) || ObjectField == nullptr || !ObjectField->IsValid())
	{
		AddSchemaError(Errors, FString::Printf(TEXT("%s.%s"), *Path, FieldName), TEXT("could not read object field"));
		return false;
	}

	OutObject = *ObjectField;
	return true;
}

bool ValidateArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Path,
	const TCHAR* FieldName,
	const TArray<TSharedPtr<FJsonValue>>*& OutArray,
	TArray<FString>& Errors)
{
	if (!ValidateTypedField(Object, Path, FieldName, EJson::Array, Errors))
	{
		return false;
	}

	if (!Object->TryGetArrayField(FieldName, OutArray) || OutArray == nullptr)
	{
		AddSchemaError(Errors, FString::Printf(TEXT("%s.%s"), *Path, FieldName), TEXT("could not read array field"));
		return false;
	}

	return true;
}

void ValidateStringArrayValues(const TArray<TSharedPtr<FJsonValue>>& Values, const FString& Path, TArray<FString>& Errors)
{
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = Values[Index];
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			AddSchemaError(
				Errors,
				FString::Printf(TEXT("%s[%d]"), *Path, Index),
				Value.IsValid()
					? FString::Printf(TEXT("expected string, got %s"), *JsonTypeName(Value->Type))
					: FString(TEXT("missing value")));
		}
	}
}

void ValidateFixtureUnitsAllowOptionalMP(const TSharedPtr<FJsonObject>& StateObject, const FString& Path, TArray<FString>& Errors)
{
	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if (!ValidateArrayField(StateObject, Path, TEXT("units"), Units, Errors))
	{
		return;
	}

	for (int32 Index = 0; Index < Units->Num(); ++Index)
	{
		const FString UnitPath = FString::Printf(TEXT("%s.units[%d]"), *Path, Index);
		const TSharedPtr<FJsonObject> UnitObject = (*Units)[Index].IsValid() ? (*Units)[Index]->AsObject() : nullptr;
		if (!UnitObject.IsValid())
		{
			AddSchemaError(Errors, UnitPath, TEXT("expected object"));
			continue;
		}

		ValidateTypedField(UnitObject, UnitPath, TEXT("id"), EJson::Number, Errors);
		ValidateTypedField(UnitObject, UnitPath, TEXT("owner_id"), EJson::Number, Errors);
		ValidateOptionalTypedField(UnitObject, UnitPath, TEXT("card_id"), EJson::String, Errors);
		ValidateTypedField(UnitObject, UnitPath, TEXT("x"), EJson::Number, Errors);
		ValidateTypedField(UnitObject, UnitPath, TEXT("y"), EJson::Number, Errors);
		ValidateOptionalTypedField(UnitObject, UnitPath, TEXT("mp_remaining"), EJson::Number, Errors);
	}
}

bool ValidateLegalActionFamilyFixtureSchema(const TSharedPtr<FJsonObject>& Fixture, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	if (!Fixture.IsValid())
	{
		AddSchemaError(OutErrors, TEXT("fixture"), TEXT("root is not an object"));
		return false;
	}

	ValidateTypedField(Fixture, TEXT("fixture"), TEXT("id"), EJson::String, OutErrors);
	ValidateTypedField(Fixture, TEXT("fixture"), TEXT("schema_version"), EJson::Number, OutErrors);
	ValidateTypedField(Fixture, TEXT("fixture"), TEXT("intent"), EJson::String, OutErrors);

	TSharedPtr<FJsonObject> InitialState;
	if (ValidateObjectField(Fixture, TEXT("fixture"), TEXT("initial_state"), InitialState, OutErrors))
	{
		TSharedPtr<FJsonObject> Board;
		if (ValidateObjectField(InitialState, TEXT("initial_state"), TEXT("board"), Board, OutErrors))
		{
			ValidateTypedField(Board, TEXT("initial_state.board"), TEXT("width"), EJson::Number, OutErrors);
			ValidateTypedField(Board, TEXT("initial_state.board"), TEXT("height"), EJson::Number, OutErrors);
		}

		ValidateTypedField(InitialState, TEXT("initial_state"), TEXT("active_player"), EJson::Number, OutErrors);
		ValidateOptionalTypedField(InitialState, TEXT("initial_state"), TEXT("priority_player"), EJson::Number, OutErrors);
		ValidateOptionalTypedField(InitialState, TEXT("initial_state"), TEXT("turn_number"), EJson::Number, OutErrors);
		ValidateFixtureUnitsAllowOptionalMP(InitialState, TEXT("initial_state"), OutErrors);

		const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
		ValidateArrayField(InitialState, TEXT("initial_state"), TEXT("walls"), Walls, OutErrors);

		TSharedPtr<FJsonObject> StatusesByUnit;
		ValidateObjectField(InitialState, TEXT("initial_state"), TEXT("statuses_by_unit"), StatusesByUnit, OutErrors);
	}

	TSharedPtr<FJsonObject> Expect;
	if (ValidateObjectField(Fixture, TEXT("fixture"), TEXT("expect"), Expect, OutErrors))
	{
		const TArray<TSharedPtr<FJsonValue>>* LegalActionIds = nullptr;
		if (ValidateArrayField(Expect, TEXT("expect"), TEXT("legal_action_ids"), LegalActionIds, OutErrors))
		{
			ValidateStringArrayValues(*LegalActionIds, TEXT("expect.legal_action_ids"), OutErrors);
		}
	}

	return OutErrors.Num() == 0;
}

void ValidateReplayFixtureActionSchema(const TSharedPtr<FJsonObject>& Action, const FString& Path, TArray<FString>& Errors)
{
	ValidateTypedField(Action, Path, TEXT("id"), EJson::String, Errors);
	ValidateTypedField(Action, Path, TEXT("kind"), EJson::String, Errors);
	ValidateTypedField(Action, Path, TEXT("player_id"), EJson::Number, Errors);

	FString Kind;
	if (Action.IsValid() && Action->TryGetStringField(TEXT("kind"), Kind) && Kind == TEXT("move"))
	{
		ValidateTypedField(Action, Path, TEXT("unit_id"), EJson::Number, Errors);
		ValidateTypedField(Action, Path, TEXT("from_x"), EJson::Number, Errors);
		ValidateTypedField(Action, Path, TEXT("from_y"), EJson::Number, Errors);
		ValidateTypedField(Action, Path, TEXT("to_x"), EJson::Number, Errors);
		ValidateTypedField(Action, Path, TEXT("to_y"), EJson::Number, Errors);
	}
}

void ValidateReplayFixtureTraceEvents(const TArray<TSharedPtr<FJsonValue>>& TraceEvents, const FString& Path, TArray<FString>& Errors)
{
	for (int32 Index = 0; Index < TraceEvents.Num(); ++Index)
	{
		const FString TracePath = FString::Printf(TEXT("%s[%d]"), *Path, Index);
		const TSharedPtr<FJsonObject> TraceObject = TraceEvents[Index].IsValid() ? TraceEvents[Index]->AsObject() : nullptr;
		if (!TraceObject.IsValid())
		{
			AddSchemaError(Errors, TracePath, TEXT("expected object"));
			continue;
		}

		ValidateTypedField(TraceObject, TracePath, TEXT("kind"), EJson::String, Errors);
		ValidateTypedField(TraceObject, TracePath, TEXT("ok"), EJson::Boolean, Errors);
		ValidateTypedField(TraceObject, TracePath, TEXT("player_id"), EJson::Number, Errors);
	}
}

void ValidateReplayFixtureDecisionSchema(const TSharedPtr<FJsonObject>& Decision, const FString& Path, TArray<FString>& Errors)
{
	ValidateTypedField(Decision, Path, TEXT("decision_index"), EJson::Number, Errors);
	ValidateTypedField(Decision, Path, TEXT("chosen_action_id"), EJson::String, Errors);

	TSharedPtr<FJsonObject> ChosenAction;
	if (ValidateObjectField(Decision, Path, TEXT("chosen_action"), ChosenAction, Errors))
	{
		ValidateReplayFixtureActionSchema(ChosenAction, FString::Printf(TEXT("%s.chosen_action"), *Path), Errors);
	}

	const TArray<TSharedPtr<FJsonValue>>* LegalActionIds = nullptr;
	if (ValidateArrayField(Decision, Path, TEXT("legal_action_ids"), LegalActionIds, Errors))
	{
		ValidateStringArrayValues(*LegalActionIds, FString::Printf(TEXT("%s.legal_action_ids"), *Path), Errors);
	}

	const TArray<TSharedPtr<FJsonValue>>* TraceEvents = nullptr;
	if (ValidateArrayField(Decision, Path, TEXT("trace_events"), TraceEvents, Errors))
	{
		ValidateReplayFixtureTraceEvents(*TraceEvents, FString::Printf(TEXT("%s.trace_events"), *Path), Errors);
	}
}

void ValidateReplayFixtureUnits(const TSharedPtr<FJsonObject>& StateObject, const FString& Path, TArray<FString>& Errors)
{
	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if (!ValidateArrayField(StateObject, Path, TEXT("units"), Units, Errors))
	{
		return;
	}

	for (int32 Index = 0; Index < Units->Num(); ++Index)
	{
		const FString UnitPath = FString::Printf(TEXT("%s.units[%d]"), *Path, Index);
		const TSharedPtr<FJsonObject> UnitObject = (*Units)[Index].IsValid() ? (*Units)[Index]->AsObject() : nullptr;
		if (!UnitObject.IsValid())
		{
			AddSchemaError(Errors, UnitPath, TEXT("expected object"));
			continue;
		}

		ValidateTypedField(UnitObject, UnitPath, TEXT("id"), EJson::Number, Errors);
		ValidateTypedField(UnitObject, UnitPath, TEXT("owner_id"), EJson::Number, Errors);
		ValidateTypedField(UnitObject, UnitPath, TEXT("x"), EJson::Number, Errors);
		ValidateTypedField(UnitObject, UnitPath, TEXT("y"), EJson::Number, Errors);
		ValidateTypedField(UnitObject, UnitPath, TEXT("mp_remaining"), EJson::Number, Errors);
	}
}

bool ValidateReplayFixtureSchema(const TSharedPtr<FJsonObject>& Fixture, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	if (!Fixture.IsValid())
	{
		AddSchemaError(OutErrors, TEXT("fixture"), TEXT("root is not an object"));
		return false;
	}

	ValidateTypedField(Fixture, TEXT("fixture"), TEXT("id"), EJson::String, OutErrors);
	ValidateTypedField(Fixture, TEXT("fixture"), TEXT("schema_version"), EJson::Number, OutErrors);
	ValidateTypedField(Fixture, TEXT("fixture"), TEXT("intent"), EJson::String, OutErrors);

	TSharedPtr<FJsonObject> InitialState;
	if (ValidateObjectField(Fixture, TEXT("fixture"), TEXT("initial_state"), InitialState, OutErrors))
	{
		const bool bHasCurrentPlayer = ValidateTypedField(InitialState, TEXT("initial_state"), TEXT("current_player"), EJson::Number, OutErrors);
		if (!bHasCurrentPlayer)
		{
			const int32 LastErrorIndex = OutErrors.Num() - 1;
			OutErrors.RemoveAt(LastErrorIndex);
			ValidateTypedField(InitialState, TEXT("initial_state"), TEXT("active_player"), EJson::Number, OutErrors);
		}

		ValidateTypedField(InitialState, TEXT("initial_state"), TEXT("priority_player"), EJson::Number, OutErrors);
		ValidateTypedField(InitialState, TEXT("initial_state"), TEXT("turn_number"), EJson::Number, OutErrors);
		ValidateReplayFixtureUnits(InitialState, TEXT("initial_state"), OutErrors);

		const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
		ValidateArrayField(InitialState, TEXT("initial_state"), TEXT("walls"), Walls, OutErrors);
	}

	TSharedPtr<FJsonObject> ReplayLog;
	if (ValidateObjectField(Fixture, TEXT("fixture"), TEXT("replay_log"), ReplayLog, OutErrors))
	{
		const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
		if (ValidateArrayField(ReplayLog, TEXT("replay_log"), TEXT("decisions"), Decisions, OutErrors))
		{
			if (Decisions->Num() == 0)
			{
				AddSchemaError(OutErrors, TEXT("replay_log.decisions"), TEXT("must contain at least one decision"));
			}

			for (int32 Index = 0; Index < Decisions->Num(); ++Index)
			{
				const FString DecisionPath = FString::Printf(TEXT("replay_log.decisions[%d]"), Index);
				const TSharedPtr<FJsonObject> Decision = (*Decisions)[Index].IsValid() ? (*Decisions)[Index]->AsObject() : nullptr;
				if (!Decision.IsValid())
				{
					AddSchemaError(OutErrors, DecisionPath, TEXT("expected object"));
					continue;
				}

				ValidateReplayFixtureDecisionSchema(Decision, DecisionPath, OutErrors);
			}
		}
	}

	TSharedPtr<FJsonObject> Expect;
	if (ValidateObjectField(Fixture, TEXT("fixture"), TEXT("expect"), Expect, OutErrors))
	{
		TSharedPtr<FJsonObject> FinalState;
		if (ValidateObjectField(Expect, TEXT("expect"), TEXT("final_state"), FinalState, OutErrors))
		{
			const bool bHasCurrentPlayer = ValidateTypedField(FinalState, TEXT("expect.final_state"), TEXT("current_player"), EJson::Number, OutErrors);
			if (!bHasCurrentPlayer)
			{
				const int32 LastErrorIndex = OutErrors.Num() - 1;
				OutErrors.RemoveAt(LastErrorIndex);
				ValidateTypedField(FinalState, TEXT("expect.final_state"), TEXT("active_player"), EJson::Number, OutErrors);
			}

			ValidateTypedField(FinalState, TEXT("expect.final_state"), TEXT("priority_player"), EJson::Number, OutErrors);
			ValidateTypedField(FinalState, TEXT("expect.final_state"), TEXT("turn_number"), EJson::Number, OutErrors);
			ValidateReplayFixtureUnits(FinalState, TEXT("expect.final_state"), OutErrors);

			const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
			ValidateArrayField(FinalState, TEXT("expect.final_state"), TEXT("walls"), Walls, OutErrors);
		}
	}

	return OutErrors.Num() == 0;
}

FString JoinSchemaErrors(const TArray<FString>& Errors)
{
	return FString::Join(Errors, TEXT("; "));
}

bool TryGetExpectedFinalState(const TSharedPtr<FJsonObject>& Fixture, TSharedPtr<FJsonObject>& OutFinalStateObject)
{
	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		|| ExpectObject == nullptr
		|| !ExpectObject->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (!(*ExpectObject)->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		|| FinalStateObject == nullptr
		|| !FinalStateObject->IsValid())
	{
		return false;
	}

	OutFinalStateObject = *FinalStateObject;
	return true;
}

bool ExpectedWallMatchesActual(const TSharedPtr<FJsonObject>& WallObject, const FWBWallEdge& ActualWall)
{
	const TArray<TSharedPtr<FJsonValue>>* Between = nullptr;
	if (!WallObject.IsValid()
		|| !WallObject->TryGetArrayField(TEXT("between"), Between)
		|| Between == nullptr
		|| Between->Num() != 2)
	{
		return false;
	}

	FWBTile A;
	FWBTile B;
	if (!ParseFixtureTile((*Between)[0]->AsObject(), A) || !ParseFixtureTile((*Between)[1]->AsObject(), B))
	{
		return false;
	}

	return WBRules::AreSameWallEdge(FWBWallEdge(A, B), ActualWall);
}

bool FinalStateMatchesFixtureExpectation(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& ActualState,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> FinalStateObject;
	if (!TryGetExpectedFinalState(Fixture, FinalStateObject))
	{
		OutReason = TEXT("missing_expected_final_state");
		return false;
	}

	int32 ExpectedCurrentPlayer = 0;
	if (!TryReadIntegerField(FinalStateObject, TEXT("current_player"), ExpectedCurrentPlayer))
	{
		if (!TryReadIntegerField(FinalStateObject, TEXT("active_player"), ExpectedCurrentPlayer))
		{
			OutReason = TEXT("missing_expected_active_player");
			return false;
		}
	}

	if (ActualState.CurrentPlayer != ExpectedCurrentPlayer)
	{
		OutReason = FString::Printf(TEXT("active_player expected %d got %d"), ExpectedCurrentPlayer, ActualState.CurrentPlayer);
		return false;
	}

	const int32 ExpectedPriorityPlayer = ReadIntegerFieldOrDefault(FinalStateObject, TEXT("priority_player"), ExpectedCurrentPlayer);
	if (ActualState.PriorityPlayer != ExpectedPriorityPlayer)
	{
		OutReason = FString::Printf(TEXT("priority_player expected %d got %d"), ExpectedPriorityPlayer, ActualState.PriorityPlayer);
		return false;
	}

	const int32 ExpectedTurnNumber = ReadIntegerFieldOrDefault(FinalStateObject, TEXT("turn_number"), 1);
	if (ActualState.TurnNumber != ExpectedTurnNumber)
	{
		OutReason = FString::Printf(TEXT("turn_number expected %d got %d"), ExpectedTurnNumber, ActualState.TurnNumber);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedUnits = nullptr;
	if (!FinalStateObject->TryGetArrayField(TEXT("units"), ExpectedUnits) || ExpectedUnits == nullptr)
	{
		OutReason = TEXT("missing_expected_units");
		return false;
	}

	if (ActualState.Units.Num() != ExpectedUnits->Num())
	{
		OutReason = FString::Printf(TEXT("unit count expected %d got %d"), ExpectedUnits->Num(), ActualState.Units.Num());
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ExpectedUnitValue : *ExpectedUnits)
	{
		const TSharedPtr<FJsonObject> ExpectedUnitObject = ExpectedUnitValue.IsValid() ? ExpectedUnitValue->AsObject() : nullptr;
		int32 ExpectedUnitId = -1;
		if (!TryReadIntegerField(ExpectedUnitObject, TEXT("id"), ExpectedUnitId))
		{
			OutReason = TEXT("missing_expected_unit_id");
			return false;
		}

		const FWBUnitState* ActualUnit = ActualState.GetUnitById(ExpectedUnitId);
		if (ActualUnit == nullptr)
		{
			OutReason = FString::Printf(TEXT("missing_actual_unit:%d"), ExpectedUnitId);
			return false;
		}

		int32 ExpectedOwnerId = ActualUnit->OwnerId;
		TryReadIntegerField(ExpectedUnitObject, TEXT("owner_id"), ExpectedOwnerId);
		if (ActualUnit->OwnerId != ExpectedOwnerId)
		{
			OutReason = FString::Printf(TEXT("unit %d owner expected %d got %d"), ExpectedUnitId, ExpectedOwnerId, ActualUnit->OwnerId);
			return false;
		}

		int32 ExpectedX = 0;
		int32 ExpectedY = 0;
		if (!TryReadIntegerField(ExpectedUnitObject, TEXT("x"), ExpectedX)
			|| !TryReadIntegerField(ExpectedUnitObject, TEXT("y"), ExpectedY))
		{
			OutReason = FString::Printf(TEXT("unit %d missing expected tile"), ExpectedUnitId);
			return false;
		}

		if (ActualUnit->X != ExpectedX || ActualUnit->Y != ExpectedY)
		{
			OutReason = FString::Printf(TEXT("unit %d tile expected (%d,%d) got (%d,%d)"), ExpectedUnitId, ExpectedX, ExpectedY, ActualUnit->X, ActualUnit->Y);
			return false;
		}

		int32 ExpectedMPRemaining = ActualUnit->MPRemaining;
		TryReadIntegerField(ExpectedUnitObject, TEXT("mp_remaining"), ExpectedMPRemaining);
		if (ActualUnit->MPRemaining != ExpectedMPRemaining)
		{
			OutReason = FString::Printf(TEXT("unit %d mp_remaining expected %d got %d"), ExpectedUnitId, ExpectedMPRemaining, ActualUnit->MPRemaining);
			return false;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedWalls = nullptr;
	if (!FinalStateObject->TryGetArrayField(TEXT("walls"), ExpectedWalls) || ExpectedWalls == nullptr)
	{
		OutReason = TEXT("missing_expected_walls");
		return false;
	}

	if (ActualState.Walls.Num() != ExpectedWalls->Num())
	{
		OutReason = FString::Printf(TEXT("wall count expected %d got %d"), ExpectedWalls->Num(), ActualState.Walls.Num());
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ExpectedWallValue : *ExpectedWalls)
	{
		const TSharedPtr<FJsonObject> ExpectedWallObject = ExpectedWallValue.IsValid() ? ExpectedWallValue->AsObject() : nullptr;
		bool bFoundWall = false;
		for (const FWBWallEdge& ActualWall : ActualState.Walls)
		{
			if (ExpectedWallMatchesActual(ExpectedWallObject, ActualWall))
			{
				bFoundWall = true;
				break;
			}
		}

		if (!bFoundWall)
		{
			OutReason = TEXT("expected_wall_missing");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardBoundsTest, "Wandbound.Core.Movement.BoardBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardBoundsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("(0,0) is valid"), WBRules::IsTileInBounds(FWBTile(0, 0)));
	TestTrue(TEXT("(8,8) is valid"), WBRules::IsTileInBounds(FWBTile(8, 8)));
	TestFalse(TEXT("(-1,0) is invalid"), WBRules::IsTileInBounds(FWBTile(-1, 0)));
	TestFalse(TEXT("(9,0) is invalid"), WBRules::IsTileInBounds(FWBTile(9, 0)));
	TestFalse(TEXT("(0,9) is invalid"), WBRules::IsTileInBounds(FWBTile(0, 9)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBValidAdjacentMoveSucceedsTest, "Wandbound.Core.Movement.ValidAdjacentMoveSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBValidAdjacentMoveSucceedsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 2)));

	const FWBAction Action = MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(4, 3));
	const FWBMoveQueryResult Query = WBRules::QueryMove(State, Action);
	TestTrue(TEXT("Godot golden movement_valid_adjacent is legal"), Query.bOk);
	TestEqual(TEXT("Adjacent move costs 1 MP"), Query.CostMP, 1);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, Action);

	TestTrue(TEXT("Move succeeds"), Result.bOk);
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), Unit != nullptr);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), Unit->X, 4);
		TestEqual(TEXT("Unit Y updated"), Unit->Y, 3);
		TestEqual(TEXT("Legacy unit MP mirror decremented"), Unit->MPRemaining, 1);
	}
	const FWBPlayerStateData* Player = State.GetPlayerById(0);
	TestTrue(TEXT("Player state exists"), Player != nullptr);
	if (Player != nullptr)
	{
		TestEqual(TEXT("Player MP decremented"), Player->RemainingMP, 1);
	}
	TestEqual(TEXT("One trace event emitted"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace event is move"), Result.TraceEvents[0].Kind, FName(TEXT("move")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBOutOfBoundsMoveFailsTest, "Wandbound.Core.Movement.OutOfBoundsMoveFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBOutOfBoundsMoveFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(8, 4), 2)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(8, 4), FWBTile(9, 4)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot reason"), Result.Reason, FString(TEXT("out_of_bounds")));
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), Unit != nullptr);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), Unit->X, 8);
		TestEqual(TEXT("Unit Y unchanged"), Unit->Y, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDiagonalMoveFailsTest, "Wandbound.Core.Movement.DiagonalMoveFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDiagonalMoveFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 2)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(5, 5)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot reason"), Result.Reason, FString(TEXT("illegal_move_distance")));
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), Unit != nullptr);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), Unit->X, 4);
		TestEqual(TEXT("Unit Y unchanged"), Unit->Y, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBOccupiedDestinationFailsTest, "Wandbound.Core.Movement.OccupiedDestinationFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBOccupiedDestinationFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit A added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 2)));
	TestTrue(TEXT("Unit B added"), State.AddUnitForTest(MakeUnit(2, 1, FWBTile(5, 4), 2)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(5, 4)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot golden movement_blocked_by_unit reason"), Result.Reason, FString(TEXT("tile_occupied")));
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), Unit != nullptr);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), Unit->X, 4);
		TestEqual(TEXT("Unit Y unchanged"), Unit->Y, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBWallBlockedMovementFailsTest, "Wandbound.Core.Movement.WallBlockedMovementFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBWallBlockedMovementFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 2)));
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 3)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(4, 3)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot golden movement_blocked_by_wall reason"), Result.Reason, FString(TEXT("blocked_by_wall")));
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), Unit != nullptr);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), Unit->X, 4);
		TestEqual(TEXT("Unit Y unchanged"), Unit->Y, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBMissingUnitFailsTest, "Wandbound.Core.Movement.MissingUnitFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMissingUnitFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 999, FWBTile(4, 4), FWBTile(4, 5)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot reason"), Result.Reason, FString(TEXT("no_unit")));
	TestEqual(TEXT("No units were created"), State.Units.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRootedUnitCannotMoveTest, "Wandbound.Core.Movement.RootedUnitCannotMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRootedUnitCannotMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	FWBUnitState Unit = MakeUnit(1, 0, FWBTile(4, 4), 2);
	Unit.Statuses.Add(FName(TEXT("root")));
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(4, 3)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot golden movement_rooted_fails reason"), Result.Reason, FString(TEXT("cannot_move")));
	const FWBUnitState* StoredUnit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), StoredUnit != nullptr);
	if (StoredUnit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), StoredUnit->X, 4);
		TestEqual(TEXT("Unit Y unchanged"), StoredUnit->Y, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStunnedUnitCannotMoveTest, "Wandbound.Core.Movement.StunnedUnitCannotMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStunnedUnitCannotMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	FWBUnitState Unit = MakeUnit(1, 0, FWBTile(4, 4), 2);
	Unit.Statuses.Add(FName(TEXT("stun")));
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(Unit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(4, 3)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot golden movement_stunned_fails reason"), Result.Reason, FString(TEXT("cannot_move")));
	const FWBUnitState* StoredUnit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), StoredUnit != nullptr);
	if (StoredUnit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), StoredUnit->X, 4);
		TestEqual(TEXT("Unit Y unchanged"), StoredUnit->Y, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBNoMPUnitCannotMoveTest, "Wandbound.Core.Movement.NoMPUnitCannotMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNoMPUnitCannotMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 0)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(4, 5)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot-style reason"), Result.Reason, FString(TEXT("insufficient_mp")));
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), Unit != nullptr);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), Unit->X, 4);
		TestEqual(TEXT("Unit Y unchanged"), Unit->Y, 4);
		TestEqual(TEXT("Legacy unit MP mirror unchanged"), Unit->MPRemaining, 0);
	}
	const FWBPlayerStateData* Player = State.GetPlayerById(0);
	TestTrue(TEXT("Player state exists"), Player != nullptr);
	if (Player != nullptr)
	{
		TestEqual(TEXT("Player MP unchanged"), Player->RemainingMP, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBFailedMoveDoesNotMutateTest, "Wandbound.Core.Movement.FailedMoveDoesNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBFailedMoveDoesNotMutateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 3)));
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 5)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 1, FWBTile(4, 4), FWBTile(4, 5)));

	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Godot reason"), Result.Reason, FString(TEXT("blocked_by_wall")));
	TestEqual(TEXT("No success trace events"), Result.TraceEvents.Num(), 0);
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestTrue(TEXT("Unit still exists"), Unit != nullptr);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Unit X unchanged"), Unit->X, 4);
		TestEqual(TEXT("Unit Y unchanged"), Unit->Y, 4);
		TestEqual(TEXT("Legacy unit MP mirror unchanged"), Unit->MPRemaining, 3);
	}
	const FWBPlayerStateData* Player = State.GetPlayerById(0);
	TestTrue(TEXT("Player state exists"), Player != nullptr);
	if (Player != nullptr)
	{
		TestEqual(TEXT("Player MP unchanged"), Player->RemainingMP, 3);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSuccessfulMoveTraceIncludesPayloadTest, "Wandbound.Core.Movement.SuccessfulMoveTraceIncludesPayload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSuccessfulMoveTraceIncludesPayloadTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyMove(State, MakeMoveAction(0, 42, FWBTile(2, 3), FWBTile(3, 3)));

	TestTrue(TEXT("Move succeeds"), Result.bOk);
	TestEqual(TEXT("One trace event emitted"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Event = Result.TraceEvents[0];
		TestEqual(TEXT("Trace player id"), Event.PlayerId, 0);
		TestEqual(TEXT("Trace source unit id"), Event.SourceUnitId, 42);
		TestTrue(TEXT("Trace from tile"), Event.FromTile == FWBTile(2, 3));
		TestTrue(TEXT("Trace to tile"), Event.ToTile == FWBTile(3, 3));
		TestTrue(TEXT("Trace bOk true"), Event.bOk);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBLegalMoveGenerationEnumeratesAdjacentGodotScenarioTilesTest, "Wandbound.Core.Movement.LegalMoveGenerationEnumeratesAdjacentGodotScenarioTiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBLegalMoveGenerationEnumeratesAdjacentGodotScenarioTilesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Mover added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 2)));
	TestTrue(TEXT("Occupied destination blocker added"), State.AddUnitForTest(MakeUnit(2, 1, FWBTile(5, 4), 2)));
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 3)));

	const TArray<FWBAction> LegalMoves = WBRules::GenerateLegalMoveActions(State, 0, 1);

	TestEqual(TEXT("Only west and south remain legal after Godot blocker scenarios"), LegalMoves.Num(), 2);
	TestTrue(TEXT("West adjacent tile is legal"), ContainsMoveTo(LegalMoves, FWBTile(3, 4)));
	TestTrue(TEXT("South adjacent tile is legal"), ContainsMoveTo(LegalMoves, FWBTile(4, 5)));
	TestFalse(TEXT("East occupied tile is not generated"), ContainsMoveTo(LegalMoves, FWBTile(5, 4)));
	TestFalse(TEXT("North wall-blocked tile is not generated"), ContainsMoveTo(LegalMoves, FWBTile(4, 3)));

	if (LegalMoves.Num() == 2)
	{
		TestTrue(TEXT("Generation preserves Godot direction order after filtering: west first"), LegalMoves[0].ToTile == FWBTile(3, 4));
		TestTrue(TEXT("Generation preserves Godot direction order after filtering: south second"), LegalMoves[1].ToTile == FWBTile(4, 5));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCannotMoveStatusesGenerateNoLegalMovesTest, "Wandbound.Core.Movement.CannotMoveStatusesGenerateNoLegalMoves", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCannotMoveStatusesGenerateNoLegalMovesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData RootedState;
	FWBUnitState RootedUnit = MakeUnit(1, 0, FWBTile(4, 4), 2);
	RootedUnit.Statuses.Add(FName(TEXT("root")));
	TestTrue(TEXT("Rooted unit added"), RootedState.AddUnitForTest(RootedUnit));
	TestEqual(TEXT("Rooted unit generates no moves"), WBRules::GenerateLegalMoveActions(RootedState, 0, 1).Num(), 0);

	FWBGameStateData StunnedState;
	FWBUnitState StunnedUnit = MakeUnit(1, 0, FWBTile(4, 4), 2);
	StunnedUnit.Statuses.Add(FName(TEXT("stun")));
	TestTrue(TEXT("Stunned unit added"), StunnedState.AddUnitForTest(StunnedUnit));
	TestEqual(TEXT("Stunned unit generates no moves"), WBRules::GenerateLegalMoveActions(StunnedState, 0, 1).Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBMovementFixtureScenariosMatchGodotCanonTest, "Wandbound.Core.Movement.FixtureScenariosMatchGodotCanon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMovementFixtureScenariosMatchGodotCanonTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetMovementFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, State);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		FString ActionJson;
		TestTrue(*FString::Printf(TEXT("%s has action"), *FixtureName), ExtractFixtureActionJson(Fixture, ActionJson));
		if (ActionJson.IsEmpty())
		{
			return false;
		}

		const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(ActionJson, State);
		TestTrue(*FString::Printf(TEXT("%s action decodes: %s"), *FixtureName, *DecodeResult.Reason), DecodeResult.bOk);
		if (!DecodeResult.bOk)
		{
			return false;
		}
		TestTrue(*FString::Printf(TEXT("%s action is move"), *FixtureName), DecodeResult.Action.Type == EWBActionType::Move);
		if (DecodeResult.Action.Type != EWBActionType::Move)
		{
			return false;
		}

		bool bExpectedLegal = false;
		FString ExpectedReason;
		FString ExpectationReadReason;
		const bool bExpectationsRead = ReadMovementFixtureExpectations(Fixture, bExpectedLegal, ExpectedReason, ExpectationReadReason);
		TestTrue(*FString::Printf(TEXT("%s movement expectations read: %s"), *FixtureName, *ExpectationReadReason), bExpectationsRead);
		if (!bExpectationsRead)
		{
			return false;
		}

		TArray<FString> ExpectedLegalMoveActionIds;
		FString LegalActionIdsReadReason;
		const bool bLegalActionIdsRead = ReadExpectedLegalMoveActionIds(Fixture, ExpectedLegalMoveActionIds, LegalActionIdsReadReason);
		TestTrue(*FString::Printf(TEXT("%s expected legal move action ids read: %s"), *FixtureName, *LegalActionIdsReadReason), bLegalActionIdsRead);
		if (!bLegalActionIdsRead)
		{
			return false;
		}

		const TArray<FWBAction> GeneratedLegalMoveActions = WBRules::GenerateLegalMoveActions(State, DecodeResult.Action.PlayerId, DecodeResult.Action.SourceUnitId);
		const TArray<FString> GeneratedLegalMoveActionIds = WBActionCodec::MakeActionIds(GeneratedLegalMoveActions);
		TestTrue(
			*FString::Printf(
				TEXT("%s generated legal move ids match fixture. expected [%s] got [%s]"),
				*FixtureName,
				*JoinStringsForTest(ExpectedLegalMoveActionIds),
				*JoinStringsForTest(GeneratedLegalMoveActionIds)),
			StringArraysEqual(GeneratedLegalMoveActionIds, ExpectedLegalMoveActionIds));

		const FWBMoveQueryResult Query = WBRules::QueryMove(State, DecodeResult.Action);
		TestEqual(*FString::Printf(TEXT("%s legality matches fixture"), *FixtureName), Query.bOk, bExpectedLegal);
		TestEqual(*FString::Printf(TEXT("%s reason matches fixture"), *FixtureName), Query.Reason, ExpectedReason);

		const FWBApplyActionResult ApplyResult = WBEffectRunner::ApplyMove(State, DecodeResult.Action);
		TestEqual(*FString::Printf(TEXT("%s apply result matches fixture legality"), *FixtureName), ApplyResult.bOk, bExpectedLegal);
		TestEqual(*FString::Printf(TEXT("%s apply reason matches fixture"), *FixtureName), ApplyResult.Reason, ExpectedReason);

		FString FinalUnitsReason;
		const bool bFinalUnitsMatch = MovementFixtureFinalUnitsMatch(Fixture, State, FinalUnitsReason);
		TestTrue(*FString::Printf(TEXT("%s final units match: %s"), *FixtureName, *FinalUnitsReason), bFinalUnitsMatch);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBFixtureDrivenMovementLegalActionGenerationTest, "Wandbound.Core.ActionGeneration.FixtureDrivenMovementLegalActionIds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBFixtureDrivenMovementLegalActionGenerationTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetMovementFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, State);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		TArray<FString> ExpectedLegalMoveActionIds;
		FString LegalMoveActionIdsReadReason;
		const bool bLegalMoveActionIdsRead = ReadExpectedLegalMoveActionIds(Fixture, ExpectedLegalMoveActionIds, LegalMoveActionIdsReadReason);
		TestTrue(*FString::Printf(TEXT("%s expected legal move action ids read: %s"), *FixtureName, *LegalMoveActionIdsReadReason), bLegalMoveActionIdsRead);
		if (!bLegalMoveActionIdsRead)
		{
			return false;
		}

		TArray<FString> GeneratedMoveActionIds;
		for (const FWBAction& Action : WBRules::GenerateLegalActions(State))
		{
			if (Action.Type == EWBActionType::Move)
			{
				GeneratedMoveActionIds.Add(WBActionCodec::MakeActionId(Action));
			}
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s generated move action ids from legal action generation match fixture. expected [%s] got [%s]"),
				*FixtureName,
				*JoinStringsForTest(ExpectedLegalMoveActionIds),
				*JoinStringsForTest(GeneratedMoveActionIds)),
			StringArraysEqual(GeneratedMoveActionIds, ExpectedLegalMoveActionIds));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBFixtureDrivenLegalActionFamilyGenerationTest, "Wandbound.Core.ActionGeneration.FixtureDrivenLegalActionFamilies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBFixtureDrivenLegalActionFamilyGenerationTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetLegalActionFamilyFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, State);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		TArray<FString> ExpectedLegalActionIds;
		FString LegalActionIdsReadReason;
		const bool bLegalActionIdsRead = ReadExpectedLegalActionIds(Fixture, ExpectedLegalActionIds, LegalActionIdsReadReason);
		TestTrue(*FString::Printf(TEXT("%s expected legal action ids read: %s"), *FixtureName, *LegalActionIdsReadReason), bLegalActionIdsRead);
		if (!bLegalActionIdsRead)
		{
			return false;
		}

		const TArray<FWBAction> GeneratedLegalActions = WBRules::GenerateLegalActions(State);
		const TArray<FString> GeneratedLegalActionIds = WBActionCodec::MakeActionIds(GeneratedLegalActions);

		TestTrue(
			*FString::Printf(
				TEXT("%s generated legal action family ids match fixture. expected [%s] got [%s]"),
				*FixtureName,
				*JoinStringsForTest(ExpectedLegalActionIds),
				*JoinStringsForTest(GeneratedLegalActionIds)),
			StringArraysEqual(GeneratedLegalActionIds, ExpectedLegalActionIds));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBActionGenerationFixtureSchemaAcceptsGodotCanonTest, "Wandbound.Core.ActionGenerationFixture.SchemaAcceptsGodotCanon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBActionGenerationFixtureSchemaAcceptsGodotCanonTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetLegalActionFamilyFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		TArray<FString> SchemaErrors;
		const bool bSchemaValid = ValidateLegalActionFamilyFixtureSchema(Fixture, SchemaErrors);
		TestTrue(*FString::Printf(TEXT("%s legal-action fixture schema valid: %s"), *FixtureName, *JoinSchemaErrors(SchemaErrors)), bSchemaValid);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBActionGenerationFixtureSchemaRejectsBadLegalActionIdsTest, "Wandbound.Core.ActionGenerationFixture.SchemaRejectsBadLegalActionIds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBActionGenerationFixtureSchemaRejectsBadLegalActionIdsTest::RunTest(const FString& Parameters)
{
	const FString MissingJson = TEXT("{\"id\":\"missing_legal_action_ids\",\"schema_version\":1,\"intent\":\"bad fixture\",\"initial_state\":{\"board\":{\"width\":9,\"height\":9},\"active_player\":0,\"units\":[],\"walls\":[],\"statuses_by_unit\":{}},\"expect\":{}}");
	const TSharedPtr<FJsonObject> MissingFixture = ParseJsonObject(MissingJson);
	TestTrue(TEXT("Missing legal action ids fixture parses"), MissingFixture.IsValid());
	if (!MissingFixture.IsValid())
	{
		return false;
	}

	TArray<FString> MissingErrors;
	const bool bMissingSchemaValid = ValidateLegalActionFamilyFixtureSchema(MissingFixture, MissingErrors);
	const FString JoinedMissingErrors = JoinSchemaErrors(MissingErrors);
	TestFalse(TEXT("Missing legal action ids fixture is rejected"), bMissingSchemaValid);
	TestTrue(TEXT("Schema error names missing expect legal action ids"), JoinedMissingErrors.Contains(TEXT("expect.legal_action_ids: missing required field")));

	const FString MalformedJson = TEXT("{\"id\":\"malformed_legal_action_ids\",\"schema_version\":1,\"intent\":\"bad fixture\",\"initial_state\":{\"board\":{\"width\":9,\"height\":9},\"active_player\":0,\"units\":[],\"walls\":[],\"statuses_by_unit\":{}},\"expect\":{\"legal_action_ids\":[\"end_turn:p0\",1]}}");
	const TSharedPtr<FJsonObject> MalformedFixture = ParseJsonObject(MalformedJson);
	TestTrue(TEXT("Malformed legal action ids fixture parses"), MalformedFixture.IsValid());
	if (!MalformedFixture.IsValid())
	{
		return false;
	}

	TArray<FString> MalformedErrors;
	const bool bMalformedSchemaValid = ValidateLegalActionFamilyFixtureSchema(MalformedFixture, MalformedErrors);
	const FString JoinedMalformedErrors = JoinSchemaErrors(MalformedErrors);
	TestFalse(TEXT("Malformed legal action ids fixture is rejected"), bMalformedSchemaValid);
	TestTrue(TEXT("Schema error names malformed expect legal action id"), JoinedMalformedErrors.Contains(TEXT("expect.legal_action_ids[1]: expected string, got number")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBActivePlayerActionGenerationUsesMovementFirstTest, "Wandbound.Core.Movement.ActivePlayerActionGenerationUsesMovementFirst", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBActivePlayerActionGenerationUsesMovementFirstTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;

	TestTrue(TEXT("First active-player unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 2)));
	TestTrue(TEXT("Enemy blocker added"), State.AddUnitForTest(MakeUnit(2, 1, FWBTile(5, 4), 2)));
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 3)));

	TestTrue(TEXT("Second active-player unit added"), State.AddUnitForTest(MakeUnit(3, 0, FWBTile(0, 0), 2)));

	FWBUnitState RootedUnit = MakeUnit(4, 0, FWBTile(8, 8), 2);
	RootedUnit.Statuses.Add(FName(TEXT("root")));
	TestTrue(TEXT("Rooted active-player unit added"), State.AddUnitForTest(RootedUnit));

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActions(State);

	TestEqual(TEXT("Active-player legal actions include movement and end turn"), Actions.Num(), 5);
	for (const FWBAction& Action : Actions)
	{
		TestEqual(TEXT("Generated action belongs to active player"), Action.PlayerId, 0);
	}

	if (Actions.Num() == 5)
	{
		TestTrue(TEXT("Movement is the first generated action family 0"), Actions[0].Type == EWBActionType::Move);
		TestTrue(TEXT("Movement is the first generated action family 1"), Actions[1].Type == EWBActionType::Move);
		TestTrue(TEXT("Movement is the first generated action family 2"), Actions[2].Type == EWBActionType::Move);
		TestTrue(TEXT("Movement is the first generated action family 3"), Actions[3].Type == EWBActionType::Move);
		TestTrue(TEXT("End turn is generated after movement"), Actions[4].Type == EWBActionType::EndTurn);
	}

	TestTrue(TEXT("First unit west move generated"), ContainsMoveFromUnitTo(Actions, 1, FWBTile(3, 4)));
	TestTrue(TEXT("First unit south move generated"), ContainsMoveFromUnitTo(Actions, 1, FWBTile(4, 5)));
	TestFalse(TEXT("First unit occupied east move excluded"), ContainsMoveFromUnitTo(Actions, 1, FWBTile(5, 4)));
	TestFalse(TEXT("First unit wall-blocked north move excluded"), ContainsMoveFromUnitTo(Actions, 1, FWBTile(4, 3)));

	TestTrue(TEXT("Second unit east move generated"), ContainsMoveFromUnitTo(Actions, 3, FWBTile(1, 0)));
	TestTrue(TEXT("Second unit south move generated"), ContainsMoveFromUnitTo(Actions, 3, FWBTile(0, 1)));
	TestFalse(TEXT("Enemy unit moves are not generated"), ContainsMoveFromUnitTo(Actions, 2, FWBTile(7, 8)));
	TestFalse(TEXT("Rooted active-player unit moves are not generated"), ContainsMoveFromUnitTo(Actions, 4, FWBTile(7, 8)));

	if (Actions.Num() == 5)
	{
		TestEqual(TEXT("State-order first unit action 0"), Actions[0].SourceUnitId, 1);
		TestEqual(TEXT("State-order first unit action 1"), Actions[1].SourceUnitId, 1);
		TestEqual(TEXT("State-order second unit action 2"), Actions[2].SourceUnitId, 3);
		TestEqual(TEXT("State-order second unit action 3"), Actions[3].SourceUnitId, 3);
		TestEqual(TEXT("End turn has no source unit"), Actions[4].SourceUnitId, -1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPriorityPassGenerationAfterMovementFamilyTest, "Wandbound.Core.Movement.PriorityPassGenerationAfterMovementFamily", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPriorityPassGenerationAfterMovementFamilyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	TestTrue(TEXT("Active player unit added"), State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4), 2)));
	TestTrue(TEXT("Priority player unit added"), State.AddUnitForTest(MakeUnit(2, 1, FWBTile(1, 1), 2)));

	const TArray<FWBAction> PriorityActions = WBRules::GenerateLegalActions(State);

	TestEqual(TEXT("Priority player gets only pass in response-like priority"), PriorityActions.Num(), 1);
	if (PriorityActions.Num() == 1)
	{
		TestTrue(TEXT("Generated action is pass"), PriorityActions[0].Type == EWBActionType::Pass);
		TestEqual(TEXT("Pass belongs to priority player"), PriorityActions[0].PlayerId, 1);
		TestEqual(TEXT("Pass has no source unit"), PriorityActions[0].SourceUnitId, -1);
	}

	TestEqual(TEXT("Active player cannot act without priority"), WBRules::GenerateLegalActionsForPlayer(State, 0).Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnApplyFlipsActivePlayerAndTracesTest, "Wandbound.Core.TurnControl.EndTurnApplyFlipsActivePlayerAndTraces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnApplyFlipsActivePlayerAndTracesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 7;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyAction(State, MakeTurnControlAction(EWBActionType::EndTurn, 0));

	TestTrue(TEXT("End turn succeeds"), Result.bOk);
	TestEqual(TEXT("Current player flips"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Priority follows new current player"), State.PriorityPlayer, 1);
	TestEqual(TEXT("Turn number advances"), State.TurnNumber, 8);
	TestEqual(TEXT("One end_turn trace"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Event = Result.TraceEvents[0];
		TestEqual(TEXT("Trace kind"), Event.Kind, FName(TEXT("end_turn")));
		TestEqual(TEXT("Trace actor"), Event.PlayerId, 0);
		TestEqual(TEXT("Trace from player"), Event.FromPlayer, 0);
		TestEqual(TEXT("Trace to player"), Event.ToPlayer, 1);
		TestEqual(TEXT("Trace turn number"), Event.TurnNumber, 8);
		TestTrue(TEXT("Trace ok"), Event.bOk);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEndTurnWithoutPriorityFailsTest, "Wandbound.Core.TurnControl.EndTurnWithoutPriorityFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEndTurnWithoutPriorityFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.TurnNumber = 4;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndTurn(State, MakeTurnControlAction(EWBActionType::EndTurn, 0));

	TestFalse(TEXT("End turn fails without priority"), Result.bOk);
	TestEqual(TEXT("Godot reason"), Result.Reason, FString(TEXT("no_priority")));
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, 0);
	TestEqual(TEXT("Priority unchanged"), State.PriorityPlayer, 1);
	TestEqual(TEXT("Turn number unchanged"), State.TurnNumber, 4);
	TestEqual(TEXT("No trace on failed end turn"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPassApplyReturnsPriorityToActivePlayerAndTracesTest, "Wandbound.Core.TurnControl.PassApplyReturnsPriorityToActivePlayerAndTraces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPassApplyReturnsPriorityToActivePlayerAndTracesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.TurnNumber = 3;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyAction(State, MakeTurnControlAction(EWBActionType::Pass, 1));

	TestTrue(TEXT("Pass succeeds"), Result.bOk);
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, 0);
	TestEqual(TEXT("Priority returns to active player"), State.PriorityPlayer, 0);
	TestEqual(TEXT("Turn number unchanged"), State.TurnNumber, 3);
	TestEqual(TEXT("One pass trace"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Event = Result.TraceEvents[0];
		TestEqual(TEXT("Trace kind"), Event.Kind, FName(TEXT("pass")));
		TestEqual(TEXT("Trace actor"), Event.PlayerId, 1);
		TestEqual(TEXT("Trace from priority player"), Event.FromPlayer, 1);
		TestEqual(TEXT("Trace to priority player"), Event.ToPlayer, 0);
		TestEqual(TEXT("Trace turn number"), Event.TurnNumber, 3);
		TestTrue(TEXT("Trace ok"), Event.bOk);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPassLegalityFailuresDoNotMutateTest, "Wandbound.Core.TurnControl.PassLegalityFailuresDoNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPassLegalityFailuresDoNotMutateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData NonPriorityState;
	NonPriorityState.CurrentPlayer = 0;
	NonPriorityState.PriorityPlayer = 1;
	NonPriorityState.TurnNumber = 5;

	const FWBApplyActionResult NonPriorityResult = WBEffectRunner::ApplyPass(NonPriorityState, MakeTurnControlAction(EWBActionType::Pass, 0));

	TestFalse(TEXT("Non-priority pass fails"), NonPriorityResult.bOk);
	TestEqual(TEXT("Non-priority reason"), NonPriorityResult.Reason, FString(TEXT("not_priority_player")));
	TestEqual(TEXT("Current player unchanged"), NonPriorityState.CurrentPlayer, 0);
	TestEqual(TEXT("Priority unchanged"), NonPriorityState.PriorityPlayer, 1);
	TestEqual(TEXT("Turn number unchanged"), NonPriorityState.TurnNumber, 5);
	TestEqual(TEXT("No trace on failed pass"), NonPriorityResult.TraceEvents.Num(), 0);

	FWBGameStateData MainFlowState;
	MainFlowState.CurrentPlayer = 0;
	MainFlowState.PriorityPlayer = 0;
	MainFlowState.TurnNumber = 5;

	const FWBApplyActionResult MainFlowResult = WBEffectRunner::ApplyPass(MainFlowState, MakeTurnControlAction(EWBActionType::Pass, 0));

	TestFalse(TEXT("Main-flow pass fails"), MainFlowResult.bOk);
	TestEqual(TEXT("Main-flow pass reason"), MainFlowResult.Reason, FString(TEXT("pass_only_valid_in_response")));
	TestEqual(TEXT("Current player unchanged"), MainFlowState.CurrentPlayer, 0);
	TestEqual(TEXT("Priority unchanged"), MainFlowState.PriorityPlayer, 0);
	TestEqual(TEXT("Turn number unchanged"), MainFlowState.TurnNumber, 5);
	TestEqual(TEXT("No trace on failed main-flow pass"), MainFlowResult.TraceEvents.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBActionCodecSerializesAndDecodesMoveTest, "Wandbound.Core.ActionCodec.SerializesAndDecodesMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBActionCodecSerializesAndDecodesMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	const FWBAction Action = MakeMoveAction(0, 42, FWBTile(2, 3), FWBTile(3, 3));
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(Action), FString(TEXT("move:p0:u42:2,3->3,3")));

	const TSharedPtr<FJsonObject> Object = ParseJsonObject(WBActionCodec::SerializeAction(Action));
	TestTrue(TEXT("Serialized action parses"), Object.IsValid());
	if (!Object.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("Action id serialized"), Object->GetStringField(TEXT("id")), FString(TEXT("move:p0:u42:2,3->3,3")));
	TestEqual(TEXT("Action kind serialized"), Object->GetStringField(TEXT("kind")), FString(TEXT("move")));
	TestEqual(TEXT("Action player serialized"), Object->GetIntegerField(TEXT("player_id")), 0);
	TestEqual(TEXT("Action unit serialized"), Object->GetIntegerField(TEXT("unit_id")), 42);
	TestEqual(TEXT("Action from x serialized"), Object->GetIntegerField(TEXT("from_x")), 2);
	TestEqual(TEXT("Action from y serialized"), Object->GetIntegerField(TEXT("from_y")), 3);
	TestEqual(TEXT("Action to x serialized"), Object->GetIntegerField(TEXT("to_x")), 3);
	TestEqual(TEXT("Action to y serialized"), Object->GetIntegerField(TEXT("to_y")), 3);

	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(WBActionCodec::SerializeAction(Action), State);
	TestTrue(TEXT("Action decodes"), DecodeResult.bOk);
	TestEqual(TEXT("Decoded action type"), static_cast<int32>(DecodeResult.Action.Type), static_cast<int32>(EWBActionType::Move));
	TestEqual(TEXT("Decoded player"), DecodeResult.Action.PlayerId, 0);
	TestEqual(TEXT("Decoded unit"), DecodeResult.Action.SourceUnitId, 42);
	TestTrue(TEXT("Decoded from tile"), DecodeResult.Action.FromTile == FWBTile(2, 3));
	TestTrue(TEXT("Decoded to tile"), DecodeResult.Action.ToTile == FWBTile(3, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBActionCodecInfersMoveSourceTileTest, "Wandbound.Core.ActionCodec.InfersMoveSourceTile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBActionCodecInfersMoveSourceTileTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	const FString Json = TEXT("{\"kind\":\"move\",\"player_id\":0,\"unit_id\":42,\"to_x\":3,\"to_y\":3}");
	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(Json, State);

	TestTrue(TEXT("Action decodes"), DecodeResult.bOk);
	TestTrue(TEXT("Source tile inferred from state"), DecodeResult.Action.FromTile == FWBTile(2, 3));
	TestEqual(TEXT("Canonical decoded id"), WBActionCodec::MakeActionId(DecodeResult.Action), FString(TEXT("move:p0:u42:2,3->3,3")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBActionCodecSerializesAndDecodesTurnControlTest, "Wandbound.Core.ActionCodec.SerializesAndDecodesTurnControl", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBActionCodecSerializesAndDecodesTurnControlTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;

	const FWBAction EndTurnAction = MakeTurnControlAction(EWBActionType::EndTurn, 0);
	TestEqual(TEXT("End turn action id"), WBActionCodec::MakeActionId(EndTurnAction), FString(TEXT("end_turn:p0")));
	FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(WBActionCodec::SerializeAction(EndTurnAction), State);
	TestTrue(TEXT("End turn decodes"), DecodeResult.bOk);
	TestEqual(TEXT("End turn type"), static_cast<int32>(DecodeResult.Action.Type), static_cast<int32>(EWBActionType::EndTurn));
	TestEqual(TEXT("End turn player"), DecodeResult.Action.PlayerId, 0);

	const FWBAction PassAction = MakeTurnControlAction(EWBActionType::Pass, 1);
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(PassAction), FString(TEXT("pass:p1")));
	DecodeResult = WBActionCodec::DecodeAction(WBActionCodec::SerializeAction(PassAction), State);
	TestTrue(TEXT("Pass decodes"), DecodeResult.bOk);
	TestEqual(TEXT("Pass type"), static_cast<int32>(DecodeResult.Action.Type), static_cast<int32>(EWBActionType::Pass));
	TestEqual(TEXT("Pass player"), DecodeResult.Action.PlayerId, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBActionCodecRejectsActionIdMismatchTest, "Wandbound.Core.ActionCodec.RejectsActionIdMismatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBActionCodecRejectsActionIdMismatchTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	const FString Json = TEXT("{\"id\":\"move:p0:u42:2,3->4,3\",\"kind\":\"move\",\"player_id\":0,\"unit_id\":42,\"from_x\":2,\"from_y\":3,\"to_x\":3,\"to_y\":3}");
	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(Json, State);

	TestFalse(TEXT("Stale action id rejected"), DecodeResult.bOk);
	TestEqual(TEXT("Mismatch reason"), DecodeResult.Reason, FString(TEXT("action_id_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayTraceSerializesMoveEventTest, "Wandbound.Core.ReplayTrace.SerializesMoveEvent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayTraceSerializesMoveEventTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyAction(State, MakeMoveAction(0, 42, FWBTile(2, 3), FWBTile(3, 3)));
	TestTrue(TEXT("Move succeeds"), Result.bOk);
	TestEqual(TEXT("Move emits one trace"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Object = ParseJsonObject(WBReplayTrace::SerializeEvent(Result.TraceEvents[0]));
	TestTrue(TEXT("Serialized move parses"), Object.IsValid());
	if (!Object.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("Move kind"), Object->GetStringField(TEXT("kind")), FString(TEXT("move")));
	TestTrue(TEXT("Move ok"), Object->GetBoolField(TEXT("ok")));
	TestEqual(TEXT("Move player"), Object->GetIntegerField(TEXT("player_id")), 0);
	TestEqual(TEXT("Move source unit"), Object->GetIntegerField(TEXT("source_unit_id")), 42);

	const TSharedPtr<FJsonObject>* FromTile = nullptr;
	TestTrue(TEXT("Move has from tile"), Object->TryGetObjectField(TEXT("from_tile"), FromTile));
	const TSharedPtr<FJsonObject>* ToTile = nullptr;
	TestTrue(TEXT("Move has to tile"), Object->TryGetObjectField(TEXT("to_tile"), ToTile));
	if (FromTile != nullptr && FromTile->IsValid() && ToTile != nullptr && ToTile->IsValid())
	{
		TestEqual(TEXT("From tile x"), (*FromTile)->GetIntegerField(TEXT("x")), 2);
		TestEqual(TEXT("From tile y"), (*FromTile)->GetIntegerField(TEXT("y")), 3);
		TestEqual(TEXT("To tile x"), (*ToTile)->GetIntegerField(TEXT("x")), 3);
		TestEqual(TEXT("To tile y"), (*ToTile)->GetIntegerField(TEXT("y")), 3);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayTraceSerializesTurnControlEventArrayTest, "Wandbound.Core.ReplayTrace.SerializesTurnControlEventArray", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayTraceSerializesTurnControlEventArrayTest::RunTest(const FString& Parameters)
{
	FWBGameStateData EndTurnState;
	EndTurnState.CurrentPlayer = 0;
	EndTurnState.PriorityPlayer = 0;
	EndTurnState.TurnNumber = 7;
	const FWBApplyActionResult EndTurnResult = WBEffectRunner::ApplyAction(EndTurnState, MakeTurnControlAction(EWBActionType::EndTurn, 0));
	TestTrue(TEXT("End turn succeeds"), EndTurnResult.bOk);
	TestEqual(TEXT("End turn emits one trace"), EndTurnResult.TraceEvents.Num(), 1);

	FWBGameStateData PassState;
	PassState.CurrentPlayer = 0;
	PassState.PriorityPlayer = 1;
	PassState.TurnNumber = 3;
	const FWBApplyActionResult PassResult = WBEffectRunner::ApplyAction(PassState, MakeTurnControlAction(EWBActionType::Pass, 1));
	TestTrue(TEXT("Pass succeeds"), PassResult.bOk);
	TestEqual(TEXT("Pass emits one trace"), PassResult.TraceEvents.Num(), 1);

	if (EndTurnResult.TraceEvents.Num() != 1 || PassResult.TraceEvents.Num() != 1)
	{
		return false;
	}

	TArray<FWBTraceEvent> Events;
	Events.Add(EndTurnResult.TraceEvents[0]);
	Events.Add(PassResult.TraceEvents[0]);

	const TArray<TSharedPtr<FJsonValue>> Values = ParseJsonArray(WBReplayTrace::SerializeEvents(Events));
	TestEqual(TEXT("Trace event array count"), Values.Num(), 2);
	if (Values.Num() != 2)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> EndTurnObject = Values[0]->AsObject();
	TestTrue(TEXT("End turn event parses"), EndTurnObject.IsValid());
	if (EndTurnObject.IsValid())
	{
		TestEqual(TEXT("End turn kind"), EndTurnObject->GetStringField(TEXT("kind")), FString(TEXT("end_turn")));
		TestTrue(TEXT("End turn ok"), EndTurnObject->GetBoolField(TEXT("ok")));
		TestEqual(TEXT("End turn player"), EndTurnObject->GetIntegerField(TEXT("player_id")), 0);
		TestEqual(TEXT("End turn from player"), EndTurnObject->GetIntegerField(TEXT("from_player")), 0);
		TestEqual(TEXT("End turn to player"), EndTurnObject->GetIntegerField(TEXT("to_player")), 1);
		TestEqual(TEXT("End turn number"), EndTurnObject->GetIntegerField(TEXT("turn_number")), 8);
	}

	const TSharedPtr<FJsonObject> PassObject = Values[1]->AsObject();
	TestTrue(TEXT("Pass event parses"), PassObject.IsValid());
	if (PassObject.IsValid())
	{
		TestEqual(TEXT("Pass kind"), PassObject->GetStringField(TEXT("kind")), FString(TEXT("pass")));
		TestTrue(TEXT("Pass ok"), PassObject->GetBoolField(TEXT("ok")));
		TestEqual(TEXT("Pass player"), PassObject->GetIntegerField(TEXT("player_id")), 1);
		TestEqual(TEXT("Pass from player"), PassObject->GetIntegerField(TEXT("from_player")), 1);
		TestEqual(TEXT("Pass to player"), PassObject->GetIntegerField(TEXT("to_player")), 0);
		TestEqual(TEXT("Pass turn number"), PassObject->GetIntegerField(TEXT("turn_number")), 3);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayLogRecordsDecisionMetadataTest, "Wandbound.Core.ReplayLog.RecordsDecisionMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayLogRecordsDecisionMetadataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	const TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(State);
	TestTrue(TEXT("Legal actions generated"), LegalActions.Num() > 0);
	if (LegalActions.Num() == 0)
	{
		return false;
	}

	const FWBAction ChosenAction = LegalActions[0];
	const FWBApplyActionResult Result = WBEffectRunner::ApplyAction(State, ChosenAction);
	TestTrue(TEXT("Chosen action applies"), Result.bOk);

	FWBReplayLog Log;
	const FWBReplayDecisionRecord& Record = Log.RecordDecision(0, ChosenAction, LegalActions, Result.TraceEvents);

	TestEqual(TEXT("Replay log has one decision"), Log.Num(), 1);
	TestEqual(TEXT("Decision index recorded"), Record.DecisionIndex, 0);
	TestEqual(TEXT("Chosen action id recorded"), Record.ChosenActionId, WBActionCodec::MakeActionId(ChosenAction));
	TestEqual(TEXT("Legal action ids recorded"), Record.LegalActionIds.Num(), LegalActions.Num());
	if (Record.LegalActionIds.Num() > 0)
	{
		TestEqual(TEXT("First legal action id is deterministic"), Record.LegalActionIds[0], WBActionCodec::MakeActionId(LegalActions[0]));
	}
	TestEqual(TEXT("Raw trace events retained"), Record.TraceEvents.Num(), Result.TraceEvents.Num());

	const TArray<TSharedPtr<FJsonValue>> SerializedTraceValues = ParseJsonArray(Record.SerializedTraceEventsJson);
	TestEqual(TEXT("Serialized trace events retained"), SerializedTraceValues.Num(), Result.TraceEvents.Num());
	if (SerializedTraceValues.Num() > 0)
	{
		const TSharedPtr<FJsonObject> TraceObject = SerializedTraceValues[0]->AsObject();
		TestTrue(TEXT("Serialized trace event parses"), TraceObject.IsValid());
		if (TraceObject.IsValid())
		{
			TestEqual(TEXT("Serialized trace event kind"), TraceObject->GetStringField(TEXT("kind")), FString(TEXT("move")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayLogSerializesDecisionDocumentTest, "Wandbound.Core.ReplayLog.SerializesDecisionDocument", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayLogSerializesDecisionDocumentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	const TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(State);
	TestTrue(TEXT("Legal actions generated"), LegalActions.Num() > 0);
	if (LegalActions.Num() == 0)
	{
		return false;
	}

	const FWBAction ChosenAction = LegalActions[0];
	const FWBApplyActionResult Result = WBEffectRunner::ApplyAction(State, ChosenAction);
	TestTrue(TEXT("Chosen action applies"), Result.bOk);

	FWBReplayLog Log;
	Log.RecordDecision(0, ChosenAction, LegalActions, Result.TraceEvents);

	const TSharedPtr<FJsonObject> Root = ParseJsonObject(Log.Serialize());
	TestTrue(TEXT("Replay log parses"), Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	TestTrue(TEXT("Replay log has decisions array"), Root->TryGetArrayField(TEXT("decisions"), Decisions));
	if (Decisions == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("Replay log serializes one decision"), Decisions->Num(), 1);
	if (Decisions->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Decision = (*Decisions)[0]->AsObject();
	TestTrue(TEXT("Decision object parses"), Decision.IsValid());
	if (!Decision.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("Decision index serialized"), Decision->GetIntegerField(TEXT("decision_index")), 0);
	TestEqual(TEXT("Chosen action id serialized"), Decision->GetStringField(TEXT("chosen_action_id")), WBActionCodec::MakeActionId(ChosenAction));

	const TSharedPtr<FJsonObject>* ChosenActionObject = nullptr;
	TestTrue(TEXT("Chosen action object serialized"), Decision->TryGetObjectField(TEXT("chosen_action"), ChosenActionObject));
	if (ChosenActionObject != nullptr && ChosenActionObject->IsValid())
	{
		TestEqual(TEXT("Chosen action kind"), (*ChosenActionObject)->GetStringField(TEXT("kind")), FString(TEXT("move")));
		TestEqual(TEXT("Chosen action player"), (*ChosenActionObject)->GetIntegerField(TEXT("player_id")), 0);
		TestEqual(TEXT("Chosen action unit"), (*ChosenActionObject)->GetIntegerField(TEXT("unit_id")), 42);
		TestEqual(TEXT("Chosen action to x"), (*ChosenActionObject)->GetIntegerField(TEXT("to_x")), ChosenAction.ToTile.X);
		TestEqual(TEXT("Chosen action to y"), (*ChosenActionObject)->GetIntegerField(TEXT("to_y")), ChosenAction.ToTile.Y);
	}

	const TArray<TSharedPtr<FJsonValue>>* LegalActionIds = nullptr;
	TestTrue(TEXT("Legal action ids serialized"), Decision->TryGetArrayField(TEXT("legal_action_ids"), LegalActionIds));
	if (LegalActionIds != nullptr)
	{
		TestEqual(TEXT("Legal action ids count"), LegalActionIds->Num(), LegalActions.Num());
		if (LegalActionIds->Num() > 0)
		{
			TestEqual(TEXT("First legal action id serialized"), (*LegalActionIds)[0]->AsString(), WBActionCodec::MakeActionId(LegalActions[0]));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* TraceEvents = nullptr;
	TestTrue(TEXT("Trace events serialized"), Decision->TryGetArrayField(TEXT("trace_events"), TraceEvents));
	if (TraceEvents != nullptr)
	{
		TestEqual(TEXT("Trace event count"), TraceEvents->Num(), 1);
		if (TraceEvents->Num() == 1)
		{
			const TSharedPtr<FJsonObject> TraceObject = (*TraceEvents)[0]->AsObject();
			TestTrue(TEXT("Trace event parses"), TraceObject.IsValid());
			if (TraceObject.IsValid())
			{
				TestEqual(TEXT("Trace event kind"), TraceObject->GetStringField(TEXT("kind")), FString(TEXT("move")));
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayLogSerializesTurnControlDecisionsTest, "Wandbound.Core.ReplayLog.SerializesTurnControlDecisions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayLogSerializesTurnControlDecisionsTest::RunTest(const FString& Parameters)
{
	FWBReplayLog Log;

	FWBGameStateData EndTurnState;
	EndTurnState.CurrentPlayer = 0;
	EndTurnState.PriorityPlayer = 0;
	EndTurnState.TurnNumber = 7;
	const TArray<FWBAction> EndTurnLegalActions = WBRules::GenerateLegalActions(EndTurnState);
	const FWBAction EndTurnAction = MakeTurnControlAction(EWBActionType::EndTurn, 0);
	const FWBApplyActionResult EndTurnResult = WBEffectRunner::ApplyAction(EndTurnState, EndTurnAction);
	TestTrue(TEXT("End turn applies"), EndTurnResult.bOk);
	Log.RecordDecision(1, EndTurnAction, EndTurnLegalActions, EndTurnResult.TraceEvents);

	FWBGameStateData PassState;
	PassState.CurrentPlayer = 0;
	PassState.PriorityPlayer = 1;
	PassState.TurnNumber = 3;
	const TArray<FWBAction> PassLegalActions = WBRules::GenerateLegalActions(PassState);
	const FWBAction PassAction = MakeTurnControlAction(EWBActionType::Pass, 1);
	const FWBApplyActionResult PassResult = WBEffectRunner::ApplyAction(PassState, PassAction);
	TestTrue(TEXT("Pass applies"), PassResult.bOk);
	Log.RecordDecision(2, PassAction, PassLegalActions, PassResult.TraceEvents);

	const TSharedPtr<FJsonObject> Root = ParseJsonObject(Log.Serialize());
	TestTrue(TEXT("Replay log parses"), Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	TestTrue(TEXT("Replay log has decisions"), Root->TryGetArrayField(TEXT("decisions"), Decisions));
	if (Decisions == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("Two turn-control decisions serialized"), Decisions->Num(), 2);
	if (Decisions->Num() != 2)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> EndTurnDecision = (*Decisions)[0]->AsObject();
	const TSharedPtr<FJsonObject> PassDecision = (*Decisions)[1]->AsObject();
	TestTrue(TEXT("End turn decision parses"), EndTurnDecision.IsValid());
	TestTrue(TEXT("Pass decision parses"), PassDecision.IsValid());
	if (!EndTurnDecision.IsValid() || !PassDecision.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("End turn decision index"), EndTurnDecision->GetIntegerField(TEXT("decision_index")), 1);
	TestEqual(TEXT("End turn chosen action id"), EndTurnDecision->GetStringField(TEXT("chosen_action_id")), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass decision index"), PassDecision->GetIntegerField(TEXT("decision_index")), 2);
	TestEqual(TEXT("Pass chosen action id"), PassDecision->GetStringField(TEXT("chosen_action_id")), FString(TEXT("pass:p1")));

	const TSharedPtr<FJsonObject>* EndTurnActionObject = nullptr;
	TestTrue(TEXT("End turn action serialized"), EndTurnDecision->TryGetObjectField(TEXT("chosen_action"), EndTurnActionObject));
	if (EndTurnActionObject != nullptr && EndTurnActionObject->IsValid())
	{
		TestEqual(TEXT("End turn action kind"), (*EndTurnActionObject)->GetStringField(TEXT("kind")), FString(TEXT("end_turn")));
	}

	const TSharedPtr<FJsonObject>* PassActionObject = nullptr;
	TestTrue(TEXT("Pass action serialized"), PassDecision->TryGetObjectField(TEXT("chosen_action"), PassActionObject));
	if (PassActionObject != nullptr && PassActionObject->IsValid())
	{
		TestEqual(TEXT("Pass action kind"), (*PassActionObject)->GetStringField(TEXT("kind")), FString(TEXT("pass")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayVerifierAcceptsMoveAndEndTurnLogTest, "Wandbound.Core.ReplayVerifier.AcceptsMoveAndEndTurnLog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayVerifierAcceptsMoveAndEndTurnLogTest::RunTest(const FString& Parameters)
{
	FWBGameStateData InitialState;
	InitialState.CurrentPlayer = 0;
	InitialState.PriorityPlayer = 0;
	InitialState.TurnNumber = 1;
	TestTrue(TEXT("Unit added"), InitialState.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));

	FWBGameStateData RecordingState = InitialState;
	FWBReplayLog Log;

	TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(RecordingState);
	TestTrue(TEXT("Initial legal actions include a move"), LegalActions.Num() > 0);
	if (LegalActions.Num() == 0)
	{
		return false;
	}

	const FWBAction MoveAction = LegalActions[0];
	TestTrue(TEXT("Chosen first action is move"), MoveAction.Type == EWBActionType::Move);
	const FWBApplyActionResult MoveResult = WBEffectRunner::ApplyAction(RecordingState, MoveAction);
	TestTrue(TEXT("Move applies while recording"), MoveResult.bOk);
	Log.RecordDecision(0, MoveAction, LegalActions, MoveResult.TraceEvents);

	LegalActions = WBRules::GenerateLegalActions(RecordingState);
	TestEqual(TEXT("Only end turn remains after spending movement"), LegalActions.Num(), 1);
	if (LegalActions.Num() != 1)
	{
		return false;
	}

	const FWBAction EndTurnAction = LegalActions[0];
	TestTrue(TEXT("Second action is end turn"), EndTurnAction.Type == EWBActionType::EndTurn);
	const FWBApplyActionResult EndTurnResult = WBEffectRunner::ApplyAction(RecordingState, EndTurnAction);
	TestTrue(TEXT("End turn applies while recording"), EndTurnResult.bOk);
	Log.RecordDecision(1, EndTurnAction, LegalActions, EndTurnResult.TraceEvents);

	const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, Log.Serialize());

	TestTrue(TEXT("Replay verifies"), VerifyResult.bOk);
	TestEqual(TEXT("Two decisions verified"), VerifyResult.VerifiedDecisionCount, 2);
	TestEqual(TEXT("No failed decision"), VerifyResult.FailedDecisionIndex, -1);
	TestTrue(TEXT("No failure reason"), VerifyResult.Reason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayVerifierAcceptsPassLogTest, "Wandbound.Core.ReplayVerifier.AcceptsPassLog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayVerifierAcceptsPassLogTest::RunTest(const FString& Parameters)
{
	FWBGameStateData InitialState;
	InitialState.CurrentPlayer = 0;
	InitialState.PriorityPlayer = 1;
	InitialState.TurnNumber = 3;

	FWBGameStateData RecordingState = InitialState;
	const TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(RecordingState);
	TestEqual(TEXT("Response priority generates one pass"), LegalActions.Num(), 1);
	if (LegalActions.Num() != 1)
	{
		return false;
	}

	const FWBAction PassAction = LegalActions[0];
	TestTrue(TEXT("Generated action is pass"), PassAction.Type == EWBActionType::Pass);
	const FWBApplyActionResult PassResult = WBEffectRunner::ApplyAction(RecordingState, PassAction);
	TestTrue(TEXT("Pass applies while recording"), PassResult.bOk);

	FWBReplayLog Log;
	Log.RecordDecision(0, PassAction, LegalActions, PassResult.TraceEvents);

	const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, Log.Serialize());

	TestTrue(TEXT("Pass replay verifies"), VerifyResult.bOk);
	TestEqual(TEXT("One decision verified"), VerifyResult.VerifiedDecisionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayVerifierRejectsIllegalChosenActionTest, "Wandbound.Core.ReplayVerifier.RejectsIllegalChosenAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayVerifierRejectsIllegalChosenActionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData InitialState;
	InitialState.CurrentPlayer = 0;
	InitialState.PriorityPlayer = 0;
	TestTrue(TEXT("Unit added"), InitialState.AddUnitForTest(MakeUnit(42, 0, FWBTile(2, 3), 1)));
	InitialState.AddWallForTest(FWBWallEdge(FWBTile(2, 3), FWBTile(3, 3)));

	const TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(InitialState);
	const FWBAction IllegalMove = MakeMoveAction(0, 42, FWBTile(2, 3), FWBTile(3, 3));

	FWBReplayLog Log;
	Log.RecordDecision(0, IllegalMove, LegalActions, TArray<FWBTraceEvent>());

	const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, Log.Serialize());

	TestFalse(TEXT("Illegal replay fails verification"), VerifyResult.bOk);
	TestEqual(TEXT("Failure reason"), VerifyResult.Reason, FString(TEXT("chosen_action_not_legal")));
	TestEqual(TEXT("Failed decision index"), VerifyResult.FailedDecisionIndex, 0);
	TestEqual(TEXT("No decisions verified"), VerifyResult.VerifiedDecisionCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayVerifierRejectsTraceMismatchTest, "Wandbound.Core.ReplayVerifier.RejectsTraceMismatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayVerifierRejectsTraceMismatchTest::RunTest(const FString& Parameters)
{
	FWBGameStateData InitialState;
	InitialState.CurrentPlayer = 0;
	InitialState.PriorityPlayer = 1;
	InitialState.TurnNumber = 3;

	FWBGameStateData RecordingState = InitialState;
	const TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(RecordingState);
	TestEqual(TEXT("Pass is generated"), LegalActions.Num(), 1);
	if (LegalActions.Num() != 1)
	{
		return false;
	}

	const FWBAction PassAction = LegalActions[0];
	const FWBApplyActionResult PassResult = WBEffectRunner::ApplyAction(RecordingState, PassAction);
	TestTrue(TEXT("Pass applies while recording"), PassResult.bOk);

	FWBReplayLog Log;
	Log.RecordDecision(0, PassAction, LegalActions, PassResult.TraceEvents);

	const FString SerializedLog = Log.Serialize();
	const FString TamperedLog = SerializedLog.Replace(TEXT("\"to_player\": 0"), TEXT("\"to_player\": 1"));
	TestNotEqual(TEXT("Trace payload was tampered"), TamperedLog, SerializedLog);

	const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, TamperedLog);

	TestFalse(TEXT("Trace mismatch fails verification"), VerifyResult.bOk);
	TestEqual(TEXT("Failure reason"), VerifyResult.Reason, FString(TEXT("trace_events_mismatch")));
	TestEqual(TEXT("Failed decision index"), VerifyResult.FailedDecisionIndex, 0);
	TestEqual(TEXT("No decisions verified"), VerifyResult.VerifiedDecisionCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureSchemaAcceptsGoldenFixturesTest, "Wandbound.Core.ReplayFixture.SchemaAcceptsGoldenFixtures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureSchemaAcceptsGoldenFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		TArray<FString> SchemaErrors;
		const bool bSchemaValid = ValidateReplayFixtureSchema(Fixture, SchemaErrors);
		TestTrue(*FString::Printf(TEXT("%s schema valid: %s"), *FixtureName, *JoinSchemaErrors(SchemaErrors)), bSchemaValid);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureSchemaRejectsMalformedFixtureTest, "Wandbound.Core.ReplayFixture.SchemaRejectsMalformedFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureSchemaRejectsMalformedFixtureTest::RunTest(const FString& Parameters)
{
	const FString MalformedJson = TEXT("{\"id\":\"bad_replay_fixture\",\"schema_version\":\"1\",\"initial_state\":{},\"replay_log\":{\"decisions\":[{\"decision_index\":0,\"chosen_action\":{\"kind\":\"move\"},\"legal_action_ids\":[1],\"trace_events\":[{}]}]},\"expect\":{}}");
	const TSharedPtr<FJsonObject> Fixture = ParseJsonObject(MalformedJson);
	TestTrue(TEXT("Malformed fixture parses as JSON"), Fixture.IsValid());
	if (!Fixture.IsValid())
	{
		return false;
	}

	TArray<FString> SchemaErrors;
	const bool bSchemaValid = ValidateReplayFixtureSchema(Fixture, SchemaErrors);
	const FString JoinedErrors = JoinSchemaErrors(SchemaErrors);

	TestFalse(TEXT("Malformed fixture is rejected"), bSchemaValid);
	TestTrue(TEXT("Schema error names schema_version type"), JoinedErrors.Contains(TEXT("fixture.schema_version: expected number")));
	TestTrue(TEXT("Schema error names missing chosen action id"), JoinedErrors.Contains(TEXT("replay_log.decisions[0].chosen_action_id: missing required field")));
	TestTrue(TEXT("Schema error names malformed legal action id"), JoinedErrors.Contains(TEXT("replay_log.decisions[0].legal_action_ids[0]: expected string")));
	TestTrue(TEXT("Schema error names missing expected final state"), JoinedErrors.Contains(TEXT("expect.final_state: missing required field")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureInitialDecisionLegalActionIdsMatchExpectTest, "Wandbound.Core.ReplayFixture.InitialDecisionLegalActionIdsMatchExpect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureInitialDecisionLegalActionIdsMatchExpectTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		TArray<FString> ExpectedLegalActionIds;
		FString ExpectedReadReason;
		const bool bExpectedRead = ReadExpectedLegalActionIds(Fixture, ExpectedLegalActionIds, ExpectedReadReason);
		TestTrue(*FString::Printf(TEXT("%s expected legal action ids read: %s"), *FixtureName, *ExpectedReadReason), bExpectedRead);
		if (!bExpectedRead)
		{
			return false;
		}

		TArray<FString> InitialDecisionLegalActionIds;
		FString DecisionReadReason;
		const bool bDecisionRead = ReadReplayDecisionLegalActionIds(Fixture, 0, InitialDecisionLegalActionIds, DecisionReadReason);
		TestTrue(*FString::Printf(TEXT("%s initial decision legal action ids read: %s"), *FixtureName, *DecisionReadReason), bDecisionRead);
		if (!bDecisionRead)
		{
			return false;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s initial replay decision legal_action_ids match expect. expected [%s] got [%s]"),
				*FixtureName,
				*JoinStringsForTest(ExpectedLegalActionIds),
				*JoinStringsForTest(InitialDecisionLegalActionIds)),
			StringArraysEqual(InitialDecisionLegalActionIds, ExpectedLegalActionIds));

		FWBGameStateData InitialState;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, InitialState);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		FString ReplayLogJson;
		TestTrue(*FString::Printf(TEXT("%s has replay log"), *FixtureName), ExtractReplayLogJson(Fixture, ReplayLogJson));
		if (ReplayLogJson.IsEmpty())
		{
			return false;
		}

		const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, ReplayLogJson);
		TestTrue(*FString::Printf(TEXT("%s replay verifies"), *FixtureName), VerifyResult.bOk);
		TestTrue(*FString::Printf(TEXT("%s replay verification has no failure reason: %s"), *FixtureName, *VerifyResult.Reason), VerifyResult.Reason.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureTamperedLegalActionIdsFailVerificationTest, "Wandbound.Core.ReplayFixture.TamperedLegalActionIdsFailVerification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureTamperedLegalActionIdsFailVerificationTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData InitialState;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, InitialState);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		TArray<FString> ExpectedLegalActionIds;
		FString ExpectedReadReason;
		const bool bExpectedRead = ReadExpectedLegalActionIds(Fixture, ExpectedLegalActionIds, ExpectedReadReason);
		TestTrue(*FString::Printf(TEXT("%s expected legal action ids read: %s"), *FixtureName, *ExpectedReadReason), bExpectedRead);
		if (!bExpectedRead)
		{
			return false;
		}

		TArray<FString> InitialDecisionLegalActionIds;
		FString DecisionReadReason;
		const bool bDecisionRead = ReadReplayDecisionLegalActionIds(Fixture, 0, InitialDecisionLegalActionIds, DecisionReadReason);
		TestTrue(*FString::Printf(TEXT("%s initial decision legal action ids read: %s"), *FixtureName, *DecisionReadReason), bDecisionRead);
		if (!bDecisionRead)
		{
			return false;
		}
		TestTrue(
			*FString::Printf(
				TEXT("%s starts with matching legal action ids. expected [%s] got [%s]"),
				*FixtureName,
				*JoinStringsForTest(ExpectedLegalActionIds),
				*JoinStringsForTest(InitialDecisionLegalActionIds)),
			StringArraysEqual(InitialDecisionLegalActionIds, ExpectedLegalActionIds));

		FString TamperedReplayLogJson;
		FString TamperReason;
		const bool bTampered = AppendTamperedReplayDecisionLegalActionId(Fixture, 0, TamperedReplayLogJson, TamperReason);
		TestTrue(*FString::Printf(TEXT("%s tampered replay log serializes: %s"), *FixtureName, *TamperReason), bTampered);
		if (!bTampered)
		{
			return false;
		}

		const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, TamperedReplayLogJson);
		TestFalse(*FString::Printf(TEXT("%s tampered replay fails verification"), *FixtureName), VerifyResult.bOk);
		TestEqual(*FString::Printf(TEXT("%s tampered replay failure reason"), *FixtureName), VerifyResult.Reason, FString(TEXT("legal_action_ids_mismatch")));
		TestEqual(*FString::Printf(TEXT("%s tampered replay failed decision"), *FixtureName), VerifyResult.FailedDecisionIndex, 0);
		TestEqual(*FString::Printf(TEXT("%s tampered replay verifies no decisions"), *FixtureName), VerifyResult.VerifiedDecisionCount, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureTamperedChosenActionIdFailVerificationTest, "Wandbound.Core.ReplayFixture.TamperedChosenActionIdFailVerification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureTamperedChosenActionIdFailVerificationTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData InitialState;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, InitialState);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		FString TamperedReplayLogJson;
		FString TamperReason;
		const bool bTampered = TamperReplayDecisionChosenActionId(Fixture, 0, TamperedReplayLogJson, TamperReason);
		TestTrue(*FString::Printf(TEXT("%s chosen action id tampered replay log serializes: %s"), *FixtureName, *TamperReason), bTampered);
		if (!bTampered)
		{
			return false;
		}

		const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, TamperedReplayLogJson);
		TestFalse(*FString::Printf(TEXT("%s chosen action id tampered replay fails verification"), *FixtureName), VerifyResult.bOk);
		TestEqual(*FString::Printf(TEXT("%s chosen action id tampered replay failure reason"), *FixtureName), VerifyResult.Reason, FString(TEXT("chosen_action_id_mismatch")));
		TestEqual(*FString::Printf(TEXT("%s chosen action id tampered replay failed decision"), *FixtureName), VerifyResult.FailedDecisionIndex, 0);
		TestEqual(*FString::Printf(TEXT("%s chosen action id tampered replay verifies no decisions"), *FixtureName), VerifyResult.VerifiedDecisionCount, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureTamperedTraceEventsFailVerificationTest, "Wandbound.Core.ReplayFixture.TamperedTraceEventsFailVerification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureTamperedTraceEventsFailVerificationTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData InitialState;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, InitialState);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		FString TamperedReplayLogJson;
		FString TamperReason;
		const bool bTampered = TamperReplayDecisionTraceOk(Fixture, 0, TamperedReplayLogJson, TamperReason);
		TestTrue(*FString::Printf(TEXT("%s trace event tampered replay log serializes: %s"), *FixtureName, *TamperReason), bTampered);
		if (!bTampered)
		{
			return false;
		}

		const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, TamperedReplayLogJson);
		TestFalse(*FString::Printf(TEXT("%s trace event tampered replay fails verification"), *FixtureName), VerifyResult.bOk);
		TestEqual(*FString::Printf(TEXT("%s trace event tampered replay failure reason"), *FixtureName), VerifyResult.Reason, FString(TEXT("trace_events_mismatch")));
		TestEqual(*FString::Printf(TEXT("%s trace event tampered replay failed decision"), *FixtureName), VerifyResult.FailedDecisionIndex, 0);
		TestEqual(*FString::Printf(TEXT("%s trace event tampered replay verifies no decisions"), *FixtureName), VerifyResult.VerifiedDecisionCount, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureMalformedChosenActionsFailVerificationTest, "Wandbound.Core.ReplayFixture.MalformedChosenActionsFailVerification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureMalformedChosenActionsFailVerificationTest::RunTest(const FString& Parameters)
{
	struct FMalformedChosenActionCase
	{
		const TCHAR* FieldName;
		FString ExpectedReason;
	};

	const FMalformedChosenActionCase Cases[] = {
		{TEXT("kind"), TEXT("missing_action_kind")},
		{TEXT("player_id"), TEXT("missing_action_player_id")}
	};

	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData InitialState;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, InitialState);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		for (const FMalformedChosenActionCase& Case : Cases)
		{
			FString TamperedReplayLogJson;
			FString TamperReason;
			const bool bTampered = RemoveReplayDecisionChosenActionField(Fixture, 0, Case.FieldName, TamperedReplayLogJson, TamperReason);
			TestTrue(*FString::Printf(TEXT("%s chosen action missing %s serializes: %s"), *FixtureName, Case.FieldName, *TamperReason), bTampered);
			if (!bTampered)
			{
				return false;
			}

			const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, TamperedReplayLogJson);
			TestFalse(*FString::Printf(TEXT("%s chosen action missing %s fails verification"), *FixtureName, Case.FieldName), VerifyResult.bOk);
			TestEqual(*FString::Printf(TEXT("%s chosen action missing %s failure reason"), *FixtureName, Case.FieldName), VerifyResult.Reason, Case.ExpectedReason);
			TestEqual(*FString::Printf(TEXT("%s chosen action missing %s failed decision"), *FixtureName, Case.FieldName), VerifyResult.FailedDecisionIndex, 0);
			TestEqual(*FString::Printf(TEXT("%s chosen action missing %s verifies no decisions"), *FixtureName, Case.FieldName), VerifyResult.VerifiedDecisionCount, 0);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureMissingRequiredFieldsFailVerificationTest, "Wandbound.Core.ReplayFixture.MissingRequiredFieldsFailVerification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureMissingRequiredFieldsFailVerificationTest::RunTest(const FString& Parameters)
{
	struct FMissingReplayFieldCase
	{
		const TCHAR* FieldName;
		FString ExpectedReason;
		bool bReplayLogField;
		int32 ExpectedFailedDecisionIndex;
	};

	const FMissingReplayFieldCase Cases[] = {
		{TEXT("decisions"), TEXT("missing_decisions"), true, -1},
		{TEXT("decision_index"), TEXT("missing_decision_index"), false, 0},
		{TEXT("chosen_action"), TEXT("missing_chosen_action"), false, 0},
		{TEXT("chosen_action_id"), TEXT("missing_chosen_action_id"), false, 0},
		{TEXT("legal_action_ids"), TEXT("missing_legal_action_ids"), false, 0},
		{TEXT("trace_events"), TEXT("missing_trace_events"), false, 0}
	};

	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData InitialState;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, InitialState);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		for (const FMissingReplayFieldCase& Case : Cases)
		{
			FString TamperedReplayLogJson;
			FString TamperReason;
			const bool bTampered = Case.bReplayLogField
				? RemoveReplayLogField(Fixture, Case.FieldName, TamperedReplayLogJson, TamperReason)
				: RemoveReplayDecisionField(Fixture, 0, Case.FieldName, TamperedReplayLogJson, TamperReason);
			TestTrue(*FString::Printf(TEXT("%s missing %s serializes: %s"), *FixtureName, Case.FieldName, *TamperReason), bTampered);
			if (!bTampered)
			{
				return false;
			}

			const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, TamperedReplayLogJson);
			TestFalse(*FString::Printf(TEXT("%s missing %s fails verification"), *FixtureName, Case.FieldName), VerifyResult.bOk);
			TestEqual(*FString::Printf(TEXT("%s missing %s failure reason"), *FixtureName, Case.FieldName), VerifyResult.Reason, Case.ExpectedReason);
			TestEqual(*FString::Printf(TEXT("%s missing %s failed decision"), *FixtureName, Case.FieldName), VerifyResult.FailedDecisionIndex, Case.ExpectedFailedDecisionIndex);
			TestEqual(*FString::Printf(TEXT("%s missing %s verifies no decisions"), *FixtureName, Case.FieldName), VerifyResult.VerifiedDecisionCount, 0);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureVerifiesGoldenFixturesTest, "Wandbound.Core.ReplayFixture.VerifiesGoldenFixtures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureVerifiesGoldenFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = GetReplayFixtureNames();
	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenScenarioFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData InitialState;
		const bool bInitialStateBuilt = BuildGameStateFromFixture(Fixture, InitialState);
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), bInitialStateBuilt);
		if (!bInitialStateBuilt)
		{
			return false;
		}

		int32 ExpectedDecisionCount = 0;
		FString DecisionCountReason;
		const bool bDecisionCountRead = ReadReplayDecisionCount(Fixture, ExpectedDecisionCount, DecisionCountReason);
		TestTrue(*FString::Printf(TEXT("%s decision count reads: %s"), *FixtureName, *DecisionCountReason), bDecisionCountRead);
		if (!bDecisionCountRead)
		{
			return false;
		}

		FString ReplayLogJson;
		TestTrue(*FString::Printf(TEXT("%s has replay log"), *FixtureName), ExtractReplayLogJson(Fixture, ReplayLogJson));
		if (ReplayLogJson.IsEmpty())
		{
			return false;
		}

		const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, ReplayLogJson);

		TestTrue(*FString::Printf(TEXT("%s replay verifies: %s"), *FixtureName, *VerifyResult.Reason), VerifyResult.bOk);
		TestEqual(*FString::Printf(TEXT("%s verified decision count"), *FixtureName), VerifyResult.VerifiedDecisionCount, ExpectedDecisionCount);
		TestTrue(*FString::Printf(TEXT("%s replay verification has no failure reason: %s"), *FixtureName, *VerifyResult.Reason), VerifyResult.Reason.IsEmpty());

		FString FinalStateReason;
		const bool bFinalStateMatches = FinalStateMatchesFixtureExpectation(Fixture, VerifyResult.FinalState, FinalStateReason);
		TestTrue(*FString::Printf(TEXT("%s final state matches: %s"), *FixtureName, *FinalStateReason), bFinalStateMatches);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureVerifiesMovementAndEndTurnTest, "Wandbound.Core.ReplayFixture.VerifiesMovementAndEndTurn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureVerifiesMovementAndEndTurnTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Movement replay fixture loads"), LoadGoldenScenarioFixture(TEXT("replay_movement_end_turn.json"), Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData InitialState;
	TestTrue(TEXT("Movement replay fixture initial state builds"), BuildGameStateFromFixture(Fixture, InitialState));

	FString ReplayLogJson;
	TestTrue(TEXT("Movement replay fixture has replay log"), ExtractReplayLogJson(Fixture, ReplayLogJson));
	if (ReplayLogJson.IsEmpty())
	{
		return false;
	}

	const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, ReplayLogJson);

	TestTrue(TEXT("Movement replay fixture verifies"), VerifyResult.bOk);
	TestEqual(TEXT("Movement replay fixture verifies two decisions"), VerifyResult.VerifiedDecisionCount, 2);
	TestTrue(TEXT("Movement replay fixture has no failure reason"), VerifyResult.Reason.IsEmpty());

	FString FinalStateReason;
	const bool bFinalStateMatches = FinalStateMatchesFixtureExpectation(Fixture, VerifyResult.FinalState, FinalStateReason);
	TestTrue(*FString::Printf(TEXT("Movement replay fixture final state matches: %s"), *FinalStateReason), bFinalStateMatches);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReplayFixtureVerifiesPassResponseTest, "Wandbound.Core.ReplayFixture.VerifiesPassResponse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReplayFixtureVerifiesPassResponseTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Pass replay fixture loads"), LoadGoldenScenarioFixture(TEXT("replay_pass_response.json"), Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData InitialState;
	TestTrue(TEXT("Pass replay fixture initial state builds"), BuildGameStateFromFixture(Fixture, InitialState));

	FString ReplayLogJson;
	TestTrue(TEXT("Pass replay fixture has replay log"), ExtractReplayLogJson(Fixture, ReplayLogJson));
	if (ReplayLogJson.IsEmpty())
	{
		return false;
	}

	const FWBReplayVerificationResult VerifyResult = WBReplayVerifier::Verify(InitialState, ReplayLogJson);

	TestTrue(TEXT("Pass replay fixture verifies"), VerifyResult.bOk);
	TestEqual(TEXT("Pass replay fixture verifies one decision"), VerifyResult.VerifiedDecisionCount, 1);
	TestTrue(TEXT("Pass replay fixture has no failure reason"), VerifyResult.Reason.IsEmpty());

	FString FinalStateReason;
	const bool bFinalStateMatches = FinalStateMatchesFixtureExpectation(Fixture, VerifyResult.FinalState, FinalStateReason);
	TestTrue(*FString::Printf(TEXT("Pass replay fixture final state matches: %s"), *FinalStateReason), bFinalStateMatches);
	return true;
}

#endif
