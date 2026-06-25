#include "WBReplayFixtureTestUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBActionCodec.h"
#include "WBCardActivationAffordability.h"
#include "WBArmorEffect.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationCommand.h"
#include "WBCardActivationLegalActionReplayVerifier.h"
#include "WBCardActivationCostPaymentVerifier.h"
#include "WBCardActivationExpansion.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBCardActivationSourceGate.h"
#include "WBCardDefinition.h"
#include "WBDamageEffect.h"
#include "WBDamageResolution.h"
#include "WBEffectRequest.h"
#include "WBEffectRunner.h"
#include "WBHealEffect.h"
#include "WBMPRollSource.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"
#include "WBStatusEffect.h"

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

bool ReadOptionalStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	TArray<FString>& OutStrings,
	FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), *FieldPath);
		return false;
	}

	return ReadStringArrayValues(*Values, FieldPath, OutStrings, OutReason);
}

bool ReadOptionalIntegerArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	TArray<int32>& OutIntegers,
	FString& OutReason)
{
	OutIntegers.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), *FieldPath);
		return false;
	}

	for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = (*Values)[Index];
		if (!Value.IsValid() || Value->Type != EJson::Number)
		{
			OutReason = FString::Printf(TEXT("malformed_%s[%d]"), *FieldPath, Index);
			return false;
		}

		OutIntegers.Add(static_cast<int32>(Value->AsNumber()));
	}

	OutReason.Reset();
	return true;
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue);

bool TryGetExpectedObject(const TSharedPtr<FJsonObject>& Fixture, TSharedPtr<FJsonObject>& OutExpectedObject, FString& OutReason);

TArray<FString> ActivationActionIdsFromSet(const FWBCardActivationLegalActionSet& ActionSet)
{
	TArray<FString> ActionIds;
	ActionIds.Reserve(ActionSet.Actions.Num());
	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		ActionIds.Add(Action.ActivationActionId);
	}

	return ActionIds;
}

TArray<FString> ActivationActionLabelsFromSet(const FWBCardActivationLegalActionSet& ActionSet)
{
	TArray<FString> Labels;
	Labels.Reserve(ActionSet.Actions.Num());
	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		Labels.Add(Action.PublicLabel);
	}

	return Labels;
}

TArray<FString> ActivationPresentationActionIdsFromSnapshot(
	const FWBCardActivationLegalActionPresentationSnapshot& Snapshot)
{
	TArray<FString> ActionIds;
	ActionIds.Reserve(Snapshot.Entries.Num());
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		ActionIds.Add(Entry.ActivationActionId);
	}

	return ActionIds;
}

TArray<FString> ActivationPresentationLabelsFromSnapshot(
	const FWBCardActivationLegalActionPresentationSnapshot& Snapshot)
{
	TArray<FString> Labels;
	Labels.Reserve(Snapshot.Entries.Num());
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		Labels.Add(Entry.PublicLabel);
	}

	return Labels;
}

TArray<FString> ActivationPresentationSourcePublicCardIdsFromSnapshot(
	const FWBCardActivationLegalActionPresentationSnapshot& Snapshot)
{
	TArray<FString> CardIds;
	CardIds.Reserve(Snapshot.Entries.Num());
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		CardIds.Add(Entry.SourcePublicCardId);
	}

	return CardIds;
}

TArray<FString> ActivationPresentationTargetPublicCardIdsFromSnapshot(
	const FWBCardActivationLegalActionPresentationSnapshot& Snapshot)
{
	TArray<FString> CardIds;
	CardIds.Reserve(Snapshot.Entries.Num());
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		CardIds.Add(Entry.TargetPublicCardId);
	}

	return CardIds;
}

FString ActivationPresentationPublicTextFromSnapshot(
	const FWBCardActivationLegalActionPresentationSnapshot& Snapshot)
{
	FString Text;
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		Text += Entry.PublicLabel;
		Text += TEXT("|");
		Text += Entry.SourcePublicCardId;
		Text += TEXT("|");
		Text += Entry.TargetPublicCardId;
		Text += TEXT("|");
	}

	return Text;
}

TArray<FString> ActivationCandidateIdsFromResult(const FWBCardActivationCandidateGenerationResult& GenerationResult)
{
	TArray<FString> CandidateIds;
	CandidateIds.Reserve(GenerationResult.Candidates.Num());
	for (const FWBCardActivationCandidate& Candidate : GenerationResult.Candidates)
	{
		CandidateIds.Add(Candidate.ActivationCandidateId);
	}

	return CandidateIds;
}

TArray<FString> ActivationCandidateLabelsFromResult(const FWBCardActivationCandidateGenerationResult& GenerationResult)
{
	TArray<FString> Labels;
	Labels.Reserve(GenerationResult.Candidates.Num());
	for (const FWBCardActivationCandidate& Candidate : GenerationResult.Candidates)
	{
		Labels.Add(Candidate.PublicLabel);
	}

	return Labels;
}

