#include "WBProductionReactionWindowSmoke.h"

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
const FWBMatchLegalAction* FindReactionEquip(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 HeroUnitId)
{
	return Actions.FindByPredicate([HeroUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Equip
			&& Action.EquipRequest.SourceCardId == TEXT("hybrid_fixture_wand")
			&& Action.EquipRequest.TargetUnitId == HeroUnitId;
	});
}

const FWBMatchLegalAction* FindHybrid(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 HeroUnitId)
{
	return Actions.FindByPredicate([HeroUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& Action.bHybridHeroReplacement
			&& Action.HybridSummonPlan.SacrificedUnitId == HeroUnitId
			&& Action.HybridSummonPlan.WandPaymentSource
				== EWBHybridWandPaymentSource::SacrificedUnit;
	});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation;
	});
}

const FWBMatchLegalAction* FindReactionDiscard(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Discard;
	});
}

bool HasReactionTrace(const TArray<FWBTraceEvent>& Events, const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

bool HasOnlyResponseActions(const TArray<FWBMatchLegalAction>& Actions)
{
	return !Actions.IsEmpty() && !Actions.ContainsByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family != EWBMatchActionFamily::Activation
				&& !(Action.Family == EWBMatchActionFamily::CoreAction
					&& Action.CoreAction.Type == EWBActionType::PassResponse);
		});
}

bool IsOpponentHandHidden(
	const FWBMatchObservation& Observation,
	const int32 OpponentPlayerId)
{
	const FWBObservedZoneSummary* Hand =
		Observation.CardZones.PublicSummary.PlayerHands.FindByPredicate(
			[OpponentPlayerId](const FWBObservedZoneSummary& Zone)
			{
				return Zone.OwnerPlayerId == OpponentPlayerId;
			});
	return Hand != nullptr
		&& Hand->Cards.IsEmpty()
		&& (Hand->Visibility == EWBZoneObservationVisibility::Hidden
			|| Hand->Visibility == EWBZoneObservationVisibility::CountOnly);
}

bool SubmitReactionAndCapture(
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
}

bool WBProductionReactionWindowSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionReactionWindowSmoke"));
}

FString WBProductionReactionWindowSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionReactionWindowReceipt.json"));
}

FWBProductionReactionWindowSmokeResult WBProductionReactionWindowSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionReactionWindowSmokeResult Result;
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
	const FWBPlayerStateData* Player = Coordinator.GetState().GetPlayerById(0);
	if (Player == nullptr || Player->HeroUnitId < 0)
	{
		Result.Reason = TEXT("reaction_smoke_hero_missing");
		return Result;
	}
	const int32 OldHeroUnitId = Player->HeroUnitId;

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap),
		Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	FWBMatchOperationResult Operation;
	const FWBMatchLegalAction* Equip = FindReactionEquip(
		Started.NextLegalActions,
		OldHeroUnitId);
	if (Equip == nullptr
		|| !SubmitReactionAndCapture(
			Coordinator, Recorder, *Equip, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("reaction_smoke_equip_missing");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult HybridLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Hybrid = HybridLegal.bOk
		? FindHybrid(HybridLegal.Actions, OldHeroUnitId)
		: nullptr;
	if (Hybrid == nullptr
		|| !SubmitReactionAndCapture(
			Coordinator, Recorder, *Hybrid, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("reaction_smoke_hybrid_missing");
		return Result;
	}
	const FWBMatchObservation ResponseObservation = Coordinator.BuildObservation(1);
	const FWBMatchObservation NonPriorityObservation = Coordinator.BuildObservation(0);
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Response
		|| !Coordinator.GetState().HasOpenReactionWindow()
		|| Coordinator.GetState().ReactionWindow.Kind
			!= EWBReactionWindowKind::PostSummon
		|| Coordinator.GetState().PriorityPlayer != 1)
	{
		Result.Reason = FString::Printf(
			TEXT("reaction_smoke_window_state_mismatch_phase_%d_open_%d_kind_%d_priority_%d"),
			static_cast<int32>(Coordinator.GetMatchPhase()),
			Coordinator.GetState().HasOpenReactionWindow() ? 1 : 0,
			static_cast<int32>(Coordinator.GetState().ReactionWindow.Kind),
			Coordinator.GetState().PriorityPlayer);
		return Result;
	}
	if (ResponseObservation.MatchPhase != EWBMatchLoopPhase::Response
		|| ResponseObservation.PublicTurn.PriorityPlayerId != 1
		|| ResponseObservation.PublicTurn.ReactionWindowKind
			!= FName(TEXT("post_summon")))
	{
		Result.Reason = TEXT("reaction_smoke_public_turn_mismatch");
		return Result;
	}
	if (!HasOnlyResponseActions(ResponseObservation.LegalActions)
		|| !NonPriorityObservation.LegalActions.IsEmpty())
	{
		Result.Reason = TEXT("reaction_smoke_legal_actions_mismatch");
		return Result;
	}
	if (!IsOpponentHandHidden(ResponseObservation, 0))
	{
		Result.Reason = TEXT("reaction_smoke_hidden_hand_leak");
		return Result;
	}
	if (!HasReactionTrace(Operation.TraceEvents, TEXT("reaction_window_opened")))
	{
		Result.Reason = TEXT("reaction_smoke_open_trace_missing");
		return Result;
	}

	const FWBMatchLegalAction* React =
		FindActivation(ResponseObservation.LegalActions);
	if (React == nullptr)
	{
		Result.Reason = TEXT("reaction_smoke_react_missing");
		return Result;
	}
	Result.ReactionActionId = React->ActionId;
	if (!SubmitReactionAndCapture(
		Coordinator, Recorder, *React, Operation, Result.Reason))
	{
		return Result;
	}
	if (Coordinator.GetState().HasOpenReactionWindow()
		|| Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| !HasReactionTrace(Operation.TraceEvents, TEXT("reaction_resolved"))
		|| !HasReactionTrace(Operation.TraceEvents, TEXT("reaction_auto_passed"))
		|| !HasReactionTrace(Operation.TraceEvents, TEXT("reaction_window_closed")))
	{
		Result.Reason = TEXT("reaction_smoke_close_mismatch");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult Continued =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Discard = Continued.bOk
		? FindReactionDiscard(Continued.Actions)
		: nullptr;
	if (Discard == nullptr
		|| !SubmitReactionAndCapture(
			Coordinator, Recorder, *Discard, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("reaction_smoke_continuation_missing");
		return Result;
	}

	const FString ArchiveBytes =
		WBProductionMatchReplay::Serialize(Recorder.GetArchive());
	FString PersistedBytes;
	const FWBProductionMatchReplayPersistenceResult Loaded =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(),
			PersistedBytes);
	if (!Loaded.bOk || PersistedBytes != ArchiveBytes)
	{
		Result.Reason = Loaded.bOk
			? FString(TEXT("reaction_smoke_archive_mismatch"))
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
			? FString(TEXT("reaction_smoke_fresh_replay_mismatch"))
			: Replay.FailureCode;
		return Result;
	}

	const FString ReceiptJson =
		WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt());
	TSharedPtr<FJsonObject> ReceiptObject;
	if (!FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(ReceiptJson),
		ReceiptObject)
		|| !ReceiptObject.IsValid()
		|| ReceiptObject->Values.Num() != 8
		|| ReceiptJson.Contains(TEXT("state_digest"))
		|| ReceiptJson.Contains(TEXT("trace_digest")))
	{
		Result.Reason = TEXT("reaction_smoke_privacy_mismatch");
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
	Result.Reason = TEXT("production_reaction_window_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
