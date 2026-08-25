#include "WBProductionMatchReplay.h"

#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <openssl/sha.h>

namespace
{
const FString ZeroHash = FString::ChrN(64, TEXT('0'));

void AppendInt(FString& Out, const TCHAR* Key, const int64 Value)
{
	Out += Key;
	Out += TEXT("=");
	Out += FString::Printf(TEXT("%lld;"), Value);
}

void AppendBool(FString& Out, const TCHAR* Key, const bool bValue)
{
	AppendInt(Out, Key, bValue ? 1 : 0);
}

void AppendString(FString& Out, const TCHAR* Key, const FString& Value)
{
	Out += Key;
	Out += TEXT("=");
	Out += FString::Printf(TEXT("%d:"), Value.Len());
	Out += Value;
	Out += TEXT(";");
}

void AppendName(FString& Out, const TCHAR* Key, const FName Value)
{
	AppendString(Out, Key, Value.IsNone() ? FString() : Value.ToString());
}

FString CanonicalTerminalName(const FName Value)
{
	return Value.IsNone() ? FString() : Value.ToString().ToLower();
}

void AppendTile(FString& Out, const TCHAR* Key, const FWBTile& Tile)
{
	AppendInt(Out, *FString::Printf(TEXT("%s.x"), Key), Tile.X);
	AppendInt(Out, *FString::Printf(TEXT("%s.y"), Key), Tile.Y);
}

bool WallLess(const FWBWallEdge& Left, const FWBWallEdge& Right)
{
	const FWBWallEdge A = Left.GetNormalized();
	const FWBWallEdge B = Right.GetNormalized();
	if (A.A.Y != B.A.Y) return A.A.Y < B.A.Y;
	if (A.A.X != B.A.X) return A.A.X < B.A.X;
	if (A.B.Y != B.B.Y) return A.B.Y < B.B.Y;
	return A.B.X < B.B.X;
}

FString CanonicalGameState(const FWBGameStateData& State)
{
	FString Out;
	Out.Reserve(16384);
	AppendInt(Out, TEXT("current_player"), State.CurrentPlayer);
	AppendInt(Out, TEXT("priority_player"), State.PriorityPlayer);
	AppendInt(Out, TEXT("first_player"), State.FirstPlayerId);
	AppendInt(Out, TEXT("turn_number"), State.TurnNumber);
	AppendInt(Out, TEXT("game_phase"), static_cast<int32>(State.Phase));
	AppendBool(Out, TEXT("initial_setup"), State.bInitialSetupInProgress);
	AppendBool(Out, TEXT("suppress_reacts"), State.bSuppressManualReactsDuringInitialHeroSetup);
	if (State.HasOpenReactionWindow())
	{
		AppendInt(Out, TEXT("reaction.kind"), static_cast<int32>(State.ReactionWindow.Kind));
		AppendInt(Out, TEXT("reaction.origin"), State.ReactionWindow.OriginatingPlayerId);
		AppendInt(Out, TEXT("reaction.passes"), State.ReactionWindow.ConsecutivePassCount);
		AppendString(Out, TEXT("reaction.action"), State.ReactionWindow.SourceActionId);
		AppendInt(Out, TEXT("reaction.source_unit"), State.ReactionWindow.SourceUnitId);
		AppendInt(Out, TEXT("reaction.target_unit"), State.ReactionWindow.TargetUnitId);
	}
	if (State.HasPendingAttack()
		&& State.PendingAttack.DamageSubstitution.bActive)
	{
		AppendInt(
			Out,
			TEXT("attack.damage_substitution.protected"),
			State.PendingAttack.DamageSubstitution.ProtectedUnitId);
		AppendInt(
			Out,
			TEXT("attack.damage_substitution.substitute"),
			State.PendingAttack.DamageSubstitution.SubstituteUnitId);
	}
	if (State.HasPendingAttack()
		&& State.PendingAttack.DamageCalculation.bValid)
	{
		const FWBPendingAttackState::FDamageCalculation& Calculation =
			State.PendingAttack.DamageCalculation;
		AppendInt(Out, TEXT("attack.damage.hit"), Calculation.HitUnitId);
		AppendInt(Out, TEXT("attack.damage.raw"), Calculation.RawAttackDamage);
		AppendInt(Out, TEXT("attack.damage.previous_hp"), Calculation.PreviousHP);
		AppendInt(Out, TEXT("attack.damage.previous_armor"), Calculation.PreviousArmor);
		AppendInt(Out, TEXT("attack.damage.calculated_armor"), Calculation.CalculatedArmor);
		AppendInt(Out, TEXT("attack.damage.armor_absorbed"), Calculation.ArmorAbsorbedAmount);
		AppendInt(Out, TEXT("attack.damage.hp"), Calculation.CalculatedHPDamage);
		AppendBool(Out, TEXT("attack.damage.frozen"), Calculation.bFrozenBreak);
		AppendBool(Out, TEXT("attack.damage.prevented"), Calculation.bPrevented);
		AppendInt(
			Out,
			TEXT("attack.damage.final_recipient"),
			State.PendingAttack.FinalDamageRecipientUnitId);
	}
	AppendBool(Out, TEXT("game_over"), State.bGameOver);
	AppendInt(Out, TEXT("winner"), State.WinnerPlayerId);
	if (State.bGameOver)
	{
		AppendInt(Out, TEXT("terminal.winner"), State.TerminalOutcome.WinnerPlayerId);
		AppendInt(Out, TEXT("terminal.loser"), State.TerminalOutcome.LoserPlayerId);
		AppendString(Out, TEXT("terminal.reason"), CanonicalTerminalName(
			WBTerminalOutcomeNames::ReasonToName(State.TerminalOutcome.Reason)));
		AppendString(Out, TEXT("terminal.source"), CanonicalTerminalName(
			WBTerminalOutcomeNames::SourceToName(State.TerminalOutcome.Source)));
		AppendInt(Out, TEXT("terminal.turn"), State.TerminalOutcome.TurnNumber);
		AppendInt(Out, TEXT("terminal.revision"), State.TerminalOutcome.CoordinatorRevision);
		AppendInt(Out, TEXT("terminal.trace_index"), State.TerminalOutcome.TraceIndex);
	}

	TArray<FWBUnitState> Units = State.Units;
	Units.Sort([](const FWBUnitState& A, const FWBUnitState& B)
	{
		return A.UnitId < B.UnitId;
	});
	AppendInt(Out, TEXT("unit_count"), Units.Num());
	for (const FWBUnitState& Unit : Units)
	{
		AppendInt(Out, TEXT("unit.id"), Unit.UnitId);
		AppendInt(Out, TEXT("unit.owner"), Unit.OwnerId);
		AppendString(Out, TEXT("unit.card"), Unit.CardId);
		AppendInt(Out, TEXT("unit.x"), Unit.X);
		AppendInt(Out, TEXT("unit.y"), Unit.Y);
		AppendInt(Out, TEXT("unit.hp"), Unit.HP);
		AppendInt(Out, TEXT("unit.max_hp"), Unit.MaxHP);
		AppendInt(Out, TEXT("unit.armor"), Unit.CurrentArmor);
		AppendInt(Out, TEXT("unit.max_armor"), Unit.MaxArmor);
		AppendInt(Out, TEXT("unit.atk"), Unit.ATK);
		AppendInt(Out, TEXT("unit.ar"), Unit.AR);
		AppendInt(Out, TEXT("unit.base_rl"), Unit.BaseRL);
		AppendInt(Out, TEXT("unit.current_rl"), Unit.CurrentRL);
		AppendInt(Out, TEXT("unit.rl_total"), Unit.RLTotal);
		AppendInt(Out, TEXT("unit.rl_used"), Unit.RLUsed);
		AppendInt(Out, TEXT("unit.attacks"), Unit.AttacksLeft);
		AppendInt(Out, TEXT("unit.max_attacks"), Unit.MaxAttacksPerTurn);
		AppendInt(Out, TEXT("unit.mp"), Unit.MPRemaining);
		AppendInt(Out, TEXT("unit.npc_order"), Unit.NPCSpawnOrder);
		AppendInt(Out, TEXT("unit.npc_turn"), Unit.NPCCreationTurnNumber);
		AppendInt(Out, TEXT("unit.npc_trigger"), Unit.NPCTriggeredByUnitId);
		AppendBool(Out, TEXT("unit.defeated"), Unit.bDefeated);
		AppendBool(Out, TEXT("unit.removed"), Unit.bRemovedFromBoard);

		TArray<FName> Statuses = Unit.Statuses.Array();
		Statuses.Sort(FNameLexicalLess());
		AppendInt(Out, TEXT("unit.status_count"), Statuses.Num());
		for (const FName Status : Statuses)
		{
			AppendName(Out, TEXT("unit.status"), Status);
			AppendInt(Out, TEXT("unit.status_turns"), Unit.StatusTurnsRemaining.FindRef(Status));
		}
		TArray<FName> Passives = Unit.Passives.Array();
		Passives.Sort(FNameLexicalLess());
		AppendInt(Out, TEXT("unit.passive_count"), Passives.Num());
		for (const FName Passive : Passives)
		{
			AppendName(Out, TEXT("unit.passive"), Passive);
		}
		TArray<EWBCombatCapability> CombatCapabilities = Unit.CombatCapabilities.Array();
		CombatCapabilities.Sort([](const EWBCombatCapability A, const EWBCombatCapability B)
		{
			return static_cast<uint8>(A) < static_cast<uint8>(B);
		});
		if (!CombatCapabilities.IsEmpty())
		{
			AppendInt(Out, TEXT("unit.combat_capability_count"), CombatCapabilities.Num());
			for (const EWBCombatCapability Capability : CombatCapabilities)
			{
				AppendInt(Out, TEXT("unit.combat_capability"), static_cast<int32>(Capability));
			}
		}
		TArray<FWBResonanceModifierState> Modifiers = Unit.ResonanceModifiers;
		Modifiers.Sort([](const FWBResonanceModifierState& A, const FWBResonanceModifierState& B)
		{
			if (A.SourceId != B.SourceId) return A.SourceId < B.SourceId;
			if (A.Target != B.Target) return static_cast<int32>(A.Target) < static_cast<int32>(B.Target);
			if (A.Operation != B.Operation) return static_cast<int32>(A.Operation) < static_cast<int32>(B.Operation);
			return A.Amount < B.Amount;
		});
		AppendInt(Out, TEXT("unit.modifier_count"), Modifiers.Num());
		for (const FWBResonanceModifierState& Modifier : Modifiers)
		{
			AppendString(Out, TEXT("unit.modifier.source"), Modifier.SourceId);
			AppendInt(Out, TEXT("unit.modifier.target"), static_cast<int32>(Modifier.Target));
			AppendInt(Out, TEXT("unit.modifier.operation"), static_cast<int32>(Modifier.Operation));
			AppendInt(Out, TEXT("unit.modifier.amount"), Modifier.Amount);
		}
	}

	TArray<FWBWallEdge> Walls = State.Walls;
	for (FWBWallEdge& Wall : Walls) Wall = Wall.GetNormalized();
	Walls.Sort(WallLess);
	AppendInt(Out, TEXT("wall_count"), Walls.Num());
	for (const FWBWallEdge& Wall : Walls)
	{
		AppendTile(Out, TEXT("wall.a"), Wall.A);
		AppendTile(Out, TEXT("wall.b"), Wall.B);
	}
	AppendName(Out, TEXT("default_terrain"), State.DefaultTerrainId);
	TArray<int32> TerrainKeys;
	State.TerrainByTileIndex.GetKeys(TerrainKeys);
	TerrainKeys.Sort();
	AppendInt(Out, TEXT("terrain_count"), TerrainKeys.Num());
	for (const int32 Key : TerrainKeys)
	{
		AppendInt(Out, TEXT("terrain.tile"), Key);
		AppendName(Out, TEXT("terrain.id"), State.TerrainByTileIndex.FindRef(Key));
	}

	TArray<FWBPlayerStateData> Players = State.Players;
	Players.Sort([](const FWBPlayerStateData& A, const FWBPlayerStateData& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	AppendInt(Out, TEXT("player_count"), Players.Num());
	for (const FWBPlayerStateData& Player : Players)
	{
		AppendInt(Out, TEXT("player.id"), Player.PlayerId);
		AppendInt(Out, TEXT("player.hero"), Player.HeroUnitId);
		AppendInt(Out, TEXT("player.walls"), Player.WallsLeft);
		AppendInt(Out, TEXT("player.wall_removals"), Player.WallRemovalsLeft);
		AppendInt(Out, TEXT("player.mp"), Player.RemainingMP);
		AppendInt(Out, TEXT("player.roll"), Player.LastMPRoll);
		AppendInt(Out, TEXT("player.legacy_deck_count"), Player.Deck.Num());
		for (const FString& Card : Player.Deck) AppendString(Out, TEXT("player.legacy_deck"), Card);
		AppendInt(Out, TEXT("player.legacy_hand_count"), Player.Hand.Num());
		for (const FString& Card : Player.Hand) AppendString(Out, TEXT("player.legacy_hand"), Card);
		AppendInt(Out, TEXT("player.legacy_discard_count"), Player.Discard.Num());
		for (const FString& Card : Player.Discard) AppendString(Out, TEXT("player.legacy_discard"), Card);
	}

	AppendBool(Out, TEXT("attack.active"), State.PendingAttack.bActive);
	if (State.PendingAttack.bActive
		&& State.PendingAttack.AuthorityKind == EWBAttackAuthorityKind::NeutralNPC)
	{
		AppendInt(Out, TEXT("attack.authority"), static_cast<int32>(State.PendingAttack.AuthorityKind));
	}
	AppendInt(Out, TEXT("attack.attacker"), State.PendingAttack.AttackerUnitId);
	AppendInt(Out, TEXT("attack.defender"), State.PendingAttack.DefenderUnitId);
	AppendInt(Out, TEXT("attack.player"), State.PendingAttack.AttackingPlayerId);
	AppendTile(Out, TEXT("attack.from"), State.PendingAttack.AttackerTile);
	AppendTile(Out, TEXT("attack.to"), State.PendingAttack.DefenderTile);
	AppendString(Out, TEXT("attack.action"), State.PendingAttack.DeclarationActionId);
	if (State.PendingAttack.bActive)
	{
		AppendInt(Out, TEXT("attack.stage"), static_cast<int32>(State.PendingAttack.Stage));
		AppendInt(Out, TEXT("attack.original_attacker"), State.PendingAttack.OriginalAttackerUnitId);
		AppendInt(Out, TEXT("attack.original_defender"), State.PendingAttack.OriginalDefenderUnitId);
		AppendString(Out, TEXT("attack.continuation"), State.PendingAttack.ContinuationId);
		AppendBool(Out, TEXT("attack.prevented"), State.PendingAttack.bPrevented);
		AppendBool(Out, TEXT("attack.damage_resolved"), State.PendingAttack.bDamageResolved);
		AppendBool(Out, TEXT("attack.post_hit_completed"), State.PendingAttack.bPostHitCompleted);
		AppendBool(Out, TEXT("attack.frozen_broken"), State.PendingAttack.bFrozenBroken);
		AppendBool(Out, TEXT("attack.counter"), State.PendingAttack.bCounter);
	}
	if (State.NPCPhaseContinuation.bActive)
	{
		AppendBool(Out, TEXT("npc_phase.active"), true);
		AppendInt(Out, TEXT("npc_phase.owner"), State.NPCPhaseContinuation.PhaseOwnerPlayerId);
		AppendInt(Out, TEXT("npc_phase.queue_index"), State.NPCPhaseContinuation.QueueIndex);
		AppendInt(Out, TEXT("npc_phase.current_unit"), State.NPCPhaseContinuation.CurrentNPCUnitId);
		AppendInt(Out, TEXT("npc_phase.action_sequence"), State.NPCPhaseContinuation.CurrentActionSequence);
		AppendInt(Out, TEXT("npc_phase.path_step"), State.NPCPhaseContinuation.CurrentPathStepIndex);
		AppendBool(Out, TEXT("npc_phase.current_started"), State.NPCPhaseContinuation.bCurrentNPCStarted);
		AppendBool(Out, TEXT("npc_phase.current_progress"), State.NPCPhaseContinuation.bCurrentNPCMadeProgress);
		AppendBool(Out, TEXT("npc_phase.waiting_attack"), State.NPCPhaseContinuation.bWaitingForAttackContinuation);
		AppendInt(Out, TEXT("npc_phase.queue_count"), State.NPCPhaseContinuation.OrderedNPCUnitIds.Num());
		for (const int32 UnitId : State.NPCPhaseContinuation.OrderedNPCUnitIds)
		{
			AppendInt(Out, TEXT("npc_phase.queue_unit"), UnitId);
		}
	}

	TArray<FWBPendingNPCSpawnState> Spawns = State.PendingNPCSpawns;
	Spawns.Sort([](const FWBPendingNPCSpawnState& A, const FWBPendingNPCSpawnState& B)
	{
		if (A.SpawnOrder != B.SpawnOrder) return A.SpawnOrder < B.SpawnOrder;
		return A.PendingSpawnId < B.PendingSpawnId;
	});
	AppendInt(Out, TEXT("spawn_count"), Spawns.Num());
	for (const FWBPendingNPCSpawnState& Spawn : Spawns)
	{
		AppendInt(Out, TEXT("spawn.id"), Spawn.PendingSpawnId);
		AppendInt(Out, TEXT("spawn.marker"), Spawn.SourceMarkerId);
		AppendInt(Out, TEXT("spawn.marker_owner"), Spawn.MarkerOwnerPlayerId);
		AppendString(Out, TEXT("spawn.card"), Spawn.NPCDefinitionId);
		AppendTile(Out, TEXT("spawn.origin"), Spawn.OriginTile);
		AppendInt(Out, TEXT("spawn.order"), Spawn.SpawnOrder);
		AppendInt(Out, TEXT("spawn.trigger_unit"), Spawn.TriggeredByUnitId);
		AppendInt(Out, TEXT("spawn.trigger_owner"), Spawn.TriggeredByOwnerId);
		AppendInt(Out, TEXT("spawn.turn"), Spawn.CreatedTurnNumber);
		AppendInt(Out, TEXT("spawn.retry"), Spawn.RetryCount);
	}

	AppendInt(
		Out,
		TEXT("destruction_event_count"),
		State.PendingUnitDestructionEvents.Num());
	for (const FWBUnitDestructionSnapshot& Event :
		State.PendingUnitDestructionEvents)
	{
		AppendString(Out, TEXT("destruction.id"), Event.EventId);
		AppendInt(Out, TEXT("destruction.unit"), Event.DestroyedUnitId);
		AppendString(Out, TEXT("destruction.card"), Event.DestroyedCardId);
		AppendInt(Out, TEXT("destruction.controller"), Event.ControllerPlayerId);
		AppendTile(Out, TEXT("destruction.tile"), Event.LastTile);
		AppendBool(Out, TEXT("destruction.hero"), Event.bWasHero);
		AppendInt(Out, TEXT("destruction.cause"), static_cast<int32>(Event.Cause));
		AppendInt(Out, TEXT("destruction.base_rl"), Event.BaseRLSnapshot);
		AppendInt(Out, TEXT("destruction.current_rl"), Event.CurrentRLSnapshot);
		AppendInt(Out, TEXT("destruction.rl_used"), Event.RLUsedSnapshot);
		AppendBool(
			Out,
			TEXT("destruction.passive_eligible"),
			Event.bCharacterPassiveEligible);
		AppendInt(Out, TEXT("destruction.order"), Event.ResolutionOrder);
		AppendInt(Out, TEXT("destruction.next_trigger"), Event.NextTriggerIndex);
		AppendInt(
			Out,
			TEXT("destruction.next_observer_trigger"),
			Event.NextObserverTriggerIndex);
		AppendInt(
			Out,
			TEXT("destruction.observer_source_count"),
			Event.ObserverSources.Num());
		for (const FWBPostDestructionObserverSourceSnapshot& Observer :
			Event.ObserverSources)
		{
			AppendInt(Out, TEXT("destruction.observer.unit"), Observer.SourceUnitId);
			AppendString(Out, TEXT("destruction.observer.card"), Observer.SourceCardId);
			AppendInt(
				Out,
				TEXT("destruction.observer.controller"),
				Observer.ControllerPlayerId);
			AppendInt(Out, TEXT("destruction.observer.order"), Observer.SourceOrder);
		}
		AppendInt(Out, TEXT("destruction.wand_count"), Event.EquippedWands.Num());
		for (const FWBEquippedCardEntry& Wand : Event.EquippedWands)
		{
			AppendString(Out, TEXT("destruction.wand.instance"), Wand.Card.InstanceId);
			AppendString(Out, TEXT("destruction.wand.card"), Wand.Card.CardId);
			AppendInt(Out, TEXT("destruction.wand.owner"), Wand.Card.OwnerPlayerId);
			AppendString(Out, TEXT("destruction.wand.slot"), Wand.SlotId);
			AppendInt(Out, TEXT("destruction.wand.order"), Wand.EquipOrder);
		}
	}
	AppendBool(
		Out,
		TEXT("mandatory_choice.active"),
		State.PendingMandatoryDeckChoice.bActive);
	if (State.PendingMandatoryDeckChoice.bActive)
	{
		const FWBPendingMandatoryDeckChoiceState& Choice =
			State.PendingMandatoryDeckChoice;
		AppendInt(
			Out,
			TEXT("mandatory_choice.origin"),
			static_cast<int32>(Choice.Origin));
		AppendString(Out, TEXT("mandatory_choice.id"), Choice.ChoiceId);
		AppendString(
			Out,
			TEXT("mandatory_choice.event"),
			Choice.DestructionEventId);
		AppendString(Out, TEXT("mandatory_choice.trigger"), Choice.TriggerId);
		AppendString(
			Out, TEXT("mandatory_choice.source_action"),
			Choice.SourceActionId);
		AppendString(
			Out, TEXT("mandatory_choice.source_frame"),
			Choice.SourceEffectFrameId);
		AppendInt(
			Out,
			TEXT("mandatory_choice.controller"),
			Choice.ControllerPlayerId);
		AppendInt(
			Out,
			TEXT("mandatory_choice.resume_priority"),
			Choice.ResumePriorityPlayerId);
		AppendInt(
			Out,
			TEXT("mandatory_choice.resume_phase"),
			Choice.ResumeMatchPhase);
		AppendString(
			Out, TEXT("mandatory_choice.required_faction"),
			Choice.RequiredFaction);
		AppendTile(
			Out, TEXT("mandatory_choice.destination"),
			Choice.DestinationTile);
		AppendBool(
			Out, TEXT("mandatory_choice.inheritance"),
			Choice.bApplyCSNInheritance);
		const bool bActivatedEffectContinuation = Choice.Origin
			== EWBMandatoryDeckChoiceOrigin::ActivatedEffectContinuation;
		const int32 SourceUnitId = bActivatedEffectContinuation
			? Choice.ActivatedEffectSourceSnapshot.SourceUnitId
			: Choice.SourceSnapshot.DestroyedUnitId;
		const FString& SourceCardId = bActivatedEffectContinuation
			? Choice.ActivatedEffectSourceSnapshot.SourceCardId
			: Choice.SourceSnapshot.DestroyedCardId;
		const int32 SourceControllerPlayerId = bActivatedEffectContinuation
			? Choice.ActivatedEffectSourceSnapshot.ControllerPlayerId
			: Choice.SourceSnapshot.ControllerPlayerId;
		const FWBTile& SourceTile = bActivatedEffectContinuation
			? Choice.ActivatedEffectSourceSnapshot.SourceTile
			: Choice.SourceSnapshot.LastTile;
		const int32 SourceBaseRL = bActivatedEffectContinuation
			? Choice.ActivatedEffectSourceSnapshot.BaseRLSnapshot
			: Choice.SourceSnapshot.BaseRLSnapshot;
		const int32 SourceCurrentRL = bActivatedEffectContinuation
			? Choice.ActivatedEffectSourceSnapshot.CurrentRLSnapshot
			: Choice.SourceSnapshot.CurrentRLSnapshot;
		const int32 SourceRLUsed = bActivatedEffectContinuation
			? Choice.ActivatedEffectSourceSnapshot.RLUsedSnapshot
			: Choice.SourceSnapshot.RLUsedSnapshot;
		const TArray<FWBEquippedCardEntry>& SourceWands =
			bActivatedEffectContinuation
				? Choice.ActivatedEffectSourceSnapshot.EquippedWands
				: Choice.SourceSnapshot.EquippedWands;
		AppendInt(
			Out, TEXT("mandatory_choice.source_unit"),
			SourceUnitId);
		AppendString(
			Out, TEXT("mandatory_choice.source_card"),
			SourceCardId);
		AppendInt(
			Out, TEXT("mandatory_choice.source_controller"),
			SourceControllerPlayerId);
		AppendTile(
			Out, TEXT("mandatory_choice.source_tile"),
			SourceTile);
		AppendInt(
			Out, TEXT("mandatory_choice.source_base_rl"),
			SourceBaseRL);
		AppendInt(
			Out, TEXT("mandatory_choice.source_current_rl"),
			SourceCurrentRL);
		AppendInt(
			Out, TEXT("mandatory_choice.source_rl_used"),
			SourceRLUsed);
		AppendInt(
			Out, TEXT("mandatory_choice.source_wand_count"),
			SourceWands.Num());
		for (const FWBEquippedCardEntry& Wand : SourceWands)
		{
			AppendString(
				Out, TEXT("mandatory_choice.source_wand.instance"),
				Wand.Card.InstanceId);
			AppendString(
				Out, TEXT("mandatory_choice.source_wand.card"),
				Wand.Card.CardId);
			AppendInt(
				Out, TEXT("mandatory_choice.source_wand.owner"),
				Wand.Card.OwnerPlayerId);
			AppendString(
				Out, TEXT("mandatory_choice.source_wand.slot"),
				Wand.SlotId);
			AppendInt(
				Out, TEXT("mandatory_choice.source_wand.order"),
				Wand.EquipOrder);
		}
		AppendInt(
			Out,
			TEXT("mandatory_choice.option_count"),
			Choice.EligibleCardInstanceIds.Num());
		for (const FString& InstanceId : Choice.EligibleCardInstanceIds)
		{
			AppendString(
				Out,
				TEXT("mandatory_choice.option"),
				InstanceId);
		}
	}

	TArray<int32> UsagePlayers;
	State.ActivationUsageKeysThisTurn.GetKeys(UsagePlayers);
	UsagePlayers.Sort();
	AppendInt(Out, TEXT("usage_player_count"), UsagePlayers.Num());
	for (const int32 PlayerId : UsagePlayers)
	{
		AppendInt(Out, TEXT("usage.player"), PlayerId);
		TArray<FString> Keys = State.ActivationUsageKeysThisTurn.FindRef(PlayerId).Array();
		Keys.Sort();
		AppendInt(Out, TEXT("usage.key_count"), Keys.Num());
		for (const FString& Key : Keys) AppendString(Out, TEXT("usage.key"), Key);
	}

	TArray<FWBPlayerCardZoneState> PlayerZones = State.CardZoneState.PlayerZones;
	PlayerZones.Sort([](const FWBPlayerCardZoneState& A, const FWBPlayerCardZoneState& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	AppendInt(Out, TEXT("zone_player_count"), PlayerZones.Num());
	const auto AppendZone = [&Out](const TCHAR* Prefix, const TArray<FWBZoneCardEntry>& Entries)
	{
		AppendInt(Out, *FString::Printf(TEXT("%s.count"), Prefix), Entries.Num());
		for (const FWBZoneCardEntry& Entry : Entries)
		{
			AppendString(Out, *FString::Printf(TEXT("%s.instance"), Prefix), Entry.Card.InstanceId);
			AppendString(Out, *FString::Printf(TEXT("%s.card"), Prefix), Entry.Card.CardId);
			AppendInt(Out, *FString::Printf(TEXT("%s.owner"), Prefix), Entry.Card.OwnerPlayerId);
			AppendInt(Out, *FString::Printf(TEXT("%s.zone"), Prefix), static_cast<int32>(Entry.Zone));
			AppendInt(Out, *FString::Printf(TEXT("%s.index"), Prefix), Entry.ZoneIndex);
		}
	};
	for (const FWBPlayerCardZoneState& Zones : PlayerZones)
	{
		AppendInt(Out, TEXT("zones.player"), Zones.PlayerId);
		AppendZone(TEXT("zones.deck"), Zones.Deck);
		AppendZone(TEXT("zones.hand"), Zones.Hand);
		AppendZone(TEXT("zones.discard"), Zones.Discard);
	}

	TArray<FWBEquippedCardEntry> Equipped = State.CardZoneState.EquippedCards;
	Equipped.Sort([](const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
	{
		if (A.EquippedToUnitId != B.EquippedToUnitId) return A.EquippedToUnitId < B.EquippedToUnitId;
		if (A.EquipOrder != B.EquipOrder) return A.EquipOrder < B.EquipOrder;
		return A.Card.InstanceId < B.Card.InstanceId;
	});
	AppendInt(Out, TEXT("equipped_count"), Equipped.Num());
	for (const FWBEquippedCardEntry& Entry : Equipped)
	{
		AppendString(Out, TEXT("equipped.instance"), Entry.Card.InstanceId);
		AppendString(Out, TEXT("equipped.card"), Entry.Card.CardId);
		AppendInt(Out, TEXT("equipped.owner"), Entry.Card.OwnerPlayerId);
		AppendInt(Out, TEXT("equipped.unit"), Entry.EquippedToUnitId);
		AppendString(Out, TEXT("equipped.slot"), Entry.SlotId);
		AppendInt(Out, TEXT("equipped.order"), Entry.EquipOrder);
	}

	TArray<FWBMarkerPlaceholderEntry> Markers = State.CardZoneState.MarkerPlaceholders;
	Markers.Sort([](const FWBMarkerPlaceholderEntry& A, const FWBMarkerPlaceholderEntry& B)
	{
		if (A.PlacementOrder != B.PlacementOrder) return A.PlacementOrder < B.PlacementOrder;
		return A.MarkerId < B.MarkerId;
	});
	AppendInt(Out, TEXT("marker_count"), Markers.Num());
	for (const FWBMarkerPlaceholderEntry& Marker : Markers)
	{
		AppendInt(Out, TEXT("marker.id"), Marker.MarkerId);
		AppendInt(Out, TEXT("marker.owner"), Marker.OwnerPlayerId);
		AppendInt(Out, TEXT("marker.type"), static_cast<int32>(Marker.Type));
		AppendTile(Out, TEXT("marker.tile"), Marker.Tile);
		AppendInt(Out, TEXT("marker.order"), Marker.PlacementOrder);
		AppendInt(Out, TEXT("marker.public"), static_cast<int32>(Marker.PublicState));
		AppendString(Out, TEXT("marker.internal_card"), Marker.InternalMarkerCardId);
	}
	return Out;
}

FString CanonicalHeader(const FWBProductionMatchReplayHeader& Header)
{
	FString Out;
	AppendInt(Out, TEXT("schema_version"), Header.SchemaVersion);
	AppendString(Out, TEXT("replay_format_id"), Header.ReplayFormatId);
	AppendString(Out, TEXT("opaque_match_id"), Header.OpaqueMatchId);
	AppendInt(Out, TEXT("rules_compatibility_version"), Header.RulesCompatibilityVersion);
	AppendString(Out, TEXT("production_bundle_digest"), Header.ProductionBundleDigest);
	AppendString(Out, TEXT("production_match_spec_digest"), Header.ProductionMatchSpecDigest);
	AppendString(Out, TEXT("active_format_digest"), Header.ActiveFormatDigest);
	AppendString(Out, TEXT("game_start_addendum_digest"), Header.GameStartAddendumDigest);
	AppendInt(Out, TEXT("initial_match_seed"), Header.InitialMatchSeed);
	AppendInt(Out, TEXT("initial_generation"), Header.InitialCoordinatorGeneration);
	AppendInt(Out, TEXT("initial_revision"), Header.InitialCoordinatorRevision);
	AppendString(Out, TEXT("initial_state_digest"), Header.InitialStateDigest);
	AppendString(Out, TEXT("initial_trace_digest"), Header.InitialTraceDigest);
	AppendString(Out, TEXT("previous_record_hash"), Header.PreviousRecordHash);
	return Out;
}

FString CanonicalRecord(const FWBProductionMatchReplayActionRecord& Record)
{
	FString Out;
	AppendInt(Out, TEXT("record_index"), Record.RecordIndex);
	AppendInt(Out, TEXT("acting_player"), Record.ActingPlayer);
	AppendString(Out, TEXT("action_family"), Record.ActionFamily);
	AppendString(Out, TEXT("chosen_action_id"), Record.ChosenActionId);
	AppendString(Out, TEXT("expected_decision_id"), Record.ExpectedDecisionId);
	AppendInt(Out, TEXT("before_generation"), Record.BeforeGeneration);
	AppendInt(Out, TEXT("before_revision"), Record.BeforeRevision);
	AppendString(Out, TEXT("before_state_digest"), Record.BeforeStateDigest);
	AppendString(Out, TEXT("legal_action_set_digest"), Record.LegalActionSetDigest);
	AppendInt(Out, TEXT("after_generation"), Record.AfterGeneration);
	AppendInt(Out, TEXT("after_revision"), Record.AfterRevision);
	AppendBool(Out, TEXT("completed"), Record.bCompleted);
	AppendBool(Out, TEXT("pending_decision"), Record.bPendingDecision);
	AppendInt(Out, TEXT("pending_player"), Record.PendingPlayer);
	AppendBool(Out, TEXT("terminal"), Record.bTerminal);
	if (Record.bTerminal)
	{
		AppendInt(Out, TEXT("winner"), Record.WinnerPlayer);
		AppendInt(Out, TEXT("loser"), Record.LoserPlayer);
		AppendString(Out, TEXT("terminal_reason"),
			CanonicalTerminalName(Record.TerminalReason));
		AppendString(Out, TEXT("terminal_source"),
			CanonicalTerminalName(Record.TerminalSource));
		AppendInt(Out, TEXT("terminal_turn"), Record.TerminalTurn);
		AppendInt(Out, TEXT("terminal_revision"), Record.TerminalRevision);
		AppendInt(Out, TEXT("terminal_trace_index"), Record.TerminalTraceIndex);
	}
	AppendInt(Out, TEXT("trace_start"), Record.TraceStart);
	AppendInt(Out, TEXT("trace_end"), Record.TraceEnd);
	AppendString(Out, TEXT("trace_digest"), Record.TraceDigest);
	AppendString(Out, TEXT("after_state_digest"), Record.AfterStateDigest);
	AppendString(Out, TEXT("previous_record_hash"), Record.PreviousRecordHash);
	return Out;
}

void WriteHeader(TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer, const FWBProductionMatchReplayHeader& Header)
{
	Writer.WriteObjectStart(TEXT("header"));
	Writer.WriteValue(TEXT("schema_version"), Header.SchemaVersion);
	Writer.WriteValue(TEXT("replay_format_id"), Header.ReplayFormatId);
	Writer.WriteValue(TEXT("opaque_match_id"), Header.OpaqueMatchId);
	Writer.WriteValue(TEXT("rules_compatibility_version"), Header.RulesCompatibilityVersion);
	Writer.WriteValue(TEXT("production_bundle_digest"), Header.ProductionBundleDigest);
	Writer.WriteValue(TEXT("production_match_spec_digest"), Header.ProductionMatchSpecDigest);
	Writer.WriteValue(TEXT("active_format_digest"), Header.ActiveFormatDigest);
	Writer.WriteValue(TEXT("game_start_addendum_digest"), Header.GameStartAddendumDigest);
	Writer.WriteValue(TEXT("initial_match_seed"), Header.InitialMatchSeed);
	Writer.WriteValue(TEXT("initial_coordinator_generation"), Header.InitialCoordinatorGeneration);
	Writer.WriteValue(TEXT("initial_coordinator_revision"), Header.InitialCoordinatorRevision);
	Writer.WriteValue(TEXT("initial_state_digest"), Header.InitialStateDigest);
	Writer.WriteValue(TEXT("initial_trace_digest"), Header.InitialTraceDigest);
	Writer.WriteValue(TEXT("previous_record_hash"), Header.PreviousRecordHash);
	Writer.WriteValue(TEXT("header_hash"), Header.HeaderHash);
	Writer.WriteObjectEnd();
}

void WriteRecord(TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer, const FWBProductionMatchReplayActionRecord& Record)
{
	Writer.WriteObjectStart();
	Writer.WriteValue(TEXT("record_index"), Record.RecordIndex);
	Writer.WriteValue(TEXT("acting_player"), Record.ActingPlayer);
	Writer.WriteValue(TEXT("action_family"), Record.ActionFamily);
	Writer.WriteValue(TEXT("chosen_action_id"), Record.ChosenActionId);
	Writer.WriteValue(TEXT("expected_decision_id"), Record.ExpectedDecisionId);
	Writer.WriteValue(TEXT("before_generation"), Record.BeforeGeneration);
	Writer.WriteValue(TEXT("before_revision"), Record.BeforeRevision);
	Writer.WriteValue(TEXT("before_state_digest"), Record.BeforeStateDigest);
	Writer.WriteValue(TEXT("legal_action_set_digest"), Record.LegalActionSetDigest);
	Writer.WriteValue(TEXT("after_generation"), Record.AfterGeneration);
	Writer.WriteValue(TEXT("after_revision"), Record.AfterRevision);
	Writer.WriteValue(TEXT("completed"), Record.bCompleted);
	Writer.WriteValue(TEXT("pending_decision"), Record.bPendingDecision);
	Writer.WriteValue(TEXT("pending_player"), Record.PendingPlayer);
	Writer.WriteValue(TEXT("terminal"), Record.bTerminal);
	if (Record.bTerminal)
	{
		Writer.WriteValue(TEXT("winner"), Record.WinnerPlayer);
		Writer.WriteValue(TEXT("loser"), Record.LoserPlayer);
		Writer.WriteValue(TEXT("terminal_reason"),
			CanonicalTerminalName(Record.TerminalReason));
		Writer.WriteValue(TEXT("terminal_source"),
			CanonicalTerminalName(Record.TerminalSource));
		Writer.WriteValue(TEXT("terminal_turn"), Record.TerminalTurn);
		Writer.WriteValue(TEXT("terminal_revision"), Record.TerminalRevision);
		Writer.WriteValue(TEXT("terminal_trace_index"), Record.TerminalTraceIndex);
	}
	Writer.WriteValue(TEXT("trace_start"), Record.TraceStart);
	Writer.WriteValue(TEXT("trace_end"), Record.TraceEnd);
	Writer.WriteValue(TEXT("trace_digest"), Record.TraceDigest);
	Writer.WriteValue(TEXT("after_state_digest"), Record.AfterStateDigest);
	Writer.WriteValue(TEXT("previous_record_hash"), Record.PreviousRecordHash);
	Writer.WriteValue(TEXT("record_hash"), Record.RecordHash);
	Writer.WriteObjectEnd();
}

void WriteFooter(TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>& Writer, const FWBProductionMatchReplayFooter& Footer)
{
	Writer.WriteObjectStart(TEXT("footer"));
	Writer.WriteValue(TEXT("complete"), Footer.bComplete);
	Writer.WriteValue(TEXT("terminal"), Footer.bTerminal);
	Writer.WriteValue(TEXT("winner"), Footer.Winner);
	Writer.WriteValue(TEXT("loser"), Footer.Loser);
	if (Footer.bTerminal)
	{
		Writer.WriteValue(TEXT("terminal_reason"),
			CanonicalTerminalName(Footer.TerminalReason));
		Writer.WriteValue(TEXT("terminal_source"),
			CanonicalTerminalName(Footer.TerminalSource));
		Writer.WriteValue(TEXT("terminal_turn"), Footer.TerminalTurn);
		Writer.WriteValue(TEXT("terminal_generation"), Footer.TerminalGeneration);
		Writer.WriteValue(TEXT("terminal_revision"), Footer.TerminalRevision);
		Writer.WriteValue(TEXT("terminal_trace_index"), Footer.TerminalTraceIndex);
	}
	Writer.WriteValue(TEXT("final_generation"), Footer.FinalGeneration);
	Writer.WriteValue(TEXT("final_revision"), Footer.FinalRevision);
	Writer.WriteValue(TEXT("record_count"), Footer.RecordCount);
	Writer.WriteValue(TEXT("final_state_digest"), Footer.FinalStateDigest);
	Writer.WriteValue(TEXT("final_trace_digest"), Footer.FinalTraceDigest);
	Writer.WriteValue(TEXT("final_record_hash"), Footer.FinalRecordHash);
	Writer.WriteValue(TEXT("replay_digest"), Footer.ReplayDigest);
	Writer.WriteObjectEnd();
}

bool GetString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
{
	return Object.IsValid() && Object->TryGetStringField(Field, Out);
}

bool GetBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool& Out)
{
	return Object.IsValid() && Object->TryGetBoolField(Field, Out);
}

bool GetInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Out)
{
	double Number = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(Field, Number)
		|| !FMath::IsFinite(Number) || FMath::FloorToDouble(Number) != Number
		|| Number < static_cast<double>(MIN_int32)
		|| Number > static_cast<double>(MAX_int32))
	{
		return false;
	}
	Out = static_cast<int32>(Number);
	return true;
}

bool ParseHeader(const TSharedPtr<FJsonObject>& Object, FWBProductionMatchReplayHeader& Header)
{
	return GetInt(Object, TEXT("schema_version"), Header.SchemaVersion)
		&& GetString(Object, TEXT("replay_format_id"), Header.ReplayFormatId)
		&& GetString(Object, TEXT("opaque_match_id"), Header.OpaqueMatchId)
		&& GetInt(Object, TEXT("rules_compatibility_version"), Header.RulesCompatibilityVersion)
		&& GetString(Object, TEXT("production_bundle_digest"), Header.ProductionBundleDigest)
		&& GetString(Object, TEXT("production_match_spec_digest"), Header.ProductionMatchSpecDigest)
		&& GetString(Object, TEXT("active_format_digest"), Header.ActiveFormatDigest)
		&& GetString(Object, TEXT("game_start_addendum_digest"), Header.GameStartAddendumDigest)
		&& GetInt(Object, TEXT("initial_match_seed"), Header.InitialMatchSeed)
		&& GetInt(Object, TEXT("initial_coordinator_generation"), Header.InitialCoordinatorGeneration)
		&& GetInt(Object, TEXT("initial_coordinator_revision"), Header.InitialCoordinatorRevision)
		&& GetString(Object, TEXT("initial_state_digest"), Header.InitialStateDigest)
		&& GetString(Object, TEXT("initial_trace_digest"), Header.InitialTraceDigest)
		&& GetString(Object, TEXT("previous_record_hash"), Header.PreviousRecordHash)
		&& GetString(Object, TEXT("header_hash"), Header.HeaderHash);
}

bool ParseRecord(const TSharedPtr<FJsonObject>& Object, FWBProductionMatchReplayActionRecord& Record)
{
	const bool bBaseValid = GetInt(Object, TEXT("record_index"), Record.RecordIndex)
		&& GetInt(Object, TEXT("acting_player"), Record.ActingPlayer)
		&& GetString(Object, TEXT("action_family"), Record.ActionFamily)
		&& GetString(Object, TEXT("chosen_action_id"), Record.ChosenActionId)
		&& GetString(Object, TEXT("expected_decision_id"), Record.ExpectedDecisionId)
		&& GetInt(Object, TEXT("before_generation"), Record.BeforeGeneration)
		&& GetInt(Object, TEXT("before_revision"), Record.BeforeRevision)
		&& GetString(Object, TEXT("before_state_digest"), Record.BeforeStateDigest)
		&& GetString(Object, TEXT("legal_action_set_digest"), Record.LegalActionSetDigest)
		&& GetInt(Object, TEXT("after_generation"), Record.AfterGeneration)
		&& GetInt(Object, TEXT("after_revision"), Record.AfterRevision)
		&& GetBool(Object, TEXT("completed"), Record.bCompleted)
		&& GetBool(Object, TEXT("pending_decision"), Record.bPendingDecision)
		&& GetInt(Object, TEXT("pending_player"), Record.PendingPlayer)
		&& GetBool(Object, TEXT("terminal"), Record.bTerminal)
		&& GetInt(Object, TEXT("trace_start"), Record.TraceStart)
		&& GetInt(Object, TEXT("trace_end"), Record.TraceEnd)
		&& GetString(Object, TEXT("trace_digest"), Record.TraceDigest)
		&& GetString(Object, TEXT("after_state_digest"), Record.AfterStateDigest)
		&& GetString(Object, TEXT("previous_record_hash"), Record.PreviousRecordHash)
		&& GetString(Object, TEXT("record_hash"), Record.RecordHash);
	if (!bBaseValid || !Record.bTerminal)
	{
		return bBaseValid;
	}
	FString Reason;
	FString Source;
	return GetInt(Object, TEXT("winner"), Record.WinnerPlayer)
		&& GetInt(Object, TEXT("loser"), Record.LoserPlayer)
		&& GetString(Object, TEXT("terminal_reason"), Reason)
		&& GetString(Object, TEXT("terminal_source"), Source)
		&& GetInt(Object, TEXT("terminal_turn"), Record.TerminalTurn)
		&& GetInt(Object, TEXT("terminal_revision"), Record.TerminalRevision)
		&& GetInt(Object, TEXT("terminal_trace_index"), Record.TerminalTraceIndex)
		&& (Record.TerminalReason = FName(*Reason), true)
		&& (Record.TerminalSource = FName(*Source), true);
}

bool ParseFooter(const TSharedPtr<FJsonObject>& Object, FWBProductionMatchReplayFooter& Footer)
{
	const bool bBaseValid = GetBool(Object, TEXT("complete"), Footer.bComplete)
		&& GetBool(Object, TEXT("terminal"), Footer.bTerminal)
		&& GetInt(Object, TEXT("winner"), Footer.Winner)
		&& GetInt(Object, TEXT("loser"), Footer.Loser)
		&& GetInt(Object, TEXT("final_generation"), Footer.FinalGeneration)
		&& GetInt(Object, TEXT("final_revision"), Footer.FinalRevision)
		&& GetInt(Object, TEXT("record_count"), Footer.RecordCount)
		&& GetString(Object, TEXT("final_state_digest"), Footer.FinalStateDigest)
		&& GetString(Object, TEXT("final_trace_digest"), Footer.FinalTraceDigest)
		&& GetString(Object, TEXT("final_record_hash"), Footer.FinalRecordHash)
		&& GetString(Object, TEXT("replay_digest"), Footer.ReplayDigest);
	if (!bBaseValid || !Footer.bTerminal)
	{
		return bBaseValid;
	}
	FString Reason;
	FString Source;
	return GetString(Object, TEXT("terminal_reason"), Reason)
		&& GetString(Object, TEXT("terminal_source"), Source)
		&& GetInt(Object, TEXT("terminal_turn"), Footer.TerminalTurn)
		&& GetInt(Object, TEXT("terminal_generation"), Footer.TerminalGeneration)
		&& GetInt(Object, TEXT("terminal_revision"), Footer.TerminalRevision)
		&& GetInt(Object, TEXT("terminal_trace_index"), Footer.TerminalTraceIndex)
		&& (Footer.TerminalReason = FName(*Reason), true)
		&& (Footer.TerminalSource = FName(*Source), true);
}
}

FString WBProductionMatchReplay::HashUtf8(const FString& Value)
{
	FTCHARToUTF8 Utf8(*Value);
	uint8 Digest[SHA256_DIGEST_LENGTH] = {};
	if (SHA256(reinterpret_cast<const uint8*>(Utf8.Get()), static_cast<size_t>(Utf8.Length()), Digest) == nullptr)
	{
		return FString();
	}
	FString Result;
	Result.Reserve(SHA256_DIGEST_LENGTH * 2);
	for (const uint8 Byte : Digest)
	{
		Result += FString::Printf(TEXT("%02x"), Byte);
	}
	return Result;
}

FString WBProductionMatchReplay::BuildGameStateDigest(const FWBGameStateData& State)
{
	return HashUtf8(CanonicalGameState(State));
}

FString WBProductionMatchReplay::BuildCoordinatorStateDigest(
	const FWBGameStateData& State,
	const int32 MatchPhase,
	const uint32 RandomState,
	const FWBTurnStartSequenceState& TurnStartSequence,
	const FString& AdditionalCanonicalState)
{
	FString Canonical = CanonicalGameState(State);
	AppendInt(Canonical, TEXT("coordinator.phase"), MatchPhase);
	AppendInt(Canonical, TEXT("coordinator.random_state"), RandomState);
	AppendInt(Canonical, TEXT("turn_start.phase"), static_cast<int32>(TurnStartSequence.Phase));
	AppendInt(Canonical, TEXT("turn_start.player"), TurnStartSequence.ActivePlayerId);
	AppendInt(Canonical, TEXT("turn_start.turn"), TurnStartSequence.TurnNumber);
	AppendInt(Canonical, TEXT("turn_start.roll"), TurnStartSequence.MPRoll);
	AppendBool(Canonical, TEXT("turn_start.draw_skipped"), TurnStartSequence.bDrawSkipped);
	AppendBool(Canonical, TEXT("turn_start.draw_done"), TurnStartSequence.bDrawCompleted);
	AppendBool(Canonical, TEXT("turn_start.mp_done"), TurnStartSequence.bMPGenerated);
	AppendBool(Canonical, TEXT("turn_start.resources_done"), TurnStartSequence.bResourcesReset);
	AppendBool(Canonical, TEXT("turn_start.status_done"), TurnStartSequence.bStatusesResolved);
	AppendBool(Canonical, TEXT("turn_start.effects_done"), TurnStartSequence.bEffectsResolved);
	AppendBool(Canonical, TEXT("turn_start.complete"), TurnStartSequence.bCompleted);
	AppendInt(Canonical, TEXT("turn_start.trigger_count"), TurnStartSequence.PendingTriggers.Num());
	for (const FWBTurnStartTriggerInstance& Trigger : TurnStartSequence.PendingTriggers)
	{
		AppendString(Canonical, TEXT("turn_start.trigger_id"), Trigger.StableTriggerId);
		AppendInt(Canonical, TEXT("turn_start.trigger_player"), Trigger.ControllerPlayerId);
		AppendInt(Canonical, TEXT("turn_start.trigger_unit"), Trigger.SourceUnitId);
		AppendString(Canonical, TEXT("turn_start.trigger_card"), Trigger.SourceCardId);
		AppendString(Canonical, TEXT("turn_start.definition_id"), Trigger.Definition.TriggerId);
	}
	Canonical += AdditionalCanonicalState;
	return HashUtf8(Canonical);
}

FString WBProductionMatchReplay::BuildTraceDigest(const TArray<FWBTraceEvent>& Events)
{
	return HashUtf8(WBReplayTrace::SerializeEvents(Events));
}

FString WBProductionMatchReplay::BuildLegalActionSetDigest(
	const TArray<FString>& CanonicalActionEntries)
{
	TArray<FString> Sorted = CanonicalActionEntries;
	Sorted.Sort();
	FString Canonical;
	AppendInt(Canonical, TEXT("count"), Sorted.Num());
	for (const FString& Entry : Sorted)
	{
		AppendString(Canonical, TEXT("action"), Entry);
	}
	return HashUtf8(Canonical);
}

FString WBProductionMatchReplay::BuildDecisionId(
	const int32 Generation,
	const int32 Revision,
	const int32 ActingPlayer,
	const int32 MatchPhase,
	const FString& LegalActionSetDigest)
{
	FString Canonical;
	AppendInt(Canonical, TEXT("generation"), Generation);
	AppendInt(Canonical, TEXT("revision"), Revision);
	AppendInt(Canonical, TEXT("acting_player"), ActingPlayer);
	AppendInt(Canonical, TEXT("match_phase"), MatchPhase);
	AppendString(Canonical, TEXT("legal_action_set_digest"), LegalActionSetDigest);
	return TEXT("decision:") + HashUtf8(Canonical);
}

void WBProductionMatchReplay::RebuildIntegrity(FWBProductionMatchReplayArchive& Archive)
{
	Archive.Header.PreviousRecordHash = ZeroHash;
	Archive.Header.HeaderHash = HashUtf8(CanonicalHeader(Archive.Header));
	FString Previous = Archive.Header.HeaderHash;
	for (int32 Index = 0; Index < Archive.Records.Num(); ++Index)
	{
		FWBProductionMatchReplayActionRecord& Record = Archive.Records[Index];
		Record.RecordIndex = Index;
		Record.PreviousRecordHash = Previous;
		Record.RecordHash = HashUtf8(CanonicalRecord(Record));
		Previous = Record.RecordHash;
	}
	Archive.Footer.RecordCount = Archive.Records.Num();
	Archive.Footer.FinalRecordHash = Previous;
	Archive.Footer.ReplayDigest.Reset();
	Archive.Footer.ReplayDigest = HashUtf8(Serialize(Archive));
}

FString WBProductionMatchReplay::Serialize(const FWBProductionMatchReplayArchive& Archive)
{
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	Writer->WriteObjectStart();
	WriteHeader(*Writer, Archive.Header);
	Writer->WriteArrayStart(TEXT("records"));
	for (const FWBProductionMatchReplayActionRecord& Record : Archive.Records)
	{
		WriteRecord(*Writer, Record);
	}
	Writer->WriteArrayEnd();
	WriteFooter(*Writer, Archive.Footer);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Json;
}

FWBProductionMatchReplayValidationResult WBProductionMatchReplay::DeserializeAndValidate(
	const FString& Json)
{
	FWBProductionMatchReplayValidationResult Result;
	int32 FooterFieldCount = 0;
	int32 FooterSearchIndex = 0;
	while ((FooterSearchIndex = Json.Find(
		TEXT("\"footer\":"),
		ESearchCase::CaseSensitive,
		ESearchDir::FromStart,
		FooterSearchIndex)) != INDEX_NONE)
	{
		++FooterFieldCount;
		FooterSearchIndex += 9;
	}
	if (FooterFieldCount != 1)
	{
		Result.FailureCode = FooterFieldCount > 1
			? FString(TEXT("replay_footer_duplicate"))
			: FString(TEXT("replay_schema_invalid"));
		return Result;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Result.FailureCode = TEXT("replay_schema_invalid");
		return Result;
	}
	const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
	const TSharedPtr<FJsonObject>* FooterObject = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* RecordValues = nullptr;
	if (!Root->TryGetObjectField(TEXT("header"), HeaderObject)
		|| HeaderObject == nullptr || !HeaderObject->IsValid()
		|| !Root->TryGetArrayField(TEXT("records"), RecordValues)
		|| RecordValues == nullptr
		|| !Root->TryGetObjectField(TEXT("footer"), FooterObject)
		|| FooterObject == nullptr || !FooterObject->IsValid()
		|| !ParseHeader(*HeaderObject, Result.Archive.Header)
		|| !ParseFooter(*FooterObject, Result.Archive.Footer))
	{
		Result.FailureCode = TEXT("replay_schema_invalid");
		return Result;
	}
	if (Result.Archive.Header.SchemaVersion != SchemaVersion
		|| Result.Archive.Header.RulesCompatibilityVersion != RulesCompatibilityVersion
		|| Result.Archive.Header.ReplayFormatId != TEXT("WandboundProductionMatchReplay"))
	{
		Result.FailureCode = TEXT("replay_schema_unsupported");
		return Result;
	}
	for (int32 Index = 0; Index < RecordValues->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Object = (*RecordValues)[Index].IsValid()
			? (*RecordValues)[Index]->AsObject() : nullptr;
		FWBProductionMatchReplayActionRecord Record;
		if (!Object.IsValid() || !ParseRecord(Object, Record))
		{
			Result.FailureCode = TEXT("replay_schema_invalid");
			Result.FailureRecordIndex = Index;
			return Result;
		}
		Result.Archive.Records.Add(MoveTemp(Record));
	}
	if (Result.Archive.Footer.RecordCount != Result.Archive.Records.Num())
	{
		Result.FailureCode = TEXT("replay_truncated");
		return Result;
	}
	const FString StoredHeaderHash = Result.Archive.Header.HeaderHash;
	const FString ExpectedHeaderHash = HashUtf8(CanonicalHeader(Result.Archive.Header));
	if (StoredHeaderHash != ExpectedHeaderHash)
	{
		Result.FailureCode = TEXT("replay_hash_chain_invalid");
		return Result;
	}
	FString Previous = StoredHeaderHash;
	for (int32 Index = 0; Index < Result.Archive.Records.Num(); ++Index)
	{
		const FWBProductionMatchReplayActionRecord& Record = Result.Archive.Records[Index];
		if (Record.RecordIndex != Index
			|| Record.PreviousRecordHash != Previous
			|| Record.RecordHash != HashUtf8(CanonicalRecord(Record)))
		{
			Result.FailureCode = TEXT("replay_hash_chain_invalid");
			Result.FailureRecordIndex = Index;
			return Result;
		}
		Previous = Record.RecordHash;
	}
	if (Result.Archive.Footer.FinalRecordHash != Previous)
	{
		Result.FailureCode = TEXT("replay_footer_mismatch");
		return Result;
	}
	const FString StoredReplayDigest = Result.Archive.Footer.ReplayDigest;
	FWBProductionMatchReplayArchive DigestArchive = Result.Archive;
	DigestArchive.Footer.ReplayDigest.Reset();
	if (StoredReplayDigest != HashUtf8(Serialize(DigestArchive)))
	{
		Result.FailureCode = TEXT("replay_digest_mismatch");
		return Result;
	}
	Result.bValid = true;
	return Result;
}

FWBProductionMatchReplayReceipt WBProductionMatchReplay::BuildReceipt(
	const FWBProductionMatchReplayArchive& Archive,
	const bool bAvailable,
	const FString& FailureCode)
{
	FWBProductionMatchReplayReceipt Receipt;
	Receipt.bAvailable = bAvailable;
	Receipt.SchemaVersion = Archive.Header.SchemaVersion;
	Receipt.OpaqueMatchId = Archive.Header.OpaqueMatchId;
	Receipt.RecordCount = Archive.Footer.RecordCount;
	Receipt.bComplete = Archive.Footer.bComplete;
	Receipt.bTerminal = Archive.Footer.bTerminal;
	Receipt.FinalReplayDigest = Archive.Footer.ReplayDigest;
	Receipt.FailureCode = FailureCode;
	return Receipt;
}

FString WBProductionMatchReplay::SerializeReceipt(
	const FWBProductionMatchReplayReceipt& Receipt)
{
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("available"), Receipt.bAvailable);
	Writer->WriteValue(TEXT("schema_version"), Receipt.SchemaVersion);
	Writer->WriteValue(TEXT("opaque_match_id"), Receipt.OpaqueMatchId);
	Writer->WriteValue(TEXT("entry_count"), Receipt.RecordCount);
	Writer->WriteValue(TEXT("complete"), Receipt.bComplete);
	Writer->WriteValue(TEXT("terminal"), Receipt.bTerminal);
	Writer->WriteValue(TEXT("final_replay_digest"), Receipt.FinalReplayDigest);
	Writer->WriteValue(TEXT("failure_code"), Receipt.FailureCode);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Json;
}