bool ValidateOptionalIntegerExpectation(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const TCHAR* FieldName,
	const int32 ActualValue,
	FString& OutReason)
{
	if (!ExpectedObject.IsValid() || !ExpectedObject->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	int32 ExpectedValue = -1;
	if (!TryReadIntegerField(ExpectedObject, FieldName, ExpectedValue))
	{
		OutReason = FString::Printf(TEXT("malformed_expected_%s"), FieldName);
		return false;
	}

	if (ActualValue != ExpectedValue)
	{
		OutReason = FString::Printf(TEXT("expected_%s_mismatch"), FieldName);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ValidateOptionalStringArrayExpectation(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const TCHAR* FieldName,
	const FString& FieldPath,
	const TArray<FString>& ActualValues,
	FString& OutReason)
{
	if (!ExpectedObject.IsValid() || !ExpectedObject->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	TArray<FString> ExpectedValues;
	if (!ReadOptionalStringArrayField(ExpectedObject, FieldName, FieldPath, ExpectedValues, OutReason))
	{
		return false;
	}

	if (ActualValues != ExpectedValues)
	{
		OutReason = FString::Printf(TEXT("expected_%s_mismatch"), FieldName);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool TryGetOptionalExpectedObject(const TSharedPtr<FJsonObject>& Fixture, TSharedPtr<FJsonObject>& OutExpectedObject)
{
	OutExpectedObject.Reset();

	const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expected"), ExpectedObject)
		&& ExpectedObject != nullptr
		&& ExpectedObject->IsValid())
	{
		OutExpectedObject = *ExpectedObject;
		return true;
	}

	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		&& ExpectObject != nullptr
		&& ExpectObject->IsValid())
	{
		OutExpectedObject = *ExpectObject;
		return true;
	}

	return false;
}

bool TryReadRequiredIntegerField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const TCHAR* MalformedReason,
	int32& OutValue,
	FString& OutReason)
{
	if (!TryReadIntegerField(Object, FieldName, OutValue))
	{
		OutReason = MalformedReason;
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ValidateExpectedCostPaymentCommit(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const FWBCardActivationCostPaymentCommit& Commit,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* CommitObject = nullptr;
	if (!ExpectedObject.IsValid() || !ExpectedObject->HasField(TEXT("cost_payment_commit")))
	{
		OutReason.Reset();
		return true;
	}

	if (!ExpectedObject->TryGetObjectField(TEXT("cost_payment_commit"), CommitObject)
		|| CommitObject == nullptr
		|| !CommitObject->IsValid())
	{
		OutReason = TEXT("malformed_expected_cost_payment_commit");
		return false;
	}

	bool bExpectedPayCost = Commit.bPayCostOnSuccess;
	if ((*CommitObject)->TryGetBoolField(TEXT("pay_cost_on_success"), bExpectedPayCost)
		&& Commit.bPayCostOnSuccess != bExpectedPayCost)
	{
		OutReason = TEXT("expected_cost_payment_commit_pay_cost_mismatch");
		return false;
	}

	int32 ExpectedInteger = -1;
	if (TryReadIntegerField(*CommitObject, TEXT("player_id"), ExpectedInteger) && Commit.PlayerId != ExpectedInteger)
	{
		OutReason = TEXT("expected_cost_payment_commit_player_id_mismatch");
		return false;
	}

	if (TryReadIntegerField(*CommitObject, TEXT("source_unit_id"), ExpectedInteger) && Commit.SourceUnitId != ExpectedInteger)
	{
		OutReason = TEXT("expected_cost_payment_commit_source_unit_id_mismatch");
		return false;
	}

	if (TryReadIntegerField(*CommitObject, TEXT("required_rr"), ExpectedInteger) && Commit.RequiredRR != ExpectedInteger)
	{
		OutReason = TEXT("expected_cost_payment_commit_required_rr_mismatch");
		return false;
	}

	FString ExpectedCostKind;
	if ((*CommitObject)->TryGetStringField(TEXT("cost_kind"), ExpectedCostKind)
		&& Commit.CostKind != FName(*ExpectedCostKind))
	{
		OutReason = TEXT("expected_cost_payment_commit_cost_kind_mismatch");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ValidateExpectedUsageCommit(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const FWBCardActivationUsageCommit& Commit,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* CommitObject = nullptr;
	if (!ExpectedObject.IsValid() || !ExpectedObject->HasField(TEXT("command_usage_commit")))
	{
		OutReason.Reset();
		return true;
	}

	if (!ExpectedObject->TryGetObjectField(TEXT("command_usage_commit"), CommitObject)
		|| CommitObject == nullptr
		|| !CommitObject->IsValid())
	{
		OutReason = TEXT("malformed_expected_command_usage_commit");
		return false;
	}

	bool bExpectedMarkUsage = Commit.bMarkUsageOnSuccess;
	if ((*CommitObject)->TryGetBoolField(TEXT("mark_usage_on_success"), bExpectedMarkUsage)
		&& Commit.bMarkUsageOnSuccess != bExpectedMarkUsage)
	{
		OutReason = TEXT("expected_command_usage_commit_mark_usage_mismatch");
		return false;
	}

	int32 ExpectedPlayerId = -1;
	if (TryReadIntegerField(*CommitObject, TEXT("player_id"), ExpectedPlayerId) && Commit.PlayerId != ExpectedPlayerId)
	{
		OutReason = TEXT("expected_command_usage_commit_player_id_mismatch");
		return false;
	}

	FString ExpectedUsageKey;
	if ((*CommitObject)->TryGetStringField(TEXT("usage_key"), ExpectedUsageKey)
		&& Commit.UsageKey != ExpectedUsageKey)
	{
		OutReason = TEXT("expected_command_usage_commit_usage_key_mismatch");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ValidateExpectedRLState(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const FWBGameStateData& State,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* RLStateObject = nullptr;
	if (!ExpectedObject.IsValid() || !ExpectedObject->HasField(TEXT("rl_state")))
	{
		OutReason.Reset();
		return true;
	}

	if (!ExpectedObject->TryGetObjectField(TEXT("rl_state"), RLStateObject)
		|| RLStateObject == nullptr
		|| !RLStateObject->IsValid())
	{
		OutReason = TEXT("malformed_expected_rl_state");
		return false;
	}

	FWBExpectedActivationCostPaymentState ExpectedState;
	if (!TryReadRequiredIntegerField(
			*RLStateObject,
			TEXT("source_unit_id"),
			TEXT("malformed_expected_rl_state"),
			ExpectedState.SourceUnitId,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*RLStateObject,
			TEXT("rl_total"),
			TEXT("malformed_expected_rl_state"),
			ExpectedState.ExpectedRLTotal,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*RLStateObject,
			TEXT("rl_used"),
			TEXT("malformed_expected_rl_state"),
			ExpectedState.ExpectedRLUsed,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*RLStateObject,
			TEXT("available_rl"),
			TEXT("malformed_expected_rl_state"),
			ExpectedState.ExpectedAvailableRL,
			OutReason))
	{
		return false;
	}

	return FWBCardActivationCostPaymentVerifier::VerifyUnitRLState(State, ExpectedState, OutReason);
}

bool ValidateExpectedCostPaidTrace(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const TArray<FWBTraceEvent>& TraceEvents,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* CostPaidTraceObject = nullptr;
	if (!ExpectedObject.IsValid() || !ExpectedObject->HasField(TEXT("cost_paid_trace")))
	{
		OutReason.Reset();
		return true;
	}

	if (!ExpectedObject->TryGetObjectField(TEXT("cost_paid_trace"), CostPaidTraceObject)
		|| CostPaidTraceObject == nullptr
		|| !CostPaidTraceObject->IsValid())
	{
		OutReason = TEXT("malformed_expected_cost_paid_trace");
		return false;
	}

	int32 ExpectedPlayerId = -1;
	int32 ExpectedSourceUnitId = -1;
	int32 ExpectedCostAmount = -1;
	int32 ExpectedPreviousRLUsed = -1;
	int32 ExpectedNewRLUsed = -1;
	int32 ExpectedAvailableRLBefore = -1;
	int32 ExpectedAvailableRLAfter = -1;
	if (!TryReadRequiredIntegerField(
			*CostPaidTraceObject,
			TEXT("player_id"),
			TEXT("malformed_expected_cost_paid_trace"),
			ExpectedPlayerId,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*CostPaidTraceObject,
			TEXT("source_unit_id"),
			TEXT("malformed_expected_cost_paid_trace"),
			ExpectedSourceUnitId,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*CostPaidTraceObject,
			TEXT("cost_amount"),
			TEXT("malformed_expected_cost_paid_trace"),
			ExpectedCostAmount,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*CostPaidTraceObject,
			TEXT("previous_rl_used"),
			TEXT("malformed_expected_cost_paid_trace"),
			ExpectedPreviousRLUsed,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*CostPaidTraceObject,
			TEXT("new_rl_used"),
			TEXT("malformed_expected_cost_paid_trace"),
			ExpectedNewRLUsed,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*CostPaidTraceObject,
			TEXT("available_rl_before"),
			TEXT("malformed_expected_cost_paid_trace"),
			ExpectedAvailableRLBefore,
			OutReason)
		|| !TryReadRequiredIntegerField(
			*CostPaidTraceObject,
			TEXT("available_rl_after"),
			TEXT("malformed_expected_cost_paid_trace"),
			ExpectedAvailableRLAfter,
			OutReason))
	{
		return false;
	}

	return FWBCardActivationCostPaymentVerifier::VerifyCostPaidTrace(
		TraceEvents,
		ExpectedPlayerId,
		ExpectedSourceUnitId,
		ExpectedCostAmount,
		ExpectedPreviousRLUsed,
		ExpectedNewRLUsed,
		ExpectedAvailableRLBefore,
		ExpectedAvailableRLAfter,
		OutReason);
}

bool ValidateForbiddenTraceSubstrings(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const TArray<FWBTraceEvent>& TraceEvents,
	FString& OutReason)
{
	if (!ExpectedObject.IsValid() || !ExpectedObject->HasField(TEXT("forbidden_trace_substrings")))
	{
		OutReason.Reset();
		return true;
	}

	TArray<FString> ForbiddenSubstrings;
	if (!ReadOptionalStringArrayField(
		ExpectedObject,
		TEXT("forbidden_trace_substrings"),
		TEXT("expected.forbidden_trace_substrings"),
		ForbiddenSubstrings,
		OutReason))
	{
		return false;
	}

	return FWBCardActivationCostPaymentVerifier::TraceJsonExcludesForbiddenSubstrings(
		TraceEvents,
		ForbiddenSubstrings,
		OutReason);
}

bool ValidateCardActivationCandidateFixtureExpectations(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBCardActivationCandidateGenerationResult& GenerationResult,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetOptionalExpectedObject(Fixture, ExpectedObject))
	{
		OutReason.Reset();
		return true;
	}

	if (!ValidateOptionalIntegerExpectation(
		ExpectedObject,
		TEXT("candidate_count"),
		GenerationResult.Candidates.Num(),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("candidate_ids"),
		TEXT("expected.candidate_ids"),
		ActivationCandidateIdsFromResult(GenerationResult),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("public_labels"),
		TEXT("expected.public_labels"),
		ActivationCandidateLabelsFromResult(GenerationResult),
		OutReason))
	{
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ValidateCardActivationLegalActionFixtureExpectations(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBCardActivationLegalActionGenerationResult& GenerationResult,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	if (!ValidateOptionalIntegerExpectation(
		ExpectedObject,
		TEXT("activation_action_count"),
		GenerationResult.ActionSet.Actions.Num(),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("activation_action_ids"),
		TEXT("expected.activation_action_ids"),
		ActivationActionIdsFromSet(GenerationResult.ActionSet),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("public_labels"),
		TEXT("expected.public_labels"),
		ActivationActionLabelsFromSet(GenerationResult.ActionSet),
		OutReason))
	{
		return false;
	}

	if (ExpectedObject.IsValid() && ExpectedObject->HasField(TEXT("cost_payment_commit")))
	{
		if (GenerationResult.ActionSet.Actions.Num() <= 0)
		{
			OutReason = TEXT("expected_cost_payment_commit_missing_action");
			return false;
		}

		if (!ValidateExpectedCostPaymentCommit(
			ExpectedObject,
			GenerationResult.ActionSet.Actions[0].Command.CostPaymentCommit,
			OutReason))
		{
			return false;
		}
	}

	if (ExpectedObject.IsValid() && ExpectedObject->HasField(TEXT("command_usage_commit")))
	{
		if (GenerationResult.ActionSet.Actions.Num() <= 0)
		{
			OutReason = TEXT("expected_command_usage_commit_missing_action");
			return false;
		}

		if (!ValidateExpectedUsageCommit(
			ExpectedObject,
			GenerationResult.ActionSet.Actions[0].Command.UsageCommit,
			OutReason))
		{
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ValidateCardActivationPresentationFixtureExpectations(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBCardActivationLegalActionPresentationSnapshot& Snapshot,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	if (!ValidateOptionalIntegerExpectation(
		ExpectedObject,
		TEXT("presentation_entry_count"),
		Snapshot.Entries.Num(),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("activation_action_ids"),
		TEXT("expected.activation_action_ids"),
		ActivationPresentationActionIdsFromSnapshot(Snapshot),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("public_labels"),
		TEXT("expected.public_labels"),
		ActivationPresentationLabelsFromSnapshot(Snapshot),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("source_public_card_ids"),
		TEXT("expected.source_public_card_ids"),
		ActivationPresentationSourcePublicCardIdsFromSnapshot(Snapshot),
		OutReason))
	{
		return false;
	}

	if (!ValidateOptionalStringArrayExpectation(
		ExpectedObject,
		TEXT("target_public_card_ids"),
		TEXT("expected.target_public_card_ids"),
		ActivationPresentationTargetPublicCardIdsFromSnapshot(Snapshot),
		OutReason))
	{
		return false;
	}

	if (ExpectedObject->HasField(TEXT("forbidden_substrings")))
	{
		TArray<FString> ForbiddenSubstrings;
		if (!ReadOptionalStringArrayField(
			ExpectedObject,
			TEXT("forbidden_substrings"),
			TEXT("expected.forbidden_substrings"),
			ForbiddenSubstrings,
			OutReason))
		{
			return false;
		}

		const FString PublicText = ActivationPresentationPublicTextFromSnapshot(Snapshot);
		for (const FString& Forbidden : ForbiddenSubstrings)
		{
			if (PublicText.Contains(Forbidden, ESearchCase::IgnoreCase))
			{
				OutReason = FString::Printf(TEXT("presentation_contains_forbidden_substring:%s"), *Forbidden);
				return false;
			}
		}
	}

	if (ExpectedObject->HasField(TEXT("lookup_activation_action_id")))
	{
		FString LookupActivationActionId;
		if (!ExpectedObject->TryGetStringField(TEXT("lookup_activation_action_id"), LookupActivationActionId))
		{
			OutReason = TEXT("malformed_expected_lookup_activation_action_id");
			return false;
		}

		bool bExpectedLookupFound = false;
		if (!ExpectedObject->TryGetBoolField(TEXT("lookup_found"), bExpectedLookupFound))
		{
			OutReason = TEXT("malformed_expected_lookup_found");
			return false;
		}

		FWBCardActivationLegalActionPresentationEntry Entry;
		const bool bActualLookupFound =
			WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(
				Snapshot,
				LookupActivationActionId,
				Entry);
		if (bActualLookupFound != bExpectedLookupFound)
		{
			OutReason = TEXT("expected_lookup_found_mismatch");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool TryReadStringFieldFromObject(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FString& OutValue,
	FString& OutReason)
{
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return false;
	}

	if (!Object->TryGetStringField(FieldName, OutValue) || OutValue.IsEmpty())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseSelectedActivationActionId(
	const TSharedPtr<FJsonObject>& Fixture,
	const TSharedPtr<FJsonObject>& OperationObject,
	FString& OutSelectedActivationActionId,
	FString& OutReason)
{
	if (TryReadStringFieldFromObject(
		OperationObject,
		TEXT("selected_activation_action_id"),
		OutSelectedActivationActionId,
		OutReason))
	{
		return true;
	}

	if (!OutReason.IsEmpty())
	{
		return false;
	}

	if (TryReadStringFieldFromObject(
		Fixture,
		TEXT("selected_activation_action_id"),
		OutSelectedActivationActionId,
		OutReason))
	{
		return true;
	}

	if (!OutReason.IsEmpty())
	{
		return false;
	}

	OutReason = TEXT("missing_selected_activation_action_id");
	return false;
}

bool ValidateSelectedActivationActionIdExpectation(
	const TSharedPtr<FJsonObject>& Fixture,
	const FString& SelectedActivationActionId,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetOptionalExpectedObject(Fixture, ExpectedObject)
		|| !ExpectedObject.IsValid()
		|| !ExpectedObject->HasField(TEXT("selected_activation_action_id")))
	{
		OutReason.Reset();
		return true;
	}

	FString ExpectedSelectedActivationActionId;
	if (!ExpectedObject->TryGetStringField(TEXT("selected_activation_action_id"), ExpectedSelectedActivationActionId)
		|| ExpectedSelectedActivationActionId.IsEmpty())
	{
		OutReason = TEXT("malformed_expected_selected_activation_action_id");
		return false;
	}

	if (ExpectedSelectedActivationActionId != SelectedActivationActionId)
	{
		OutReason = TEXT("expected_selected_activation_action_id_mismatch");
		return false;
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

EWBGamePhase ReadPhaseOrDefault(const TSharedPtr<FJsonObject>& Object, const EWBGamePhase DefaultPhase)
{
	FString PhaseString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("phase"), PhaseString))
	{
		return DefaultPhase;
	}

	return PhaseString == TEXT("response") ? EWBGamePhase::Response : EWBGamePhase::NormalTurn;
}

bool TryGetExpectedObject(const TSharedPtr<FJsonObject>& Fixture, TSharedPtr<FJsonObject>& OutExpectedObject, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expected"), ExpectedObject)
		&& ExpectedObject != nullptr
		&& ExpectedObject->IsValid())
	{
		OutExpectedObject = *ExpectedObject;
		OutReason.Reset();
		return true;
	}

	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		&& ExpectObject != nullptr
		&& ExpectObject->IsValid())
	{
		OutExpectedObject = *ExpectObject;
		OutReason.Reset();
		return true;
	}

	OutReason = TEXT("missing_expected");
	return false;
}

bool TryGetOperationKindAndObject(
	const TSharedPtr<FJsonObject>& Fixture,
	FString& OutOperationKind,
	TSharedPtr<FJsonObject>& OutOperationObject,
	FString& OutReason)
{
	OutOperationKind.Reset();
	OutOperationObject.Reset();

	if (!Fixture.IsValid())
	{
		OutReason = TEXT("missing_fixture");
		return false;
	}

	const TSharedPtr<FJsonValue>* OperationValue = Fixture->Values.Find(TEXT("operation"));
	if (OperationValue == nullptr || !OperationValue->IsValid())
	{
		OutReason = TEXT("missing_operation");
		return false;
	}

	if ((*OperationValue)->Type == EJson::String)
	{
		OutOperationKind = (*OperationValue)->AsString();
		OutOperationObject = Fixture;
		OutReason.Reset();
		return true;
	}

	if ((*OperationValue)->Type == EJson::Object)
	{
		OutOperationObject = (*OperationValue)->AsObject();
		if (!OutOperationObject.IsValid() || !OutOperationObject->TryGetStringField(TEXT("kind"), OutOperationKind))
		{
			OutReason = TEXT("missing_operation_kind");
			return false;
		}

		OutReason.Reset();
		return true;
	}

	OutReason = TEXT("malformed_operation");
	return false;
}

FWBApplyActionResult MakeFixtureFailure(const FString& Reason)
{
	FWBApplyActionResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

bool ParseFixtureTile(const TSharedPtr<FJsonObject>& Object, FWBTile& OutTile)
{
	return TryReadIntegerField(Object, TEXT("x"), OutTile.X)
		&& TryReadIntegerField(Object, TEXT("y"), OutTile.Y);
}

bool ParseOptionalFixtureTileField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBTile& OutTile,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* TileObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, TileObject)
		|| TileObject == nullptr
		|| !TileObject->IsValid()
		|| !ParseFixtureTile(*TileObject, OutTile))
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseOptionalWallEdgeField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBWallEdge& OutWallEdge,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* WallObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, WallObject)
		|| WallObject == nullptr
		|| !WallObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	if (!ParseOptionalFixtureTileField(*WallObject, TEXT("a"), OutWallEdge.A, OutReason)
		|| !ParseOptionalFixtureTileField(*WallObject, TEXT("b"), OutWallEdge.B, OutReason))
	{
		return false;
	}

	if (OutWallEdge.A.X == -1 || OutWallEdge.A.Y == -1 || OutWallEdge.B.X == -1 || OutWallEdge.B.Y == -1)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseActionFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FWBAction& OutAction,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ActionObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("action"), ActionObject)
		|| ActionObject == nullptr
		|| !ActionObject->IsValid())
	{
		OutReason = TEXT("missing_action");
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

bool ParseRuntimeRollSource(
	const TSharedPtr<FJsonObject>& RuntimeContextObject,
	TUniquePtr<FWBFixedMPRollSource>& OutFixedRollSource,
	TUniquePtr<FWBQueuedMPRollSource>& OutQueuedRollSource,
	IWBMPRollSource*& OutRollSource,
	FString& OutReason)
{
	OutFixedRollSource.Reset();
	OutQueuedRollSource.Reset();
	OutRollSource = nullptr;

	const bool bHasFixedRoll = RuntimeContextObject.IsValid()
		&& RuntimeContextObject->HasTypedField<EJson::Number>(TEXT("fixed_mp_roll"));
	const bool bHasQueuedRolls = RuntimeContextObject.IsValid()
		&& RuntimeContextObject->HasTypedField<EJson::Array>(TEXT("queued_mp_rolls"));

	if (bHasFixedRoll && bHasQueuedRolls)
	{
		OutReason = TEXT("multiple_mp_roll_sources");
		return false;
	}

	if (bHasFixedRoll)
	{
		int32 FixedRoll = 0;
		if (!TryReadIntegerField(RuntimeContextObject, TEXT("fixed_mp_roll"), FixedRoll))
		{
			OutReason = TEXT("malformed_fixed_mp_roll");
			return false;
		}

		OutFixedRollSource = MakeUnique<FWBFixedMPRollSource>(FixedRoll);
		OutRollSource = OutFixedRollSource.Get();
		OutReason.Reset();
		return true;
	}

	if (bHasQueuedRolls)
	{
		const TArray<TSharedPtr<FJsonValue>>* QueuedRollValues = nullptr;
		if (!RuntimeContextObject->TryGetArrayField(TEXT("queued_mp_rolls"), QueuedRollValues)
			|| QueuedRollValues == nullptr)
		{
			OutReason = TEXT("malformed_queued_mp_rolls");
			return false;
		}

		OutQueuedRollSource = MakeUnique<FWBQueuedMPRollSource>();
		for (int32 Index = 0; Index < QueuedRollValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& RollValue = (*QueuedRollValues)[Index];
			if (!RollValue.IsValid() || RollValue->Type != EJson::Number)
			{
				OutReason = FString::Printf(TEXT("malformed_queued_mp_rolls[%d]"), Index);
				return false;
			}

			OutQueuedRollSource->EnqueueRoll(static_cast<int32>(RollValue->AsNumber()));
		}

		OutRollSource = OutQueuedRollSource.Get();
		OutReason.Reset();
		return true;
	}

	OutReason.Reset();
	return true;
}

bool ParseRuntimeSelectedActionFixtureInputs(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FWBAction& OutSelectedAction,
	FWBRuntimeTurnResolutionContext& OutRuntimeContext,
	TUniquePtr<FWBFixedMPRollSource>& OutFixedRollSource,
	TUniquePtr<FWBQueuedMPRollSource>& OutQueuedRollSource,
	FString& OutReason)
{
	if (!ParseActionFromFixture(Fixture, State, OutSelectedAction, OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* RuntimeContextObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("runtime_context"), RuntimeContextObject)
		|| RuntimeContextObject == nullptr
		|| !RuntimeContextObject->IsValid())
	{
		OutReason = TEXT("missing_runtime_context");
		return false;
	}

	if (!(*RuntimeContextObject)->TryGetBoolField(
		TEXT("resolve_end_turn_as_full_transition"),
		OutRuntimeContext.bResolveEndTurnAsFullTransition))
	{
		OutReason = TEXT("missing_runtime_context_resolve_end_turn_as_full_transition");
		return false;
	}

	IWBMPRollSource* RollSource = nullptr;
	if (!ParseRuntimeRollSource(*RuntimeContextObject, OutFixedRollSource, OutQueuedRollSource, RollSource, OutReason))
	{
		return false;
	}

	OutRuntimeContext.MPRollSource = RollSource;
	OutReason.Reset();
	return true;
}

bool ExpectPublicPlayerSummary(
	const TSharedPtr<FJsonObject>& PlayerSummaryObject,
	const FWBPublicTurnSummary& Summary,
	FString& OutReason)
{
	int32 PlayerId = -1;
	if (!PlayerSummaryObject.IsValid() || !TryReadIntegerField(PlayerSummaryObject, TEXT("player_id"), PlayerId))
	{
		OutReason = TEXT("malformed_final_public_turn_summary_player");
		return false;
	}

	const FWBPublicPlayerTurnSummary* ActualPlayerSummary = nullptr;
	for (const FWBPublicPlayerTurnSummary& Candidate : Summary.Players)
	{
		if (Candidate.PlayerId == PlayerId)
		{
			ActualPlayerSummary = &Candidate;
			break;
		}
	}

	if (ActualPlayerSummary == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_final_public_turn_summary_player_%d"), PlayerId);
		return false;
	}

	if (ActualPlayerSummary->RemainingMP != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("remaining_mp"), ActualPlayerSummary->RemainingMP)
		|| ActualPlayerSummary->LastMPRoll != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("last_mp_roll"), ActualPlayerSummary->LastMPRoll)
		|| ActualPlayerSummary->WallsLeft != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("walls_left"), ActualPlayerSummary->WallsLeft)
		|| ActualPlayerSummary->WallRemovalsLeft != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("wall_removals_left"), ActualPlayerSummary->WallRemovalsLeft))
	{
		OutReason = FString::Printf(TEXT("final_public_turn_summary_player_%d_mismatch"), PlayerId);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ExpectFinalPublicTurnSummary(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const FWBPublicTurnSummary& Summary,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* SummaryObject = nullptr;
	if (!ExpectedObject.IsValid()
		|| !ExpectedObject->TryGetObjectField(TEXT("final_public_turn_summary"), SummaryObject)
		|| SummaryObject == nullptr
		|| !SummaryObject->IsValid())
	{
		OutReason = TEXT("missing_expected_final_public_turn_summary");
		return false;
	}

	if (Summary.CurrentPlayerId != ReadIntegerFieldOrDefault(*SummaryObject, TEXT("current_player_id"), Summary.CurrentPlayerId)
		|| Summary.PriorityPlayerId != ReadIntegerFieldOrDefault(*SummaryObject, TEXT("priority_player_id"), Summary.PriorityPlayerId)
		|| Summary.TurnNumber != ReadIntegerFieldOrDefault(*SummaryObject, TEXT("turn_number"), Summary.TurnNumber))
	{
		OutReason = TEXT("final_public_turn_summary_turn_state_mismatch");
		return false;
	}

	bool bExpectedGameOver = Summary.bGameOver;
	(*SummaryObject)->TryGetBoolField(TEXT("game_over"), bExpectedGameOver);
	if (Summary.bGameOver != bExpectedGameOver)
	{
		OutReason = TEXT("final_public_turn_summary_game_over_mismatch");
		return false;
	}

	const int32 ExpectedWinnerPlayerId = ReadIntegerFieldOrDefault(*SummaryObject, TEXT("winner_player_id"), Summary.WinnerPlayerId);
	if (Summary.WinnerPlayerId != ExpectedWinnerPlayerId)
	{
		OutReason = TEXT("final_public_turn_summary_winner_mismatch");
		return false;
	}

	FString ExpectedPhase;
	if ((*SummaryObject)->TryGetStringField(TEXT("phase"), ExpectedPhase) && Summary.Phase.ToString() != ExpectedPhase)
	{
		OutReason = TEXT("final_public_turn_summary_phase_mismatch");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if ((*SummaryObject)->TryGetArrayField(TEXT("players"), Players) && Players != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& PlayerValue : *Players)
		{
			if (!ExpectPublicPlayerSummary(PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr, Summary, OutReason))
			{
				return false;
			}
		}
	}

	OutReason.Reset();
	return true;
}

void ApplyFixtureStatuses(FWBGameStateData& State, const TSharedPtr<FJsonObject>& InitialStateObject)
{
	const TSharedPtr<FJsonObject>* StatusesByUnit = nullptr;
	if (!InitialStateObject.IsValid()
		|| !InitialStateObject->TryGetObjectField(TEXT("statuses_by_unit"), StatusesByUnit)
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
			FString StatusId = StatusPair.Key;
			int32 TurnsRemaining = 0;
			const TSharedPtr<FJsonObject> StatusObject = StatusPair.Value.IsValid()
				? StatusPair.Value->AsObject()
				: nullptr;
			if (StatusObject.IsValid())
			{
				StatusObject->TryGetStringField(TEXT("status_id"), StatusId);
				TurnsRemaining = ReadIntegerFieldOrDefault(
					StatusObject,
					TEXT("turns_remaining"),
					ReadIntegerFieldOrDefault(StatusObject, TEXT("ticks_remaining"), 0));
			}
			else if (StatusPair.Value.IsValid() && StatusPair.Value->Type == EJson::Number)
			{
				TurnsRemaining = static_cast<int32>(StatusPair.Value->AsNumber());
			}

			Unit->AddStatus(FName(*StatusId), TurnsRemaining);
		}
	}
}

bool ApplyFixturePlayers(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("players"), Players) || Players == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& PlayerValue : *Players)
	{
		const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
		if (!PlayerObject.IsValid())
		{
			OutReason = TEXT("malformed_initial_state_player");
			return false;
		}

		FWBPlayerStateData Player;
		if (!TryReadIntegerField(PlayerObject, TEXT("player_id"), Player.PlayerId))
		{
			OutReason = TEXT("missing_initial_state_player_id");
			return false;
		}

		Player.RemainingMP = ReadIntegerFieldOrDefault(PlayerObject, TEXT("remaining_mp"), 0);
		Player.LastMPRoll = ReadIntegerFieldOrDefault(PlayerObject, TEXT("last_mp_roll"), 0);
		Player.HeroUnitId = ReadIntegerFieldOrDefault(PlayerObject, TEXT("hero_unit_id"), Player.HeroUnitId);
		Player.WallsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("walls_left"), 0);
		Player.WallRemovalsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("wall_removals_left"), 0);
		if (!ReadOptionalStringArrayField(
				PlayerObject,
				TEXT("deck"),
				FString::Printf(TEXT("initial_state.players[%d].deck"), Player.PlayerId),
				Player.Deck,
				OutReason)
			|| !ReadOptionalStringArrayField(
				PlayerObject,
				TEXT("hand"),
				FString::Printf(TEXT("initial_state.players[%d].hand"), Player.PlayerId),
				Player.Hand,
				OutReason)
			|| !ReadOptionalStringArrayField(
				PlayerObject,
				TEXT("discard"),
				FString::Printf(TEXT("initial_state.players[%d].discard"), Player.PlayerId),
				Player.Discard,
				OutReason))
		{
			return false;
		}
		OutState.Players.Add(Player);
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixtureUnits(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
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
			OutReason = TEXT("malformed_initial_state_unit");
			return false;
		}

		FWBUnitState Unit;
		if (!TryReadIntegerField(UnitObject, TEXT("id"), Unit.UnitId)
			|| !TryReadIntegerField(UnitObject, TEXT("owner_id"), Unit.OwnerId)
			|| !TryReadIntegerField(UnitObject, TEXT("x"), Unit.X)
			|| !TryReadIntegerField(UnitObject, TEXT("y"), Unit.Y))
		{
			OutReason = TEXT("missing_initial_state_unit_required_field");
			return false;
		}

		UnitObject->TryGetStringField(TEXT("card_id"), Unit.CardId);
		Unit.HP = ReadIntegerFieldOrDefault(UnitObject, TEXT("hp"), Unit.HP);
		Unit.MaxHP = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_hp"), Unit.MaxHP);
		Unit.CurrentArmor = ReadIntegerFieldOrDefault(UnitObject, TEXT("current_armor"), Unit.CurrentArmor);
		Unit.MaxArmor = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_armor"), Unit.MaxArmor);
		Unit.ATK = ReadIntegerFieldOrDefault(UnitObject, TEXT("atk"), Unit.ATK);
		Unit.AR = ReadIntegerFieldOrDefault(UnitObject, TEXT("ar"), Unit.AR);
		Unit.RLTotal = ReadIntegerFieldOrDefault(UnitObject, TEXT("rl_total"), Unit.RLTotal);
		Unit.RLUsed = ReadIntegerFieldOrDefault(UnitObject, TEXT("rl_used"), Unit.RLUsed);
		Unit.AttacksLeft = ReadIntegerFieldOrDefault(UnitObject, TEXT("attacks_left"), Unit.AttacksLeft);
		Unit.MaxAttacksPerTurn = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_attacks_per_turn"), Unit.MaxAttacksPerTurn);
		Unit.MPRemaining = ReadIntegerFieldOrDefault(UnitObject, TEXT("mp_remaining"), 0);
		Unit.bDefeated = ReadBoolFieldOrDefault(UnitObject, TEXT("defeated"), false);
		Unit.bRemovedFromBoard = ReadBoolFieldOrDefault(UnitObject, TEXT("removed_from_board"), false);

		if (!OutState.AddUnitForTest(Unit))
		{
			OutReason = FString::Printf(TEXT("could_not_add_initial_state_unit_%d"), Unit.UnitId);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixtureWalls(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("walls"), Walls) || Walls == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& WallValue : *Walls)
	{
		const TSharedPtr<FJsonObject> WallObject = WallValue.IsValid() ? WallValue->AsObject() : nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Between = nullptr;
		if (!WallObject.IsValid()
			|| !WallObject->TryGetArrayField(TEXT("between"), Between)
			|| Between == nullptr
			|| Between->Num() != 2)
		{
			OutReason = TEXT("malformed_initial_state_wall");
			return false;
		}

		FWBTile A;
		FWBTile B;
		if (!ParseFixtureTile((*Between)[0]->AsObject(), A) || !ParseFixtureTile((*Between)[1]->AsObject(), B))
		{
			OutReason = TEXT("malformed_initial_state_wall_tile");
			return false;
		}

		OutState.AddWallForTest(FWBWallEdge(A, B));
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixtureTerrain(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	FString DefaultTerrainId;
	if (StateObject->TryGetStringField(TEXT("default_terrain_id"), DefaultTerrainId) && !DefaultTerrainId.IsEmpty())
	{
		OutState.DefaultTerrainId = FName(*DefaultTerrainId);
	}

	const TArray<TSharedPtr<FJsonValue>>* TerrainTiles = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("terrain_tiles"), TerrainTiles) || TerrainTiles == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& TerrainValue : *TerrainTiles)
	{
		const TSharedPtr<FJsonObject> TerrainObject = TerrainValue.IsValid() ? TerrainValue->AsObject() : nullptr;
		if (!TerrainObject.IsValid())
		{
			OutReason = TEXT("malformed_initial_state_terrain_tile");
			return false;
		}

		FWBTile Tile;
		FString TerrainId;
		if (!ParseFixtureTile(TerrainObject, Tile) || !TerrainObject->TryGetStringField(TEXT("terrain_id"), TerrainId) || TerrainId.IsEmpty())
		{
			OutReason = TEXT("malformed_initial_state_terrain_tile");
			return false;
		}

		OutState.SetTerrainForTest(Tile, FName(*TerrainId));
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixturePendingAttack(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* PendingAttackObject = nullptr;
	if (!StateObject->TryGetObjectField(TEXT("pending_attack"), PendingAttackObject)
		|| PendingAttackObject == nullptr
		|| !PendingAttackObject->IsValid())
	{
		return true;
	}

	FWBPendingAttackState PendingAttack;
	PendingAttack.bActive = ReadBoolFieldOrDefault(*PendingAttackObject, TEXT("active"), false);
	if (!PendingAttack.bActive)
	{
		OutState.ClearPendingAttack();
		OutReason.Reset();
		return true;
	}

	const TSharedPtr<FJsonObject>* AttackerTileObject = nullptr;
	const TSharedPtr<FJsonObject>* DefenderTileObject = nullptr;
	if (!TryReadIntegerField(*PendingAttackObject, TEXT("attacker_unit_id"), PendingAttack.AttackerUnitId)
		|| !TryReadIntegerField(*PendingAttackObject, TEXT("defender_unit_id"), PendingAttack.DefenderUnitId)
		|| !TryReadIntegerField(*PendingAttackObject, TEXT("attacking_player_id"), PendingAttack.AttackingPlayerId)
		|| !(*PendingAttackObject)->TryGetObjectField(TEXT("attacker_tile"), AttackerTileObject)
		|| AttackerTileObject == nullptr
		|| !AttackerTileObject->IsValid()
		|| !ParseFixtureTile(*AttackerTileObject, PendingAttack.AttackerTile)
		|| !(*PendingAttackObject)->TryGetObjectField(TEXT("defender_tile"), DefenderTileObject)
		|| DefenderTileObject == nullptr
		|| !DefenderTileObject->IsValid()
		|| !ParseFixtureTile(*DefenderTileObject, PendingAttack.DefenderTile))
	{
		OutReason = TEXT("malformed_initial_state_pending_attack");
		return false;
	}

	PendingAttack.DeclarationActionId = TEXT("");
	(*PendingAttackObject)->TryGetStringField(TEXT("declaration_action_id"), PendingAttack.DeclarationActionId);
	OutState.SetPendingAttackForTest(PendingAttack);
	OutReason.Reset();
	return true;
}

bool ApplyFixtureActivationUsageKeys(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* UsageObject = nullptr;
	if (!StateObject->TryGetObjectField(TEXT("activation_usage_keys_this_turn"), UsageObject)
		|| UsageObject == nullptr
		|| !UsageObject->IsValid())
	{
		return true;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& PlayerUsagePair : (*UsageObject)->Values)
	{
		const int32 PlayerId = FCString::Atoi(*PlayerUsagePair.Key);
		const TSharedPtr<FJsonValue>& UsageValue = PlayerUsagePair.Value;
		if (!UsageValue.IsValid() || UsageValue->Type != EJson::Array)
		{
			OutReason = FString::Printf(TEXT("malformed_initial_state_activation_usage_keys_%s"), *PlayerUsagePair.Key);
			return false;
		}

		TArray<FString> UsageKeys;
		if (!ReadStringArrayValues(
			UsageValue->AsArray(),
			FString::Printf(TEXT("initial_state.activation_usage_keys_this_turn.%s"), *PlayerUsagePair.Key),
			UsageKeys,
			OutReason))
		{
			return false;
		}

		for (const FString& UsageKey : UsageKeys)
		{
			OutState.MarkActivationUsageKeyForTest(PlayerId, UsageKey);
		}
	}

	OutReason.Reset();
	return true;
}

bool ReadExpectedFinalIntegerField(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const TCHAR* FieldName,
	const TCHAR* FinalStateFieldName,
	int32& OutValue)
{
	if (TryReadIntegerField(ExpectedObject, FieldName, OutValue))
	{
		return true;
	}

	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (ExpectedObject.IsValid()
		&& ExpectedObject->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		&& FinalStateObject != nullptr
		&& FinalStateObject->IsValid())
	{
		return TryReadIntegerField(*FinalStateObject, FinalStateFieldName, OutValue);
	}

	return false;
}

bool ReadExpectedFinalPhase(const TSharedPtr<FJsonObject>& ExpectedObject, EWBGamePhase& OutPhase)
{
	FString PhaseString;
	if (ExpectedObject.IsValid() && ExpectedObject->TryGetStringField(TEXT("final_phase"), PhaseString))
	{
		OutPhase = PhaseString == TEXT("response") ? EWBGamePhase::Response : EWBGamePhase::NormalTurn;
		return true;
	}

	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (ExpectedObject.IsValid()
		&& ExpectedObject->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		&& FinalStateObject != nullptr
		&& FinalStateObject->IsValid()
		&& (*FinalStateObject)->TryGetStringField(TEXT("phase"), PhaseString))
	{
		OutPhase = PhaseString == TEXT("response") ? EWBGamePhase::Response : EWBGamePhase::NormalTurn;
		return true;
	}

	return false;
}

bool TraceEventMatchesLabel(const FWBTraceEvent& Event, const FString& ExpectedLabel)
{
	FString NormalizedLabel = ExpectedLabel;
	NormalizedLabel.ReplaceInline(TEXT(" "), TEXT(":"));

	FString ExpectedKind = NormalizedLabel;
	FString ExpectedStatusId;
	if (NormalizedLabel.Split(TEXT(":"), &ExpectedKind, &ExpectedStatusId))
	{
		return Event.Kind.ToString() == ExpectedKind && Event.StatusId.ToString() == ExpectedStatusId;
	}

	return Event.Kind.ToString() == ExpectedKind;
}
}

bool BuildGameStateFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& OutState,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* InitialStateObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("initial_state"), InitialStateObject)
		|| InitialStateObject == nullptr
		|| !InitialStateObject->IsValid())
	{
		OutReason = TEXT("missing_initial_state");
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
	OutState.Phase = ReadPhaseOrDefault(*InitialStateObject, EWBGamePhase::NormalTurn);
	OutState.bGameOver = ReadBoolFieldOrDefault(*InitialStateObject, TEXT("game_over"), false);
	OutState.WinnerPlayerId = ReadIntegerFieldOrDefault(*InitialStateObject, TEXT("winner_player_id"), -1);

	if (!ApplyFixturePlayers(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureUnits(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureWalls(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureTerrain(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixturePendingAttack(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureActivationUsageKeys(*InitialStateObject, OutState, OutReason))
	{
		return false;
	}

	ApplyFixtureStatuses(OutState, *InitialStateObject);
	OutReason.Reset();
	return true;
}

bool ParseTurnCommandFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBTurnCommand& OutCommand,
	FString& OutReason)
{
	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		return false;
	}

	if (OperationKind != TEXT("apply_turn_command"))
	{
		OutReason = TEXT("fixture_operation_is_not_apply_turn_command");
		return false;
	}

	FString Mode;
	if (!OperationObject->TryGetStringField(TEXT("mode"), Mode))
	{
		OutReason = TEXT("missing_turn_command_mode");
		return false;
	}

	if (Mode == TEXT("basic_end_turn"))
	{
		OutCommand.Mode = EWBTurnCommandMode::BasicEndTurn;
	}
	else if (Mode == TEXT("deterministic_full_transition"))
	{
		OutCommand.Mode = EWBTurnCommandMode::DeterministicFullTransition;
	}
	else
	{
		OutReason = FString::Printf(TEXT("unsupported_turn_command_mode:%s"), *Mode);
		return false;
	}

	if (!TryReadIntegerField(OperationObject, TEXT("acting_player_id"), OutCommand.ActingPlayerId))
	{
		OutReason = TEXT("missing_turn_command_acting_player_id");
		return false;
	}

	OutCommand.NextPlayerExplicitMPRoll = ReadIntegerFieldOrDefault(
		OperationObject,
		TEXT("next_player_explicit_mp_roll"),
		0);

	if (OutCommand.Mode == EWBTurnCommandMode::DeterministicFullTransition
		&& !OperationObject->HasTypedField<EJson::Number>(TEXT("next_player_explicit_mp_roll")))
	{
		OutReason = TEXT("missing_turn_command_next_player_explicit_mp_roll");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ApplyTurnCommandFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	FString& OutReason)
{
	FWBTurnCommand Command;
	if (!ParseTurnCommandFromFixture(Fixture, Command, OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	OutResult = WBTurnController::ApplyTurnCommand(State, Command);
	OutReason.Reset();
	return true;
}

bool ApplyRuntimeSelectedActionFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	int32& OutRollSourceRemainingCount,
	FString& OutReason)
{
	OutRollSourceRemainingCount = -1;

	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	if (OperationKind != TEXT("apply_runtime_selected_action"))
	{
		OutReason = TEXT("fixture_operation_is_not_apply_runtime_selected_action");
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	FWBAction SelectedAction;
	FWBRuntimeTurnResolutionContext RuntimeContext;
	TUniquePtr<FWBFixedMPRollSource> FixedRollSource;
	TUniquePtr<FWBQueuedMPRollSource> QueuedRollSource;
	if (!ParseRuntimeSelectedActionFixtureInputs(
		Fixture,
		State,
		SelectedAction,
		RuntimeContext,
		FixedRollSource,
		QueuedRollSource,
		OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	OutResult = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(State, SelectedAction, RuntimeContext);
	if (QueuedRollSource.IsValid())
	{
		OutRollSourceRemainingCount = QueuedRollSource->NumRemainingRolls();
	}

	OutReason.Reset();
	return true;
}

bool ApplyRuntimeSelectedActionWithResultFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBRuntimeSelectedActionResult& OutEnvelope,
	int32& OutRollSourceRemainingCount,
	FString& OutReason)
{
	OutRollSourceRemainingCount = -1;
	OutEnvelope = FWBRuntimeSelectedActionResult();

	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		OutEnvelope.ApplyResult = MakeFixtureFailure(OutReason);
		return false;
	}

	if (OperationKind != TEXT("apply_runtime_selected_action_with_result")
		&& OperationKind != TEXT("apply_runtime_selected_action_with_result_and_serialize"))
	{
		OutReason = TEXT("fixture_operation_is_not_apply_runtime_selected_action_with_result");
		OutEnvelope.ApplyResult = MakeFixtureFailure(OutReason);
		return false;
	}

	FWBAction SelectedAction;
	FWBRuntimeTurnResolutionContext RuntimeContext;
	TUniquePtr<FWBFixedMPRollSource> FixedRollSource;
	TUniquePtr<FWBQueuedMPRollSource> QueuedRollSource;
	if (!ParseRuntimeSelectedActionFixtureInputs(
		Fixture,
		State,
		SelectedAction,
		RuntimeContext,
		FixedRollSource,
		QueuedRollSource,
		OutReason))
	{
		OutEnvelope.ApplyResult = MakeFixtureFailure(OutReason);
		return false;
	}

	OutEnvelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		SelectedAction,
		RuntimeContext);
	if (QueuedRollSource.IsValid())
	{
		OutRollSourceRemainingCount = QueuedRollSource->NumRemainingRolls();
	}

	OutReason.Reset();
	return true;
}

bool ApplyRuntimeSelectedActionWithResultAndSerializeFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBRuntimeSelectedActionResult& OutEnvelope,
	FString& OutSerializedJson,
	int32& OutRollSourceRemainingCount,
	FString& OutReason)
{
	OutSerializedJson.Reset();
	if (!ApplyRuntimeSelectedActionWithResultFixture(
			Fixture,
			State,
			OutEnvelope,
			OutRollSourceRemainingCount,
			OutReason))
	{
		return false;
	}

	if (!WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(OutEnvelope, OutSerializedJson))
	{
		OutReason = TEXT("runtime_result_serialization_failed");
		return false;
	}

	OutReason.Reset();
	return true;
}

EWBDamageKind ReadDamageKindOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString DamageKindString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("damage_kind"), DamageKindString))
	{
		return EWBDamageKind::Unknown;
	}

	if (DamageKindString == TEXT("attack"))
	{
		return EWBDamageKind::Attack;
	}

	if (DamageKindString == TEXT("burn"))
	{
		return EWBDamageKind::Burn;
	}

	if (DamageKindString == TEXT("effect"))
	{
		return EWBDamageKind::Effect;
	}

	return EWBDamageKind::Unknown;
}

EWBArmorEffectOp ReadArmorEffectOperationOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString OperationString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("operation"), OperationString))
	{
		return EWBArmorEffectOp::Unknown;
	}

	if (OperationString == TEXT("add_current_armor"))
	{
		return EWBArmorEffectOp::AddCurrentArmor;
	}

	if (OperationString == TEXT("reduce_current_armor"))
	{
		return EWBArmorEffectOp::ReduceCurrentArmor;
	}

	if (OperationString == TEXT("set_current_armor"))
	{
		return EWBArmorEffectOp::SetCurrentArmor;
	}

	if (OperationString == TEXT("add_max_armor"))
	{
		return EWBArmorEffectOp::AddMaxArmor;
	}

	if (OperationString == TEXT("reduce_max_armor"))
	{
		return EWBArmorEffectOp::ReduceMaxArmor;
	}

	if (OperationString == TEXT("set_max_armor"))
	{
		return EWBArmorEffectOp::SetMaxArmor;
	}

	if (OperationString == TEXT("restore_armor_to_max"))
	{
		return EWBArmorEffectOp::RestoreArmorToMax;
	}

	return EWBArmorEffectOp::Unknown;
}

EWBStatusEffectOp ReadStatusEffectOperationOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString OperationString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("operation"), OperationString))
	{
		return EWBStatusEffectOp::Unknown;
	}

	if (OperationString == TEXT("apply_status"))
	{
		return EWBStatusEffectOp::ApplyStatus;
	}

	if (OperationString == TEXT("remove_status"))
	{
		return EWBStatusEffectOp::RemoveStatus;
	}

	if (OperationString == TEXT("set_status_duration"))
	{
		return EWBStatusEffectOp::SetStatusDuration;
	}

	if (OperationString == TEXT("add_status_duration"))
	{
		return EWBStatusEffectOp::AddStatusDuration;
	}

	if (OperationString == TEXT("reduce_status_duration"))
	{
		return EWBStatusEffectOp::ReduceStatusDuration;
	}

	if (OperationString == TEXT("cleanse_status"))
	{
		return EWBStatusEffectOp::CleanseStatus;
	}

	if (OperationString == TEXT("cleanse_all_statuses"))
	{
		return EWBStatusEffectOp::CleanseAllStatuses;
	}

	return EWBStatusEffectOp::Unknown;
}

