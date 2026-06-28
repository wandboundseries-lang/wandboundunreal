#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationLegalAction.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBProductionActivationDataProvider.h"
#include "WBProductionActivationTargetSelectionBridge.h"

enum class EWBProductionActivationExecutionHandoffResultCode : uint8
{
	Success,
	HandoffNotConfigured,
	GameStateMissing,
	RepositoryMissing,
	ProviderMissing,
	SelectionFailed,
	ActivationEntryMissing,
	InternalExecutionMetadataMissing,
	CommandRebuildFailed,
	ExecutionRejected,
	ExecutionFailed,
	ProviderRefreshFailed,
	UnsupportedTargetRequirement,
	HiddenDataRejected
};

struct WANDBOUNDRUNTIME_API FWBProductionActivationExecutionHandoffInput
{
	FWBGameStateData* GameState = nullptr;
	const FWBCardDefinitionRepository* Repository = nullptr;
	int32 ViewerPlayerId = -1;

	const FWBProductionActivationDataProvider* Provider = nullptr;

	FWBProductionActivationTargetSelectionRequest SelectionRequest;
};

struct WANDBOUNDRUNTIME_API FWBProductionActivationExecutionHandoffResult
{
	bool bOk = false;
	EWBProductionActivationExecutionHandoffResultCode Code =
		EWBProductionActivationExecutionHandoffResultCode::HandoffNotConfigured;

	FString Reason;

	FString SourceCardId;
	FString EffectId;
	int32 TargetUnitId = -1;

	bool bExecutionApplied = false;

	TArray<FWBCardActivationLegalAction> RefreshedActivationEntries;
	TArray<FString> Diagnostics;
};

class WANDBOUNDRUNTIME_API FWBProductionActivationExecutionHandoff
{
public:
	FWBProductionActivationExecutionHandoffResult ExecuteSelectedActivation(
		const FWBProductionActivationExecutionHandoffInput& Input) const;

	static FString ResultCodeToString(
		EWBProductionActivationExecutionHandoffResultCode Code);
};
