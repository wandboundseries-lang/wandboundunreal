#include "WBRuntimeActivationFixedDataProviderForTests.h"

FWBRuntimeActivationDataProviderResult
FWBRuntimeActivationFixedDataProviderForTests::GetActivationDecisionData(
	const FWBRuntimeActivationDataProviderRequest& Request) const
{
	FWBRuntimeActivationDataProviderResult Result = FixedResult;
	Result.Request = Request;
	return Result;
}
