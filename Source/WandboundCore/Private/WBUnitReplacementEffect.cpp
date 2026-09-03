#include "WBUnitReplacementEffect.h"

#include "WBCardZoneState.h"
#include "WBCSNInheritance.h"
#include "WBDeathResolution.h"
#include "WBEffectRunner.h"

namespace
{
constexpr int32 BoardSize = 9;
constexpr int32 MaxOwnedUnitsIncludingHero = 4;

FWBApplyActionResult Fail(const FString& Reason)
{
	FWBApplyActionResult Result;
	Result.Reason = Reason;
	return Result;
}

bool IsValidCharacterDefinition(const FWBCardDefinition& Definition)
{
	return Definition.Kind == EWBCardDefinitionKind::Character
		&& Definition.CharacterStats.HP > 0
		&& Definition.CharacterStats.ATK >= 0
		&& Definition.CharacterStats.AR >= 0
		&& Definition.CharacterStats.RL >= 0;
}

int32 AllocateUnitId(const FWBGameStateData& State)
{
	int32 MaxUnitId = -1;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId == MAX_int32)
		{
			return INDEX_NONE;
		}
		MaxUnitId = FMath::Max(MaxUnitId, Unit.UnitId);
	}
	return MaxUnitId + 1;
}

int32 OpponentOf(const int32 PlayerId)
{
	return PlayerId == 0 ? 1 : (PlayerId == 1 ? 0 : -1);
}

bool CrashInEquippedEntryLess(
	const FWBEquippedCardEntry& A,
	const FWBEquippedCardEntry& B)
{
	if (A.EquipOrder != B.EquipOrder)
	{
		return A.EquipOrder < B.EquipOrder;
	}
	if (A.SlotId != B.SlotId)
	{
		return A.SlotId < B.SlotId;
	}
	return A.Card.InstanceId < B.Card.InstanceId;
}
}

