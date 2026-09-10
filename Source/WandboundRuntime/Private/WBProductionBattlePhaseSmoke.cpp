#include "WBProductionBattlePhaseSmoke.h"
#include "WBProductionMatchReplayRuntime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool WBProductionBattlePhaseSmoke::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("WandboundProductionBattlePhaseSmoke"));
}

FWBProductionBattlePhaseSmokeResult WBProductionBattlePhaseSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& Request, const FString& Scenario)
{
	FWBProductionBattlePhaseSmokeResult Result;
	if (Scenario != TEXT("normal") && Scenario != TEXT("counter")
		&& Scenario != TEXT("nested") && Scenario != TEXT("prevented")
		&& Scenario != TEXT("terminal") && Scenario != TEXT("terminal_counter"))
	{
		Result.Reason = TEXT("battle_smoke_scenario_unknown");
		return Result;
	}
	const auto Bootstrap = WBProductionRuntimeBootstrap::Build(Request);
	if (!Bootstrap.bOk) { Result.Reason = Bootstrap.Reason; return Result; }
	WBMatchCoordinator Coordinator;
	const auto Initialized = Coordinator.InitializeMatch(Bootstrap.InitializationRequest);
	if (!Initialized.bOk) { Result.Reason = Initialized.Reason; return Result; }
	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap), Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}
	auto Observe = [&]()
	{
		const int32 Priority = Coordinator.BuildObservation(0).PublicTurn.PriorityPlayerId;
		return Coordinator.BuildObservation(Priority);
	};
	auto Submit = [&](const EWBActionType Type, const FString& Effect = FString())
	{
		const auto Observation = Observe();
		const auto* Action = Observation.LegalActions.FindByPredicate([&](const FWBMatchLegalAction& A)
		{
			return Effect.IsEmpty()
				? A.Family == EWBMatchActionFamily::CoreAction && A.CoreAction.Type == Type
				: A.Family == EWBMatchActionFamily::Activation && A.ActivationCommand.Source.SourceEffectId == Effect;
		});
		if (!Action) { Result.Reason = TEXT("battle_smoke_action_missing"); return false; }
		const auto Applied = Coordinator.SubmitActionId(Action->PlayerId, Action->ActionId);
		if (!Applied.bOk) { Result.Reason = Applied.Reason; return false; }
		Recorder.CaptureCommittedActions(Coordinator);
		if (!Recorder.IsAvailable()) { Result.Reason = Recorder.GetReceipt().FailureCode; return false; }
		return true;
	};
	if (!Submit(EWBActionType::EndTurn) || !Submit(EWBActionType::EndTurn)
		|| !Submit(EWBActionType::Attack)) return Result;
	const bool bRespond = Scenario == TEXT("nested") || Scenario == TEXT("prevented");
	if (bRespond)
	{
		const auto A = Coordinator.BuildObservation(0).PublicBattle;
		const auto B = Coordinator.BuildObservation(1).PublicBattle;
		if (!A.bActive || !B.bActive || A.DeclaringPlayerId != 0
			|| A.OriginalAttackerUnitId != B.OriginalAttackerUnitId
			|| A.OriginalDefenderUnitId != B.OriginalDefenderUnitId
			|| Observe().PublicTurn.PriorityPlayerId != 1)
		{
			Result.Reason = TEXT("battle_smoke_public_summary_mismatch");
			return Result;
		}
		if (!Submit(EWBActionType::PassResponse, TEXT("suspended_attack_effect_a"))) return Result;
		if (Scenario == TEXT("nested"))
		{
			if (!Observe().PublicBattle.bActive
				|| !Submit(EWBActionType::PassResponse, TEXT("suspended_attack_effect_b"))
				|| !Observe().PublicBattle.bActive
				|| !Submit(EWBActionType::PassResponse, TEXT("suspended_attack_effect_c"))) return Result;
		}
	}
	for (int32 Guard = 0; Guard < 32 && Observe().MatchPhase == EWBMatchLoopPhase::Response; ++Guard)
		if (!Submit(EWBActionType::PassResponse)) return Result;
	const auto Final = Observe();
	if (Final.PublicBattle.bActive
		|| (Scenario.StartsWith(TEXT("terminal"))
			? !Final.PublicTurn.bGameOver : Final.MatchPhase != EWBMatchLoopPhase::Action))
	{
		Result.Reason = TEXT("battle_smoke_final_phase_mismatch");
		return Result;
	}
	int32 Starts = 0, Ends = 0, Counters = 0;
	for (const auto& E : Coordinator.GetTraceLog())
	{
		if (E.Kind == FName(TEXT("battle_phase_started"))) ++Starts;
		if (E.Kind == FName(TEXT("battle_phase_ended"))) ++Ends;
		if (E.Kind == FName(TEXT("counter_started"))) ++Counters;
		if ((E.Kind == FName(TEXT("battle_phase_started")) || E.Kind == FName(TEXT("battle_phase_ended")))
			&& (!E.ActionId.IsEmpty() || !E.AttackContinuationId.IsEmpty() || !E.CardInstanceId.IsEmpty()))
		{
			Result.Reason = TEXT("battle_smoke_boundary_privacy_failure");
			return Result;
		}
	}
	if (Starts != 1 || Ends != 1 || Counters != (Scenario.Contains(TEXT("counter")) ? 1 : 0))
	{
		Result.Reason = TEXT("battle_smoke_boundary_count_mismatch");
		return Result;
	}
	const FString Bytes = WBProductionMatchReplay::Serialize(Recorder.GetArchive());
	FString Persisted;
	const auto Loaded = WBProductionMatchReplayPersistence::Load(Recorder.GetArchivePathForServer(), Persisted);
	if (!Loaded.bOk || Bytes != Persisted)
	{
		Result.Reason = TEXT("battle_smoke_archive_persistence_mismatch");
		return Result;
	}
	FWBProductionMatchReplayRunRequest ReplayRequest;
	ReplayRequest.BootstrapRequest = Request;
	ReplayRequest.SerializedArchive = Persisted;
	const auto Replay = FWBProductionMatchReplayRunner::Run(ReplayRequest);
	if (!Replay.bValid || Replay.FinalStateDigest != Coordinator.GetCurrentStateDigest()
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest()
		|| Replay.FinalGeneration != Coordinator.GetCoordinatorGeneration()
		|| Replay.FinalRevision != Coordinator.GetCoordinatorRevision())
	{
		Result.Reason = Replay.FailureCode.IsEmpty() ? TEXT("battle_smoke_replay_mismatch") : Replay.FailureCode;
		return Result;
	}
	const FString Receipt = WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt());
	TSharedPtr<FJsonObject> Json;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Receipt), Json)
		|| !Json.IsValid() || Json->Values.Num() != 8
		|| Receipt.Contains(TEXT("battle_id")) || Receipt.Contains(TEXT("state_digest"))
		|| Receipt.Contains(TEXT("trace_digest")) || Receipt.Contains(TEXT("continuation_id")))
	{
		Result.Reason = TEXT("battle_smoke_receipt_privacy_failure");
		return Result;
	}
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(),
		TEXT("SmokeTest"), TEXT("BattlePhase_") + Scenario + TEXT("_Receipt.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Receipt, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("battle_smoke_receipt_write_failed");
		return Result;
	}
	Result.bOk = true;
	Result.Reason = TEXT("production_battle_phase_verified");
	Result.FinalStateDigest = Replay.FinalStateDigest;
	Result.FinalTraceDigest = Replay.FinalTraceDigest;
	return Result;
}
