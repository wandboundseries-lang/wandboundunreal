#include "WBProductionActivationTargetSelectionBridge.h"

namespace
{
bool IsValidTile(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.Y >= 0;
}

bool ContainsHiddenSentinel(const FString& Text)
{
	return Text.Contains(TEXT("SECRET_"), ESearchCase::IgnoreCase)
		|| Text.Contains(TEXT("DO_NOT_LEAK"), ESearchCase::IgnoreCase);
}

bool ContainsHiddenTargetData(const FWBCardActivationTargetOption& Option)
{
	return ContainsHiddenSentinel(Option.TargetCardId)
		|| ContainsHiddenSentinel(Option.PublicLabel);
}

FString SafeSourceCardIdFromAction(const FWBCardActivationLegalAction& Action)
{
	if (!Action.Candidate.SourceCardId.IsEmpty())
	{
		return Action.Candidate.SourceCardId;
	}

	return Action.Command.Source.SourceCardId;
}

FString SafeEffectIdFromAction(const FWBCardActivationLegalAction& Action)
{
	if (!Action.Candidate.SourceEffectId.IsEmpty())
	{
		return Action.Candidate.SourceEffectId;
	}

	return Action.Command.Source.SourceEffectId;
}

bool MatchesRequestedSourceInstance(
	const FWBCardActivationLegalAction& Action,
	const FString& SourceInstanceId)
{
	if (SourceInstanceId.IsEmpty())
	{
		return true;
	}

	const FString HandInstancePart = FString::Printf(TEXT(":i%s:"), *SourceInstanceId);
	return Action.ActivationActionId.Contains(HandInstancePart, ESearchCase::CaseSensitive);
}

bool MatchesRequest(
	const FWBCardActivationLegalAction& Action,
	const FWBProductionActivationTargetSelectionRequest& Request)
{
	if (!Request.ActivationEntryId.IsEmpty()
		&& Action.ActivationActionId != Request.ActivationEntryId)
	{
		return false;
	}

	if (!Request.SourceCardId.IsEmpty()
		&& SafeSourceCardIdFromAction(Action) != Request.SourceCardId)
	{
		return false;
	}

	if (!Request.EffectId.IsEmpty()
		&& SafeEffectIdFromAction(Action) != Request.EffectId)
	{
		return false;
	}

	return MatchesRequestedSourceInstance(Action, Request.SourceInstanceId);
}

bool TargetOptionMatchesProviderOption(
	const FWBCardActivationTargetOption& Selected,
	const FWBCardActivationTargetOption& ProviderOption)
{
	if (ProviderOption.Type != EWBCardActivationTargetOptionType::Unit
		|| Selected.Type != EWBCardActivationTargetOptionType::Unit)
	{
		return false;
	}

	if (Selected.TargetUnitId != ProviderOption.TargetUnitId)
	{
		return false;
	}

	if (ProviderOption.TargetOwnerPlayerId != -1
		&& Selected.TargetOwnerPlayerId != ProviderOption.TargetOwnerPlayerId)
	{
		return false;
	}

	if (IsValidTile(ProviderOption.TargetTile)
		&& Selected.TargetTile != ProviderOption.TargetTile)
	{
		return false;
	}

	if (!ProviderOption.TargetCardId.IsEmpty()
		&& Selected.TargetCardId != ProviderOption.TargetCardId)
	{
		return false;
	}

	return true;
}

bool HasMatchingProviderTargetOption(
	const FWBCardActivationLegalAction& Action,
	const FWBCardActivationTargetOption& Selected)
{
	for (const FWBCardActivationTargetOption& ProviderOption : Action.TargetOptions)
	{
		if (TargetOptionMatchesProviderOption(Selected, ProviderOption))
		{
			return true;
		}
	}

	return false;
}

FWBCardActivationCommand MakeSafeCommandCopy(const FWBCardActivationLegalAction& Action)
{
	FWBCardActivationCommand Command = Action.Command;
	Command.DebugActivationId.Reset();
	Command.UsageCommit = FWBCardActivationUsageCommit();
	return Command;
}

FWBProductionActivationTargetSelectionResult MakeFailure(
	const EWBProductionActivationTargetSelectionResultCode Code)
{
	FWBProductionActivationTargetSelectionResult Result;
	Result.bOk = false;
	Result.Code = Code;
	Result.Reason = FWBProductionActivationTargetSelectionBridge::ResultCodeToString(Code);
	return Result;
}

FWBProductionActivationTargetSelectionResult MakeSuccess(
	const FWBCardActivationLegalAction& Action,
	FWBCardActivationCommand&& Command,
	const int32 TargetUnitId)
{
	FWBProductionActivationTargetSelectionResult Result;
	Result.bOk = true;
	Result.Code = EWBProductionActivationTargetSelectionResultCode::Success;
	Result.Reason = FWBProductionActivationTargetSelectionBridge::ResultCodeToString(Result.Code);
	Result.Command = MoveTemp(Command);
	Result.SourceCardId = SafeSourceCardIdFromAction(Action);
	Result.EffectId = SafeEffectIdFromAction(Action);
	Result.TargetUnitId = TargetUnitId;
	return Result;
}
}

