#include "WBProductionSummonNegationSmoke.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBProductionMatchReplayRuntime.h"

namespace
{
enum class ESummonNegationSmokePath : uint8
{
	PassThrough,
	Negated,
	NegationCountered
};

const FWBMatchLegalAction* FindSummon(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& !Action.bHybridSummon
			&& Action.SummonRequest.SourceCardId == TEXT("sn_fixture_student");
	});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId)
{
	return Actions.FindByPredicate([&EffectId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == EffectId;
	});
}

const FWBMatchLegalAction* FindPass(const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::PassResponse;
	});
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

bool HasStudent(const FWBGameStateData& State)
{
	return State.Units.ContainsByPredicate([](const FWBUnitState& Unit)
	{
		return Unit.IsUnitOnBoard()
			&& Unit.CardId == TEXT("sn_fixture_student");
	});
}

bool SubmitAndCapture(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FWBMatchOperationResult& OutOperation,
	FString& OutReason)
{
	OutOperation = Coordinator.SubmitActionId(Action.PlayerId, Action.ActionId);
	if (!OutOperation.bOk)
	{
		OutReason = OutOperation.Reason;
		return false;
	}
	Recorder.CaptureCommittedActions(Coordinator);
	if (!Recorder.IsAvailable())
	{
		OutReason = Recorder.GetReceipt().FailureCode;
		return false;
	}
	return true;
}

bool VerifyReceiptPrivacy(
	const FWBProductionMatchReplayReceipt& Receipt,
	FString& OutJson)
{
	OutJson = WBProductionMatchReplay::SerializeReceipt(Receipt);
	TSharedPtr<FJsonObject> Object;
	return FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(OutJson), Object)
		&& Object.IsValid()
		&& Object->Values.Num() == 8
		&& !OutJson.Contains(TEXT("state_digest"))
		&& !OutJson.Contains(TEXT("trace_digest"))
		&& !OutJson.Contains(TEXT("sn_fixture_student#"));
}

