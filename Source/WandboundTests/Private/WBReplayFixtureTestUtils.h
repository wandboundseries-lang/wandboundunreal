#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace WandboundTest
{
bool ExtractReplayLogJson(const TSharedPtr<FJsonObject>& Fixture, FString& OutReplayLogJson);

bool TryGetReplayDecisionObject(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 DecisionIndex,
	TSharedPtr<FJsonObject>& OutDecisionObject,
	FString& OutReason);

bool ReadReplayDecisionCount(
	const TSharedPtr<FJsonObject>& Fixture,
	int32& OutDecisionCount,
	FString& OutReason);

bool ReadReplayDecisionLegalActionIds(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 DecisionIndex,
	TArray<FString>& OutActionIds,
	FString& OutReason);

bool RemoveReplayLogField(
	const TSharedPtr<FJsonObject>& Fixture,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason);

bool RemoveReplayDecisionField(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 DecisionIndex,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason);

bool RemoveReplayDecisionChosenActionField(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 DecisionIndex,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason);

bool AppendTamperedReplayDecisionLegalActionId(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason);

bool TamperReplayDecisionChosenActionId(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason);

bool TamperReplayDecisionTraceOk(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason);
}
