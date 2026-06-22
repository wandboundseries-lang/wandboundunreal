#include "WBRuntimeDecisionLoopTestHarness.h"

#include "WBPublicBoardSummary.h"
#include "WBRules.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeTurnResolutionAdapter.h"

TArray<FWBAction> FWBDecisionLoopTestHarness::GenerateExternalLegalActionsForTest(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	// Test-only external rules owner simulation. Production runtime must not call this.
	return WBRules::GenerateLegalActionsForPlayer(State, PlayerId);
}

FWBPublicBoardSummary FWBDecisionLoopTestHarness::BuildExternalPublicSummaryForTest(
	const FWBGameStateData& State)
{
	// Test-only external public summary provider simulation. Production runtime receives summaries from elsewhere.
	return WBPublicBoardSummary::Build(State);
}

bool FWBDecisionLoopTestHarness::FindFirstPresentedActionIdOfType(
	const FWBRuntimeLegalActionPresentationSnapshot& Snapshot,
	const EWBRuntimeActionPresentationType Type,
	FString& OutActionId)
{
	for (const FWBRuntimeActionPresentationEntry& Entry : Snapshot.Entries)
	{
		if (Entry.Type == Type)
		{
			OutActionId = Entry.ActionId;
			return true;
		}
	}

	OutActionId.Reset();
	return false;
}

FWBDecisionLoopTestHarnessStepResult FWBDecisionLoopTestHarness::RefreshAndExecuteFirstActionOfType(
	FWBGameStateData& State,
	const int32 PlayerId,
	const EWBRuntimeActionPresentationType Type,
	FWBRuntimeTurnResolutionContext& RuntimeContext,
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator,
	UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade)
{
	FWBDecisionLoopTestHarnessStepResult StepResult;

	if (Coordinator == nullptr)
	{
		return StepResult;
	}

	const TArray<FWBAction> LegalActions = GenerateExternalLegalActionsForTest(State, PlayerId);
	const FWBPublicBoardSummary PublicSummary = BuildExternalPublicSummaryForTest(State);
	StepResult.RefreshResult = Coordinator->RefreshDecisionPoint(LegalActions, PublicSummary);
	StepResult.bRefreshOk = StepResult.RefreshResult.bOk;

	const FWBRuntimeLegalActionPresentationSnapshot* Snapshot = Coordinator->GetCurrentPresentationSnapshot();
	if (!StepResult.bRefreshOk || Snapshot == nullptr)
	{
		return StepResult;
	}

	if (!FindFirstPresentedActionIdOfType(*Snapshot, Type, StepResult.SelectedActionId))
	{
		return StepResult;
	}

	StepResult.ExecutionResult = Coordinator->ExecuteSelectedActionId(
		State,
		StepResult.SelectedActionId,
		RuntimeContext,
		RuntimeControllerFacade);
	StepResult.bExecutionOk = StepResult.ExecutionResult.Selection.bOk
		&& StepResult.ExecutionResult.Execution.RuntimeResult.ApplyResult.bOk;

	return StepResult;
}
