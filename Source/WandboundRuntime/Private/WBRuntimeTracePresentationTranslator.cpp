#include "WBRuntimeTracePresentationTranslator.h"

namespace
{
bool HasTile(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.Y >= 0;
}

FIntPoint ToPoint(const FWBTile& Tile)
{
	return FIntPoint(Tile.X, Tile.Y);
}

float DurationFor(const EWBRuntimePresentationEventType Type)
{
	switch (Type)
	{
	case EWBRuntimePresentationEventType::UnitMoved:
	case EWBRuntimePresentationEventType::NPCMoved:
	case EWBRuntimePresentationEventType::UnitSummoned:
	case EWBRuntimePresentationEventType::NPCSpawned:
		return 0.18f;
	case EWBRuntimePresentationEventType::UnitDefeated:
	case EWBRuntimePresentationEventType::HeroDefeated:
		return 0.20f;
	case EWBRuntimePresentationEventType::AttackDeclared:
	case EWBRuntimePresentationEventType::NPCAttacked:
	case EWBRuntimePresentationEventType::MarkerRevealed:
	case EWBRuntimePresentationEventType::WandEquipped:
	case EWBRuntimePresentationEventType::ActivationResolved:
		return 0.12f;
	case EWBRuntimePresentationEventType::AttackImpact:
	case EWBRuntimePresentationEventType::TrapTriggered:
	case EWBRuntimePresentationEventType::DamageApplied:
	case EWBRuntimePresentationEventType::HPChanged:
	case EWBRuntimePresentationEventType::ArmorChanged:
	case EWBRuntimePresentationEventType::MarkerConsumed:
	case EWBRuntimePresentationEventType::EquipmentDiscarded:
		return 0.08f;
	case EWBRuntimePresentationEventType::TurnStarted:
	case EWBRuntimePresentationEventType::TurnEnded:
	case EWBRuntimePresentationEventType::NPCPhaseStarted:
	case EWBRuntimePresentationEventType::NPCPhaseCompleted:
		return 0.10f;
	default:
		return 0.0f;
	}
}

FWBRuntimePresentationEvent MakeEvent(
	const FWBTraceEvent& Trace,
	const int32 TraceIndex,
	const int32 StageIndex,
	const EWBRuntimePresentationEventType Type)
{
	FWBRuntimePresentationEvent Event;
	Event.SourceTraceIndex = TraceIndex;
	Event.SourceStageIndex = StageIndex;
	Event.Type = Type;
	Event.PlayerId = Trace.PlayerId;
	Event.SourceUnitId = Trace.SourceUnitId;
	Event.TargetUnitId = Trace.TargetUnitId;
	Event.SourceTile = ToPoint(Trace.FromTile);
	Event.DestinationTile = ToPoint(Trace.ToTile);
	Event.DamageAmount = Trace.FinalDamageAmount >= 0 ? Trace.FinalDamageAmount : FMath::Max(Trace.DamageAmount, 0);
	Event.PreviousHP = Trace.PreviousHP;
	Event.NewHP = Trace.NewHP;
	Event.PreviousArmor = Trace.PreviousArmor;
	Event.NewArmor = Trace.NewArmor;
	Event.PreviousRLUsed = Trace.PreviousRLUsed;
	Event.NewRLUsed = Trace.NewRLUsed;
	Event.MarkerId = Trace.MarkerId;
	Event.TurnNumber = Trace.TurnNumber;
	Event.WinnerPlayerId = Trace.WinningPlayerId;
	Event.SuggestedDurationSeconds = DurationFor(Type);
	Event.bTerminal = Type == EWBRuntimePresentationEventType::GameOver;
	return Event;
}

void AddEvent(
	TArray<FWBRuntimePresentationEvent>& Events,
	const FWBTraceEvent& Trace,
	const int32 TraceIndex,
	const int32 StageIndex,
	const EWBRuntimePresentationEventType Type)
{
	FWBRuntimePresentationEvent Event = MakeEvent(Trace, TraceIndex, StageIndex, Type);
	Event.SequenceIndex = Events.Num();
	Events.Add(MoveTemp(Event));
}

void AddDamageEvents(
	TArray<FWBRuntimePresentationEvent>& Events,
	const FWBTraceEvent& Trace,
	const int32 TraceIndex,
	int32 StageIndex)
{
	AddEvent(Events, Trace, TraceIndex, StageIndex++, EWBRuntimePresentationEventType::DamageApplied);
	if (Trace.PreviousArmor >= 0 && Trace.NewArmor >= 0 && Trace.PreviousArmor != Trace.NewArmor)
	{
		AddEvent(Events, Trace, TraceIndex, StageIndex++, EWBRuntimePresentationEventType::ArmorChanged);
	}
	if (Trace.PreviousHP >= 0 && Trace.NewHP >= 0 && Trace.PreviousHP != Trace.NewHP)
	{
		AddEvent(Events, Trace, TraceIndex, StageIndex, EWBRuntimePresentationEventType::HPChanged);
	}
}

FWBRuntimePresentationTranslationResult Fail(const int32 TraceIndex, const FString& Reason)
{
	FWBRuntimePresentationTranslationResult Result;
	Result.Reason = FString::Printf(TEXT("trace_%d:%s"), TraceIndex, *Reason);
	return Result;
}
}

