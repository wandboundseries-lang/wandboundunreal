#include "WBProductionCSNBodyDoubleSmoke.h"

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
const FWBMatchLegalAction* FindBodyDoubleCore(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindBodyDoubleSummon(
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

const FWBMatchLegalAction* FindBodyDoubleArmorSetup(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& SourceCardId,
	const int32 TargetUnitId)
{
	return Actions.FindByPredicate(
		[&SourceCardId, TargetUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceCardId == SourceCardId
				&& Action.ActivationCommand.Source.SourceEffectId
					== TEXT("fixture_set_armor")
				&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId
					== TargetUnitId;
		});
}

const FWBMatchLegalAction* FindBodyDoubleMove(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 SourceUnitId,
	const FWBTile TargetTile)
{
	return Actions.FindByPredicate(
		[SourceUnitId, TargetTile](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == EWBActionType::Move
				&& Action.CoreAction.SourceUnitId == SourceUnitId
				&& Action.CoreAction.ToTile == TargetTile;
		});
}

bool SubmitBodyDouble(
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

bool ResolveBodyDoubleTurnStart(
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
				? FString(TEXT("csn_body_double_turn_start_missing"))
				: Legal.Reason;
			return false;
		}
		FWBMatchOperationResult Operation;
		if (!SubmitBodyDouble(
			Coordinator, Recorder, Legal.Actions[0], Operation, OutReason))
		{
			return false;
		}
	}
	return true;
}

bool PassBodyDoubleResponse(
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
		const FWBMatchLegalAction* Pass = FindBodyDoubleCore(
			Observation.LegalActions, EWBActionType::PassResponse);
		if (Pass == nullptr)
		{
			OutReason = TEXT("csn_body_double_pass_missing");
			return false;
		}
		FWBMatchOperationResult Operation;
		if (!SubmitBodyDouble(
			Coordinator, Recorder, *Pass, Operation, OutReason))
		{
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response)
	{
		OutReason = TEXT("csn_body_double_response_guard_exceeded");
		return false;
	}
	return true;
}

bool EndBodyDoubleTurn(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = Legal.bOk
		? FindBodyDoubleCore(Legal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr)
	{
		OutReason = Legal.bOk
			? FString(TEXT("csn_body_double_end_turn_missing"))
			: Legal.Reason;
		return false;
	}
	FWBMatchOperationResult Operation;
	return SubmitBodyDouble(
		Coordinator, Recorder, *EndTurn, Operation, OutReason)
		&& ResolveBodyDoubleTurnStart(Coordinator, Recorder, OutReason);
}

int32 CountBodyDoubleAccepted(
	const WBMatchCoordinator& Coordinator,
	const FString& Prefix)
{
	return Coordinator.GetCommittedActionRecords().FilterByPredicate(
		[&Prefix](const FWBMatchCommittedActionRecord& Record)
		{
			return Record.ChosenActionId.StartsWith(Prefix);
		}).Num();
}
}

bool WBProductionCSNBodyDoubleSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionCSNBodyDoubleSmoke"));
}

FString WBProductionCSNBodyDoubleSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionCSNBodyDoubleReceipt.json"));
}

