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

	if (IsUnsupportedSourceZone(Context.SourceZone)
		|| Gate.RequiredZone == EWBCardActivationSourceZone::Unknown)
	{
		return MakeSourceGateFailure(TEXT("source_zone_mismatch"));
	}

	if (Gate.RequiredZone != EWBCardActivationSourceZone::Fixture
		&& Gate.RequiredZone != Context.SourceZone)
	{
		return MakeSourceGateFailure(TEXT("source_zone_mismatch"));
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

	if (Gate.bRequiresSourceUnit && Context.SourceUnitId == -1)
	{
		return MakeSourceGateFailure(TEXT("source_unit_required"));
	}

	const FWBUnitState* SourceUnit = nullptr;
	if (Context.SourceUnitId != -1)
	{
		SourceUnit = State.GetUnitById(Context.SourceUnitId);
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
	}

	if (Gate.bRequiresCostsSatisfiedExternally && !Context.bCostsSatisfiedExternally)
	{
		return MakeSourceGateFailure(TEXT("costs_not_satisfied"));
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
