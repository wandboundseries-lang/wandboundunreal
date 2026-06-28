#include "WBProductionActivationExecutionHandoff.h"

#include "WBCardActivationExpansion.h"
#include "WBRuntimeActivationExecutionBridge.h"
#include "WBRuntimeActivationExecutionHandoff.h"

namespace
{
FWBRuntimeActivationDataProviderRequest MakeProviderRequest(
	const EWBRuntimeActivationDataRequestKind Kind,
	const int32 ViewerPlayerId,
	const FString& SelectedActivationActionId,
	const TCHAR* Reason)
{
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = Kind;
	Request.PlayerId = ViewerPlayerId;
	Request.DecisionPointId = TEXT("production_activation_execution_handoff");
	Request.SelectedActivationActionId = SelectedActivationActionId;
	Request.RequestReason = Reason;
	return Request;
}

FWBProductionActivationExecutionHandoffResult MakeFailure(
	const EWBProductionActivationExecutionHandoffResultCode Code,
	const FString& Reason = FString())
{
	FWBProductionActivationExecutionHandoffResult Result;
	Result.bOk = false;
	Result.Code = Code;
	Result.Reason = Reason.IsEmpty()
		? FWBProductionActivationExecutionHandoff::ResultCodeToString(Code)
		: Reason;
	return Result;
}

FString SourceCardIdFromAction(const FWBCardActivationLegalAction& Action)
{
	if (!Action.Candidate.SourceCardId.IsEmpty())
	{
		return Action.Candidate.SourceCardId;
	}

	return Action.Command.Source.SourceCardId;
}

FString EffectIdFromAction(const FWBCardActivationLegalAction& Action)
{
	if (!Action.Candidate.SourceEffectId.IsEmpty())
	{
		return Action.Candidate.SourceEffectId;
	}

	return Action.Command.Source.SourceEffectId;
}

bool MatchesRequestedSourceInstance(
	const FWBCardActivationLegalAction& Action,
	const FString& SourceInstanceId)
{
	if (SourceInstanceId.IsEmpty())
	{
		return true;
	}

	const FString HandInstancePart = FString::Printf(TEXT(":i%s:"), *SourceInstanceId);
	return Action.ActivationActionId.Contains(HandInstancePart, ESearchCase::CaseSensitive);
}

bool MatchesSelectionRequest(
	const FWBCardActivationLegalAction& Action,
	const FWBProductionActivationTargetSelectionRequest& Request)
{
	if (!Request.ActivationEntryId.IsEmpty()
		&& Action.ActivationActionId != Request.ActivationEntryId)
	{
		return false;
	}

	if (!Request.SourceCardId.IsEmpty()
		&& SourceCardIdFromAction(Action) != Request.SourceCardId)
	{
		return false;
	}

	if (!Request.EffectId.IsEmpty()
		&& EffectIdFromAction(Action) != Request.EffectId)
	{
		return false;
	}

	return MatchesRequestedSourceInstance(Action, Request.SourceInstanceId);
}

const FWBCardActivationLegalAction* FindSingleMatchingAction(
	const TArray<FWBCardActivationLegalAction>& Actions,
	const FWBProductionActivationTargetSelectionRequest& Request,
	bool& bOutAmbiguous)
{
	const FWBCardActivationLegalAction* MatchedAction = nullptr;
	bOutAmbiguous = false;

	for (const FWBCardActivationLegalAction& Action : Actions)
	{
		if (!MatchesSelectionRequest(Action, Request))
		{
			continue;
		}

		if (MatchedAction != nullptr)
		{
			bOutAmbiguous = true;
			return nullptr;
		}

		MatchedAction = &Action;
	}

	return MatchedAction;
}

EWBProductionActivationExecutionHandoffResultCode MapSelectionFailureCode(
	const EWBProductionActivationTargetSelectionResultCode Code)
{
	switch (Code)
	{
	case EWBProductionActivationTargetSelectionResultCode::ActivationEntryMissing:
		return EWBProductionActivationExecutionHandoffResultCode::ActivationEntryMissing;
	case EWBProductionActivationTargetSelectionResultCode::UnsupportedTargetRequirement:
		return EWBProductionActivationExecutionHandoffResultCode::UnsupportedTargetRequirement;
	case EWBProductionActivationTargetSelectionResultCode::HiddenTargetRejected:
		return EWBProductionActivationExecutionHandoffResultCode::HiddenDataRejected;
	default:
		return EWBProductionActivationExecutionHandoffResultCode::SelectionFailed;
	}
}

EWBCardActivationSourceZone InferSourceZone(const FWBCardActivationLegalAction& Action)
{
	if (Action.ActivationActionId.Contains(TEXT(":zhand:"), ESearchCase::CaseSensitive))
	{
		return EWBCardActivationSourceZone::Hand;
	}

	if (Action.ActivationActionId.Contains(TEXT(":zboard:"), ESearchCase::CaseSensitive))
	{
		return EWBCardActivationSourceZone::Board;
	}

	return Action.SourceUnitId == -1
		? EWBCardActivationSourceZone::Hand
		: EWBCardActivationSourceZone::Board;
}

FWBCardActivationSourceGateContext MakeSourceGateContextForAction(
	const FWBCardActivationLegalAction& Action)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = Action.PlayerId;
	Context.SourceUnitId = Action.SourceUnitId;
	Context.SourceCardId = SourceCardIdFromAction(Action);
	Context.SourceZone = InferSourceZone(Action);
	Context.bCostsSatisfiedExternally = true;
	Context.bHasExplicitSourceGateContext = true;

