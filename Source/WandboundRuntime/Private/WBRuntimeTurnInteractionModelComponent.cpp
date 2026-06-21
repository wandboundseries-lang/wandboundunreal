#include "WBRuntimeTurnInteractionModelComponent.h"

namespace
{
FWBResolvedRuntimeActionSelection MakeInteractionSelectionFailure(
	const FString& SelectedActionId,
	const FString& Reason)
{
	FWBResolvedRuntimeActionSelection Selection;
	Selection.bOk = false;
	Selection.SelectedActionId = SelectedActionId;
	Selection.Reason = Reason;
	return Selection;
}
}

UWBRuntimeTurnInteractionModelComponent::UWBRuntimeTurnInteractionModelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimeTurnInteractionModelComponent::SetCurrentInteractionState(
	const TArray<FWBAction>& PrecomputedLegalActions,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	CurrentLegalActions = PrecomputedLegalActions;
	CurrentPublicBoardSummary = PublicBoardSummary;
	CurrentPresentationSnapshot = WBRuntimeLegalActionPresentation::BuildSnapshot(
		CurrentLegalActions,
		CurrentPublicBoardSummary);
	bHasCurrentInteractionState = true;
	ClearLastExecutionResult();
}

void UWBRuntimeTurnInteractionModelComponent::ClearCurrentInteractionState()
{
	CurrentLegalActions.Reset();
	CurrentPublicBoardSummary = FWBPublicBoardSummary();
	CurrentPresentationSnapshot = FWBRuntimeLegalActionPresentationSnapshot();
	bHasCurrentInteractionState = false;
	ClearLastExecutionResult();
}

bool UWBRuntimeTurnInteractionModelComponent::HasCurrentInteractionState() const
{
	return bHasCurrentInteractionState;
}

const TArray<FWBAction>& UWBRuntimeTurnInteractionModelComponent::GetCurrentLegalActions() const
{
	return CurrentLegalActions;
}

const FWBRuntimeLegalActionPresentationSnapshot& UWBRuntimeTurnInteractionModelComponent::GetCurrentPresentationSnapshot() const
{
	return CurrentPresentationSnapshot;
}

bool UWBRuntimeTurnInteractionModelComponent::TryFindPresentationEntryByActionId(
	const FString& ActionId,
	FWBRuntimeActionPresentationEntry& OutEntry) const
{
	return WBRuntimeLegalActionPresentation::TryFindEntryByActionId(
		CurrentPresentationSnapshot,
		ActionId,
		OutEntry);
}

FWBRuntimeActionSelectionExecutionResult UWBRuntimeTurnInteractionModelComponent::ExecuteSelectedActionId(
	FWBGameStateData& State,
	const FString& SelectedActionId,
	FWBRuntimeTurnResolutionContext& RuntimeContext,
	UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade)
{
	LastSelectedActionId = SelectedActionId;

	FWBRuntimeActionSelectionExecutionResult Result;
	if (!bHasCurrentInteractionState)
	{
		Result.Selection = MakeInteractionSelectionFailure(
			SelectedActionId,
			TEXT("no_current_interaction_state"));
	}
	else
	{
		Result = WBRuntimeActionSelectionBridge::ExecuteSelectedActionId(
			State,
			CurrentLegalActions,
			SelectedActionId,
			RuntimeContext,
			RuntimeControllerFacade);
	}

	LastSelectionResult = Result.Selection;
	LastExecutionResult = Result.Execution;
	bHasLastExecutionResult = true;
	return Result;
}

bool UWBRuntimeTurnInteractionModelComponent::HasLastExecutionResult() const
{
	return bHasLastExecutionResult;
}

FString UWBRuntimeTurnInteractionModelComponent::GetLastSelectedActionId() const
{
	return LastSelectedActionId;
}

FWBResolvedRuntimeActionSelection UWBRuntimeTurnInteractionModelComponent::GetLastSelectionResult() const
{
	return LastSelectionResult;
}

FWBSelectedActionVisualHarnessResult UWBRuntimeTurnInteractionModelComponent::GetLastExecutionResult() const
{
	return LastExecutionResult;
}

void UWBRuntimeTurnInteractionModelComponent::ClearLastExecutionResult()
{
	LastSelectedActionId.Reset();
	LastSelectionResult = FWBResolvedRuntimeActionSelection();
	LastExecutionResult = FWBSelectedActionVisualHarnessResult();
	bHasLastExecutionResult = false;
}
