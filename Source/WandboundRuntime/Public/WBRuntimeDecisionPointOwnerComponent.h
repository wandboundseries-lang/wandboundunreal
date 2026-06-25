#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.generated.h"

class UWBRuntimeControllerFacadeComponent;

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeDecisionPointOwnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeDecisionPointOwnerComponent();

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	TObjectPtr<UWBRuntimeDecisionPointCoordinatorComponent> DecisionPointCoordinator;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	TObjectPtr<UWBRuntimeControllerFacadeComponent> RuntimeControllerFacade;

	void SetDecisionPointCoordinator(UWBRuntimeDecisionPointCoordinatorComponent* InCoordinator);
	UWBRuntimeDecisionPointCoordinatorComponent* GetDecisionPointCoordinator() const;

	void SetRuntimeControllerFacade(UWBRuntimeControllerFacadeComponent* InFacade);
	UWBRuntimeControllerFacadeComponent* GetRuntimeControllerFacade() const;

	FWBRuntimeInteractionRefreshResult RefreshDecisionPointFromExternalData(
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FWBPublicBoardSummary& PublicBoardSummary);

	FWBRuntimeActivationPresentationRefreshResult RefreshActivationPresentationFromExternalData(
		const FWBCardActivationLegalActionSet& ActivationActionSet,
		const FWBPublicBoardSummary& PublicBoardSummary);

	FWBRuntimeActionSelectionExecutionResult ExecuteSelectedActionId(
		FWBGameStateData& State,
		const FString& SelectedActionId,
		FWBRuntimeTurnResolutionContext& RuntimeContext);

	FWBRuntimeActionSelectionExecutionResult ExecuteSelectedActionIdAndRefreshFromExternalData(
		FWBGameStateData& State,
		const FString& SelectedActionId,
		FWBRuntimeTurnResolutionContext& RuntimeContext,
		const TArray<FWBAction>& PostActionPrecomputedLegalActions,
		const FWBPublicBoardSummary& PostActionPublicBoardSummary);

	bool HasCurrentDecisionPoint() const;
	FWBRuntimeDecisionPointStatus GetCurrentStatus() const;
	FWBRuntimeInteractionRefreshResult GetLastRefreshResult() const;
	bool HasLastExecutionResult() const;
	FWBRuntimeActionSelectionExecutionResult GetLastExecutionResult() const;
	void ClearActivationPresentation();
	void Clear();

private:
	bool bHasCurrentDecisionPoint = false;
	FWBRuntimeDecisionPointStatus CurrentStatus;
	FWBRuntimeInteractionRefreshResult LastRefreshResult;
	FWBRuntimeActionSelectionExecutionResult LastExecutionResult;
	bool bHasLastExecutionResult = false;
};
