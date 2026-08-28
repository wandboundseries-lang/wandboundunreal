#include "WBProductionTerrainCartographerSmoke.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBRuntimeActivationExecutionBridge.h"

namespace
{
const FWBMatchLegalAction* FindCore(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindSummon(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& CardId,
	const FWBTile Tile)
{
	return Actions.FindByPredicate([&CardId, Tile](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& !Action.bHybridSummon
			&& Action.SummonRequest.SourceCardId == CardId
			&& Action.SummonRequest.TargetTile == Tile;
	});
}

const FWBMatchLegalAction* FindMove(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 SourceUnitId,
	const FWBTile Tile)
{
	return Actions.FindByPredicate([SourceUnitId, Tile](
		const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Move
			&& Action.CoreAction.SourceUnitId == SourceUnitId
			&& Action.CoreAction.ToTile == Tile;
	});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& CardId,
	const FWBTile Tile)
{
	return Actions.FindByPredicate([&CardId, Tile](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceCardId == CardId
			&& Action.ActivationCommand.EffectRequest.Target.TargetTile == Tile;
	});
}

const FWBMatchLegalAction* FindResponsePass(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::PassResponse;
	});
}

bool Submit(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FString& OutReason)
{
	const FWBMatchOperationResult Operation = Coordinator.SubmitActionId(
		Action.PlayerId, Action.ActionId);
	if (!Operation.bOk)
	{
		OutReason = Operation.Reason;
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

bool ResolveResponses(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 4 && Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response;
		++Guard)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindResponsePass(Legal.Actions) : nullptr;
		if (Pass == nullptr)
		{
			OutReason = Legal.bOk
				? FString(TEXT("terrain_cartographer_response_pass_missing"))
				: Legal.Reason;
			return false;
		}
		if (!Submit(Coordinator, Recorder, *Pass, OutReason))
		{
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action)
	{
		OutReason = TEXT("terrain_cartographer_response_guard_exceeded");
		return false;
	}
	return true;
}

bool SubmitAndResolve(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FString& OutReason)
{
	return Submit(Coordinator, Recorder, Action, OutReason)
		&& ResolveResponses(Coordinator, Recorder, OutReason);
}

bool ResolveTurnStart(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 16 && Coordinator.HasPendingTurnStartDecision();
		++Guard)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		if (!Legal.bOk || Legal.Actions.IsEmpty())
		{
			OutReason = Legal.Reason.IsEmpty()
				? FString(TEXT("terrain_cartographer_turn_start_decision_missing"))
				: Legal.Reason;
			return false;
		}
		if (!Submit(Coordinator, Recorder, Legal.Actions[0], OutReason))
		{
			return false;
		}
	}
	return !Coordinator.HasPendingTurnStartDecision();
}

bool EndTurn(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* End = Legal.bOk
		? FindCore(Legal.Actions, EWBActionType::EndTurn) : nullptr;
	if (End == nullptr)
	{
		OutReason = Legal.bOk
			? FString(TEXT("terrain_cartographer_end_turn_missing")) : Legal.Reason;
		return false;
	}
	return Submit(Coordinator, Recorder, *End, OutReason)
		&& ResolveTurnStart(Coordinator, Recorder, OutReason);
}

bool Summon(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FString& CardId,
	const FWBTile Tile,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Action = Legal.bOk
		? FindSummon(Legal.Actions, CardId, Tile) : nullptr;
	if (Action == nullptr)
	{
		OutReason = Legal.bOk
			? FString::Printf(TEXT("terrain_cartographer_summon_missing:%s"), *CardId)
			: Legal.Reason;
		return false;
	}
	return SubmitAndResolve(Coordinator, Recorder, *Action, OutReason);
}

bool Activate(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FString& CardId,
	const FWBTile Tile,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Action = Legal.bOk
		? FindActivation(Legal.Actions, CardId, Tile) : nullptr;
	if (Action == nullptr)
	{
		OutReason = Legal.bOk
			? FString::Printf(TEXT("terrain_cartographer_activation_missing:%s"), *CardId)
			: Legal.Reason;
		return false;
	}
	return SubmitAndResolve(Coordinator, Recorder, *Action, OutReason);
}

