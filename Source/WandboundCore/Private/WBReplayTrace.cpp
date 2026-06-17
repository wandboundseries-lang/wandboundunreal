#include "WBReplayTrace.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBEffectRunner.h"
#include "WBRules.h"

namespace
{
bool IsSetTile(const FWBTile& Tile)
{
	return Tile.X != -1 || Tile.Y != -1;
}

TSharedRef<FJsonObject> TileToJsonObject(const FWBTile& Tile)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("x"), Tile.X);
	Object->SetNumberField(TEXT("y"), Tile.Y);
	return Object;
}

TSharedRef<FJsonObject> MakeTraceEventJsonObject(const FWBTraceEvent& Event)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("kind"), Event.Kind.ToString());
	Object->SetBoolField(TEXT("ok"), Event.bOk);
	Object->SetNumberField(TEXT("player_id"), Event.PlayerId);

	if (!Event.ActionId.IsEmpty())
	{
		Object->SetStringField(TEXT("action_id"), Event.ActionId);
	}

	if (Event.SourceUnitId != -1)
	{
		Object->SetNumberField(TEXT("source_unit_id"), Event.SourceUnitId);
	}

	if (Event.FromPlayer != -1)
	{
		Object->SetNumberField(TEXT("from_player"), Event.FromPlayer);
	}

	if (Event.ToPlayer != -1)
	{
		Object->SetNumberField(TEXT("to_player"), Event.ToPlayer);
	}

	if (Event.NextPlayerId != -1)
	{
		Object->SetNumberField(TEXT("next_player_id"), Event.NextPlayerId);
	}

	if (Event.TurnNumber != -1)
	{
		Object->SetNumberField(TEXT("turn_number"), Event.TurnNumber);
	}

	if (Event.TurnNumberBefore != -1)
	{
		Object->SetNumberField(TEXT("turn_number_before"), Event.TurnNumberBefore);
	}

	if (Event.TurnNumberAfter != -1)
	{
		Object->SetNumberField(TEXT("turn_number_after"), Event.TurnNumberAfter);
	}

	if (Event.MPRoll != -1)
	{
		Object->SetNumberField(TEXT("mp_roll"), Event.MPRoll);
	}

	if (Event.RemainingMP != -1)
	{
		Object->SetNumberField(TEXT("remaining_mp"), Event.RemainingMP);
	}

	if (Event.WallsLeft != -1)
	{
		Object->SetNumberField(TEXT("walls_left"), Event.WallsLeft);
	}

	if (Event.WallRemovalsLeft != -1)
	{
		Object->SetNumberField(TEXT("wall_removals_left"), Event.WallRemovalsLeft);
	}

	if (!Event.StatusId.IsNone())
	{
		Object->SetStringField(TEXT("status_id"), Event.StatusId.ToString());
	}

	if (Event.TargetUnitId != -1)
	{
		Object->SetNumberField(TEXT("target_unit_id"), Event.TargetUnitId);
	}

	if (Event.AttacksLeftBefore != -1)
	{
		Object->SetNumberField(TEXT("attacks_left_before"), Event.AttacksLeftBefore);
	}

	if (Event.AttacksLeftAfter != -1)
	{
		Object->SetNumberField(TEXT("attacks_left_after"), Event.AttacksLeftAfter);
	}

	if (Event.DamageAmount != -1)
	{
		Object->SetNumberField(TEXT("damage_amount"), Event.DamageAmount);
	}

	if (Event.PreviousHP != -1)
	{
		Object->SetNumberField(TEXT("previous_hp"), Event.PreviousHP);
	}

	if (Event.NewHP != -1)
	{
		Object->SetNumberField(TEXT("new_hp"), Event.NewHP);
	}

	if (Event.PreviousMaxHP != -1)
	{
		Object->SetNumberField(TEXT("previous_max_hp"), Event.PreviousMaxHP);
	}

	if (Event.NewMaxHP != -1)
	{
		Object->SetNumberField(TEXT("new_max_hp"), Event.NewMaxHP);
	}

	if (Event.PreviousStatusTurns != -1)
	{
		Object->SetNumberField(TEXT("previous_status_turns"), Event.PreviousStatusTurns);
	}

	if (Event.NewStatusTurns != -1)
	{
		Object->SetNumberField(TEXT("new_status_turns"), Event.NewStatusTurns);
	}

	if (Event.bExpiredStatus)
	{
		Object->SetBoolField(TEXT("expired_status"), Event.bExpiredStatus);
	}

	if (Event.bAtOrBelowZeroHP)
	{
		Object->SetBoolField(TEXT("at_or_below_zero_hp"), Event.bAtOrBelowZeroHP);
	}

	if (IsSetTile(Event.FromTile))
	{
		Object->SetObjectField(TEXT("from_tile"), TileToJsonObject(Event.FromTile));
	}

	if (IsSetTile(Event.ToTile))
	{
		Object->SetObjectField(TEXT("to_tile"), TileToJsonObject(Event.ToTile));
	}

	if (!Event.Reason.IsEmpty())
	{
		Object->SetStringField(TEXT("reason"), Event.Reason);
	}

	return Object;
}

