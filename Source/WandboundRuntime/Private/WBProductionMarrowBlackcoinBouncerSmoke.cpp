#include "WBProductionMarrowBlackcoinBouncerSmoke.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBPreDamageAttackTrigger.h"
#include "WBProductionMatchReplayRuntime.h"

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

const FWBMatchLegalAction* FindAttack(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 SourceUnitId,
	const int32 TargetUnitId)
{
	return Actions.FindByPredicate(
		[SourceUnitId, TargetUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == EWBActionType::Attack
				&& Action.CoreAction.SourceUnitId == SourceUnitId
				&& Action.CoreAction.TargetUnitId == TargetUnitId;
		});
}

const FWBMatchLegalAction* FindMove(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 SourceUnitId,
	const FWBTile TargetTile)
{
	return Actions.FindByPredicate([SourceUnitId, TargetTile](
		const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Move
			&& Action.CoreAction.SourceUnitId == SourceUnitId
			&& Action.CoreAction.ToTile == TargetTile;
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
				? FString(TEXT("blackcoin_turn_start_decision_missing"))
				: Legal.Reason;
			return false;
		}
		if (!Submit(Coordinator, Recorder, Legal.Actions[0], OutReason))
		{
			return false;
		}
	}
	if (Coordinator.HasPendingTurnStartDecision())
	{
		OutReason = TEXT("blackcoin_turn_start_guard_exceeded");
		return false;
	}
	return true;
}

bool EndTurn(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Action = Legal.bOk
		? FindCore(Legal.Actions, EWBActionType::EndTurn) : nullptr;
	if (Action == nullptr)
	{
		OutReason = Legal.bOk
			? FString(TEXT("blackcoin_end_turn_missing")) : Legal.Reason;
		return false;
	}
	return Submit(Coordinator, Recorder, *Action, OutReason)
		&& ResolveTurnStart(Coordinator, Recorder, OutReason);
}

int32 CountCoinTraces(const TArray<FWBTraceEvent>& Trace)
{
	return Trace.FilterByPredicate([](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(TEXT("random_branch_resolved"))
			&& Event.ActionId == TEXT("when_attacked_coin_reflect_or_bonus");
	}).Num();
}

const FWBTraceEvent* FindCoinTrace(
	const TArray<FWBTraceEvent>& Trace,
	const FName Outcome,
	const int32 SourceUnitId)
{
	return Trace.FindByPredicate([Outcome, SourceUnitId](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(TEXT("random_branch_resolved"))
			&& Event.ActionId == TEXT("when_attacked_coin_reflect_or_bonus")
			&& Event.SourceUnitId == SourceUnitId
			&& Event.RandomOutcome == Outcome;
	});
}
}

bool WBProductionMarrowBlackcoinBouncerSmoke::IsRequested(
	const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionMarrowBlackcoinBouncerSmoke"));
}

FString WBProductionMarrowBlackcoinBouncerSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionMarrowBlackcoinBouncerReceipt.json"));
}

