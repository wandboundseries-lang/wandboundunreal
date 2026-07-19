#include "WBNPCPhaseResolution.h"

#include "WBEffectRunner.h"
#include "WBMarkerResolution.h"
#include "WBRules.h"

namespace
{
const FWBTile PathDirections[] = {
	FWBTile(1, 0),
	FWBTile(-1, 0),
	FWBTile(0, 1),
	FWBTile(0, -1)
};

struct FPathNode
{
	FWBTile Tile;
	int32 ParentIndex = INDEX_NONE;
};

struct FTargetRank
{
	int32 UnitId = INDEX_NONE;
	bool bReachable = false;
	int32 PathDistance = MAX_int32;
	int32 DirectDistance = MAX_int32;
	int32 HP = MAX_int32;
	int32 TriggerPriority = 1;
	int32 OwnerId = MAX_int32;
};

FWBNPCPhaseResolutionResult Failure(const FString& Reason)
{
	FWBNPCPhaseResolutionResult Result;
	Result.Reason = Reason;
	return Result;
}

bool NPCOrderLess(const FWBUnitState& A, const FWBUnitState& B)
{
	if (A.NPCSpawnOrder != B.NPCSpawnOrder)
	{
		return A.NPCSpawnOrder < B.NPCSpawnOrder;
	}
	if (A.UnitId != B.UnitId)
	{
		return A.UnitId < B.UnitId;
	}
	return A.CardId < B.CardId;
}

bool TargetRankLess(const FTargetRank& A, const FTargetRank& B)
{
	if (A.bReachable != B.bReachable)
	{
		return A.bReachable;
	}
	if (A.PathDistance != B.PathDistance)
	{
		return A.PathDistance < B.PathDistance;
	}
	if (A.DirectDistance != B.DirectDistance)
	{
		return A.DirectDistance < B.DirectDistance;
	}
	if (A.HP != B.HP)
	{
		return A.HP < B.HP;
	}
	if (A.TriggerPriority != B.TriggerPriority)
	{
		return A.TriggerPriority < B.TriggerPriority;
	}
	if (A.OwnerId != B.OwnerId)
	{
		return A.OwnerId < B.OwnerId;
	}
	return A.UnitId < B.UnitId;
}

FWBAction MakeNPCMoveAction(const FWBUnitState& NPC, const FWBTile& ToTile)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = -1;
	Action.SourceUnitId = NPC.UnitId;
	Action.FromTile = FWBTile(NPC.X, NPC.Y);
	Action.ToTile = ToTile;
	return Action;
}

FWBAction MakeNPCAttackAction(const FWBUnitState& NPC, const FWBUnitState& Target)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = -1;
	Action.SourceUnitId = NPC.UnitId;
	Action.TargetUnitId = Target.UnitId;
	Action.FromTile = FWBTile(NPC.X, NPC.Y);
	Action.ToTile = FWBTile(Target.X, Target.Y);
	return Action;
}

