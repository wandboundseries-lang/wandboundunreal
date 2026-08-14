#include "WBProductionAfterDamageTriggerSmoke.h"

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
const FWBMatchLegalAction* FindAfterDamageAttack(
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

const FWBMatchLegalAction* FindCoreAction(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

bool SubmitAfterDamageAndCapture(
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

int32 FindTraceIndex(
	const TArray<FWBTraceEvent>& Trace,
	const FName Kind,
	const FName ReactionKind = NAME_None)
{
	for (int32 Index = 0; Index < Trace.Num(); ++Index)
	{
		if (Trace[Index].Kind == Kind
			&& (ReactionKind.IsNone()
				|| Trace[Index].ReactionWindowKind == ReactionKind))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
}

bool WBProductionAfterDamageTriggerSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionAfterDamageTriggerSmoke"));
}

FString WBProductionAfterDamageTriggerSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionAfterDamageTriggerReceipt.json"));
}

FWBProductionAfterDamageTriggerSmokeResult
WBProductionAfterDamageTriggerSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionAfterDamageTriggerSmokeResult Result;
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
	const FWBUnitState* Attacker = PlayerZero != nullptr
		? Coordinator.GetState().GetUnitById(PlayerZero->HeroUnitId)
		: nullptr;
	const FWBUnitState* Defender = PlayerOne != nullptr
		? Coordinator.GetState().GetUnitById(PlayerOne->HeroUnitId)
		: nullptr;
	if (Attacker == nullptr || Defender == nullptr)
	{
		Result.Reason = TEXT("after_damage_smoke_hero_missing");
		return Result;
	}

	const int32 AttackerUnitId = Attacker->UnitId;
	const int32 DefenderUnitId = Defender->UnitId;
	const int32 AttackerHPBefore = Attacker->HP;
	const int32 DefenderHPBefore = Defender->HP;
	for (int32 TurnTransition = 0; TurnTransition < 2; ++TurnTransition)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* EndTurn = Legal.bOk
			? FindCoreAction(Legal.Actions, EWBActionType::EndTurn)
			: nullptr;
		if (EndTurn == nullptr
			|| !SubmitAfterDamageAndCapture(
				Coordinator, Recorder, *EndTurn, Result.Reason))
		{
			if (Result.Reason.IsEmpty())
			{
				Result.Reason = TEXT("after_damage_smoke_turn_advance_failed");
			}
			return Result;
		}
	}
	const FWBMatchLegalActionGenerationResult AttackLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = FindAfterDamageAttack(
		AttackLegal.Actions, AttackerUnitId, DefenderUnitId);
	if (Attack == nullptr)
	{
		TArray<FString> LegalIds;
		for (const FWBMatchLegalAction& Action : AttackLegal.Actions)
		{
			LegalIds.Add(Action.ActionId);
		}
		Result.Reason = FString::Printf(
			TEXT("after_damage_smoke_attack_missing:%s"),
			*FString::Join(LegalIds, TEXT(",")));
		return Result;
	}

	if (!SubmitAfterDamageAndCapture(Coordinator, Recorder, *Attack, Result.Reason))
	{
		return Result;
	}

	Attacker = Coordinator.GetState().GetUnitById(AttackerUnitId);
	Defender = Coordinator.GetState().GetUnitById(DefenderUnitId);
	const TArray<FWBTraceEvent>& Trace = Coordinator.GetTraceLog();
	const int32 DamageIndex = FindTraceIndex(
		Trace, FName(TEXT("attack_damage_applied")));
	const int32 CollectedIndex = FindTraceIndex(
		Trace, FName(TEXT("after_damage_trigger_collected")));
	const int32 TriggerIndex = FindTraceIndex(
		Trace, FName(TEXT("after_damage_trigger_resolved")));
	const int32 PostHitIndex = FindTraceIndex(
		Trace, FName(TEXT("attack_post_hit_closed")));
	const FWBTraceEvent* Trigger = TriggerIndex != INDEX_NONE
		? &Trace[TriggerIndex] : nullptr;
	const int32 TriggerCount = Trace.FilterByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("after_damage_trigger_resolved"));
		}).Num();
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| Coordinator.GetState().HasPendingAttack()
		|| Coordinator.GetState().HasOpenReactionWindow()
		|| Attacker == nullptr
		|| Defender == nullptr
		|| Attacker->HP != AttackerHPBefore - 1
		|| Defender->HP != DefenderHPBefore - 3
		|| !Attacker->HasStatus(FName(TEXT("Rooted")))
		|| DamageIndex == INDEX_NONE
		|| CollectedIndex <= DamageIndex
		|| TriggerIndex <= CollectedIndex
		|| PostHitIndex <= TriggerIndex
		|| Trigger == nullptr
		|| Trigger->SourceUnitId != DefenderUnitId
		|| Trigger->TargetUnitId != AttackerUnitId
		|| Trigger->HitUnitId != DefenderUnitId
		|| Trigger->DamageRecipientUnitId != DefenderUnitId
		|| Trigger->ActualHPDamageAmount != 3
		|| TriggerCount != 1
		|| Coordinator.GetCommittedActionRecords().Num() != 3)
	{
		TArray<FString> TraceKinds;
		for (const FWBTraceEvent& Event : Trace)
		{
			TraceKinds.Add(FString::Printf(
				TEXT("%s/%s"),
				*Event.Kind.ToString(),
				*Event.ReactionWindowKind.ToString()));
		}
		Result.Reason = FString::Printf(
			TEXT("after_damage_smoke_resolution_mismatch:phase=%d:pending=%d:reaction=%d:attacker_hp=%d/%d:defender_hp=%d/%d:rooted=%d:damage=%d:collected=%d:trigger=%d:posthit=%d:trigger_count=%d:records=%d:trace=%s"),
			static_cast<int32>(Coordinator.GetMatchPhase()),
			Coordinator.GetState().HasPendingAttack() ? 1 : 0,
			Coordinator.GetState().HasOpenReactionWindow() ? 1 : 0,
			Attacker != nullptr ? Attacker->HP : -999,
			AttackerHPBefore - 1,
			Defender != nullptr ? Defender->HP : -999,
			DefenderHPBefore - 3,
			Attacker != nullptr
				&& Attacker->HasStatus(FName(TEXT("Rooted"))) ? 1 : 0,
			DamageIndex,
			CollectedIndex,
			TriggerIndex,
			PostHitIndex,
			TriggerCount,
			Coordinator.GetCommittedActionRecords().Num(),
			*FString::Join(TraceKinds, TEXT(",")));
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
		Result.Reason = TEXT("after_damage_smoke_opponent_hand_leak");
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
			? FString(TEXT("after_damage_smoke_archive_mismatch"))
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
			? FString(TEXT("after_damage_smoke_fresh_replay_mismatch"))
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
		|| ReceiptJson.Contains(TEXT("after_damage_fixture_filler"))
		|| ReceiptJson.Contains(TEXT("Data/Replay")))
	{
		Result.Reason = TEXT("after_damage_smoke_receipt_privacy_mismatch");
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
	Result.Reason = TEXT("production_after_damage_trigger_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
