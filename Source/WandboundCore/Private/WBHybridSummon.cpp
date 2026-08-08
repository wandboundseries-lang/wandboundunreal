#include "WBHybridSummon.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBRules.h"
#include "WBTurnOneRestrictions.h"

namespace
{
constexpr int32 MaxOwnedUnitsIncludingHero = 4;

FWBHybridSummonPlanResult MakePlanFailure(
	const EWBHybridSummonResultCode Code)
{
	FWBHybridSummonPlanResult Result;
	Result.Code = Code;
	Result.Reason = WBHybridSummon::ResultCodeToString(Code);
	return Result;
}

FWBHybridSummonResult MakeExecutionFailure(
	const EWBHybridSummonResultCode Code,
	const FWBHybridSummonPlan& Plan)
{
	FWBHybridSummonResult Result;
	Result.Code = Code;
	Result.Reason = WBHybridSummon::ResultCodeToString(Code);
	Result.Plan = Plan;
	return Result;
}

bool IsSupportedHybridDefinition(const FWBCardDefinition& Definition)
{
	return Definition.Kind == EWBCardDefinitionKind::Hybrid
		&& Definition.CharacterStats.HP > 0
		&& Definition.CharacterStats.ATK >= 0
		&& Definition.CharacterStats.AR >= 0
		&& Definition.CharacterStats.RL >= 0
		&& Definition.HybridSummon.SacrificeCount == 1
		&& Definition.HybridSummon.SacrificeRequirement
			== FName(TEXT("controlled_character"))
		&& Definition.HybridSummon.WandPaymentCount == 1
		&& Definition.HybridSummon.WandPaymentSources.Contains(
			FName(TEXT("hand")))
		&& Definition.HybridSummon.WandPaymentSources.Contains(
			FName(TEXT("sacrificed_unit")))
		&& Definition.HybridSummon.HeroDestination
			== FName(TEXT("sacrificed_hero_tile"))
		&& Definition.HybridSummon.NonHeroDestination
			== FName(TEXT("adjacent_to_hero"));
}

bool IsValidOwnedHero(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const FWBUnitState*& OutHero)
{
	OutHero = nullptr;
	const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
	if (Player == nullptr || Player->HeroUnitId < 0)
	{
		return false;
	}
	const FWBUnitState* Hero = State.GetUnitById(Player->HeroUnitId);
	if (Hero == nullptr || Hero->OwnerId != PlayerId || !Hero->IsUnitOnBoard())
	{
		return false;
	}
	OutHero = Hero;
	return true;
}

bool PaymentEntryLess(const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
{
	if (A.ZoneIndex != B.ZoneIndex) return A.ZoneIndex < B.ZoneIndex;
	if (A.Card.InstanceId != B.Card.InstanceId)
		return A.Card.InstanceId < B.Card.InstanceId;
	return A.Card.CardId < B.Card.CardId;
}

bool EquippedEntryLess(const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
{
	if (A.EquipOrder != B.EquipOrder) return A.EquipOrder < B.EquipOrder;
	if (A.SlotId != B.SlotId) return A.SlotId < B.SlotId;
	return A.Card.InstanceId < B.Card.InstanceId;
}

bool IsWandDefinition(
	const FWBCardDefinitionRepository& Repository,
	const FString& CardId)
{
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, CardId);
	return Lookup.bFound
		&& Lookup.Definition.Kind == EWBCardDefinitionKind::Wand;
}

bool PlansEqual(const FWBHybridSummonPlan& A, const FWBHybridSummonPlan& B)
{
	return A.ActingPlayerId == B.ActingPlayerId
		&& A.HybridCardInstanceId == B.HybridCardInstanceId
		&& A.HybridDefinitionId == B.HybridDefinitionId
		&& A.SacrificedHeroUnitId == B.SacrificedHeroUnitId
		&& A.WandPaymentSource == B.WandPaymentSource
		&& A.WandPaymentCardInstanceId == B.WandPaymentCardInstanceId
		&& A.WandPaymentUnitId == B.WandPaymentUnitId
		&& A.DestinationTile == B.DestinationTile
		&& A.bBecomesReplacementHero == B.bBecomesReplacementHero
		&& A.BeforeGeneration == B.BeforeGeneration
		&& A.BeforeRevision == B.BeforeRevision;
}

int32 AllocateNextUnitId(const FWBGameStateData& State)
{
	int32 MaxUnitId = -1;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId == MAX_int32) return INDEX_NONE;
		MaxUnitId = FMath::Max(MaxUnitId, Unit.UnitId);
	}
	return MaxUnitId + 1;
}

