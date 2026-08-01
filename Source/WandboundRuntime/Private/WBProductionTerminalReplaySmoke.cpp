#include "WBProductionTerminalReplaySmoke.h"

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "WBProductionMatchReplayRuntime.h"

namespace
{
const FWBMatchLegalAction* FindDiscard(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Discard;
	});
}

const FWBMatchLegalAction* FindEndTurn(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::EndTurn;
	});
}

const FWBMatchLegalAction* FindAttack(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 AttackerUnitId,
	const int32 DefenderUnitId)
{
	return Actions.FindByPredicate(
		[AttackerUnitId, DefenderUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == EWBActionType::Attack
				&& Action.CoreAction.SourceUnitId == AttackerUnitId
				&& Action.CoreAction.TargetUnitId == DefenderUnitId;
		});
}

bool SubmitAndCapture(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FWBMatchOperationResult& OutOperation,
	FString& OutReason)
{
	OutOperation = Coordinator.SubmitActionId(
		Action.PlayerId,
		Action.ActionId);
	if (!OutOperation.bOk)
	{
		OutReason = OutOperation.Reason.IsEmpty()
			? FString(TEXT("terminal_replay_submission_failed"))
			: OutOperation.Reason;
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

bool ResolvePendingTurnStart(
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
				? FString(TEXT("terminal_replay_turn_start_choice_missing"))
				: Legal.Reason;
			return false;
		}
		FWBMatchOperationResult Operation;
		if (!SubmitAndCapture(
			Coordinator,
			Recorder,
			Legal.Actions[0],
			Operation,
			OutReason))
		{
			return false;
		}
	}
	return true;
}

bool EndCurrentTurn(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn =
		Legal.bOk ? FindEndTurn(Legal.Actions) : nullptr;
	if (EndTurn == nullptr)
	{
		OutReason = Legal.Reason.IsEmpty()
			? FString(TEXT("terminal_replay_end_turn_missing"))
			: Legal.Reason;
		return false;
	}
	FWBMatchOperationResult Operation;
	return SubmitAndCapture(
		Coordinator,
		Recorder,
		*EndTurn,
		Operation,
		OutReason)
		&& ResolvePendingTurnStart(Coordinator, Recorder, OutReason);
}

bool AttackHero(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const int32 AttackerUnitId,
	const int32 DefenderUnitId,
	FWBMatchOperationResult& OutOperation,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = Legal.bOk
		? FindAttack(Legal.Actions, AttackerUnitId, DefenderUnitId)
		: nullptr;
	if (Attack == nullptr)
	{
		OutReason = Legal.Reason.IsEmpty()
			? FString(TEXT("terminal_replay_attack_missing"))
			: Legal.Reason;
		return false;
	}
	return SubmitAndCapture(
		Coordinator,
		Recorder,
		*Attack,
		OutOperation,
		OutReason);
}
}

bool WBProductionTerminalReplaySmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionTerminalReplaySmoke"));
}

FString WBProductionTerminalReplaySmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionTerminalReplayReceipt.json"));
}