bool ParseArmorEffectRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBArmorEffectRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ArmorRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("armor_effect_request"), ArmorRequestObject)
		|| ArmorRequestObject == nullptr
		|| !ArmorRequestObject->IsValid())
	{
		OutReason = TEXT("missing_armor_effect_request");
		return false;
	}

	OutRequest.Operation = ReadArmorEffectOperationOrDefault(*ArmorRequestObject);
	if (!TryReadIntegerField(*ArmorRequestObject, TEXT("target_unit_id"), OutRequest.TargetUnitId)
		|| !TryReadIntegerField(*ArmorRequestObject, TEXT("amount"), OutRequest.Amount))
	{
		OutReason = TEXT("malformed_armor_effect_request");
		return false;
	}

	FString SourceReason;
	if ((*ArmorRequestObject)->TryGetStringField(TEXT("source_reason"), SourceReason))
	{
		OutRequest.SourceReason = FName(*SourceReason);
	}

	OutReason.Reset();
	return true;
}

bool ParseArmorEffectRequestObject(
	const TSharedPtr<FJsonObject>& ArmorRequestObject,
	FWBArmorEffectRequest& OutRequest,
	FString& OutReason)
{
	if (!ArmorRequestObject.IsValid())
	{
		OutReason = TEXT("missing_armor_effect_payload");
		return false;
	}

	OutRequest.Operation = ReadArmorEffectOperationOrDefault(ArmorRequestObject);
	OutRequest.TargetUnitId = ReadIntegerFieldOrDefault(ArmorRequestObject, TEXT("target_unit_id"), -1);
	if (!TryReadIntegerField(ArmorRequestObject, TEXT("amount"), OutRequest.Amount))
	{
		OutReason = TEXT("malformed_armor_effect_payload");
		return false;
	}

	FString SourceReason;
	if (ArmorRequestObject->TryGetStringField(TEXT("source_reason"), SourceReason))
	{
		OutRequest.SourceReason = FName(*SourceReason);
	}

	OutReason.Reset();
	return true;
}