void NormalizeHand(FWBPlayerCardZoneState& PlayerZones)
{
	PlayerZones.Hand.Sort(PaymentEntryLess);
	for (int32 Index = 0; Index < PlayerZones.Hand.Num(); ++Index)
	{
		PlayerZones.Hand[Index].Zone = EWBCardZone::Hand;
		PlayerZones.Hand[Index].ZoneIndex = Index;
		PlayerZones.Hand[Index].Card.OwnerPlayerId = PlayerZones.PlayerId;
	}
}

bool RemoveHybridFromHand(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), PlayerId);
	if (Zones == nullptr) return false;
	const int32 Index = Zones->Hand.IndexOfByPredicate(
		[&InstanceId](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == InstanceId;
		});
	if (Index == INDEX_NONE) return false;
	Zones->Hand.RemoveAt(Index, 1, EAllowShrinking::No);
	NormalizeHand(*Zones);
	WBCardZoneState::SortOrderedZonesDeterministically(
		State.GetMutableCardZoneStateForTest());
	return true;
}

FWBTraceEvent MakeTrace(
	const FName Kind,
	const FWBHybridSummonPlan& Plan,
	const int32 OldHeroUnitId,
	const int32 NewHeroUnitId)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = Plan.ActingPlayerId;
	Event.SourceUnitId = OldHeroUnitId;
	Event.TargetUnitId = NewHeroUnitId;
	Event.CardId = Plan.HybridDefinitionId;
	Event.ToTile = Plan.DestinationTile;
	Event.bHeroUnit = true;
	Event.bOk = true;
	return Event;
}
}

