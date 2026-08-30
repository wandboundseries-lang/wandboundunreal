#include "WBHybridSummon.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBRules.h"
#include "WBTurnOneRestrictions.h"

namespace
{
constexpr int32 HybridMaxOwnedUnitsIncludingHero = 4;

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
	if (Hero == nullptr || Hero->GetControllerPlayerIdForRules() != PlayerId || !Hero->IsUnitOnBoard())
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
		&& A.SacrificedUnitId == B.SacrificedUnitId
		&& A.WandPaymentSource == B.WandPaymentSource
		&& A.WandPaymentCardInstanceId == B.WandPaymentCardInstanceId
		&& A.WandPaymentUnitId == B.WandPaymentUnitId
		&& A.DestinationTile == B.DestinationTile
		&& A.bBecomesReplacementHero == B.bBecomesReplacementHero
		&& A.BeforeGeneration == B.BeforeGeneration
		&& A.BeforeRevision == B.BeforeRevision;
}

int32 AllocateNextHybridUnitId(const FWBGameStateData& State)
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
	Event.bHeroUnit = Plan.bBecomesReplacementHero;
	Event.bOk = true;
	return Event;
}
}

FWBHybridSummonPlanResult WBHybridSummon::BuildSummonPlans(
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
	const int32 CompletedUnitCount =
		State.GetUnitsForPlayer(ActingPlayerId).Num() - 1 + 1;
	if (CompletedUnitCount > HybridMaxOwnedUnitsIncludingHero)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridUnitCapExceeded);
	}

	TArray<FWBHybridSummonPlan> Plans;
	TArray<FWBZoneCardEntry> Hand = PlayerZones->Hand;
	Hand.Sort(PaymentEntryLess);
	TArray<FWBEquippedCardEntry> Equipped = State.GetCardZoneState().EquippedCards;
	Equipped.Sort(EquippedEntryLess);

	TArray<const FWBUnitState*> SacrificeCandidates =
		State.GetUnitsForPlayer(ActingPlayerId);
	SacrificeCandidates.Sort([](const FWBUnitState& A, const FWBUnitState& B)
	{
		return A.UnitId < B.UnitId;
	});

	bool bFoundCharacterSacrifice = false;
	bool bFoundDestination = false;
	for (const FWBUnitState* Sacrifice : SacrificeCandidates)
	{
		if (Sacrifice == nullptr || !Sacrifice->IsUnitOnBoard())
		{
			continue;
		}
		const FWBCardDefinitionRepositoryLookupResult SacrificeLookup =
			WBCardDefinitionRepository::FindCardById(
				Repository, Sacrifice->CardId);
		if (!SacrificeLookup.bFound
			|| SacrificeLookup.Definition.Kind
				!= EWBCardDefinitionKind::Character)
		{
			continue;
		}
		bFoundCharacterSacrifice = true;

		const bool bReplacesHero = Sacrifice->UnitId == Hero->UnitId;
		TArray<FWBTile> Destinations;
		if (bReplacesHero)
		{
			Destinations.Add(FWBTile(Sacrifice->X, Sacrifice->Y));
		}
		else
		{
			Destinations = {
				FWBTile(Hero->X + 1, Hero->Y),
				FWBTile(Hero->X - 1, Hero->Y),
				FWBTile(Hero->X, Hero->Y + 1),
				FWBTile(Hero->X, Hero->Y - 1)
			};
			Destinations.Sort([](const FWBTile& A, const FWBTile& B)
			{
				return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
			});
		}

		for (const FWBTile& Destination : Destinations)
		{
			if (!WBRules::IsTileInBounds(Destination))
			{
				continue;
			}
			const int32 Occupant = State.UnitIdAt(Destination);
			if (Occupant != -1 && Occupant != Sacrifice->UnitId)
			{
				continue;
			}
			const FWBTurnOneRestrictionQuery TurnOne =
				WBTurnOneRestrictions::QuerySummonPlacement(
					State, ActingPlayerId, Destination);
			if (!TurnOne.bOk)
			{
				continue;
			}
			bFoundDestination = true;

			for (const FWBZoneCardEntry& Entry : Hand)
			{
				if (Entry.Card.InstanceId == HybridCardInstanceId
					|| Entry.Card.OwnerPlayerId != ActingPlayerId
					|| !IsWandDefinition(Repository, Entry.Card.CardId))
				{
					continue;
				}
				FWBHybridSummonPlan Plan;
				Plan.ActingPlayerId = ActingPlayerId;
				Plan.HybridCardInstanceId = HybridCardInstanceId;
				Plan.HybridDefinitionId = HybridEntry->Card.CardId;
				Plan.SacrificedUnitId = Sacrifice->UnitId;
				Plan.WandPaymentSource = EWBHybridWandPaymentSource::Hand;
				Plan.WandPaymentCardInstanceId = Entry.Card.InstanceId;
				Plan.DestinationTile = Destination;
				Plan.bBecomesReplacementHero = bReplacesHero;
				Plan.BeforeGeneration = CoordinatorGeneration;
				Plan.BeforeRevision = CoordinatorRevision;
				Plans.Add(MoveTemp(Plan));
			}

			for (const FWBEquippedCardEntry& Entry : Equipped)
			{
				if (Entry.EquippedToUnitId != Sacrifice->UnitId
					|| Entry.Card.OwnerPlayerId != ActingPlayerId
					|| !IsWandDefinition(Repository, Entry.Card.CardId))
				{
					continue;
				}
				FWBHybridSummonPlan Plan;
				Plan.ActingPlayerId = ActingPlayerId;
				Plan.HybridCardInstanceId = HybridCardInstanceId;
				Plan.HybridDefinitionId = HybridEntry->Card.CardId;
				Plan.SacrificedUnitId = Sacrifice->UnitId;
				Plan.WandPaymentSource =
					EWBHybridWandPaymentSource::SacrificedUnit;
				Plan.WandPaymentCardInstanceId = Entry.Card.InstanceId;
				Plan.WandPaymentUnitId = Sacrifice->UnitId;
				Plan.DestinationTile = Destination;
				Plan.bBecomesReplacementHero = bReplacesHero;
				Plan.BeforeGeneration = CoordinatorGeneration;
				Plan.BeforeRevision = CoordinatorRevision;
				Plans.Add(MoveTemp(Plan));
			}
		}
	}

	if (!bFoundCharacterSacrifice)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridSacrificeRequired);
	}
	if (!bFoundDestination)
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridDestinationInvalid);
	}

	if (Plans.IsEmpty())
	{
		return MakePlanFailure(EWBHybridSummonResultCode::HybridWandPaymentRequired);
	}
	Plans.Sort([](const FWBHybridSummonPlan& A, const FWBHybridSummonPlan& B)
	{
		if (A.SacrificedUnitId != B.SacrificedUnitId)
		{
			return A.SacrificedUnitId < B.SacrificedUnitId;
		}
		if (A.DestinationTile.Y != B.DestinationTile.Y)
		{
			return A.DestinationTile.Y < B.DestinationTile.Y;
		}
		if (A.DestinationTile.X != B.DestinationTile.X)
		{
			return A.DestinationTile.X < B.DestinationTile.X;
		}
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

FWBHybridSummonPlanResult WBHybridSummon::BuildHeroReplacementPlans(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ActingPlayerId,
	const FString& HybridCardInstanceId,
	const int32 CoordinatorGeneration,
	const int32 CoordinatorRevision)
{
	FWBHybridSummonPlanResult Result = BuildSummonPlans(
		State,
		Repository,
		ActingPlayerId,
		HybridCardInstanceId,
		CoordinatorGeneration,
		CoordinatorRevision);
	if (!Result.bOk)
	{
		return Result;
	}
	Result.Plans = Result.Plans.FilterByPredicate(
		[](const FWBHybridSummonPlan& Plan)
		{
			return Plan.bBecomesReplacementHero;
		});
	if (Result.Plans.IsEmpty())
	{
		return MakePlanFailure(
			EWBHybridSummonResultCode::HybridHeroSacrificeInvalid);
	}
	return Result;
}

FWBHybridSummonResult WBHybridSummon::ExecuteSummon(
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
	const FWBHybridSummonPlanResult Preflight = BuildSummonPlans(
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
		if (Entry.EquippedToUnitId == Plan.SacrificedUnitId)
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

	FWBPlayerStateData* Player = WorkingState.GetMutablePlayerById(
		Plan.ActingPlayerId);
	if (Player == nullptr)
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridWrongPlayer, Plan);
	}
	const int32 OriginalHeroUnitId = Player->HeroUnitId;
	FWBUnitState* SacrificedUnit = WorkingState.GetMutableUnitById(
		Plan.SacrificedUnitId);
	if (SacrificedUnit == nullptr || !SacrificedUnit->IsUnitOnBoard())
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridSacrificeInvalid, Plan);
	}
	const FWBTile SacrificedFromTile(SacrificedUnit->X, SacrificedUnit->Y);
	SacrificedUnit->ResonanceModifiers.Reset();
	SacrificedUnit->SetCanonicalRL(
		SacrificedUnit->GetBaseRLForRules(),
		SacrificedUnit->GetBaseRLForRules(),
		0);
	SacrificedUnit->RemoveUnitFromBoard();
	if (WorkingState.HasPendingAttack()
		&& (WorkingState.PendingAttack.AttackerUnitId == Plan.SacrificedUnitId
			|| WorkingState.PendingAttack.DefenderUnitId == Plan.SacrificedUnitId))
	{
		WorkingState.ClearPendingAttack();
	}

	const int32 NewUnitId = AllocateNextHybridUnitId(WorkingState);
	if (NewUnitId < 0 || WorkingState.GetUnitById(NewUnitId) != nullptr)
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridUnitIdAllocationFailed, Plan);
	}
	FWBUnitState NewHybrid;
	NewHybrid.UnitId = NewUnitId;
	NewHybrid.SetOwnerAndControllerForRules(Plan.ActingPlayerId, Plan.ActingPlayerId);
	NewHybrid.CardId = Plan.HybridDefinitionId;
	NewHybrid.X = Plan.DestinationTile.X;
	NewHybrid.Y = Plan.DestinationTile.Y;
	NewHybrid.HP = HybridLookup.Definition.CharacterStats.HP;
	NewHybrid.MaxHP = HybridLookup.Definition.CharacterStats.HP;
	NewHybrid.ATK = HybridLookup.Definition.CharacterStats.ATK;
	NewHybrid.AR = HybridLookup.Definition.CharacterStats.AR;
	NewHybrid.SetCanonicalRL(
		HybridLookup.Definition.CharacterStats.RL,
		HybridLookup.Definition.CharacterStats.RL,
		0);
	NewHybrid.AttacksLeft = 0;
	NewHybrid.MaxAttacksPerTurn = 1;
	NewHybrid.MPRemaining = 0;
	WorkingState.Units.Add(NewHybrid);
	if (Plan.bBecomesReplacementHero)
	{
		Player->HeroUnitId = NewUnitId;
	}
	else if (Player->HeroUnitId != OriginalHeroUnitId
		|| OriginalHeroUnitId == Plan.SacrificedUnitId)
	{
		return MakeExecutionFailure(
			EWBHybridSummonResultCode::HybridHeroSacrificeInvalid, Plan);
	}

	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(
		WorkingState.GetCardZoneState(), ZoneReason))
	{
		return MakeExecutionFailure(EWBHybridSummonResultCode::HybridZoneStateInvalid, Plan);
	}

	TArray<FWBTraceEvent> Traces;
	FWBTraceEvent Declared = MakeTrace(
		FName(TEXT("hybrid_summon_declared")), Plan,
		Plan.SacrificedUnitId, NewUnitId);
	Declared.CardInstanceId = Plan.HybridCardInstanceId;
	Traces.Add(Declared);
	FWBTraceEvent Sacrificed = MakeTrace(
		FName(TEXT("unit_sacrificed")), Plan,
		Plan.SacrificedUnitId, -1);
	Sacrificed.FromTile = SacrificedFromTile;
	Sacrificed.DamageCause = FName(TEXT("hybrid_summon_cost"));
	Traces.Add(Sacrificed);
	if (Plan.bBecomesReplacementHero)
	{
		Traces.Add(MakeTrace(
			FName(TEXT("hero_sacrifice_committed")), Plan,
			Plan.SacrificedUnitId, -1));
	}
	FWBTraceEvent Paid = MakeTrace(
		FName(TEXT("wand_payment_committed")), Plan,
		Plan.SacrificedUnitId, -1);
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
			Plan.SacrificedUnitId, -1);
		Cleanup.CardId = Entry.Card.CardId;
		Cleanup.CardInstanceId = Entry.Card.InstanceId;
		Cleanup.SlotId = Entry.SlotId;
		Cleanup.EquipOrder = Entry.EquipOrder;
		Traces.Add(MoveTemp(Cleanup));
	}
	Traces.Add(MakeTrace(
		FName(TEXT("hybrid_summoned")), Plan,
		Plan.SacrificedUnitId, NewUnitId));
	if (Plan.bBecomesReplacementHero)
	{
		Traces.Add(MakeTrace(
			FName(TEXT("hero_replacement_committed")), Plan,
			Plan.SacrificedUnitId, NewUnitId));
	}

	State = MoveTemp(WorkingState);
	FWBHybridSummonResult Result;
	Result.bOk = true;
	Result.Code = EWBHybridSummonResultCode::Success;
	Result.Reason = ResultCodeToString(Result.Code);
	Result.Plan = Plan;
	Result.SacrificedUnitId = Plan.SacrificedUnitId;
	Result.OriginalHeroUnitId = OriginalHeroUnitId;
	Result.NewHybridUnitId = NewUnitId;
	Result.OldHeroUnitId = Plan.bBecomesReplacementHero
		? Plan.SacrificedUnitId
		: OriginalHeroUnitId;
	Result.NewHeroUnitId = Player->HeroUnitId;
	Result.TraceEvents = MoveTemp(Traces);
	return Result;
}

FWBHybridSummonResult WBHybridSummon::ExecuteHeroReplacement(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBHybridSummonPlan& Plan,
	const int32 CoordinatorGeneration,
	const int32 CoordinatorRevision)
{
	if (!Plan.bBecomesReplacementHero)
	{
		return MakeExecutionFailure(
			EWBHybridSummonResultCode::HybridHeroSacrificeInvalid, Plan);
	}
	return ExecuteSummon(
		State,
		Repository,
		Plan,
		CoordinatorGeneration,
		CoordinatorRevision);
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
		Plan.SacrificedUnitId,
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