FWBProductionTerminalReplaySmokeResult WBProductionTerminalReplaySmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionTerminalReplaySmokeResult Result;
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
		Result.Reason = TEXT("terminal_replay_hero_missing");
		return Result;
	}
	const int32 AttackerUnitId = PlayerZero->HeroUnitId;
	const int32 DefenderUnitId = PlayerOne->HeroUnitId;

	FWBProductionMatchReplayRecorder Recorder;
	const FWBProductionMatchReplayMetadata Metadata =
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap);
	if (!Recorder.Begin(Metadata, Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	const FWBMatchLegalAction* Discard = FindDiscard(Started.NextLegalActions);
	FWBMatchOperationResult Operation;
	if (Discard == nullptr
		|| !SubmitAndCapture(
			Coordinator,
			Recorder,
			*Discard,
			Operation,
			Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("terminal_replay_ordinary_action_missing");
		return Result;
	}
	if (!EndCurrentTurn(Coordinator, Recorder, Result.Reason)
		|| !EndCurrentTurn(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}
	if (!AttackHero(
		Coordinator,
		Recorder,
		AttackerUnitId,
		DefenderUnitId,
		Operation,
		Result.Reason)
		|| Operation.bTerminal)
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("terminal_replay_first_attack_invalid");
		return Result;
	}
	if (!EndCurrentTurn(Coordinator, Recorder, Result.Reason)
		|| !EndCurrentTurn(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}
	if (!AttackHero(
		Coordinator,
		Recorder,
		AttackerUnitId,
		DefenderUnitId,
		Operation,
		Result.Reason))
	{
		return Result;
	}
	if (!Operation.bTerminal
		|| Operation.bPendingDecision
		|| Operation.WinnerPlayerId != 0
		|| Operation.LoserPlayerId != 1
		|| Operation.TerminalReason
			!= FName(TEXT("hero_defeated_without_replacement"))
		|| Operation.TerminalSource != FName(TEXT("attack")))
	{
		Result.Reason = TEXT("terminal_replay_outcome_mismatch");
		return Result;
	}
	const FWBMatchLegalActionGenerationResult TerminalLegal =
		Coordinator.EnumerateLegalActions();
	if (!TerminalLegal.bOk || !TerminalLegal.Actions.IsEmpty())
	{
		Result.Reason = TEXT("terminal_replay_legal_actions_remain");
		return Result;
	}

	const FString FinalizedBytes =
		WBProductionMatchReplay::Serialize(Recorder.GetArchive());
	if (!Recorder.MarkComplete(Coordinator)
		|| !Recorder.MarkComplete(Coordinator)
		|| FinalizedBytes
			!= WBProductionMatchReplay::Serialize(Recorder.GetArchive()))
	{
		Result.Reason = TEXT("terminal_replay_finalization_not_idempotent");
		return Result;
	}
	const int32 GenerationBefore = Coordinator.GetCoordinatorGeneration();
	const int32 RevisionBefore = Coordinator.GetCoordinatorRevision();
	const FString StateBefore = Coordinator.GetCurrentStateDigest();
	const FString TraceBefore = Coordinator.GetCurrentTraceDigest();
	const int32 RecordCountBefore = Coordinator.GetCommittedActionRecords().Num();
	const FWBMatchOperationResult Rejected =
		Coordinator.SubmitActionId(0, TEXT("end_turn:p0"));
	Recorder.CaptureCommittedActions(Coordinator);
	if (Rejected.bOk
		|| Rejected.Reason != TEXT("game_over")
		|| Coordinator.GetCoordinatorGeneration() != GenerationBefore
		|| Coordinator.GetCoordinatorRevision() != RevisionBefore
		|| Coordinator.GetCurrentStateDigest() != StateBefore
		|| Coordinator.GetCurrentTraceDigest() != TraceBefore
		|| Coordinator.GetCommittedActionRecords().Num() != RecordCountBefore
		|| FinalizedBytes
			!= WBProductionMatchReplay::Serialize(Recorder.GetArchive()))
	{
		Result.Reason = TEXT("terminal_replay_post_terminal_mutation");
		return Result;
	}

	FString ArchiveJson;
	const FWBProductionMatchReplayPersistenceResult LoadResult =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(),
			ArchiveJson);
	if (!LoadResult.bOk || ArchiveJson != FinalizedBytes)
	{
		Result.Reason = LoadResult.bOk
			? FString(TEXT("terminal_replay_archive_bytes_mismatch"))
			: LoadResult.FailureCode;
		return Result;
	}
	FWBProductionMatchReplayRunRequest RunRequest;
	RunRequest.SerializedArchive = ArchiveJson;
	RunRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(RunRequest);
	if (!Replay.bValid || !Replay.bComplete || !Replay.bTerminal
		|| Replay.WinnerPlayerId != 0 || Replay.LoserPlayerId != 1
		|| Replay.TerminalReason
			!= FName(TEXT("hero_defeated_without_replacement")))
	{
		Result.Reason = Replay.FailureCode.IsEmpty()
			? FString(TEXT("terminal_replay_fresh_verification_failed"))
			: Replay.FailureCode;
		return Result;
	}

	const FString ReceiptPath = GetReceiptPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	if (!FFileHelper::SaveStringToFile(
		WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt()),
		*ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("replay_write_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("production_terminal_replay_verified");
	return Result;
}
