#include "WBMarkerResolution.h"

#include "WBCardZoneState.h"
#include "WBDamageResolution.h"
#include "WBDeathResolution.h"
#include "WBRules.h"

namespace
{
constexpr int32 CanonicalMarkerCount = 8;
constexpr int32 MarkersPerPlayer = 4;
constexpr int32 MarkersPerTypePerPlayer = 2;
constexpr int32 TrapDamageAmount = 2;

FWBMarkerResolutionResult MakeMarkerFailure(const FString& Reason)
{
	FWBMarkerResolutionResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBNPCPhaseResult MakeNPCPhaseFailure(const FString& Reason)
{
	FWBNPCPhaseResult Result;
	Result.Reason = Reason;
	return Result;
}

bool PlacementLess(const FWBSetupMarkerPlacement& A, const FWBSetupMarkerPlacement& B)
{
	if (A.PlacementOrder != B.PlacementOrder)
	{
		return A.PlacementOrder < B.PlacementOrder;
	}
	if (A.PlayerId != B.PlayerId)
	{
		return A.PlayerId < B.PlayerId;
	}
	return FWBGameStateData::TileToIndex(A.Tile) < FWBGameStateData::TileToIndex(B.Tile);
}

bool PendingSpawnLess(const FWBPendingNPCSpawnState& A, const FWBPendingNPCSpawnState& B)
{
	if (A.SpawnOrder != B.SpawnOrder)
	{
		return A.SpawnOrder < B.SpawnOrder;
	}
	if (A.PendingSpawnId != B.PendingSpawnId)
	{
		return A.PendingSpawnId < B.PendingSpawnId;
	}
	if (A.MarkerOwnerPlayerId != B.MarkerOwnerPlayerId)
	{
		return A.MarkerOwnerPlayerId < B.MarkerOwnerPlayerId;
	}
	return FWBGameStateData::TileToIndex(A.OriginTile) < FWBGameStateData::TileToIndex(B.OriginTile);
}

bool IsTileInPlayerSetupHalf(const int32 PlayerId, const FWBTile& Tile)
{
	return PlayerId == 0 ? Tile.Y >= 4 : Tile.Y <= 4;
}

bool IsHeroSpawnTile(const FWBTile& Tile)
{
	return Tile == FWBTile(4, 8) || Tile == FWBTile(4, 0);
}

bool DefinitionMatchesMarkerType(
	const FWBCardDefinitionRepository& Repository,
	const FString& DefinitionId,
	const EWBMarkerType Type,
	FString& OutReason)
{
	if (DefinitionId.IsEmpty())
	{
		OutReason = TEXT("marker_definition_missing");
		return false;
	}

	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, DefinitionId);
	if (!Lookup.bFound)
	{
		OutReason = Type == EWBMarkerType::NPC
			? FString(TEXT("npc_definition_not_found"))
			: FString(TEXT("marker_definition_not_found"));
		return false;
	}

	if ((Type == EWBMarkerType::Trap && Lookup.Definition.Kind != EWBCardDefinitionKind::Trap)
		|| (Type == EWBMarkerType::NPC && Lookup.Definition.Kind != EWBCardDefinitionKind::NPC))
	{
		OutReason = TEXT("marker_definition_kind_mismatch");
		return false;
	}

	OutReason.Reset();
	return true;
}

FWBTraceEvent MakeMarkerTrace(const FName Kind, const FWBMarkerPlaceholderEntry& Marker)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = Marker.OwnerPlayerId;
	Event.MarkerId = Marker.MarkerId;
	Event.MarkerOwnerId = Marker.OwnerPlayerId;
	Event.MarkerType = WBMarkerResolution::MarkerTypeToName(Marker.Type);
	Event.PlacementOrder = Marker.PlacementOrder;
	Event.ToTile = Marker.Tile;
	Event.bOk = true;
	return Event;
}

