#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationLegalAction.h"
#include "WBCardActivationTargetPresentation.h"

class UWBRuntimeActivationPresentationModelComponent;

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationSelectionResolution
{
	bool bOk = false;
	FString Reason;

	FString SelectedActivationActionId;

	FWBCardActivationLegalAction ActivationAction;

	bool bHasActivationPresentationEntry = false;
	FWBCardActivationLegalActionPresentationEntry ActivationPresentationEntry;

	bool bHasTargetPresentationEntry = false;
	FWBCardActivationTargetPresentationEntry TargetPresentationEntry;
};

class WANDBOUNDRUNTIME_API WBRuntimeActivationSelectionResolver
{
public:
	static FWBRuntimeActivationSelectionResolution ResolveSelectedActivationActionId(
		const UWBRuntimeActivationPresentationModelComponent* ActivationPresentationModel,
		const FString& SelectedActivationActionId);
};
