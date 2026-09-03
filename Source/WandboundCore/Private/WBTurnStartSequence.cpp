#include "WBTurnStartSequence.h"

#include "WBCharacterPassiveEligibility.h"
#include "WBCardLifecycle.h"
#include "WBEffectRunner.h"

namespace
{
FWBTraceEvent MakeTurnStartTrace(
	const FName Kind,
	const int32 PlayerId,
	const int32 TurnNumber)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = PlayerId;
	Event.TurnNumber = TurnNumber;
	Event.MatchPhase = FName(TEXT("turn_start"));
	Event.bOk = true;
	return Event;
}

FWBTurnStartSequenceResult MakeTurnStartFailure(const FString& Reason)
{
	FWBTurnStartSequenceResult Result;
	Result.Reason = Reason;
	return Result;
}

bool TriggerInstanceLess(
	const FWBTurnStartTriggerInstance& A,
	const FWBTurnStartTriggerInstance& B,
	const int32 ActivePlayerId)
{
	const bool bAActiveControlled =
		A.SourceSnapshot.ControllerPlayerId == ActivePlayerId;
	const bool bBActiveControlled =
		B.SourceSnapshot.ControllerPlayerId == ActivePlayerId;
	if (bAActiveControlled != bBActiveControlled)
	{
		return bAActiveControlled;
	}
	if (A.SourceSnapshot.ControllerPlayerId
		!= B.SourceSnapshot.ControllerPlayerId)
	{
		return A.SourceSnapshot.ControllerPlayerId
			< B.SourceSnapshot.ControllerPlayerId;
	}
	if (A.SourceSnapshot.SourceUnitId != B.SourceSnapshot.SourceUnitId)
	{
		return A.SourceSnapshot.SourceUnitId
			< B.SourceSnapshot.SourceUnitId;
	}
	return A.StableTriggerId < B.StableTriggerId;
}

bool IsEligibleSource(
	const FWBGameStateData& State,
	const FWBTurnStartTriggerInstance& Trigger)
{
	const FWBUnitState* Source =
		State.GetUnitById(Trigger.SourceSnapshot.SourceUnitId);
	return Source != nullptr
		&& WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(*Source)
		&& Source->CardId == Trigger.SourceSnapshot.SourceCardId;
}

FString ChoiceActionId(
	const FWBTurnStartTriggerInstance& Trigger,
	const int32 TargetUnitId)
{
	const FString Base = FString::Printf(
		TEXT("turn_start_trigger:p%d:u%d:%s"),
		Trigger.SourceSnapshot.ControllerPlayerId,
		Trigger.SourceSnapshot.SourceUnitId,
		*Trigger.Definition.TriggerId);
	return TargetUnitId == -1
		? Base
		: FString::Printf(TEXT("%s:t%d"), *Base, TargetUnitId);
}

TArray<int32> EligibleUnitTargets(const FWBGameStateData& State)
{
	TArray<int32> UnitIds;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.IsUnitOnBoard() && !Unit.bDefeated)
		{
			UnitIds.Add(Unit.UnitId);
		}
	}
	UnitIds.Sort();
	return UnitIds;
}

bool BuildEffectRequest(
	const FWBTurnStartTriggerInstance& Trigger,
	const int32 TargetUnitId,
	FWBEffectRequest& OutRequest,
	FString& OutReason)
{
	if (Trigger.Definition.Payloads.IsEmpty())
	{
		OutRequest = FWBEffectRequest();
		OutReason.Reset();
		return true;
	}
	if (Trigger.Definition.TargetRequirement
		!= EWBCardEffectTargetRequirement::Unit
		|| TargetUnitId == -1)
	{
		OutReason = TEXT("turn_start_trigger_target_required");
		return false;
	}

	OutRequest.Source.PlayerId = Trigger.SourceSnapshot.ControllerPlayerId;
	OutRequest.Source.SourceUnitId = Trigger.SourceSnapshot.SourceUnitId;
	OutRequest.Source.SourceCardId = Trigger.SourceSnapshot.SourceCardId;
	OutRequest.Source.ActivationProvenance =
		EWBActivationProvenance::ResolutionOnly;
	OutRequest.Source.SourceEffectId =
		Trigger.Definition.TriggerId;
	OutRequest.Target.TargetUnitId = TargetUnitId;
	OutRequest.Payloads = Trigger.Definition.Payloads;
	OutReason.Reset();
	return true;
}