int32 AllocateNextPendingSpawnId(const FWBGameStateData& State)
{
	int32 MaxId = -1;
	for (const FWBPendingNPCSpawnState& Pending : State.PendingNPCSpawns)
	{
		if (Pending.PendingSpawnId == MAX_int32)
		{
			return INDEX_NONE;
		}
		MaxId = FMath::Max(MaxId, Pending.PendingSpawnId);
	}
	return MaxId + 1;
}

int32 AllocateNextSpawnOrder(const FWBGameStateData& State)
{
	int32 MaxOrder = -1;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.NPCSpawnOrder == MAX_int32)
		{
			return INDEX_NONE;
		}
		MaxOrder = FMath::Max(MaxOrder, Unit.NPCSpawnOrder);
	}
	for (const FWBPendingNPCSpawnState& Pending : State.PendingNPCSpawns)
	{
		if (Pending.SpawnOrder == MAX_int32)
		{
			return INDEX_NONE;
		}
		MaxOrder = FMath::Max(MaxOrder, Pending.SpawnOrder);
	}
	return MaxOrder + 1;
}

int32 AllocateNextUnitId(const FWBGameStateData& State)
{
	int32 MaxId = -1;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId == MAX_int32)
		{
			return INDEX_NONE;
		}
		MaxId = FMath::Max(MaxId, Unit.UnitId);
	}
	return MaxId + 1;
}

FWBTraceEvent MakePendingTrace(
	const FName Kind,
	const FWBPendingNPCSpawnState& Pending,
	const FWBTile& Destination = FWBTile())
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = Pending.MarkerOwnerPlayerId;
	Event.SourceUnitId = Pending.TriggeredByUnitId;
	Event.MarkerId = Pending.SourceMarkerId;
	Event.MarkerOwnerId = Pending.MarkerOwnerPlayerId;
	Event.MarkerType = FName(TEXT("NPC"));
	Event.PendingSpawnId = Pending.PendingSpawnId;
	Event.SpawnOrder = Pending.SpawnOrder;
	Event.RetryCount = Pending.RetryCount;
	Event.FromTile = Pending.OriginTile;
	Event.ToTile = Destination;
	Event.bOk = true;
	return Event;
}
}

FName WBMarkerResolution::MarkerTypeToName(const EWBMarkerType Type)
{
	switch (Type)
	{
	case EWBMarkerType::Trap:
		return FName(TEXT("Trap"));
	case EWBMarkerType::NPC:
		return FName(TEXT("NPC"));
	default:
		return FName(TEXT("Unknown"));
	}
}