FWBApplyActionResult WBUnitReplacementEffect::ApplyPendingAttackDefenderReplacement(
	FWBGameStateData& State,
	const FWBEffectRequest& Request,
	const FWBGenericEffectPayload& Payload,
	const FWBCardDefinitionRepository& Repository)
{
	if (!State.HasPendingAttack()
		|| State.PendingAttack.Stage != EWBAttackContinuationStage::PreHit)
	{
		return Fail(TEXT("pending_attack_not_pre_hit"));
	}
	if (Payload.PendingAttackContinuationId.IsEmpty()
		|| Payload.PendingAttackContinuationId != State.PendingAttack.ContinuationId)
	{
		return Fail(TEXT("pending_attack_target_mismatch"));
	}
	if (Request.Target.TargetUnitId != State.PendingAttack.DefenderUnitId)
	{
		return Fail(TEXT("replacement_source_not_current_defender"));
	}
	if (!FWBGameStateData::IsValidPlayerId(Request.Source.PlayerId))
	{
		return Fail(TEXT("invalid_effect_source_player"));
	}
	if (Request.AuxiliaryCardSelection.Zone != EWBEffectAuxiliaryCardZone::Hand
		|| Request.AuxiliaryCardSelection.CardInstanceId.IsEmpty()
		|| Request.AuxiliaryCardSelection.CardId.IsEmpty())
	{
		return Fail(TEXT("auxiliary_hand_card_selection_missing"));
	}
	if (Payload.RequiredReplacementKind != EWBEffectReplacementCardKind::Character
		|| Payload.InheritancePolicy !=
			EWBEffectInheritancePolicy::TransferEquippedWandsAndAddSourceCurrentRL)
	{
		return Fail(TEXT("unsupported_replacement_policy"));
	}

	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(
		State.GetCardZoneState(), ZoneReason))
	{
		return Fail(ZoneReason.IsEmpty() ? TEXT("invalid_zone_state") : ZoneReason);
	}

	const FWBUnitState* SourceUnit = State.GetUnitById(Request.Target.TargetUnitId);
	if (SourceUnit == nullptr || !SourceUnit->IsUnitOnBoard() || SourceUnit->bDefeated)
	{
		return Fail(TEXT("replacement_source_unavailable"));
	}
	if (SourceUnit->GetControllerPlayerIdForRules() != Request.Source.PlayerId)
	{
		return Fail(TEXT("replacement_source_owner_mismatch"));
	}
	FWBDeathResolutionCandidate DeathCandidate;
	DeathCandidate.UnitId = SourceUnit->UnitId;
	DeathCandidate.OwnerId = SourceUnit->GetControllerPlayerIdForRules();
	DeathCandidate.bIsHero = State.GetPlayerById(
		SourceUnit->GetOwnerPlayerIdForRules()) != nullptr
		&& State.GetPlayerById(SourceUnit->GetOwnerPlayerIdForRules())->HeroUnitId
			== SourceUnit->UnitId;
	const FWBDeathPreventionResult DeathPrevention =
		WBDeathResolution::EvaluateDeathPrevention(State, DeathCandidate);
	if (DeathPrevention.bPrevented)
	{
		return Fail(TEXT("replacement_source_destruction_prevented"));
	}
	const FWBCardDefinitionRepositoryLookupResult SourceDefinition =
		WBCardDefinitionRepository::FindCardById(Repository, SourceUnit->CardId);
	if (!SourceDefinition.bFound
		|| (!Payload.RequiredSourceFaction.IsEmpty()
			&& !SourceDefinition.Definition.PublicFactions.Contains(
				Payload.RequiredSourceFaction)))
	{
		return Fail(TEXT("replacement_source_faction_mismatch"));
	}

	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), Request.Source.PlayerId);
	if (PlayerZones == nullptr)
	{
		return Fail(TEXT("player_zones_missing"));
	}
	int32 HandIndex = INDEX_NONE;
	for (int32 Index = 0; Index < PlayerZones->Hand.Num(); ++Index)
	{
		const FWBZoneCardEntry& Entry = PlayerZones->Hand[Index];
		if (Entry.Card.InstanceId == Request.AuxiliaryCardSelection.CardInstanceId)
		{
			HandIndex = Index;
			if (Entry.Card.CardId != Request.AuxiliaryCardSelection.CardId
				|| Entry.Card.OwnerPlayerId != Request.Source.PlayerId)
			{
				return Fail(TEXT("auxiliary_hand_card_selection_mismatch"));
			}
			break;
		}
	}
	if (HandIndex == INDEX_NONE)
	{
		return Fail(TEXT("selected_replacement_not_in_hand"));
	}
	const int32 ReplacementOwnerPlayerId =
		PlayerZones->Hand[HandIndex].Card.OwnerPlayerId;

	const FWBCardDefinitionRepositoryLookupResult ReplacementDefinition =
		WBCardDefinitionRepository::FindCardById(
			Repository, Request.AuxiliaryCardSelection.CardId);
	if (!ReplacementDefinition.bFound
		|| !IsValidCharacterDefinition(ReplacementDefinition.Definition))
	{
		return Fail(TEXT("replacement_definition_invalid"));
	}
	if (!Payload.RequiredReplacementFaction.IsEmpty()
		&& !ReplacementDefinition.Definition.PublicFactions.Contains(
			Payload.RequiredReplacementFaction))
	{
		return Fail(TEXT("replacement_faction_mismatch"));
	}
	if (State.GetUnitsForPlayer(Request.Source.PlayerId).Num()
		> MaxOwnedUnitsIncludingHero)
	{
		return Fail(TEXT("unit_cap_exceeded"));
	}

	const FWBTile VacatedTile(SourceUnit->X, SourceUnit->Y);
	if (VacatedTile.X < 0 || VacatedTile.X >= BoardSize
		|| VacatedTile.Y < 0 || VacatedTile.Y >= BoardSize)
	{
		return Fail(TEXT("replacement_tile_out_of_bounds"));
	}
	const int32 SourceCurrentRL = SourceUnit->GetCurrentRLForRules();
	if (SourceCurrentRL < 0
		|| ReplacementDefinition.Definition.CharacterStats.RL
			> MAX_int32 - SourceCurrentRL)
	{
		return Fail(TEXT("replacement_rl_invalid"));
	}
	const int32 NewUnitId = AllocateUnitId(State);
	if (NewUnitId < 0)
	{
		return Fail(TEXT("unit_id_allocation_failed"));
	}

	TArray<FWBEquippedCardEntry> InheritedWands;
	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		if (Entry.EquippedToUnitId != SourceUnit->UnitId)
		{
			continue;
		}
		const FWBCardDefinitionRepositoryLookupResult WandDefinition =
			WBCardDefinitionRepository::FindCardById(Repository, Entry.Card.CardId);
		if (!WandDefinition.bFound
			|| WandDefinition.Definition.Kind != EWBCardDefinitionKind::Wand)
		{
			return Fail(TEXT("inherited_equipment_not_wand"));
		}
		InheritedWands.Add(Entry);
	}
	InheritedWands.Sort(CrashInEquippedEntryLess);
	FWBUnitDestructionSnapshot DestructionSnapshot;
	if (!WBDeathResolution::BuildSuccessfulDestructionSnapshot(
		State,
		SourceUnit->UnitId,
		EWBUnitDestructionCause::ReplacementEffect,
		0,
		DestructionSnapshot,
		ZoneReason))
	{
		return Fail(ZoneReason);
	}

	FWBGameStateData WorkingState = State;
	FWBPlayerCardZoneState* MutableZones = WBCardZoneState::FindMutablePlayerZones(
		WorkingState.GetMutableCardZoneStateForTest(), Request.Source.PlayerId);
	FWBUnitState* MutableSource = WorkingState.GetMutableUnitById(SourceUnit->UnitId);
	if (MutableZones == nullptr || MutableSource == nullptr
		|| !MutableZones->Hand.IsValidIndex(HandIndex)
		|| MutableZones->Hand[HandIndex].Card.InstanceId
			!= Request.AuxiliaryCardSelection.CardInstanceId)
	{
		return Fail(TEXT("replacement_plan_stale"));
	}

	const bool bSourceWasHero = WorkingState.GetPlayerById(Request.Source.PlayerId)
		!= nullptr
		&& WorkingState.GetPlayerById(Request.Source.PlayerId)->HeroUnitId
			== MutableSource->UnitId;
	const int32 PreviousSourceHP = MutableSource->HP;
	MutableZones->Hand.RemoveAt(HandIndex, 1, EAllowShrinking::No);
	for (int32 Index = 0; Index < MutableZones->Hand.Num(); ++Index)
	{
		MutableZones->Hand[Index].ZoneIndex = Index;
	}

	MutableSource->HP = 0;
	MutableSource->ResonanceModifiers.Reset();
	MutableSource->SetCanonicalRL(
		MutableSource->GetBaseRLForRules(),
		MutableSource->GetBaseRLForRules(),
		0);
	MutableSource->MarkUnitDefeated();
	MutableSource->RemoveUnitFromBoard();

	FWBUnitState Replacement;
	Replacement.UnitId = NewUnitId;
	Replacement.SetOwnerAndControllerForRules(
		ReplacementOwnerPlayerId,
		Request.Source.PlayerId);
	Replacement.CardId = Request.AuxiliaryCardSelection.CardId;
	Replacement.X = VacatedTile.X;
	Replacement.Y = VacatedTile.Y;
	Replacement.HP = ReplacementDefinition.Definition.CharacterStats.HP;
	Replacement.MaxHP = ReplacementDefinition.Definition.CharacterStats.HP;
	Replacement.ATK = ReplacementDefinition.Definition.CharacterStats.ATK;
	Replacement.AR = ReplacementDefinition.Definition.CharacterStats.AR;
	Replacement.BaseRL = ReplacementDefinition.Definition.CharacterStats.RL;
	Replacement.CurrentRL = Replacement.BaseRL;
	Replacement.RLTotal = Replacement.BaseRL;
	Replacement.RLUsed = 0;
	Replacement.AttacksLeft = 0;
	Replacement.MaxAttacksPerTurn = 1;
	Replacement.MPRemaining = 0;
	WorkingState.Units.Add(Replacement);

	const FWBApplyActionResult RedirectResult =
		WBEffectRunner::ApplyPendingAttackRedirect(
			WorkingState,
			Payload.PendingAttackContinuationId,
			NewUnitId);
	if (!RedirectResult.bOk)
	{
		return Fail(RedirectResult.Reason);
	}

	if (bSourceWasHero)
	{
		const int32 Winner = OpponentOf(Request.Source.PlayerId);
		WorkingState.bGameOver = true;
		WorkingState.WinnerPlayerId = Winner;
		WorkingState.TerminalOutcome.bTerminal = true;
		WorkingState.TerminalOutcome.WinnerPlayerId = Winner;
		WorkingState.TerminalOutcome.LoserPlayerId = Request.Source.PlayerId;
		WorkingState.TerminalOutcome.Reason =
			EWBTerminalReason::HeroDefeatedWithoutReplacement;
		WorkingState.TerminalOutcome.Source = EWBTerminalSource::Effect;
		WorkingState.TerminalOutcome.TurnNumber = WorkingState.TurnNumber;
	}

	FWBCSNInheritanceMutationRequest InheritanceRequest;
	InheritanceRequest.SourceSnapshot =
		DestructionSnapshot.DestroyedUnitSnapshot;
	InheritanceRequest.ControllerPlayerId = Request.Source.PlayerId;
	InheritanceRequest.SourceUnitId = SourceUnit->UnitId;
	InheritanceRequest.TargetUnitId = NewUnitId;
	InheritanceRequest.SourceCurrentRL = SourceCurrentRL;
	InheritanceRequest.EquippedWandSnapshot = InheritedWands;
	InheritanceRequest.ExpectedWandLocation =
		EWBCSNInheritanceWandLocation::EquippedToSource;
	InheritanceRequest.TransactionId = Payload.PendingAttackContinuationId;
	const FWBCSNInheritanceMutationResult InheritanceResult =
		WBCSNInheritance::Apply(
			WorkingState, Repository, InheritanceRequest);
	if (!InheritanceResult.bOk)
	{
		return Fail(InheritanceResult.Reason);
	}
	WBDeathResolution::QueueSuccessfulDestructionEvent(
		WorkingState,
		MoveTemp(DestructionSnapshot));
	if (!WBCardZoneState::ValidateZoneStateForTest(
		WorkingState.GetCardZoneState(), ZoneReason))
	{
		return Fail(ZoneReason.IsEmpty() ? TEXT("invalid_zone_state") : ZoneReason);
	}

	FWBApplyActionResult Result;
	Result.bOk = true;

	FWBTraceEvent Defeated;
	Defeated.Kind = FName(TEXT("unit_defeated"));
	Defeated.PlayerId = Request.Source.PlayerId;
	Defeated.TargetUnitId = SourceUnit->UnitId;
	Defeated.PreviousHP = PreviousSourceHP;
	Defeated.NewHP = 0;
	Defeated.bHeroUnit = bSourceWasHero;
	Defeated.bAtOrBelowZeroHP = true;
	Defeated.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Defeated));

	FWBTraceEvent Removed;
	Removed.Kind = FName(TEXT("unit_removed_from_board"));
	Removed.PlayerId = Request.Source.PlayerId;
	Removed.TargetUnitId = SourceUnit->UnitId;
	Removed.FromTile = VacatedTile;
	Removed.bHeroUnit = bSourceWasHero;
	Removed.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Removed));

	FWBTraceEvent Summoned;
	Summoned.Kind = FName(TEXT("effect_replacement_summon"));
	Summoned.PlayerId = Request.Source.PlayerId;
	Summoned.SourceUnitId = SourceUnit->UnitId;
	Summoned.TargetUnitId = NewUnitId;
	Summoned.CardInstanceId = Request.AuxiliaryCardSelection.CardInstanceId;
	Summoned.CardId = Request.AuxiliaryCardSelection.CardId;
	Summoned.ToTile = VacatedTile;
	Summoned.AttackContinuationId = Payload.PendingAttackContinuationId;
	Summoned.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Summoned));

	Result.TraceEvents.Append(InheritanceResult.TraceEvents);
	Result.TraceEvents.Append(RedirectResult.TraceEvents);

	if (bSourceWasHero)
	{
		FWBTraceEvent HeroDefeated;
		HeroDefeated.Kind = FName(TEXT("hero_defeated"));
		HeroDefeated.PlayerId = Request.Source.PlayerId;
		HeroDefeated.TargetUnitId = SourceUnit->UnitId;
		HeroDefeated.WinningPlayerId = OpponentOf(Request.Source.PlayerId);
		HeroDefeated.bHeroUnit = true;
		HeroDefeated.bOk = true;
		Result.TraceEvents.Add(MoveTemp(HeroDefeated));
	}

	State = MoveTemp(WorkingState);
	return Result;
}