void CollectTriggers(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBTurnStartSequenceState& Sequence)
{
	for (const FWBUnitState& Unit : State.Units)
	{
		if (!WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(Unit))
		{
			continue;
		}

		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository,
				Unit.CardId);
		if (!Lookup.bFound)
		{
			continue;
		}

		for (const FWBTurnStartTriggerDefinition& Definition :
			Lookup.Definition.TurnStartTriggers)
		{
			const bool bApplies =
				Definition.Scope
					== EWBTurnStartTriggerScope::AtStartOfEachTurn
				|| Unit.GetControllerPlayerIdForRules()
					== Sequence.ActivePlayerId;
			if (!bApplies || Definition.TriggerId.IsEmpty())
			{
				continue;
			}

			FWBTurnStartTriggerInstance Trigger;
			Trigger.SourceSnapshot =
				WBEventSnapshot::CaptureUnitSource(State, Unit);
			Trigger.EligibilityPolicy =
				EWBTriggerEligibilityPolicy::Hybrid;
			Trigger.ControllerPlayerId =
				Trigger.SourceSnapshot.ControllerPlayerId;
			Trigger.SourceUnitId = Unit.UnitId;
			Trigger.SourceCardId = Unit.CardId;
			Trigger.Definition = Definition;
			Trigger.StableTriggerId = ChoiceActionId(Trigger, -1);
			Trigger.EventIdentity = WBEventSnapshot::MakeIdentity(
				EWBEventKind::TurnStart,
				FString::Printf(
					TEXT("turn_start:%d:%s"),
					Sequence.TurnNumber,
					*Trigger.StableTriggerId),
				Sequence.TurnNumber);
			Sequence.PendingTriggers.Add(MoveTemp(Trigger));
		}
	}

	Sequence.PendingTriggers.Sort(
		[ActivePlayerId = Sequence.ActivePlayerId](
			const FWBTurnStartTriggerInstance& A,
			const FWBTurnStartTriggerInstance& B)
		{
			return TriggerInstanceLess(A, B, ActivePlayerId);
		});
}

struct FResolvedChoice
{
	int32 TriggerIndex = INDEX_NONE;
	int32 TargetUnitId = -1;
	FString ActionId;
};

TArray<FResolvedChoice> EnumerateChoices(
	const FWBGameStateData& State,
	const FWBTurnStartSequenceState& Sequence)
{
	TArray<FResolvedChoice> Choices;
	const TArray<int32> UnitTargets = EligibleUnitTargets(State);
	for (int32 Index = 0;
		Index < Sequence.PendingTriggers.Num();
		++Index)
	{
		const FWBTurnStartTriggerInstance& Trigger =
			Sequence.PendingTriggers[Index];
		if (!IsEligibleSource(State, Trigger))
		{
			continue;
		}

		if (Trigger.Definition.TargetRequirement
			== EWBCardEffectTargetRequirement::None)
		{
			FResolvedChoice Choice;
			Choice.TriggerIndex = Index;
			Choice.ActionId = ChoiceActionId(Trigger, -1);
			Choices.Add(MoveTemp(Choice));
		}
		else if (Trigger.Definition.TargetRequirement
			== EWBCardEffectTargetRequirement::Unit)
		{
			for (const int32 TargetUnitId : UnitTargets)
			{
				FResolvedChoice Choice;
				Choice.TriggerIndex = Index;
				Choice.TargetUnitId = TargetUnitId;
				Choice.ActionId =
					ChoiceActionId(Trigger, TargetUnitId);
				Choices.Add(MoveTemp(Choice));
			}
		}
	}
	Choices.Sort([](
		const FResolvedChoice& A,
		const FResolvedChoice& B)
	{
		return A.ActionId < B.ActionId;
	});
	return Choices;
}

