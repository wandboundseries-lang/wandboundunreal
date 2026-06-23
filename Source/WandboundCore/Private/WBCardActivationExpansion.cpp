#include "WBCardActivationExpansion.h"

namespace
{
FWBCardActivationExpansionResult MakeExpansionFailure(
	const FWBCardActivationExpansionRequest& Request,
	const TCHAR* Reason)
{
	FWBCardActivationExpansionResult Result;
	Result.Request = Request;
	Result.Reason = Reason;
	return Result;
}

bool IsKnownPayloadOperation(const EWBGenericEffectOp Operation)
{
	return Operation == EWBGenericEffectOp::ArmorEffect
		|| Operation == EWBGenericEffectOp::StatusEffect
		|| Operation == EWBGenericEffectOp::DamageEffect
		|| Operation == EWBGenericEffectOp::HealEffect;
}

bool IsValidTileCoordinate(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.Y >= 0;
}

bool IsValidWallEdgeCoordinate(const FWBWallEdge& Edge)
{
	return IsValidTileCoordinate(Edge.A)
		&& IsValidTileCoordinate(Edge.B)
		&& Edge.A != Edge.B;
}

bool DoesTargetMatchRequirement(
	const EWBCardEffectTargetRequirement Requirement,
	const FWBEffectTargetRef& Target,
	FString& OutReason)
{
	switch (Requirement)
	{
	case EWBCardEffectTargetRequirement::None:
		OutReason.Reset();
		return true;
	case EWBCardEffectTargetRequirement::Unit:
		if (Target.TargetUnitId < 0)
		{
			OutReason = TEXT("missing_unit_target");
			return false;
		}
		OutReason.Reset();
		return true;
	case EWBCardEffectTargetRequirement::Tile:
		if (!IsValidTileCoordinate(Target.TargetTile))
		{
			OutReason = TEXT("missing_tile_target");
			return false;
		}
		OutReason.Reset();
		return true;
	case EWBCardEffectTargetRequirement::WallEdge:
		if (!IsValidWallEdgeCoordinate(Target.TargetWallEdge))
		{
			OutReason = TEXT("missing_wall_edge_target");
			return false;
		}
		OutReason.Reset();
		return true;
	default:
		OutReason = TEXT("unknown_target_requirement");
		return false;
	}
}
}

FWBCardActivationExpansionResult WBCardActivationExpansion::BuildActivationCommand(
	const FWBCardActivationExpansionRequest& Request)
{
	if (Request.CardDefinition.CardId.IsEmpty())
	{
		return MakeExpansionFailure(Request, TEXT("missing_card_id"));
	}

	if (Request.EffectId.IsEmpty())
	{
		return MakeExpansionFailure(Request, TEXT("missing_effect_id"));
	}

	if (Request.PlayerId != 0 && Request.PlayerId != 1)
	{
		return MakeExpansionFailure(Request, TEXT("bad_player"));
	}

	const FWBCardEffectDefinition* MatchingEffect = nullptr;
	int32 MatchCount = 0;
	for (const FWBCardEffectDefinition& Effect : Request.CardDefinition.ActivatedEffects)
	{
		if (Effect.EffectId == Request.EffectId)
		{
			MatchingEffect = &Effect;
			++MatchCount;
		}
	}

	if (MatchCount == 0)
	{
		return MakeExpansionFailure(Request, TEXT("effect_id_not_found"));
	}

	if (MatchCount > 1)
	{
		return MakeExpansionFailure(Request, TEXT("effect_id_ambiguous"));
	}

	FString TargetReason;
	if (!DoesTargetMatchRequirement(MatchingEffect->TargetRequirement, Request.Target, TargetReason))
	{
		return MakeExpansionFailure(Request, *TargetReason);
	}

	if (MatchingEffect->Payloads.IsEmpty())
	{
		return MakeExpansionFailure(Request, TEXT("empty_effect_payloads"));
	}

	for (const FWBGenericEffectPayload& Payload : MatchingEffect->Payloads)
	{
		if (!IsKnownPayloadOperation(Payload.Operation))
		{
			return MakeExpansionFailure(Request, TEXT("unknown_effect_payload_operation"));
		}
	}

	FWBCardActivationExpansionResult Result;
	Result.bOk = true;
	Result.Request = Request;
	Result.Command.Source.PlayerId = Request.PlayerId;
	Result.Command.Source.SourceUnitId = Request.SourceUnitId;
	Result.Command.Source.SourceCardId = Request.CardDefinition.CardId;
	Result.Command.Source.SourceEffectId = MatchingEffect->EffectId;
	Result.Command.EffectRequest.Source.PlayerId = Request.PlayerId;
	Result.Command.EffectRequest.Source.SourceUnitId = Request.SourceUnitId;
	Result.Command.EffectRequest.Source.SourceCardId = Request.CardDefinition.CardId;
	Result.Command.EffectRequest.Source.SourceEffectId = MatchingEffect->EffectId;
	Result.Command.EffectRequest.Target = Request.Target;
	Result.Command.EffectRequest.Payloads = MatchingEffect->Payloads;
	Result.Command.DebugActivationId = Request.DebugActivationId;
	Result.Reason.Reset();
	return Result;
}

void WBCardActivationExpansion::BuildActivationCommandCandidates(
	const FWBCardDefinition& CardDefinition,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const TArray<FWBEffectTargetRef>& CandidateTargets,
	TArray<FWBCardActivationCommand>& OutCommands)
{
	OutCommands.Reset();

	for (const FWBCardEffectDefinition& Effect : CardDefinition.ActivatedEffects)
	{
		for (const FWBEffectTargetRef& Target : CandidateTargets)
		{
			FWBCardActivationExpansionRequest Request;
			Request.PlayerId = PlayerId;
			Request.SourceUnitId = SourceUnitId;
			Request.CardDefinition = CardDefinition;
			Request.EffectId = Effect.EffectId;
			Request.Target = Target;

			const FWBCardActivationExpansionResult Result = BuildActivationCommand(Request);
			if (Result.bOk)
			{
				OutCommands.Add(Result.Command);
			}
		}
	}
}