bool WBMarkerResolution::ValidateAuthoritativeState(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FString& OutReason)
{
	if (!WBCardZoneState::ValidateZoneStateForTest(State.GetCardZoneState(), OutReason))
	{
		return false;
	}

	TSet<int32> PlacementOrders;
	for (const FWBMarkerPlaceholderEntry& Marker : State.GetCardZoneState().MarkerPlaceholders)
	{
		if (Marker.MarkerId < 0)
		{
			OutReason = TEXT("marker_id_invalid");
			return false;
		}
		if (Marker.Type != EWBMarkerType::Trap && Marker.Type != EWBMarkerType::NPC)
		{
			OutReason = TEXT("marker_type_invalid");
			return false;
		}
		if (Marker.PlacementOrder < 0 || PlacementOrders.Contains(Marker.PlacementOrder))
		{
			OutReason = TEXT("marker_placement_order_invalid");
			return false;
		}
		PlacementOrders.Add(Marker.PlacementOrder);
		if (!DefinitionMatchesMarkerType(Repository, Marker.InternalMarkerCardId, Marker.Type, OutReason))
		{
			return false;
		}
	}

	TSet<int32> PendingIds;
	TSet<int32> SpawnOrders;
	for (const FWBPendingNPCSpawnState& Pending : State.PendingNPCSpawns)
	{
		if (Pending.PendingSpawnId < 0 || PendingIds.Contains(Pending.PendingSpawnId))
		{
			OutReason = TEXT("pending_spawn_id_invalid");
			return false;
		}
		PendingIds.Add(Pending.PendingSpawnId);
		if (Pending.SourceMarkerId < 0)
		{
			OutReason = TEXT("pending_source_marker_invalid");
			return false;
		}
		if (!FWBGameStateData::IsValidPlayerId(Pending.MarkerOwnerPlayerId))
		{
			OutReason = TEXT("pending_marker_owner_invalid");
			return false;
		}
		if (!WBRules::IsTileInBounds(Pending.OriginTile))
		{
			OutReason = TEXT("pending_origin_invalid");
			return false;
		}
		if (Pending.SpawnOrder < 0 || SpawnOrders.Contains(Pending.SpawnOrder))
		{
			OutReason = TEXT("pending_spawn_order_invalid");
			return false;
		}
		SpawnOrders.Add(Pending.SpawnOrder);
		if (Pending.CreatedTurnNumber < 1 || Pending.RetryCount < 0)
		{
			OutReason = TEXT("pending_spawn_metadata_invalid");
			return false;
		}
		if (!DefinitionMatchesMarkerType(
			Repository,
			Pending.NPCDefinitionId,
			EWBMarkerType::NPC,
			OutReason))
		{
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

FWBMarkerResolutionResult WBMarkerResolution::ApplyCanonicalSetup(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const TArray<FWBSetupMarkerPlacement>& Placements)
{
	if (!State.GetCardZoneState().MarkerPlaceholders.IsEmpty() || !State.PendingNPCSpawns.IsEmpty())
	{
		return MakeMarkerFailure(TEXT("marker_state_not_empty"));
	}
	if (Placements.Num() != CanonicalMarkerCount)
	{
		return MakeMarkerFailure(TEXT("expected_eight_markers"));
	}

	TArray<FWBSetupMarkerPlacement> SortedPlacements = Placements;
	SortedPlacements.Sort(PlacementLess);
	TSet<int32> SeenOrders;
	TSet<int32> SeenTiles;
	int32 Counts[2][2] = {};
	for (int32 Index = 0; Index < SortedPlacements.Num(); ++Index)
	{
		const FWBSetupMarkerPlacement& Placement = SortedPlacements[Index];
		if (!FWBGameStateData::IsValidPlayerId(Placement.PlayerId))
		{
			return MakeMarkerFailure(TEXT("marker_owner_invalid"));
		}
		if (Placement.Type != EWBMarkerType::Trap && Placement.Type != EWBMarkerType::NPC)
		{
			return MakeMarkerFailure(TEXT("marker_type_invalid"));
		}
		if (!WBRules::IsTileInBounds(Placement.Tile))
		{
			return MakeMarkerFailure(TEXT("marker_tile_out_of_bounds"));
		}
		if (!IsTileInPlayerSetupHalf(Placement.PlayerId, Placement.Tile))
		{
			return MakeMarkerFailure(TEXT("marker_outside_player_half"));
		}
		if (IsHeroSpawnTile(Placement.Tile) || State.IsTileOccupied(Placement.Tile))
		{
			return MakeMarkerFailure(TEXT("marker_setup_tile_occupied"));
		}

		const int32 TileIndex = FWBGameStateData::TileToIndex(Placement.Tile);
		if (SeenTiles.Contains(TileIndex))
		{
			return MakeMarkerFailure(TEXT("duplicate_marker_tile"));
		}
		SeenTiles.Add(TileIndex);

		if (Placement.PlacementOrder != Index || SeenOrders.Contains(Placement.PlacementOrder))
		{
			return MakeMarkerFailure(TEXT("marker_placement_order_invalid"));
		}
		SeenOrders.Add(Placement.PlacementOrder);

		FString DefinitionReason;
		if (!DefinitionMatchesMarkerType(Repository, Placement.DefinitionId, Placement.Type, DefinitionReason))
		{
			return MakeMarkerFailure(DefinitionReason);
		}
		++Counts[Placement.PlayerId][Placement.Type == EWBMarkerType::Trap ? 0 : 1];
	}

	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		if (Counts[PlayerId][0] + Counts[PlayerId][1] != MarkersPerPlayer)
		{
			return MakeMarkerFailure(TEXT("expected_four_markers_per_player"));
		}
		if (Counts[PlayerId][0] != MarkersPerTypePerPlayer
			|| Counts[PlayerId][1] != MarkersPerTypePerPlayer)
		{
			return MakeMarkerFailure(TEXT("invalid_marker_type_distribution"));
		}
	}

	FWBGameStateData WorkingState = State;
	TArray<FWBTraceEvent> TraceEvents;
	for (int32 Index = 0; Index < SortedPlacements.Num(); ++Index)
	{
		const FWBSetupMarkerPlacement& Placement = SortedPlacements[Index];
		FWBMarkerPlaceholderEntry Marker;
		Marker.MarkerId = Index;
		Marker.OwnerPlayerId = Placement.PlayerId;
		Marker.Type = Placement.Type;
		Marker.Tile = Placement.Tile;
		Marker.PlacementOrder = Placement.PlacementOrder;
		Marker.PublicState = EWBMarkerPublicState::Hidden;
		Marker.InternalMarkerCardId = Placement.DefinitionId;
		WorkingState.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
		FWBTraceEvent Placed = MakeMarkerTrace(FName(TEXT("marker_placed")), Marker);
		Placed.MarkerType = NAME_None;
		TraceEvents.Add(Placed);
	}
	WBCardZoneState::SortOrderedZonesDeterministically(WorkingState.GetMutableCardZoneStateForTest());

	FString ValidationReason;
	if (!ValidateAuthoritativeState(WorkingState, Repository, ValidationReason))
	{
		return MakeMarkerFailure(ValidationReason);
	}

	FWBTraceEvent Completed;
	Completed.Kind = FName(TEXT("marker_setup_completed"));
	Completed.PlayerId = WorkingState.CurrentPlayer;
	Completed.CardCount = CanonicalMarkerCount;
	Completed.bOk = true;
	TraceEvents.Add(Completed);
	State = MoveTemp(WorkingState);

	FWBMarkerResolutionResult Result;
	Result.bOk = true;
	Result.TraceEvents = MoveTemp(TraceEvents);
	return Result;
}

FWBMarkerResolutionResult WBMarkerResolution::ResolveMarkerAtUnitTile(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 EnteringUnitId)
{
	FString ValidationReason;
	if (!ValidateAuthoritativeState(State, Repository, ValidationReason))
	{
		return MakeMarkerFailure(ValidationReason);
	}

	const FWBUnitState* EnteringUnit = State.GetUnitById(EnteringUnitId);
	if (EnteringUnit == nullptr || !EnteringUnit->IsUnitOnBoard())
	{
		return MakeMarkerFailure(TEXT("entering_unit_not_found"));
	}

	const FWBTile EnteredTile(EnteringUnit->X, EnteringUnit->Y);
	TArray<FWBMarkerPlaceholderEntry> Matches;
	for (const FWBMarkerPlaceholderEntry& Marker : State.GetCardZoneState().MarkerPlaceholders)
	{
		if (Marker.Tile == EnteredTile)
		{
			Matches.Add(Marker);
		}
	}
	if (Matches.IsEmpty())
	{
		FWBMarkerResolutionResult Result;
		Result.bOk = true;
		Result.EnteringUnitId = EnteringUnitId;
		return Result;
	}
	if (Matches.Num() != 1)
	{
		return MakeMarkerFailure(TEXT("multiple_markers_on_tile"));
	}

	const FWBMarkerPlaceholderEntry Marker = Matches[0];
	FWBGameStateData WorkingState = State;
	TArray<FWBTraceEvent> TraceEvents;
	FWBTraceEvent Triggered = MakeMarkerTrace(FName(TEXT("marker_triggered")), Marker);
	Triggered.SourceUnitId = EnteringUnitId;
	TraceEvents.Add(Triggered);
	FWBTraceEvent Revealed = MakeMarkerTrace(FName(TEXT("marker_revealed")), Marker);
	Revealed.SourceUnitId = EnteringUnitId;
	TraceEvents.Add(Revealed);

	bool bTrapDamageApplied = false;
	bool bNPCSpawnScheduled = false;
	bool bUnitDefeated = false;
	int32 PendingSpawnId = -1;
	if (Marker.Type == EWBMarkerType::Trap)
	{
		FWBDamageRequest DamageRequest;
		DamageRequest.DamageKind = EWBDamageKind::Effect;
		DamageRequest.SourcePlayerId = Marker.OwnerPlayerId;
		DamageRequest.TargetUnitId = EnteringUnitId;
		DamageRequest.BaseDamage = TrapDamageAmount;
		DamageRequest.bBypassArmor = false;
		DamageRequest.DamageCause = FName(TEXT("Trap"));
		const FWBDamageResolutionResult DamageResult =
			WBDamageResolution::ResolveDamageRequest(WorkingState, DamageRequest);
		if (!DamageResult.bOk)
		{
			return MakeMarkerFailure(DamageResult.Reason);
		}

		FWBTraceEvent Damage = MakeMarkerTrace(FName(TEXT("trap_damage_resolved")), Marker);
		Damage.SourceUnitId = EnteringUnitId;
		Damage.TargetUnitId = EnteringUnitId;
		Damage.DamageAmount = TrapDamageAmount;
		Damage.FinalDamageAmount = DamageResult.Prevention.FinalDamage;
		Damage.PreviousHP = DamageResult.PreviousHP;
		Damage.NewHP = DamageResult.NewHP;
		Damage.PreviousArmor = DamageResult.PreviousArmor;
		Damage.NewArmor = DamageResult.NewArmor;
		Damage.ArmorAbsorbedAmount = DamageResult.ArmorAbsorbedAmount;
		Damage.HPDamageAmount = DamageResult.HPDamageAmount;
		Damage.DamageCause = DamageRequest.DamageCause;
		Damage.bAtOrBelowZeroHP = DamageResult.bAtOrBelowZeroHP;
		TraceEvents.Add(Damage);
		bTrapDamageApplied = true;
	}
	else
	{
		PendingSpawnId = AllocateNextPendingSpawnId(WorkingState);
		const int32 SpawnOrder = AllocateNextSpawnOrder(WorkingState);
		if (PendingSpawnId < 0 || SpawnOrder < 0)
		{
			return MakeMarkerFailure(TEXT("pending_spawn_id_allocation_failed"));
		}

		FWBPendingNPCSpawnState Pending;
		Pending.PendingSpawnId = PendingSpawnId;
		Pending.SourceMarkerId = Marker.MarkerId;
		Pending.MarkerOwnerPlayerId = Marker.OwnerPlayerId;
		Pending.NPCDefinitionId = Marker.InternalMarkerCardId;
		Pending.OriginTile = Marker.Tile;
		Pending.SpawnOrder = SpawnOrder;
		Pending.TriggeredByUnitId = EnteringUnitId;
		Pending.TriggeredByOwnerId = EnteringUnit->OwnerId;
		Pending.CreatedTurnNumber = WorkingState.TurnNumber;
		WorkingState.PendingNPCSpawns.Add(Pending);
		TraceEvents.Add(MakePendingTrace(FName(TEXT("npc_spawn_scheduled")), Pending));
		bNPCSpawnScheduled = true;
	}

	const int32 RemovedCount = WorkingState.GetMutableCardZoneStateForTest().MarkerPlaceholders.RemoveAll(
		[&Marker](const FWBMarkerPlaceholderEntry& Candidate)
		{
			return Candidate.MarkerId == Marker.MarkerId;
		});
	if (RemovedCount != 1)
	{
		return MakeMarkerFailure(TEXT("marker_consumption_failed"));
	}
	TraceEvents.Add(MakeMarkerTrace(FName(TEXT("marker_consumed")), Marker));

	if (Marker.Type == EWBMarkerType::Trap)
	{
		const FWBActionQueryResult DeathQuery = WBRules::CanApplyZeroHPDeathRemoval(WorkingState);
		if (DeathQuery.bOk)
		{
			const FWBApplyActionResult DeathResult = WBDeathResolution::ApplyZeroHPDeathResolution(WorkingState);
			if (!DeathResult.bOk)
			{
				return MakeMarkerFailure(DeathResult.Reason);
			}
			TraceEvents.Append(DeathResult.TraceEvents);
			bUnitDefeated = true;
			FWBTraceEvent Defeated = MakeMarkerTrace(FName(TEXT("marker_triggered_death")), Marker);
			Defeated.TargetUnitId = EnteringUnitId;
			Defeated.bAtOrBelowZeroHP = true;
			TraceEvents.Add(Defeated);
			if (WorkingState.bGameOver)
			{
				FWBTraceEvent GameOver = MakeMarkerTrace(FName(TEXT("marker_triggered_game_over")), Marker);
				GameOver.WinningPlayerId = WorkingState.WinnerPlayerId;
				TraceEvents.Add(GameOver);
			}
		}
	}

	if (!ValidateAuthoritativeState(WorkingState, Repository, ValidationReason))
	{
		return MakeMarkerFailure(ValidationReason);
	}
	State = MoveTemp(WorkingState);

	FWBMarkerResolutionResult Result;
	Result.bOk = true;
	Result.bMarkerFound = true;
	Result.bMarkerConsumed = true;
	Result.bTrapDamageApplied = bTrapDamageApplied;
	Result.bNPCSpawnScheduled = bNPCSpawnScheduled;
	Result.bUnitDefeated = bUnitDefeated;
	Result.MarkerId = Marker.MarkerId;
	Result.EnteringUnitId = EnteringUnitId;
	Result.PendingSpawnId = PendingSpawnId;
	Result.TraceEvents = MoveTemp(TraceEvents);
	return Result;
}

FWBNPCPhaseResult WBMarkerResolution::ProcessPendingNPCSpawns(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 PhaseOwnerPlayerId)
{
	FString ValidationReason;
	if (!ValidateAuthoritativeState(State, Repository, ValidationReason))
	{
		return MakeNPCPhaseFailure(ValidationReason);
	}

	FWBGameStateData WorkingState = State;
	TArray<FWBTraceEvent> TraceEvents;
	(void)PhaseOwnerPlayerId;

	TArray<FWBPendingNPCSpawnState> OrderedPending = WorkingState.PendingNPCSpawns;
	OrderedPending.Sort(PendingSpawnLess);
	int32 SpawnedCount = 0;
	int32 BlockedCount = 0;
	for (const FWBPendingNPCSpawnState& PendingSnapshot : OrderedPending)
	{
		const int32 PendingIndex = WorkingState.PendingNPCSpawns.IndexOfByPredicate(
			[&PendingSnapshot](const FWBPendingNPCSpawnState& Candidate)
			{
				return Candidate.PendingSpawnId == PendingSnapshot.PendingSpawnId;
			});
		if (PendingIndex == INDEX_NONE)
		{
			return MakeNPCPhaseFailure(TEXT("pending_spawn_missing"));
		}

		TraceEvents.Add(MakePendingTrace(FName(TEXT("npc_spawn_attempted")), PendingSnapshot));
		// Godot's canonical adjacent order is East, West, South, North.
		const TArray<FWBTile> Candidates = {
			FWBTile(PendingSnapshot.OriginTile.X + 1, PendingSnapshot.OriginTile.Y),
			FWBTile(PendingSnapshot.OriginTile.X - 1, PendingSnapshot.OriginTile.Y),
			FWBTile(PendingSnapshot.OriginTile.X, PendingSnapshot.OriginTile.Y + 1),
			FWBTile(PendingSnapshot.OriginTile.X, PendingSnapshot.OriginTile.Y - 1)
		};

		FWBTile SpawnTile;
		bool bFoundSpawnTile = false;
		for (const FWBTile& Candidate : Candidates)
		{
			if (WBRules::IsTileInBounds(Candidate) && !WorkingState.IsTileOccupied(Candidate))
			{
				SpawnTile = Candidate;
				bFoundSpawnTile = true;
				break;
			}
		}

		if (!bFoundSpawnTile)
		{
			++WorkingState.PendingNPCSpawns[PendingIndex].RetryCount;
			FWBPendingNPCSpawnState Blocked = WorkingState.PendingNPCSpawns[PendingIndex];
			TraceEvents.Add(MakePendingTrace(FName(TEXT("npc_spawn_blocked")), Blocked));
			++BlockedCount;
			continue;
		}

		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, PendingSnapshot.NPCDefinitionId);
		if (!Lookup.bFound || Lookup.Definition.Kind != EWBCardDefinitionKind::NPC)
		{
			return MakeNPCPhaseFailure(TEXT("npc_definition_not_found"));
		}

		const int32 NewUnitId = AllocateNextUnitId(WorkingState);
		if (NewUnitId < 0)
		{
			return MakeNPCPhaseFailure(TEXT("npc_unit_id_allocation_failed"));
		}

		FWBUnitState NPC;
		NPC.UnitId = NewUnitId;
		NPC.OwnerId = -1;
		NPC.CardId = PendingSnapshot.NPCDefinitionId;
		NPC.X = SpawnTile.X;
		NPC.Y = SpawnTile.Y;
		NPC.HP = Lookup.Definition.CharacterStats.HP;
		NPC.MaxHP = NPC.HP;
		NPC.ATK = Lookup.Definition.CharacterStats.ATK;
		NPC.AR = Lookup.Definition.CharacterStats.AR;
		NPC.SetCanonicalRL(
			Lookup.Definition.CharacterStats.RL,
			Lookup.Definition.CharacterStats.RL,
			0);
		NPC.SetArmorForTest(0, 0);
		NPC.AttacksLeft = 0;
		NPC.MaxAttacksPerTurn = 1;
		NPC.NPCSpawnOrder = PendingSnapshot.SpawnOrder;
		NPC.NPCCreationTurnNumber = WorkingState.TurnNumber;
		NPC.NPCTriggeredByUnitId = PendingSnapshot.TriggeredByUnitId;
		WorkingState.Units.Add(NPC);
		WorkingState.PendingNPCSpawns.RemoveAt(PendingIndex, 1, EAllowShrinking::No);

		FWBTraceEvent Spawned = MakePendingTrace(FName(TEXT("npc_spawn_succeeded")), PendingSnapshot, SpawnTile);
		Spawned.TargetUnitId = NewUnitId;
		Spawned.CardId = PendingSnapshot.NPCDefinitionId;
		TraceEvents.Add(Spawned);
		++SpawnedCount;
	}

	if (!ValidateAuthoritativeState(WorkingState, Repository, ValidationReason))
	{
		return MakeNPCPhaseFailure(ValidationReason);
	}
	State = MoveTemp(WorkingState);

	FWBNPCPhaseResult Result;
	Result.bOk = true;
	Result.SpawnedCount = SpawnedCount;
	Result.BlockedCount = BlockedCount;
	Result.TraceEvents = MoveTemp(TraceEvents);
	return Result;
}
