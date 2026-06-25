#include "WBCardActivationCandidateGenerator.h"

#include "WBCardActivationExpansion.h"
#include "WBCardActivationSourceGate.h"

namespace
{
FWBCardActivationCandidateGenerationResult MakeGenerationFailure(const TCHAR* Reason)
{
	FWBCardActivationCandidateGenerationResult Result;
	Result.Reason = Reason;
	return Result;
}

bool IsDynamicSourceExcluded(const FWBUnitState& Unit)
{
	return Unit.bRemovedFromBoard
		|| Unit.bDefeated
		|| Unit.HasStatus(FName(TEXT("Stunned")))
		|| Unit.HasStatus(FName(TEXT("Frozen")));
}

bool ShouldApplyLegacyDynamicSourceExclusion(
	const FWBCardActivationCandidateSource& Source,
	const FWBCardEffectDefinition& Effect)
{
	return !Source.SourceGateContext.bHasExplicitSourceGateContext
		&& !Effect.SourceGate.bHasExplicitSourceGate;
}

FWBCardActivationSourceGateContext BuildGateContext(
	const FWBCardActivationCandidateSource& Source,
	const FWBCardEffectDefinition& Effect)
{
	const FWBCardActivationSourceGateContext* EffectContext =
		Source.EffectIdToSourceGateContext.Find(Effect.EffectId);
	FWBCardActivationSourceGateContext Context = EffectContext != nullptr
		? *EffectContext
		: Source.SourceGateContext;
	if (!FWBGameStateData::IsValidPlayerId(Context.PlayerId))
	{
		Context.PlayerId = Source.PlayerId;
	}

	if (Context.SourceUnitId == -1)
	{
		Context.SourceUnitId = Source.SourceUnitId;
	}

	if (Context.SourceCardId.IsEmpty())
	{
		Context.SourceCardId = !Source.SourceGateContext.SourceCardId.IsEmpty()
			? Source.SourceGateContext.SourceCardId
			: Source.CardDefinition.CardId;
	}

	if (Context.SourceZone == EWBCardActivationSourceZone::Fixture
		&& Source.SourceGateContext.SourceZone != EWBCardActivationSourceZone::Fixture)
	{
		Context.SourceZone = Source.SourceGateContext.SourceZone;
	}

	if (Context.FixtureZoneContext.Entries.IsEmpty()
		&& !Source.SourceGateContext.FixtureZoneContext.Entries.IsEmpty())
	{
		Context.FixtureZoneContext = Source.SourceGateContext.FixtureZoneContext;
	}

	if (Effect.SourceGate.bOncePerTurn
		&& Context.ActivationUsageKey.IsEmpty()
		&& Effect.SourceGate.OncePerTurnKey.IsEmpty())
	{
		Context.ActivationUsageKey = WBCardActivationSourceGate::BuildDefaultUsageKey(
			Context.PlayerId,
			Context.SourceUnitId,
			Source.CardDefinition.CardId,
			Effect.EffectId);
	}

	return Context;
}

bool IsDynamicTargetExcluded(
	const FWBGameStateData& State,
	const FWBCardEffectDefinition& Effect,
	const FWBEffectTargetRef& Target)
{
	if (Effect.TargetRequirement != EWBCardEffectTargetRequirement::Unit)
	{
		return false;
	}

	const FWBUnitState* TargetUnit = State.GetUnitById(Target.TargetUnitId);
	return TargetUnit == nullptr
		|| TargetUnit->bRemovedFromBoard
		|| TargetUnit->bDefeated;
}

bool IsTargetMismatchReason(const FString& Reason)
{
	return Reason == TEXT("missing_unit_target")
		|| Reason == TEXT("missing_tile_target")
		|| Reason == TEXT("missing_wall_edge_target");
}

bool IsMalformedCardReason(const FString& Reason)
{
	return Reason == TEXT("missing_card_id")
		|| Reason == TEXT("missing_effect_id")
		|| Reason == TEXT("effect_id_ambiguous")
		|| Reason == TEXT("empty_effect_payloads")
		|| Reason == TEXT("unknown_effect_payload_operation")
		|| Reason == TEXT("unknown_target_requirement");
}

FString BuildPublicLabel(const FWBCardEffectDefinition& Effect)
{
	if (!Effect.PublicLabel.IsEmpty())
	{
		return Effect.PublicLabel;
	}

	return TEXT("Activate");
}

FString FormatTargetPart(const FWBEffectTargetRef& Target)
{
	return FString::Printf(
		TEXT("t%d:x%d:y%d:wa%d,%d:wb%d,%d"),
		Target.TargetUnitId,
		Target.TargetTile.X,
		Target.TargetTile.Y,
		Target.TargetWallEdge.A.X,
		Target.TargetWallEdge.A.Y,
		Target.TargetWallEdge.B.X,
		Target.TargetWallEdge.B.Y);
}

FWBCardActivationCandidate MakeCandidate(
	const FWBCardDefinition& CardDefinition,
	const FWBCardEffectDefinition& Effect,
	const FWBEffectTargetRef& Target,
	const FWBCardActivationCommand& Command,
	const int32 PlayerId,
	const int32 SourceUnitId)
{
	FWBCardActivationCandidate Candidate;
	Candidate.PlayerId = PlayerId;
	Candidate.SourceUnitId = SourceUnitId;
	Candidate.SourceCardId = CardDefinition.CardId;
	Candidate.SourceEffectId = Effect.EffectId;
	Candidate.PublicLabel = BuildPublicLabel(Effect);
	Candidate.Target = Target;
	Candidate.Command = Command;
	Candidate.ActivationCandidateId = WBCardActivationCandidateGenerator::MakeActivationCandidateId(Candidate);
	return Candidate;
}
}

