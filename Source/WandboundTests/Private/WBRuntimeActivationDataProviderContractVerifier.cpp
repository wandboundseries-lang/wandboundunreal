#include "WBRuntimeActivationDataProviderContractVerifier.h"

#include "WBActionCodec.h"

namespace
{
int32 RequiredMinimum(const bool bRequired, const int32 ExpectedMinimum)
{
	return bRequired ? FMath::Max(ExpectedMinimum, 1) : ExpectedMinimum;
}

FString RequestKindToContractString(const EWBRuntimeActivationDataRequestKind Kind)
{
	switch (Kind)
	{
	case EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint:
		return TEXT("current_decision_point");
	case EWBRuntimeActivationDataRequestKind::PostActivation:
		return TEXT("post_activation");
	case EWBRuntimeActivationDataRequestKind::Unknown:
	default:
		return TEXT("unknown");
	}
}

bool IsAcceptedRequestKind(const EWBRuntimeActivationDataRequestKind Kind)
{
	return Kind == EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint
		|| Kind == EWBRuntimeActivationDataRequestKind::PostActivation;
}

bool ContainsForbiddenSubstring(
	const FString& Text,
	const TArray<FString>& ForbiddenSubstrings,
	FString& OutToken)
{
	for (const FString& Token : ForbiddenSubstrings)
	{
		if (!Token.IsEmpty() && Text.Contains(Token, ESearchCase::IgnoreCase))
		{
			OutToken = Token;
			return true;
		}
	}

	OutToken.Reset();
	return false;
}

void AppendActivationActionDebugString(
	FString& OutDebug,
	const FWBCardActivationLegalAction& Action)
{
	OutDebug += TEXT("|activation=");
	OutDebug += Action.ActivationActionId;
	OutDebug += TEXT(":label=");
	OutDebug += Action.PublicLabel;
	OutDebug += TEXT(":player=");
	OutDebug += FString::FromInt(Action.PlayerId);
	OutDebug += TEXT(":source=");
	OutDebug += FString::FromInt(Action.SourceUnitId);
	OutDebug += TEXT(":target=");
	OutDebug += FString::FromInt(Action.Target.TargetUnitId);
}

void AppendPublicSummaryDebugString(
	FString& OutDebug,
	const FWBPublicBoardSummary& Summary)
{
	OutDebug += TEXT("|public_units=");
	OutDebug += FString::FromInt(Summary.Units.Num());
	for (const FWBPublicUnitBoardSummary& Unit : Summary.Units)
	{
		OutDebug += TEXT(":unit=");
		OutDebug += FString::FromInt(Unit.UnitId);
		OutDebug += TEXT(":owner=");
		OutDebug += FString::FromInt(Unit.OwnerId);
		OutDebug += TEXT(":card=");
		OutDebug += Unit.CardId;
		OutDebug += TEXT(":x=");
		OutDebug += FString::FromInt(Unit.X);
		OutDebug += TEXT(":y=");
		OutDebug += FString::FromInt(Unit.Y);
	}
}
}