TArray<TSharedPtr<FJsonValue>> TraceEventsToJsonValues(const TArray<FWBTraceEvent>& Events)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(Events.Num());
	for (const FWBTraceEvent& Event : Events)
	{
		Values.Add(MakeShared<FJsonValueObject>(MakeTraceEventJsonObject(Event)));
	}

	return Values;
}

TSharedRef<FJsonObject> ActionToJsonObject(const FWBAction& Action)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(WBActionCodec::SerializeAction(Action));
	if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
	{
		return Object.ToSharedRef();
	}

	return MakeShared<FJsonObject>();
}

TSharedRef<FJsonObject> ReplayDecisionToJsonObject(const FWBReplayDecisionRecord& Record)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("decision_index"), Record.DecisionIndex);
	Object->SetStringField(TEXT("chosen_action_id"), Record.ChosenActionId);
	Object->SetObjectField(TEXT("chosen_action"), ActionToJsonObject(Record.ChosenAction));

	TArray<TSharedPtr<FJsonValue>> LegalActionIds;
	LegalActionIds.Reserve(Record.LegalActionIds.Num());
	for (const FString& ActionId : Record.LegalActionIds)
	{
		LegalActionIds.Add(MakeShared<FJsonValueString>(ActionId));
	}
	Object->SetArrayField(TEXT("legal_action_ids"), MoveTemp(LegalActionIds));
	Object->SetArrayField(TEXT("trace_events"), TraceEventsToJsonValues(Record.TraceEvents));
	return Object;
}

bool TryGetIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
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

FWBReplayVerificationResult VerificationFailure(
	const FString& Reason,
	const int32 FailedDecisionIndex,
	const int32 VerifiedDecisionCount)
{
	FWBReplayVerificationResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.FailedDecisionIndex = FailedDecisionIndex;
	Result.VerifiedDecisionCount = VerifiedDecisionCount;
	return Result;
}

bool TryParseReplayRoot(const FString& SerializedReplayLog, TSharedPtr<FJsonObject>& OutRoot)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SerializedReplayLog);
	return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
}

FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Object)
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

bool TryParseStringArray(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<FString>& OutStrings)
{
	OutStrings.Reset();
	OutStrings.Reserve(Values.Num());
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			return false;
		}

		OutStrings.Add(Value->AsString());
	}

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

bool JsonValuesEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B);

bool JsonObjectsEqual(const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
{
	if (!A.IsValid() || !B.IsValid() || A->Values.Num() != B->Values.Num())
	{
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : A->Values)
	{
		const TSharedPtr<FJsonValue>* OtherValue = B->Values.Find(Pair.Key);
		if (OtherValue == nullptr || !JsonValuesEqual(Pair.Value, *OtherValue))
		{
			return false;
		}
	}

	return true;
}

bool JsonArraysEqual(const TArray<TSharedPtr<FJsonValue>>& A, const TArray<TSharedPtr<FJsonValue>>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		if (!JsonValuesEqual(A[Index], B[Index]))
		{
			return false;
		}
	}

	return true;
}

