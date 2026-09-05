#include "WBDeathResolution.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBCharacterPassiveEligibility.h"
#include "WBRules.h"

namespace
{
struct FWBDeathCleanupUnitPlan
{
	int32 UnitId = -1;
};

bool UnitIdLess(const FWBDeathCleanupUnitPlan& A, const FWBDeathCleanupUnitPlan& B)
{
	return A.UnitId < B.UnitId;
}

bool EquippedCardCleanupLess(const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
{
	if (A.EquipOrder != B.EquipOrder)
	{
		return A.EquipOrder < B.EquipOrder;
	}

	if (A.SlotId != B.SlotId)
	{
		return A.SlotId < B.SlotId;
	}

	if (A.Card.InstanceId != B.Card.InstanceId)
	{
		return A.Card.InstanceId < B.Card.InstanceId;
	}

	return A.Card.CardId < B.Card.CardId;
}

FWBApplyActionResult MakeDeathResolutionFailure(const FString& Reason)
{
	FWBApplyActionResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

bool IsHeroUnitForOwner(const FWBGameStateData& State, const FWBUnitState& Unit)
{
	const FWBPlayerStateData* Player = State.GetPlayerById(
		Unit.GetOwnerPlayerIdForRules());
	return Player != nullptr && Player->HeroUnitId == Unit.UnitId;
}

FWBDeathResolutionCandidate MakeDeathResolutionCandidate(
	const FWBGameStateData& State,
	const FWBUnitState& Unit)
{
	FWBDeathResolutionCandidate Candidate;
	Candidate.UnitId = Unit.UnitId;
	Candidate.OwnerId = Unit.GetControllerPlayerIdForRules();
	Candidate.bIsHero = IsHeroUnitForOwner(State, Unit);
	return Candidate;
}

int32 OpposingPlayerId(const int32 PlayerId)
{
	if (PlayerId == 0)
	{
		return 1;
	}

	if (PlayerId == 1)
	{
		return 0;
	}

	return -1;
}

FWBTraceEvent MakeEquippedCardDiscardedOnDeathTrace(
	const FWBEquippedCardEntry& Entry,
	const int32 DiscardIndex,
	const int32 DefeatedUnitId,
	const bool bHeroUnit,
	const int32 ResolutionOrder)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("equipped_card_discarded_on_death"));
	Event.PlayerId = Entry.Card.OwnerPlayerId;
	Event.TargetUnitId = DefeatedUnitId;
	Event.CardInstanceId = Entry.Card.InstanceId;
	Event.CardId = Entry.Card.CardId;
	Event.SlotId = Entry.SlotId;
	Event.EquipOrder = Entry.EquipOrder;
	Event.DiscardIndex = DiscardIndex;
	Event.ResolutionOrder = ResolutionOrder;
	Event.bHeroUnit = bHeroUnit;
	Event.bOk = true;
	return Event;
}

FWBTraceEvent MakeUnitDefeatedTrace(
	const FWBUnitState& Unit,
	const int32 PreviousHP,
	const bool bHeroUnit,
	const int32 ResolutionOrder)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("unit_defeated"));
	Event.PlayerId = Unit.GetControllerPlayerIdForRules();
	Event.TargetUnitId = Unit.UnitId;
	Event.PreviousHP = PreviousHP;
	Event.NewHP = Unit.HP;
	Event.ResolutionOrder = ResolutionOrder;
	Event.bHeroUnit = bHeroUnit;
	Event.bAtOrBelowZeroHP = true;
	Event.bOk = true;
	return Event;
}

FWBTraceEvent MakeUnitRemovedFromBoardTrace(
	const FWBUnitState& Unit,
	const FWBTile& PreviousTile,
	const bool bHeroUnit,
	const int32 ResolutionOrder)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("unit_removed_from_board"));
	Event.PlayerId = Unit.GetControllerPlayerIdForRules();
	Event.TargetUnitId = Unit.UnitId;
	Event.FromTile = PreviousTile;
	Event.ResolutionOrder = ResolutionOrder;
	Event.bHeroUnit = bHeroUnit;
	Event.bOk = true;
	return Event;
}

FWBTraceEvent MakeHeroDefeatedTrace(
	const FWBUnitState& Unit,
	const int32 WinningPlayerId,
	const int32 ResolutionOrder)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("hero_defeated"));
	Event.PlayerId = Unit.GetControllerPlayerIdForRules();
	Event.TargetUnitId = Unit.UnitId;
	Event.WinningPlayerId = WinningPlayerId;
	Event.ResolutionOrder = ResolutionOrder;
	Event.bHeroUnit = true;
	Event.bOk = true;
	return Event;
}