	if (Context.SourceZone == EWBCardActivationSourceZone::Hand)
	{
		FWBCardActivationFixtureZoneEntry Entry;
		Entry.CardId = Context.SourceCardId;
		Entry.OwnerPlayerId = Action.PlayerId;
		Entry.Zone = EWBCardActivationSourceZone::Hand;
		Context.FixtureZoneContext.Entries.Add(Entry);
	}

	return Context;
}

FWBCardActivationExpansionResult RebuildInternalCommand(
	const FWBCardActivationLegalAction& Action,
	const FWBProductionActivationTargetSelectionResult& SelectionResult,
	const FWBCardDefinitionRepository& Repository)
{
	FWBCardActivationExpansionRequest Request;
	Request.PlayerId = Action.PlayerId;
	Request.SourceUnitId = Action.SourceUnitId;
	Request.EffectId = EffectIdFromAction(Action);
	Request.Target = SelectionResult.Command.EffectRequest.Target;
	Request.SourceGateContext = MakeSourceGateContextForAction(Action);

	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, SourceCardIdFromAction(Action));
	if (Lookup.bFound)
	{
		Request.CardDefinition = Lookup.Definition;
	}

	return WBCardActivationExpansion::BuildActivationCommand(Request);
}

FWBRuntimeActivationExecutionHandoffResult MakeRuntimeExecutionHandoff(
	const FWBCardActivationLegalAction& ProviderAction,
	const FWBCardActivationCommand& Command)
{
	FWBCardActivationLegalAction Action = ProviderAction;
	Action.Command = Command;
	Action.Target = Command.EffectRequest.Target;

	FWBRuntimeActivationExecutionHandoffResult Handoff;
	Handoff.bHandoffOk = true;
	Handoff.bSelectionResolved = true;
	Handoff.bExecutionImplemented = true;
	Handoff.bExecutionAttempted = false;
	Handoff.Reason = TEXT("production_activation_execution_handoff");
	Handoff.SelectedActivationActionId = Action.ActivationActionId;
	Handoff.ActivationAction = Action;
	return Handoff;
}

void AppendSanitizedDiagnostics(
	const FWBProductionActivationDataProvider& Provider,
	TArray<FString>& OutDiagnostics)
{
	for (const FWBProductionActivationDataProviderDiagnostic& Diagnostic : Provider.GetDiagnosticsForTest())
	{
		if (!Diagnostic.Code.IsEmpty())
		{
			OutDiagnostics.Add(Diagnostic.Code);
		}
	}
}
}