FWBRuntimePresentationTranslationResult WBRuntimeTracePresentationTranslator::Translate(
	const TArray<FWBTraceEvent>& TraceEvents)
{
	FWBRuntimePresentationTranslationResult Result;
	for (int32 TraceIndex = 0; TraceIndex < TraceEvents.Num(); ++TraceIndex)
	{
		const FWBTraceEvent& Trace = TraceEvents[TraceIndex];
		const FName Kind = Trace.Kind;

		if (Kind == FName(TEXT("match_initialized")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::MatchInitialized);
		}
		else if (Kind == FName(TEXT("turn_started")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::TurnStarted);
		}
		else if (Kind == FName(TEXT("turn_ended")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::TurnEnded);
		}
		else if (Kind == FName(TEXT("npc_phase_started")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::NPCPhaseStarted);
		}
		else if (Kind == FName(TEXT("npc_phase_ended")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::NPCPhaseCompleted);
		}
		else if (Kind == FName(TEXT("move")) || Kind == FName(TEXT("npc_moved")))
		{
			if (Trace.SourceUnitId < 0 || !HasTile(Trace.FromTile) || !HasTile(Trace.ToTile))
			{
				return Fail(TraceIndex, TEXT("movement_trace_malformed"));
			}
			AddEvent(
				Result.Events,
				Trace,
				TraceIndex,
				0,
				Kind == FName(TEXT("npc_moved"))
					? EWBRuntimePresentationEventType::NPCMoved
					: EWBRuntimePresentationEventType::UnitMoved);
		}
		else if (Kind == FName(TEXT("attack_declared")) || Kind == FName(TEXT("npc_attack_declared")))
		{
			if (Trace.SourceUnitId < 0 || Trace.TargetUnitId < 0)
			{
				return Fail(TraceIndex, TEXT("attack_trace_malformed"));
			}
			const bool bNPC = Kind == FName(TEXT("npc_attack_declared"));
			AddEvent(
				Result.Events,
				Trace,
				TraceIndex,
				0,
				bNPC ? EWBRuntimePresentationEventType::NPCAttacked : EWBRuntimePresentationEventType::AttackDeclared);
			AddEvent(Result.Events, Trace, TraceIndex, 1, EWBRuntimePresentationEventType::AttackImpact);
		}
		else if (Kind == FName(TEXT("attack_damage_resolved"))
			|| Kind == FName(TEXT("damage_effect_resolved"))
			|| (Kind == FName(TEXT("status_tick")) && Trace.PreviousHP >= 0))
		{
			AddDamageEvents(Result.Events, Trace, TraceIndex, 0);
		}
		else if (Kind == FName(TEXT("heal_effect_resolved")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::HPChanged);
		}
		else if (Kind == FName(TEXT("armor_modified")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::ArmorChanged);
		}
		else if (Kind == FName(TEXT("summon_unit"))
			|| Kind == FName(TEXT("hero_spawned"))
			|| Kind == FName(TEXT("hybrid_summoned")))
		{
			FWBRuntimePresentationEvent Event = MakeEvent(
				Trace,
				TraceIndex,
				0,
				EWBRuntimePresentationEventType::UnitSummoned);
			Event.SequenceIndex = Result.Events.Num();
			Event.PublicDefinitionId = Trace.CardId;
			Result.Events.Add(MoveTemp(Event));
		}
		else if (Kind == FName(TEXT("unit_sacrificed")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0,
				EWBRuntimePresentationEventType::UnitSacrificed);
		}
		else if (Kind == FName(TEXT("wand_payment_committed")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0,
				EWBRuntimePresentationEventType::WandPaymentCommitted);
		}
		else if (Kind == FName(TEXT("hero_replacement_committed")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0,
				EWBRuntimePresentationEventType::HeroReplaced);
		}
		else if (Kind == FName(TEXT("equip_wand")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::WandEquipped);
		}
		else if (Kind == FName(TEXT("card_activation_resolved")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::ActivationResolved);
		}
		else if (Kind == FName(TEXT("marker_revealed")))
		{
			FWBRuntimePresentationEvent Event = MakeEvent(
				Trace,
				TraceIndex,
				0,
				EWBRuntimePresentationEventType::MarkerRevealed);
			Event.SequenceIndex = Result.Events.Num();
			Event.PublicMarkerType = Trace.MarkerType;
			Result.Events.Add(MoveTemp(Event));
		}
		else if (Kind == FName(TEXT("marker_consumed")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::MarkerConsumed);
		}
		else if (Kind == FName(TEXT("trap_damage_resolved")))
		{
			FWBRuntimePresentationEvent Trap = MakeEvent(
				Trace,
				TraceIndex,
				0,
				EWBRuntimePresentationEventType::TrapTriggered);
			Trap.SequenceIndex = Result.Events.Num();
			Trap.PublicMarkerType = FName(TEXT("Trap"));
			Result.Events.Add(MoveTemp(Trap));
			AddDamageEvents(Result.Events, Trace, TraceIndex, 1);
		}
		else if (Kind == FName(TEXT("npc_spawn_succeeded")))
		{
			FWBRuntimePresentationEvent Event = MakeEvent(
				Trace,
				TraceIndex,
				0,
				EWBRuntimePresentationEventType::NPCSpawned);
			Event.SequenceIndex = Result.Events.Num();
			Event.SourceUnitId = Trace.TargetUnitId;
			Event.PublicDefinitionId = Trace.CardId;
			Result.Events.Add(MoveTemp(Event));
		}
		else if (Kind == FName(TEXT("unit_defeated")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::UnitDefeated);
		}
		else if (Kind == FName(TEXT("equipped_card_discarded_on_death"))
			|| Kind == FName(TEXT("rl_overflow_remove_wand")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::EquipmentDiscarded);
		}
		else if (Kind == FName(TEXT("hero_defeated")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::HeroDefeated);
		}
		else if (Kind == FName(TEXT("game_over")) || Kind == FName(TEXT("marker_triggered_game_over")))
		{
			AddEvent(Result.Events, Trace, TraceIndex, 0, EWBRuntimePresentationEventType::GameOver);
		}
	}

	Result.bOk = true;
	Result.Reason = TEXT("presentation_events_translated");
	return Result;
}