bool MoveUnit(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const int32 SourceUnitId,
	const FWBTile Tile,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Action = Legal.bOk
		? FindMove(Legal.Actions, SourceUnitId, Tile) : nullptr;
	if (Action == nullptr)
	{
		OutReason = Legal.bOk
			? FString(TEXT("terrain_cartographer_vex_move_missing")) : Legal.Reason;
		return false;
	}
	return SubmitAndResolve(Coordinator, Recorder, *Action, OutReason);
}

bool HasTerrainTrace(
	const TArray<FWBTraceEvent>& Trace,
	const FName Previous,
	const FName Next,
	const FWBTile Tile)
{
	return Trace.ContainsByPredicate([Previous, Next, Tile](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(TEXT("terrain_changed"))
			&& Event.PreviousTerrainId == Previous
			&& Event.NewTerrainId == Next
			&& Event.ToTile == Tile;
	});
}
}

bool WBProductionTerrainCartographerSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionTerrainCartographerSmoke"));
}

FString WBProductionTerrainCartographerSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionTerrainCartographerReceipt.json"));
}

FWBProductionTerrainCartographerSmokeResult
WBProductionTerrainCartographerSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionTerrainCartographerSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk || !Bootstrap.Database.IsValid())
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(
		Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap), Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	if (!Summon(Coordinator, Recorder, TEXT("char_mire_cartographer"),
			FWBTile(4, 7), Result.Reason)
		|| !Summon(Coordinator, Recorder, TEXT("char_emberfault_cartographer"),
			FWBTile(3, 8), Result.Reason)
		|| !Summon(Coordinator, Recorder, TEXT("char_tidecall_cartographer"),
			FWBTile(5, 8), Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason)
		|| !Summon(Coordinator, Recorder, TEXT("char_rimecall_cartographer"),
			FWBTile(3, 0), Result.Reason)
		|| !Summon(Coordinator, Recorder, TEXT("char_csn_vex"),
			FWBTile(4, 1), Result.Reason)
		|| !Activate(Coordinator, Recorder, TEXT("char_rimecall_cartographer"),
			FWBTile(3, 0), Result.Reason))
	{
		return Result;
	}
	const FWBUnitState* Vex = Coordinator.GetState().Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("char_csn_vex") && Unit.IsUnitOnBoard();
		});
	if (Vex == nullptr
		|| !MoveUnit(Coordinator, Recorder, Vex->UnitId,
			FWBTile(4, 2), Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason)
		|| !MoveUnit(Coordinator, Recorder, Vex->UnitId,
			FWBTile(4, 3), Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason)
		|| !MoveUnit(Coordinator, Recorder, Vex->UnitId,
			FWBTile(4, 4), Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}

	const FWBMatchLegalActionGenerationResult EffectiveRangeLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* WallProbeAction = FindActivation(
		EffectiveRangeLegal.Actions,
		TEXT("char_mire_cartographer"),
		FWBTile(4, 5));
	if (!EffectiveRangeLegal.bOk
		|| WallProbeAction == nullptr
		|| FindActivation(EffectiveRangeLegal.Actions,
			TEXT("char_mire_cartographer"), FWBTile(4, 4)) != nullptr)
	{
		Result.Reason = TEXT("terrain_cartographer_effective_ar_mismatch");
		return Result;
	}
	FWBGameStateData WallProbe = Coordinator.GetState();
	FWBUnitState* ProbeVex = WallProbe.GetMutableUnitById(Vex->UnitId);
	if (ProbeVex == nullptr)
	{
		Result.Reason = TEXT("terrain_cartographer_wall_probe_unit_missing");
		return Result;
	}
	ProbeVex->X = 4;
	ProbeVex->Y = 6;
	WallProbe.Walls.Add(FWBWallEdge(FWBTile(4, 6), FWBTile(4, 5)));
	FWBRuntimeActivationExecutionHandoffResult WallProbeHandoff;
	WallProbeHandoff.bHandoffOk = true;
	WallProbeHandoff.bSelectionResolved = true;
	WallProbeHandoff.SelectedActivationActionId = WallProbeAction->ActionId;
	WallProbeHandoff.ActivationAction.ActivationActionId =
		WallProbeAction->ActionId;
	WallProbeHandoff.ActivationAction.Command =
		WallProbeAction->ActivationCommand;
	const FWBRuntimeActivationExecutionResult WallProbeExecution =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			WallProbe,
			Bootstrap.Database->CoreRepository,
			WallProbeHandoff);
	if (!WallProbeExecution.bOk
		|| WallProbe.GetTerrainAt(FWBTile(4, 5)) != FName(TEXT("mud")))
	{
		Result.Reason = FString::Printf(
			TEXT("terrain_cartographer_wall_probe_failed:%s"),
			*WallProbeExecution.Reason);
		return Result;
	}

	if (!Activate(Coordinator, Recorder, TEXT("char_mire_cartographer"),
			FWBTile(4, 7), Result.Reason))
	{
		return Result;
	}
	const FWBMatchLegalActionGenerationResult UsedLegal =
		Coordinator.EnumerateLegalActions();
	if (!UsedLegal.bOk || UsedLegal.Actions.ContainsByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceCardId
					== TEXT("char_mire_cartographer");
		}))
	{
		Result.Reason = TEXT("terrain_cartographer_once_per_turn_mismatch");
		return Result;
	}
	if (!Activate(Coordinator, Recorder, TEXT("char_emberfault_cartographer"),
			FWBTile(4, 7), Result.Reason)
		|| !Activate(Coordinator, Recorder, TEXT("char_tidecall_cartographer"),
			FWBTile(5, 8), Result.Reason))
	{
		return Result;
	}

	const FWBGameStateData& State = Coordinator.GetState();
	if (State.GetTerrainAt(FWBTile(4, 7)) != FName(TEXT("lava"))
		|| State.GetTerrainAt(FWBTile(5, 8)) != FName(TEXT("water"))
		|| State.GetTerrainAt(FWBTile(3, 0)) != FName(TEXT("ice"))
		|| !HasTerrainTrace(Coordinator.GetTraceLog(),
			FName(TEXT("normal")), FName(TEXT("mud")), FWBTile(4, 7))
		|| !HasTerrainTrace(Coordinator.GetTraceLog(),
			FName(TEXT("mud")), FName(TEXT("lava")), FWBTile(4, 7)))
	{
		Result.Reason = TEXT("terrain_cartographer_final_terrain_mismatch");
		return Result;
	}

	const FWBMatchObservation Public = Coordinator.BuildObservation(0);
	if (Public.PublicBoard.TerrainTiles.Num() != 3)
	{
		Result.Reason = TEXT("terrain_cartographer_public_terrain_mismatch");
		return Result;
	}

	const FString ArchiveBytes = WBProductionMatchReplay::Serialize(
		Recorder.GetArchive());
	FString PersistedBytes;
	const FWBProductionMatchReplayPersistenceResult Loaded =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(), PersistedBytes);
	if (!Loaded.bOk || PersistedBytes != ArchiveBytes)
	{
		Result.Reason = Loaded.bOk
			? TEXT("terrain_cartographer_archive_mismatch") : Loaded.FailureCode;
		return Result;
	}
	FWBProductionMatchReplayRunRequest ReplayRequest;
	ReplayRequest.SerializedArchive = PersistedBytes;
	ReplayRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(ReplayRequest);
	if (!Replay.bValid
		|| Replay.FinalStateDigest != Coordinator.GetCurrentStateDigest()
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest())
	{
		Result.Reason = Replay.FailureCode.IsEmpty()
			? TEXT("terrain_cartographer_fresh_replay_mismatch")
			: Replay.FailureCode;
		return Result;
	}

	const FString ReceiptJson = WBProductionMatchReplay::SerializeReceipt(
		Recorder.GetReceipt());
	TSharedPtr<FJsonObject> ReceiptObject;
	if (!FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(ReceiptJson), ReceiptObject)
		|| !ReceiptObject.IsValid() || ReceiptObject->Values.Num() != 8
		|| ReceiptJson.Contains(TEXT("state_digest"))
		|| ReceiptJson.Contains(TEXT("trace_digest"))
		|| ReceiptJson.Contains(TEXT("ordered_deck")))
	{
		Result.Reason = TEXT("terrain_cartographer_receipt_privacy_mismatch");
		return Result;
	}
	const FString ReceiptPath = GetReceiptPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	if (!FFileHelper::SaveStringToFile(
		ReceiptJson, *ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("replay_write_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("production_terrain_cartographers_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalGeneration = Coordinator.GetCoordinatorGeneration();
	Result.FinalRevision = Coordinator.GetCoordinatorRevision();
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	Result.SerializedArchive = PersistedBytes;
	Result.SerializedReceipt = ReceiptJson;
	return Result;
}