int32 FindDiscardIndexForCard(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const FString& CardInstanceId)
{
	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), PlayerId);
	if (PlayerZones == nullptr)
	{
		return INDEX_NONE;
	}

	for (const FWBZoneCardEntry& Entry : PlayerZones->Discard)
	{
		if (Entry.Card.InstanceId == CardInstanceId)
		{
			return Entry.ZoneIndex;
		}
	}

	return INDEX_NONE;
}

bool ValidateDeathCleanupZoneState(const FWBGameStateData& State, FString& OutReason)
{
	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(State.GetCardZoneState(), ZoneReason))
	{
		OutReason = ZoneReason.IsEmpty() ? FString(TEXT("invalid_zone_state")) : ZoneReason;
		return false;
	}

	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		if (!FWBGameStateData::IsValidPlayerId(Entry.Card.OwnerPlayerId)
			|| State.GetPlayerById(Entry.Card.OwnerPlayerId) == nullptr)
		{
			OutReason = TEXT("equipped_card_owner_invalid");
			return false;
		}

		if (WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), Entry.Card.OwnerPlayerId) == nullptr)
		{
			OutReason = TEXT("player_zones_missing");
			return false;
		}

		const FWBUnitState* EquippedUnit = State.GetUnitById(Entry.EquippedToUnitId);
		if (EquippedUnit == nullptr || !EquippedUnit->IsUnitOnBoard())
		{
			OutReason = TEXT("equipped_unit_not_found");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

TArray<FWBEquippedCardEntry> CollectEquippedCardsForUnit(
	const FWBGameStateData& State,
	const int32 UnitId)
{
	TArray<FWBEquippedCardEntry> Entries;
	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		if (Entry.EquippedToUnitId == UnitId)
		{
			Entries.Add(Entry);
		}
	}

	Entries.Sort(EquippedCardCleanupLess);
	return Entries;
}

void ClearPendingAttackIfUnitRemoved(FWBGameStateData& State, const int32 UnitId)
{
	if (!State.HasPendingAttack())
	{
		return;
	}

	if (State.PendingAttack.AttackerUnitId == UnitId || State.PendingAttack.DefenderUnitId == UnitId)
	{
		State.ClearPendingAttack();
	}
}
}

FWBDeathPreventionResult WBDeathResolution::EvaluateDeathPrevention(
	const FWBGameStateData& State,
	const FWBDeathResolutionCandidate& Candidate)
{
	(void)State;
	(void)Candidate;

	FWBDeathPreventionResult Result;
	Result.bPrevented = false;
	Result.PreventionReason = NAME_None;
	return Result;
}

bool WBDeathResolution::BuildSuccessfulDestructionSnapshot(
	const FWBGameStateData& State,
	const int32 UnitId,
	const EWBUnitDestructionCause Cause,
	const int32 ResolutionOrder,
	FWBUnitDestructionSnapshot& OutSnapshot,
	FString& OutReason)
{
	OutSnapshot = FWBUnitDestructionSnapshot();
	const FWBUnitState* Unit = State.GetUnitById(UnitId);
	if (Unit == nullptr || !Unit->IsUnitOnBoard() || Unit->bDefeated)
	{
		OutReason = TEXT("destruction_snapshot_unit_unavailable");
		return false;
	}

	OutSnapshot.EventId = FString::Printf(
		TEXT("destroyed:t%d:q%d:o%d:u%d"),
		State.TurnNumber,
		State.PendingUnitDestructionEvents.Num(),
		ResolutionOrder,
		UnitId);
	OutSnapshot.EventIdentity = WBEventSnapshot::MakeIdentity(
		EWBEventKind::Destruction,
		OutSnapshot.EventId,
		State.TurnNumber);
	OutSnapshot.DestroyedUnitSnapshot =
		WBEventSnapshot::CaptureUnitParticipant(State, *Unit);
	OutSnapshot.DestroyedUnitId = UnitId;
	OutSnapshot.DestroyedCardId = Unit->CardId;
	OutSnapshot.OwnerPlayerId = Unit->GetOwnerPlayerIdForRules();
	OutSnapshot.ControllerPlayerId = Unit->GetControllerPlayerIdForRules();
	OutSnapshot.LastTile = FWBTile(Unit->X, Unit->Y);
	OutSnapshot.bWasHero = IsHeroUnitForOwner(State, *Unit);
	OutSnapshot.Cause = Cause;
	OutSnapshot.BaseRLSnapshot = Unit->GetBaseRLForRules();
	OutSnapshot.CurrentRLSnapshot = Unit->GetCurrentRLForRules();
	OutSnapshot.RLUsedSnapshot = Unit->RLUsed;
	OutSnapshot.EquippedWands = CollectEquippedCardsForUnit(State, UnitId);
	OutSnapshot.bCharacterPassiveEligible =
		WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(*Unit);
	for (const FWBUnitState& Candidate : State.Units)
	{
		if (Candidate.UnitId == UnitId
			|| !WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(
				Candidate))
		{
			continue;
		}
		FWBPostDestructionObserverSourceSnapshot Observer;
		Observer.SourceSnapshot =
			WBEventSnapshot::CaptureUnitSource(State, Candidate);
		Observer.EligibilityPolicy = EWBTriggerEligibilityPolicy::Hybrid;
		Observer.SourceUnitId = Candidate.UnitId;
		Observer.SourceCardId = Candidate.CardId;
		Observer.OwnerPlayerId = Candidate.GetOwnerPlayerIdForRules();
		Observer.ControllerPlayerId =
			Candidate.GetControllerPlayerIdForRules();
		OutSnapshot.ObserverSources.Add(MoveTemp(Observer));
	}
	OutSnapshot.ObserverSources.Sort([](
		const FWBPostDestructionObserverSourceSnapshot& A,
		const FWBPostDestructionObserverSourceSnapshot& B)
	{
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
		return A.SourceSnapshot.SourceCardId
			< B.SourceSnapshot.SourceCardId;
	});
	for (int32 Index = 0; Index < OutSnapshot.ObserverSources.Num(); ++Index)
	{
		OutSnapshot.ObserverSources[Index].SourceOrder = Index;
	}
	OutSnapshot.ResolutionOrder = ResolutionOrder;
	OutReason.Reset();
	return true;
}

