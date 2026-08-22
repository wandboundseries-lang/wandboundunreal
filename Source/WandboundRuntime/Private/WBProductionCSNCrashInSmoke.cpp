#include "WBProductionCSNCrashInSmoke.h"

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
const FWBMatchLegalAction* FindCrashInCore(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindCrashInSummon(
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

const FWBMatchLegalAction* FindCrashInEquip(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 TargetUnitId)
{
	return Actions.FindByPredicate([TargetUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Equip
			&& Action.EquipRequest.SourceCardId == TEXT("wand_equip_switch")
			&& Action.EquipRequest.TargetUnitId == TargetUnitId;
	});
}

const FWBMatchLegalAction* FindCrashInAttack(
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

const FWBMatchLegalAction* FindCrashInActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 DefenderUnitId,
	const FString& ReplacementCardId)
{
	return Actions.FindByPredicate(
		[DefenderUnitId, &ReplacementCardId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceCardId
					== TEXT("effect_react_csn_crash_in")
				&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId
					== DefenderUnitId
				&& Action.ActivationCommand.EffectRequest
					.AuxiliaryCardSelection.CardId == ReplacementCardId;
		});
}

const FWBMatchLegalAction* FindCrashInActivationByCard(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& CardId)
{
	return Actions.FindByPredicate([&CardId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceCardId == CardId;
	});
}

bool SubmitCrashIn(
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

bool ResolveCrashInTurnStart(
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
				? FString(TEXT("csn_crash_in_turn_start_choice_missing"))
				: Legal.Reason;
			return false;
		}
		if (!SubmitCrashIn(Coordinator, Recorder, Legal.Actions[0], OutReason))
		{
			return false;
		}
	}
	return true;
}

bool PassCrashInResponse(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 32 && Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response;
		++Guard)
	{
		const FWBMatchObservation Observation = Coordinator.BuildObservation(
			Coordinator.GetState().PriorityPlayer);
		const FWBMatchLegalAction* Pass = FindCrashInCore(
			Observation.LegalActions, EWBActionType::PassResponse);
		if (Pass == nullptr)
		{
			OutReason = TEXT("csn_crash_in_response_pass_missing");
			return false;
		}
		if (!SubmitCrashIn(Coordinator, Recorder, *Pass, OutReason))
		{
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response)
	{
		OutReason = TEXT("csn_crash_in_response_guard_exceeded");
		return false;
	}
	return true;
}

bool EndCrashInTurn(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = Legal.bOk
		? FindCrashInCore(Legal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr)
	{
		OutReason = Legal.bOk
			? FString(TEXT("csn_crash_in_end_turn_missing"))
			: Legal.Reason;
		return false;
	}
	return SubmitCrashIn(Coordinator, Recorder, *EndTurn, OutReason)
		&& ResolveCrashInTurnStart(Coordinator, Recorder, OutReason);
}

bool OpponentHandIsHidden(
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

bool ObservationContainsPrivateChoice(
	const FWBMatchObservation& Observation,
	const FString& CardId,
	const FString& InstanceId)
{
	return Observation.LegalActions.ContainsByPredicate(
		[&CardId, &InstanceId](const FWBMatchLegalAction& Action)
		{
			const FWBEffectAuxiliaryCardSelection& Selection =
				Action.ActivationCommand.EffectRequest.AuxiliaryCardSelection;
			return Action.ActionId.Contains(CardId)
				|| Action.ActionId.Contains(InstanceId)
				|| Selection.CardId == CardId
				|| Selection.CardInstanceId == InstanceId;
		});
}

int32 CountTrace(const TArray<FWBTraceEvent>& Trace, const FName Kind)
{
	return Trace.FilterByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	}).Num();
}

int32 CountAcceptedFamily(
	const WBMatchCoordinator& Coordinator,
	const FString& Family)
{
	return Coordinator.GetCommittedActionRecords().FilterByPredicate(
		[&Family](const FWBMatchCommittedActionRecord& Record)
		{
			return Record.ActionFamily == Family;
		}).Num();
}

int32 CountAcceptedPrefix(
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

bool WBProductionCSNCrashInSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionCSNCrashInSmoke"));
}

