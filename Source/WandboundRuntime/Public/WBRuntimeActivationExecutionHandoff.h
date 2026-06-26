#pragma once

#include "CoreMinimal.h"
#include "WBRuntimeActivationSelectionResolver.h"

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationExecutionHandoffResult
{
	bool bHandoffOk = false;
	bool bSelectionResolved = false;

	bool bExecutionImplemented = false;
	bool bExecutionAttempted = false;

	FString Reason;
	FString SelectedActivationActionId;

	FWBCardActivationLegalAction ActivationAction;

	bool bHasActivationPresentationEntry = false;
	FWBCardActivationLegalActionPresentationEntry ActivationPresentationEntry;

	bool bHasTargetPresentationEntry = false;
	FWBCardActivationTargetPresentationEntry TargetPresentationEntry;
};

class WANDBOUNDRUNTIME_API WBRuntimeActivationExecutionHandoff
{
public:
	static FWBRuntimeActivationExecutionHandoffResult CreateNotImplementedHandoff(
		const FWBRuntimeActivationSelectionResolution& SelectionResolution);
};