FWBProductionMarrowBlackcoinBouncerSmokeResult
WBProductionMarrowBlackcoinBouncerSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionMarrowBlackcoinBouncerSmokeResult Result;
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

	const FWBMatchLegalAction* FixtureSummon = FindSummon(
		Started.NextLegalActions,
		TEXT("blackcoin_smoke_attacker"),
		FWBTile(4, 7));
	if (FixtureSummon == nullptr
		|| !Submit(Coordinator, Recorder, *FixtureSummon, Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("blackcoin_fixture_attacker_summon_missing");
		}
		return Result;
	}

	const FWBMatchLegalActionGenerationResult PlayerOneLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* BlackcoinSummon = PlayerOneLegal.bOk
		? FindSummon(
			PlayerOneLegal.Actions,
			TEXT("char_marrow_blackcoin_bouncer"),
			FWBTile(4, 1)) : nullptr;
	if (BlackcoinSummon == nullptr
		|| !Submit(Coordinator, Recorder, *BlackcoinSummon, Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("blackcoin_summon_missing");
		}
		return Result;
	}

	const FWBPlayerStateData* PlayerZero =
		Coordinator.GetState().GetPlayerById(0);
	const FWBUnitState* Blackcoin =
		Coordinator.GetState().Units.FindByPredicate([](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("char_marrow_blackcoin_bouncer")
				&& Unit.IsUnitOnBoard();
		});
	const FWBUnitState* FixtureAttacker =
		Coordinator.GetState().Units.FindByPredicate([](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("blackcoin_smoke_attacker")
				&& Unit.IsUnitOnBoard();
		});
	const FWBUnitState* Hero = PlayerZero != nullptr
		? Coordinator.GetState().GetUnitById(PlayerZero->HeroUnitId) : nullptr;
	if (Blackcoin == nullptr || FixtureAttacker == nullptr || Hero == nullptr)
	{
		Result.Reason = TEXT("blackcoin_smoke_participant_missing");
		return Result;
	}
	const int32 BlackcoinUnitId = Blackcoin->UnitId;
	const int32 FixtureAttackerUnitId = FixtureAttacker->UnitId;
	const int32 HeroUnitId = Hero->UnitId;
	const int32 InitialBlackcoinHP = Blackcoin->HP;
	const int32 InitialFixtureAttackerHP = FixtureAttacker->HP;

	const FWBMatchLegalActionGenerationResult HeadsLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* HeadsAttack = HeadsLegal.bOk
		? FindAttack(
			HeadsLegal.Actions, FixtureAttackerUnitId, BlackcoinUnitId) : nullptr;
	if (HeadsAttack == nullptr
		|| !Submit(Coordinator, Recorder, *HeadsAttack, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("blackcoin_heads_attack_missing");
		return Result;
	}
	FixtureAttacker = Coordinator.GetState().GetUnitById(FixtureAttackerUnitId);
	Blackcoin = Coordinator.GetState().GetUnitById(BlackcoinUnitId);
	const int32 CoinCountAfterHeads = CountCoinTraces(Coordinator.GetTraceLog());
	const FString UsageKey = WBPreDamageAttackTrigger::BuildUsageKey(
		BlackcoinUnitId,
		TEXT("when_attacked_coin_reflect_or_bonus"),
		Coordinator.GetState().TurnNumber);
	if (FixtureAttacker == nullptr || Blackcoin == nullptr
		|| FixtureAttacker->HP != InitialFixtureAttackerHP - 4
		|| Blackcoin->HP != InitialBlackcoinHP
		|| CoinCountAfterHeads != 1
		|| FindCoinTrace(
			Coordinator.GetTraceLog(), FName(TEXT("heads")), BlackcoinUnitId) == nullptr
		|| !Coordinator.GetState().HasActivationUsageKeyThisTurn(1, UsageKey))
	{
		Result.Reason = TEXT("blackcoin_heads_resolution_mismatch");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult MoveLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* MoveAside = MoveLegal.bOk
		? FindMove(MoveLegal.Actions, FixtureAttackerUnitId, FWBTile(3, 7))
		: nullptr;
	if (MoveAside == nullptr
		|| !Submit(Coordinator, Recorder, *MoveAside, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("blackcoin_attacker_move_missing");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult RepeatLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* RepeatAttack = RepeatLegal.bOk
		? FindAttack(
			RepeatLegal.Actions, HeroUnitId, BlackcoinUnitId) : nullptr;
	if (RepeatAttack == nullptr
		|| !Submit(Coordinator, Recorder, *RepeatAttack, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("blackcoin_repeat_attack_missing");
		return Result;
	}
	Blackcoin = Coordinator.GetState().GetUnitById(BlackcoinUnitId);
	if (Blackcoin == nullptr || Blackcoin->HP != InitialBlackcoinHP - 4
		|| CountCoinTraces(Coordinator.GetTraceLog()) != CoinCountAfterHeads)
	{
		Result.Reason = TEXT("blackcoin_same_turn_usage_mismatch");
		return Result;
	}

	if (!EndTurn(Coordinator, Recorder, Result.Reason)
		|| !EndTurn(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}
	const int32 BeforeTailsHP =
		Coordinator.GetState().GetUnitById(BlackcoinUnitId)->HP;
	const FWBMatchLegalActionGenerationResult TailsLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* TailsAttack = TailsLegal.bOk
		? FindAttack(TailsLegal.Actions, HeroUnitId, BlackcoinUnitId) : nullptr;
	if (TailsAttack == nullptr
		|| !Submit(Coordinator, Recorder, *TailsAttack, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("blackcoin_tails_attack_missing");
		return Result;
	}
	Blackcoin = Coordinator.GetState().GetUnitById(BlackcoinUnitId);
	if (Blackcoin == nullptr || Blackcoin->HP != BeforeTailsHP - 7
		|| CountCoinTraces(Coordinator.GetTraceLog()) != 2
		|| FindCoinTrace(
			Coordinator.GetTraceLog(), FName(TEXT("tails")), BlackcoinUnitId) == nullptr)
	{
		Result.Reason = TEXT("blackcoin_tails_resolution_mismatch");
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
			? TEXT("blackcoin_archive_mismatch") : Loaded.FailureCode;
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
			? TEXT("blackcoin_fresh_replay_mismatch") : Replay.FailureCode;
		return Result;
	}

	const FString ReceiptJson = WBProductionMatchReplay::SerializeReceipt(
		Recorder.GetReceipt());
	TSharedPtr<FJsonObject> ReceiptObject;
	if (!FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(ReceiptJson), ReceiptObject)
		|| !ReceiptObject.IsValid() || ReceiptObject->Values.Num() != 8
		|| ReceiptJson.Contains(TEXT("random_state"))
		|| ReceiptJson.Contains(TEXT("random_seed"))
		|| ReceiptJson.Contains(TEXT("state_digest"))
		|| ReceiptJson.Contains(TEXT("trace_digest")))
	{
		Result.Reason = TEXT("blackcoin_receipt_privacy_mismatch");
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
	Result.Reason = TEXT("production_marrow_blackcoin_bouncer_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalGeneration = Coordinator.GetCoordinatorGeneration();
	Result.FinalRevision = Coordinator.GetCoordinatorRevision();
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	Result.SerializedArchive = PersistedBytes;
	Result.SerializedReceipt = ReceiptJson;
	return Result;
}