FWBHybridSummonPlanResult WBHybridSummon::BuildHeroReplacementPlans(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ActingPlayerId,
	const FString& HybridCardInstanceId,
	const int32 CoordinatorGeneration,
	const int32 CoordinatorRevision)
{
	if (!FWBGameStateData::IsValidPlayerId(ActingPlayerId)
		|| State.GetPlayerById(ActingPlayerId) == nullptr)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridWrongPlayer);
	}

	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(
		State.GetCardZoneState(), ZoneReason))
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridZoneStateInvalid);
	}

	const FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), ActingPlayerId);
	if (PlayerZones == nullptr)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridZoneStateInvalid);
	}
	const FWBZoneCardEntry* HybridEntry = PlayerZones->Hand.FindByPredicate(
		[&HybridCardInstanceId](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == HybridCardInstanceId;
		});
	if (HybridEntry == nullptr)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridNotInHand);
	}
	if (HybridEntry->Card.OwnerPlayerId != ActingPlayerId)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridWrongPlayer);
	}

	const FWBCardDefinitionRepositoryLookupResult HybridLookup =
		WBCardDefinitionRepository::FindCardById(
			Repository, HybridEntry->Card.CardId);
	if (!HybridLookup.bFound
		|| !IsSupportedHybridDefinition(HybridLookup.Definition))
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridDefinitionInvalid);
	}

	const FWBUnitState* Hero = nullptr;
	if (!IsValidOwnedHero(State, ActingPlayerId, Hero))
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridHeroSacrificeInvalid);
	}
	const FWBCardDefinitionRepositoryLookupResult HeroLookup =
		WBCardDefinitionRepository::FindCardById(Repository, Hero->CardId);
	if (!HeroLookup.bFound
		|| HeroLookup.Definition.Kind != EWBCardDefinitionKind::Character)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridSacrificeInvalid);
	}

	const int32 CompletedUnitCount =
		State.GetUnitsForPlayer(ActingPlayerId).Num() - 1 + 1;
	if (CompletedUnitCount > MaxOwnedUnitsIncludingHero)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridUnitCapExceeded);
	}

	const FWBTile Destination(Hero->X, Hero->Y);
	if (!WBRules::IsTileInBounds(Destination))
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridDestinationInvalid);
	}
	const int32 Occupant = State.UnitIdAt(Destination);
	if (Occupant != Hero->UnitId)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridDestinationOccupied);
	}
	const FWBTurnOneRestrictionQuery TurnOne =
		WBTurnOneRestrictions::QuerySummonPlacement(
			State, ActingPlayerId, Destination);
	if (!TurnOne.bOk)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridDestinationInvalid);
	}

	TArray<FWBHybridSummonPlan> Plans;
	TArray<FWBZoneCardEntry> Hand = PlayerZones->Hand;
	Hand.Sort(PaymentEntryLess);
	for (const FWBZoneCardEntry& Entry : Hand)
	{
		if (Entry.Card.InstanceId == HybridCardInstanceId
			|| !IsWandDefinition(Repository, Entry.Card.CardId))
		{
			continue;
		}
		FWBHybridSummonPlan Plan;
		Plan.ActingPlayerId = ActingPlayerId;
		Plan.HybridCardInstanceId = HybridCardInstanceId;
		Plan.HybridDefinitionId = HybridEntry->Card.CardId;
		Plan.SacrificedHeroUnitId = Hero->UnitId;
		Plan.WandPaymentSource = EWBHybridWandPaymentSource::Hand;
		Plan.WandPaymentCardInstanceId = Entry.Card.InstanceId;
		Plan.DestinationTile = Destination;
		Plan.bBecomesReplacementHero = true;
		Plan.BeforeGeneration = CoordinatorGeneration;
		Plan.BeforeRevision = CoordinatorRevision;
		Plans.Add(MoveTemp(Plan));
	}

	TArray<FWBEquippedCardEntry> Equipped = State.GetCardZoneState().EquippedCards;
	Equipped.Sort(EquippedEntryLess);
	for (const FWBEquippedCardEntry& Entry : Equipped)
	{
		if (Entry.EquippedToUnitId != Hero->UnitId
			|| Entry.Card.OwnerPlayerId != ActingPlayerId
			|| !IsWandDefinition(Repository, Entry.Card.CardId))
		{
			continue;
		}
		FWBHybridSummonPlan Plan;
		Plan.ActingPlayerId = ActingPlayerId;
		Plan.HybridCardInstanceId = HybridCardInstanceId;
		Plan.HybridDefinitionId = HybridEntry->Card.CardId;
		Plan.SacrificedHeroUnitId = Hero->UnitId;
		Plan.WandPaymentSource = EWBHybridWandPaymentSource::SacrificedUnit;
		Plan.WandPaymentCardInstanceId = Entry.Card.InstanceId;
		Plan.WandPaymentUnitId = Hero->UnitId;
		Plan.DestinationTile = Destination;
		Plan.bBecomesReplacementHero = true;
		Plan.BeforeGeneration = CoordinatorGeneration;
		Plan.BeforeRevision = CoordinatorRevision;
		Plans.Add(MoveTemp(Plan));
	}

	if (Plans.IsEmpty())
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridWandPaymentRequired);
	}
	Plans.Sort([](const FWBHybridSummonPlan& A, const FWBHybridSummonPlan& B)
	{
		if (A.WandPaymentSource != B.WandPaymentSource)
		{
			return static_cast<uint8>(A.WandPaymentSource)
				< static_cast<uint8>(B.WandPaymentSource);
		}
		return A.WandPaymentCardInstanceId < B.WandPaymentCardInstanceId;
	});

	FWBHybridSummonPlanResult Result;
	Result.bOk = true;
	Result.Code = EWBHybridSummonResultCode::Success;
	Result.Reason = ResultCodeToString(Result.Code);
	Result.Plans = MoveTemp(Plans);
	return Result;
}

