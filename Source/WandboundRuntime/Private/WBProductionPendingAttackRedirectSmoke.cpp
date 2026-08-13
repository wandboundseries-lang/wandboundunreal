#include "WBProductionPendingAttackRedirectSmoke.h"

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
const FWBMatchLegalAction* FindRedirectCoreAction(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindRedirectSummon(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& !Action.bHybridSummon
			&& Action.SummonRequest.SourceCardId
				== TEXT("redirect_fixture_attacker")
			&& Action.SummonRequest.TargetTile == FWBTile(4, 7);
	});
}

const FWBMatchLegalAction* FindRedirectActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId,
	const int32 TargetUnitId = INDEX_NONE)
{
	return Actions.FindByPredicate(
		[&EffectId, TargetUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceEffectId == EffectId
				&& (TargetUnitId == INDEX_NONE
					|| Action.ActivationCommand.EffectRequest.Target.TargetUnitId
						== TargetUnitId);
		});
}

bool SubmitRedirectCapture(
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

bool ResolveRedirectTurnStart(
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
				? FString(TEXT("pending_attack_redirect_turn_start_choice_missing"))
				: Legal.Reason;
			return false;
		}
		FWBMatchOperationResult Operation;
		if (!SubmitRedirectCapture(
			Coordinator, Recorder, Legal.Actions[0], Operation, OutReason))
		{
			return false;
		}
	}
	return true;
}

bool ResolveRedirectResponseWindow(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 24 && Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response;
		++Guard)
	{
		const FWBMatchObservation Observation = Coordinator.BuildObservation(
			Coordinator.GetState().PriorityPlayer);
		const FWBMatchLegalAction* Pass = FindRedirectCoreAction(
			Observation.LegalActions, EWBActionType::PassResponse);
		if (Pass == nullptr)
		{
			OutReason = TEXT("pending_attack_redirect_pass_missing");
			return false;
		}
		FWBMatchOperationResult Operation;
		if (!SubmitRedirectCapture(
			Coordinator, Recorder, *Pass, Operation, OutReason))
		{
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response)
	{
		OutReason = TEXT("pending_attack_redirect_response_guard_exceeded");
		return false;
	}
	return true;
}

bool EndRedirectTurn(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = Legal.bOk
		? FindRedirectCoreAction(Legal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr)
	{
		OutReason = Legal.bOk
			? FString(TEXT("pending_attack_redirect_end_turn_missing"))
			: Legal.Reason;
		return false;
	}
	FWBMatchOperationResult Operation;
	return SubmitRedirectCapture(
		Coordinator, Recorder, *EndTurn, Operation, OutReason)
		&& ResolveRedirectTurnStart(Coordinator, Recorder, OutReason);
}

bool RedirectOpponentHandHidden(
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

int32 CountRedirectTraces(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind,
	const int32 TargetUnitId = INDEX_NONE)
{
	int32 Count = 0;
	for (const FWBTraceEvent& Event : Events)
	{
		if (Event.Kind == Kind
			&& (TargetUnitId == INDEX_NONE || Event.TargetUnitId == TargetUnitId))
		{
			++Count;
		}
	}
	return Count;
}

int32 CountRedirectAcceptedAttacks(const WBMatchCoordinator& Coordinator)
{
	int32 Count = 0;
	for (const FWBMatchCommittedActionRecord& Record :
		Coordinator.GetCommittedActionRecords())
	{
		if (Record.ChosenActionId.StartsWith(TEXT("attack:")))
		{
			++Count;
		}
	}
	return Count;
}
}

bool WBProductionPendingAttackRedirectSmoke::IsRequested(
	const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionPendingAttackRedirectSmoke"));
}

FString WBProductionPendingAttackRedirectSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionPendingAttackRedirectReceipt.json"));
}

