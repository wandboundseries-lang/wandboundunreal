#include "WBProductionStatusAuthoritySmoke.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBStatusTypes.h"

namespace
{
const FWBMatchLegalAction* FindStatusSmokeCoreAction(
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

const FWBMatchLegalAction* FindFreezeActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 SourceUnitId)
{
	return Actions.FindByPredicate(
		[SourceUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceUnitId == SourceUnitId
				&& Action.ActivationCommand.Source.SourceEffectId
					== TEXT("fixture_freeze_self")
				&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId
					== SourceUnitId;
		});
}

bool SubmitAndCapture(
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

bool PassResponses(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 16 && Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response;
		++Guard)
	{
		const FWBMatchObservation Observation = Coordinator.BuildObservation(
			Coordinator.GetState().PriorityPlayer);
		const FWBMatchLegalAction* Pass = FindStatusSmokeCoreAction(
			Observation.LegalActions, EWBActionType::PassResponse);
		if (Pass == nullptr || !SubmitAndCapture(
			Coordinator, Recorder, *Pass, OutReason))
		{
			OutReason = OutReason.IsEmpty()
				? FString(TEXT("status_authority_response_pass_missing"))
				: OutReason;
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response)
	{
		OutReason = TEXT("status_authority_response_guard_exceeded");
		return false;
	}
	return true;
}
}

bool WBProductionStatusAuthoritySmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionStatusAuthoritySmoke"));
}

FString WBProductionStatusAuthoritySmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionStatusAuthorityReceipt.json"));
}

