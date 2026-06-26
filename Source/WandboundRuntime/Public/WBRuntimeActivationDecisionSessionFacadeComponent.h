#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBAction.h"
#include "WBCardActivationLegalAction.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeActivationExecutionHandoff.h"
#include "WBRuntimeActivationSelectionResolver.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimePostActivationRefreshSequencer.h"
#include "WBRuntimeActivationDecisionSessionFacadeComponent.generated.h"

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationDecisionSessionRefreshInput
{
	TArray<FWBAction> NormalLegalActions;
	FWBCardActivationLegalActionSet ActivationActionSet;
	FWBPublicBoardSummary PublicBoardSummary;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationDecisionSessionStatus
{
	bool bReady = false;

	bool bNormalDecisionPointReady = false;
	bool bActivationPresentationReady = false;

	int32 NormalLegalActionCount = 0;
	int32 NormalPresentationEntryCount = 0;

	int32 ActivationActionCount = 0;
	int32 ActivationPresentationEntryCount = 0;
	int32 ActivationTargetPresentationEntryCount = 0;

	bool bHasLastActivationSelection = false;
	bool bHasLastActivationExecution = false;

	FString LastSelectedActivationActionId;
	FString LastReason;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationDecisionSessionRefreshResult
{
	bool bOk = false;
	FString Reason;

	FWBRuntimeInteractionRefreshResult NormalRefreshResult;
	FWBRuntimeActivationPresentationRefreshResult ActivationRefreshResult;

	FWBRuntimeActivationDecisionSessionStatus Status;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationDecisionSessionExecutionResult
{
	bool bOk = false;
	FString Reason;

	FWBRuntimeActivationSelectionResolution SelectionResult;
	FWBRuntimeActivationExecutionHandoffResult HandoffResult;
	FWBRuntimePostActivationRefreshResult PostActivationRefreshResult;

	FWBRuntimeActivationDecisionSessionStatus Status;
};

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeActivationDecisionSessionFacadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeActivationDecisionSessionFacadeComponent();

	UPROPERTY(EditAnywhere, Category = "Runtime Activation Decision Session")
	TObjectPtr<UWBRuntimeDecisionPointOwnerComponent> DecisionPointOwner;

	void SetDecisionPointOwner(UWBRuntimeDecisionPointOwnerComponent* InOwner);
	UWBRuntimeDecisionPointOwnerComponent* GetDecisionPointOwner() const;

	FWBRuntimeActivationDecisionSessionRefreshResult RefreshSessionFromExternalData(
		const FWBRuntimeActivationDecisionSessionRefreshInput& Input);

	FWBRuntimeActivationSelectionResolution ResolveActivationActionId(
		const FString& SelectedActivationActionId) const;

	FWBRuntimeActivationExecutionHandoffResult CreateActivationExecutionHandoff(
		const FString& SelectedActivationActionId) const;

	FWBRuntimeActivationDecisionSessionExecutionResult ExecuteActivationActionIdAndRefreshFromExternalData(
		FWBGameStateData& State,
		const FString& SelectedActivationActionId,
		const FWBRuntimeActivationDecisionSessionRefreshInput& PostActionInput,
		const FWBRuntimePostActivationRefreshOptions& Options = FWBRuntimePostActivationRefreshOptions());

	bool HasSessionState() const;
	FWBRuntimeActivationDecisionSessionStatus GetCurrentStatus() const;

	FWBRuntimeActivationSelectionResolution GetLastActivationSelection() const;
	FWBRuntimeActivationExecutionHandoffResult GetLastActivationHandoff() const;
	FWBRuntimePostActivationRefreshResult GetLastPostActivationRefreshResult() const;

	void ClearSession();

private:
	bool bHasSessionState = false;
	FWBRuntimeActivationDecisionSessionStatus CurrentStatus;
	FWBRuntimeActivationSelectionResolution LastActivationSelection;
	FWBRuntimeActivationExecutionHandoffResult LastActivationHandoff;
	FWBRuntimePostActivationRefreshResult LastPostActivationRefreshResult;
};
