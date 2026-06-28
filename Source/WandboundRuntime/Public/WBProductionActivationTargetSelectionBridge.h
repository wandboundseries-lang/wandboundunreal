#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationLegalAction.h"

enum class EWBProductionActivationTargetSelectionResultCode : uint8
{
	Success,
	BridgeNotConfigured,
	ActivationEntryMissing,
	TargetRequiredButMissing,
	TargetNotAllowed,
	TargetTypeMismatch,
	TargetStaleOrMissing,
	TargetProvidedForNoTargetEffect,
	UnsupportedTargetRequirement,
	HiddenTargetRejected,
	CommandBuildFailed
};

struct WANDBOUNDRUNTIME_API FWBProductionActivationTargetSelectionRequest
{
	FString ActivationEntryId;
	FString SourceInstanceId;
	FString SourceCardId;
	FString EffectId;

	bool bHasSelectedTarget = false;
	FWBCardActivationTargetOption SelectedTargetOption;
};

struct WANDBOUNDRUNTIME_API FWBProductionActivationTargetSelectionResult
{
	bool bOk = false;
	EWBProductionActivationTargetSelectionResultCode Code =
		EWBProductionActivationTargetSelectionResultCode::BridgeNotConfigured;

	FString Reason;

	FWBCardActivationCommand Command;

	FString SourceCardId;
	FString EffectId;
	int32 TargetUnitId = -1;
};

class WANDBOUNDRUNTIME_API FWBProductionActivationTargetSelectionBridge
{
public:
	void ConfigureFromProviderData(
		const TArray<FWBCardActivationLegalAction>& ProviderActivationEntries);

	bool IsConfigured() const;

	FWBProductionActivationTargetSelectionResult BuildCommandForSelection(
		const FWBProductionActivationTargetSelectionRequest& Request) const;

	static FString ResultCodeToString(
		EWBProductionActivationTargetSelectionResultCode Code);

private:
	TArray<FWBCardActivationLegalAction> ActivationEntries;
	bool bConfigured = false;
};