bool WBProductionCSNCrashInSmoke::IsUndertowRequested(
	const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionCSNUndertowArchivistSmoke"));
}

FString WBProductionCSNCrashInSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionCSNCrashInReceipt.json"));
}

FString WBProductionCSNCrashInSmoke::GetUndertowReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionCSNUndertowArchivistReceipt.json"));
}

FWBProductionCSNCrashInSmokeResult WBProductionCSNCrashInSmoke::RunScenario(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest,
	const bool bUndertow,
	const bool bNegateCrashIn)
{
	FWBProductionCSNCrashInSmokeResult Result;
	const FString ReplacementCardId = bUndertow
		? FString(TEXT("char_csn_undertow_archivist"))
		: FString(TEXT("char_csn_echo"));
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

	if (!EndCrashInTurn(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}
	const FWBMatchLegalActionGenerationResult DefenderLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Summon = DefenderLegal.bOk
		? FindCrashInSummon(
			DefenderLegal.Actions, TEXT("char_csn_rook"), FWBTile(4, 1))
		: nullptr;
	if (Summon == nullptr
		|| !SubmitCrashIn(Coordinator, Recorder, *Summon, Result.Reason)
		|| !PassCrashInResponse(Coordinator, Recorder, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_crash_in_source_summon_missing");
		}
		return Result;
	}

	const FWBUnitState* Source = Coordinator.GetState().Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("char_csn_rook")
				&& Unit.IsUnitOnBoard();
		});
	if (Source == nullptr)
	{
		Result.Reason = TEXT("csn_crash_in_source_unit_missing");
		return Result;
	}
	const int32 SourceUnitId = Source->UnitId;
	const FWBTile SourceTile(Source->X, Source->Y);
	const int32 SourceCurrentRL = Source->GetCurrentRLForRules();
	const FWBMatchLegalActionGenerationResult EquipLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Equip = EquipLegal.bOk
		? FindCrashInEquip(EquipLegal.Actions, SourceUnitId)
		: nullptr;
	if (Equip == nullptr)
	{
		Result.Reason = TEXT("csn_crash_in_wand_equip_missing");
		return Result;
	}
	const FString WandInstanceId = Equip->EquipRequest.SourceInstanceId;
	if (!SubmitCrashIn(Coordinator, Recorder, *Equip, Result.Reason)
		|| !EndCrashInTurn(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}

	const FWBPlayerStateData* AttackerPlayer =
		Coordinator.GetState().GetPlayerById(0);
	const FWBUnitState* Attacker = AttackerPlayer != nullptr
		? Coordinator.GetState().GetUnitById(AttackerPlayer->HeroUnitId)
		: nullptr;
	if (Attacker == nullptr)
	{
		Result.Reason = TEXT("csn_crash_in_attacker_missing");
		return Result;
	}
	const int32 AttackerUnitId = Attacker->UnitId;
	const int32 AttacksLeftBefore = Attacker->AttacksLeft;
	const FWBUnitState* DefenderBeforeAttack =
		Coordinator.GetState().GetUnitById(SourceUnitId);
	const int32 DefenderHPBefore = DefenderBeforeAttack != nullptr
		? DefenderBeforeAttack->HP
		: -1;
	const FWBMatchLegalActionGenerationResult AttackLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = AttackLegal.bOk
		? FindCrashInAttack(AttackLegal.Actions, AttackerUnitId, SourceUnitId)
		: nullptr;
	if (Attack == nullptr
		|| !SubmitCrashIn(Coordinator, Recorder, *Attack, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_crash_in_attack_missing");
		}
		return Result;
	}
	const FWBPendingAttackState DeclaredAttack = Coordinator.GetState().PendingAttack;

	const FWBMatchObservation DefenderResponse = Coordinator.BuildObservation(1);
	const int32 CrashInCandidateCount =
		DefenderResponse.LegalActions.FilterByPredicate(
			[](const FWBMatchLegalAction& Action)
			{
				return Action.Family == EWBMatchActionFamily::Activation
					&& Action.ActivationCommand.Source.SourceCardId
						== TEXT("effect_react_csn_crash_in");
			}).Num();
	const FWBMatchLegalAction* CrashIn = FindCrashInActivation(
		DefenderResponse.LegalActions,
		SourceUnitId,
		ReplacementCardId);
	if (CrashIn == nullptr || CrashInCandidateCount != 1)
	{
		Result.Reason = TEXT("csn_crash_in_activation_missing");
		return Result;
	}
	const FString ReplacementInstanceId = CrashIn->ActivationCommand
		.EffectRequest.AuxiliaryCardSelection.CardInstanceId;
	const FWBPlayerCardZoneState* DefenderZonesBefore =
		Coordinator.GetState().GetCardZoneState().PlayerZones.FindByPredicate(
			[](const FWBPlayerCardZoneState& Candidate)
			{
				return Candidate.PlayerId == 1;
			});
	const FString DrawnInstanceId = bUndertow
		&& DefenderZonesBefore != nullptr
		&& !DefenderZonesBefore->Deck.IsEmpty()
		? DefenderZonesBefore->Deck[0].Card.InstanceId
		: FString();
	if (ReplacementInstanceId.IsEmpty()
		|| !CrashIn->ActionId.Contains(ReplacementInstanceId))
	{
		Result.Reason = TEXT("csn_crash_in_immutable_choice_missing");
		return Result;
	}
	const FWBMatchObservation AttackerBefore = Coordinator.BuildObservation(0);
	if (!OpponentHandIsHidden(AttackerBefore, 1)
		|| ObservationContainsPrivateChoice(
			AttackerBefore, ReplacementCardId, ReplacementInstanceId))
	{
		Result.Reason = TEXT("csn_crash_in_pre_reveal_privacy_mismatch");
		return Result;
	}

	if (!SubmitCrashIn(Coordinator, Recorder, *CrashIn, Result.Reason))
	{
		return Result;
	}
	const FWBMatchObservation NestedOpponent = Coordinator.BuildObservation(0);
	if (!OpponentHandIsHidden(NestedOpponent, 1)
		|| ObservationContainsPrivateChoice(
			NestedOpponent, ReplacementCardId, ReplacementInstanceId))
	{
		Result.Reason = TEXT("csn_crash_in_pending_choice_privacy_mismatch");
		return Result;
	}
	const FWBMatchLegalAction* ResponseB = FindCrashInActivationByCard(
		NestedOpponent.LegalActions, TEXT("crash_in_smoke_negate_b"));
	if (ResponseB == nullptr
		|| !SubmitCrashIn(Coordinator, Recorder, *ResponseB, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_crash_in_nested_response_b_missing");
		}
		return Result;
	}
	if (bNegateCrashIn)
	{
		if (!PassCrashInResponse(Coordinator, Recorder, Result.Reason))
		{
			return Result;
		}
		const FWBUnitState* NegatedSource =
			Coordinator.GetState().GetUnitById(SourceUnitId);
		const FWBUnitState* UnexpectedReplacement =
			Coordinator.GetState().Units.FindByPredicate(
				[&ReplacementCardId](const FWBUnitState& Unit)
				{
					return Unit.CardId == ReplacementCardId
						&& Unit.IsUnitOnBoard();
				});
		const FWBPlayerCardZoneState* NegatedZones =
			Coordinator.GetState().GetCardZoneState().PlayerZones.FindByPredicate(
				[](const FWBPlayerCardZoneState& Candidate)
				{
					return Candidate.PlayerId == 1;
				});
		const bool bReplacementStillInHand = NegatedZones != nullptr
			&& NegatedZones->Hand.ContainsByPredicate(
				[&ReplacementInstanceId](const FWBZoneCardEntry& Entry)
				{
					return Entry.Card.InstanceId == ReplacementInstanceId;
				});
		const bool bPrivateDrawStillInDeck = NegatedZones != nullptr
			&& !DrawnInstanceId.IsEmpty()
			&& NegatedZones->Deck.ContainsByPredicate(
				[&DrawnInstanceId](const FWBZoneCardEntry& Entry)
				{
					return Entry.Card.InstanceId == DrawnInstanceId;
				});
		const TArray<FWBTraceEvent>& NegatedTrace = Coordinator.GetTraceLog();
		if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
			|| Coordinator.GetState().HasPendingAttack()
			|| Coordinator.GetState().HasOpenReactionWindow()
			|| !Coordinator.GetPendingEffectActivationStack().IsEmpty()
			|| NegatedSource == nullptr
			|| !NegatedSource->IsUnitOnBoard()
			|| NegatedSource->HP >= DefenderHPBefore
			|| UnexpectedReplacement != nullptr
			|| !bReplacementStillInHand
			|| !bPrivateDrawStillInDeck
			|| CountTrace(NegatedTrace,
				FName(TEXT("effect_replacement_summon"))) != 0
			|| CountTrace(NegatedTrace,
				FName(TEXT("csn_inheritance"))) != 0
			|| CountTrace(NegatedTrace,
				FName(TEXT("csn_inheritance_triggered"))) != 0
			|| CountTrace(NegatedTrace,
				FName(TEXT("csn_inheritance_card_drawn"))) != 0
			|| CountTrace(NegatedTrace,
				FName(TEXT("pending_attack_redirected"))) != 0
			|| CountTrace(NegatedTrace,
				FName(TEXT("attack_damage_resolved"))) != 1
			|| CountTrace(NegatedTrace,
				FName(TEXT("pending_effect_activation_negated"))) != 1
			|| CountAcceptedPrefix(Coordinator, TEXT("attack:")) != 1
			|| CountAcceptedPrefix(Coordinator, TEXT("summon:")) != 1
			|| CountAcceptedFamily(Coordinator, TEXT("activate")) != 2)
		{
			Result.Reason = TEXT("csn_undertow_negated_crash_in_mismatch");
			return Result;
		}
		Result.bOk = true;
		Result.Reason =
			TEXT("production_csn_undertow_archivist_negated_verified");
		Result.FinalGeneration = Coordinator.GetCoordinatorGeneration();
		Result.FinalRevision = Coordinator.GetCoordinatorRevision();
		Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
		Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
		return Result;
	}
	const FWBMatchObservation NestedOwner = Coordinator.BuildObservation(1);
	const FWBMatchLegalAction* ResponseC = FindCrashInActivationByCard(
		NestedOwner.LegalActions, TEXT("crash_in_smoke_negate_c"));
	if (ResponseC == nullptr
		|| !SubmitCrashIn(Coordinator, Recorder, *ResponseC, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
		{
			Result.Reason = TEXT("csn_crash_in_nested_response_c_missing");
		}
		return Result;
	}
	if (!PassCrashInResponse(Coordinator, Recorder, Result.Reason))
	{
		return Result;
	}

	Source = Coordinator.GetState().GetUnitById(SourceUnitId);
	const FWBUnitState* Replacement = Coordinator.GetState().Units.FindByPredicate(
		[&ReplacementCardId](const FWBUnitState& Unit)
		{
			return Unit.CardId == ReplacementCardId
				&& Unit.IsUnitOnBoard();
		});
	Attacker = Coordinator.GetState().GetUnitById(AttackerUnitId);
	const FWBEquippedCardEntry* InheritedWand =
		Coordinator.GetState().GetCardZoneState().EquippedCards.FindByPredicate(
			[&WandInstanceId](const FWBEquippedCardEntry& Entry)
			{
				return Entry.Card.InstanceId == WandInstanceId;
			});
	const auto* DefenderZones =
		Coordinator.GetState().GetCardZoneState().PlayerZones.FindByPredicate(
			[](const auto& Candidate)
			{
				return Candidate.PlayerId == 1;
			});
	const bool bSelectedStillInHand = DefenderZones != nullptr
		&& DefenderZones->Hand.ContainsByPredicate(
			[&ReplacementInstanceId](const FWBZoneCardEntry& Entry)
			{
				return Entry.Card.InstanceId == ReplacementInstanceId;
			});
	const bool bWandInDiscard = DefenderZones != nullptr
		&& DefenderZones->Discard.ContainsByPredicate(
			[&WandInstanceId](const FWBZoneCardEntry& Entry)
			{
				return Entry.Card.InstanceId == WandInstanceId;
			});
	const bool bExactDrawInHand = DefenderZones != nullptr
		&& !DrawnInstanceId.IsEmpty()
		&& DefenderZones->Hand.ContainsByPredicate(
			[&DrawnInstanceId](const FWBZoneCardEntry& Entry)
			{
				return Entry.Card.InstanceId == DrawnInstanceId;
			});
	const TArray<FWBTraceEvent>& Trace = Coordinator.GetTraceLog();
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action
		|| Coordinator.GetState().HasPendingAttack()
		|| Coordinator.GetState().HasOpenReactionWindow()
		|| !Coordinator.GetPendingEffectActivationStack().IsEmpty()
		|| Source == nullptr
		|| !Source->bDefeated
		|| !Source->bRemovedFromBoard
		|| Replacement == nullptr
		|| Replacement->X != SourceTile.X
		|| Replacement->Y != SourceTile.Y
		|| Replacement->HP != (bUndertow ? 7 : 9)
		|| Replacement->ATK != 2
		|| Replacement->AR != (bUndertow ? 3 : 2)
		|| Replacement->BaseRL != 2 + SourceCurrentRL
		|| Replacement->CurrentRL != 2 + SourceCurrentRL
		|| Replacement->RLUsed != 1
		|| bSelectedStillInHand
		|| InheritedWand == nullptr
		|| InheritedWand->EquippedToUnitId != Replacement->UnitId
		|| InheritedWand->Card.OwnerPlayerId != 1
		|| bWandInDiscard
		|| Attacker == nullptr
		|| Attacker->AttacksLeft != AttacksLeftBefore - 1
		|| DeclaredAttack.ContinuationId.IsEmpty()
		|| CountTrace(Trace, FName(TEXT("effect_replacement_summon"))) != 1
		|| CountTrace(Trace, FName(TEXT("inherited_wand_transferred"))) != 1
		|| CountTrace(Trace, FName(TEXT("csn_inheritance"))) != 1
		|| CountTrace(
			Trace, FName(TEXT("csn_inheritance_triggered")))
			!= (bUndertow ? 1 : 0)
		|| CountTrace(
			Trace, FName(TEXT("csn_inheritance_card_drawn")))
			!= (bUndertow ? 1 : 0)
		|| (bUndertow && !bExactDrawInHand)
		|| CountTrace(Trace, FName(TEXT("pending_attack_redirected"))) != 1
		|| CountTrace(Trace, FName(TEXT("attack_damage_resolved"))) != 1
		|| CountTrace(
			Trace, FName(TEXT("pending_effect_activation_negated"))) != 1
		|| CountTrace(
			Trace, FName(TEXT("pending_effect_parent_context_restored"))) < 3
		|| CountAcceptedPrefix(Coordinator, TEXT("attack:")) != 1
		|| CountAcceptedPrefix(Coordinator, TEXT("summon:")) != 1
		|| CountAcceptedFamily(Coordinator, TEXT("activate")) != 3)
	{
		Result.Reason = FString::Printf(
			TEXT("csn_crash_in_resolution_mismatch:phase=%d:pending_attack=%d:reaction=%d:stack=%d:source=%d/%d/%d:replacement=%d:tile=%d,%d:hp=%d:atk=%d:ar=%d:base_rl=%d:current_rl=%d:rl_used=%d:hand=%d:wand=%d:wand_target=%d:wand_owner=%d:wand_discard=%d:attacker=%d:attacks=%d/%d:continuation=%d:summon_trace=%d:wand_trace=%d:inherit_trace=%d:redirect_trace=%d:damage_trace=%d:negated_trace=%d:restore_trace=%d:attack_actions=%d:summon_actions=%d:activation_actions=%d"),
			static_cast<int32>(Coordinator.GetMatchPhase()),
			Coordinator.GetState().HasPendingAttack() ? 1 : 0,
			Coordinator.GetState().HasOpenReactionWindow() ? 1 : 0,
			Coordinator.GetPendingEffectActivationStack().Num(),
			Source != nullptr ? 1 : 0,
			Source != nullptr && Source->bDefeated ? 1 : 0,
			Source != nullptr && Source->bRemovedFromBoard ? 1 : 0,
			Replacement != nullptr ? 1 : 0,
			Replacement != nullptr ? Replacement->X : -1,
			Replacement != nullptr ? Replacement->Y : -1,
			Replacement != nullptr ? Replacement->HP : -1,
			Replacement != nullptr ? Replacement->ATK : -1,
			Replacement != nullptr ? Replacement->AR : -1,
			Replacement != nullptr ? Replacement->BaseRL : -1,
			Replacement != nullptr ? Replacement->CurrentRL : -1,
			Replacement != nullptr ? Replacement->RLUsed : -1,
			bSelectedStillInHand ? 1 : 0,
			InheritedWand != nullptr ? 1 : 0,
			InheritedWand != nullptr ? InheritedWand->EquippedToUnitId : -1,
			InheritedWand != nullptr ? InheritedWand->Card.OwnerPlayerId : -1,
			bWandInDiscard ? 1 : 0,
			Attacker != nullptr ? 1 : 0,
			Attacker != nullptr ? Attacker->AttacksLeft : -1,
			AttacksLeftBefore - 1,
			DeclaredAttack.ContinuationId.IsEmpty() ? 0 : 1,
			CountTrace(Trace, FName(TEXT("effect_replacement_summon"))),
			CountTrace(Trace, FName(TEXT("inherited_wand_transferred"))),
			CountTrace(Trace, FName(TEXT("csn_inheritance"))),
			CountTrace(Trace, FName(TEXT("pending_attack_redirected"))),
			CountTrace(Trace, FName(TEXT("attack_damage_resolved"))),
			CountTrace(Trace, FName(TEXT("pending_effect_activation_negated"))),
			CountTrace(Trace, FName(TEXT("pending_effect_parent_context_restored"))),
			CountAcceptedPrefix(Coordinator, TEXT("attack:")),
			CountAcceptedPrefix(Coordinator, TEXT("summon:")),
			CountAcceptedFamily(Coordinator, TEXT("activate")));
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
			? FString(TEXT("csn_crash_in_archive_mismatch"))
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
			? FString(TEXT("csn_crash_in_fresh_replay_mismatch"))
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
		|| ReceiptJson.Contains(ReplacementCardId)
		|| ReceiptJson.Contains(ReplacementInstanceId))
	{
		Result.Reason = TEXT("csn_crash_in_receipt_privacy_mismatch");
		return Result;
	}
	const FWBMatchObservation FinalOpponentObservation =
		Coordinator.BuildObservation(0);
	if (!OpponentHandIsHidden(FinalOpponentObservation, 1)
		|| (bUndertow
			&& (!DrawnInstanceId.IsEmpty()
				&& ObservationContainsPrivateChoice(
					FinalOpponentObservation,
					TEXT("undertow_smoke_private_draw"),
					DrawnInstanceId))))
	{
		Result.Reason = TEXT("csn_undertow_final_privacy_mismatch");
		return Result;
	}
	const FString ReceiptPath = bUndertow
		? GetUndertowReceiptPath()
		: GetReceiptPath();
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
	Result.Reason = bUndertow
		? TEXT("production_csn_undertow_archivist_verified")
		: TEXT("production_csn_crash_in_verified");
	Result.RecordsVerified = Replay.RecordsVerified;
	Result.FinalGeneration = Coordinator.GetCoordinatorGeneration();
	Result.FinalRevision = Coordinator.GetCoordinatorRevision();
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	Result.SerializedArchive = ArchiveBytes;
	Result.SerializedReceipt = ReceiptJson;
	return Result;
}

FWBProductionCSNCrashInSmokeResult WBProductionCSNCrashInSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	return RunScenario(BootstrapRequest, false, false);
}

FWBProductionCSNCrashInSmokeResult WBProductionCSNCrashInSmoke::RunUndertow(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	return RunScenario(BootstrapRequest, true, false);
}

FWBProductionCSNCrashInSmokeResult
WBProductionCSNCrashInSmoke::RunUndertowNegatedForTest(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	return RunScenario(BootstrapRequest, true, true);
}