bool JsonValuesEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
	if (!A.IsValid() || !B.IsValid() || A->Type != B->Type)
	{
		return false;
	}

	switch (A->Type)
	{
	case EJson::None:
	case EJson::Null:
		return true;
	case EJson::String:
		return A->AsString() == B->AsString();
	case EJson::Number:
		return FMath::IsNearlyEqual(A->AsNumber(), B->AsNumber());
	case EJson::Boolean:
		return A->AsBool() == B->AsBool();
	case EJson::Array:
		return JsonArraysEqual(A->AsArray(), B->AsArray());
	case EJson::Object:
		return JsonObjectsEqual(A->AsObject(), B->AsObject());
	default:
		return false;
	}
}

bool ParseTraceEventValues(const TArray<FWBTraceEvent>& TraceEvents, TArray<TSharedPtr<FJsonValue>>& OutValues)
{
	const FString Json = WBReplayTrace::SerializeEvents(TraceEvents);
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutValues);
}
}

TSharedRef<FJsonObject> WBReplayTrace::TraceEventToJsonObject(const FWBTraceEvent& Event)
{
	return MakeTraceEventJsonObject(Event);
}

FString WBReplayTrace::SerializeEvent(const FWBTraceEvent& Event)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(MakeTraceEventJsonObject(Event), Writer);
	return Output;
}

FString WBReplayTrace::SerializeEvents(const TArray<FWBTraceEvent>& Events)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(TraceEventsToJsonValues(Events), Writer);
	return Output;
}

const FWBReplayDecisionRecord& FWBReplayLog::RecordDecision(
	const int32 DecisionIndex,
	const FWBAction& ChosenAction,
	const TArray<FWBAction>& LegalActions,
	const TArray<FWBTraceEvent>& TraceEvents)
{
	FWBReplayDecisionRecord Record;
	Record.DecisionIndex = DecisionIndex;
	Record.ChosenAction = ChosenAction;
	Record.ChosenActionId = WBActionCodec::MakeActionId(ChosenAction);
	Record.LegalActionIds = WBActionCodec::MakeActionIds(LegalActions);
	Record.TraceEvents = TraceEvents;
	Record.SerializedTraceEventsJson = WBReplayTrace::SerializeEvents(TraceEvents);

	DecisionRecords.Add(MoveTemp(Record));
	return DecisionRecords.Last();
}

void FWBReplayLog::Reset()
{
	DecisionRecords.Reset();
}

int32 FWBReplayLog::Num() const
{
	return DecisionRecords.Num();
}

const TArray<FWBReplayDecisionRecord>& FWBReplayLog::GetDecisionRecords() const
{
	return DecisionRecords;
}

