#include "WBProductionResonanceOverflowHandoff.h"

namespace
{
FWBProductionResonanceOverflowHandoffResult MakeFailure(
	const FWBProductionResonanceOverflowRequest& Request,
	const TCHAR* Reason)
{
	FWBProductionResonanceOverflowHandoffResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.ViewerPlayerId = Request.ViewerPlayerId;
	Result.UnitId = Request.UnitId;
	return Result;
}

FWBProductionResonanceOverflowHandoffResult MakeFailure(
	const FWBProductionResonanceOverflowRequest& Request,
	const FString& Reason)
{
	FWBProductionResonanceOverflowHandoffResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.ViewerPlayerId = Request.ViewerPlayerId;
	Result.UnitId = Request.UnitId;
	return Result;
}
}

FWBProductionResonanceOverflowHandoffResult
FWBProductionResonanceOverflowHandoff::ResolveOverflowAndRefresh(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBProductionSummonEquipDecisionData& CurrentDecisionData,
	const FWBProductionResonanceOverflowRequest& Request) const
{
	if (!FWBGameStateData::IsValidPlayerId(Request.ViewerPlayerId)
		|| State.GetPlayerById(Request.ViewerPlayerId) == nullptr)
	{
		return MakeFailure(Request, TEXT("invalid_viewer_player"));
	}

	if (CurrentDecisionData.ViewerPlayerId != Request.ViewerPlayerId)
	{
		return MakeFailure(Request, TEXT("stale_provider_data"));
	}

	const FWBUnitState* Unit = State.GetUnitById(Request.UnitId);
	if (Unit == nullptr || !Unit->IsUnitOnBoard())
	{
		return MakeFailure(Request, TEXT("target_unit_not_found"));
	}

	if (Unit->OwnerId != Request.ViewerPlayerId)
	{
		return MakeFailure(Request, TEXT("target_unit_not_owned"));
	}

	if (Request.ExpectedRLUsedBeforeResolution != INDEX_NONE
		&& Unit->RLUsed != Request.ExpectedRLUsedBeforeResolution)
	{
		FWBProductionResonanceOverflowHandoffResult Result = MakeFailure(Request, TEXT("stale_provider_data"));
		Result.RLUsedBefore = Unit->RLUsed;
		Result.RLUsedAfter = Unit->RLUsed;
		return Result;
	}

	const FWBResonanceOverflowResult CoreResult =
		WBResonanceOverflow::ResolveOverflowForUnit(
			State,
			Repository,
			Request.UnitId);
	if (!CoreResult.bOk)
	{
		FWBProductionResonanceOverflowHandoffResult Result =
			MakeFailure(Request, CoreResult.Reason);
		Result.RLUsedBefore = CoreResult.Plan.RLUsedBefore;
		Result.RLUsedAfter = CoreResult.Plan.RLUsedAfter;
		Result.TraceEvents = CoreResult.TraceEvents;
		return Result;
	}

	FWBProductionResonanceOverflowHandoffResult Result;
	Result.bOk = true;
	Result.bResolvedOverflow = CoreResult.bResolvedOverflow;
	Result.Reason = WBResonanceOverflow::ResultCodeToString(EWBResonanceOverflowResultCode::Success);
	Result.ViewerPlayerId = Request.ViewerPlayerId;
	Result.UnitId = Request.UnitId;
	Result.RLUsedBefore = CoreResult.Plan.RLUsedBefore;
	Result.RLUsedAfter = CoreResult.Plan.RLUsedAfter;
	Result.TraceEvents = CoreResult.TraceEvents;
	Result.RefreshedSummonEquipData =
		FWBProductionSummonEquipDataProvider().BuildDecisionData(
			State,
			Repository,
			Request.ViewerPlayerId);
	return Result;
}
