#include "WBProductionSuspendedAttackSmoke.h"

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
const FWBMatchLegalAction* FindSuspendedCoreAction(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindSuspendedActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId)
{
	return Actions.FindByPredicate([&EffectId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == EffectId;
	});
}

bool SubmitSuspendedCapture(
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

bool ResolveSuspendedTurnStart(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	while (Coordinator.HasPendingTurnStartDecision())
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		if (!Legal.bOk || Legal.Actions.IsEmpty())
		{
			OutReason = Legal.Reason.IsEmpty()
				? FString(TEXT("suspended_attack_turn_start_choice_missing"))
				: Legal.Reason;
			return false;
		}
		FWBMatchOperationResult Operation;
		if (!SubmitSuspendedCapture(
			Coordinator, Recorder, Legal.Actions[0], Operation, OutReason))
		{
			return false;
		}
	}
	return true;
}

bool EndSuspendedTurn(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = Legal.bOk
		? FindSuspendedCoreAction(Legal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr)
	{
		OutReason = Legal.bOk
			? FString(TEXT("suspended_attack_end_turn_missing"))
			: Legal.Reason;
		return false;
	}
	FWBMatchOperationResult Operation;
	return SubmitSuspendedCapture(
		Coordinator, Recorder, *EndTurn, Operation, OutReason)
		&& ResolveSuspendedTurnStart(Coordinator, Recorder, OutReason);
}

bool SuspendedHasTrace(
	const TArray<FWBTraceEvent>& Events,
	const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

bool SuspendedOpponentHandHidden(
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

bool WBProductionSuspendedAttackSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionSuspendedAttackSmoke"));
}

FString WBProductionSuspendedAttackSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionSuspendedAttackReceipt.json"));
}

FWBProductionSuspendedAttackSmokeResult WBProductionSuspendedAttackSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionSuspendedAttackSmokeResult Result;
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
	const FWBPlayerStateData* PlayerZero = Coordinator.GetState().GetPlayerById(0);
	const FWBPlayerStateData* PlayerOne = Coordinator.GetState().GetPlayerById(1);
	if (PlayerZero == nullptr || PlayerOne == nullptr)
	{
		Result.Reason = TEXT("suspended_attack_hero_missing");
		return Result;
	}
	const int32 AttackerUnitId = PlayerZero->HeroUnitId;
	const int32 DefenderUnitId = PlayerOne->HeroUnitId;
	const FWBUnitState* Defender = Coordinator.GetState().GetUnitById(DefenderUnitId);
	const int32 DefenderHPBefore = Defender != nullptr ? Defender->HP : -1;

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap),
		Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}
	if (!EndSuspendedTurn(Coordinator, Recorder, Result.Reason)
		|| !EndSuspendedTurn(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}

	const FWBMatchLegalActionGenerationResult AttackLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = AttackLegal.bOk
		? AttackLegal.Actions.FindByPredicate(
			[AttackerUnitId, DefenderUnitId](const FWBMatchLegalAction& Action)
			{
				return Action.Family == EWBMatchActionFamily::CoreAction
					&& Action.CoreAction.Type == EWBActionType::Attack
					&& Action.CoreAction.SourceUnitId == AttackerUnitId
					&& Action.CoreAction.TargetUnitId == DefenderUnitId;
			})
		: nullptr;
	FWBMatchOperationResult Operation;
	if (Attack == nullptr
		|| !SubmitSuspendedCapture(
			Coordinator, Recorder, *Attack, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("suspended_attack_action_missing");
		return Result;
	}
	if (!Coordinator.GetState().HasPendingAttack()
		|| Coordinator.GetState().PendingAttack.Stage
			!= EWBAttackContinuationStage::PreHit
		|| Coordinator.GetState().ReactionWindow.Kind
			!= EWBReactionWindowKind::PreHit
		|| Coordinator.GetState().PriorityPlayer != 1)
	{
		Result.Reason = TEXT("suspended_attack_pre_hit_state_mismatch");
		return Result;
	}
	const FWBMatchObservation AObservation = Coordinator.BuildObservation(1);
	if (!SuspendedOpponentHandHidden(AObservation, 0))
	{
		Result.Reason = TEXT("suspended_attack_hidden_hand_leak");
		return Result;
	}
	const FWBMatchLegalAction* A = FindSuspendedActivation(
		AObservation.LegalActions, TEXT("suspended_attack_effect_a"));
	if (A == nullptr
		|| !SubmitSuspendedCapture(
			Coordinator, Recorder, *A, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("suspended_attack_effect_a_missing");
		return Result;
	}

	const FWBMatchObservation BObservation = Coordinator.BuildObservation(0);
	const FWBMatchLegalAction* B = FindSuspendedActivation(
		BObservation.LegalActions, TEXT("suspended_attack_effect_b"));
	if (B == nullptr
		|| !SubmitSuspendedCapture(
			Coordinator, Recorder, *B, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("suspended_attack_effect_b_missing");
		return Result;
	}

	const FWBMatchObservation CObservation = Coordinator.BuildObservation(1);
	if (!SuspendedOpponentHandHidden(CObservation, 0))
	{
		Result.Reason = TEXT("suspended_attack_nested_hidden_hand_leak");
		return Result;
	}
	const FWBMatchLegalAction* C = FindSuspendedActivation(
		CObservation.LegalActions, TEXT("suspended_attack_effect_c"));
	if (C == nullptr
		|| !SubmitSuspendedCapture(
			Coordinator, Recorder, *C, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("suspended_attack_effect_c_missing");
		return Result;
	}
	Defender = Coordinator.GetState().GetUnitById(DefenderUnitId);
	if (Coordinator.GetState().HasPendingAttack()
		|| Coordinator.GetState().HasOpenReactionWindow()
		|| !Coordinator.GetPendingEffectActivationStack().IsEmpty()
		|| Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| Defender == nullptr
		|| Defender->HP != DefenderHPBefore
		|| !SuspendedHasTrace(Operation.TraceEvents, TEXT("attack_prevented"))
		|| SuspendedHasTrace(Operation.TraceEvents, TEXT("attack_damage_started")))
	{
		Result.Reason = TEXT("suspended_attack_nested_resolution_mismatch");
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
			? FString(TEXT("suspended_attack_archive_mismatch"))
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
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest()
		|| Replay.FinalGeneration != Coordinator.GetCoordinatorGeneration()
		|| Replay.FinalRevision != Coordinator.GetCoordinatorRevision())
	{
		Result.Reason = Replay.FailureCode.IsEmpty()
			? FString(TEXT("suspended_attack_fresh_replay_mismatch"))
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
		|| ReceiptJson.Contains(TEXT("continuation_id"))
		|| ReceiptJson.Contains(TEXT("pending_effect_frame_id")))
	{
		Result.Reason = TEXT("suspended_attack_receipt_privacy_mismatch");
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
	Result.Reason = TEXT("production_suspended_attack_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