bool ParseStatusEffectRequestObject(
	const TSharedPtr<FJsonObject>& StatusRequestObject,
	FWBStatusEffectRequest& OutRequest,
	FString& OutReason)
{
	if (!StatusRequestObject.IsValid())
	{
		OutReason = TEXT("missing_status_effect_payload");
		return false;
	}

	OutRequest.Operation = ReadStatusEffectOperationOrDefault(StatusRequestObject);
	OutRequest.TargetUnitId = ReadIntegerFieldOrDefault(StatusRequestObject, TEXT("target_unit_id"), -1);
	OutRequest.Duration = ReadIntegerFieldOrDefault(StatusRequestObject, TEXT("duration"), 0);

	FString StatusId;
	if (StatusRequestObject->TryGetStringField(TEXT("status_id"), StatusId))
	{
		OutRequest.StatusId = FName(*StatusId);
	}

	FString SourceReason;
	if (StatusRequestObject->TryGetStringField(TEXT("source_reason"), SourceReason))
	{
		OutRequest.SourceReason = FName(*SourceReason);
	}

	OutReason.Reset();
	return true;
}

bool ParseStatusEffectRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBStatusEffectRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* StatusRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("status_effect_request"), StatusRequestObject)
		|| StatusRequestObject == nullptr
		|| !StatusRequestObject->IsValid())
	{
		OutReason = TEXT("missing_status_effect_request");
		return false;
	}

	if (!ParseStatusEffectRequestObject(*StatusRequestObject, OutRequest, OutReason))
	{
		return false;
	}

	if (OutRequest.TargetUnitId == -1)
	{
		OutReason = TEXT("malformed_status_effect_request");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseDamageEffectRequestObject(
	const TSharedPtr<FJsonObject>& DamageRequestObject,
	FWBDamageEffectRequest& OutRequest,
	FString& OutReason)
{
	if (!DamageRequestObject.IsValid())
	{
		OutReason = TEXT("missing_damage_effect_payload");
		return false;
	}

	OutRequest.TargetUnitId = ReadIntegerFieldOrDefault(DamageRequestObject, TEXT("target_unit_id"), -1);
	OutRequest.SourceUnitId = ReadIntegerFieldOrDefault(DamageRequestObject, TEXT("source_unit_id"), -1);
	OutRequest.SourcePlayerId = ReadIntegerFieldOrDefault(DamageRequestObject, TEXT("source_player_id"), -1);
	if (!TryReadIntegerField(DamageRequestObject, TEXT("amount"), OutRequest.Amount))
	{
		OutReason = TEXT("malformed_damage_effect_payload");
		return false;
	}

	DamageRequestObject->TryGetBoolField(TEXT("bypass_armor"), OutRequest.bBypassArmor);
	FString DamageCause;
	if (DamageRequestObject->TryGetStringField(TEXT("damage_cause"), DamageCause))
	{
		OutRequest.DamageCause = FName(*DamageCause);
	}

	FString SourceReason;
	if (DamageRequestObject->TryGetStringField(TEXT("source_reason"), SourceReason))
	{
		OutRequest.SourceReason = FName(*SourceReason);
	}

	OutReason.Reset();
	return true;
}

bool ParseDamageEffectRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBDamageEffectRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* DamageRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("damage_effect_request"), DamageRequestObject)
		|| DamageRequestObject == nullptr
		|| !DamageRequestObject->IsValid())
	{
		OutReason = TEXT("missing_damage_effect_request");
		return false;
	}

	if (!ParseDamageEffectRequestObject(*DamageRequestObject, OutRequest, OutReason))
	{
		return false;
	}

	if (OutRequest.TargetUnitId == -1)
	{
		OutReason = TEXT("malformed_damage_effect_request");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseHealEffectRequestObject(
	const TSharedPtr<FJsonObject>& HealRequestObject,
	FWBHealEffectRequest& OutRequest,
	FString& OutReason)
{
	if (!HealRequestObject.IsValid())
	{
		OutReason = TEXT("missing_heal_effect_payload");
		return false;
	}

	OutRequest.TargetUnitId = ReadIntegerFieldOrDefault(HealRequestObject, TEXT("target_unit_id"), -1);
	OutRequest.SourceUnitId = ReadIntegerFieldOrDefault(HealRequestObject, TEXT("source_unit_id"), -1);
	OutRequest.SourcePlayerId = ReadIntegerFieldOrDefault(HealRequestObject, TEXT("source_player_id"), -1);
	if (!TryReadIntegerField(HealRequestObject, TEXT("amount"), OutRequest.Amount))
	{
		OutReason = TEXT("malformed_heal_effect_payload");
		return false;
	}

	FString SourceReason;
	if (HealRequestObject->TryGetStringField(TEXT("source_reason"), SourceReason))
	{
		OutRequest.SourceReason = FName(*SourceReason);
	}

	OutReason.Reset();
	return true;
}

bool ParseHealEffectRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBHealEffectRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* HealRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("heal_effect_request"), HealRequestObject)
		|| HealRequestObject == nullptr
		|| !HealRequestObject->IsValid())
	{
		OutReason = TEXT("missing_heal_effect_request");
		return false;
	}

	if (!ParseHealEffectRequestObject(*HealRequestObject, OutRequest, OutReason))
	{
		return false;
	}

	if (OutRequest.TargetUnitId == -1)
	{
		OutReason = TEXT("malformed_heal_effect_request");
		return false;
	}

	OutReason.Reset();
	return true;
}

EWBGenericEffectOp ReadGenericEffectOperationOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString OperationString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("operation"), OperationString))
	{
		return EWBGenericEffectOp::Unknown;
	}

	if (OperationString == TEXT("armor_effect"))
	{
		return EWBGenericEffectOp::ArmorEffect;
	}

	if (OperationString == TEXT("status_effect"))
	{
		return EWBGenericEffectOp::StatusEffect;
	}

	if (OperationString == TEXT("damage_effect"))
	{
		return EWBGenericEffectOp::DamageEffect;
	}

	if (OperationString == TEXT("heal_effect"))
	{
		return EWBGenericEffectOp::HealEffect;
	}

	return EWBGenericEffectOp::Unknown;
}

bool ParseEffectSourceRef(
	const TSharedPtr<FJsonObject>& EffectRequestObject,
	FWBEffectSourceRef& OutSource,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* SourceObject = nullptr;
	if (!EffectRequestObject.IsValid() || !EffectRequestObject->HasField(TEXT("source")))
	{
		OutReason.Reset();
		return true;
	}

	if (!EffectRequestObject->TryGetObjectField(TEXT("source"), SourceObject)
		|| SourceObject == nullptr
		|| !SourceObject->IsValid())
	{
		OutReason = TEXT("malformed_effect_request_source");
		return false;
	}

	OutSource.PlayerId = ReadIntegerFieldOrDefault(*SourceObject, TEXT("player_id"), -1);
	OutSource.SourceUnitId = ReadIntegerFieldOrDefault(*SourceObject, TEXT("source_unit_id"), -1);
	(*SourceObject)->TryGetStringField(TEXT("source_card_id"), OutSource.SourceCardId);
	(*SourceObject)->TryGetStringField(TEXT("source_effect_id"), OutSource.SourceEffectId);
	OutReason.Reset();
	return true;
}