bool ResolveChoice(
	FWBGameStateData& State,
	const FResolvedChoice& Choice,
	FWBTurnStartSequenceState& Sequence,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason)
{
	if (!Sequence.PendingTriggers.IsValidIndex(Choice.TriggerIndex))
	{
		OutReason = TEXT("turn_start_trigger_choice_invalid");
		return false;
	}

	const FWBTurnStartTriggerInstance Trigger =
		Sequence.PendingTriggers[Choice.TriggerIndex];
	if (!IsEligibleSource(State, Trigger))
	{
		OutReason =
			TEXT("turn_start_trigger_source_no_longer_valid");
		return false;
	}

	FWBTraceEvent Selected = MakeTurnStartTrace(
		FName(TEXT("turn_start_trigger_order_selected")),
		Sequence.ActivePlayerId,
		Sequence.TurnNumber);
	Selected.ActionId = Choice.ActionId;
	Selected.SourceUnitId = Trigger.SourceSnapshot.SourceUnitId;
	Selected.TargetUnitId = Choice.TargetUnitId;
	OutTraceEvents.Add(Selected);

	for (int32 DrawIndex = 0;
		DrawIndex < Trigger.Definition.DrawCount;
		++DrawIndex)
	{
		const FWBCardLifecycleResult Draw =
			WBCardLifecycle::DrawOneCard(
				State,
				Trigger.SourceSnapshot.ControllerPlayerId);
		if (!Draw.bOk)
		{
			OutReason = Draw.Reason;
			return false;
		}

		FWBTraceEvent Drawn = MakeTurnStartTrace(
			FName(TEXT("turn_start_trigger_card_drawn")),
			Trigger.SourceSnapshot.ControllerPlayerId,
			Sequence.TurnNumber);
		Drawn.SourceUnitId = Trigger.SourceSnapshot.SourceUnitId;
		Drawn.CardCount = 1;
		OutTraceEvents.Add(Drawn);
	}

	FWBEffectRequest EffectRequest;
	if (!BuildEffectRequest(
			Trigger,
			Choice.TargetUnitId,
			EffectRequest,
			OutReason))
	{
		return false;
	}
	if (!Trigger.Definition.Payloads.IsEmpty())
	{
		const FWBEffectRequestResult EffectResult =
			WBEffectRunner::ApplyEffectRequest(
				State,
				EffectRequest);
		if (!EffectResult.bOk)
		{
			OutReason = EffectResult.Reason;
			return false;
		}
		OutTraceEvents.Append(EffectResult.TraceEvents);
	}

	FWBTraceEvent Resolved = MakeTurnStartTrace(
		FName(TEXT("turn_start_trigger_resolved")),
		Trigger.SourceSnapshot.ControllerPlayerId,
		Sequence.TurnNumber);
	Resolved.ActionId = Trigger.StableTriggerId;
	Resolved.SourceUnitId = Trigger.SourceSnapshot.SourceUnitId;
	Resolved.TargetUnitId = Choice.TargetUnitId;
	OutTraceEvents.Add(Resolved);
	Sequence.PendingTriggers.RemoveAt(
		Choice.TriggerIndex,
		1,
		EAllowShrinking::No);
	OutReason.Reset();
	return true;
}

