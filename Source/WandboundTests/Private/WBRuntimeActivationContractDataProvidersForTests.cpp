#include "WBRuntimeActivationContractDataProvidersForTests.h"

FWBRuntimeActivationDataProviderResult
FWBDeterministicActivationDataProviderForTests::GetActivationDecisionData(
	const FWBRuntimeActivationDataProviderRequest& Request) const
{
	FWBRuntimeActivationDataProviderResult Result = FixedResult;
	Result.Request = Request;
	return Result;
}

FWBRuntimeActivationDataProviderResult
FWBFailingActivationDataProviderForTests::GetActivationDecisionData(
	const FWBRuntimeActivationDataProviderRequest& Request) const
{
	FWBRuntimeActivationDataProviderResult Result;
	Result.bOk = false;
	Result.Reason = FailureReason;
	Result.Request = Request;
	return Result;
}

FWBRuntimeActivationDataProviderResult
FWBHiddenTokenActivationDataProviderForTests::GetActivationDecisionData(
	const FWBRuntimeActivationDataProviderRequest& Request) const
{
	FWBRuntimeActivationDataProviderResult Result = HiddenResult;
	Result.Request = Request;
	return Result;
}

FWBRuntimeActivationDataProviderResult
FWBChangingActivationDataProviderForTests::GetActivationDecisionData(
	const FWBRuntimeActivationDataProviderRequest& Request) const
{
	FWBRuntimeActivationDataProviderResult Result = BaseResult;
	Result.Request = Request;
	Result.Request.DecisionPointId = FString::Printf(TEXT("%s:%d"), *Request.DecisionPointId, CallCount);
	++CallCount;
	return Result;
}
