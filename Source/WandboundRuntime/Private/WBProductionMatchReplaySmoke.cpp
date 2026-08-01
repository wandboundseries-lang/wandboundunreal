#include "WBProductionMatchReplaySmoke.h"

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "WBProductionMatchReplayRuntime.h"

namespace
{
const FWBMatchLegalAction* FindOrdinaryAction(
	const TArray<FWBMatchLegalAction>& Actions)
{
	const FWBMatchLegalAction* Fallback = nullptr;
	for (const FWBMatchLegalAction& Action : Actions)
	{
		if (Action.Family == EWBMatchActionFamily::Discard)
		{
			return &Action;
		}
		if (!(Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::EndTurn)
			&& Fallback == nullptr)
		{
			Fallback = &Action;
		}
	}
	return Fallback;
}

const FWBMatchLegalAction* FindEndTurn(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family
					== EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type
					== EWBActionType::EndTurn;
		});
}

bool SubmitAndCapture(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FString& OutReason)
{
	const FWBMatchOperationResult Result =
		Coordinator.SubmitActionId(
			Action.PlayerId,
			Action.ActionId);
	if (!Result.bOk)
	{
		OutReason = Result.Reason.IsEmpty()
			? FString(TEXT("replay_smoke_submission_failed"))
			: Result.Reason;
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
}

bool WBProductionMatchReplaySmoke::IsRequested(
	const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionReplaySmoke"));
}

FString WBProductionMatchReplaySmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionReplayReceipt.json"));
}

FWBProductionMatchReplaySmokeResult
WBProductionMatchReplaySmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionMatchReplaySmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(
			Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	FWBProductionMatchReplayRecorder Recorder;
	const FWBProductionMatchReplayMetadata Metadata =
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap);
	if (!Recorder.Begin(Metadata, Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	const FWBMatchLegalAction* Ordinary =
		FindOrdinaryAction(Started.NextLegalActions);
	if (Ordinary == nullptr
		|| !SubmitAndCapture(
			Coordinator,
			Recorder,
			*Ordinary,
			Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("replay_smoke_ordinary_action_missing");
		return Result;
	}

	FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn =
		Legal.bOk ? FindEndTurn(Legal.Actions) : nullptr;
	if (EndTurn == nullptr
		|| !SubmitAndCapture(
			Coordinator,
			Recorder,
			*EndTurn,
			Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("replay_smoke_end_turn_missing");
		return Result;
	}

	while (Coordinator.HasPendingTurnStartDecision())
	{
		Legal = Coordinator.EnumerateLegalActions();
		if (!Legal.bOk || Legal.Actions.IsEmpty()
			|| !SubmitAndCapture(
				Coordinator,
				Recorder,
				Legal.Actions[0],
				Result.Reason))
		{
			if (Result.Reason.IsEmpty())
				Result.Reason = TEXT("replay_smoke_turn_start_choice_missing");
			return Result;
		}
	}

	Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Later =
		Legal.bOk ? FindOrdinaryAction(Legal.Actions) : nullptr;
	if (Later == nullptr
		|| !SubmitAndCapture(
			Coordinator,
			Recorder,
			*Later,
			Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("replay_smoke_later_action_missing");
		return Result;
	}

	FString ArchiveJson;
	const FWBProductionMatchReplayPersistenceResult LoadResult =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(),
			ArchiveJson);
	if (!LoadResult.bOk)
	{
		Result.Reason = LoadResult.FailureCode;
		return Result;
	}
	FWBProductionMatchReplayRunRequest RunRequest;
	RunRequest.SerializedArchive = ArchiveJson;
	RunRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult RunResult =
		FWBProductionMatchReplayRunner::Run(RunRequest);
	if (!RunResult.bValid)
	{
		Result.Reason = RunResult.FailureCode;
		return Result;
	}

	const FString ReceiptPath = GetReceiptPath();
	IFileManager::Get().MakeDirectory(
		*FPaths::GetPath(ReceiptPath),
		true);
	if (!FFileHelper::SaveStringToFile(
		WBProductionMatchReplay::SerializeReceipt(
			Recorder.GetReceipt()),
		*ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("replay_write_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("production_replay_verified");
	return Result;
}