FString FWBReplayLog::Serialize() const
{
	TArray<TSharedPtr<FJsonValue>> Decisions;
	Decisions.Reserve(DecisionRecords.Num());
	for (const FWBReplayDecisionRecord& Record : DecisionRecords)
	{
		Decisions.Add(MakeShared<FJsonValueObject>(ReplayDecisionToJsonObject(Record)));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("decisions"), MoveTemp(Decisions));

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

FWBReplayVerificationResult WBReplayVerifier::Verify(
	const FWBGameStateData& InitialState,
	const FString& SerializedReplayLog)
{
	TSharedPtr<FJsonObject> Root;
	if (!TryParseReplayRoot(SerializedReplayLog, Root))
	{
		return VerificationFailure(TEXT("malformed_replay_log"), -1, 0);
	}

	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (!Root->TryGetArrayField(TEXT("decisions"), Decisions))
	{
		return VerificationFailure(TEXT("missing_decisions"), -1, 0);
	}

	FWBGameStateData State = InitialState;
	int32 VerifiedDecisionCount = 0;

	for (int32 DecisionArrayIndex = 0; DecisionArrayIndex < Decisions->Num(); ++DecisionArrayIndex)
	{
		const TSharedPtr<FJsonObject> Decision = (*Decisions)[DecisionArrayIndex]->AsObject();
		if (!Decision.IsValid())
		{
			return VerificationFailure(TEXT("malformed_decision"), DecisionArrayIndex, VerifiedDecisionCount);
		}

		int32 DecisionIndex = -1;
		if (!TryGetIntegerField(Decision, TEXT("decision_index"), DecisionIndex))
		{
			return VerificationFailure(TEXT("missing_decision_index"), DecisionArrayIndex, VerifiedDecisionCount);
		}

		const TSharedPtr<FJsonObject>* ChosenActionObject = nullptr;
		if (!Decision->TryGetObjectField(TEXT("chosen_action"), ChosenActionObject)
			|| ChosenActionObject == nullptr
			|| !ChosenActionObject->IsValid())
		{
			return VerificationFailure(TEXT("missing_chosen_action"), DecisionIndex, VerifiedDecisionCount);
		}

		const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(SerializeJsonObject(*ChosenActionObject), State);
		if (!DecodeResult.bOk)
		{
			return VerificationFailure(DecodeResult.Reason, DecisionIndex, VerifiedDecisionCount);
		}
		const FWBAction ChosenAction = DecodeResult.Action;

		const FString GeneratedChosenActionId = WBActionCodec::MakeActionId(ChosenAction);

		FString RecordedChosenActionId;
		if (!Decision->TryGetStringField(TEXT("chosen_action_id"), RecordedChosenActionId))
		{
			return VerificationFailure(TEXT("missing_chosen_action_id"), DecisionIndex, VerifiedDecisionCount);
		}

		if (RecordedChosenActionId != GeneratedChosenActionId)
		{
			return VerificationFailure(TEXT("chosen_action_id_mismatch"), DecisionIndex, VerifiedDecisionCount);
		}

		const TArray<FWBAction> GeneratedLegalActions = WBRules::GenerateLegalActions(State);
		const TArray<FString> GeneratedLegalActionIds = WBActionCodec::MakeActionIds(GeneratedLegalActions);

		const TArray<TSharedPtr<FJsonValue>>* RecordedLegalActionIdValues = nullptr;
		if (!Decision->TryGetArrayField(TEXT("legal_action_ids"), RecordedLegalActionIdValues)
			|| RecordedLegalActionIdValues == nullptr)
		{
			return VerificationFailure(TEXT("missing_legal_action_ids"), DecisionIndex, VerifiedDecisionCount);
		}

		TArray<FString> RecordedLegalActionIds;
		if (!TryParseStringArray(*RecordedLegalActionIdValues, RecordedLegalActionIds))
		{
			return VerificationFailure(TEXT("malformed_legal_action_ids"), DecisionIndex, VerifiedDecisionCount);
		}

		if (!StringArraysEqual(RecordedLegalActionIds, GeneratedLegalActionIds))
		{
			return VerificationFailure(TEXT("legal_action_ids_mismatch"), DecisionIndex, VerifiedDecisionCount);
		}

		if (!GeneratedLegalActionIds.Contains(GeneratedChosenActionId))
		{
			return VerificationFailure(TEXT("chosen_action_not_legal"), DecisionIndex, VerifiedDecisionCount);
		}

		const FWBApplyActionResult ApplyResult = WBEffectRunner::ApplyAction(State, ChosenAction);
		if (!ApplyResult.bOk)
		{
			return VerificationFailure(FString::Printf(TEXT("apply_failed:%s"), *ApplyResult.Reason), DecisionIndex, VerifiedDecisionCount);
		}

		const TArray<TSharedPtr<FJsonValue>>* RecordedTraceEvents = nullptr;
		if (!Decision->TryGetArrayField(TEXT("trace_events"), RecordedTraceEvents)
			|| RecordedTraceEvents == nullptr)
		{
			return VerificationFailure(TEXT("missing_trace_events"), DecisionIndex, VerifiedDecisionCount);
		}

		TArray<TSharedPtr<FJsonValue>> GeneratedTraceEvents;
		if (!ParseTraceEventValues(ApplyResult.TraceEvents, GeneratedTraceEvents))
		{
			return VerificationFailure(TEXT("generated_trace_events_malformed"), DecisionIndex, VerifiedDecisionCount);
		}

		if (!JsonArraysEqual(*RecordedTraceEvents, GeneratedTraceEvents))
		{
			return VerificationFailure(TEXT("trace_events_mismatch"), DecisionIndex, VerifiedDecisionCount);
		}

		++VerifiedDecisionCount;
	}

	FWBReplayVerificationResult Result;
	Result.bOk = true;
	Result.VerifiedDecisionCount = VerifiedDecisionCount;
	Result.FinalState = State;
	return Result;
}
