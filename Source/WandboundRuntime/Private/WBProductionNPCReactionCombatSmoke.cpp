#include "WBProductionNPCReactionCombatSmoke.h"

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
const FWBMatchLegalAction* FindNPCCombatEquip(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 HeroUnitId)
{
	return Actions.FindByPredicate([HeroUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Equip
			&& Action.EquipRequest.SourceCardId == TEXT("npc_reaction_wand")
			&& Action.EquipRequest.TargetUnitId == HeroUnitId;
	});
}

const FWBMatchLegalAction* FindNPCCombatCoreAction(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type,
	const int32 SourceUnitId = INDEX_NONE,
	const int32 TargetUnitId = INDEX_NONE)
{
	return Actions.FindByPredicate(
		[Type, SourceUnitId, TargetUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == Type
				&& (SourceUnitId == INDEX_NONE
					|| Action.CoreAction.SourceUnitId == SourceUnitId)
				&& (TargetUnitId == INDEX_NONE
					|| Action.CoreAction.TargetUnitId == TargetUnitId);
		});
}

const FWBMatchLegalAction* FindNPCCombatMove(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 HeroUnitId,
	const FWBTile& Destination)
{
	return Actions.FindByPredicate(
		[HeroUnitId, Destination](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == EWBActionType::Move
				&& Action.CoreAction.SourceUnitId == HeroUnitId
				&& Action.CoreAction.ToTile == Destination;
		});
}

const FWBMatchLegalAction* FindNPCCombatReaction(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 HeroUnitId)
{
	return Actions.FindByPredicate([HeroUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId
				== TEXT("npc_reaction_reinforce")
			&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId
				== HeroUnitId;
	});
}

bool SubmitNPCCombatCapture(
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

bool PassNPCCombatResponses(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FWBMatchOperationResult& OutOperation,
	FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 8 && Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response;
		++Guard)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindNPCCombatCoreAction(
				Legal.Actions,
				EWBActionType::PassResponse)
			: nullptr;
		if (Pass == nullptr)
		{
			OutReason = Legal.bOk
				? FString(TEXT("npc_reaction_combat_response_pass_missing"))
				: Legal.Reason;
			return false;
		}
		if (!SubmitNPCCombatCapture(
			Coordinator, Recorder, *Pass, OutOperation, OutReason))
		{
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response)
	{
		OutReason = TEXT("npc_reaction_combat_response_guard_exceeded");
		return false;
	}
	return true;
}

bool NPCCombatHasTrace(
	const TArray<FWBTraceEvent>& Events,
	const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

bool NPCCombatHasOnlyResponseActions(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return !Actions.IsEmpty() && !Actions.ContainsByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family != EWBMatchActionFamily::Activation
				&& !(Action.Family == EWBMatchActionFamily::CoreAction
					&& Action.CoreAction.Type == EWBActionType::PassResponse);
		});
}

bool NPCCombatOpponentHandHidden(
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

bool WBProductionNPCReactionCombatSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionNPCReactionCombatSmoke"));
}

FString WBProductionNPCReactionCombatSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionNPCReactionCombatReceipt.json"));
}