FWBProductionStatusAuthoritySmokeResult WBProductionStatusAuthoritySmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionStatusAuthoritySmokeResult Result;
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
	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap), Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	const FWBPlayerStateData* PlayerZero = Coordinator.GetState().GetPlayerById(0);
	const FWBPlayerStateData* PlayerOne = Coordinator.GetState().GetPlayerById(1);
	const int32 AttackerUnitId = PlayerZero != nullptr
		? PlayerZero->HeroUnitId : INDEX_NONE;
	const int32 FrozenUnitId = PlayerOne != nullptr
		? PlayerOne->HeroUnitId : INDEX_NONE;
	if (AttackerUnitId == INDEX_NONE || FrozenUnitId == INDEX_NONE)
	{
		Result.Reason = TEXT("status_authority_smoke_hero_missing");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult FirstTurnLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* FirstEndTurn = FirstTurnLegal.bOk
		? FindStatusSmokeCoreAction(
			FirstTurnLegal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (FirstEndTurn == nullptr || !SubmitAndCapture(
		Coordinator, Recorder, *FirstEndTurn, Result.Reason))
	{
		Result.Reason = Result.Reason.IsEmpty()
			? FString(TEXT("status_authority_first_end_turn_missing"))
			: Result.Reason;
		return Result;
	}

	const FWBMatchLegalActionGenerationResult FreezeLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Freeze = FreezeLegal.bOk
		? FindFreezeActivation(FreezeLegal.Actions, FrozenUnitId)
		: nullptr;
	if (Freeze == nullptr || !SubmitAndCapture(
		Coordinator, Recorder, *Freeze, Result.Reason)
		|| !PassResponses(Coordinator, Recorder, Result.Reason))
	{
		Result.Reason = Result.Reason.IsEmpty()
			? FString(TEXT("status_authority_freeze_activation_missing"))
			: Result.Reason;
		return Result;
	}

	const FWBUnitState* FrozenUnit = Coordinator.GetState().GetUnitById(FrozenUnitId);
	const FWBStatusInstanceState* Frozen = FrozenUnit != nullptr
		? FrozenUnit->GetStatusState(FName(TEXT("Frozen"))) : nullptr;
	if (Frozen == nullptr
		|| Frozen->Duration != 2
		|| Frozen->Source.SourcePlayerId != 1
		|| Frozen->Source.SourceOwnerPlayerId != 1
		|| Frozen->Source.SourceUnitId != FrozenUnitId
		|| Frozen->Source.SourceCardId != TEXT("status_authority_fixture_hero_beta")
		|| Frozen->Source.SourceEffectId != TEXT("fixture_freeze_self")
		|| Frozen->Source.Origin != EWBStatusApplicationOrigin::Activation)
	{
		Result.Reason = TEXT("status_authority_provenance_mismatch");
		return Result;
	}

	const int32 HPBefore = FrozenUnit->HP;
	const int32 ArmorBefore = FrozenUnit->GetCurrentArmor();
	const FWBMatchLegalActionGenerationResult SecondTurnLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* SecondEndTurn = SecondTurnLegal.bOk
		? FindStatusSmokeCoreAction(
			SecondTurnLegal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (SecondEndTurn == nullptr || !SubmitAndCapture(
		Coordinator, Recorder, *SecondEndTurn, Result.Reason))
	{
		Result.Reason = Result.Reason.IsEmpty()
			? FString(TEXT("status_authority_second_end_turn_missing"))
			: Result.Reason;
		return Result;
	}
	FrozenUnit = Coordinator.GetState().GetUnitById(FrozenUnitId);
	Frozen = FrozenUnit != nullptr
		? FrozenUnit->GetStatusState(FName(TEXT("Frozen"))) : nullptr;
	if (Frozen == nullptr
		|| Frozen->Duration != 1
		|| Frozen->Source.SourceUnitId != FrozenUnitId)
	{
		Result.Reason = TEXT("status_authority_end_turn_tick_mismatch");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult AttackLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = AttackLegal.bOk
		? FindStatusSmokeCoreAction(
			AttackLegal.Actions,
			EWBActionType::Attack,
			AttackerUnitId,
			FrozenUnitId)
		: nullptr;
	if (Attack == nullptr || !SubmitAndCapture(
		Coordinator, Recorder, *Attack, Result.Reason)
		|| !PassResponses(Coordinator, Recorder, Result.Reason))
	{
		Result.Reason = Result.Reason.IsEmpty()
			? FString(TEXT("status_authority_attack_missing"))
			: Result.Reason;
		return Result;
	}

	FrozenUnit = Coordinator.GetState().GetUnitById(FrozenUnitId);
	const TArray<FWBTraceEvent>& Trace = Coordinator.GetTraceLog();
	const FWBTraceEvent* Applied = Trace.FindByPredicate(
		[FrozenUnitId](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("status_modified"))
				&& Event.StatusId == FName(TEXT("Frozen"))
				&& Event.SourceUnitId == FrozenUnitId
				&& Event.TargetUnitId == FrozenUnitId;
		});
	const FWBTraceEvent* Tick = Trace.FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("end_turn_status_ticks"))
				&& Event.PlayerId == 1;
		});
	const FWBTraceEvent* FrozenDamage = Trace.FindByPredicate(
		[FrozenUnitId](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("attack_damage_applied"))
				&& Event.TargetUnitId == FrozenUnitId
				&& Event.bFrozenBreak
				&& Event.ArmorAbsorbedAmount == 0
				&& Event.HPDamageAmount == 0;
		});
	if (FrozenUnit == nullptr
		|| FrozenUnit->HasStatus(FName(TEXT("Frozen")))
		|| FrozenUnit->HP != HPBefore
		|| FrozenUnit->GetCurrentArmor() != ArmorBefore
		|| Applied == nullptr
		|| Tick == nullptr
		|| FrozenDamage == nullptr
		|| Coordinator.GetState().HasPendingAttack()
		|| Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action)
	{
		Result.Reason = TEXT("status_authority_frozen_break_mismatch");
		return Result;
	}

	const FWBMatchObservation PlayerZeroObservation =
		Coordinator.BuildObservation(0);
	const FWBObservedZoneSummary* OpponentHand =
		PlayerZeroObservation.CardZones.PublicSummary.PlayerHands.FindByPredicate(
			[](const FWBObservedZoneSummary& Hand)
			{
				return Hand.OwnerPlayerId == 1;
			});
	if (OpponentHand == nullptr
		|| (OpponentHand->Visibility != EWBZoneObservationVisibility::Hidden
			&& OpponentHand->Visibility
				!= EWBZoneObservationVisibility::CountOnly)
		|| !OpponentHand->Cards.IsEmpty())
	{
		Result.Reason = TEXT("status_authority_opponent_hand_leak");
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
			? FString(TEXT("status_authority_archive_mismatch"))
			: Loaded.FailureCode;
		return Result;
	}

	FWBProductionMatchReplayRunRequest ReplayRequest;
	ReplayRequest.SerializedArchive = PersistedBytes;
	ReplayRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(ReplayRequest);
	if (!Replay.bValid
		|| Replay.bTerminal
		|| Replay.RecordsVerified != Recorder.GetArchive().Records.Num()
		|| Replay.FinalStateDigest != Coordinator.GetCurrentStateDigest()
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest()
		|| Replay.FinalGeneration != Coordinator.GetCoordinatorGeneration()
		|| Replay.FinalRevision != Coordinator.GetCoordinatorRevision())
	{
		Result.Reason = Replay.FailureCode.IsEmpty()
			? FString(TEXT("status_authority_fresh_replay_mismatch"))
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
		|| ReceiptJson.Contains(TEXT("status_authority_fixture_filler"))
		|| ReceiptJson.Contains(TEXT("Data/Replay")))
	{
		Result.Reason = TEXT("status_authority_receipt_privacy_mismatch");
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
	Result.Reason = TEXT("production_status_authority_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