bool FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
	const FWBRuntimeActivationDataProviderResult& Result,
	const FWBRuntimeActivationProviderContractExpectations& Expectations,
	FString& OutReason)
{
	if (Expectations.bRequireOk && !Result.bOk)
	{
		OutReason = Result.Reason.IsEmpty()
			? FString(TEXT("provider_result_not_ok"))
			: FString::Printf(TEXT("provider_result_not_ok:%s"), *Result.Reason);
		return false;
	}

	if (Result.bOk && !IsAcceptedRequestKind(Result.Request.Kind))
	{
		OutReason = TEXT("provider_request_kind_unknown");
		return false;
	}

	if (Result.bOk && Result.Request.PlayerId < 0)
	{
		OutReason = TEXT("provider_player_id_missing");
		return false;
	}

	if (Result.bOk && Result.Request.DecisionPointId.IsEmpty())
	{
		OutReason = TEXT("provider_decision_point_id_missing");
		return false;
	}

	if (Result.bOk
		&& Result.Request.Kind == EWBRuntimeActivationDataRequestKind::PostActivation
		&& Result.Request.SelectedActivationActionId.IsEmpty())
	{
		OutReason = TEXT("provider_selected_activation_action_id_missing");
		return false;
	}

	const int32 MinNormalActions = RequiredMinimum(
		Expectations.bRequireNormalLegalActions,
		Expectations.ExpectedMinNormalLegalActions);
	if (Result.RefreshInput.NormalLegalActions.Num() < MinNormalActions)
	{
		OutReason = TEXT("normal_legal_actions_missing");
		return false;
	}

	const int32 MinActivationActions = RequiredMinimum(
		Expectations.bRequireActivationActions,
		Expectations.ExpectedMinActivationActions);
	if (Result.RefreshInput.ActivationActionSet.Actions.Num() < MinActivationActions)
	{
		OutReason = TEXT("activation_actions_missing");
		return false;
	}

	const int32 MinPublicUnits = RequiredMinimum(
		Expectations.bRequirePublicBoardSummary,
		Expectations.ExpectedMinPublicUnits);
	if (Result.RefreshInput.PublicBoardSummary.Units.Num() < MinPublicUnits)
	{
		OutReason = TEXT("public_board_summary_missing");
		return false;
	}

	for (int32 ActionIndex = 0; ActionIndex < Result.RefreshInput.ActivationActionSet.Actions.Num(); ++ActionIndex)
	{
		const FWBCardActivationLegalAction& Action =
			Result.RefreshInput.ActivationActionSet.Actions[ActionIndex];
		if (Action.ActivationActionId.IsEmpty())
		{
			OutReason = FString::Printf(TEXT("activation_action_id_missing:%d"), ActionIndex);
			return false;
		}
	}

	if (Expectations.bRequireNoHiddenSubstrings && !Expectations.ForbiddenSubstrings.IsEmpty())
	{
		FString DebugString;
		if (!ProviderResultToDebugStringForTest(Result, DebugString))
		{
			OutReason = TEXT("provider_debug_string_failed");
			return false;
		}

		FString ForbiddenToken;
		if (ContainsForbiddenSubstring(DebugString, Expectations.ForbiddenSubstrings, ForbiddenToken))
		{
			OutReason = FString::Printf(TEXT("hidden_substring_found:%s"), *ForbiddenToken);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool FWBRuntimeActivationDataProviderContractVerifier::VerifyDeterministicProviderOutput(
	const IWBRuntimeActivationDataProvider& Provider,
	const FWBRuntimeActivationDataProviderRequest& Request,
	FString& OutReason)
{
	const FWBRuntimeActivationDataProviderResult First = Provider.GetActivationDecisionData(Request);
	const FWBRuntimeActivationDataProviderResult Second = Provider.GetActivationDecisionData(Request);

	FString FirstDebug;
	FString SecondDebug;
	if (!ProviderResultToDebugStringForTest(First, FirstDebug)
		|| !ProviderResultToDebugStringForTest(Second, SecondDebug))
	{
		OutReason = TEXT("provider_debug_string_failed");
		return false;
	}

	if (FirstDebug != SecondDebug)
	{
		OutReason = TEXT("provider_output_not_deterministic");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool FWBRuntimeActivationDataProviderContractVerifier::ProviderResultToDebugStringForTest(
	const FWBRuntimeActivationDataProviderResult& Result,
	FString& OutDebugString)
{
	OutDebugString.Reset();
	OutDebugString += TEXT("ok=");
	OutDebugString += Result.bOk ? TEXT("true") : TEXT("false");
	OutDebugString += TEXT("|reason=");
	OutDebugString += Result.Reason;
	OutDebugString += TEXT("|kind=");
	OutDebugString += RequestKindToContractString(Result.Request.Kind);
	OutDebugString += TEXT("|player=");
	OutDebugString += FString::FromInt(Result.Request.PlayerId);
	OutDebugString += TEXT("|decision=");
	OutDebugString += Result.Request.DecisionPointId;
	OutDebugString += TEXT("|selected=");
	OutDebugString += Result.Request.SelectedActivationActionId;

	OutDebugString += TEXT("|normal_actions=");
	OutDebugString += FString::FromInt(Result.RefreshInput.NormalLegalActions.Num());
	for (const FWBAction& Action : Result.RefreshInput.NormalLegalActions)
	{
		OutDebugString += TEXT(":action=");
		OutDebugString += WBActionCodec::MakeActionId(Action);
	}

	OutDebugString += TEXT("|activation_actions=");
	OutDebugString += FString::FromInt(Result.RefreshInput.ActivationActionSet.Actions.Num());
	for (const FWBCardActivationLegalAction& Action : Result.RefreshInput.ActivationActionSet.Actions)
	{
		AppendActivationActionDebugString(OutDebugString, Action);
	}

	AppendPublicSummaryDebugString(OutDebugString, Result.RefreshInput.PublicBoardSummary);
	return true;
}