bool FindPathToTarget(
	const FWBGameStateData& State,
	const int32 NPCUnitId,
	const int32 TargetUnitId,
	TArray<FWBTile>& OutPath)
{
	OutPath.Reset();
	const FWBUnitState* NPC = State.GetUnitById(NPCUnitId);
	const FWBUnitState* Target = State.GetUnitById(TargetUnitId);
	if (NPC == nullptr || Target == nullptr)
	{
		return false;
	}

	const FWBTile Start(NPC->X, NPC->Y);
	const FWBTile Goal(Target->X, Target->Y);
	TArray<FPathNode> Nodes;
	TArray<int32> Frontier;
	TSet<int32> Visited;
	Nodes.Add({ Start, INDEX_NONE });
	Frontier.Add(0);
	Visited.Add(FWBGameStateData::TileToIndex(Start));

	int32 ReadIndex = 0;
	int32 GoalNodeIndex = INDEX_NONE;
	while (ReadIndex < Frontier.Num())
	{
		const int32 NodeIndex = Frontier[ReadIndex++];
		const FWBTile Current = Nodes[NodeIndex].Tile;
		for (const FWBTile& Direction : PathDirections)
		{
			const FWBTile Next(Current.X + Direction.X, Current.Y + Direction.Y);
			if (!WBRules::IsTileInBounds(Next))
			{
				continue;
			}
			const int32 TileIndex = FWBGameStateData::TileToIndex(Next);
			if (Visited.Contains(TileIndex))
			{
				continue;
			}

			bool bLegalStep = false;
			if (Next == Goal)
			{
				bLegalStep = !WBRules::HasWallBetween(State, Current, Next);
			}
			else
			{
				FWBGameStateData QueryState = State;
				FWBUnitState* QueryNPC = QueryState.GetMutableUnitById(NPCUnitId);
				if (QueryNPC != nullptr)
				{
					QueryNPC->X = Current.X;
					QueryNPC->Y = Current.Y;
					bLegalStep = WBRules::QueryNPCMove(
						QueryState,
						MakeNPCMoveAction(*QueryNPC, Next),
						1).bOk;
				}
			}
			if (!bLegalStep)
			{
				continue;
			}

			Visited.Add(TileIndex);
			const int32 NewNodeIndex = Nodes.Add({ Next, NodeIndex });
			if (Next == Goal)
			{
				GoalNodeIndex = NewNodeIndex;
				break;
			}
			Frontier.Add(NewNodeIndex);
		}
		if (GoalNodeIndex != INDEX_NONE)
		{
			break;
		}
	}

	if (GoalNodeIndex == INDEX_NONE)
	{
		return false;
	}
	for (int32 NodeIndex = GoalNodeIndex; Nodes[NodeIndex].ParentIndex != INDEX_NONE; NodeIndex = Nodes[NodeIndex].ParentIndex)
	{
		OutPath.Insert(Nodes[NodeIndex].Tile, 0);
	}
	return true;
}

int32 SelectTarget(
	const FWBGameStateData& State,
	const FWBUnitState& NPC,
	const bool bRequireAttackable)
{
	TArray<FTargetRank> Ranks;
	for (const FWBUnitState& Candidate : State.Units)
	{
		if (!FWBGameStateData::IsValidPlayerId(Candidate.OwnerId)
			|| Candidate.bDefeated
			|| !Candidate.IsUnitOnBoard())
		{
			continue;
		}
		if (bRequireAttackable
			&& !WBRules::CanDeclareNPCAttack(State, MakeNPCAttackAction(NPC, Candidate)).bOk)
		{
			continue;
		}

		TArray<FWBTile> Path;
		const bool bReachable = FindPathToTarget(State, NPC.UnitId, Candidate.UnitId, Path);
		FTargetRank Rank;
		Rank.UnitId = Candidate.UnitId;
		Rank.bReachable = bReachable;
		Rank.PathDistance = bReachable ? Path.Num() : MAX_int32;
		Rank.DirectDistance = WBRules::OrthogonalDistance(
			FWBTile(NPC.X, NPC.Y),
			FWBTile(Candidate.X, Candidate.Y));
		Rank.HP = Candidate.HP;
		Rank.TriggerPriority = Candidate.UnitId == NPC.NPCTriggeredByUnitId ? 0 : 1;
		Rank.OwnerId = Candidate.OwnerId;
		Ranks.Add(Rank);
	}
	if (Ranks.IsEmpty())
	{
		return INDEX_NONE;
	}
	Ranks.Sort(TargetRankLess);
	return Ranks[0].UnitId;
}

