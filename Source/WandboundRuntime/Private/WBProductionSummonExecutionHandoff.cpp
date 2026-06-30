#include "WBProductionSummonExecutionHandoff.h"

namespace
{
FWBProductionSummonExecutionHandoffResult MakeFailure(
	const FWBProductionSummonExecutionRequest& Request,
	const TCHAR* Reason)
{
	FWBProductionSummonExecutionHandoffResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.SourceInstanceId = Request.SourceInstanceId;
	Result.SourceCardId = Request.SourceCardId;
	Result.TargetTile = Request.TargetTile;
	return Result;
}

bool TilesContain(const TArray<FWBTile>& Tiles, const FWBTile& TargetTile)
{
	for (const FWBTile& Tile : Tiles)
	{
		if (Tile == TargetTile)
		{
			return true;
		}
	}

	return false;
}

const FWBProductionSummonOption* FindMatchingSummonOption(
	const FWBProductionSummonEquipDecisionData& DecisionData,
	const FWBProductionSummonExecutionRequest& Request)
{
	for (const FWBProductionSummonOption& Option : DecisionData.SummonOptions)
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

FWBProductionSummonExecutionHandoffResult
FWBProductionSummonExecutionHandoff::ExecuteSummonFromProviderOption(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBProductionSummonEquipDecisionData& CurrentDecisionData,
	const FWBProductionSummonExecutionRequest& Request) const
{
	if (!FWBGameStateData::IsValidPlayerId(Request.ViewerPlayerId)
		|| State.GetPlayerById(Request.ViewerPlayerId) == nullptr)
	{
		return MakeFailure(Request, TEXT("invalid_viewer_player"));
	}

	if (CurrentDecisionData.ViewerPlayerId != Request.ViewerPlayerId)
	{
		return MakeFailure(Request, TEXT("summon_option_missing"));
	}

	const FWBProductionSummonOption* Option = FindMatchingSummonOption(CurrentDecisionData, Request);
	if (Option == nullptr)
	{
		return MakeFailure(Request, TEXT("summon_option_missing"));
	}

	if (!TilesContain(Option->LegalTiles, Request.TargetTile))
	{
		return MakeFailure(Request, TEXT("target_tile_not_allowed"));
	}

	FWBSummonExecutionRequest CoreRequest;
	CoreRequest.PlayerId = Request.ViewerPlayerId;
	CoreRequest.SourceInstanceId = Request.SourceInstanceId;
	CoreRequest.SourceCardId = Request.SourceCardId;
	CoreRequest.TargetTile = Request.TargetTile;

	const FWBSummonExecutionResult CoreResult =
		WBSummonExecution::ExecuteCharacterSummonFromHand(State, Repository, CoreRequest);
	if (!CoreResult.bOk)
	{
		FWBProductionSummonExecutionHandoffResult Result = MakeFailure(Request, *CoreResult.Reason);
		Result.CreatedUnitId = CoreResult.CreatedUnitId;
		Result.TraceEvents = CoreResult.TraceEvents;
		return Result;
	}

	FWBProductionSummonExecutionHandoffResult Result;
	Result.bOk = true;
	Result.Reason = WBSummonExecution::ResultCodeToString(EWBSummonExecutionResultCode::Success);
	Result.CreatedUnitId = CoreResult.CreatedUnitId;
	Result.SourceInstanceId = Request.SourceInstanceId;
	Result.SourceCardId = Request.SourceCardId;
	Result.TargetTile = Request.TargetTile;
	Result.TraceEvents = CoreResult.TraceEvents;
	Result.RefreshedSummonEquipData =
		FWBProductionSummonEquipDataProvider().BuildDecisionData(
			State,
			Repository,
			Request.ViewerPlayerId);
	return Result;
}
