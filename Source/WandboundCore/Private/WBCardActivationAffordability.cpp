#include "WBCardActivationAffordability.h"

namespace
{
FWBCardActivationAffordabilityResult MakeAffordabilityFailure(
	const FWBCardActivationAffordabilityRequest& Request,
	const TCHAR* Reason)
{
	FWBCardActivationAffordabilityResult Result;
	Result.Reason = Reason;
	Result.PlayerId = Request.PlayerId;
	Result.SourceUnitId = Request.SourceUnitId;
	Result.RequiredRR = Request.RequiredRR;
	Result.CostKind = Request.CostKind;
	return Result;
}

FWBCardActivationSourceGateContext BuildBaseContextForEffect(
	const FWBCardActivationCandidateSource& Source,
	const FWBCardEffectDefinition& Effect)
{
	if (const FWBCardActivationSourceGateContext* EffectContext =
		Source.EffectIdToSourceGateContext.Find(Effect.EffectId))
	{
		return *EffectContext;
	}

	return Source.SourceGateContext;
}

void FillContextIdentityFromSource(
	const FWBCardActivationCandidateSource& Source,
	FWBCardActivationSourceGateContext& Context)
{
	if (!FWBGameStateData::IsValidPlayerId(Context.PlayerId))
	{
		Context.PlayerId = Source.PlayerId;
	}

	if (Context.SourceUnitId == -1)
	{
		Context.SourceUnitId = Source.SourceUnitId;
	}
}
}

FWBCardActivationAffordabilityResult WBCardActivationAffordability::QueryFromUnitRL(
	const FWBGameStateData& State,
	const FWBCardActivationAffordabilityRequest& Request)
{
	if (!FWBGameStateData::IsValidPlayerId(Request.PlayerId))
	{
		return MakeAffordabilityFailure(Request, TEXT("invalid_player"));
	}

	if (Request.RequiredRR < 0)
	{
		return MakeAffordabilityFailure(Request, TEXT("invalid_cost_requirement"));
	}

	const FWBUnitState* SourceUnit = State.GetUnitById(Request.SourceUnitId);
	if (SourceUnit == nullptr)
	{
		return MakeAffordabilityFailure(Request, TEXT("source_unit_missing"));
	}

	if (!SourceUnit->IsUnitOnBoard() || SourceUnit->bDefeated)
	{
		return MakeAffordabilityFailure(Request, TEXT("source_unit_removed"));
	}

	if (SourceUnit->OwnerId != Request.PlayerId)
	{
		return MakeAffordabilityFailure(Request, TEXT("source_unit_owner_mismatch"));
	}

	FWBCardActivationAffordabilityResult Result;
	Result.bOk = true;
	Result.PlayerId = Request.PlayerId;
	Result.SourceUnitId = Request.SourceUnitId;
	Result.RequiredRR = Request.RequiredRR;
	Result.CurrentRL = SourceUnit->RLTotal;
	Result.RLUsed = SourceUnit->RLUsed;
	Result.AvailableRL = FMath::Max(0, Result.CurrentRL - Result.RLUsed);
	Result.bAffordable = Result.AvailableRL >= Result.RequiredRR;
	Result.CostKind = Request.CostKind;
	Result.Reason.Reset();
	return Result;
}

void WBCardActivationAffordability::ApplyResultToSourceGateContext(
	const FWBCardActivationAffordabilityResult& Result,
	FWBCardActivationSourceGateContext& Context)
{
	Context.CostContext.bHasExternalAffordability = Result.bOk;
	Context.CostContext.bExternallyAffordable = Result.bAffordable;
	Context.CostContext.SuppliedRequiredRR = Result.RequiredRR;
	Context.CostContext.SuppliedAvailableRL = Result.AvailableRL;
	Context.CostContext.CostKind = Result.CostKind;

	if (Result.bOk)
	{
		Context.bCostsSatisfiedExternally = Result.bAffordable;
	}
}

FWBCardActivationCandidateSource WBCardActivationAffordability::BuildCandidateSourceWithAffordability(
	const FWBGameStateData& State,
	const FWBCardActivationCandidateSource& Source,
	const IWBCardActivationAffordabilityProvider& Provider)
{
	FWBCardActivationCandidateSource PreparedSource = Source;

	for (const FWBCardEffectDefinition& Effect : Source.CardDefinition.ActivatedEffects)
	{
		if (!Effect.SourceGate.CostGate.bRequiresExternalAffordability)
		{
			continue;
		}

		FWBCardActivationSourceGateContext EffectContext = BuildBaseContextForEffect(Source, Effect);
		FillContextIdentityFromSource(Source, EffectContext);

		FWBCardActivationAffordabilityRequest Request;
		Request.PlayerId = EffectContext.PlayerId;
		Request.SourceUnitId = EffectContext.SourceUnitId;
		Request.SourceCardId = Source.CardDefinition.CardId;
		Request.SourceEffectId = Effect.EffectId;
		Request.RequiredRR = Effect.SourceGate.CostGate.RequiredRR;
		Request.CostKind = Effect.SourceGate.CostGate.CostKind;

		const FWBCardActivationAffordabilityResult Result =
			Provider.QueryAffordability(State, Request);
		if (Result.bOk)
		{
			WBCardActivationAffordability::ApplyResultToSourceGateContext(Result, EffectContext);
		}
		else
		{
			EffectContext.CostContext.bHasExternalAffordability = false;
			EffectContext.bCostsSatisfiedExternally = false;
		}

		EffectContext.bHasExplicitSourceGateContext = true;
		PreparedSource.EffectIdToSourceGateContext.Add(Effect.EffectId, EffectContext);
	}

	return PreparedSource;
}

FWBCardActivationAffordabilityResult FWBFixedCardActivationAffordabilityProvider::QueryAffordability(
	const FWBGameStateData& State,
	const FWBCardActivationAffordabilityRequest& Request) const
{
	(void)State;

	FWBCardActivationAffordabilityResult Result = FixedResult;
	if (!FWBGameStateData::IsValidPlayerId(Result.PlayerId))
	{
		Result.PlayerId = Request.PlayerId;
	}

	if (Result.SourceUnitId == -1)
	{
		Result.SourceUnitId = Request.SourceUnitId;
	}

	if (Result.RequiredRR == 0 && Request.RequiredRR != 0)
	{
		Result.RequiredRR = Request.RequiredRR;
	}

	if (Result.CostKind.IsNone())
	{
		Result.CostKind = Request.CostKind;
	}

	return Result;
}