void FWBProductionActivationTargetSelectionBridge::ConfigureFromProviderData(
	const TArray<FWBCardActivationLegalAction>& ProviderActivationEntries)
{
	ActivationEntries = ProviderActivationEntries;
	bConfigured = true;
}

bool FWBProductionActivationTargetSelectionBridge::IsConfigured() const
{
	return bConfigured;
}

FWBProductionActivationTargetSelectionResult
FWBProductionActivationTargetSelectionBridge::BuildCommandForSelection(
	const FWBProductionActivationTargetSelectionRequest& Request) const
{
	if (!bConfigured)
	{
		return MakeFailure(EWBProductionActivationTargetSelectionResultCode::BridgeNotConfigured);
	}

	const FWBCardActivationLegalAction* MatchedAction = nullptr;
	for (const FWBCardActivationLegalAction& Action : ActivationEntries)
	{
		if (!MatchesRequest(Action, Request))
		{
			continue;
		}

		if (MatchedAction != nullptr)
		{
			return MakeFailure(EWBProductionActivationTargetSelectionResultCode::CommandBuildFailed);
		}

		MatchedAction = &Action;
	}

	if (MatchedAction == nullptr)
	{
		return MakeFailure(EWBProductionActivationTargetSelectionResultCode::ActivationEntryMissing);
	}

	if (ContainsHiddenTargetData(Request.SelectedTargetOption))
	{
		return MakeFailure(EWBProductionActivationTargetSelectionResultCode::HiddenTargetRejected);
	}

	switch (MatchedAction->TargetRequirement)
	{
	case EWBCardEffectTargetRequirement::None:
	{
		if (Request.bHasSelectedTarget)
		{
			return MakeFailure(EWBProductionActivationTargetSelectionResultCode::TargetProvidedForNoTargetEffect);
		}

		FWBCardActivationCommand Command = MakeSafeCommandCopy(*MatchedAction);
		Command.EffectRequest.Target = FWBEffectTargetRef();
		return MakeSuccess(*MatchedAction, MoveTemp(Command), -1);
	}
	case EWBCardEffectTargetRequirement::Unit:
	{
		if (!Request.bHasSelectedTarget)
		{
			return MakeFailure(EWBProductionActivationTargetSelectionResultCode::TargetRequiredButMissing);
		}

		if (Request.SelectedTargetOption.Type != EWBCardActivationTargetOptionType::Unit)
		{
			return MakeFailure(EWBProductionActivationTargetSelectionResultCode::TargetTypeMismatch);
		}

		if (Request.SelectedTargetOption.TargetUnitId < 0)
		{
			return MakeFailure(EWBProductionActivationTargetSelectionResultCode::TargetStaleOrMissing);
		}

		if (!HasMatchingProviderTargetOption(*MatchedAction, Request.SelectedTargetOption))
		{
			return MakeFailure(EWBProductionActivationTargetSelectionResultCode::TargetNotAllowed);
		}

		FWBCardActivationCommand Command = MakeSafeCommandCopy(*MatchedAction);
		Command.EffectRequest.Target.TargetUnitId = Request.SelectedTargetOption.TargetUnitId;
		Command.EffectRequest.Target.TargetTile = Request.SelectedTargetOption.TargetTile;
		Command.EffectRequest.Target.TargetWallEdge = FWBWallEdge();
		return MakeSuccess(
			*MatchedAction,
			MoveTemp(Command),
			Request.SelectedTargetOption.TargetUnitId);
	}
	case EWBCardEffectTargetRequirement::Tile:
	case EWBCardEffectTargetRequirement::WallEdge:
	default:
		return MakeFailure(EWBProductionActivationTargetSelectionResultCode::UnsupportedTargetRequirement);
	}
}

FString FWBProductionActivationTargetSelectionBridge::ResultCodeToString(
	const EWBProductionActivationTargetSelectionResultCode Code)
{
	switch (Code)
	{
	case EWBProductionActivationTargetSelectionResultCode::Success:
		return TEXT("success");
	case EWBProductionActivationTargetSelectionResultCode::BridgeNotConfigured:
		return TEXT("bridge_not_configured");
	case EWBProductionActivationTargetSelectionResultCode::ActivationEntryMissing:
		return TEXT("activation_entry_missing");
	case EWBProductionActivationTargetSelectionResultCode::TargetRequiredButMissing:
		return TEXT("target_required_but_missing");
	case EWBProductionActivationTargetSelectionResultCode::TargetNotAllowed:
		return TEXT("target_not_allowed");
	case EWBProductionActivationTargetSelectionResultCode::TargetTypeMismatch:
		return TEXT("target_type_mismatch");
	case EWBProductionActivationTargetSelectionResultCode::TargetStaleOrMissing:
		return TEXT("target_stale_or_missing");
	case EWBProductionActivationTargetSelectionResultCode::TargetProvidedForNoTargetEffect:
		return TEXT("target_provided_for_no_target_effect");
	case EWBProductionActivationTargetSelectionResultCode::UnsupportedTargetRequirement:
		return TEXT("unsupported_target_requirement");
	case EWBProductionActivationTargetSelectionResultCode::HiddenTargetRejected:
		return TEXT("hidden_target_rejected");
	case EWBProductionActivationTargetSelectionResultCode::CommandBuildFailed:
	default:
		return TEXT("command_build_failed");
	}
}
