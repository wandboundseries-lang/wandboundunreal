#include "WBInitialHeroSetup.h"

#include "WBCardLifecycle.h"
#include "WBRules.h"

namespace
{
struct FCollectedSetupTrigger
{
	FWBEventIdentitySnapshot EventIdentity;
	FWBEventSourceSnapshot SourceSnapshot;
	FWBUnitParticipantSnapshot SummonedSnapshot;
	EWBTriggerEligibilityPolicy EligibilityPolicy =
		EWBTriggerEligibilityPolicy::SnapshotAtCollection;
	FString InstanceId;
	int32 ControllerPlayerId = -1;
	int32 ListenerUnitId = -1;
	int32 SummonedUnitId = -1;
	FWBSetupSummonTriggerDefinition Definition;
};

FWBInitialHeroSetupResult MakeInitialHeroSetupFailure(const FString& Reason)
{
	FWBInitialHeroSetupResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBTraceEvent SetupTrace(
	const FName Kind,
	const int32 PlayerId,
	const int32 TurnNumber)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = PlayerId;
	Event.TurnNumber = TurnNumber;
	Event.MatchPhase = FName(TEXT("setup"));
	Event.bOk = true;
	return Event;
}

bool TriggerMatches(
	const FWBSetupSummonTriggerDefinition& Trigger,
	const FWBUnitState& Listener,
	const FWBUnitState& Summoned,
	const FWBCardDefinition& SummonedDefinition)
{
	switch (Trigger.Scope)
	{
	case EWBSetupSummonTriggerScope::OwnWhenSummoned:
		return Listener.UnitId == Summoned.UnitId;
	case EWBSetupSummonTriggerScope::CharacterSummoned:
	case EWBSetupSummonTriggerScope::UnitSummoned:
		return true;
	case EWBSetupSummonTriggerScope::YouSummonUnit:
		return Listener.GetControllerPlayerIdForRules()
			== Summoned.GetControllerPlayerIdForRules();
	case EWBSetupSummonTriggerScope::OpponentSummonsUnit:
		return FWBGameStateData::IsValidPlayerId(
				Listener.GetControllerPlayerIdForRules())
			&& FWBGameStateData::IsValidPlayerId(
				Summoned.GetControllerPlayerIdForRules())
			&& Listener.GetControllerPlayerIdForRules()
				!= Summoned.GetControllerPlayerIdForRules();
	case EWBSetupSummonTriggerScope::FactionSummoned:
		return !Trigger.FactionId.IsEmpty()
			&& SummonedDefinition.PublicFactions.Contains(Trigger.FactionId);
	default:
		return false;
	}
}

bool InitialHeroSetupTriggerLess(
	const FCollectedSetupTrigger& A,
	const FCollectedSetupTrigger& B)
{
	return A.InstanceId < B.InstanceId;
}
}