void WBDeathResolution::QueueSuccessfulDestructionEvent(
	FWBGameStateData& State,
	FWBUnitDestructionSnapshot Snapshot)
{
	State.PendingUnitDestructionEvents.Add(MoveTemp(Snapshot));
	State.PendingUnitDestructionEvents.StableSort([](
		const FWBUnitDestructionSnapshot& A,
		const FWBUnitDestructionSnapshot& B)
	{
		if (A.ResolutionOrder != B.ResolutionOrder)
		{
			return A.ResolutionOrder < B.ResolutionOrder;
		}
		if (A.DestroyedUnitId != B.DestroyedUnitId)
		{
			return A.DestroyedUnitId < B.DestroyedUnitId;
		}
		return A.EventId < B.EventId;
	});
}

FWBUnitDestructionResult WBDeathResolution::ApplyGenuineUnitDestruction(
	FWBGameStateData& State,
	const FWBUnitDestructionRequest& Request)
{
	FWBUnitDestructionResult Result;
	FString ValidationReason;
	if (!ValidateDeathCleanupZoneState(State, ValidationReason))
	{
		Result.Reason = ValidationReason;
		return Result;
	}
	const FWBUnitState* Target = State.GetUnitById(Request.TargetUnitId);
	if (Target == nullptr || !Target->IsUnitOnBoard() || Target->bDefeated)
	{
		Result.Reason = TEXT("destroy_target_unavailable");
		return Result;
	}
	const FWBDeathPreventionResult Prevention = EvaluateDeathPrevention(
		State, MakeDeathResolutionCandidate(State, *Target));
	if (Prevention.bPrevented)
	{
		Result.bOk = true;
		Result.bPrevented = true;
		Result.Reason = Prevention.PreventionReason.IsNone()
			? FString(TEXT("destruction_prevented"))
			: Prevention.PreventionReason.ToString();
		return Result;
	}

	FWBUnitDestructionSnapshot Snapshot;
	if (!BuildSuccessfulDestructionSnapshot(
		State,
		Request.TargetUnitId,
		Request.Cause,
		Request.ResolutionOrder,
		Snapshot,
		ValidationReason))
	{
		Result.Reason = ValidationReason;
		return Result;
	}

	FWBGameStateData WorkingState = State;
	FWBUnitState* MutableTarget = WorkingState.GetMutableUnitById(
		Request.TargetUnitId);
	if (MutableTarget == nullptr)
	{
		Result.Reason = TEXT("destroy_target_unavailable");
		return Result;
	}
	const int32 PreviousHP = MutableTarget->HP;
	const FWBTile PreviousTile(MutableTarget->X, MutableTarget->Y);
	const bool bHeroUnit = Snapshot.bWasHero;

	for (const FWBEquippedCardEntry& Entry : Snapshot.EquippedWands)
	{
		if (Request.EquipmentDisposition
			== EWBDestructionEquipmentDisposition::Discard)
		{
			const FWBCardLifecycleResult MoveResult =
				WBCardLifecycle::MoveEquippedCardToDiscard(
					WorkingState,
					Entry.Card.OwnerPlayerId,
					Entry.Card.InstanceId);
			if (!MoveResult.bOk)
			{
				Result.Reason = WBCardLifecycle::ResultCodeToString(
					MoveResult.Code);
				return Result;
			}
			const int32 DiscardIndex = FindDiscardIndexForCard(
				WorkingState,
				Entry.Card.OwnerPlayerId,
				Entry.Card.InstanceId);
			if (DiscardIndex == INDEX_NONE)
			{
				Result.Reason = TEXT("discarded_card_not_found");
				return Result;
			}
			Result.TraceEvents.Add(MakeEquippedCardDiscardedOnDeathTrace(
				Entry,
				DiscardIndex,
				Request.TargetUnitId,
				bHeroUnit,
				Request.ResolutionOrder));
		}
		else
		{
			FWBCardZoneState& Zones =
				WorkingState.GetMutableCardZoneStateForTest();
			const int32 Removed = Zones.EquippedCards.RemoveAll(
				[&Entry](const FWBEquippedCardEntry& Candidate)
				{
					return Candidate.Card.InstanceId == Entry.Card.InstanceId
						&& Candidate.EquippedToUnitId == Entry.EquippedToUnitId;
				});
			if (Removed != 1)
			{
				Result.Reason = TEXT("detached_equipment_unavailable");
				return Result;
			}
		}
	}

	MutableTarget = WorkingState.GetMutableUnitById(Request.TargetUnitId);
	MutableTarget->HP = 0;
	MutableTarget->ResonanceModifiers.Reset();
	MutableTarget->SetCanonicalRL(
		MutableTarget->GetBaseRLForRules(),
		MutableTarget->GetBaseRLForRules(),
		0);
	MutableTarget->MarkUnitDefeated();
	MutableTarget->RemoveUnitFromBoard();
	if (Request.PendingAttackPolicy
		== EWBDestructionPendingAttackPolicy::ClearIfParticipant)
	{
		ClearPendingAttackIfUnitRemoved(WorkingState, Request.TargetUnitId);
	}

	Result.TraceEvents.Add(MakeUnitDefeatedTrace(
		*MutableTarget, PreviousHP, bHeroUnit, Request.ResolutionOrder));
	Result.TraceEvents.Add(MakeUnitRemovedFromBoardTrace(
		*MutableTarget, PreviousTile, bHeroUnit, Request.ResolutionOrder));
	QueueSuccessfulDestructionEvent(WorkingState, Snapshot);

	if (bHeroUnit
		&& Request.TerminalPolicy
			== EWBDestructionTerminalPolicy::CommitImmediately)
	{
		const FWBApplyActionResult Terminal = CommitDeferredHeroDestruction(
			WorkingState, Snapshot, Request.TerminalSource);
		if (!Terminal.bOk)
		{
			Result.Reason = Terminal.Reason;
			return Result;
		}
		Result.TraceEvents.Append(Terminal.TraceEvents);
	}
	if (!ValidateDeathCleanupZoneState(WorkingState, ValidationReason))
	{
		Result.Reason = ValidationReason;
		return Result;
	}

	Result.bOk = true;
	Result.bDestroyed = true;
	Result.Snapshot = Snapshot;
	State = MoveTemp(WorkingState);
	return Result;
}

