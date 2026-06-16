#include "WBReplayFixtureTestUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WandboundTest
{
namespace
{
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

bool ReadStringArrayValues(
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

bool TryGetReplayLogObject(
	const TSharedPtr<FJsonObject>& Fixture,
	TSharedPtr<FJsonObject>& OutReplayLogObject,
	FString& OutReason)
{
	OutReplayLogObject.Reset();

	const TSharedPtr<FJsonObject>* ReplayLogObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("replay_log"), ReplayLogObject)
		|| ReplayLogObject == nullptr
		|| !ReplayLogObject->IsValid())
	{
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	OutReplayLogObject = *ReplayLogObject;
	OutReason.Reset();
	return true;
}

bool TryGetReplayDecisions(
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<TSharedPtr<FJsonValue>>*& OutDecisions,
	FString& OutReason)
{
	OutDecisions = nullptr;

	TSharedPtr<FJsonObject> ReplayLogObject;
	if (!TryGetReplayLogObject(Fixture, ReplayLogObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (!ReplayLogObject->TryGetArrayField(TEXT("decisions"), Decisions) || Decisions == nullptr)
	{
		OutReason = TEXT("missing_replay_log_decisions");
		return false;
	}

	OutDecisions = Decisions;
	OutReason.Reset();
	return true;
}

bool RemoveObjectFieldAndExtractReplayLog(
	const TSharedPtr<FJsonObject>& Fixture,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldPath,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	if (!Object.IsValid())
	{
		OutReason = FString::Printf(TEXT("missing_%s"), *FieldPath);
		return false;
	}

	const FString FieldNameString(FieldName);
	const TSharedPtr<FJsonValue>* OriginalValuePtr = Object->Values.Find(FieldNameString);
	if (OriginalValuePtr == nullptr || !OriginalValuePtr->IsValid())
	{
		OutReason = FString::Printf(TEXT("missing_%s_%s"), *FieldPath, FieldName);
		return false;
	}

	const TSharedPtr<FJsonValue> OriginalValue = *OriginalValuePtr;
	Object->Values.Remove(FieldNameString);
	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		Object->Values.Add(FieldNameString, OriginalValue);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	Object->Values.Add(FieldNameString, OriginalValue);
	OutReason.Reset();
	return true;
}
}

bool ExtractReplayLogJson(const TSharedPtr<FJsonObject>& Fixture, FString& OutReplayLogJson)
{
	TSharedPtr<FJsonObject> ReplayLogObject;
	FString Reason;
	if (!TryGetReplayLogObject(Fixture, ReplayLogObject, Reason))
	{
		return false;
	}

	OutReplayLogJson = SerializeJsonObject(ReplayLogObject);
	return !OutReplayLogJson.IsEmpty();
}

bool TryGetReplayDecisionObject(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	TSharedPtr<FJsonObject>& OutDecisionObject,
	FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (!TryGetReplayDecisions(Fixture, Decisions, OutReason))
	{
		return false;
	}

	if (!Decisions->IsValidIndex(DecisionIndex))
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d"), DecisionIndex);
		return false;
	}

	OutDecisionObject = (*Decisions)[DecisionIndex].IsValid() ? (*Decisions)[DecisionIndex]->AsObject() : nullptr;
	if (!OutDecisionObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_replay_log_decision_%d"), DecisionIndex);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ReadReplayDecisionCount(
	const TSharedPtr<FJsonObject>& Fixture,
	int32& OutDecisionCount,
	FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (!TryGetReplayDecisions(Fixture, Decisions, OutReason))
	{
		OutDecisionCount = 0;
		return false;
	}

	OutDecisionCount = Decisions->Num();
	return true;
}

bool RemoveReplayLogField(
	const TSharedPtr<FJsonObject>& Fixture,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ReplayLogObject;
	if (!TryGetReplayLogObject(Fixture, ReplayLogObject, OutReason))
	{
		return false;
	}

	return RemoveObjectFieldAndExtractReplayLog(Fixture, ReplayLogObject, TEXT("replay_log"), FieldName, OutReplayLogJson, OutReason);
}

bool RemoveReplayDecisionField(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	return RemoveObjectFieldAndExtractReplayLog(
		Fixture,
		DecisionObject,
		FString::Printf(TEXT("replay_log_decision_%d"), DecisionIndex),
		FieldName,
		OutReplayLogJson,
		OutReason);
}

bool RemoveReplayDecisionChosenActionField(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ChosenActionObject = nullptr;
	if (!DecisionObject->TryGetObjectField(TEXT("chosen_action"), ChosenActionObject)
		|| ChosenActionObject == nullptr
		|| !ChosenActionObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_chosen_action"), DecisionIndex);
		return false;
	}

	return RemoveObjectFieldAndExtractReplayLog(
		Fixture,
		*ChosenActionObject,
		FString::Printf(TEXT("replay_log_decision_%d_chosen_action"), DecisionIndex),
		FieldName,
		OutReplayLogJson,
		OutReason);
}

bool ReadReplayDecisionLegalActionIds(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	TArray<FString>& OutActionIds,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ActionIdValues = nullptr;
	if (!DecisionObject->TryGetArrayField(TEXT("legal_action_ids"), ActionIdValues) || ActionIdValues == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_legal_action_ids"), DecisionIndex);
		return false;
	}

	return ReadStringArrayValues(
		*ActionIdValues,
		FString::Printf(TEXT("replay_log_decision_%d_legal_action_ids"), DecisionIndex),
		OutActionIds,
		OutReason);
}

bool AppendTamperedReplayDecisionLegalActionId(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ExistingActionIdValues = nullptr;
	if (!DecisionObject->TryGetArrayField(TEXT("legal_action_ids"), ExistingActionIdValues) || ExistingActionIdValues == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_legal_action_ids"), DecisionIndex);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>> OriginalActionIdValues = *ExistingActionIdValues;
	TArray<TSharedPtr<FJsonValue>> TamperedActionIdValues = OriginalActionIdValues;
	TamperedActionIdValues.Add(MakeShared<FJsonValueString>(TEXT("tampered:legal_action_ids")));
	DecisionObject->SetArrayField(TEXT("legal_action_ids"), TamperedActionIdValues);

	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		DecisionObject->SetArrayField(TEXT("legal_action_ids"), OriginalActionIdValues);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	DecisionObject->SetArrayField(TEXT("legal_action_ids"), OriginalActionIdValues);
	OutReason.Reset();
	return true;
}

bool TamperReplayDecisionChosenActionId(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	FString OriginalChosenActionId;
	if (!DecisionObject->TryGetStringField(TEXT("chosen_action_id"), OriginalChosenActionId))
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_chosen_action_id"), DecisionIndex);
		return false;
	}

	DecisionObject->SetStringField(TEXT("chosen_action_id"), OriginalChosenActionId + TEXT(":tampered"));
	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		DecisionObject->SetStringField(TEXT("chosen_action_id"), OriginalChosenActionId);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	DecisionObject->SetStringField(TEXT("chosen_action_id"), OriginalChosenActionId);
	OutReason.Reset();
	return true;
}

bool TamperReplayDecisionTraceOk(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* TraceEventValues = nullptr;
	if (!DecisionObject->TryGetArrayField(TEXT("trace_events"), TraceEventValues)
		|| TraceEventValues == nullptr
		|| TraceEventValues->Num() == 0)
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_trace_events"), DecisionIndex);
		return false;
	}

	const TSharedPtr<FJsonObject> TraceEventObject = (*TraceEventValues)[0].IsValid()
		? (*TraceEventValues)[0]->AsObject()
		: nullptr;
	if (!TraceEventObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_replay_log_decision_%d_trace_events[0]"), DecisionIndex);
		return false;
	}

	bool bOriginalOk = false;
	if (!TraceEventObject->TryGetBoolField(TEXT("ok"), bOriginalOk))
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_trace_events[0]_ok"), DecisionIndex);
		return false;
	}

	TraceEventObject->SetBoolField(TEXT("ok"), !bOriginalOk);
	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		TraceEventObject->SetBoolField(TEXT("ok"), bOriginalOk);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	TraceEventObject->SetBoolField(TEXT("ok"), bOriginalOk);
	OutReason.Reset();
	return true;
}
}
