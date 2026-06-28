#include "WBCardActivationLegalActionGenerator.h"

namespace
{
FWBCardActivationLegalActionGenerationResult MakeGenerationFailure(const FString& Reason)
{
	FWBCardActivationLegalActionGenerationResult Result;
	Result.Reason = Reason;
	return Result;
}

bool HasMissingPayloads(const FWBCardActivationCandidate& Candidate)
{
	return Candidate.Command.EffectRequest.Payloads.IsEmpty();
}

bool HasUnknownPayloadOperation(const FWBCardActivationCandidate& Candidate)
{
	for (const FWBGenericEffectPayload& Payload : Candidate.Command.EffectRequest.Payloads)
	{
		if (Payload.Operation == EWBGenericEffectOp::Unknown)
		{
			return true;
		}
	}

	return false;
}

bool IsPublicLabelAllowed(const FString& PublicLabel)
{
	const TArray<FString> ForbiddenSubstrings = {
		TEXT("damage_effect"),
		TEXT("heal_effect"),
		TEXT("armor_effect"),
		TEXT("status_effect"),
		TEXT("EffectRunner"),
		TEXT("Rules"),
		TEXT("payload"),
		TEXT("schema"),
		TEXT("hook")
	};

	for (const FString& ForbiddenSubstring : ForbiddenSubstrings)
	{
		if (PublicLabel.Contains(ForbiddenSubstring, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	return true;
}
}

FWBCardActivationLegalActionGenerationResult WBCardActivationLegalActionGenerator::GenerateFromCandidates(
	const TArray<FWBCardActivationCandidate>& Candidates)
{
	FWBCardActivationLegalActionGenerationResult Result;
	Result.ActionSet.Actions.Reserve(Candidates.Num());

	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		const FWBCardActivationCandidate& Candidate = Candidates[CandidateIndex];
		if (Candidate.ActivationCandidateId.IsEmpty())
		{
			return MakeGenerationFailure(FString::Printf(TEXT("missing_activation_candidate_id:%d"), CandidateIndex));
		}

		if (HasMissingPayloads(Candidate))
		{
			return MakeGenerationFailure(FString::Printf(TEXT("missing_command_payloads:%d"), CandidateIndex));
		}

		if (HasUnknownPayloadOperation(Candidate))
		{
			return MakeGenerationFailure(FString::Printf(TEXT("unknown_command_payload_operation:%d"), CandidateIndex));
		}

		const FString PublicLabel = Candidate.PublicLabel.IsEmpty()
			? FString(TEXT("Activate"))
			: Candidate.PublicLabel;
		if (!IsPublicLabelAllowed(PublicLabel))
		{
			return MakeGenerationFailure(FString::Printf(TEXT("unsafe_public_label:%d"), CandidateIndex));
		}

		FWBCardActivationLegalAction Action;
		Action.ActivationActionId = MakeActivationActionId(Candidate);
		Action.PlayerId = Candidate.PlayerId;
		Action.SourceUnitId = Candidate.SourceUnitId;
		Action.PublicLabel = PublicLabel;
		if (Candidate.Target.TargetUnitId >= 0)
		{
			Action.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
		}
		else if (Candidate.Target.TargetTile.X >= 0 && Candidate.Target.TargetTile.Y >= 0)
		{
			Action.TargetRequirement = EWBCardEffectTargetRequirement::Tile;
		}
		else if (Candidate.Target.TargetWallEdge.A.X >= 0
			&& Candidate.Target.TargetWallEdge.A.Y >= 0
			&& Candidate.Target.TargetWallEdge.B.X >= 0
			&& Candidate.Target.TargetWallEdge.B.Y >= 0)
		{
			Action.TargetRequirement = EWBCardEffectTargetRequirement::WallEdge;
		}
		Action.Target = Candidate.Target;
		Action.Candidate = Candidate;
		Action.Command = Candidate.Command;
		Result.ActionSet.Actions.Add(Action);
	}

	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}

FString WBCardActivationLegalActionGenerator::MakeActivationActionId(
	const FWBCardActivationCandidate& Candidate)
{
	return Candidate.ActivationCandidateId;
}
