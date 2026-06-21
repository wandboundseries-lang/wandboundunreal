#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBRuntimeActionSelectionBridge.h"
#include "WBRuntimeLegalActionPresentation.h"
#include "WBRuntimeTurnInteractionModelComponent.generated.h"

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeTurnInteractionModelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeTurnInteractionModelComponent();

	void SetCurrentInteractionState(
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FWBPublicBoardSummary& PublicBoardSummary);

	void ClearCurrentInteractionState();

	bool HasCurrentInteractionState() const;

	const TArray<FWBAction>& GetCurrentLegalActions() const;
	const FWBRuntimeLegalActionPresentationSnapshot& GetCurrentPresentationSnapshot() const;

	bool TryFindPresentationEntryByActionId(
		const FString& ActionId,
		FWBRuntimeActionPresentationEntry& OutEntry) const;

	FWBRuntimeActionSelectionExecutionResult ExecuteSelectedActionId(
		FWBGameStateData& State,
		const FString& SelectedActionId,
		FWBRuntimeTurnResolutionContext& RuntimeContext,
		UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade);

	bool HasLastExecutionResult() const;
	FString GetLastSelectedActionId() const;
	FWBResolvedRuntimeActionSelection GetLastSelectionResult() const;
	FWBSelectedActionVisualHarnessResult GetLastExecutionResult() const;
	void ClearLastExecutionResult();

private:
	TArray<FWBAction> CurrentLegalActions;
	FWBPublicBoardSummary CurrentPublicBoardSummary;
	FWBRuntimeLegalActionPresentationSnapshot CurrentPresentationSnapshot;
	bool bHasCurrentInteractionState = false;
	FString LastSelectedActionId;
	FWBResolvedRuntimeActionSelection LastSelectionResult;
	FWBSelectedActionVisualHarnessResult LastExecutionResult;
	bool bHasLastExecutionResult = false;
};