bool ParseEffectTargetRef(
	const TSharedPtr<FJsonObject>& EffectRequestObject,
	FWBEffectTargetRef& OutTarget,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (!EffectRequestObject.IsValid()
		|| !EffectRequestObject->TryGetObjectField(TEXT("target"), TargetObject)
		|| TargetObject == nullptr
		|| !TargetObject->IsValid())
	{
		OutReason = TEXT("missing_effect_request_target");
		return false;
	}

	OutTarget.TargetUnitId = ReadIntegerFieldOrDefault(*TargetObject, TEXT("target_unit_id"), -1);
	if (!ParseOptionalFixtureTileField(*TargetObject, TEXT("target_tile"), OutTarget.TargetTile, OutReason)
		|| !ParseOptionalWallEdgeField(*TargetObject, TEXT("target_wall_edge"), OutTarget.TargetWallEdge, OutReason))
	{
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseGenericEffectPayload(
	const TSharedPtr<FJsonObject>& PayloadObject,
	const int32 PayloadIndex,
	FWBGenericEffectPayload& OutPayload,
	FString& OutReason)
{
	if (!PayloadObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_effect_request_payload_%d"), PayloadIndex);
		return false;
	}

	OutPayload.Operation = ReadGenericEffectOperationOrDefault(PayloadObject);
	if (OutPayload.Operation == EWBGenericEffectOp::ArmorEffect)
	{
		const TSharedPtr<FJsonObject>* ArmorEffectObject = nullptr;
		if (!PayloadObject->TryGetObjectField(TEXT("armor_effect"), ArmorEffectObject)
			|| ArmorEffectObject == nullptr
			|| !ArmorEffectObject->IsValid())
		{
			OutReason = FString::Printf(TEXT("missing_effect_request_payload_%d_armor_effect"), PayloadIndex);
			return false;
		}

		return ParseArmorEffectRequestObject(*ArmorEffectObject, OutPayload.ArmorEffect, OutReason);
	}

	if (OutPayload.Operation == EWBGenericEffectOp::StatusEffect)
	{
		const TSharedPtr<FJsonObject>* StatusEffectObject = nullptr;
		if (!PayloadObject->TryGetObjectField(TEXT("status_effect"), StatusEffectObject)
			|| StatusEffectObject == nullptr
			|| !StatusEffectObject->IsValid())
		{
			OutReason = FString::Printf(TEXT("missing_effect_request_payload_%d_status_effect"), PayloadIndex);
			return false;
		}

		return ParseStatusEffectRequestObject(*StatusEffectObject, OutPayload.StatusEffect, OutReason);
	}

	if (OutPayload.Operation == EWBGenericEffectOp::DamageEffect)
	{
		const TSharedPtr<FJsonObject>* DamageEffectObject = nullptr;
		if (!PayloadObject->TryGetObjectField(TEXT("damage_effect"), DamageEffectObject)
			|| DamageEffectObject == nullptr
			|| !DamageEffectObject->IsValid())
		{
			OutReason = FString::Printf(TEXT("missing_effect_request_payload_%d_damage_effect"), PayloadIndex);
			return false;
		}

		return ParseDamageEffectRequestObject(*DamageEffectObject, OutPayload.DamageEffect, OutReason);
	}

	if (OutPayload.Operation == EWBGenericEffectOp::HealEffect)
	{
		const TSharedPtr<FJsonObject>* HealEffectObject = nullptr;
		if (!PayloadObject->TryGetObjectField(TEXT("heal_effect"), HealEffectObject)
			|| HealEffectObject == nullptr
			|| !HealEffectObject->IsValid())
		{
			OutReason = FString::Printf(TEXT("missing_effect_request_payload_%d_heal_effect"), PayloadIndex);
			return false;
		}

		return ParseHealEffectRequestObject(*HealEffectObject, OutPayload.HealEffect, OutReason);
	}

	OutReason.Reset();
	return true;
}

bool ParseEffectRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBEffectRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* EffectRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("effect_request"), EffectRequestObject)
		|| EffectRequestObject == nullptr
		|| !EffectRequestObject->IsValid())
	{
		OutReason = TEXT("missing_effect_request");
		return false;
	}

	if (!ParseEffectSourceRef(*EffectRequestObject, OutRequest.Source, OutReason)
		|| !ParseEffectTargetRef(*EffectRequestObject, OutRequest.Target, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* PayloadValues = nullptr;
	if (!(*EffectRequestObject)->TryGetArrayField(TEXT("payloads"), PayloadValues) || PayloadValues == nullptr)
	{
		OutReason = TEXT("missing_effect_request_payloads");
		return false;
	}

	for (int32 PayloadIndex = 0; PayloadIndex < PayloadValues->Num(); ++PayloadIndex)
	{
		const TSharedPtr<FJsonObject> PayloadObject = (*PayloadValues)[PayloadIndex].IsValid()
			? (*PayloadValues)[PayloadIndex]->AsObject()
			: nullptr;
		FWBGenericEffectPayload Payload;
		if (!ParseGenericEffectPayload(PayloadObject, PayloadIndex, Payload, OutReason))
		{
			return false;
		}

		OutRequest.Payloads.Add(Payload);
	}

	OutReason.Reset();
	return true;
}

bool ParseCardActivationSourceObject(
	const TSharedPtr<FJsonObject>& SourceObject,
	FWBCardActivationSource& OutSource,
	FString& OutReason)
{
	if (!SourceObject.IsValid())
	{
		OutReason = TEXT("missing_card_activation_source");
		return false;
	}

	OutSource.PlayerId = ReadIntegerFieldOrDefault(SourceObject, TEXT("player_id"), -1);
	OutSource.SourceUnitId = ReadIntegerFieldOrDefault(SourceObject, TEXT("source_unit_id"), -1);
	SourceObject->TryGetStringField(TEXT("source_card_id"), OutSource.SourceCardId);
	SourceObject->TryGetStringField(TEXT("source_effect_id"), OutSource.SourceEffectId);

	OutReason.Reset();
	return true;
}

bool ParseOptionalCardActivationUsageCommitField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardActivationUsageCommit& OutUsageCommit,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* UsageCommitObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, UsageCommitObject)
		|| UsageCommitObject == nullptr
		|| !UsageCommitObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	OutUsageCommit.bMarkUsageOnSuccess = ReadBoolFieldOrDefault(
		*UsageCommitObject,
		TEXT("mark_usage_on_success"),
		OutUsageCommit.bMarkUsageOnSuccess);
	OutUsageCommit.PlayerId = ReadIntegerFieldOrDefault(
		*UsageCommitObject,
		TEXT("player_id"),
		OutUsageCommit.PlayerId);
	(*UsageCommitObject)->TryGetStringField(TEXT("usage_key"), OutUsageCommit.UsageKey);

	OutReason.Reset();
	return true;
}

bool ParseOptionalCardActivationCostPaymentCommitField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardActivationCostPaymentCommit& OutCostPaymentCommit,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* CostPaymentCommitObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, CostPaymentCommitObject)
		|| CostPaymentCommitObject == nullptr
		|| !CostPaymentCommitObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	OutCostPaymentCommit.bPayCostOnSuccess = ReadBoolFieldOrDefault(
		*CostPaymentCommitObject,
		TEXT("pay_cost_on_success"),
		OutCostPaymentCommit.bPayCostOnSuccess);
	OutCostPaymentCommit.PlayerId = ReadIntegerFieldOrDefault(
		*CostPaymentCommitObject,
		TEXT("player_id"),
		OutCostPaymentCommit.PlayerId);
	OutCostPaymentCommit.SourceUnitId = ReadIntegerFieldOrDefault(
		*CostPaymentCommitObject,
		TEXT("source_unit_id"),
		OutCostPaymentCommit.SourceUnitId);
	OutCostPaymentCommit.RequiredRR = ReadIntegerFieldOrDefault(
		*CostPaymentCommitObject,
		TEXT("required_rr"),
		OutCostPaymentCommit.RequiredRR);

	FString CostKind;
	if ((*CostPaymentCommitObject)->TryGetStringField(TEXT("cost_kind"), CostKind))
	{
		OutCostPaymentCommit.CostKind = FName(*CostKind);
	}

	OutReason.Reset();
	return true;
}

bool ParseCardActivationCommandFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBCardActivationCommand& OutCommand,
	FString& OutReason)
{
	OutCommand = FWBCardActivationCommand();

	const TSharedPtr<FJsonObject>* CardActivationObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("card_activation"), CardActivationObject)
		|| CardActivationObject == nullptr
		|| !CardActivationObject->IsValid())
	{
		OutReason = TEXT("missing_card_activation");
		return false;
	}

	const TSharedPtr<FJsonObject>* SourceObject = nullptr;
	if (!(*CardActivationObject)->TryGetObjectField(TEXT("source"), SourceObject)
		|| SourceObject == nullptr
		|| !SourceObject->IsValid())
	{
		OutReason = TEXT("missing_card_activation_source");
		return false;
	}

	if (!ParseCardActivationSourceObject(*SourceObject, OutCommand.Source, OutReason))
	{
		return false;
	}

	(*CardActivationObject)->TryGetStringField(TEXT("debug_activation_id"), OutCommand.DebugActivationId);
	if (!ParseOptionalCardActivationCostPaymentCommitField(
		*CardActivationObject,
		TEXT("cost_payment_commit"),
		OutCommand.CostPaymentCommit,
		OutReason))
	{
		return false;
	}

	if (!ParseOptionalCardActivationUsageCommitField(
		*CardActivationObject,
		TEXT("usage_commit"),
		OutCommand.UsageCommit,
		OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* EffectRequestObject = nullptr;
	if (!(*CardActivationObject)->TryGetObjectField(TEXT("effect_request"), EffectRequestObject)
		|| EffectRequestObject == nullptr
		|| !EffectRequestObject->IsValid())
	{
		OutReason = TEXT("missing_card_activation_effect_request");
		return false;
	}

	TSharedPtr<FJsonObject> EffectRequestRoot = MakeShared<FJsonObject>();
	EffectRequestRoot->SetObjectField(TEXT("effect_request"), *EffectRequestObject);
	return ParseEffectRequestFromFixture(EffectRequestRoot, OutCommand.EffectRequest, OutReason);
}

EWBCardDefinitionKind ReadCardDefinitionKindOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString KindString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("kind"), KindString))
	{
		return EWBCardDefinitionKind::Unknown;
	}

	if (KindString == TEXT("character"))
	{
		return EWBCardDefinitionKind::Character;
	}

	if (KindString == TEXT("wand"))
	{
		return EWBCardDefinitionKind::Wand;
	}

	if (KindString == TEXT("action"))
	{
		return EWBCardDefinitionKind::Action;
	}

	if (KindString == TEXT("terrain"))
	{
		return EWBCardDefinitionKind::Terrain;
	}

	if (KindString == TEXT("wall"))
	{
		return EWBCardDefinitionKind::Wall;
	}

	if (KindString == TEXT("fixture"))
	{
		return EWBCardDefinitionKind::Fixture;
	}

	return EWBCardDefinitionKind::Unknown;
}

EWBCardEffectTargetRequirement ReadCardEffectTargetRequirementOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString RequirementString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("target_requirement"), RequirementString))
	{
		return EWBCardEffectTargetRequirement::None;
	}

	if (RequirementString == TEXT("unit"))
	{
		return EWBCardEffectTargetRequirement::Unit;
	}

	if (RequirementString == TEXT("tile"))
	{
		return EWBCardEffectTargetRequirement::Tile;
	}

	if (RequirementString == TEXT("wall_edge"))
	{
		return EWBCardEffectTargetRequirement::WallEdge;
	}

	return EWBCardEffectTargetRequirement::None;
}

EWBCardActivationSourceZone ReadCardActivationSourceZoneOrDefault(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const EWBCardActivationSourceZone DefaultValue)
{
	FString ZoneString;
	if (!Object.IsValid() || !Object->TryGetStringField(FieldName, ZoneString))
	{
		return DefaultValue;
	}

	if (ZoneString == TEXT("fixture"))
	{
		return EWBCardActivationSourceZone::Fixture;
	}

	if (ZoneString == TEXT("hand"))
	{
		return EWBCardActivationSourceZone::Hand;
	}

	if (ZoneString == TEXT("board"))
	{
		return EWBCardActivationSourceZone::Board;
	}

	if (ZoneString == TEXT("equipped"))
	{
		return EWBCardActivationSourceZone::Equipped;
	}

	if (ZoneString == TEXT("discard"))
	{
		return EWBCardActivationSourceZone::Discard;
	}

	if (ZoneString == TEXT("deck"))
	{
		return EWBCardActivationSourceZone::Deck;
	}

	return EWBCardActivationSourceZone::Unknown;
}

EWBCardActivationTimingRequirement ReadCardActivationTimingOrDefault(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const EWBCardActivationTimingRequirement DefaultValue)
{
	FString TimingString;
	if (!Object.IsValid() || !Object->TryGetStringField(FieldName, TimingString))
	{
		return DefaultValue;
	}

	if (TimingString == TEXT("any"))
	{
		return EWBCardActivationTimingRequirement::Any;
	}

	if (TimingString == TEXT("normal_turn_priority"))
	{
		return EWBCardActivationTimingRequirement::NormalTurnPriority;
	}

	if (TimingString == TEXT("response_window"))
	{
		return EWBCardActivationTimingRequirement::ResponseWindow;
	}

	return EWBCardActivationTimingRequirement::Unknown;
}

bool TryReadOptionalBoolField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	bool& OutValue,
	FString& OutReason)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid()
		? Object->Values.Find(FieldName)
		: nullptr;
	if (Value == nullptr)
	{
		OutReason.Reset();
		return true;
	}

	if (!Value->IsValid() || (*Value)->Type != EJson::Boolean)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), *FieldPath);
		return false;
	}

	OutValue = (*Value)->AsBool();
	OutReason.Reset();
	return true;
}

bool TryReadOptionalIntegerField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	int32& OutValue,
	FString& OutReason)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid()
		? Object->Values.Find(FieldName)
		: nullptr;
	if (Value == nullptr)
	{
		OutReason.Reset();
		return true;
	}

	if (!Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), *FieldPath);
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	OutReason.Reset();
	return true;
}

bool TryReadOptionalFNameField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	FName& OutValue,
	FString& OutReason)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid()
		? Object->Values.Find(FieldName)
		: nullptr;
	if (Value == nullptr)
	{
		OutReason.Reset();
		return true;
	}

	if (!Value->IsValid() || (*Value)->Type != EJson::String)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), *FieldPath);
		return false;
	}

	OutValue = FName(*(*Value)->AsString());
	OutReason.Reset();
	return true;
}

bool TryReadOptionalStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	FString& OutValue,
	FString& OutReason)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid()
		? Object->Values.Find(FieldName)
		: nullptr;
	if (Value == nullptr)
	{
		OutReason.Reset();
		return true;
	}

	if (!Value->IsValid() || (*Value)->Type != EJson::String)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), *FieldPath);
		return false;
	}

	OutValue = (*Value)->AsString();
	OutReason.Reset();
	return true;
}

bool ParseOptionalCardActivationCostGateDefinitionField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardActivationCostGateDefinition& OutCostGate,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* CostGateObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, CostGateObject)
		|| CostGateObject == nullptr
		|| !CostGateObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	return TryReadOptionalBoolField(
			*CostGateObject,
			TEXT("requires_external_affordability"),
			TEXT("source_gate.cost_gate.requires_external_affordability"),
			OutCostGate.bRequiresExternalAffordability,
			OutReason)
		&& TryReadOptionalIntegerField(
			*CostGateObject,
			TEXT("required_rr"),
			TEXT("source_gate.cost_gate.required_rr"),
			OutCostGate.RequiredRR,
			OutReason)
		&& TryReadOptionalFNameField(
			*CostGateObject,
			TEXT("cost_kind"),
			TEXT("source_gate.cost_gate.cost_kind"),
			OutCostGate.CostKind,
			OutReason);
}

bool ParseOptionalCardActivationCostGateContextField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardActivationCostGateContext& OutCostContext,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* CostContextObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, CostContextObject)
		|| CostContextObject == nullptr
		|| !CostContextObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	return TryReadOptionalBoolField(
			*CostContextObject,
			TEXT("has_external_affordability"),
			TEXT("source_gate_context.cost_context.has_external_affordability"),
			OutCostContext.bHasExternalAffordability,
			OutReason)
		&& TryReadOptionalBoolField(
			*CostContextObject,
			TEXT("externally_affordable"),
			TEXT("source_gate_context.cost_context.externally_affordable"),
			OutCostContext.bExternallyAffordable,
			OutReason)
		&& TryReadOptionalIntegerField(
			*CostContextObject,
			TEXT("supplied_required_rr"),
			TEXT("source_gate_context.cost_context.supplied_required_rr"),
			OutCostContext.SuppliedRequiredRR,
			OutReason)
		&& TryReadOptionalIntegerField(
			*CostContextObject,
			TEXT("supplied_available_rl"),
			TEXT("source_gate_context.cost_context.supplied_available_rl"),
			OutCostContext.SuppliedAvailableRL,
			OutReason)
		&& TryReadOptionalFNameField(
			*CostContextObject,
			TEXT("cost_kind"),
			TEXT("source_gate_context.cost_context.cost_kind"),
			OutCostContext.CostKind,
			OutReason);
}

