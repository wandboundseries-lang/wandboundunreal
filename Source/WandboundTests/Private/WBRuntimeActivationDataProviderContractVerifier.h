#pragma once

#include "CoreMinimal.h"
#include "WBRuntimeActivationDataProvider.h"

struct FWBRuntimeActivationProviderContractExpectations
{
	bool bRequireOk = true;

	bool bRequireNormalLegalActions = true;
	bool bRequireActivationActions = true;
	bool bRequirePublicBoardSummary = true;

	int32 ExpectedMinNormalLegalActions = 0;
	int32 ExpectedMinActivationActions = 0;
	int32 ExpectedMinPublicUnits = 0;

	bool bRequireDeterministicOrdering = true;
	bool bRequireNoHiddenSubstrings = true;

	TArray<FString> ForbiddenSubstrings;
};

// Test-only contract verifier for future activation data providers.
// Do not include this from WandboundRuntime or production code.
class FWBRuntimeActivationDataProviderContractVerifier
{
public:
	static bool VerifyProviderResultContract(
		const FWBRuntimeActivationDataProviderResult& Result,
		const FWBRuntimeActivationProviderContractExpectations& Expectations,
		FString& OutReason);

	static bool VerifyDeterministicProviderOutput(
		const IWBRuntimeActivationDataProvider& Provider,
		const FWBRuntimeActivationDataProviderRequest& Request,
		FString& OutReason);

	static bool ProviderResultToDebugStringForTest(
		const FWBRuntimeActivationDataProviderResult& Result,
		FString& OutDebugString);
};