bool ValidateNPCState(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FString& OutReason)
{
	if (!WBMarkerResolution::ValidateAuthoritativeState(State, Repository, OutReason))
	{
		return false;
	}
	TSet<int32> UnitIds;
	TSet<int32> SpawnOrders;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId < 0 || UnitIds.Contains(Unit.UnitId))
		{
			OutReason = TEXT("npc_phase_unit_id_invalid");
			return false;
		}
		UnitIds.Add(Unit.UnitId);
		const bool bHasNPCMetadata = Unit.NPCSpawnOrder != INDEX_NONE
			|| Unit.NPCCreationTurnNumber != INDEX_NONE;
		if (Unit.OwnerId != -1 && !bHasNPCMetadata)
		{
			continue;
		}
		if (Unit.OwnerId != -1)
		{
			OutReason = TEXT("npc_owner_invalid");
			return false;
		}
		if (Unit.NPCSpawnOrder < 0 || SpawnOrders.Contains(Unit.NPCSpawnOrder)
			|| Unit.NPCCreationTurnNumber < 1)
		{
			OutReason = TEXT("npc_spawn_metadata_invalid");
			return false;
		}
		SpawnOrders.Add(Unit.NPCSpawnOrder);
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Unit.CardId);
		if (!Lookup.bFound || Lookup.Definition.Kind != EWBCardDefinitionKind::NPC)
		{
			OutReason = TEXT("npc_definition_not_found");
			return false;
		}
	}
	for (const FWBPendingNPCSpawnState& Pending : State.PendingNPCSpawns)
	{
		if (SpawnOrders.Contains(Pending.SpawnOrder))
		{
			OutReason = TEXT("npc_spawn_metadata_invalid");
			return false;
		}
		SpawnOrders.Add(Pending.SpawnOrder);
	}
	OutReason.Reset();
	return true;
}

void AnnotateEvents(
	TArray<FWBTraceEvent>& Events,
	const FWBUnitState& NPC,
	const int32 ActionSequence,
	const int32 PathStepIndex = INDEX_NONE)
{
	for (FWBTraceEvent& Event : Events)
	{
		Event.SpawnOrder = NPC.NPCSpawnOrder;
		Event.ActionSequence = ActionSequence;
		Event.CardId = NPC.CardId;
		if (PathStepIndex != INDEX_NONE)
		{
			Event.PathStepIndex = PathStepIndex;
		}
	}
}

FWBTraceEvent MakeNPCTrace(
	const FName Kind,
	const FWBUnitState& NPC,
	const int32 ActionSequence)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = -1;
	Event.SourceUnitId = NPC.UnitId;
	Event.SpawnOrder = NPC.NPCSpawnOrder;
	Event.ActionSequence = ActionSequence;
	Event.CardId = NPC.CardId;
	Event.TurnNumber = NPC.NPCCreationTurnNumber;
	Event.MatchPhase = FName(TEXT("NPCPhase"));
	Event.bOk = true;
	return Event;
}
}