FWBInitialHeroSetupResult WBInitialHeroSetup::Apply(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBInitialHeroSetupRequest& Request)
{
	if (!FWBGameStateData::IsValidPlayerId(Request.FirstPlayerId)
		|| Request.Placements.Num() != 2)
	{
		return MakeInitialHeroSetupFailure(TEXT("initial_hero_setup_invalid"));
	}

	TArray<FWBInitialHeroPlacement> Placements = Request.Placements;
	Placements.Sort([](
		const FWBInitialHeroPlacement& A,
		const FWBInitialHeroPlacement& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	if (Placements[0].PlayerId != 0 || Placements[1].PlayerId != 1
		|| Placements[0].SpawnTile == Placements[1].SpawnTile)
	{
		return MakeInitialHeroSetupFailure(TEXT("initial_hero_setup_invalid"));
	}

	FWBGameStateData WorkingState = State;
	TArray<FWBUnitState> Heroes;
	for (const FWBInitialHeroPlacement& Placement : Placements)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository,
				Placement.HeroCardId);
		if (!Lookup.bFound
			|| Lookup.Definition.Kind != EWBCardDefinitionKind::Character
			|| !WBRules::IsTileInBounds(Placement.SpawnTile)
			|| WorkingState.IsTileOccupied(Placement.SpawnTile))
		{
			return MakeInitialHeroSetupFailure(TEXT("hero_definition_invalid"));
		}

		FWBPlayerStateData* Player =
			WorkingState.GetMutablePlayerById(Placement.PlayerId);
		if (Player == nullptr)
		{
			return MakeInitialHeroSetupFailure(TEXT("invalid_player_setup"));
		}

		FWBUnitState Hero;
		Hero.UnitId = Placement.PlayerId;
		Hero.SetOwnerAndControllerForRules(Placement.PlayerId, Placement.PlayerId);
		Hero.CardId = Placement.HeroCardId;
		Hero.X = Placement.SpawnTile.X;
		Hero.Y = Placement.SpawnTile.Y;
		Hero.HP = Lookup.Definition.CharacterStats.HP;
		Hero.MaxHP = Hero.HP;
		Hero.ATK = Lookup.Definition.CharacterStats.ATK;
		Hero.AR = Lookup.Definition.CharacterStats.AR;
		Hero.SetCanonicalRL(
			Lookup.Definition.CharacterStats.RL,
			Lookup.Definition.CharacterStats.RL,
			0);
		Hero.MaxAttacksPerTurn = 1;
		Heroes.Add(Hero);
		Player->HeroUnitId = Hero.UnitId;
	}

	FWBInitialHeroSetupResult Result;
	Result.TraceEvents.Add(SetupTrace(
		FName(TEXT("hero_spawn_batch_started")),
		Request.FirstPlayerId,
		WorkingState.TurnNumber));

	// The shared mutation boundary is intentional: trigger collection starts only
	// after both prepared units have entered the authoritative state.
	WorkingState.Units.Append(Heroes);
	for (const FWBInitialHeroPlacement& Placement : Placements)
	{
		FWBTraceEvent Spawned = SetupTrace(
			FName(TEXT("hero_spawned")),
			Placement.PlayerId,
			WorkingState.TurnNumber);
		Spawned.SourceUnitId = Placement.PlayerId;
		Spawned.CardInstanceId = Placement.HeroInstanceId;
		Spawned.CardId = Placement.HeroCardId;
		Spawned.ToTile = Placement.SpawnTile;
		Spawned.bHeroUnit = true;
		Result.TraceEvents.Add(Spawned);
	}
	Result.TraceEvents.Add(SetupTrace(
		FName(TEXT("hero_spawn_batch_committed")),
		Request.FirstPlayerId,
		WorkingState.TurnNumber));
	Result.bSpawnBatchCommitted = true;
	Result.FinalPhase = EWBInitialSetupPhase::HeroTriggerCollection;

	TArray<FCollectedSetupTrigger> Collected;
	for (const FWBUnitState& Listener : WorkingState.Units)
	{
		const FWBCardDefinitionRepositoryLookupResult ListenerLookup =
			WBCardDefinitionRepository::FindCardById(
				Repository,
				Listener.CardId);
		if (!ListenerLookup.bFound)
		{
			continue;
		}
		for (const FWBSetupSummonTriggerDefinition& Trigger :
			ListenerLookup.Definition.SetupSummonTriggers)
		{
			for (const FWBUnitState& Summoned : Heroes)
			{
				const FWBCardDefinitionRepositoryLookupResult SummonedLookup =
					WBCardDefinitionRepository::FindCardById(
						Repository,
						Summoned.CardId);
				if (!SummonedLookup.bFound
					|| !TriggerMatches(
						Trigger,
						Listener,
						Summoned,
						SummonedLookup.Definition))
				{
					continue;
				}

				FCollectedSetupTrigger Item;
				Item.SourceSnapshot =
					WBEventSnapshot::CaptureUnitSource(WorkingState, Listener);
				Item.SummonedSnapshot =
					WBEventSnapshot::CaptureUnitParticipant(
						WorkingState, Summoned);
				Item.ControllerPlayerId =
					Item.SourceSnapshot.ControllerPlayerId;
				Item.ListenerUnitId = Item.SourceSnapshot.SourceUnitId;
				Item.SummonedUnitId = Item.SummonedSnapshot.UnitId;
				Item.Definition = Trigger;
				Item.InstanceId = FString::Printf(
					TEXT("setup_trigger:p%d:l%d:s%d:%s"),
					Item.ControllerPlayerId,
					Item.ListenerUnitId,
					Item.SummonedUnitId,
					*Trigger.TriggerId);
				Item.EventIdentity = WBEventSnapshot::MakeIdentity(
					EWBEventKind::Summon,
					Item.InstanceId,
					WorkingState.TurnNumber);
				Collected.Add(MoveTemp(Item));
			}
		}
	}
	Collected.Sort(InitialHeroSetupTriggerLess);
	for (const FCollectedSetupTrigger& Trigger : Collected)
	{
		Result.CollectedTriggerIds.Add(Trigger.InstanceId);
	}

	FWBTraceEvent CollectedTrace = SetupTrace(
		FName(TEXT("hero_summon_triggers_collected")),
		Request.FirstPlayerId,
		WorkingState.TurnNumber);
	CollectedTrace.CardCount = Collected.Num();
	Result.TraceEvents.Add(CollectedTrace);

	for (int32 BatchIndex = 0; BatchIndex < 2; ++BatchIndex)
	{
		Result.FinalPhase = BatchIndex == 0
			? EWBInitialSetupPhase::FirstPlayerHeroTriggerResolution
			: EWBInitialSetupPhase::SecondPlayerHeroTriggerResolution;
		const int32 Controller = BatchIndex == 0
			? Request.FirstPlayerId
			: 1 - Request.FirstPlayerId;
		TArray<FCollectedSetupTrigger> Batch =
			Collected.FilterByPredicate(
				[Controller](const FCollectedSetupTrigger& Trigger)
				{
					return Trigger.SourceSnapshot.ControllerPlayerId
						== Controller;
				});
		if (Batch.Num() > 1)
		{
			const TArray<FString>* Choice =
				Request.TriggerOrderChoices.Find(Controller);
			if (Choice == nullptr || Choice->Num() != Batch.Num())
			{
				return MakeInitialHeroSetupFailure(TEXT("setup_trigger_order_choice_required"));
			}
			TArray<FCollectedSetupTrigger> Ordered;
			for (const FString& ChosenId : *Choice)
			{
				const FCollectedSetupTrigger* Match =
					Batch.FindByPredicate(
						[&ChosenId](const FCollectedSetupTrigger& Candidate)
						{
							return Candidate.InstanceId == ChosenId;
						});
				if (Match == nullptr
					|| Ordered.ContainsByPredicate(
						[&ChosenId](const FCollectedSetupTrigger& Existing)
						{
							return Existing.InstanceId == ChosenId;
						}))
				{
					return MakeInitialHeroSetupFailure(TEXT("setup_trigger_order_choice_invalid"));
				}
				Ordered.Add(*Match);
			}
			Batch = MoveTemp(Ordered);
			FWBTraceEvent ChoiceTrace = SetupTrace(
				FName(TEXT("setup_trigger_order_chosen")),
				Controller,
				WorkingState.TurnNumber);
			ChoiceTrace.ActionId = FString::Printf(
				TEXT("setup_trigger_order:p%d:%s"),
				Controller,
				*FString::Join(*Choice, TEXT(",")));
			Result.TraceEvents.Add(ChoiceTrace);
		}

		for (const FCollectedSetupTrigger& Trigger : Batch)
		{
			for (int32 DrawIndex = 0;
				DrawIndex < Trigger.Definition.DrawCount;
				++DrawIndex)
			{
				const FWBCardLifecycleResult Draw =
					WBCardLifecycle::DrawOneCard(
						WorkingState,
						Trigger.SourceSnapshot.ControllerPlayerId);
				if (!Draw.bOk)
				{
					return MakeInitialHeroSetupFailure(Draw.Reason);
				}
				FWBTraceEvent DrawTrace = SetupTrace(
					FName(TEXT("setup_trigger_draw")),
					Trigger.SourceSnapshot.ControllerPlayerId,
					WorkingState.TurnNumber);
				DrawTrace.CardCount = 1;
				Result.TraceEvents.Add(DrawTrace);
			}

			FWBTraceEvent Resolved = SetupTrace(
				FName(TEXT("setup_trigger_resolved")),
				Trigger.SourceSnapshot.ControllerPlayerId,
				WorkingState.TurnNumber);
			Resolved.ActionId = Trigger.InstanceId;
			Resolved.SourceUnitId = Trigger.SourceSnapshot.SourceUnitId;
			Resolved.TargetUnitId = Trigger.SummonedSnapshot.UnitId;
			Result.TraceEvents.Add(Resolved);
		}
	}

	State = MoveTemp(WorkingState);
	Result.bOk = true;
	Result.bTriggersResolved = true;
	Result.FinalPhase =
		EWBInitialSetupPhase::SecondPlayerHeroTriggerResolution;
	return Result;
}

bool WBInitialHeroSetup::CanSubmitManualReact(
	const FWBGameStateData& State,
	FString& OutReason)
{
	if (State.bSuppressManualReactsDuringInitialHeroSetup)
	{
		OutReason =
			TEXT("manual_react_not_allowed_during_initial_hero_setup");
		return false;
	}
	OutReason.Reset();
	return true;
}
