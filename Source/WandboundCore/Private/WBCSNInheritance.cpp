#include "WBCSNInheritance.h"

#include "WBCSNInheritanceTrigger.h"
#include "WBCardZoneState.h"
#include "WBResonanceOverflow.h"
#include "WBResonanceRecalculation.h"

namespace
{
FWBCSNInheritanceMutationResult MakeCSNInheritanceFailure(const FString& Reason)
{
	FWBCSNInheritanceMutationResult Result;
	Result.Reason = Reason;
	return Result;
}

void NormalizeCSNInheritanceZoneIndexes(TArray<FWBZoneCardEntry>& Entries)
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index].ZoneIndex = Index;
	}
}

bool CSNInheritanceWandLess(const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
{
	if (A.EquipOrder != B.EquipOrder) return A.EquipOrder < B.EquipOrder;
	if (A.SlotId != B.SlotId) return A.SlotId < B.SlotId;
	return A.Card.InstanceId < B.Card.InstanceId;
}
}

FWBCSNInheritanceMutationResult WBCSNInheritance::Apply(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBCSNInheritanceMutationRequest& Request)
{
	if (!FWBGameStateData::IsValidPlayerId(Request.ControllerPlayerId)
		|| Request.SourceUnitId < 0
		|| Request.TargetUnitId < 0
		|| Request.SourceCurrentRL < 0
		|| Request.TransactionId.IsEmpty())
	{
		return MakeCSNInheritanceFailure(TEXT("csn_inheritance_context_invalid"));
	}
	const FWBUnitState* Target = State.GetUnitById(Request.TargetUnitId);
	if (Target == nullptr
		|| Target->GetControllerPlayerIdForRules() != Request.ControllerPlayerId
		|| !Target->IsUnitOnBoard() || Target->bDefeated)
	{
		return MakeCSNInheritanceFailure(TEXT("csn_inheritance_target_invalid"));
	}
	if (Target->GetBaseRLForRules() > MAX_int32 - Request.SourceCurrentRL)
	{
		return MakeCSNInheritanceFailure(TEXT("csn_inheritance_rl_invalid"));
	}

	TArray<FWBEquippedCardEntry> Wands = Request.EquippedWandSnapshot;
	Wands.Sort(CSNInheritanceWandLess);
	for (const FWBEquippedCardEntry& Wand : Wands)
	{
		const FWBCardDefinitionRepositoryLookupResult Definition =
			WBCardDefinitionRepository::FindCardById(Repository, Wand.Card.CardId);
		if (!Definition.bFound
			|| Definition.Definition.Kind != EWBCardDefinitionKind::Wand
			|| Wand.Card.OwnerPlayerId != Request.ControllerPlayerId
			|| Wand.Card.InstanceId.IsEmpty())
		{
			return MakeCSNInheritanceFailure(TEXT("inherited_equipment_not_wand"));
		}
	}

	FWBGameStateData WorkingState = State;
	FWBCardZoneState& Zones = WorkingState.GetMutableCardZoneStateForTest();
	for (const FWBEquippedCardEntry& Snapshot : Wands)
	{
		if (Request.ExpectedWandLocation
			== EWBCSNInheritanceWandLocation::EquippedToSource)
		{
			FWBEquippedCardEntry* Live = Zones.EquippedCards.FindByPredicate(
				[&Snapshot](const FWBEquippedCardEntry& Entry)
				{
					return Entry.Card.InstanceId == Snapshot.Card.InstanceId;
				});
			if (Live == nullptr
				|| Live->EquippedToUnitId != Request.SourceUnitId
				|| Live->Card.CardId != Snapshot.Card.CardId)
			{
				return MakeCSNInheritanceFailure(TEXT("inherited_wand_unavailable"));
			}
			Live->EquippedToUnitId = Request.TargetUnitId;
		}
		else
		{
			if (Request.ExpectedWandLocation
				== EWBCSNInheritanceWandLocation::DetachedSourceSnapshot)
			{
				FWBZoneCardEntry Existing;
				if (WBCardZoneState::FindCardByInstanceId(
					Zones, Snapshot.Card.InstanceId, Existing)
					|| Zones.EquippedCards.ContainsByPredicate(
						[&Snapshot](const FWBEquippedCardEntry& Entry)
						{
							return Entry.Card.InstanceId
								== Snapshot.Card.InstanceId;
						}))
				{
					return MakeCSNInheritanceFailure(
						TEXT("inherited_wand_already_zoned"));
				}
				FWBEquippedCardEntry Equipped = Snapshot;
				Equipped.EquippedToUnitId = Request.TargetUnitId;
				Zones.EquippedCards.Add(MoveTemp(Equipped));
				continue;
			}
			FWBPlayerCardZoneState* PlayerZones =
				WBCardZoneState::FindMutablePlayerZones(
					Zones, Request.ControllerPlayerId);
			if (PlayerZones == nullptr)
			{
				return MakeCSNInheritanceFailure(TEXT("player_zones_missing"));
			}
			const int32 DiscardIndex = PlayerZones->Discard.IndexOfByPredicate(
				[&Snapshot](const FWBZoneCardEntry& Entry)
				{
					return Entry.Card.InstanceId == Snapshot.Card.InstanceId;
				});
			if (DiscardIndex == INDEX_NONE
				|| PlayerZones->Discard[DiscardIndex].Card.CardId
					!= Snapshot.Card.CardId)
			{
				return MakeCSNInheritanceFailure(TEXT("inherited_wand_unavailable"));
			}
			PlayerZones->Discard.RemoveAt(
				DiscardIndex, 1, EAllowShrinking::No);
			NormalizeCSNInheritanceZoneIndexes(PlayerZones->Discard);
			FWBEquippedCardEntry Equipped = Snapshot;
			Equipped.EquippedToUnitId = Request.TargetUnitId;
			Zones.EquippedCards.Add(MoveTemp(Equipped));
		}
	}

	FWBUnitState* MutableTarget = WorkingState.GetMutableUnitById(
		Request.TargetUnitId);
	if (MutableTarget == nullptr)
	{
		return MakeCSNInheritanceFailure(TEXT("csn_inheritance_target_invalid"));
	}
	const int32 PreviousBaseRL = MutableTarget->GetBaseRLForRules();
	MutableTarget->SetCanonicalRL(
		PreviousBaseRL + Request.SourceCurrentRL,
		PreviousBaseRL + Request.SourceCurrentRL,
		0);
	WBCardZoneState::SortOrderedZonesDeterministically(Zones);

	const FWBResonanceRecalculationResult RLResult =
		WBResonanceRecalculation::RecalculateUnit(
			WorkingState, Request.TargetUnitId, Repository);
	if (!RLResult.bSucceeded)
	{
		return MakeCSNInheritanceFailure(RLResult.FailureReason);
	}
	const FWBResonanceOverflowResult Overflow =
		WBResonanceOverflow::ResolveOverflowForUnit(
			WorkingState, Repository, Request.TargetUnitId);
	if (!Overflow.bOk)
	{
		return MakeCSNInheritanceFailure(Overflow.Reason);
	}

	FWBCSNInheritanceEventContext TriggerContext;
	TriggerContext.EventIdentity = WBEventSnapshot::MakeIdentity(
		EWBEventKind::Inheritance,
		Request.TransactionId,
		WorkingState.TurnNumber,
		FString(),
		Request.TransactionId);
	TriggerContext.SourceSnapshot = Request.SourceSnapshot;
	TriggerContext.InheritingSnapshot =
		WBEventSnapshot::CaptureUnitParticipant(WorkingState, *MutableTarget);
	TriggerContext.EligibilityPolicy = EWBTriggerEligibilityPolicy::Hybrid;
	TriggerContext.InheritingUnitId = Request.TargetUnitId;
	TriggerContext.InheritingPlayerId = Request.ControllerPlayerId;
	TriggerContext.SourceUnitId = Request.SourceUnitId;
	TriggerContext.SourceCurrentRL = Request.SourceCurrentRL;
	TriggerContext.InheritedWandCount = Wands.Num();
	TriggerContext.TransactionId = Request.TransactionId;
	const FWBCSNInheritanceTriggerResult Triggers =
		WBCSNInheritanceTrigger::ResolveAfterSuccessfulInheritance(
			WorkingState, Repository, TriggerContext);
	if (!Triggers.bOk)
	{
		return MakeCSNInheritanceFailure(Triggers.Reason);
	}

	FWBCSNInheritanceMutationResult Result;
	Result.bOk = true;
	for (const FWBEquippedCardEntry& Wand : Wands)
	{
		FWBTraceEvent Transferred;
		Transferred.Kind = FName(TEXT("inherited_wand_transferred"));
		Transferred.PlayerId = Request.ControllerPlayerId;
		Transferred.SourceUnitId = Request.SourceUnitId;
		Transferred.TargetUnitId = Request.TargetUnitId;
		Transferred.CardInstanceId = Wand.Card.InstanceId;
		Transferred.CardId = Wand.Card.CardId;
		Transferred.SlotId = Wand.SlotId;
		Transferred.EquipOrder = Wand.EquipOrder;
		Transferred.bOk = true;
		Result.TraceEvents.Add(MoveTemp(Transferred));
	}
	const FWBUnitState* FinalTarget = WorkingState.GetUnitById(
		Request.TargetUnitId);
	FWBTraceEvent Inheritance;
	Inheritance.Kind = FName(TEXT("csn_inheritance"));
	Inheritance.PlayerId = Request.ControllerPlayerId;
	Inheritance.SourceUnitId = Request.SourceUnitId;
	Inheritance.TargetUnitId = Request.TargetUnitId;
	Inheritance.CardCount = Wands.Num();
	Inheritance.PreviousBaseRL = PreviousBaseRL;
	Inheritance.NewBaseRL = FinalTarget != nullptr
		? FinalTarget->GetBaseRLForRules() : -1;
	Inheritance.PreviousCurrentRL = Request.SourceCurrentRL;
	Inheritance.NewCurrentRL = FinalTarget != nullptr
		? FinalTarget->GetCurrentRLForRules() : -1;
	Inheritance.InheritedRL = Request.SourceCurrentRL;
	Inheritance.NewRLUsed = FinalTarget != nullptr ? FinalTarget->RLUsed : -1;
	Inheritance.AttackContinuationId = Request.TransactionId;
	Inheritance.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Inheritance));
	Result.TraceEvents.Append(Triggers.TraceEvents);
	State = MoveTemp(WorkingState);
	return Result;
}