FWBTurnStartSequenceResult ContinueAutomaticResolution(
	FWBGameStateData& State,
	FWBTurnStartSequenceState& Sequence)
{
	FWBTurnStartSequenceResult Result;
	Result.bOk = true;
	Sequence.Phase =
		EWBTurnStartSequencePhase::EffectResolution;

	while (!Sequence.PendingTriggers.IsEmpty())
	{
		for (int32 Index =
				Sequence.PendingTriggers.Num() - 1;
			Index >= 0;
			--Index)
		{
			if (!IsEligibleSource(
				State,
				Sequence.PendingTriggers[Index]))
			{
				Sequence.PendingTriggers.RemoveAt(
					Index,
					1,
					EAllowShrinking::No);
			}
		}
		if (Sequence.PendingTriggers.IsEmpty())
		{
			break;
		}

		const TArray<FResolvedChoice> Choices =
			EnumerateChoices(State, Sequence);
		if (Choices.IsEmpty())
		{
			return MakeTurnStartFailure(
				TEXT("turn_start_trigger_choice_invalid"));
		}
		if (Choices.Num() > 1)
		{
			Result.bChoiceRequired = true;
			for (const FResolvedChoice& Choice : Choices)
			{
				Result.LegalChoiceActionIds.Add(
					Choice.ActionId);
			}
			FWBTraceEvent Requested =
				MakeTurnStartTrace(
					FName(
						TEXT("turn_start_trigger_order_requested")),
					Sequence.ActivePlayerId,
					Sequence.TurnNumber);
			Requested.CardCount = Choices.Num();
			Result.TraceEvents.Add(Requested);
			return Result;
		}

		FString Reason;
		if (!ResolveChoice(
			State,
			Choices[0],
			Sequence,
			Result.TraceEvents,
			Reason))
		{
			return MakeTurnStartFailure(Reason);
		}
		if (State.bGameOver)
		{
			Sequence.Phase =
				EWBTurnStartSequencePhase::Terminal;
			Result.bTerminal = true;
			return Result;
		}
	}

	Sequence.bEffectsResolved = true;
	Sequence.bCompleted = true;
	Sequence.Phase =
		EWBTurnStartSequencePhase::Complete;
	Result.bCompleted = true;
	Result.TraceEvents.Add(MakeTurnStartTrace(
		FName(TEXT("turn_start_completed")),
		Sequence.ActivePlayerId,
		Sequence.TurnNumber));
	return Result;
}
}