bool RunPath(
	const FWBProductionRuntimeBootstrapResult& Bootstrap,
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest,
	const ESummonNegationSmokePath Path,
	FWBProductionSummonNegationSmokeResult& OutResult,
	FString& OutReceiptJson)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		OutResult.Reason = Started.Reason;
		return false;
	}
	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap), Coordinator))
	{
		OutResult.Reason = Recorder.GetReceipt().FailureCode;
		return false;
	}

	const FWBMatchLegalAction* Summon = FindSummon(Started.NextLegalActions);
	FWBMatchOperationResult Operation;
	if (Summon == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *Summon, Operation, OutResult.Reason))
	{
		if (OutResult.Reason.IsEmpty())
		{
			OutResult.Reason = TEXT("summon_negation_smoke_summon_missing");
		}
		return false;
	}
	const FString PendingId = Coordinator.GetState().HasPendingSummon()
		? Coordinator.GetState().PendingSummon.PendingSummonId
		: FString();
	const FWBMatchObservation OpponentObservation = Coordinator.BuildObservation(1);
	const FWBObservedZoneSummary* SummonerHand =
		OpponentObservation.CardZones.PublicSummary.PlayerHands.FindByPredicate(
			[](const FWBObservedZoneSummary& Hand)
			{
				return Hand.OwnerPlayerId == 0;
			});
	if (PendingId.IsEmpty()
		|| Coordinator.GetState().ReactionWindow.Kind
			!= EWBReactionWindowKind::PreSummon
		|| Coordinator.GetState().PriorityPlayer != 1
		|| OpponentObservation.PublicTurn.ReactionWindowKind
			!= FName(TEXT("pre_summon"))
		|| SummonerHand == nullptr
		|| !SummonerHand->Cards.IsEmpty()
		|| HasStudent(Coordinator.GetState()))
	{
		OutResult.Reason = TEXT("summon_negation_smoke_pending_or_privacy_mismatch");
		return false;
	}

	const FWBMatchLegalActionGenerationResult ResponseLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* SummonNegate = FindActivation(
		ResponseLegal.Actions, TEXT("sn_fixture_negate_summon"));
	const FWBMatchLegalAction* Pass = FindPass(ResponseLegal.Actions);
	if (SummonNegate == nullptr || Pass == nullptr)
	{
		OutResult.Reason = TEXT("summon_negation_smoke_response_actions_missing");
		return false;
	}
	if (SummonNegate->ActivationCommand.EffectRequest.Payloads.IsEmpty()
		|| SummonNegate->ActivationCommand.EffectRequest.Payloads[0].PendingSummonId
			!= PendingId)
	{
		OutResult.Reason = TEXT("summon_negation_smoke_target_identity_mismatch");
		return false;
	}

	if (Path == ESummonNegationSmokePath::PassThrough)
	{
		if (!SubmitAndCapture(
			Coordinator, Recorder, *Pass, Operation, OutResult.Reason))
		{
			return false;
		}
	}
	else
	{
		if (!SubmitAndCapture(
			Coordinator,
			Recorder,
			*SummonNegate,
			Operation,
			OutResult.Reason))
		{
			return false;
		}
		if (!Coordinator.GetState().HasPendingSummon()
			|| Coordinator.GetState().PendingSummon.bNegated)
		{
			OutResult.Reason = TEXT("summon_negation_smoke_premature_negation");
			return false;
		}
		const FWBMatchLegalActionGenerationResult NestedLegal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Nested =
			Path == ESummonNegationSmokePath::Negated
				? FindPass(NestedLegal.Actions)
				: FindActivation(
					NestedLegal.Actions, TEXT("sn_fixture_negate_effect"));
		if (Nested == nullptr
			|| !SubmitAndCapture(
				Coordinator, Recorder, *Nested, Operation, OutResult.Reason))
		{
			if (OutResult.Reason.IsEmpty())
			{
				OutResult.Reason = TEXT("summon_negation_smoke_nested_action_missing");
			}
			return false;
		}
	}

	const bool bShouldSummon = Path != ESummonNegationSmokePath::Negated;
	if (Coordinator.GetState().HasPendingSummon()
		|| Coordinator.GetState().HasOpenReactionWindow()
		|| HasStudent(Coordinator.GetState()) != bShouldSummon
		|| (Path == ESummonNegationSmokePath::Negated
			&& (!HasTrace(Operation.TraceEvents, FName(TEXT("summon_negated")))
				|| HasTrace(Operation.TraceEvents, FName(TEXT("summon_unit")))))
		|| (Path == ESummonNegationSmokePath::NegationCountered
			&& (!HasTrace(Operation.TraceEvents, FName(TEXT("pending_effect_activation_negated")))
				|| HasTrace(Operation.TraceEvents, FName(TEXT("summon_negated")))))
		|| (bShouldSummon
			&& !HasTrace(Operation.TraceEvents, FName(TEXT("summon_unit")))) )
	{
		OutResult.Reason = TEXT("summon_negation_smoke_outcome_mismatch");
		return false;
	}

	const FString ArchiveBytes =
		WBProductionMatchReplay::Serialize(Recorder.GetArchive());
	FString PersistedBytes;
	const FWBProductionMatchReplayPersistenceResult Loaded =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(), PersistedBytes);
	if (!Loaded.bOk || PersistedBytes != ArchiveBytes)
	{
		OutResult.Reason = Loaded.bOk
			? FString(TEXT("summon_negation_smoke_archive_mismatch"))
			: Loaded.FailureCode;
		return false;
	}
	FWBProductionMatchReplayRunRequest ReplayRequest;
	ReplayRequest.SerializedArchive = PersistedBytes;
	ReplayRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(ReplayRequest);
	if (!Replay.bValid
		|| Replay.RecordsVerified != Recorder.GetArchive().Records.Num()
		|| Replay.FinalStateDigest != Coordinator.GetCurrentStateDigest()
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest()
		|| Replay.FinalGeneration != Coordinator.GetCoordinatorGeneration()
		|| Replay.FinalRevision != Coordinator.GetCoordinatorRevision())
	{
		OutResult.Reason = Replay.FailureCode.IsEmpty()
			? FString(TEXT("summon_negation_smoke_fresh_replay_mismatch"))
			: Replay.FailureCode;
		return false;
	}
	if (!VerifyReceiptPrivacy(Recorder.GetReceipt(), OutReceiptJson))
	{
		OutResult.Reason = TEXT("summon_negation_smoke_receipt_privacy_mismatch");
		return false;
	}

	++OutResult.ScenariosVerified;
	OutResult.RecordsVerified += Replay.RecordsVerified;
	OutResult.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	OutResult.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return true;
}
}

bool WBProductionSummonNegationSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionSummonNegationSmoke"));
}

FString WBProductionSummonNegationSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionSummonNegationReceipt.json"));
}

FWBProductionSummonNegationSmokeResult WBProductionSummonNegationSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionSummonNegationSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	FString ReceiptJson;
	const ESummonNegationSmokePath Paths[] = {
		ESummonNegationSmokePath::PassThrough,
		ESummonNegationSmokePath::Negated,
		ESummonNegationSmokePath::NegationCountered
	};
	for (const ESummonNegationSmokePath Path : Paths)
	{
		if (!RunPath(Bootstrap, BootstrapRequest, Path, Result, ReceiptJson))
		{
			return Result;
		}
	}

	const FString ReceiptPath = GetReceiptPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	if (!FFileHelper::SaveStringToFile(
		ReceiptJson,
		*ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("replay_write_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("production_summon_negation_verified");
	return Result;
}
