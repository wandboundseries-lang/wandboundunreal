#include "WBRuntimeActivationDataProviderAdapter.h"

namespace
{
FWBRuntimeActivationProviderRefreshResult MakeProviderRefreshFailure(const FString& Reason)
{
	FWBRuntimeActivationProviderRefreshResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBRuntimeActivationProviderExecutionResult MakeProviderExecutionFailure(const FString& Reason)
{
	FWBRuntimeActivationProviderExecutionResult Result;
	Result.Reason = Reason;
	return Result;
}

FString ProviderFailureReason(const FWBRuntimeActivationDataProviderResult& ProviderResult)
{
	return ProviderResult.Reason.IsEmpty()
		? FString(TEXT("activation_data_provider_failed"))
		: ProviderResult.Reason;
}
}

FWBRuntimeActivationProviderRefreshResult
WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
	UWBRuntimeActivationDecisionSessionFacadeComponent* SessionFacade,
	const IWBRuntimeActivationDataProvider& Provider,
	const FWBRuntimeActivationDataProviderRequest& Request)
{
	if (SessionFacade == nullptr)
	{
		return MakeProviderRefreshFailure(TEXT("session_facade_missing"));
	}

	FWBRuntimeActivationProviderRefreshResult Result;
	Result.ProviderResult = Provider.GetActivationDecisionData(Request);
	if (!Result.ProviderResult.bOk)
	{
		Result.Reason = ProviderFailureReason(Result.ProviderResult);
		return Result;
	}

	Result.SessionRefreshResult = SessionFacade->RefreshSessionFromExternalData(
		Result.ProviderResult.RefreshInput);
	Result.bOk = Result.SessionRefreshResult.bOk;
	Result.Reason = Result.SessionRefreshResult.Reason;
	return Result;
}

FWBRuntimeActivationProviderExecutionResult
WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
	FWBGameStateData& State,
	UWBRuntimeActivationDecisionSessionFacadeComponent* SessionFacade,
	const FString& SelectedActivationActionId,
	const IWBRuntimeActivationDataProvider& Provider,
	const FWBRuntimeActivationDataProviderRequest& PostActivationRequest,
	const FWBRuntimePostActivationRefreshOptions& Options)
{
	if (SessionFacade == nullptr)
	{
		return MakeProviderExecutionFailure(TEXT("session_facade_missing"));
	}

	FWBRuntimeActivationProviderExecutionResult Result;
	Result.PostActivationProviderResult = Provider.GetActivationDecisionData(PostActivationRequest);
	if (!Result.PostActivationProviderResult.bOk)
	{
		Result.Reason = ProviderFailureReason(Result.PostActivationProviderResult);
		return Result;
	}

	Result.SessionExecutionResult = SessionFacade->ExecuteActivationActionIdAndRefreshFromExternalData(
		State,
		SelectedActivationActionId,
		Result.PostActivationProviderResult.RefreshInput,
		Options);
	Result.bOk = Result.SessionExecutionResult.bOk;
	Result.Reason = Result.SessionExecutionResult.Reason;
	return Result;
}