FWBProductionCSNBodyDoubleSmokeResult WBProductionCSNBodyDoubleSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionCSNBodyDoubleSmokeResult Result;
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

	FWBMatchOperationResult Operation;
	const FWBMatchLegalAction* AttackerSummon = FindBodyDoubleSummon(
		Started.NextLegalActions,
		TEXT("body_double_fixture_attacker"),
		FWBTile(4, 7));
	if (AttackerSummon == nullptr
		|| !SubmitBodyDouble(
			Coordinator, Recorder, *AttackerSummon, Operation, Result.Reason)
		|| !PassBodyDoubleResponse(Coordinator, Recorder, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_body_double_attacker_summon_missing");
		}
		return Result;
	}
	const FWBPlayerStateData* SetupPlayerOne =
		Coordinator.GetState().GetPlayerById(1);
	const FWBMatchLegalActionGenerationResult ArmorHeroLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* ArmorHero = SetupPlayerOne != nullptr
		? FindBodyDoubleArmorSetup(
			ArmorHeroLegal.Actions,
			TEXT("body_double_fixture_hero_a"),
			SetupPlayerOne->HeroUnitId)
		: nullptr;
	if (ArmorHero == nullptr
		|| !SubmitBodyDouble(
			Coordinator, Recorder, *ArmorHero, Operation, Result.Reason)
		|| !PassBodyDoubleResponse(Coordinator, Recorder, Result.Reason)
		|| !EndBodyDoubleTurn(Coordinator, Recorder, Result.Reason))
	{
		Result.Reason = Result.Reason.IsEmpty()
			? FString(TEXT("csn_body_double_hero_armor_setup_missing"))
			: Result.Reason;
		return Result;
	}

	const FWBMatchLegalActionGenerationResult PlayerOneLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* CSNSummon = PlayerOneLegal.bOk
		? FindBodyDoubleSummon(
			PlayerOneLegal.Actions,
			TEXT("body_double_fixture_csn"),
			FWBTile(5, 0))
		: nullptr;
	if (CSNSummon == nullptr
		|| !SubmitBodyDouble(
			Coordinator, Recorder, *CSNSummon, Operation, Result.Reason)
		|| !PassBodyDoubleResponse(Coordinator, Recorder, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_body_double_csn_summon_missing");
		}
		return Result;
	}

	const FWBPlayerStateData* PlayerOne = Coordinator.GetState().GetPlayerById(1);
	const FWBUnitState* Attacker = Coordinator.GetState().Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("body_double_fixture_attacker");
		});
	const FWBUnitState* Recipient = Coordinator.GetState().Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("body_double_fixture_csn");
		});
	const FWBUnitState* Hero = PlayerOne != nullptr
		? Coordinator.GetState().GetUnitById(PlayerOne->HeroUnitId)
		: nullptr;
	if (Attacker == nullptr || Recipient == nullptr || Hero == nullptr)
	{
		Result.Reason = TEXT("csn_body_double_participant_missing");
		return Result;
	}
	const FWBMatchLegalActionGenerationResult ArmorCSNLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* ArmorCSN = FindBodyDoubleArmorSetup(
		ArmorCSNLegal.Actions, TEXT("body_double_fixture_csn"), Recipient->UnitId);
	if (ArmorCSN == nullptr
		|| !SubmitBodyDouble(
			Coordinator, Recorder, *ArmorCSN, Operation, Result.Reason)
		|| !PassBodyDoubleResponse(Coordinator, Recorder, Result.Reason))
	{
		Result.Reason = Result.Reason.IsEmpty()
			? FString(TEXT("csn_body_double_substitute_armor_setup_missing"))
			: Result.Reason;
		return Result;
	}
	const FWBMatchLegalActionGenerationResult MoveLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* MoveRemote = FindBodyDoubleMove(
		MoveLegal.Actions, Recipient->UnitId, FWBTile(5, 1));
	if (MoveRemote == nullptr
		|| !SubmitBodyDouble(
			Coordinator, Recorder, *MoveRemote, Operation, Result.Reason)
		|| !PassBodyDoubleResponse(Coordinator, Recorder, Result.Reason)
		|| !EndBodyDoubleTurn(Coordinator, Recorder, Result.Reason))
	{
		Result.Reason = Result.Reason.IsEmpty()
			? FString(TEXT("csn_body_double_remote_move_missing"))
			: Result.Reason;
		return Result;
	}
	Attacker = Coordinator.GetState().GetUnitById(Attacker->UnitId);
	Recipient = Coordinator.GetState().GetUnitById(Recipient->UnitId);
	Hero = Coordinator.GetState().GetUnitById(Hero->UnitId);
	const int32 AttackerUnitId = Attacker->UnitId;
	const int32 RecipientUnitId = Recipient->UnitId;
	const int32 HeroUnitId = Hero->UnitId;
	const int32 HeroHPBefore = Hero->HP;
	const int32 HeroArmorBefore = Hero->GetCurrentArmor();
	const int32 RecipientHPBefore = Recipient->HP;
	const int32 RecipientArmorBefore = Recipient->GetCurrentArmor();

	const FWBMatchLegalActionGenerationResult AttackLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = AttackLegal.bOk
		? AttackLegal.Actions.FindByPredicate(
			[AttackerUnitId, HeroUnitId](const FWBMatchLegalAction& Action)
			{
				return Action.Family == EWBMatchActionFamily::CoreAction
					&& Action.CoreAction.Type == EWBActionType::Attack
					&& Action.CoreAction.SourceUnitId == AttackerUnitId
					&& Action.CoreAction.TargetUnitId == HeroUnitId;
			})
		: nullptr;
	if (Attack == nullptr
		|| !SubmitBodyDouble(
			Coordinator, Recorder, *Attack, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_body_double_attack_missing");
		}
		return Result;
	}
	const FWBPendingAttackState Declared = Coordinator.GetState().PendingAttack;
	const FWBMatchObservation Response = Coordinator.BuildObservation(1);
	const FWBMatchLegalAction* BodyDouble = Response.LegalActions.FindByPredicate(
		[RecipientUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceCardId
					== TEXT("effect_react_csn_body_double")
				&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId
					== RecipientUnitId;
		});
	if (BodyDouble == nullptr
		|| !SubmitBodyDouble(
			Coordinator, Recorder, *BodyDouble, Operation, Result.Reason)
		|| !PassBodyDoubleResponse(Coordinator, Recorder, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_body_double_activation_missing");
		}
		return Result;
	}

	Hero = Coordinator.GetState().GetUnitById(HeroUnitId);
	Recipient = Coordinator.GetState().GetUnitById(RecipientUnitId);
	const TArray<FWBTraceEvent>& Trace = Coordinator.GetTraceLog();
	const FWBTraceEvent* Substituted = Trace.FindByPredicate(
		[HeroUnitId, RecipientUnitId](const FWBTraceEvent& Event)
		{
			return Event.Kind
					== FName(TEXT("attack_damage_substituted"))
				&& Event.AttackDefenderUnitId == HeroUnitId
				&& Event.DamageRecipientUnitId == RecipientUnitId;
		});
	const FWBTraceEvent* Damage = Trace.FindByPredicate(
		[HeroUnitId, RecipientUnitId](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("attack_damage_resolved"))
				&& Event.TargetUnitId == RecipientUnitId
				&& Event.AttackDefenderUnitId == HeroUnitId;
		});
	const FWBTraceEvent* CostPaid = Trace.FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("card_activation_cost_paid"));
		});
	const FWBTraceEvent* Discarded = Trace.FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind
					== FName(TEXT("card_discarded_for_pending_effect"))
				&& Event.CardId == TEXT("effect_react_csn_body_double");
		});
	if (Declared.DefenderUnitId != HeroUnitId
		|| Declared.OriginalDefenderUnitId != HeroUnitId
		|| Declared.ContinuationId.IsEmpty()
		|| Substituted == nullptr
		|| Damage == nullptr
		|| Hero == nullptr
		|| Hero->HP != HeroHPBefore
		|| HeroArmorBefore != 2
		|| Hero->GetCurrentArmor() != 0
		|| CostPaid == nullptr
		|| CostPaid->SourceUnitId != HeroUnitId
		|| CostPaid->CostAmount != 2
		|| Discarded == nullptr
		|| Recipient == nullptr
		|| Recipient->HP != RecipientHPBefore - 3
		|| RecipientArmorBefore != 7
		|| Recipient->GetCurrentArmor() != 7
		|| CountBodyDoubleAccepted(Coordinator, TEXT("attack:")) != 1
		|| Coordinator.GetCommittedActionRecords().FilterByPredicate(
			[](const FWBMatchCommittedActionRecord& Record)
			{
				return Record.ActionFamily == TEXT("activate");
			}).Num() != 3)
	{
		Result.Reason = FString::Printf(
			TEXT("csn_body_double_resolution_mismatch:defender=%d:original=%d:sub=%d:damage=%d:hero_hp=%d/%d:hero_rl=%d:cost=%d:cost_source=%d:recipient_hp=%d/%d:attacks=%d:activations=%d"),
			Declared.DefenderUnitId,
			Declared.OriginalDefenderUnitId,
			Substituted != nullptr ? 1 : 0,
			Damage != nullptr ? 1 : 0,
			Hero != nullptr ? Hero->HP : -999,
			HeroHPBefore,
			Hero != nullptr ? Hero->RLUsed : -999,
			CostPaid != nullptr ? CostPaid->CostAmount : -999,
			CostPaid != nullptr ? CostPaid->SourceUnitId : -999,
			Recipient != nullptr ? Recipient->HP : -999,
			RecipientHPBefore - 3,
			CountBodyDoubleAccepted(Coordinator, TEXT("attack:")),
			Coordinator.GetCommittedActionRecords().FilterByPredicate(
				[](const FWBMatchCommittedActionRecord& Record)
				{
					return Record.ActionFamily == TEXT("activate");
				}).Num());
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
			? FString(TEXT("csn_body_double_archive_mismatch"))
			: Loaded.FailureCode;
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
			? FString(TEXT("csn_body_double_fresh_replay_mismatch"))
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
		|| ReceiptJson.Contains(TEXT("effect_react_csn_body_double")))
	{
		Result.Reason = TEXT("csn_body_double_receipt_privacy_mismatch");
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
	Result.Reason = TEXT("production_csn_body_double_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