FWBProductionNPCReactionCombatSmokeResult
WBProductionNPCReactionCombatSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionNPCReactionCombatSmokeResult Result;
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
	if (PlayerZero == nullptr || PlayerZero->HeroUnitId < 0)
	{
		Result.Reason = TEXT("npc_reaction_combat_hero_missing");
		return Result;
	}
	const int32 HeroUnitId = PlayerZero->HeroUnitId;

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap),
		Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	FWBMatchOperationResult Operation;
	const FWBMatchLegalAction* Equip = FindNPCCombatEquip(
		Started.NextLegalActions,
		HeroUnitId);
	if (Equip == nullptr
		|| !SubmitNPCCombatCapture(
			Coordinator, Recorder, *Equip, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("npc_reaction_combat_equip_missing");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult MoveLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Move = MoveLegal.bOk
		? FindNPCCombatMove(MoveLegal.Actions, HeroUnitId, FWBTile(4, 7))
		: nullptr;
	if (Move == nullptr
		|| !SubmitNPCCombatCapture(
			Coordinator, Recorder, *Move, Operation, Result.Reason)
		|| !NPCCombatHasTrace(Operation.TraceEvents, TEXT("npc_spawn_scheduled")))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("npc_reaction_combat_marker_move_missing");
		return Result;
	}
	if (!PassNPCCombatResponses(Coordinator, Recorder, Operation, Result.Reason))
	{
		return Result;
	}

	const FWBMatchLegalActionGenerationResult EndLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = EndLegal.bOk
		? FindNPCCombatCoreAction(EndLegal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr
		|| !SubmitNPCCombatCapture(
			Coordinator, Recorder, *EndTurn, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("npc_reaction_combat_end_turn_missing");
		return Result;
	}
	const FWBGameStateData& ResponseState = Coordinator.GetState();
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Response
		|| !ResponseState.HasPendingAttack()
		|| ResponseState.PendingAttack.AuthorityKind
			!= EWBAttackAuthorityKind::NeutralNPC
		|| ResponseState.PendingAttack.Stage
			!= EWBAttackContinuationStage::PreHit
		|| !ResponseState.NPCPhaseContinuation.bActive
		|| !ResponseState.NPCPhaseContinuation.bWaitingForAttackContinuation
		|| ResponseState.PriorityPlayer != 0)
	{
		Result.Reason = TEXT("npc_reaction_combat_pre_hit_state_mismatch");
		return Result;
	}
	const int32 NPCUnitId = ResponseState.PendingAttack.AttackerUnitId;
	const FWBMatchObservation ResponseObservation = Coordinator.BuildObservation(0);
	const FWBMatchObservation NonPriorityObservation = Coordinator.BuildObservation(1);
	if (ResponseObservation.MatchPhase != EWBMatchLoopPhase::Response
		|| ResponseObservation.PublicTurn.PriorityPlayerId != 0
		|| ResponseObservation.PublicTurn.ReactionWindowKind != FName(TEXT("pre_hit"))
		|| !NPCCombatHasOnlyResponseActions(ResponseObservation.LegalActions)
		|| !NonPriorityObservation.LegalActions.IsEmpty()
		|| !NPCCombatOpponentHandHidden(ResponseObservation, 1))
	{
		Result.Reason = TEXT("npc_reaction_combat_public_response_mismatch");
		return Result;
	}
	const FWBMatchLegalAction* React = FindNPCCombatReaction(
		ResponseObservation.LegalActions,
		HeroUnitId);
	if (React == nullptr)
	{
		Result.Reason = TEXT("npc_reaction_combat_react_missing");
		return Result;
	}
	Result.ReactionActionId = React->ActionId;
	if (!SubmitNPCCombatCapture(
		Coordinator, Recorder, *React, Operation, Result.Reason))
	{
		return Result;
	}
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| Coordinator.GetState().CurrentPlayer != 1
		|| Coordinator.GetState().HasPendingAttack()
		|| Coordinator.GetState().NPCPhaseContinuation.bActive
		|| !NPCCombatHasTrace(Operation.TraceEvents, TEXT("npc_attack_damage_resolved"))
		|| !NPCCombatHasTrace(Operation.TraceEvents, TEXT("counter_started"))
		|| !NPCCombatHasTrace(Operation.TraceEvents, TEXT("npc_phase_ended")))
	{
		Result.Reason = TEXT("npc_reaction_combat_resume_mismatch");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult PlayerOneLegal =
		Coordinator.EnumerateLegalActions();
	EndTurn = PlayerOneLegal.bOk
		? FindNPCCombatCoreAction(PlayerOneLegal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr
		|| !SubmitNPCCombatCapture(
			Coordinator, Recorder, *EndTurn, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("npc_reaction_combat_return_turn_missing");
		return Result;
	}
	if (!PassNPCCombatResponses(Coordinator, Recorder, Operation, Result.Reason)
		|| Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| Coordinator.GetState().CurrentPlayer != 0)
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("npc_reaction_combat_return_turn_missing");
		return Result;
	}

	const FWBUnitState* HeroBeforeAttack = Coordinator.GetState().GetUnitById(HeroUnitId);
	const FWBUnitState* NPCBeforeAttack = Coordinator.GetState().GetUnitById(NPCUnitId);
	if (HeroBeforeAttack == nullptr || NPCBeforeAttack == nullptr)
	{
		Result.Reason = TEXT("npc_reaction_combat_participant_missing");
		return Result;
	}
	const int32 HeroHPBeforeAttack = HeroBeforeAttack->HP;
	const int32 NPCHPBeforeAttack = NPCBeforeAttack->HP;
	const FWBMatchLegalActionGenerationResult AttackLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = AttackLegal.bOk
		? FindNPCCombatCoreAction(
			AttackLegal.Actions,
			EWBActionType::Attack,
			HeroUnitId,
			NPCUnitId)
		: nullptr;
	if (Attack == nullptr)
	{
		Result.Reason = TEXT("npc_reaction_combat_player_attack_missing");
		return Result;
	}
	Result.PlayerAttackActionId = Attack->ActionId;
	if (!SubmitNPCCombatCapture(
		Coordinator, Recorder, *Attack, Operation, Result.Reason))
	{
		return Result;
	}
	if (!PassNPCCombatResponses(Coordinator, Recorder, Operation, Result.Reason))
	{
		return Result;
	}
	const FWBUnitState* HeroAfterAttack = Coordinator.GetState().GetUnitById(HeroUnitId);
	const FWBUnitState* NPCAfterAttack = Coordinator.GetState().GetUnitById(NPCUnitId);
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| Coordinator.GetState().HasPendingAttack()
		|| HeroAfterAttack == nullptr
		|| NPCAfterAttack == nullptr
		|| HeroAfterAttack->HP != HeroHPBeforeAttack
		|| NPCAfterAttack->HP >= NPCHPBeforeAttack
		|| NPCCombatHasTrace(Operation.TraceEvents, TEXT("counter_started")))
	{
		Result.Reason = TEXT("npc_reaction_combat_counterability_mismatch");
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
			? FString(TEXT("npc_reaction_combat_archive_mismatch"))
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
			? FString(TEXT("npc_reaction_combat_fresh_replay_mismatch"))
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
		|| ReceiptJson.Contains(TEXT("pending_effect_frame_id"))
		|| ReceiptJson.Contains(TEXT("npc_reaction_filler")))
	{
		Result.Reason = TEXT("npc_reaction_combat_receipt_privacy_mismatch");
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
	Result.Reason = TEXT("production_npc_reaction_combat_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