FWBProductionActivationExecutionHandoffResult
FWBProductionActivationExecutionHandoff::ExecuteSelectedActivation(
	const FWBProductionActivationExecutionHandoffInput& Input) const
{
	if (Input.GameState == nullptr)
	{
		return MakeFailure(EWBProductionActivationExecutionHandoffResultCode::GameStateMissing);
	}

	if (Input.Repository == nullptr)
	{
		return MakeFailure(EWBProductionActivationExecutionHandoffResultCode::RepositoryMissing);
	}

	if (Input.Provider == nullptr || !Input.Provider->IsConfigured())
	{
		return MakeFailure(EWBProductionActivationExecutionHandoffResultCode::ProviderMissing);
	}

	if (!FWBGameStateData::IsValidPlayerId(Input.ViewerPlayerId)
		|| Input.GameState->GetPlayerById(Input.ViewerPlayerId) == nullptr)
	{
		return MakeFailure(EWBProductionActivationExecutionHandoffResultCode::ExecutionRejected);
	}

	const FWBRuntimeActivationDataProviderRequest ProviderRequest = MakeProviderRequest(
		EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
		Input.ViewerPlayerId,
		Input.SelectionRequest.ActivationEntryId,
		TEXT("production_activation_execution_handoff_pre"));
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		Input.Provider->GetActivationDecisionData(ProviderRequest);
	if (!ProviderResult.bOk)
	{
		FWBProductionActivationExecutionHandoffResult Result =
			MakeFailure(EWBProductionActivationExecutionHandoffResultCode::ProviderMissing, ProviderResult.Reason);
		AppendSanitizedDiagnostics(*Input.Provider, Result.Diagnostics);
		return Result;
	}

	FWBProductionActivationTargetSelectionBridge SelectionBridge;
	SelectionBridge.ConfigureFromProviderData(ProviderResult.RefreshInput.ActivationActionSet.Actions);
	const FWBProductionActivationTargetSelectionResult SelectionResult =
		SelectionBridge.BuildCommandForSelection(Input.SelectionRequest);
	if (!SelectionResult.bOk)
	{
		FWBProductionActivationExecutionHandoffResult Result = MakeFailure(
			MapSelectionFailureCode(SelectionResult.Code),
			SelectionResult.Reason);
		Result.SourceCardId = SelectionResult.SourceCardId;
		Result.EffectId = SelectionResult.EffectId;
		Result.TargetUnitId = SelectionResult.TargetUnitId;
		return Result;
	}

	if (SelectionResult.Command.DebugActivationId.Len() > 0
		|| SelectionResult.Command.UsageCommit.UsageKey.Len() > 0)
	{
		return MakeFailure(EWBProductionActivationExecutionHandoffResultCode::HiddenDataRejected);
	}

	bool bAmbiguous = false;
	const FWBCardActivationLegalAction* ProviderAction = FindSingleMatchingAction(
		ProviderResult.RefreshInput.ActivationActionSet.Actions,
		Input.SelectionRequest,
		bAmbiguous);
	if (bAmbiguous)
	{
		return MakeFailure(EWBProductionActivationExecutionHandoffResultCode::InternalExecutionMetadataMissing);
	}

	if (ProviderAction == nullptr)
	{
		return MakeFailure(EWBProductionActivationExecutionHandoffResultCode::ActivationEntryMissing);
	}

	FWBProductionActivationExecutionHandoffResult Result;
	Result.SourceCardId = SelectionResult.SourceCardId;
	Result.EffectId = SelectionResult.EffectId;
	Result.TargetUnitId = SelectionResult.TargetUnitId;

	const FWBCardActivationExpansionResult ExpansionResult =
		RebuildInternalCommand(*ProviderAction, SelectionResult, *Input.Repository);
	if (!ExpansionResult.bOk)
	{
		Result.bOk = false;
		Result.Code = ExpansionResult.Reason == TEXT("missing_card_id")
			|| ExpansionResult.Reason == TEXT("effect_id_not_found")
			|| ExpansionResult.Reason == TEXT("effect_id_ambiguous")
				? EWBProductionActivationExecutionHandoffResultCode::InternalExecutionMetadataMissing
				: EWBProductionActivationExecutionHandoffResultCode::CommandRebuildFailed;
		Result.Reason = ExpansionResult.Reason.IsEmpty()
			? ResultCodeToString(Result.Code)
			: ExpansionResult.Reason;
		return Result;
	}

	const FWBRuntimeActivationExecutionHandoffResult RuntimeHandoff =
		MakeRuntimeExecutionHandoff(*ProviderAction, ExpansionResult.Command);
	const FWBRuntimeActivationExecutionResult ExecutionResult =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			*Input.GameState,
			RuntimeHandoff);
	if (!ExecutionResult.bOk)
	{
		Result.bOk = false;
		Result.Code = ExecutionResult.bExecutionAttempted
			? EWBProductionActivationExecutionHandoffResultCode::ExecutionFailed
			: EWBProductionActivationExecutionHandoffResultCode::ExecutionRejected;
		Result.Reason = ExecutionResult.Reason.IsEmpty()
			? ResultCodeToString(Result.Code)
			: ExecutionResult.Reason;
		Result.bExecutionApplied = false;
		return Result;
	}

	FWBProductionActivationDataProvider PostProvider;
	FWBProductionActivationDataProviderInput PostInput;
	PostInput.GameState = Input.GameState;
	PostInput.Repository = Input.Repository;
	PostInput.ViewerPlayerId = Input.ViewerPlayerId;
	PostProvider.Configure(PostInput, FWBProductionActivationDataProviderConfig());

	const FWBRuntimeActivationDataProviderRequest RefreshRequest = MakeProviderRequest(
		EWBRuntimeActivationDataRequestKind::PostActivation,
		Input.ViewerPlayerId,
		Input.SelectionRequest.ActivationEntryId,
		TEXT("production_activation_execution_handoff_post"));
	const FWBRuntimeActivationDataProviderResult RefreshResult =
		PostProvider.GetActivationDecisionData(RefreshRequest);
	if (!RefreshResult.bOk)
	{
		Result.bOk = false;
		Result.Code = EWBProductionActivationExecutionHandoffResultCode::ProviderRefreshFailed;
		Result.Reason = RefreshResult.Reason.IsEmpty()
			? ResultCodeToString(Result.Code)
			: RefreshResult.Reason;
		Result.bExecutionApplied = true;
		AppendSanitizedDiagnostics(PostProvider, Result.Diagnostics);
		return Result;
	}

	Result.bOk = true;
	Result.Code = EWBProductionActivationExecutionHandoffResultCode::Success;
	Result.Reason = ResultCodeToString(Result.Code);
	Result.bExecutionApplied = true;
	Result.RefreshedActivationEntries = RefreshResult.RefreshInput.ActivationActionSet.Actions;
	AppendSanitizedDiagnostics(PostProvider, Result.Diagnostics);
	return Result;
}