FWBHybridSummonResult WBHybridSummon::ExecuteHeroReplacement(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBHybridSummonPlan& Plan,
	const int32 CoordinatorGeneration,
	const int32 CoordinatorRevision)
{
	if (Plan.BeforeGeneration != CoordinatorGeneration
		|| Plan.BeforeRevision != CoordinatorRevision)
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridPlanStale, Plan);
	}
	const FWBHybridSummonPlanResult Preflight = BuildHeroReplacementPlans(
		State,
		Repository,
		Plan.ActingPlayerId,
		Plan.HybridCardInstanceId,
		CoordinatorGeneration,
		CoordinatorRevision);
	if (!Preflight.bOk)
	{
		return MakeExecutionFailure(Preflight.Code, Plan);
	}
	if (!Preflight.Plans.ContainsByPredicate(
		[&Plan](const FWBHybridSummonPlan& Candidate)
		{
			return PlansEqual(Candidate, Plan);
		}))
	{
		return MakeExecutionFailure(
			Plan.WandPaymentCardInstanceId.IsEmpty()
				? EWBHybridSummonResultCode::HybridWandPaymentRequired
				: EWBHybridSummonResultCode::HybridWandPaymentInvalid,
			Plan);
	}

	const FWBCardDefinitionRepositoryLookupResult HybridLookup =
		WBCardDefinitionRepository::FindCardById(
			Repository, Plan.HybridDefinitionId);
	if (!HybridLookup.bFound)
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridDefinitionInvalid, Plan);
	}

	FWBGameStateData WorkingState = State;
	if (Plan.WandPaymentSource == EWBHybridWandPaymentSource::Hand)
	{
		const FWBCardLifecycleResult Payment =
			WBCardLifecycle::MoveHandCardToDiscard(
				WorkingState,
				Plan.ActingPlayerId,
				Plan.WandPaymentCardInstanceId);
		if (!Payment.bOk)
		{
			return MakeExecutionFailure(EWBHybridSummonResultCode::HybridWandPaymentInvalid, Plan);
		}
	}
	else if (Plan.WandPaymentSource == EWBHybridWandPaymentSource::SacrificedUnit)
	{
		const FWBCardLifecycleResult Payment =
			WBCardLifecycle::MoveEquippedCardToDiscard(
				WorkingState,
				Plan.ActingPlayerId,
				Plan.WandPaymentCardInstanceId);
		if (!Payment.bOk)
		{
			return MakeExecutionFailure(EWBHybridSummonResultCode::HybridWandPaymentInvalid, Plan);
		}
	}
	else
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridWandPaymentRequired, Plan);
	}

	TArray<FWBEquippedCardEntry> RemainingEquipment;
	for (const FWBEquippedCardEntry& Entry : WorkingState.GetCardZoneState().EquippedCards)
	{
		if (Entry.EquippedToUnitId == Plan.SacrificedHeroUnitId)
		{
			RemainingEquipment.Add(Entry);
		}
	}
	RemainingEquipment.Sort(EquippedEntryLess);
	for (const FWBEquippedCardEntry& Entry : RemainingEquipment)
	{
		const FWBCardLifecycleResult Cleanup =
			WBCardLifecycle::MoveEquippedCardToDiscard(
				WorkingState,
				Entry.Card.OwnerPlayerId,
				Entry.Card.InstanceId);
		if (!Cleanup.bOk)
		{
			return MakeExecutionFailure(EWBHybridSummonResultCode::HybridZoneStateInvalid, Plan);
		}
	}

	if (!RemoveHybridFromHand(
		WorkingState, Plan.ActingPlayerId, Plan.HybridCardInstanceId))
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridNotInHand, Plan);
	}

	FWBUnitState* OldHero = WorkingState.GetMutableUnitById(Plan.SacrificedHeroUnitId);
	if (OldHero == nullptr || !OldHero->IsUnitOnBoard())
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridHeroSacrificeInvalid, Plan);
	}
	OldHero->ResonanceModifiers.Reset();
	OldHero->SetCanonicalRL(OldHero->GetBaseRLForRules(), OldHero->GetBaseRLForRules(), 0);
	OldHero->RemoveUnitFromBoard();
	if (WorkingState.HasPendingAttack()
		&& (WorkingState.PendingAttack.AttackerUnitId == Plan.SacrificedHeroUnitId
			|| WorkingState.PendingAttack.DefenderUnitId == Plan.SacrificedHeroUnitId))
	{
		WorkingState.ClearPendingAttack();
	}

	const int32 NewUnitId = AllocateNextUnitId(WorkingState);
	if (NewUnitId < 0 || WorkingState.GetUnitById(NewUnitId) != nullptr)
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridUnitIdAllocationFailed, Plan);
	}
	FWBUnitState NewHero;
	NewHero.UnitId = NewUnitId;
	NewHero.OwnerId = Plan.ActingPlayerId;
	NewHero.CardId = Plan.HybridDefinitionId;
	NewHero.X = Plan.DestinationTile.X;
	NewHero.Y = Plan.DestinationTile.Y;
	NewHero.HP = HybridLookup.Definition.CharacterStats.HP;
	NewHero.MaxHP = HybridLookup.Definition.CharacterStats.HP;
	NewHero.ATK = HybridLookup.Definition.CharacterStats.ATK;
	NewHero.AR = HybridLookup.Definition.CharacterStats.AR;
	NewHero.SetCanonicalRL(
		HybridLookup.Definition.CharacterStats.RL,
		HybridLookup.Definition.CharacterStats.RL,
		0);
	NewHero.AttacksLeft = 0;
	NewHero.MaxAttacksPerTurn = 1;
	NewHero.MPRemaining = 0;
	WorkingState.Units.Add(NewHero);
	FWBPlayerStateData* Player = WorkingState.GetMutablePlayerById(Plan.ActingPlayerId);
	if (Player == nullptr)
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridWrongPlayer, Plan);
	}
	Player->HeroUnitId = NewUnitId;

	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(
		WorkingState.GetCardZoneState(), ZoneReason))
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridZoneStateInvalid, Plan);
	}

	TArray<FWBTraceEvent> Traces;
	FWBTraceEvent Declared = MakeTrace(
		FName(TEXT("hybrid_summon_declared")), Plan,
		Plan.SacrificedHeroUnitId, NewUnitId);
	Declared.CardInstanceId = Plan.HybridCardInstanceId;
	Traces.Add(Declared);
	FWBTraceEvent Sacrificed = MakeTrace(
		FName(TEXT("unit_sacrificed")), Plan,
		Plan.SacrificedHeroUnitId, -1);
	Sacrificed.FromTile = Plan.DestinationTile;
	Sacrificed.DamageCause = FName(TEXT("hybrid_summon_cost"));
	Traces.Add(Sacrificed);
	Traces.Add(MakeTrace(
		FName(TEXT("hero_sacrifice_committed")), Plan,
		Plan.SacrificedHeroUnitId, -1));
	FWBTraceEvent Paid = MakeTrace(
		FName(TEXT("wand_payment_committed")), Plan,
		Plan.SacrificedHeroUnitId, -1);
	Paid.CardId.Reset();
	Paid.CostAmount = 1;
	Paid.CostKind = FName(TEXT("wand"));
	Paid.Reason = Plan.WandPaymentSource == EWBHybridWandPaymentSource::Hand
		? TEXT("hand")
		: TEXT("sacrificed_unit");
	Traces.Add(Paid);
	for (const FWBEquippedCardEntry& Entry : RemainingEquipment)
	{
		FWBTraceEvent Cleanup = MakeTrace(
			FName(TEXT("equipped_card_discarded_on_sacrifice")), Plan,
			Plan.SacrificedHeroUnitId, -1);
		Cleanup.CardId = Entry.Card.CardId;
		Cleanup.CardInstanceId = Entry.Card.InstanceId;
		Cleanup.SlotId = Entry.SlotId;
		Cleanup.EquipOrder = Entry.EquipOrder;
		Traces.Add(MoveTemp(Cleanup));
	}
	Traces.Add(MakeTrace(
		FName(TEXT("hybrid_summoned")), Plan,
		Plan.SacrificedHeroUnitId, NewUnitId));
	Traces.Add(MakeTrace(
		FName(TEXT("hero_replacement_committed")), Plan,
		Plan.SacrificedHeroUnitId, NewUnitId));

	State = MoveTemp(WorkingState);
	FWBHybridSummonResult Result;
	Result.bOk = true;
	Result.Code = EWBHybridSummonResultCode::Success;
	Result.Reason = ResultCodeToString(Result.Code);
	Result.Plan = Plan;
	Result.OldHeroUnitId = Plan.SacrificedHeroUnitId;
	Result.NewHeroUnitId = NewUnitId;
	Result.TraceEvents = MoveTemp(Traces);
	return Result;
}

