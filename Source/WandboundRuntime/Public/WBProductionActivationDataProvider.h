#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBRuntimeActivationDataProvider.h"

struct WANDBOUNDRUNTIME_API FWBProductionActivationDataProviderInput
{
	const FWBGameStateData* GameState = nullptr;
	const FWBCardDefinitionRepository* Repository = nullptr;
	int32 ViewerPlayerId = -1;
};

struct WANDBOUNDRUNTIME_API FWBProductionActivationDataProviderConfig
{
	bool bIncludeBoardSources = true;
	bool bIncludeOwnHandSources = true;
	bool bIncludeDiscardSources = false;
	bool bIncludeEquippedSources = false;
};

struct WANDBOUNDRUNTIME_API FWBProductionActivationDataProviderDiagnostic
{
	FString Code;
	FString CardId;
	FString InstanceId;
	int32 UnitId = -1;
};

class WANDBOUNDRUNTIME_API FWBProductionActivationDataProvider final : public IWBRuntimeActivationDataProvider
{
public:
	void Configure(
		const FWBProductionActivationDataProviderInput& InInput,
		const FWBProductionActivationDataProviderConfig& InConfig);

	bool IsConfigured() const;

	TArray<FWBProductionActivationDataProviderDiagnostic> GetDiagnosticsForTest() const;

	virtual FWBRuntimeActivationDataProviderResult GetActivationDecisionData(
		const FWBRuntimeActivationDataProviderRequest& Request) const override;

private:
	FWBProductionActivationDataProviderInput Input;
	FWBProductionActivationDataProviderConfig Config;
	bool bConfigured = false;

	mutable TArray<FWBProductionActivationDataProviderDiagnostic> LastDiagnostics;
};
