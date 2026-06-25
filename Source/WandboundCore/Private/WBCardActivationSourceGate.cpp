#include "WBCardActivationSourceGate.h"

namespace
{
FWBCardActivationSourceGateResult MakeSourceGateFailure(const TCHAR* Reason)
{
	FWBCardActivationSourceGateResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBCardActivationSourceGateResult MakeSourceGateSuccess()
{
	FWBCardActivationSourceGateResult Result;
	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}

bool IsUnsupportedSourceZone(const EWBCardActivationSourceZone SourceZone)
{
	return SourceZone == EWBCardActivationSourceZone::Unknown
		|| SourceZone == EWBCardActivationSourceZone::Deck
		|| SourceZone == EWBCardActivationSourceZone::Discard;
}

FWBCardActivationSourceGateResult EvaluateCostGate(
	const FWBCardActivationCostGateDefinition& CostGate,
	const FWBCardActivationCostGateContext& CostContext)
{
	if (CostGate.RequiredRR < 0
		|| CostContext.SuppliedRequiredRR < 0
		|| CostContext.SuppliedAvailableRL < 0)
	{
		return MakeSourceGateFailure(TEXT("invalid_cost_requirement"));
	}

	if (!CostGate.bRequiresExternalAffordability)
	{
		return MakeSourceGateSuccess();
	}

	if (!CostContext.bHasExternalAffordability)
	{
		return MakeSourceGateFailure(TEXT("cost_affordability_missing"));
	}

	if (!CostContext.bExternallyAffordable)
	{
		return MakeSourceGateFailure(TEXT("cost_not_affordable"));
	}

	if (CostGate.RequiredRR > 0
		&& CostContext.SuppliedRequiredRR > 0
		&& CostGate.RequiredRR != CostContext.SuppliedRequiredRR)
	{
		return MakeSourceGateFailure(TEXT("cost_requirement_mismatch"));
	}

	if (CostContext.SuppliedRequiredRR > 0
		&& CostContext.SuppliedAvailableRL < CostContext.SuppliedRequiredRR)
	{
		return MakeSourceGateFailure(TEXT("cost_not_affordable"));
	}

	return MakeSourceGateSuccess();
}

FWBCardActivationSourceGateResult EvaluateSourceUnitState(
	const FWBGameStateData& State,
	const FWBCardActivationSourceGateDefinition& Gate,
	const FWBCardActivationSourceGateContext& Context)
{
	if (Gate.bRequiresSourceUnit && Context.SourceUnitId == -1)
	{
		return MakeSourceGateFailure(TEXT("source_unit_required"));
	}

	if (Context.SourceUnitId == -1)
	{
		return MakeSourceGateSuccess();
	}

	const FWBUnitState* SourceUnit = State.GetUnitById(Context.SourceUnitId);
	if (SourceUnit == nullptr)
	{
		return MakeSourceGateFailure(TEXT("source_unit_missing"));
	}

	if (!SourceUnit->IsUnitOnBoard())
	{
		return MakeSourceGateFailure(TEXT("source_unit_removed"));
	}

	if (Gate.bRequiresSourceUnitOwnership && SourceUnit->OwnerId != Context.PlayerId)
	{
		return MakeSourceGateFailure(TEXT("source_unit_owner_mismatch"));
	}

	if (Gate.bBlockedByStunned && SourceUnit->HasStatus(FName(TEXT("Stunned"))))
	{
		return MakeSourceGateFailure(TEXT("source_unit_stunned"));
	}

	if (Gate.bBlockedByFrozen && SourceUnit->HasStatus(FName(TEXT("Frozen"))))
	{
		return MakeSourceGateFailure(TEXT("source_unit_frozen"));
	}

	return MakeSourceGateSuccess();
}
}

FWBCardActivationSourceGateResult WBCardActivationSourceGate::Evaluate(
	const FWBGameStateData& State,
	const FWBCardActivationSourceGateDefinition& Gate,
	const FWBCardActivationSourceGateContext& Context)
{
	if (!FWBGameStateData::IsValidPlayerId(Context.PlayerId))
	{
		return MakeSourceGateFailure(TEXT("invalid_player"));
	}

	if (State.bGameOver)
	{
		return MakeSourceGateFailure(TEXT("game_over"));
	}

	const bool bUnsupportedSourceZone = IsUnsupportedSourceZone(Context.SourceZone);
	if (bUnsupportedSourceZone
		&& !(Gate.bRequiresFixtureZoneOwnership
			&& (Context.SourceZone == EWBCardActivationSourceZone::Discard
				|| Context.SourceZone == EWBCardActivationSourceZone::Deck)))
	{
		return MakeSourceGateFailure(TEXT("source_zone_mismatch"));
	}

	if (Gate.RequiredZone != EWBCardActivationSourceZone::Fixture
		&& Gate.RequiredZone != Context.SourceZone)
	{
		return MakeSourceGateFailure(TEXT("source_zone_mismatch"));
	}

	if (Gate.bRequiresFixtureZoneOwnership)
	{
		const FWBCardActivationSourceGateResult SourceZoneResult =
			Gate.RequiredZone == EWBCardActivationSourceZone::Board
				? EvaluateBoardSourceParity(State, Gate, Context)
				: EvaluateFixtureZoneOwnership(Gate, Context);
		if (!SourceZoneResult.bOk)
		{
			return SourceZoneResult;
		}
	}

	if (Gate.Timing == EWBCardActivationTimingRequirement::ResponseWindow)
	{
		return MakeSourceGateFailure(TEXT("response_window_not_supported"));
	}

	if (Gate.Timing == EWBCardActivationTimingRequirement::NormalTurnPriority
		&& (!State.IsNormalTurnPhase()
			|| State.CurrentPlayer != Context.PlayerId
			|| State.PriorityPlayer != Context.PlayerId))
	{
		return MakeSourceGateFailure(TEXT("timing_not_normal_turn_priority"));
	}

	if (Gate.Timing == EWBCardActivationTimingRequirement::Unknown)
	{
		return MakeSourceGateFailure(TEXT("timing_not_normal_turn_priority"));
	}

	const FWBCardActivationSourceGateResult SourceUnitResult =
		EvaluateSourceUnitState(State, Gate, Context);
	if (!SourceUnitResult.bOk)
	{
		return SourceUnitResult;
	}

	if (Gate.bRequiresCostsSatisfiedExternally && !Context.bCostsSatisfiedExternally)
	{
		return MakeSourceGateFailure(TEXT("costs_not_satisfied"));
	}

	const FWBCardActivationSourceGateResult CostGateResult = EvaluateCostGate(Gate.CostGate, Context.CostContext);
	if (!CostGateResult.bOk)
	{
		return CostGateResult;
	}

	if (Gate.bOncePerTurn)
	{
		const FString UsageKey = !Context.ActivationUsageKey.IsEmpty()
			? Context.ActivationUsageKey
			: Gate.OncePerTurnKey;
		if (UsageKey.IsEmpty())
		{
			return MakeSourceGateFailure(TEXT("usage_key_missing"));
		}

		if (State.HasActivationUsageKeyThisTurn(Context.PlayerId, UsageKey))
		{
			return MakeSourceGateFailure(TEXT("once_per_turn_already_used"));
		}
	}

	return MakeSourceGateSuccess();
}

FWBCardActivationSourceGateResult WBCardActivationSourceGate::EvaluateFixtureZoneOwnership(
	const FWBCardActivationSourceGateDefinition& Gate,
	const FWBCardActivationSourceGateContext& Context)
{
	if (!Gate.bRequiresFixtureZoneOwnership)
	{
		return MakeSourceGateSuccess();
	}

	if (Context.SourceCardId.IsEmpty())
	{
		return MakeSourceGateFailure(TEXT("source_card_id_missing"));
	}

	if (Gate.RequiredZone == EWBCardActivationSourceZone::Unknown)
	{
		return MakeSourceGateFailure(TEXT("source_zone_unknown"));
	}

	if (Gate.RequiredZone == EWBCardActivationSourceZone::Fixture)
	{
		return MakeSourceGateSuccess();
	}

	int32 MatchingEntryCount = 0;
	FWBCardActivationFixtureZoneEntry MatchingEntry;
	for (const FWBCardActivationFixtureZoneEntry& Entry : Context.FixtureZoneContext.Entries)
	{
		if (Entry.CardId == Context.SourceCardId
			&& Entry.OwnerPlayerId == Context.PlayerId
			&& Entry.Zone == Gate.RequiredZone)
		{
			++MatchingEntryCount;
			MatchingEntry = Entry;
		}
	}

	if (MatchingEntryCount == 0)
	{
		return MakeSourceGateFailure(TEXT("source_zone_card_not_found"));
	}

	if (MatchingEntryCount > 1)
	{
		return MakeSourceGateFailure(TEXT("source_zone_card_ambiguous"));
	}

	if (Gate.RequiredZone == EWBCardActivationSourceZone::Deck)
	{
		return MakeSourceGateFailure(TEXT("source_zone_deck_not_supported"));
	}

	if (Gate.RequiredZone == EWBCardActivationSourceZone::Equipped)
	{
		if (Context.SourceUnitId == -1)
		{
			return MakeSourceGateFailure(TEXT("equipped_source_unit_required"));
		}

		if (MatchingEntry.EquippedToUnitId != Context.SourceUnitId)
		{
			return MakeSourceGateFailure(TEXT("equipped_source_unit_mismatch"));
		}
	}

	return MakeSourceGateSuccess();
}

FWBCardActivationSourceGateResult WBCardActivationSourceGate::EvaluateBoardSourceParity(
	const FWBGameStateData& State,
	const FWBCardActivationSourceGateDefinition& Gate,
	const FWBCardActivationSourceGateContext& Context)
{
	if (Gate.RequiredZone != EWBCardActivationSourceZone::Board
		|| !Gate.bRequiresFixtureZoneOwnership)
	{
		return MakeSourceGateSuccess();
	}

	if (Context.SourceUnitId == -1)
	{
		return MakeSourceGateFailure(TEXT("board_source_unit_required"));
	}

	if (Context.SourceCardId.IsEmpty())
	{
		return MakeSourceGateFailure(TEXT("board_source_card_id_missing"));
	}

	const FWBUnitState* SourceUnit = State.GetUnitById(Context.SourceUnitId);
	if (SourceUnit == nullptr)
	{
		return MakeSourceGateFailure(TEXT("board_source_unit_missing"));
	}

	if (SourceUnit->bDefeated || SourceUnit->bRemovedFromBoard)
	{
		return MakeSourceGateFailure(TEXT("board_source_unit_removed"));
	}

	if (!SourceUnit->IsUnitOnBoard())
	{
		return MakeSourceGateFailure(TEXT("board_source_unit_not_on_board"));
	}

	if (Gate.bRequiresSourceUnitOwnership && SourceUnit->OwnerId != Context.PlayerId)
	{
		return MakeSourceGateFailure(TEXT("board_source_owner_mismatch"));
	}

	if (SourceUnit->CardId.IsEmpty())
	{
		return MakeSourceGateFailure(TEXT("board_source_unit_card_id_missing"));
	}

	if (SourceUnit->CardId != Context.SourceCardId)
	{
		return MakeSourceGateFailure(TEXT("board_source_card_id_mismatch"));
	}

	return MakeSourceGateSuccess();
}

FString WBCardActivationSourceGate::BuildDefaultUsageKey(
	const int32 PlayerId,
	const int32 SourceUnitId,
	const FString& CardId,
	const FString& EffectId)
{
	return FString::Printf(
		TEXT("activate_usage:p%d:u%d:c%s:e%s"),
		PlayerId,
		SourceUnitId,
		*CardId,
		*EffectId);
}

bool WBCardActivationSourceGate::MarkUsageIfAllowedForTest(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& UsageKey,
	FString& OutReason)
{
	if (!FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		OutReason = TEXT("invalid_player");
		return false;
	}

	if (UsageKey.IsEmpty())
	{
		OutReason = TEXT("usage_key_missing");
		return false;
	}

	State.MarkActivationUsageKeyForTest(PlayerId, UsageKey);
	OutReason.Reset();
	return true;
}
