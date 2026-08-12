#include "WBProductionPendingEffectSmoke.h"

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
const FWBMatchLegalAction* FindPendingActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId)
{
	return Actions.FindByPredicate([&EffectId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == EffectId;
	});
}

bool SubmitPendingCapture(
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

bool PendingHasTrace(
	const TArray<FWBTraceEvent>& Events,
	const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

bool PendingOpponentHandHidden(
	const FWBMatchObservation& Observation,
	const int32 OpponentPlayerId)
{
	const FWBObservedZoneSummary* Hand =
		Observation.CardZones.PublicSummary.PlayerHands.FindByPredicate(
			[OpponentPlayerId](const FWBObservedZoneSummary& Zone)
			{
				return Zone.OwnerPlayerId == OpponentPlayerId;
			});
	return Hand != nullptr && Hand->Cards.IsEmpty();
}
}

bool WBProductionPendingEffectSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionPendingEffectSmoke"));
}

FString WBProductionPendingEffectSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionPendingEffectReceipt.json"));
}

FWBProductionPendingEffectSmokeResult WBProductionPendingEffectSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionPendingEffectSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap),
		Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	FWBMatchOperationResult Operation;
	const FWBMatchLegalAction* A = FindPendingActivation(
		Started.NextLegalActions, TEXT("pending_effect_a"));
	if (A == nullptr
		|| !SubmitPendingCapture(
			Coordinator, Recorder, *A, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("pending_effect_a_missing");
		return Result;
	}
	if (Coordinator.GetPendingEffectActivationStack().Num() != 1
		|| Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Response
		|| Coordinator.GetState().PriorityPlayer != 1)
	{
		Result.Reason = TEXT("pending_effect_a_state_mismatch");
		return Result;
	}

	const FWBMatchObservation BObservation = Coordinator.BuildObservation(1);
	if (!PendingOpponentHandHidden(BObservation, 0))
	{
		Result.Reason = TEXT("pending_effect_hidden_hand_leak");
		return Result;
	}
	const FWBMatchLegalAction* B = FindPendingActivation(
		BObservation.LegalActions, TEXT("pending_effect_b"));
	if (B == nullptr
		|| !SubmitPendingCapture(
			Coordinator, Recorder, *B, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("pending_effect_b_missing");
		return Result;
	}
	if (Coordinator.GetPendingEffectActivationStack().Num() != 2)
	{
		Result.Reason = TEXT("pending_effect_b_state_mismatch");
		return Result;
	}
	const FString BFrameId =
		Coordinator.GetPendingEffectActivationStack().Last().FrameId;

	const FWBMatchObservation CObservation = Coordinator.BuildObservation(0);
	const FWBMatchLegalAction* C = FindPendingActivation(
		CObservation.LegalActions, TEXT("pending_effect_c"));
	if (C == nullptr
		|| C->ActivationCommand.EffectRequest.Payloads.IsEmpty()
		|| C->ActivationCommand.EffectRequest.Payloads[0].PendingEffectFrameId
			!= BFrameId
		|| !SubmitPendingCapture(
			Coordinator, Recorder, *C, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("pending_effect_c_missing_or_mistargeted");
		return Result;
	}
	if (!Coordinator.GetPendingEffectActivationStack().IsEmpty()
		|| Coordinator.GetState().HasOpenReactionWindow()
		|| Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| !PendingHasTrace(Operation.TraceEvents, TEXT("pending_effect_activation_negated"))
		|| !PendingHasTrace(Operation.TraceEvents, TEXT("pending_effect_activation_skipped"))
		|| !PendingHasTrace(Operation.TraceEvents, TEXT("pending_effect_parent_context_restored")))
	{
		Result.Reason = TEXT("pending_effect_nested_resolution_mismatch");
		return Result;
	}

	const FString ArchiveBytes =
		WBProductionMatchReplay::Serialize(Recorder.GetArchive());
	FString PersistedBytes;
	const FWBProductionMatchReplayPersistenceResult Loaded =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(), PersistedBytes);
	if (!Loaded.bOk || PersistedBytes != ArchiveBytes)
	{
		Result.Reason = Loaded.bOk
			? FString(TEXT("pending_effect_archive_mismatch"))
			: Loaded.FailureCode;
		return Result;
	}

	FWBProductionMatchReplayRunRequest ReplayRequest;
	ReplayRequest.SerializedArchive = PersistedBytes;
	ReplayRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(ReplayRequest);
	if (!Replay.bValid
		|| Replay.RecordsVerified != Recorder.GetArchive().Records.Num()
		|| Replay.FinalStateDigest != Coordinator.GetCurrentStateDigest()
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest())
	{
		Result.Reason = Replay.FailureCode.IsEmpty()
			? FString(TEXT("pending_effect_fresh_replay_mismatch"))
			: Replay.FailureCode;
		return Result;
	}

	const FString ReceiptJson =
		WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt());
	TSharedPtr<FJsonObject> ReceiptObject;
	if (!FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(ReceiptJson), ReceiptObject)
		|| !ReceiptObject.IsValid()
		|| ReceiptObject->Values.Num() != 8
		|| ReceiptJson.Contains(TEXT("state_digest"))
		|| ReceiptJson.Contains(TEXT("trace_digest"))
		|| ReceiptJson.Contains(TEXT("pending_effect_frame_id")))
	{
		Result.Reason = TEXT("pending_effect_receipt_privacy_mismatch");
		return Result;
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
	Result.Reason = TEXT("production_pending_effect_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