FString WBHybridSummon::BuildStableActionId(const FWBHybridSummonPlan& Plan)
{
	const TCHAR* Payment = Plan.WandPaymentSource == EWBHybridWandPaymentSource::Hand
		? TEXT("hand")
		: TEXT("sacrificed_unit");
	return FString::Printf(
		TEXT("hybrid_summon:p%d:i%s:s%d:w%s:i%s:x%d:y%d"),
		Plan.ActingPlayerId,
		*Plan.HybridCardInstanceId,
		Plan.SacrificedHeroUnitId,
		Payment,
		*Plan.WandPaymentCardInstanceId,
		Plan.DestinationTile.X,
		Plan.DestinationTile.Y);
}

FString WBHybridSummon::ResultCodeToString(const EWBHybridSummonResultCode Code)
{
	switch (Code)
	{
	case EWBHybridSummonResultCode::Success: return TEXT("success");
	case EWBHybridSummonResultCode::HybridDefinitionInvalid: return TEXT("hybrid_definition_invalid");
	case EWBHybridSummonResultCode::HybridNotInHand: return TEXT("hybrid_not_in_hand");
	case EWBHybridSummonResultCode::HybridWrongPlayer: return TEXT("hybrid_wrong_player");
	case EWBHybridSummonResultCode::HybridSacrificeRequired: return TEXT("hybrid_sacrifice_required");
	case EWBHybridSummonResultCode::HybridSacrificeInvalid: return TEXT("hybrid_sacrifice_invalid");
	case EWBHybridSummonResultCode::HybridHeroSacrificeInvalid: return TEXT("hybrid_hero_sacrifice_invalid");
	case EWBHybridSummonResultCode::HybridWandPaymentRequired: return TEXT("hybrid_wand_payment_required");
	case EWBHybridSummonResultCode::HybridWandPaymentInvalid: return TEXT("hybrid_wand_payment_invalid");
	case EWBHybridSummonResultCode::HybridDestinationInvalid: return TEXT("hybrid_destination_invalid");
	case EWBHybridSummonResultCode::HybridDestinationOccupied: return TEXT("hybrid_destination_occupied");
	case EWBHybridSummonResultCode::HybridUnitCapExceeded: return TEXT("hybrid_unit_cap_exceeded");
	case EWBHybridSummonResultCode::HybridReplacementNotSupported: return TEXT("hybrid_replacement_not_supported");
	case EWBHybridSummonResultCode::HybridPlanStale: return TEXT("hybrid_plan_stale");
	case EWBHybridSummonResultCode::HybridZoneStateInvalid: return TEXT("hybrid_zone_state_invalid");
	case EWBHybridSummonResultCode::HybridUnitIdAllocationFailed: return TEXT("hybrid_unit_id_allocation_failed");
	default: return TEXT("hybrid_replacement_not_supported");
	}
}
