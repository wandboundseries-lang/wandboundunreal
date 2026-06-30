#include "WBProductionEquipExecutionHandoff.h"

namespace
{
FWBProductionEquipExecutionHandoffResult MakeFailure(
	const FWBProductionEquipExecutionRequest& Request,
	const TCHAR* Reason)
{
	FWBProductionEquipExecutionHandoffResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.SourceInstanceId = Request.SourceInstanceId;
	Result.SourceCardId = Request.SourceCardId;
	Result.EquippedToUnitId = Request.TargetUnitId;
	return Result;
}

bool UnitIdsContain(const TArray<int32>& UnitIds, const int32 TargetUnitId)
{
	return UnitIds.Contains(TargetUnitId);
}

const FWBProductionEquipOption* FindMatchingEquipOption(
	const FWBProductionSummonEquipDecisionData& DecisionData,
	const FWBProductionEquipExecutionRequest& Request)
{
	for (const FWBProductionEquipOption& Option : DecisionData.EquipOptions)
	{
		if (Option.SourceInstanceId == Request.SourceInstanceId
			&& Option.SourceCardId == Request.SourceCardId)
		{
			return &Option;
		}
	}

	return nullptr;
}
}

FWBProductionEquipExecutionHandoffResult
FWBProductionEquipExecutionHandoff::ExecuteEquipFromProviderOption(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBProductionSummonEquipDecisionData& CurrentDecisionData,
	const FWBProductionEquipExecutionRequest& Request) const
{
	if (!FWBGameStateData::IsValidPlayerId(Request.ViewerPlayerId)
		|| State.GetPlayerById(Request.ViewerPlayerId) == nullptr)
	{
		return MakeFailure(Request, TEXT("invalid_viewer_player"));
	}

	if (CurrentDecisionData.ViewerPlayerId != Request.ViewerPlayerId)
	{
		return MakeFailure(Request, TEXT("equip_option_missing"));
	}

	const FWBProductionEquipOption* Option = FindMatchingEquipOption(CurrentDecisionData, Request);
	if (Option == nullptr)
	{
		return MakeFailure(Request, TEXT("equip_option_missing"));
	}

	if (!UnitIdsContain(Option->EligibleUnitIds, Request.TargetUnitId))
	{
		return MakeFailure(Request, TEXT("target_unit_not_allowed"));
	}

	FWBEquipExecutionRequest CoreRequest;
	CoreRequest.PlayerId = Request.ViewerPlayerId;
	CoreRequest.SourceInstanceId = Request.SourceInstanceId;
	CoreRequest.SourceCardId = Request.SourceCardId;
	CoreRequest.TargetUnitId = Request.TargetUnitId;

	const FWBEquipExecutionResult CoreResult =
		WBEquipExecution::ExecuteWandEquipFromHand(State, Repository, CoreRequest);
	if (!CoreResult.bOk)
	{
		FWBProductionEquipExecutionHandoffResult Result = MakeFailure(Request, *CoreResult.Reason);
		Result.RR = CoreResult.RR;
		Result.RLUsedBefore = CoreResult.RLUsedBefore;
		Result.RLUsedAfter = CoreResult.RLUsedAfter;
		Result.TraceEvents = CoreResult.TraceEvents;
		return Result;
	}

	FWBProductionEquipExecutionHandoffResult Result;
	Result.bOk = true;
	Result.Reason = WBEquipExecution::ResultCodeToString(EWBEquipExecutionResultCode::Success);
	Result.EquippedToUnitId = CoreResult.EquippedToUnitId;
	Result.SourceInstanceId = Request.SourceInstanceId;
	Result.SourceCardId = Request.SourceCardId;
	Result.SlotId = CoreResult.SlotId;
	Result.RR = CoreResult.RR;
	Result.RLUsedBefore = CoreResult.RLUsedBefore;
	Result.RLUsedAfter = CoreResult.RLUsedAfter;
	Result.TraceEvents = CoreResult.TraceEvents;
	Result.RefreshedSummonEquipData =
		FWBProductionSummonEquipDataProvider().BuildDecisionData(
			State,
			Repository,
			Request.ViewerPlayerId);
	return Result;
}