FWBApplyActionResult WBDeathResolution::CommitDeferredHeroDestruction(
	FWBGameStateData& State,
	const FWBUnitDestructionSnapshot& Snapshot,
	const EWBTerminalSource TerminalSource)
{
	if (!Snapshot.bWasHero)
	{
		return MakeDeathResolutionFailure(TEXT("deferred_hero_snapshot_required"));
	}
	FWBUnitState* Unit = State.GetMutableUnitById(Snapshot.DestroyedUnitId);
	if (Unit == nullptr || Unit->IsUnitOnBoard() || !Unit->bDefeated)
	{
		return MakeDeathResolutionFailure(TEXT("deferred_hero_state_invalid"));
	}
	const int32 WinningPlayerId = OpposingPlayerId(Snapshot.OwnerPlayerId);
	if (!FWBGameStateData::IsValidPlayerId(WinningPlayerId))
	{
		return MakeDeathResolutionFailure(TEXT("deferred_hero_owner_invalid"));
	}
	State.bGameOver = true;
	State.WinnerPlayerId = WinningPlayerId;
	State.TerminalOutcome.bTerminal = true;
	State.TerminalOutcome.WinnerPlayerId = WinningPlayerId;
	State.TerminalOutcome.LoserPlayerId = Snapshot.OwnerPlayerId;
	State.TerminalOutcome.Reason =
		EWBTerminalReason::HeroDefeatedWithoutReplacement;
	State.TerminalOutcome.Source = TerminalSource;
	State.TerminalOutcome.TurnNumber = State.TurnNumber;

	FWBApplyActionResult Result;
	Result.bOk = true;
	Result.TraceEvents.Add(MakeHeroDefeatedTrace(
		*Unit, WinningPlayerId, Snapshot.ResolutionOrder));
	return Result;
}