FWBProductionPendingAttackRedirectSmokeResult
WBProductionPendingAttackRedirectSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionPendingAttackRedirectSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
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
	const FWBPlayerStateData* PlayerZero = Coordinator.GetState().GetPlayerById(0);
	const FWBPlayerStateData* PlayerOne = Coordinator.GetState().GetPlayerById(1);
	if (PlayerZero == nullptr || PlayerOne == nullptr)
	{
		Result.Reason = TEXT("pending_attack_redirect_hero_missing");
		return Result;
	}
	const int32 RedirectTargetUnitId = PlayerZero->HeroUnitId;
	const int32 OriginalDefenderUnitId = PlayerOne->HeroUnitId;
	const FWBUnitState* RedirectTarget =
		Coordinator.GetState().GetUnitById(RedirectTargetUnitId);
	const FWBUnitState* OriginalDefender =
		Coordinator.GetState().GetUnitById(OriginalDefenderUnitId);
	if (RedirectTarget == nullptr || OriginalDefender == nullptr)
	{
		Result.Reason = TEXT("pending_attack_redirect_participant_missing");
		return Result;
	}
	const int32 RedirectTargetHPBefore = RedirectTarget->HP;
	const int32 OriginalDefenderHPBefore = OriginalDefender->HP;

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap), Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	FWBMatchOperationResult Operation;
	const FWBMatchLegalAction* Summon = FindRedirectSummon(
		Started.NextLegalActions);
	if (Summon == nullptr
		|| !SubmitRedirectCapture(
			Coordinator, Recorder, *Summon, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("pending_attack_redirect_summon_missing");
		}
		return Result;
	}
	const TArray<const FWBUnitState*> PlayerZeroUnits =
		Coordinator.GetState().GetUnitsForPlayer(0);
	const FWBUnitState* const* SummonedAttackerEntry =
		PlayerZeroUnits.FindByPredicate(
			[](const FWBUnitState* Unit)
			{
				return Unit != nullptr
					&& Unit->X == 4
					&& Unit->Y == 7;
			});
	const FWBUnitState* SummonedAttacker = SummonedAttackerEntry != nullptr
		? *SummonedAttackerEntry
		: nullptr;
	const int32 ResolvedAttackerUnitId = SummonedAttacker != nullptr
		? SummonedAttacker->UnitId
		: INDEX_NONE;
	if (ResolvedAttackerUnitId < 0
		|| !ResolveRedirectResponseWindow(
			Coordinator, Recorder, Result.Reason)
		|| !EndRedirectTurn(Coordinator, Recorder, Result.Reason)
		|| !EndRedirectTurn(Coordinator, Recorder, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("pending_attack_redirect_attacker_missing");
		}
		return Result;
	}

	const FWBUnitState* Attacker =
		Coordinator.GetState().GetUnitById(ResolvedAttackerUnitId);
	const int32 AttacksLeftBefore = Attacker != nullptr
		? Attacker->AttacksLeft
		: -1;
	const FWBMatchLegalActionGenerationResult AttackLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = AttackLegal.bOk
		? AttackLegal.Actions.FindByPredicate(
			[ResolvedAttackerUnitId, OriginalDefenderUnitId](
				const FWBMatchLegalAction& Action)
			{
				return Action.Family == EWBMatchActionFamily::CoreAction
					&& Action.CoreAction.Type == EWBActionType::Attack
					&& Action.CoreAction.SourceUnitId == ResolvedAttackerUnitId
					&& Action.CoreAction.TargetUnitId == OriginalDefenderUnitId;
			})
		: nullptr;
	if (Attack == nullptr
		|| !SubmitRedirectCapture(
			Coordinator, Recorder, *Attack, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("pending_attack_redirect_attack_missing");
		}
		return Result;
	}
	if (!Coordinator.GetState().HasPendingAttack())
	{
		Result.Reason = TEXT("pending_attack_redirect_pre_hit_missing");
		return Result;
	}
	const FWBPendingAttackState Declared = Coordinator.GetState().PendingAttack;
	if (Declared.DefenderUnitId != OriginalDefenderUnitId
		|| Declared.OriginalDefenderUnitId != OriginalDefenderUnitId
		|| Declared.AttackerUnitId != ResolvedAttackerUnitId
		|| Declared.OriginalAttackerUnitId != ResolvedAttackerUnitId
		|| Declared.DeclarationActionId != Attack->ActionId)
	{
		Result.Reason = TEXT("pending_attack_redirect_declaration_identity_mismatch");
		return Result;
	}

	const FWBMatchObservation AObservation = Coordinator.BuildObservation(1);
	if (!RedirectOpponentHandHidden(AObservation, 0))
	{
		Result.Reason = TEXT("pending_attack_redirect_hidden_hand_leak");
		return Result;
	}
	const FWBMatchLegalAction* A = FindRedirectActivation(
		AObservation.LegalActions, TEXT("redirect_fixture_prevent_a"));
	if (A == nullptr
		|| !SubmitRedirectCapture(
			Coordinator, Recorder, *A, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("pending_attack_redirect_prevent_a_missing");
		}
		return Result;
	}

	const FWBMatchObservation BObservation = Coordinator.BuildObservation(0);
	const FWBMatchLegalAction* B = FindRedirectActivation(
		BObservation.LegalActions, TEXT("redirect_fixture_negate_b"));
	if (B == nullptr
		|| !SubmitRedirectCapture(
			Coordinator, Recorder, *B, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("pending_attack_redirect_negate_b_missing");
		}
		return Result;
	}

	const FWBMatchObservation CObservation = Coordinator.BuildObservation(1);
	if (!RedirectOpponentHandHidden(CObservation, 0))
	{
		Result.Reason = TEXT("pending_attack_redirect_nested_hidden_hand_leak");
		return Result;
	}
	const FWBMatchLegalAction* C = FindRedirectActivation(
		CObservation.LegalActions,
		TEXT("redirect_fixture_redirect_c"),
		RedirectTargetUnitId);
	if (C == nullptr
		|| !SubmitRedirectCapture(
			Coordinator, Recorder, *C, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("pending_attack_redirect_redirect_c_missing");
		}
		return Result;
	}

	if (!ResolveRedirectResponseWindow(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}

	RedirectTarget = Coordinator.GetState().GetUnitById(RedirectTargetUnitId);
	OriginalDefender = Coordinator.GetState().GetUnitById(OriginalDefenderUnitId);
	Attacker = Coordinator.GetState().GetUnitById(ResolvedAttackerUnitId);
	const TArray<FWBTraceEvent>& Trace = Coordinator.GetTraceLog();
	const int32 RedirectRestoreCount = Trace.FilterByPredicate(
		[RedirectTargetUnitId, &Declared](const FWBTraceEvent& Event)
		{
			return Event.Kind
					== FName(TEXT("pending_effect_parent_context_restored"))
				&& Event.TargetUnitId == RedirectTargetUnitId
				&& Event.SourceUnitId == Declared.AttackerUnitId
				&& Event.AttackContinuationId == Declared.ContinuationId;
		}).Num();
	const FWBTraceEvent* Redirected = Trace.FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("pending_attack_redirected"));
		});
	const FWBTraceEvent* Damage = Trace.FindByPredicate(
		[RedirectTargetUnitId](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("attack_damage_resolved"))
				&& Event.TargetUnitId == RedirectTargetUnitId;
		});
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| Coordinator.GetState().HasPendingAttack()
		|| Coordinator.GetState().HasOpenReactionWindow()
		|| !Coordinator.GetPendingEffectActivationStack().IsEmpty()
		|| Redirected == nullptr
		|| Redirected->PreviousTargetUnitId != OriginalDefenderUnitId
		|| Redirected->TargetUnitId != RedirectTargetUnitId
		|| Redirected->SourceUnitId != Declared.AttackerUnitId
		|| Redirected->AttackContinuationId != Declared.ContinuationId
		|| RedirectRestoreCount < 3
		|| Damage == nullptr
		|| Damage->ActionId != Declared.DeclarationActionId
		|| Damage->SourceUnitId != Declared.OriginalAttackerUnitId
		|| RedirectTarget == nullptr
		|| RedirectTarget->HP != RedirectTargetHPBefore - 3
		|| OriginalDefender == nullptr
		|| OriginalDefender->HP != OriginalDefenderHPBefore
		|| Attacker == nullptr
		|| Attacker->AttacksLeft != AttacksLeftBefore - 1
		|| CountRedirectTraces(
			Trace, FName(TEXT("attack_damage_resolved")), RedirectTargetUnitId) != 1
		|| CountRedirectTraces(
			Trace, FName(TEXT("attack_damage_resolved")), OriginalDefenderUnitId) != 0
		|| CountRedirectTraces(
			Trace, FName(TEXT("pending_attack_redirected"))) != 1
		|| CountRedirectTraces(Trace, FName(TEXT("attack_prevented"))) != 0
		|| CountRedirectAcceptedAttacks(Coordinator) != 1)
	{
		Result.Reason = TEXT("pending_attack_redirect_resolution_mismatch");
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
			? FString(TEXT("pending_attack_redirect_archive_mismatch"))
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
			? FString(TEXT("pending_attack_redirect_fresh_replay_mismatch"))
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
		|| ReceiptJson.Contains(TEXT("original_defender"))
		|| ReceiptJson.Contains(TEXT("current_defender"))
		|| ReceiptJson.Contains(TEXT("redirect_fixture_filler"))
		|| ReceiptJson.Contains(TEXT("Data/Replay")))
	{
		Result.Reason = TEXT("pending_attack_redirect_receipt_privacy_mismatch");
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
	Result.Reason = TEXT("production_pending_attack_redirect_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
