#pragma once

#include "CoreMinimal.h"
#include "WBRuntimeActivationDataProvider.h"

class FWBDeterministicActivationDataProviderForTests final : public IWBRuntimeActivationDataProvider
{
public:
	FWBRuntimeActivationDataProviderResult FixedResult;

	virtual FWBRuntimeActivationDataProviderResult GetActivationDecisionData(
		const FWBRuntimeActivationDataProviderRequest& Request) const override;
};

class FWBFailingActivationDataProviderForTests final : public IWBRuntimeActivationDataProvider
{
public:
	FString FailureReason = TEXT("provider_failed_for_test");

	virtual FWBRuntimeActivationDataProviderResult GetActivationDecisionData(
		const FWBRuntimeActivationDataProviderRequest& Request) const override;
};

class FWBHiddenTokenActivationDataProviderForTests final : public IWBRuntimeActivationDataProvider
{
public:
	FWBRuntimeActivationDataProviderResult HiddenResult;

	virtual FWBRuntimeActivationDataProviderResult GetActivationDecisionData(
		const FWBRuntimeActivationDataProviderRequest& Request) const override;
};

class FWBChangingActivationDataProviderForTests final : public IWBRuntimeActivationDataProvider
{
public:
	FWBRuntimeActivationDataProviderResult BaseResult;
	mutable int32 CallCount = 0;

	virtual FWBRuntimeActivationDataProviderResult GetActivationDecisionData(
		const FWBRuntimeActivationDataProviderRequest& Request) const override;
};
