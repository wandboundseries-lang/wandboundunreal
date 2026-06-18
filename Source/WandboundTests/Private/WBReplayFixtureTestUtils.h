#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"
#include "WBRuntimeTurnResolutionAdapter.h"
#include "WBTurnController.h"

class FJsonObject;

namespace WandboundTest
{
enum class EWBFixtureOperationKind : uint8
{
	Unknown,
	ApplyAction,
	ApplyAttackDeclare,
	ApplyPendingAttackDamage,
	ApplyZeroHPDeathRemoval,
	ResolveDamageRequest,
	ApplyArmorEffect,
	ApplyStartOfTurnStatusTicks,
	ApplyEndOfTurnStatusTicks,
	AttackDeclareThenDamage,
	ApplyTurnCommand,
	ApplyRuntimeSelectedAction,
	ApplyRuntimeSelectedActionWithResult,
	ApplyRuntimeSelectedActionWithResultAndSerialize
};

bool BuildGameStateFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& OutState,
	FString& OutReason);

bool ParseTurnCommandFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBTurnCommand& OutCommand,
	FString& OutReason);

bool ApplyTurnCommandFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	FString& OutReason);

bool ApplyRuntimeSelectedActionFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	int32& OutRollSourceRemainingCount,
	FString& OutReason);

bool ApplyRuntimeSelectedActionWithResultFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBRuntimeSelectedActionResult& OutEnvelope,
	int32& OutRollSourceRemainingCount,
	FString& OutReason);

bool ApplyRuntimeSelectedActionWithResultAndSerializeFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBRuntimeSelectedActionResult& OutEnvelope,
	FString& OutSerializedJson,
	int32& OutRollSourceRemainingCount,
	FString& OutReason);

bool ApplyFixtureOperation(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	EWBFixtureOperationKind& OutOperationKind,
	FString& OutReason);

bool ExpectTraceOrder(
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<FWBTraceEvent>& TraceEvents,
	FString& OutReason);

bool ExpectPlayerResources(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason);

bool ExpectUnitStatusSummary(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason);

bool ExpectFinalTurnState(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason);

bool ExpectRuntimeRollSourceRemainingCount(
	const TSharedPtr<FJsonObject>& Fixture,
	int32 RollSourceRemainingCount,
	FString& OutReason);

bool ExpectRuntimeSelectedActionEnvelope(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBRuntimeSelectedActionResult& Envelope,
	int32 RollSourceRemainingCount,
	FString& OutReason);

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