FWBCardActivationCandidateGenerationResult WBCardActivationCandidateGenerator::GenerateCandidates(
	const FWBGameStateData& State,
	const TArray<FWBCardActivationCandidateSource>& Sources)
{
	FWBCardActivationCandidateGenerationResult Result;

	for (const FWBCardActivationCandidateSource& Source : Sources)
	{
		if (!FWBGameStateData::IsValidPlayerId(Source.PlayerId))
		{
			return MakeGenerationFailure(TEXT("bad_player"));
		}

		if (State.GetPlayerById(Source.PlayerId) == nullptr)
		{
			return MakeGenerationFailure(TEXT("missing_player"));
		}

		if (Source.CardDefinition.CardId.IsEmpty())
		{
			return MakeGenerationFailure(TEXT("missing_card_id"));
		}

		if (Source.SourceUnitId != -1)
		{
			const FWBUnitState* SourceUnit = State.GetUnitById(Source.SourceUnitId);
			if (SourceUnit == nullptr)
			{
				return MakeGenerationFailure(TEXT("missing_source_unit"));
			}

			if (SourceUnit->OwnerId != Source.PlayerId)
			{
				return MakeGenerationFailure(TEXT("source_unit_owner_mismatch"));
			}
		}

		if (Source.CandidateTargets.IsEmpty())
		{
			continue;
		}

		for (const FWBCardEffectDefinition& Effect : Source.CardDefinition.ActivatedEffects)
		{
			const FWBUnitState* SourceUnit = Source.SourceUnitId != -1
				? State.GetUnitById(Source.SourceUnitId)
				: nullptr;
			if (SourceUnit != nullptr
				&& ShouldApplyLegacyDynamicSourceExclusion(Source, Effect)
				&& IsDynamicSourceExcluded(*SourceUnit))
			{
				continue;
			}

			const FWBCardActivationSourceGateContext GateContext = BuildGateContext(Source, Effect);
			const FWBCardActivationSourceGateResult GateResult =
				WBCardActivationSourceGate::Evaluate(State, Effect.SourceGate, GateContext);
			if (!GateResult.bOk)
			{
				continue;
			}

			for (const FWBEffectTargetRef& Target : Source.CandidateTargets)
			{
				if (IsDynamicTargetExcluded(State, Effect, Target))
				{
					continue;
				}

				FWBCardActivationExpansionRequest Request;
				Request.PlayerId = Source.PlayerId;
				Request.SourceUnitId = Source.SourceUnitId;
				Request.CardDefinition = Source.CardDefinition;
				Request.EffectId = Effect.EffectId;
				Request.Target = Target;
				Request.SourceGateContext = GateContext;

				const FWBCardActivationExpansionResult ExpansionResult =
					WBCardActivationExpansion::BuildActivationCommand(Request);
				if (ExpansionResult.bOk)
				{
					Result.Candidates.Add(MakeCandidate(
						Source.CardDefinition,
						Effect,
						Target,
						ExpansionResult.Command,
						Source.PlayerId,
						Source.SourceUnitId));
					continue;
				}

				if (IsTargetMismatchReason(ExpansionResult.Reason)
					|| ExpansionResult.Reason == TEXT("effect_id_not_found"))
				{
					continue;
				}

				if (IsMalformedCardReason(ExpansionResult.Reason))
				{
					return MakeGenerationFailure(*ExpansionResult.Reason);
				}

				return MakeGenerationFailure(*ExpansionResult.Reason);
			}
		}
	}

	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}

FString WBCardActivationCandidateGenerator::MakeActivationCandidateId(const FWBCardActivationCandidate& Candidate)
{
	return FString::Printf(
		TEXT("activate:p%d:u%d:c%s:e%s:%s"),
		Candidate.PlayerId,
		Candidate.SourceUnitId,
		*Candidate.SourceCardId,
		*Candidate.SourceEffectId,
		*FormatTargetPart(Candidate.Target));
}