FWBNPCPhaseResolutionResult WBNPCPhaseResolution::ResolvePhase(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	uint32& InOutRandomState,
	const int32 PhaseOwnerPlayerId)
{
	if (InOutRandomState == 0)
	{
		return Failure(TEXT("npc_rng_state_invalid"));
	}
	FString ValidationReason;
	if (!ValidateNPCState(State, Repository, ValidationReason))
	{
		return Failure(ValidationReason);
	}

	FWBGameStateData WorkingState = State;
	uint32 WorkingRandomState = InOutRandomState;
	FWBNPCPhaseResolutionResult Result;
	FWBTraceEvent PhaseStarted;
	PhaseStarted.Kind = FName(TEXT("npc_phase_started"));
	PhaseStarted.PlayerId = PhaseOwnerPlayerId;
	PhaseStarted.TurnNumber = WorkingState.TurnNumber;
	PhaseStarted.MatchPhase = FName(TEXT("NPCPhase"));
	PhaseStarted.bOk = true;
	Result.TraceEvents.Add(PhaseStarted);

	const FWBNPCPhaseResult SpawnResult = WBMarkerResolution::ProcessPendingNPCSpawns(
		WorkingState,
		Repository,
		PhaseOwnerPlayerId);
	if (!SpawnResult.bOk)
	{
		return Failure(SpawnResult.Reason);
	}
	Result.SpawnedCount += SpawnResult.SpawnedCount;
	Result.BlockedSpawnCount += SpawnResult.BlockedCount;
	Result.TraceEvents.Append(SpawnResult.TraceEvents);
	if (!ValidateNPCState(WorkingState, Repository, ValidationReason))
	{
		return Failure(ValidationReason);
	}

	TArray<FWBUnitState> OrderedNPCs;
	for (const FWBUnitState& Unit : WorkingState.Units)
	{
		if (Unit.OwnerId == -1 && Unit.IsUnitOnBoard() && !Unit.bDefeated)
		{
			OrderedNPCs.Add(Unit);
		}
	}
	OrderedNPCs.Sort(NPCOrderLess);
	TArray<int32> Queue;
	for (const FWBUnitState& NPC : OrderedNPCs)
	{
		Queue.Add(NPC.UnitId);
	}
	Result.EligibleNPCCount = Queue.Num();

	FWBTraceEvent QueueTrace;
	QueueTrace.Kind = FName(TEXT("npc_action_queue_created"));
	QueueTrace.PlayerId = PhaseOwnerPlayerId;
	QueueTrace.CardCount = Queue.Num();
	QueueTrace.TurnNumber = WorkingState.TurnNumber;
	QueueTrace.MatchPhase = FName(TEXT("NPCPhase"));
	QueueTrace.bOk = true;
	Result.TraceEvents.Add(QueueTrace);

	int32 QueueIndex = 0;
	while (QueueIndex < Queue.Num() && !WorkingState.bGameOver)
	{
		const int32 NPCUnitId = Queue[QueueIndex++];
		FWBUnitState* NPC = WorkingState.GetMutableUnitById(NPCUnitId);
		if (NPC == nullptr || NPC->bDefeated || !NPC->IsUnitOnBoard())
		{
			FWBTraceEvent Skipped;
			Skipped.Kind = FName(TEXT("npc_skipped"));
			Skipped.SourceUnitId = NPCUnitId;
			Skipped.ActionSequence = QueueIndex - 1;
			Skipped.Reason = TEXT("npc_no_longer_available");
			Skipped.MatchPhase = FName(TEXT("NPCPhase"));
			Skipped.bOk = true;
			Result.TraceEvents.Add(Skipped);
			continue;
		}

		const int32 ActionSequence = QueueIndex - 1;
		Result.TraceEvents.Add(MakeNPCTrace(FName(TEXT("npc_action_started")), *NPC, ActionSequence));
		NPC->AttacksLeft = FMath::Max(NPC->MaxAttacksPerTurn, 1);
		NPC->MPRemaining = RollD6(WorkingRandomState);
		Result.MPRolls.Add(NPC->MPRemaining);
		FWBTraceEvent Roll = MakeNPCTrace(FName(TEXT("npc_mp_rolled")), *NPC, ActionSequence);
		Roll.MPRoll = NPC->MPRemaining;
		Roll.RemainingMP = NPC->MPRemaining;
		Result.TraceEvents.Add(Roll);

		int32 PathStepIndex = 0;
		bool bMadeProgress = false;
		while (!WorkingState.bGameOver)
		{
			NPC = WorkingState.GetMutableUnitById(NPCUnitId);
			if (NPC == nullptr || NPC->bDefeated || !NPC->IsUnitOnBoard())
			{
				break;
			}

			if (NPC->AttacksLeft > 0)
			{
				const int32 AttackTargetId = SelectTarget(WorkingState, *NPC, true);
				if (AttackTargetId != INDEX_NONE)
				{
					const FWBUnitState* Target = WorkingState.GetUnitById(AttackTargetId);
					FWBTraceEvent Selected = MakeNPCTrace(FName(TEXT("npc_target_selected")), *NPC, ActionSequence);
					Selected.TargetUnitId = AttackTargetId;
					Selected.Reason = TEXT("attackable_target");
					Result.TraceEvents.Add(Selected);
					const FWBApplyActionResult DeclareResult = WBEffectRunner::ApplyNPCAttackDeclare(
						WorkingState,
						MakeNPCAttackAction(*NPC, *Target));
					if (!DeclareResult.bOk)
					{
						return Failure(DeclareResult.Reason);
					}
					TArray<FWBTraceEvent> DeclareEvents = DeclareResult.TraceEvents;
					AnnotateEvents(DeclareEvents, *NPC, ActionSequence);
					Result.TraceEvents.Append(DeclareEvents);
					const FWBApplyActionResult DamageResult = WBEffectRunner::ApplyPendingAttackDamage(WorkingState);
					if (!DamageResult.bOk)
					{
						return Failure(DamageResult.Reason);
					}
					TArray<FWBTraceEvent> DamageEvents = DamageResult.TraceEvents;
					AnnotateEvents(DamageEvents, *NPC, ActionSequence);
					Result.TraceEvents.Append(DamageEvents);
					FWBTraceEvent Resolved = MakeNPCTrace(FName(TEXT("npc_attack_damage_resolved")), *NPC, ActionSequence);
					Resolved.TargetUnitId = AttackTargetId;
					if (const FWBTraceEvent* CoreDamage = DamageEvents.FindByPredicate([](const FWBTraceEvent& Event)
					{
						return Event.Kind == FName(TEXT("attack_damage_resolved"));
					}))
					{
						Resolved.DamageAmount = CoreDamage->DamageAmount;
						Resolved.FinalDamageAmount = CoreDamage->FinalDamageAmount;
						Resolved.ArmorAbsorbedAmount = CoreDamage->ArmorAbsorbedAmount;
						Resolved.HPDamageAmount = CoreDamage->HPDamageAmount;
						Resolved.PreviousHP = CoreDamage->PreviousHP;
						Resolved.NewHP = CoreDamage->NewHP;
					}
					Result.TraceEvents.Add(Resolved);
					bMadeProgress = true;
					continue;
				}
			}

			if (NPC->MPRemaining <= 0)
			{
				break;
			}
			const int32 ChaseTargetId = SelectTarget(WorkingState, *NPC, false);
			if (ChaseTargetId == INDEX_NONE)
			{
				FWBTraceEvent NoTarget = MakeNPCTrace(FName(TEXT("npc_no_target")), *NPC, ActionSequence);
				NoTarget.Reason = TEXT("no_player_controlled_targets");
				Result.TraceEvents.Add(NoTarget);
				break;
			}
			FWBTraceEvent Selected = MakeNPCTrace(FName(TEXT("npc_target_selected")), *NPC, ActionSequence);
			Selected.TargetUnitId = ChaseTargetId;
			Selected.Reason = TEXT("chase_target");
			Result.TraceEvents.Add(Selected);

			TArray<FWBTile> Path;
			if (!FindPathToTarget(WorkingState, NPCUnitId, ChaseTargetId, Path)
				|| Path.IsEmpty()
				|| WorkingState.UnitIdAt(Path[0]) != -1)
			{
				FWBTraceEvent Skipped = MakeNPCTrace(FName(TEXT("npc_skipped")), *NPC, ActionSequence);
				Skipped.TargetUnitId = ChaseTargetId;
				Skipped.Reason = TEXT("no_legal_path");
				Result.TraceEvents.Add(Skipped);
				break;
			}

			FWBTraceEvent Planned = MakeNPCTrace(FName(TEXT("npc_movement_planned")), *NPC, ActionSequence);
			Planned.TargetUnitId = ChaseTargetId;
			Planned.FromTile = FWBTile(NPC->X, NPC->Y);
			Planned.ToTile = Path[0];
			Planned.PathStepIndex = PathStepIndex;
			Result.TraceEvents.Add(Planned);
			const FWBApplyActionResult MoveResult = WBEffectRunner::ApplyNPCMove(
				WorkingState,
				MakeNPCMoveAction(*NPC, Path[0]));
			if (!MoveResult.bOk)
			{
				return Failure(MoveResult.Reason);
			}
			TArray<FWBTraceEvent> MoveEvents = MoveResult.TraceEvents;
			AnnotateEvents(MoveEvents, *NPC, ActionSequence, PathStepIndex++);
			Result.TraceEvents.Append(MoveEvents);
			bMadeProgress = true;

			const FWBMarkerResolutionResult MarkerResult = WBMarkerResolution::ResolveMarkerAtUnitTile(
				WorkingState,
				Repository,
				NPCUnitId);
			if (!MarkerResult.bOk)
			{
				return Failure(MarkerResult.Reason);
			}
			if (MarkerResult.bMarkerFound)
			{
				FWBTraceEvent MarkerTriggered = MakeNPCTrace(
					FName(TEXT("npc_marker_triggered")),
					*NPC,
					ActionSequence);
				MarkerTriggered.MarkerId = MarkerResult.MarkerId;
				Result.TraceEvents.Add(MarkerTriggered);
			}
			TArray<FWBTraceEvent> MarkerEvents = MarkerResult.TraceEvents;
			AnnotateEvents(MarkerEvents, *NPC, ActionSequence, PathStepIndex - 1);
			Result.TraceEvents.Append(MarkerEvents);

			if (MarkerResult.bNPCSpawnScheduled && !WorkingState.bGameOver)
			{
				const FWBNPCPhaseResult MidPhaseSpawn = WBMarkerResolution::ProcessPendingNPCSpawns(
					WorkingState,
					Repository,
					PhaseOwnerPlayerId);
				if (!MidPhaseSpawn.bOk)
				{
					return Failure(MidPhaseSpawn.Reason);
				}
				Result.SpawnedCount += MidPhaseSpawn.SpawnedCount;
				Result.BlockedSpawnCount += MidPhaseSpawn.BlockedCount;
				Result.TraceEvents.Append(MidPhaseSpawn.TraceEvents);
				TArray<FWBUnitState> NewlySpawned;
				for (const FWBUnitState& Candidate : WorkingState.Units)
				{
					if (Candidate.OwnerId == -1 && Candidate.IsUnitOnBoard()
						&& !Queue.Contains(Candidate.UnitId))
					{
						NewlySpawned.Add(Candidate);
					}
				}
				NewlySpawned.Sort(NPCOrderLess);
				for (const FWBUnitState& Candidate : NewlySpawned)
				{
					Queue.Add(Candidate.UnitId);
					++Result.EligibleNPCCount;
				}
			}
		}

		NPC = WorkingState.GetMutableUnitById(NPCUnitId);
		if (NPC != nullptr)
		{
			FWBTraceEvent Completed = MakeNPCTrace(FName(TEXT("npc_action_completed")), *NPC, ActionSequence);
			Completed.Reason = bMadeProgress ? TEXT("resolved") : TEXT("no_action");
			Completed.RemainingMP = NPC->MPRemaining;
			Result.TraceEvents.Add(Completed);
		}
		++Result.CompletedNPCCount;
	}

	if (!ValidateNPCState(WorkingState, Repository, ValidationReason))
	{
		return Failure(ValidationReason);
	}
	FWBTraceEvent PhaseEnded;
	PhaseEnded.Kind = FName(TEXT("npc_phase_ended"));
	PhaseEnded.PlayerId = PhaseOwnerPlayerId;
	PhaseEnded.TurnNumber = WorkingState.TurnNumber;
	PhaseEnded.MatchPhase = FName(TEXT("NPCPhase"));
	PhaseEnded.WinningPlayerId = WorkingState.WinnerPlayerId;
	PhaseEnded.bOk = true;
	Result.TraceEvents.Add(PhaseEnded);

	State = MoveTemp(WorkingState);
	InOutRandomState = WorkingRandomState;
	Result.bOk = true;
	return Result;
}

int32 WBNPCPhaseResolution::RollD6(uint32& InOutRandomState)
{
	InOutRandomState = InOutRandomState * 1664525u + 1013904223u;
	return static_cast<int32>((InOutRandomState % 6u) + 1u);
}