FWBTurnStartSequenceResult WBTurnStartSequence::Begin(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ActivePlayerId,
	const int32 ExplicitMPRoll,
	FWBTurnStartSequenceState& InOutSequence)
{
	if (InOutSequence.Phase
		!= EWBTurnStartSequencePhase::NotStarted)
	{
		return MakeTurnStartFailure(
			TEXT("turn_start_sequence_already_started"));
	}
	if (!FWBGameStateData::IsValidPlayerId(ActivePlayerId)
		|| State.GetPlayerById(ActivePlayerId) == nullptr)
	{
		return MakeTurnStartFailure(TEXT("bad_player"));
	}
	if (ActivePlayerId != State.CurrentPlayer)
	{
		return MakeTurnStartFailure(TEXT("not_active_player"));
	}
	if (ExplicitMPRoll < 1 || ExplicitMPRoll > 6)
	{
		return MakeTurnStartFailure(TEXT("invalid_mp_roll"));
	}
	if (State.bGameOver)
	{
		return MakeTurnStartFailure(TEXT("game_over"));
	}

	FWBGameStateData WorkingState = State;
	FWBTurnStartSequenceState WorkingSequence;
	WorkingSequence.ActivePlayerId = ActivePlayerId;
	WorkingSequence.TurnNumber = WorkingState.TurnNumber;
	WorkingSequence.MPRoll = ExplicitMPRoll;

	FWBTurnStartSequenceResult Result;
	Result.bOk = true;
	Result.TraceEvents.Add(MakeTurnStartTrace(
		FName(TEXT("turn_start_started")),
		ActivePlayerId,
		WorkingSequence.TurnNumber));

	WorkingSequence.Phase =
		EWBTurnStartSequencePhase::Draw;
	const FWBCardLifecycleResult DrawResult =
		WBCardLifecycle::ApplyTurnStartDraw(
			WorkingState,
			ActivePlayerId,
			WorkingState.TurnNumber,
			WorkingState.FirstPlayerId);
	if (!DrawResult.bOk)
	{
		return MakeTurnStartFailure(DrawResult.Reason);
	}
	WorkingSequence.bDrawSkipped =
		DrawResult.Code
			== EWBCardLifecycleResultCode::
				FirstPlayerFirstTurnDrawSkipped;
	WorkingSequence.bDrawCompleted = true;
	FWBTraceEvent DrawTrace = MakeTurnStartTrace(
		WorkingSequence.bDrawSkipped
			? FName(TEXT("turn_start_draw_skipped"))
			: FName(TEXT("turn_start_card_drawn")),
		ActivePlayerId,
		WorkingSequence.TurnNumber);
	DrawTrace.CardCount =
		WorkingSequence.bDrawSkipped ? 0 : 1;
	DrawTrace.Reason = WorkingSequence.bDrawSkipped
		? TEXT("first_player_turn_one_draw_skipped")
		: TEXT("normal_turn_start_draw");
	Result.TraceEvents.Add(DrawTrace);

	WorkingSequence.Phase =
		EWBTurnStartSequencePhase::MPRoll;
	const FWBApplyActionResult MPRollResult =
		WBEffectRunner::ApplyTurnStartMPRoll(
			WorkingState,
			ActivePlayerId,
			ExplicitMPRoll);
	if (!MPRollResult.bOk)
	{
		return MakeTurnStartFailure(MPRollResult.Reason);
	}
	WorkingSequence.bMPGenerated = true;
	Result.TraceEvents.Append(MPRollResult.TraceEvents);

	WorkingSequence.Phase =
		EWBTurnStartSequencePhase::ResourceReset;
	const FWBApplyActionResult ResetResult =
		WBEffectRunner::ApplyTurnStartResourceReset(
			WorkingState,
			ActivePlayerId);
	if (!ResetResult.bOk)
	{
		return MakeTurnStartFailure(ResetResult.Reason);
	}
	WorkingSequence.bResourcesReset = true;
	Result.TraceEvents.Append(ResetResult.TraceEvents);

	WorkingSequence.Phase =
		EWBTurnStartSequencePhase::StatusResolution;
	Result.TraceEvents.Add(MakeTurnStartTrace(
		FName(TEXT("turn_start_status_phase_started")),
		ActivePlayerId,
		WorkingSequence.TurnNumber));
	TArray<int32> UnitsBeforeStatus;
	for (const FWBUnitState& Unit : WorkingState.Units)
	{
		if (Unit.IsUnitOnBoard())
		{
			UnitsBeforeStatus.Add(Unit.UnitId);
		}
	}
	UnitsBeforeStatus.Sort();
	const FWBApplyActionResult StatusResult =
		WBEffectRunner::ApplyStartOfTurnStatusTicks(
			WorkingState,
			ActivePlayerId);
	if (!StatusResult.bOk)
	{
		return MakeTurnStartFailure(StatusResult.Reason);
	}
	Result.TraceEvents.Append(StatusResult.TraceEvents);
	for (const int32 UnitId : UnitsBeforeStatus)
	{
		const FWBUnitState* Unit =
			WorkingState.GetUnitById(UnitId);
		if (Unit == nullptr || !Unit->IsUnitOnBoard())
		{
			FWBTraceEvent Defeated = MakeTurnStartTrace(
				FName(TEXT("turn_start_unit_defeated")),
				ActivePlayerId,
				WorkingSequence.TurnNumber);
			Defeated.TargetUnitId = UnitId;
			Result.TraceEvents.Add(Defeated);
		}
	}
	WorkingSequence.bStatusesResolved = true;
	Result.TraceEvents.Add(MakeTurnStartTrace(
		FName(TEXT("turn_start_status_phase_completed")),
		ActivePlayerId,
		WorkingSequence.TurnNumber));
	if (WorkingState.bGameOver)
	{
		WorkingSequence.Phase =
			EWBTurnStartSequencePhase::Terminal;
		State = MoveTemp(WorkingState);
		InOutSequence = MoveTemp(WorkingSequence);
		Result.bTerminal = true;
		return Result;
	}

	WorkingSequence.Phase =
		EWBTurnStartSequencePhase::EffectCollection;
	CollectTriggers(
		WorkingState,
		Repository,
		WorkingSequence);
	FWBTraceEvent Collected = MakeTurnStartTrace(
		FName(TEXT("turn_start_triggers_collected")),
		ActivePlayerId,
		WorkingSequence.TurnNumber);
	Collected.CardCount =
		WorkingSequence.PendingTriggers.Num();
	Result.TraceEvents.Add(Collected);

	FWBTurnStartSequenceResult Resolution =
		ContinueAutomaticResolution(
			WorkingState,
			WorkingSequence);
	if (!Resolution.bOk)
	{
		return Resolution;
	}
	Result.TraceEvents.Append(Resolution.TraceEvents);
	Result.bCompleted = Resolution.bCompleted;
	Result.bChoiceRequired =
		Resolution.bChoiceRequired;
	Result.bTerminal = Resolution.bTerminal;
	Result.LegalChoiceActionIds =
		MoveTemp(Resolution.LegalChoiceActionIds);
	State = MoveTemp(WorkingState);
	InOutSequence = MoveTemp(WorkingSequence);
	return Result;
}

