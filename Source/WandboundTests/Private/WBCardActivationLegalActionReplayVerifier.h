#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationCommand.h"
#include "WBCardActivationLegalAction.h"
#include "WBGameStateData.h"

struct FWBCardActivationLegalActionReplaySelection
{
	bool bOk = false;
	FString Reason;
	FString SelectedActivationActionId;
	FWBCardActivationLegalAction SelectedAction;
};

struct FWBCardActivationLegalActionReplayResult
{
	FWBCardActivationLegalActionReplaySelection Selection;
	FWBCardActivationCommandResult ActivationResult;
};

class FWBCardActivationLegalActionReplayVerifier
{
public:
	static FWBCardActivationLegalActionReplaySelection ResolveActivationActionId(
		const FWBCardActivationLegalActionSet& ActionSet,
		const FString& SelectedActivationActionId);

	static FWBCardActivationLegalActionReplayResult ApplySelectedActivationActionId(
		FWBGameStateData& State,
		const FWBCardActivationLegalActionSet& ActionSet,
		const FString& SelectedActivationActionId);
};
