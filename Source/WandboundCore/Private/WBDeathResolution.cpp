#include "WBDeathResolution.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
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
	const FWBPlayerStateData* Player = State.GetPlayerById(Unit.OwnerId);
	return Player != nullptr && Player->HeroUnitId == Unit.UnitId;
}

FWBDeathResolutionCandidate MakeDeathResolutionCandidate(
	const FWBGameStateData& State,
	const FWBUnitState& Unit)
{
	FWBDeathResolutionCandidate Candidate;
	Candidate.UnitId = Unit.UnitId;
	Candidate.OwnerId = Unit.OwnerId;
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
	Event.PlayerId = Unit.OwnerId;
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
	Event.PlayerId = Unit.OwnerId;
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
	Event.PlayerId = Unit.OwnerId;
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

FWBApplyActionResult WBDeathResolution::ApplyZeroHPDeathResolution(FWBGameStateData& State)
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
				LosingHeroOwnerIds.Add(Unit.OwnerId);
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

		const bool bHeroUnit = IsHeroUnitForOwner(WorkingState, *Unit);
		const int32 PreviousHP = Unit->HP;
		const FWBTile PreviousTile(Unit->X, Unit->Y);
		const FWBDeathPreventionResult DeathPrevention = EvaluateDeathPrevention(
			WorkingState,
			MakeDeathResolutionCandidate(WorkingState, *Unit));
		if (DeathPrevention.bPrevented)
		{
			continue;
		}

		const TArray<FWBEquippedCardEntry> EquippedCards = CollectEquippedCardsForUnit(WorkingState, Plan.UnitId);
		for (const FWBEquippedCardEntry& Entry : EquippedCards)
		{
			const FWBCardLifecycleResult MoveResult =
				WBCardLifecycle::MoveEquippedCardToDiscard(
					WorkingState,
					Entry.Card.OwnerPlayerId,
					Entry.Card.InstanceId);
			if (!MoveResult.bOk)
			{
				return MakeDeathResolutionFailure(WBCardLifecycle::ResultCodeToString(MoveResult.Code));
			}

			const int32 DiscardIndex = FindDiscardIndexForCard(
				WorkingState,
				Entry.Card.OwnerPlayerId,
				Entry.Card.InstanceId);
			if (DiscardIndex == INDEX_NONE)
			{
				return MakeDeathResolutionFailure(TEXT("discarded_card_not_found"));
			}

			TraceEvents.Add(MakeEquippedCardDiscardedOnDeathTrace(
				Entry,
				DiscardIndex,
				Plan.UnitId,
				bHeroUnit,
				ResolutionOrder));
		}

		Unit = WorkingState.GetMutableUnitById(Plan.UnitId);
		if (Unit == nullptr)
		{
			return MakeDeathResolutionFailure(TEXT("death_candidate_missing"));
		}

		Unit->ResonanceModifiers.Reset();
		Unit->SetCanonicalRL(Unit->GetBaseRLForRules(), Unit->GetBaseRLForRules(), 0);
		Unit->MarkUnitDefeated();
		Unit->RemoveUnitFromBoard();
		ClearPendingAttackIfUnitRemoved(WorkingState, Plan.UnitId);

		TraceEvents.Add(MakeUnitDefeatedTrace(*Unit, PreviousHP, bHeroUnit, ResolutionOrder));
		TraceEvents.Add(MakeUnitRemovedFromBoardTrace(*Unit, PreviousTile, bHeroUnit, ResolutionOrder));

		if (bHeroUnit)
		{
			const int32 WinningPlayerId = OpposingPlayerId(Unit->OwnerId);
			WorkingState.bGameOver = true;
			WorkingState.WinnerPlayerId = WinningPlayerId;
			TraceEvents.Add(MakeHeroDefeatedTrace(*Unit, WinningPlayerId, ResolutionOrder));
		}

		++ResolutionOrder;
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