FWBApplyActionResult WBDeathResolution::ApplyZeroHPDeathResolution(
	FWBGameStateData& State,
	const EWBUnitDestructionCause Cause)
{
	const FWBActionQueryResult CleanupQuery = WBRules::CanApplyZeroHPDeathRemoval(State);
	if (!CleanupQuery.bOk)
	{
		return MakeDeathResolutionFailure(CleanupQuery.Reason);
	}

	FString ValidationReason;
	if (!ValidateDeathCleanupZoneState(State, ValidationReason))
	{
		return MakeDeathResolutionFailure(ValidationReason);
	}

	TArray<FWBDeathCleanupUnitPlan> UnitsToRemove;
	TSet<int32> LosingHeroOwnerIds;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (WBRules::ShouldUnitBeDefeatedAtZeroHP(State, Unit))
		{
			FWBDeathCleanupUnitPlan Plan;
			Plan.UnitId = Unit.UnitId;
			UnitsToRemove.Add(Plan);

			if (IsHeroUnitForOwner(State, Unit))
			{
				LosingHeroOwnerIds.Add(Unit.GetOwnerPlayerIdForRules());
			}
		}
	}

	if (UnitsToRemove.Num() <= 0)
	{
		return MakeDeathResolutionFailure(TEXT("no_zero_hp_units"));
	}

	if (LosingHeroOwnerIds.Num() > 1)
	{
		return MakeDeathResolutionFailure(TEXT("simultaneous_hero_death_unsupported"));
	}

	UnitsToRemove.Sort(UnitIdLess);

	FWBGameStateData WorkingState = State;
	TArray<FWBTraceEvent> TraceEvents;
	int32 ResolutionOrder = 0;
	for (const FWBDeathCleanupUnitPlan& Plan : UnitsToRemove)
	{
		FWBUnitState* Unit = WorkingState.GetMutableUnitById(Plan.UnitId);
		if (Unit == nullptr || !WBRules::ShouldUnitBeDefeatedAtZeroHP(WorkingState, *Unit))
		{
			return MakeDeathResolutionFailure(TEXT("death_candidate_missing"));
		}

		FWBUnitDestructionRequest Request;
		Request.TargetUnitId = Plan.UnitId;
		Request.Cause = Cause;
		Request.ResolutionOrder = ResolutionOrder;
		const FWBUnitDestructionResult Destroyed =
			ApplyGenuineUnitDestruction(WorkingState, Request);
		if (!Destroyed.bOk)
		{
			return MakeDeathResolutionFailure(Destroyed.Reason);
		}
		if (Destroyed.bDestroyed)
		{
			TraceEvents.Append(Destroyed.TraceEvents);
			++ResolutionOrder;
		}
	}

	if (!ValidateDeathCleanupZoneState(WorkingState, ValidationReason))
	{
		return MakeDeathResolutionFailure(ValidationReason);
	}

	State = WorkingState;

	FWBApplyActionResult Result;
	Result.bOk = true;
	Result.TraceEvents = MoveTemp(TraceEvents);
	return Result;
}

FWBApplyActionResult WBDeathResolution::ApplyExplicitUnitDestruction(
	FWBGameStateData& State,
	const int32 UnitId)
{
	FWBUnitDestructionRequest Request;
	Request.TargetUnitId = UnitId;
	Request.Cause = EWBUnitDestructionCause::ExplicitDestroy;
	Request.TerminalSource = EWBTerminalSource::Effect;
	const FWBUnitDestructionResult Destroyed =
		ApplyGenuineUnitDestruction(State, Request);
	FWBApplyActionResult Result;
	Result.bOk = Destroyed.bOk;
	Result.Reason = Destroyed.Reason;
	Result.TraceEvents = Destroyed.TraceEvents;
	return Result;
}