FWBTurnStartSequenceResult WBTurnStartSequence::SubmitChoice(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FString& ActionId,
	FWBTurnStartSequenceState& InOutSequence)
{
	if (InOutSequence.Phase
		!= EWBTurnStartSequencePhase::EffectResolution
		|| InOutSequence.bCompleted)
	{
		return MakeTurnStartFailure(
			TEXT("turn_start_sequence_not_complete"));
	}

	FWBGameStateData WorkingState = State;
	FWBTurnStartSequenceState WorkingSequence =
		InOutSequence;
	const TArray<FResolvedChoice> Choices =
		EnumerateChoices(WorkingState, WorkingSequence);
	const FResolvedChoice* Selected =
		Choices.FindByPredicate(
			[&ActionId](const FResolvedChoice& Choice)
			{
				return Choice.ActionId == ActionId;
			});
	if (Selected == nullptr)
	{
		return MakeTurnStartFailure(
			TEXT("turn_start_trigger_choice_invalid"));
	}

	FWBTurnStartSequenceResult Result;
	Result.bOk = true;
	FString Reason;
	if (!ResolveChoice(
		WorkingState,
		*Selected,
		WorkingSequence,
		Result.TraceEvents,
		Reason))
	{
		return MakeTurnStartFailure(Reason);
	}

	FWBTurnStartSequenceResult Resolution =
		ContinueAutomaticResolution(
			WorkingState,
			WorkingSequence);
	if (!Resolution.bOk)
	{
		return Resolution;
	}
	Result.TraceEvents.Append(Resolution.TraceEvents);
	Result.bCompleted = Resolution.bCompleted;
	Result.bChoiceRequired =
		Resolution.bChoiceRequired;
	Result.bTerminal =
		Resolution.bTerminal || WorkingState.bGameOver;
	Result.LegalChoiceActionIds =
		MoveTemp(Resolution.LegalChoiceActionIds);
	State = MoveTemp(WorkingState);
	InOutSequence = MoveTemp(WorkingSequence);
	return Result;
}

TArray<FString>
WBTurnStartSequence::EnumerateLegalChoiceActionIds(
	const FWBGameStateData& State,
	const FWBTurnStartSequenceState& Sequence)
{
	TArray<FString> ActionIds;
	for (const FResolvedChoice& Choice :
		EnumerateChoices(State, Sequence))
	{
		ActionIds.Add(Choice.ActionId);
	}
	return ActionIds;
}