FString FWBProductionActivationExecutionHandoff::ResultCodeToString(
	const EWBProductionActivationExecutionHandoffResultCode Code)
{
	switch (Code)
	{
	case EWBProductionActivationExecutionHandoffResultCode::Success:
		return TEXT("success");
	case EWBProductionActivationExecutionHandoffResultCode::HandoffNotConfigured:
		return TEXT("handoff_not_configured");
	case EWBProductionActivationExecutionHandoffResultCode::GameStateMissing:
		return TEXT("game_state_missing");
	case EWBProductionActivationExecutionHandoffResultCode::RepositoryMissing:
		return TEXT("repository_missing");
	case EWBProductionActivationExecutionHandoffResultCode::ProviderMissing:
		return TEXT("provider_missing");
	case EWBProductionActivationExecutionHandoffResultCode::SelectionFailed:
		return TEXT("selection_failed");
	case EWBProductionActivationExecutionHandoffResultCode::ActivationEntryMissing:
		return TEXT("activation_entry_missing");
	case EWBProductionActivationExecutionHandoffResultCode::InternalExecutionMetadataMissing:
		return TEXT("internal_execution_metadata_missing");
	case EWBProductionActivationExecutionHandoffResultCode::CommandRebuildFailed:
		return TEXT("command_rebuild_failed");
	case EWBProductionActivationExecutionHandoffResultCode::ExecutionRejected:
		return TEXT("execution_rejected");
	case EWBProductionActivationExecutionHandoffResultCode::ExecutionFailed:
		return TEXT("execution_failed");
	case EWBProductionActivationExecutionHandoffResultCode::ProviderRefreshFailed:
		return TEXT("provider_refresh_failed");
	case EWBProductionActivationExecutionHandoffResultCode::UnsupportedTargetRequirement:
		return TEXT("unsupported_target_requirement");
	case EWBProductionActivationExecutionHandoffResultCode::HiddenDataRejected:
		return TEXT("hidden_data_rejected");
	default:
		return TEXT("handoff_not_configured");
	}
}
