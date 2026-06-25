#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBRuntimeActionSelectionBridge.h"
#include "WBRuntimeActivationPresentationRefreshAdapter.h"
#include "WBRuntimeInteractionRefreshAdapter.h"
#include "WBRuntimeLegalActionPresentation.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.generated.h"

class UWBRuntimeActivationPresentationModelComponent;
class UWBRuntimeTurnInteractionModelComponent;
class UWBRuntimeVisualControllerComponent;

struct WANDBOUNDRUNTIME_API FWBRuntimeDecisionPointStatus
{
	bool bReady = false;
	bool bHasActions = false;
	int32 LegalActionCount = 0;
	int32 PresentationEntryCount = 0;
	int32 ActivationActionCount = 0;
	int32 ActivationPresentationEntryCount = 0;
	int32 ActivationTargetPresentationEntryCount = 0;
	int32 PublicUnitCount = 0;
	int32 PublicWallCount = 0;
	int32 PublicTerrainCount = 0;
	FString LastRefreshReason;
	bool bHasLastSelectedAction = false;
	FString LastSelectedActionId;
	bool bLastSelectedActionResolved = false;
	FString LastSelectedActionReason;
};

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeDecisionPointCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeDecisionPointCoordinatorComponent();

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	TObjectPtr<UWBRuntimeTurnInteractionModelComponent> TurnInteractionModel;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	TObjectPtr<UWBRuntimeVisualControllerComponent> VisualController;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	TObjectPtr<UWBRuntimeActivationPresentationModelComponent> ActivationPresentationModel;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	bool bRefreshVisualController = true;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	bool bRefreshTurnInteractionModel = true;

	void SetTurnInteractionModel(UWBRuntimeTurnInteractionModelComponent* InModel);
	UWBRuntimeTurnInteractionModelComponent* GetTurnInteractionModel() const;

	void SetVisualController(UWBRuntimeVisualControllerComponent* InVisualController);
	UWBRuntimeVisualControllerComponent* GetVisualController() const;

	void SetActivationPresentationModel(UWBRuntimeActivationPresentationModelComponent* InModel);
	UWBRuntimeActivationPresentationModelComponent* GetActivationPresentationModel() const;

	FWBRuntimeInteractionRefreshResult RefreshDecisionPoint(
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FWBPublicBoardSummary& PublicBoardSummary);

	FWBRuntimeActivationPresentationRefreshResult RefreshActivationPresentationFromExternalData(
		const FWBCardActivationLegalActionSet& ActivationActionSet,
		const FWBPublicBoardSummary& PublicBoardSummary);

	void ClearDecisionPoint();
	void ClearActivationPresentation();

	bool HasCurrentDecisionPoint() const;
	FWBRuntimeDecisionPointStatus GetCurrentStatus() const;
	FWBRuntimeInteractionRefreshResult GetLastRefreshResult() const;

	const FWBRuntimeLegalActionPresentationSnapshot* GetCurrentPresentationSnapshot() const;

	bool TryFindPresentationEntryByActionId(
		const FString& ActionId,
		FWBRuntimeActionPresentationEntry& OutEntry) const;

	FWBRuntimeActionSelectionExecutionResult ExecuteSelectedActionId(
		FWBGameStateData& State,
		const FString& SelectedActionId,
		FWBRuntimeTurnResolutionContext& RuntimeContext,
		UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade);

	bool HasLastSelectedActionExecution() const;
	FWBRuntimeActionSelectionExecutionResult GetLastSelectedActionExecutionResult() const;
	void ClearLastSelectedActionExecution();

private:
	FWBRuntimeInteractionRefreshResult LastRefreshResult;
	FWBRuntimeActionSelectionExecutionResult LastSelectedActionExecutionResult;
	FWBRuntimeDecisionPointStatus CurrentStatus;
	bool bHasCurrentDecisionPoint = false;
	bool bHasLastSelectedActionExecution = false;
};
