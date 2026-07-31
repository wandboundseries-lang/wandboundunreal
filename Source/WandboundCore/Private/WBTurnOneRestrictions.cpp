#include "WBTurnOneRestrictions.h"

#include "WBBoardRegion.h"

FWBTurnOneRestrictionQuery FWBTurnOneRestrictionQuery::Allow()
{
	FWBTurnOneRestrictionQuery Result;
	Result.bOk = true;
	return Result;
}

FWBTurnOneRestrictionQuery FWBTurnOneRestrictionQuery::Deny(
	const TCHAR* Reason)
{
	FWBTurnOneRestrictionQuery Result;
	Result.Reason = Reason;
	return Result;
}

bool WBTurnOneRestrictions::IsFirstPlayerTurnOneRestrictionActive(
	const FWBGameStateData& State)
{
	return !State.bInitialSetupInProgress
		&& FWBGameStateData::IsValidPlayerId(State.FirstPlayerId)
		&& State.TurnNumber == 1
		&& State.CurrentPlayer == State.FirstPlayerId;
}

FWBTurnOneRestrictionQuery WBTurnOneRestrictions::QuerySummonPlacement(
	const FWBGameStateData& State,
	const int32 SummoningPlayerId,
	const FWBTile& Destination)
{
	if (!IsFirstPlayerTurnOneRestrictionActive(State)
		|| SummoningPlayerId != State.FirstPlayerId)
	{
		return FWBTurnOneRestrictionQuery::Allow();
	}
	return WBBoardRegion::IsOpponentHalfForPlayer(
		State.FirstPlayerId,
		Destination)
		? FWBTurnOneRestrictionQuery::Deny(
			TEXT("first_player_turn_one_summon_into_opponent_half"))
		: FWBTurnOneRestrictionQuery::Allow();
}

FWBTurnOneRestrictionQuery WBTurnOneRestrictions::QueryRelocation(
	const FWBGameStateData& State,
	const TArray<FWBRelocationStep>& Steps)
{
	if (!IsFirstPlayerTurnOneRestrictionActive(State))
	{
		return FWBTurnOneRestrictionQuery::Allow();
	}

	for (const FWBRelocationStep& Step : Steps)
	{
		const EWBPlayerRelativeBoardRegion From =
			WBBoardRegion::GetBoardRegionForPlayer(
				State.FirstPlayerId,
				Step.FromTile);
		const EWBPlayerRelativeBoardRegion To =
			WBBoardRegion::GetBoardRegionForPlayer(
				State.FirstPlayerId,
				Step.ToTile);
		if (From == EWBPlayerRelativeBoardRegion::Invalid
			|| To == EWBPlayerRelativeBoardRegion::Invalid
			|| From != To)
		{
			return FWBTurnOneRestrictionQuery::Deny(
				TEXT("first_player_turn_one_protected_boundary_crossing"));
		}
	}
	return FWBTurnOneRestrictionQuery::Allow();
}

FWBTurnOneRestrictionQuery WBTurnOneRestrictions::QueryAttackTarget(
	const FWBGameStateData& State,
	const int32 AttackingPlayerId,
	const int32 TargetOwnerId)
{
	if (!IsFirstPlayerTurnOneRestrictionActive(State)
		|| AttackingPlayerId != State.FirstPlayerId
		|| TargetOwnerId == -1)
	{
		return FWBTurnOneRestrictionQuery::Allow();
	}
	return TargetOwnerId == (1 - State.FirstPlayerId)
		? FWBTurnOneRestrictionQuery::Deny(
			TEXT("first_player_turn_one_opponent_controlled_attack_forbidden"))
		: FWBTurnOneRestrictionQuery::Allow();
}
