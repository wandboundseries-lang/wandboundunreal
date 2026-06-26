#pragma once

#include "CoreMinimal.h"
#include "WBRuntimeActivationDataProvider.h"

class FWBRuntimeActivationFixedDataProviderForTests final : public IWBRuntimeActivationDataProvider
{
public:
	FWBRuntimeActivationDataProviderResult FixedResult;

	virtual FWBRuntimeActivationDataProviderResult GetActivationDecisionData(
		const FWBRuntimeActivationDataProviderRequest& Request) const override;
};