bool ParseOptionalCardActivationFixtureZoneContextField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardActivationFixtureZoneContext& OutFixtureZoneContext,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* FixtureZoneContextObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, FixtureZoneContextObject)
		|| FixtureZoneContextObject == nullptr
		|| !FixtureZoneContextObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EntryValues = nullptr;
	if (!(*FixtureZoneContextObject)->TryGetArrayField(TEXT("entries"), EntryValues) || EntryValues == nullptr)
	{
		OutReason = TEXT("malformed_source_gate_context.fixture_zone_context.entries");
		return false;
	}

	OutFixtureZoneContext.Entries.Reset();
	for (int32 EntryIndex = 0; EntryIndex < EntryValues->Num(); ++EntryIndex)
	{
		const TSharedPtr<FJsonObject> EntryObject = (*EntryValues)[EntryIndex].IsValid()
			? (*EntryValues)[EntryIndex]->AsObject()
			: nullptr;
		if (!EntryObject.IsValid())
		{
			OutReason = FString::Printf(
				TEXT("malformed_source_gate_context.fixture_zone_context.entries[%d]"),
				EntryIndex);
			return false;
		}

		FWBCardActivationFixtureZoneEntry Entry;
		if (!TryReadOptionalStringField(
				EntryObject,
				TEXT("card_id"),
				FString::Printf(TEXT("source_gate_context.fixture_zone_context.entries[%d].card_id"), EntryIndex),
				Entry.CardId,
				OutReason)
			|| Entry.CardId.IsEmpty())
		{
			if (OutReason.IsEmpty())
			{
				OutReason = FString::Printf(
					TEXT("malformed_source_gate_context.fixture_zone_context.entries[%d].card_id"),
					EntryIndex);
			}
			return false;
		}

		if (!TryReadOptionalIntegerField(
				EntryObject,
				TEXT("owner_player_id"),
				FString::Printf(TEXT("source_gate_context.fixture_zone_context.entries[%d].owner_player_id"), EntryIndex),
				Entry.OwnerPlayerId,
				OutReason))
		{
			return false;
		}

		Entry.Zone = ReadCardActivationSourceZoneOrDefault(EntryObject, TEXT("zone"), Entry.Zone);
		if (Entry.Zone == EWBCardActivationSourceZone::Unknown)
		{
			OutReason = FString::Printf(
				TEXT("malformed_source_gate_context.fixture_zone_context.entries[%d].zone"),
				EntryIndex);
			return false;
		}

		if (!TryReadOptionalIntegerField(
				EntryObject,
				TEXT("equipped_to_unit_id"),
				FString::Printf(TEXT("source_gate_context.fixture_zone_context.entries[%d].equipped_to_unit_id"), EntryIndex),
				Entry.EquippedToUnitId,
				OutReason))
		{
			return false;
		}

		OutFixtureZoneContext.Entries.Add(Entry);
	}

	OutReason.Reset();
	return true;
}

bool ParseOptionalCardActivationSourceGateDefinitionField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardActivationSourceGateDefinition& OutGate,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* GateObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, GateObject)
		|| GateObject == nullptr
		|| !GateObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	OutGate.RequiredZone = ReadCardActivationSourceZoneOrDefault(
		*GateObject,
		TEXT("required_zone"),
		OutGate.RequiredZone);
	OutGate.Timing = ReadCardActivationTimingOrDefault(
		*GateObject,
		TEXT("timing"),
		OutGate.Timing);
	OutGate.bRequiresFixtureZoneOwnership = ReadBoolFieldOrDefault(
		*GateObject,
		TEXT("requires_fixture_zone_ownership"),
		OutGate.bRequiresFixtureZoneOwnership);
	OutGate.bRequiresSourceUnit = ReadBoolFieldOrDefault(
		*GateObject,
		TEXT("requires_source_unit"),
		OutGate.bRequiresSourceUnit);
	OutGate.bRequiresSourceUnitOwnership = ReadBoolFieldOrDefault(
		*GateObject,
		TEXT("requires_source_unit_ownership"),
		OutGate.bRequiresSourceUnitOwnership);
	OutGate.bBlockedByStunned = ReadBoolFieldOrDefault(
		*GateObject,
		TEXT("blocked_by_stunned"),
		OutGate.bBlockedByStunned);
	OutGate.bBlockedByFrozen = ReadBoolFieldOrDefault(
		*GateObject,
		TEXT("blocked_by_frozen"),
		OutGate.bBlockedByFrozen);
	OutGate.bRequiresCostsSatisfiedExternally = ReadBoolFieldOrDefault(
		*GateObject,
		TEXT("requires_costs_satisfied_externally"),
		OutGate.bRequiresCostsSatisfiedExternally);
	if (!ParseOptionalCardActivationCostGateDefinitionField(
		*GateObject,
		TEXT("cost_gate"),
		OutGate.CostGate,
		OutReason))
	{
		return false;
	}
	OutGate.bOncePerTurn = ReadBoolFieldOrDefault(
		*GateObject,
		TEXT("once_per_turn"),
		OutGate.bOncePerTurn);
	(*GateObject)->TryGetStringField(TEXT("once_per_turn_key"), OutGate.OncePerTurnKey);
	OutGate.bHasExplicitSourceGate = true;

	OutReason.Reset();
	return true;
}

bool ParseOptionalCardActivationSourceGateContextField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardActivationSourceGateContext& OutContext,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ContextObject = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetObjectField(FieldName, ContextObject)
		|| ContextObject == nullptr
		|| !ContextObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), FieldName);
		return false;
	}

	OutContext.PlayerId = ReadIntegerFieldOrDefault(*ContextObject, TEXT("player_id"), OutContext.PlayerId);
	OutContext.SourceUnitId = ReadIntegerFieldOrDefault(*ContextObject, TEXT("source_unit_id"), OutContext.SourceUnitId);
	OutContext.SourceZone = ReadCardActivationSourceZoneOrDefault(
		*ContextObject,
		TEXT("source_zone"),
		OutContext.SourceZone);
	if (!TryReadOptionalStringField(
		*ContextObject,
		TEXT("source_card_id"),
		TEXT("source_gate_context.source_card_id"),
		OutContext.SourceCardId,
		OutReason))
	{
		return false;
	}
	if (!ParseOptionalCardActivationFixtureZoneContextField(
		*ContextObject,
		TEXT("fixture_zone_context"),
		OutContext.FixtureZoneContext,
		OutReason))
	{
		return false;
	}
	OutContext.bCostsSatisfiedExternally = ReadBoolFieldOrDefault(
		*ContextObject,
		TEXT("costs_satisfied_externally"),
		OutContext.bCostsSatisfiedExternally);
	if (!ParseOptionalCardActivationCostGateContextField(
		*ContextObject,
		TEXT("cost_context"),
		OutContext.CostContext,
		OutReason))
	{
		return false;
	}
	(*ContextObject)->TryGetStringField(TEXT("activation_usage_key"), OutContext.ActivationUsageKey);
	OutContext.bHasExplicitSourceGateContext = true;

	OutReason.Reset();
	return true;
}

bool ParseCardEffectDefinitionObject(
	const TSharedPtr<FJsonObject>& EffectObject,
	const int32 EffectIndex,
	FWBCardEffectDefinition& OutEffect,
	FString& OutReason)
{
	if (!EffectObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_card_definition_activated_effect_%d"), EffectIndex);
		return false;
	}

	EffectObject->TryGetStringField(TEXT("effect_id"), OutEffect.EffectId);
	EffectObject->TryGetStringField(TEXT("public_label"), OutEffect.PublicLabel);
	OutEffect.TargetRequirement = ReadCardEffectTargetRequirementOrDefault(EffectObject);
	if (!ParseOptionalCardActivationSourceGateDefinitionField(
		EffectObject,
		TEXT("source_gate"),
		OutEffect.SourceGate,
		OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* PayloadValues = nullptr;
	if (!EffectObject->TryGetArrayField(TEXT("payloads"), PayloadValues) || PayloadValues == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_card_definition_activated_effect_%d_payloads"), EffectIndex);
		return false;
	}

	for (int32 PayloadIndex = 0; PayloadIndex < PayloadValues->Num(); ++PayloadIndex)
	{
		const TSharedPtr<FJsonObject> PayloadObject = (*PayloadValues)[PayloadIndex].IsValid()
			? (*PayloadValues)[PayloadIndex]->AsObject()
			: nullptr;
		FWBGenericEffectPayload Payload;
		if (!ParseGenericEffectPayload(PayloadObject, PayloadIndex, Payload, OutReason))
		{
			return false;
		}

		OutEffect.Payloads.Add(Payload);
	}

	OutReason.Reset();
	return true;
}

bool ParseCardDefinitionObject(
	const TSharedPtr<FJsonObject>& CardDefinitionObject,
	FWBCardDefinition& OutDefinition,
	FString& OutReason)
{
	if (!CardDefinitionObject.IsValid())
	{
		OutReason = TEXT("missing_card_definition");
		return false;
	}

	OutDefinition = FWBCardDefinition();
	CardDefinitionObject->TryGetStringField(TEXT("card_id"), OutDefinition.CardId);
	CardDefinitionObject->TryGetStringField(TEXT("public_name"), OutDefinition.PublicName);
	OutDefinition.Kind = ReadCardDefinitionKindOrDefault(CardDefinitionObject);

	const TArray<TSharedPtr<FJsonValue>>* EffectValues = nullptr;
	if (!CardDefinitionObject->TryGetArrayField(TEXT("activated_effects"), EffectValues) || EffectValues == nullptr)
	{
		OutReason = TEXT("missing_card_definition_activated_effects");
		return false;
	}

	for (int32 EffectIndex = 0; EffectIndex < EffectValues->Num(); ++EffectIndex)
	{
		const TSharedPtr<FJsonObject> EffectObject = (*EffectValues)[EffectIndex].IsValid()
			? (*EffectValues)[EffectIndex]->AsObject()
			: nullptr;
		FWBCardEffectDefinition Effect;
		if (!ParseCardEffectDefinitionObject(EffectObject, EffectIndex, Effect, OutReason))
		{
			return false;
		}

		OutDefinition.ActivatedEffects.Add(Effect);
	}

	OutReason.Reset();
	return true;
}

bool ParseCardDefinitionFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBCardDefinition& OutDefinition,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* CardDefinitionObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("card_definition"), CardDefinitionObject)
		|| CardDefinitionObject == nullptr
		|| !CardDefinitionObject->IsValid())
	{
		OutReason = TEXT("missing_card_definition");
		return false;
	}

	return ParseCardDefinitionObject(*CardDefinitionObject, OutDefinition, OutReason);
}

bool ParseOptionalActivationTargetRef(
	const TSharedPtr<FJsonObject>& ActivationRequestObject,
	FWBEffectTargetRef& OutTarget,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (!ActivationRequestObject.IsValid() || !ActivationRequestObject->HasField(TEXT("target")))
	{
		OutReason.Reset();
		return true;
	}

	if (!ActivationRequestObject->TryGetObjectField(TEXT("target"), TargetObject)
		|| TargetObject == nullptr
		|| !TargetObject->IsValid())
	{
		OutReason = TEXT("malformed_activation_request_target");
		return false;
	}

	OutTarget.TargetUnitId = ReadIntegerFieldOrDefault(*TargetObject, TEXT("target_unit_id"), -1);
	if (!ParseOptionalFixtureTileField(*TargetObject, TEXT("target_tile"), OutTarget.TargetTile, OutReason)
		|| !ParseOptionalWallEdgeField(*TargetObject, TEXT("target_wall_edge"), OutTarget.TargetWallEdge, OutReason))
	{
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseActivationRequestObject(
	const TSharedPtr<FJsonObject>& ActivationRequestObject,
	FWBCardActivationExpansionRequest& OutRequest,
	FString& OutReason)
{
	if (!ActivationRequestObject.IsValid())
	{
		OutReason = TEXT("missing_activation_request");
		return false;
	}

	OutRequest.PlayerId = ReadIntegerFieldOrDefault(ActivationRequestObject, TEXT("player_id"), -1);
	OutRequest.SourceUnitId = ReadIntegerFieldOrDefault(ActivationRequestObject, TEXT("source_unit_id"), -1);
	ActivationRequestObject->TryGetStringField(TEXT("effect_id"), OutRequest.EffectId);
	ActivationRequestObject->TryGetStringField(TEXT("debug_activation_id"), OutRequest.DebugActivationId);
	OutRequest.SourceGateContext.PlayerId = OutRequest.PlayerId;
	OutRequest.SourceGateContext.SourceUnitId = OutRequest.SourceUnitId;
	if (!ParseOptionalCardActivationSourceGateContextField(
		ActivationRequestObject,
		TEXT("source_gate_context"),
		OutRequest.SourceGateContext,
		OutReason))
	{
		return false;
	}

	return ParseOptionalActivationTargetRef(ActivationRequestObject, OutRequest.Target, OutReason);
}

bool ParseCardActivationExpansionRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBCardActivationExpansionRequest& OutRequest,
	FString& OutReason)
{
	if (!ParseCardDefinitionFromFixture(Fixture, OutRequest.CardDefinition, OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ActivationRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("activation_request"), ActivationRequestObject)
		|| ActivationRequestObject == nullptr
		|| !ActivationRequestObject->IsValid())
	{
		OutReason = TEXT("missing_activation_request");
		return false;
	}

	return ParseActivationRequestObject(*ActivationRequestObject, OutRequest, OutReason);
}

bool ParseCardActivationAffordabilityRequestObject(
	const TSharedPtr<FJsonObject>& RequestObject,
	FWBCardActivationAffordabilityRequest& OutRequest,
	FString& OutReason)
{
	if (!RequestObject.IsValid())
	{
		OutReason = TEXT("missing_affordability_request");
		return false;
	}

	OutRequest = FWBCardActivationAffordabilityRequest();
	if (!TryReadIntegerField(RequestObject, TEXT("player_id"), OutRequest.PlayerId)
		|| !TryReadIntegerField(RequestObject, TEXT("source_unit_id"), OutRequest.SourceUnitId)
		|| !TryReadIntegerField(RequestObject, TEXT("required_rr"), OutRequest.RequiredRR))
	{
		OutReason = TEXT("malformed_affordability_request");
		return false;
	}

	RequestObject->TryGetStringField(TEXT("source_card_id"), OutRequest.SourceCardId);
	RequestObject->TryGetStringField(TEXT("source_effect_id"), OutRequest.SourceEffectId);
	FString CostKind;
	if (RequestObject->TryGetStringField(TEXT("cost_kind"), CostKind))
	{
		OutRequest.CostKind = FName(*CostKind);
	}

	OutReason.Reset();
	return true;
}

bool ParseEffectSourceGateContextsObject(
	const TSharedPtr<FJsonObject>& SourceObject,
	const int32 SourceIndex,
	FWBCardActivationCandidateSource& OutSource,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ContextsObject = nullptr;
	if (!SourceObject.IsValid() || !SourceObject->HasField(TEXT("effect_source_gate_contexts")))
	{
		OutReason.Reset();
		return true;
	}

	if (!SourceObject->TryGetObjectField(TEXT("effect_source_gate_contexts"), ContextsObject)
		|| ContextsObject == nullptr
		|| !ContextsObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_activation_source_%d_effect_source_gate_contexts"), SourceIndex);
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ContextsObject)->Values)
	{
		const TSharedPtr<FJsonObject> ContextObject = Pair.Value.IsValid()
			? Pair.Value->AsObject()
			: nullptr;
		if (!ContextObject.IsValid())
		{
			OutReason = FString::Printf(
				TEXT("malformed_activation_source_%d_effect_source_gate_contexts_%s"),
				SourceIndex,
				*Pair.Key);
			return false;
		}

		TSharedPtr<FJsonObject> WrapperObject = MakeShared<FJsonObject>();
		WrapperObject->SetObjectField(TEXT("source_gate_context"), ContextObject);

		FWBCardActivationSourceGateContext Context;
		Context.PlayerId = OutSource.PlayerId;
		Context.SourceUnitId = OutSource.SourceUnitId;
		if (!ParseOptionalCardActivationSourceGateContextField(
			WrapperObject,
			TEXT("source_gate_context"),
			Context,
			OutReason))
		{
			OutReason = FString::Printf(
				TEXT("malformed_activation_source_%d_effect_source_gate_contexts_%s"),
				SourceIndex,
				*Pair.Key);
			return false;
		}

		OutSource.EffectIdToSourceGateContext.Add(Pair.Key, Context);
	}

	OutReason.Reset();
	return true;
}

bool ParseCardActivationAffordabilityRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBCardActivationAffordabilityRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* RequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("affordability_request"), RequestObject)
		|| RequestObject == nullptr
		|| !RequestObject->IsValid())
	{
		OutReason = TEXT("missing_affordability_request");
		return false;
	}

	return ParseCardActivationAffordabilityRequestObject(*RequestObject, OutRequest, OutReason);
}

bool ParseActivationCandidateTargetRefObject(
	const TSharedPtr<FJsonObject>& TargetObject,
	const int32 TargetIndex,
	FWBEffectTargetRef& OutTarget,
	FString& OutReason)
{
	if (!TargetObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_activation_candidate_target_%d"), TargetIndex);
		return false;
	}

	OutTarget.TargetUnitId = ReadIntegerFieldOrDefault(TargetObject, TEXT("target_unit_id"), -1);
	if (!ParseOptionalFixtureTileField(TargetObject, TEXT("target_tile"), OutTarget.TargetTile, OutReason)
		|| !ParseOptionalWallEdgeField(TargetObject, TEXT("target_wall_edge"), OutTarget.TargetWallEdge, OutReason))
	{
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ParseCardActivationCandidateSourceObject(
	const TSharedPtr<FJsonObject>& SourceObject,
	const int32 SourceIndex,
	FWBCardActivationCandidateSource& OutSource,
	FString& OutReason)
{
	if (!SourceObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_activation_source_%d"), SourceIndex);
		return false;
	}

	OutSource.PlayerId = ReadIntegerFieldOrDefault(SourceObject, TEXT("player_id"), -1);
	OutSource.SourceUnitId = ReadIntegerFieldOrDefault(SourceObject, TEXT("source_unit_id"), -1);
	OutSource.SourceGateContext.PlayerId = OutSource.PlayerId;
	OutSource.SourceGateContext.SourceUnitId = OutSource.SourceUnitId;
	if (!ParseOptionalCardActivationSourceGateContextField(
		SourceObject,
		TEXT("source_gate_context"),
		OutSource.SourceGateContext,
		OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* CardDefinitionObject = nullptr;
	if (!SourceObject->TryGetObjectField(TEXT("card_definition"), CardDefinitionObject)
		|| CardDefinitionObject == nullptr
		|| !CardDefinitionObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("missing_activation_source_%d_card_definition"), SourceIndex);
		return false;
	}

	if (!ParseCardDefinitionObject(*CardDefinitionObject, OutSource.CardDefinition, OutReason))
	{
		return false;
	}

	if (!ParseEffectSourceGateContextsObject(SourceObject, SourceIndex, OutSource, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* CandidateTargetValues = nullptr;
	if (!SourceObject->TryGetArrayField(TEXT("candidate_targets"), CandidateTargetValues) || CandidateTargetValues == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_activation_source_%d_candidate_targets"), SourceIndex);
		return false;
	}

	for (int32 TargetIndex = 0; TargetIndex < CandidateTargetValues->Num(); ++TargetIndex)
	{
		const TSharedPtr<FJsonObject> TargetObject = (*CandidateTargetValues)[TargetIndex].IsValid()
			? (*CandidateTargetValues)[TargetIndex]->AsObject()
			: nullptr;
		FWBEffectTargetRef Target;
		if (!ParseActivationCandidateTargetRefObject(TargetObject, TargetIndex, Target, OutReason))
		{
			return false;
		}

		OutSource.CandidateTargets.Add(Target);
	}

	OutReason.Reset();
	return true;
}

bool ParseCardActivationCandidateSourcesFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	TArray<FWBCardActivationCandidateSource>& OutSources,
	FString& OutReason)
{
	OutSources.Reset();

	const TArray<TSharedPtr<FJsonValue>>* SourceValues = nullptr;
	if (!Fixture.IsValid() || !Fixture->TryGetArrayField(TEXT("activation_sources"), SourceValues) || SourceValues == nullptr)
	{
		OutReason = TEXT("missing_activation_sources");
		return false;
	}

	for (int32 SourceIndex = 0; SourceIndex < SourceValues->Num(); ++SourceIndex)
	{
		const TSharedPtr<FJsonObject> SourceObject = (*SourceValues)[SourceIndex].IsValid()
			? (*SourceValues)[SourceIndex]->AsObject()
			: nullptr;
		FWBCardActivationCandidateSource Source;
		if (!ParseCardActivationCandidateSourceObject(SourceObject, SourceIndex, Source, OutReason))
		{
			return false;
		}

		OutSources.Add(Source);
	}

	OutReason.Reset();
	return true;
}

bool ParseDamageRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBDamageRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* DamageRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("damage_request"), DamageRequestObject)
		|| DamageRequestObject == nullptr
		|| !DamageRequestObject->IsValid())
	{
		OutReason = TEXT("missing_damage_request");
		return false;
	}

	OutRequest.DamageKind = ReadDamageKindOrDefault(*DamageRequestObject);
	if (!TryReadIntegerField(*DamageRequestObject, TEXT("source_unit_id"), OutRequest.SourceUnitId)
		|| !TryReadIntegerField(*DamageRequestObject, TEXT("target_unit_id"), OutRequest.TargetUnitId)
		|| !TryReadIntegerField(*DamageRequestObject, TEXT("source_player_id"), OutRequest.SourcePlayerId)
		|| !TryReadIntegerField(*DamageRequestObject, TEXT("base_damage"), OutRequest.BaseDamage))
	{
		OutReason = TEXT("malformed_damage_request");
		return false;
	}

	(*DamageRequestObject)->TryGetBoolField(TEXT("bypass_armor"), OutRequest.bBypassArmor);
	FString DamageCause;
	if ((*DamageRequestObject)->TryGetStringField(TEXT("damage_cause"), DamageCause))
	{
		OutRequest.DamageCause = FName(*DamageCause);
	}

	OutReason.Reset();
	return true;
}

bool BuildActivationPresentationPublicBoardSummaryFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FWBPublicBoardSummary& OutSummary,
	FString& OutReason)
{
	OutSummary = WBPublicBoardSummary::Build(State);

	TArray<int32> ExcludedUnitIds;
	if (!ReadOptionalIntegerArrayField(
		Fixture,
		TEXT("presentation_public_summary_excluded_unit_ids"),
		TEXT("presentation_public_summary_excluded_unit_ids"),
		ExcludedUnitIds,
		OutReason))
	{
		return false;
	}

	if (ExcludedUnitIds.Num() > 0)
	{
		OutSummary.Units.RemoveAll(
			[&ExcludedUnitIds](const FWBPublicUnitBoardSummary& Unit)
			{
				return ExcludedUnitIds.Contains(Unit.UnitId);
			});
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixtureOperation(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	EWBFixtureOperationKind& OutOperationKind,
	FString& OutReason)
{
	OutOperationKind = EWBFixtureOperationKind::Unknown;

	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	if (OperationKind == TEXT("apply_attack_declare"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyAttackDeclare;
		FWBAction Action;
		if (!ParseActionFromFixture(Fixture, State, Action, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyAttackDeclare(State, Action);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_pending_attack_damage"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyPendingAttackDamage;
		OutResult = WBEffectRunner::ApplyPendingAttackDamage(State);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_zero_hp_death_removal"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyZeroHPDeathRemoval;
		OutResult = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("resolve_damage_request"))
	{
		OutOperationKind = EWBFixtureOperationKind::ResolveDamageRequest;
		FWBDamageRequest DamageRequest;
		if (!ParseDamageRequestFromFixture(Fixture, DamageRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBDamageResolutionResult DamageResult = WBDamageResolution::ResolveDamageRequest(State, DamageRequest);
		OutResult.bOk = DamageResult.bOk;
		OutResult.Reason = DamageResult.Reason;
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_armor_effect"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyArmorEffect;
		FWBArmorEffectRequest ArmorRequest;
		if (!ParseArmorEffectRequestFromFixture(Fixture, ArmorRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyArmorEffect(State, ArmorRequest);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_status_effect"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyStatusEffect;
		FWBStatusEffectRequest StatusRequest;
		if (!ParseStatusEffectRequestFromFixture(Fixture, StatusRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyStatusEffect(State, StatusRequest);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_damage_effect"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyDamageEffect;
		FWBDamageEffectRequest DamageRequest;
		if (!ParseDamageEffectRequestFromFixture(Fixture, DamageRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyDamageEffect(State, DamageRequest);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_heal_effect"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyHealEffect;
		FWBHealEffectRequest HealRequest;
		if (!ParseHealEffectRequestFromFixture(Fixture, HealRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyHealEffect(State, HealRequest);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_effect_request"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyEffectRequest;
		FWBEffectRequest EffectRequest;
		if (!ParseEffectRequestFromFixture(Fixture, EffectRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBEffectRequestResult EffectResult = WBEffectRunner::ApplyEffectRequest(State, EffectRequest);
		OutResult.bOk = EffectResult.bOk;
		OutResult.Reason = EffectResult.Reason;
		OutResult.TraceEvents = EffectResult.TraceEvents;
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_card_activation_command"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyCardActivationCommand;
		FWBCardActivationCommand Command;
		if (!ParseCardActivationCommandFromFixture(Fixture, Command, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationCommandResult CommandResult = WBEffectRunner::ApplyCardActivationCommand(State, Command);
		OutResult.bOk = CommandResult.bOk;
		OutResult.Reason = CommandResult.Reason;
		OutResult.TraceEvents = CommandResult.TraceEvents;
		if (!ExpectActivationCostPaymentVerifierExpectations(Fixture, State, OutResult.TraceEvents, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("build_card_activation_command"))
	{
		OutOperationKind = EWBFixtureOperationKind::BuildCardActivationCommand;
		FWBCardActivationExpansionRequest Request;
		if (!ParseCardActivationExpansionRequestFromFixture(Fixture, Request, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationExpansionResult ExpansionResult = WBCardActivationExpansion::BuildActivationCommand(Request);
		OutResult.bOk = ExpansionResult.bOk;
		OutResult.Reason = ExpansionResult.Reason;
		OutResult.TraceEvents.Reset();
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("build_and_apply_card_activation_command"))
	{
		OutOperationKind = EWBFixtureOperationKind::BuildAndApplyCardActivationCommand;
		FWBCardActivationExpansionRequest Request;
		if (!ParseCardActivationExpansionRequestFromFixture(Fixture, Request, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationExpansionResult ExpansionResult = WBCardActivationExpansion::BuildActivationCommand(Request);
		if (!ExpansionResult.bOk)
		{
			OutResult.bOk = false;
			OutResult.Reason = ExpansionResult.Reason;
			OutResult.TraceEvents.Reset();
			if (!ExpectActivationCostPaymentVerifierExpectations(Fixture, State, OutResult.TraceEvents, OutReason))
			{
				OutResult = MakeFixtureFailure(OutReason);
				return false;
			}

			OutReason.Reset();
			return true;
		}

		const FWBCardActivationCommandResult CommandResult = WBEffectRunner::ApplyCardActivationCommand(
			State,
			ExpansionResult.Command);
		OutResult.bOk = CommandResult.bOk;
		OutResult.Reason = CommandResult.Reason;
		OutResult.TraceEvents = CommandResult.TraceEvents;
		if (!ExpectActivationCostPaymentVerifierExpectations(Fixture, State, OutResult.TraceEvents, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("evaluate_card_activation_source_gate"))
	{
		OutOperationKind = EWBFixtureOperationKind::EvaluateCardActivationSourceGate;
		FWBCardActivationSourceGateDefinition Gate;
		if (!ParseOptionalCardActivationSourceGateDefinitionField(
			OperationObject,
			TEXT("source_gate"),
			Gate,
			OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		FWBCardActivationSourceGateContext Context;
		if (!ParseOptionalCardActivationSourceGateContextField(
			OperationObject,
			TEXT("source_gate_context"),
			Context,
			OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationSourceGateResult GateResult =
			WBCardActivationSourceGate::Evaluate(State, Gate, Context);
		OutResult.bOk = GateResult.bOk;
		OutResult.Reason = GateResult.Reason;
		OutResult.TraceEvents.Reset();
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("query_card_activation_affordability"))
	{
		OutOperationKind = EWBFixtureOperationKind::QueryCardActivationAffordability;
		FWBCardActivationAffordabilityRequest Request;
		if (!ParseCardActivationAffordabilityRequestFromFixture(Fixture, Request, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationAffordabilityResult QueryResult =
			WBCardActivationAffordability::QueryFromUnitRL(State, Request);
		OutResult.bOk = QueryResult.bOk;
		OutResult.Reason = QueryResult.Reason;
		OutResult.TraceEvents.Reset();
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("generate_card_activation_candidates"))
	{
		OutOperationKind = EWBFixtureOperationKind::GenerateCardActivationCandidates;
		TArray<FWBCardActivationCandidateSource> Sources;
		if (!ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationCandidateGenerationResult GenerationResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		OutResult.bOk = GenerationResult.bOk;
		OutResult.Reason = GenerationResult.Reason;
		OutResult.TraceEvents.Reset();
		if (GenerationResult.bOk
			&& !ValidateCardActivationCandidateFixtureExpectations(Fixture, GenerationResult, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("generate_card_activation_legal_actions"))
	{
		OutOperationKind = EWBFixtureOperationKind::GenerateCardActivationLegalActions;
		TArray<FWBCardActivationCandidateSource> Sources;
		if (!ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationCandidateGenerationResult CandidateResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		if (!CandidateResult.bOk)
		{
			OutResult.bOk = false;
			OutResult.Reason = CandidateResult.Reason;
			OutResult.TraceEvents.Reset();
			OutReason.Reset();
			return true;
		}

		if (!ValidateCardActivationCandidateFixtureExpectations(Fixture, CandidateResult, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationLegalActionGenerationResult GenerationResult =
			WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
		OutResult.bOk = GenerationResult.bOk;
		OutResult.Reason = GenerationResult.Reason;
		OutResult.TraceEvents.Reset();
		if (GenerationResult.bOk
			&& !ValidateCardActivationLegalActionFixtureExpectations(Fixture, GenerationResult, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		if (!ExpectActivationCostPaymentVerifierExpectations(Fixture, State, OutResult.TraceEvents, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("build_card_activation_presentation_snapshot"))
	{
		OutOperationKind = EWBFixtureOperationKind::BuildCardActivationPresentationSnapshot;
		TArray<FWBCardActivationCandidateSource> Sources;
		if (!ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationCandidateGenerationResult CandidateResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		if (!CandidateResult.bOk)
		{
			OutResult.bOk = false;
			OutResult.Reason = CandidateResult.Reason;
			OutResult.TraceEvents.Reset();
			OutReason.Reset();
			return true;
		}

		if (!ValidateCardActivationCandidateFixtureExpectations(Fixture, CandidateResult, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationLegalActionGenerationResult GenerationResult =
			WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
		OutResult.bOk = GenerationResult.bOk;
		OutResult.Reason = GenerationResult.Reason;
		OutResult.TraceEvents.Reset();
		if (!GenerationResult.bOk)
		{
			OutReason.Reset();
			return true;
		}

		if (!ValidateCardActivationLegalActionFixtureExpectations(Fixture, GenerationResult, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		FWBPublicBoardSummary PublicBoardSummary;
		if (!BuildActivationPresentationPublicBoardSummaryFromFixture(
			Fixture,
			State,
			PublicBoardSummary,
			OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationLegalActionPresentationSnapshot Snapshot =
			WBCardActivationLegalActionPresentation::BuildSnapshot(
				GenerationResult.ActionSet,
				PublicBoardSummary);
		if (!ValidateCardActivationPresentationFixtureExpectations(Fixture, Snapshot, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_card_activation_legal_action_by_id"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyCardActivationLegalActionById;
		TArray<FWBCardActivationCandidateSource> Sources;
		if (!ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		FString SelectedActivationActionId;
		if (!ParseSelectedActivationActionId(Fixture, OperationObject, SelectedActivationActionId, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationCandidateGenerationResult CandidateResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		if (!CandidateResult.bOk)
		{
			OutResult.bOk = false;
			OutResult.Reason = CandidateResult.Reason;
			OutResult.TraceEvents.Reset();
			OutReason.Reset();
			return true;
		}

		if (!ValidateCardActivationCandidateFixtureExpectations(Fixture, CandidateResult, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationLegalActionGenerationResult GenerationResult =
			WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
		if (!GenerationResult.bOk)
		{
			OutResult.bOk = false;
			OutResult.Reason = GenerationResult.Reason;
			OutResult.TraceEvents.Reset();
			OutReason.Reset();
			return true;
		}

		if (!ValidateCardActivationLegalActionFixtureExpectations(Fixture, GenerationResult, OutReason)
			|| !ValidateSelectedActivationActionIdExpectation(Fixture, SelectedActivationActionId, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBCardActivationLegalActionReplayResult ReplayResult =
			FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(
				State,
				GenerationResult.ActionSet,
				SelectedActivationActionId);
		OutResult.bOk = ReplayResult.ActivationResult.bOk;
		OutResult.Reason = ReplayResult.ActivationResult.Reason;
		OutResult.TraceEvents = ReplayResult.ActivationResult.TraceEvents;

		if (!ExpectActivationCostPaymentVerifierExpectations(Fixture, State, OutResult.TraceEvents, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_start_turn_status_ticks"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyStartOfTurnStatusTicks;
		int32 PlayerId = -1;
		if (!TryReadIntegerField(OperationObject, TEXT("player_id"), PlayerId))
		{
			OutReason = TEXT("missing_operation_player_id");
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, PlayerId);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_end_turn_status_ticks"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyEndOfTurnStatusTicks;
		int32 PlayerId = -1;
		if (!TryReadIntegerField(OperationObject, TEXT("player_id"), PlayerId))
		{
			OutReason = TEXT("missing_operation_player_id");
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, PlayerId);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("attack_declare_then_damage"))
	{
		OutOperationKind = EWBFixtureOperationKind::AttackDeclareThenDamage;
		FWBAction Action;
		if (!ParseActionFromFixture(Fixture, State, Action, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		FWBApplyActionResult DeclareResult = WBEffectRunner::ApplyAttackDeclare(State, Action);
		OutResult = DeclareResult;
		if (!DeclareResult.bOk)
		{
			OutReason.Reset();
			return true;
		}

		FWBApplyActionResult DamageResult = WBEffectRunner::ApplyPendingAttackDamage(State);
		OutResult.bOk = DamageResult.bOk;
		OutResult.Reason = DamageResult.Reason;
		OutResult.TraceEvents.Append(DamageResult.TraceEvents);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_turn_command"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyTurnCommand;
		return ApplyTurnCommandFixture(Fixture, State, OutResult, OutReason);
	}

	if (OperationKind == TEXT("apply_runtime_selected_action"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyRuntimeSelectedAction;
		int32 RollSourceRemainingCount = -1;
		return ApplyRuntimeSelectedActionFixture(Fixture, State, OutResult, RollSourceRemainingCount, OutReason);
	}

	if (OperationKind == TEXT("apply_runtime_selected_action_with_result"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyRuntimeSelectedActionWithResult;
		FWBRuntimeSelectedActionResult Envelope;
		int32 RollSourceRemainingCount = -1;
		const bool bApplied = ApplyRuntimeSelectedActionWithResultFixture(
			Fixture,
			State,
			Envelope,
			RollSourceRemainingCount,
			OutReason);
		OutResult = Envelope.ApplyResult;
		return bApplied;
	}

	if (OperationKind == TEXT("apply_runtime_selected_action_with_result_and_serialize"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyRuntimeSelectedActionWithResultAndSerialize;
		FWBRuntimeSelectedActionResult Envelope;
		FString SerializedJson;
		int32 RollSourceRemainingCount = -1;
		const bool bApplied = ApplyRuntimeSelectedActionWithResultAndSerializeFixture(
			Fixture,
			State,
			Envelope,
			SerializedJson,
			RollSourceRemainingCount,
			OutReason);
		OutResult = Envelope.ApplyResult;
		return bApplied;
	}

	if (OperationKind == TEXT("apply_action"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyAction;
		const TSharedPtr<FJsonObject>* ActionObject = nullptr;
		if (!Fixture.IsValid()
			|| !Fixture->TryGetObjectField(TEXT("action"), ActionObject)
			|| ActionObject == nullptr
			|| !ActionObject->IsValid())
		{
			OutReason = TEXT("missing_action");
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(SerializeJsonObject(*ActionObject), State);
		if (!DecodeResult.bOk)
		{
			OutReason = DecodeResult.Reason;
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyAction(State, DecodeResult.Action);
		OutReason.Reset();
		return true;
	}

	OutReason = FString::Printf(TEXT("unsupported_fixture_operation:%s"), *OperationKind);
	OutResult = MakeFixtureFailure(OutReason);
	return false;
}

bool ExpectTraceOrder(
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<FWBTraceEvent>& TraceEvents,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* TraceOrderValues = nullptr;
	if (!ExpectedObject->TryGetArrayField(TEXT("expected_trace_order"), TraceOrderValues) || TraceOrderValues == nullptr)
	{
		OutReason = TEXT("missing_expected_trace_order");
		return false;
	}

	TArray<FString> ExpectedTraceOrder;
	if (!ReadStringArrayValues(*TraceOrderValues, TEXT("expected.expected_trace_order"), ExpectedTraceOrder, OutReason))
	{
		return false;
	}

	if (TraceEvents.Num() != ExpectedTraceOrder.Num())
	{
		OutReason = FString::Printf(TEXT("trace_count_expected_%d_got_%d"), ExpectedTraceOrder.Num(), TraceEvents.Num());
		return false;
	}

	for (int32 Index = 0; Index < ExpectedTraceOrder.Num(); ++Index)
	{
		if (!TraceEventMatchesLabel(TraceEvents[Index], ExpectedTraceOrder[Index]))
		{
			OutReason = FString::Printf(
				TEXT("trace_order_%d_expected_%s_got_%s:%s"),
				Index,
				*ExpectedTraceOrder[Index],
				*TraceEvents[Index].Kind.ToString(),
				*TraceEvents[Index].StatusId.ToString());
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectPlayerResources(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PlayerResourcesObject = nullptr;
	if (!ExpectedObject->TryGetObjectField(TEXT("player_resources"), PlayerResourcesObject)
		|| PlayerResourcesObject == nullptr
		|| !PlayerResourcesObject->IsValid())
	{
		OutReason = TEXT("missing_expected_player_resources");
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& PlayerResourcePair : (*PlayerResourcesObject)->Values)
	{
		const int32 PlayerId = FCString::Atoi(*PlayerResourcePair.Key);
		const TSharedPtr<FJsonObject> PlayerResourceObject = PlayerResourcePair.Value.IsValid()
			? PlayerResourcePair.Value->AsObject()
			: nullptr;
		const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
		if (Player == nullptr || !PlayerResourceObject.IsValid())
		{
			OutReason = FString::Printf(TEXT("missing_expected_player_resource_%d"), PlayerId);
			return false;
		}

		if (Player->RemainingMP != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("remaining_mp"), Player->RemainingMP)
			|| Player->LastMPRoll != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("last_mp_roll"), Player->LastMPRoll)
			|| Player->WallsLeft != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("walls_left"), Player->WallsLeft)
			|| Player->WallRemovalsLeft != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("wall_removals_left"), Player->WallRemovalsLeft))
		{
			OutReason = FString::Printf(TEXT("player_%d_resource_mismatch"), PlayerId);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectUnitStatusSummary(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* FinalUnits = nullptr;
	if (!ExpectedObject->TryGetArrayField(TEXT("final_units"), FinalUnits) || FinalUnits == nullptr)
	{
		OutReason = TEXT("missing_expected_final_units");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FinalUnitValue : *FinalUnits)
	{
		const TSharedPtr<FJsonObject> FinalUnitObject = FinalUnitValue.IsValid() ? FinalUnitValue->AsObject() : nullptr;
		int32 UnitId = -1;
		if (!FinalUnitObject.IsValid() || !TryReadIntegerField(FinalUnitObject, TEXT("id"), UnitId))
		{
			OutReason = TEXT("malformed_expected_final_unit");
			return false;
		}

		const FWBUnitState* Unit = State.GetUnitById(UnitId);
		if (Unit == nullptr)
		{
			OutReason = FString::Printf(TEXT("missing_unit_%d"), UnitId);
			return false;
		}

		if (Unit->X != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("x"), Unit->X)
			|| Unit->Y != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("y"), Unit->Y)
			|| Unit->HP != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("hp"), Unit->HP)
			|| Unit->MaxHP != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("max_hp"), Unit->MaxHP)
			|| Unit->GetCurrentArmor() != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("current_armor"), Unit->GetCurrentArmor())
			|| Unit->GetMaxArmor() != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("max_armor"), Unit->GetMaxArmor())
			|| Unit->RLTotal != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("rl_total"), Unit->RLTotal)
			|| Unit->RLUsed != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("rl_used"), Unit->RLUsed)
			|| Unit->AttacksLeft != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("attacks_left"), Unit->AttacksLeft)
			|| Unit->bDefeated != ReadBoolFieldOrDefault(FinalUnitObject, TEXT("defeated"), Unit->bDefeated)
			|| Unit->bRemovedFromBoard != ReadBoolFieldOrDefault(FinalUnitObject, TEXT("removed_from_board"), Unit->bRemovedFromBoard))
		{
			OutReason = FString::Printf(TEXT("unit_%d_state_mismatch"), UnitId);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Statuses = nullptr;
		if (FinalUnitObject->TryGetArrayField(TEXT("statuses"), Statuses) && Statuses != nullptr)
		{
			if (Unit->Statuses.Num() != Statuses->Num())
			{
				OutReason = FString::Printf(TEXT("unit_%d_status_count_mismatch"), UnitId);
				return false;
			}

			for (const TSharedPtr<FJsonValue>& StatusValue : *Statuses)
			{
				if (!StatusValue.IsValid()
					|| StatusValue->Type != EJson::String
					|| !Unit->HasStatus(FName(*StatusValue->AsString())))
				{
					OutReason = FString::Printf(TEXT("unit_%d_status_mismatch"), UnitId);
					return false;
				}
			}
		}

		const TSharedPtr<FJsonObject>* StatusTurns = nullptr;
		if (FinalUnitObject->TryGetObjectField(TEXT("status_turns"), StatusTurns)
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

bool ExpectActivationCostPaymentVerifierExpectations(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	const TArray<FWBTraceEvent>& TraceEvents,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetOptionalExpectedObject(Fixture, ExpectedObject))
	{
		OutReason.Reset();
		return true;
	}

	if (!ValidateExpectedRLState(ExpectedObject, State, OutReason)
		|| !ValidateExpectedCostPaidTrace(ExpectedObject, TraceEvents, OutReason)
		|| !ValidateForbiddenTraceSubstrings(ExpectedObject, TraceEvents, OutReason))
	{
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ExpectFinalTurnState(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	int32 ExpectedCurrentPlayer = State.CurrentPlayer;
	int32 ExpectedPriorityPlayer = State.PriorityPlayer;
	int32 ExpectedTurnNumber = State.TurnNumber;
	int32 ExpectedWinnerPlayerId = State.WinnerPlayerId;
	EWBGamePhase ExpectedPhase = State.Phase;

	if (ReadExpectedFinalIntegerField(ExpectedObject, TEXT("final_current_player"), TEXT("current_player"), ExpectedCurrentPlayer)
		&& State.CurrentPlayer != ExpectedCurrentPlayer)
	{
		OutReason = TEXT("final_current_player_mismatch");
		return false;
	}

	if (ReadExpectedFinalIntegerField(ExpectedObject, TEXT("final_priority_player"), TEXT("priority_player"), ExpectedPriorityPlayer)
		&& State.PriorityPlayer != ExpectedPriorityPlayer)
	{
		OutReason = TEXT("final_priority_player_mismatch");
		return false;
	}

	if (ReadExpectedFinalIntegerField(ExpectedObject, TEXT("final_turn_number"), TEXT("turn_number"), ExpectedTurnNumber)
		&& State.TurnNumber != ExpectedTurnNumber)
	{
		OutReason = TEXT("final_turn_number_mismatch");
		return false;
	}

	if (ReadExpectedFinalPhase(ExpectedObject, ExpectedPhase) && State.Phase != ExpectedPhase)
	{
		OutReason = TEXT("final_phase_mismatch");
		return false;
	}

	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (ExpectedObject->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		&& FinalStateObject != nullptr
		&& FinalStateObject->IsValid())
	{
		const bool bExpectedGameOver = ReadBoolFieldOrDefault(*FinalStateObject, TEXT("game_over"), State.bGameOver);
		if (State.bGameOver != bExpectedGameOver)
		{
			OutReason = TEXT("final_game_over_mismatch");
			return false;
		}

		if (TryReadIntegerField(*FinalStateObject, TEXT("winner_player_id"), ExpectedWinnerPlayerId)
			&& State.WinnerPlayerId != ExpectedWinnerPlayerId)
		{
			OutReason = TEXT("final_winner_player_id_mismatch");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectRuntimeRollSourceRemainingCount(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 RollSourceRemainingCount,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	int32 ExpectedRemainingCount = -1;
	if (!TryReadIntegerField(ExpectedObject, TEXT("roll_source_remaining_count"), ExpectedRemainingCount))
	{
		OutReason.Reset();
		return true;
	}

	if (RollSourceRemainingCount != ExpectedRemainingCount)
	{
		OutReason = FString::Printf(
			TEXT("roll_source_remaining_count_expected_%d_got_%d"),
			ExpectedRemainingCount,
			RollSourceRemainingCount);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ExpectRuntimeSelectedActionEnvelope(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBRuntimeSelectedActionResult& Envelope,
	const int32 RollSourceRemainingCount,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	FString ExpectedSelectedActionId;
	if (ExpectedObject->TryGetStringField(TEXT("selected_action_id"), ExpectedSelectedActionId)
		&& Envelope.SelectedActionId != ExpectedSelectedActionId)
	{
		OutReason = TEXT("selected_action_id_mismatch");
		return false;
	}

	FString ExpectedSelectedActionType;
	if (ExpectedObject->TryGetStringField(TEXT("selected_action_type"), ExpectedSelectedActionType)
		&& Envelope.SelectedActionType.ToString() != ExpectedSelectedActionType)
	{
		OutReason = TEXT("selected_action_type_mismatch");
		return false;
	}

	bool bExpectedConsumedMPRoll = Envelope.bConsumedMPRoll;
	if (ExpectedObject->TryGetBoolField(TEXT("consumed_mp_roll"), bExpectedConsumedMPRoll)
		&& Envelope.bConsumedMPRoll != bExpectedConsumedMPRoll)
	{
		OutReason = TEXT("consumed_mp_roll_mismatch");
		return false;
	}

	int32 ExpectedConsumedMPRollValue = Envelope.ConsumedMPRoll;
	if (TryReadIntegerField(ExpectedObject, TEXT("consumed_mp_roll_value"), ExpectedConsumedMPRollValue)
		&& Envelope.ConsumedMPRoll != ExpectedConsumedMPRollValue)
	{
		OutReason = TEXT("consumed_mp_roll_value_mismatch");
		return false;
	}

	if (!ExpectFinalPublicTurnSummary(ExpectedObject, Envelope.FinalPublicTurnSummary, OutReason))
	{
		return false;
	}

	if (!ExpectRuntimeRollSourceRemainingCount(Fixture, RollSourceRemainingCount, OutReason))
	{
		return false;
	}

	if (!ExpectTraceOrder(Fixture, Envelope.ApplyResult.TraceEvents, OutReason))
	{
		return false;
	}

	OutReason.Reset();
	return true;
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
