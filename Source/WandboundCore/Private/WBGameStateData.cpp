#include "WBGameStateData.h"

#include "WBRules.h"
#include "WBStatusSemantics.h"

namespace
{
bool StatusStateLess(
	const FWBStatusInstanceState& A,
	const FWBStatusInstanceState& B)
{
	return A.StatusId.GetPlainNameString() < B.StatusId.GetPlainNameString();
}

int32 FindCanonicalStatusStateIndex(
	const TArray<FWBStatusInstanceState>& States,
	const FName StatusId)
{
	const FName Canonical = WBStatusSemantics::CanonicalizeStatusId(StatusId);
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		if (States[Index].StatusId == Canonical)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 GetLegacyStatusDuration(
	const FWBUnitState& Unit,
	const FName CanonicalStatusId)

{
	bool bFoundMatchingStatus = false;
	int32 MaximumDuration = 0;
	for (const FName ActiveStatusId : Unit.Statuses)
	{
		if (!WBStatusSemantics::IsCanonicalStatusId(ActiveStatusId)
			|| WBStatusSemantics::CanonicalizeStatusId(ActiveStatusId)
				!= CanonicalStatusId)
		{
			continue;
		}
		bFoundMatchingStatus = true;
		const int32* Duration = Unit.StatusTurnsRemaining.Find(ActiveStatusId);
		if (Duration == nullptr || *Duration <= 0)
		{
			return 0;
		}
		MaximumDuration = FMath::Max(MaximumDuration, *Duration);
	}
	return bFoundMatchingStatus ? MaximumDuration : 0;
}

void RebuildCanonicalStatusMirrors(FWBUnitState& Unit)
{
	for (auto It = Unit.Statuses.CreateIterator(); It; ++It)
	{
		if (WBStatusSemantics::IsCanonicalStatusId(*It))
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = Unit.StatusTurnsRemaining.CreateIterator(); It; ++It)
	{
		if (WBStatusSemantics::IsCanonicalStatusId(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	for (const FWBStatusInstanceState& Status : Unit.StatusStates)
	{
		Unit.Statuses.Add(Status.StatusId);
		if (Status.Duration > 0)
		{
			Unit.StatusTurnsRemaining.Add(Status.StatusId, Status.Duration);
		}
	}
}

FWBPlayerStateData* FindOrAddTestPlayerState(FWBGameStateData& State, const int32 PlayerId, const int32 InitialRemainingMP)
{
	if (!FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		return nullptr;
	}

	FWBPlayerStateData* ExistingPlayer = State.GetMutablePlayerById(PlayerId);
	if (ExistingPlayer != nullptr)
	{
		ExistingPlayer->RemainingMP += FMath::Max(InitialRemainingMP, 0);
		return ExistingPlayer;
	}

	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = FMath::Max(InitialRemainingMP, 0);
	State.Players.Add(Player);
	return &State.Players.Last();
}
}

bool FWBUnitState::HasStatus(const FName StatusId) const
{
	const FName CanonicalStatusId =
		WBStatusSemantics::CanonicalizeStatusId(StatusId);
	if (WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId)
		&& FindCanonicalStatusStateIndex(StatusStates, CanonicalStatusId)
			!= INDEX_NONE)
	{
		return true;
	}
	for (const FName ActiveStatusId : Statuses)
	{
		if (WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId))
		{
			if (WBStatusSemantics::CanonicalizeStatusId(ActiveStatusId)
				== CanonicalStatusId)
			{
				return true;
			}
		}
		else if (ActiveStatusId == StatusId)
		{
			return true;
		}
	}

	return false;
}

bool FWBUnitState::IsUnitOnBoard() const
{
	return !bDefeated && !bRemovedFromBoard && X >= 0 && Y >= 0;
}

int32 FWBUnitState::GetOwnerPlayerIdForRules() const
{
	return FWBGameStateData::IsValidPlayerId(OwnerPlayerId)
		? OwnerPlayerId
		: GetControllerPlayerIdForRules();
}

int32 FWBUnitState::GetControllerPlayerIdForRules() const
{
	// Legacy callers still mutate OwnerId directly to change control, including
	// assigning INDEX_NONE for neutral NPCs. Production identity setters keep
	// both fields synchronized, so a mismatch means the compatibility field was
	// changed after normalization and must remain authoritative for control.
	return OwnerId != ControllerPlayerId ? OwnerId : ControllerPlayerId;
}

void FWBUnitState::SetOwnerAndControllerForRules(
	const int32 InOwnerPlayerId,
	const int32 InControllerPlayerId)
{
	OwnerPlayerId = InOwnerPlayerId;
	ControllerPlayerId = InControllerPlayerId;
	OwnerId = InControllerPlayerId;
}

void FWBUnitState::SetControllerPlayerIdForRules(const int32 InControllerPlayerId)
{
	ControllerPlayerId = InControllerPlayerId;
	OwnerId = InControllerPlayerId;
}

void FWBUnitState::NormalizeIdentityForRules()
{
	const int32 Controller = GetControllerPlayerIdForRules();
	const int32 Owner = FWBGameStateData::IsValidPlayerId(OwnerPlayerId)
		? OwnerPlayerId
		: Controller;
	SetOwnerAndControllerForRules(Owner, Controller);
}

int32 FWBUnitState::GetBaseRLForRules() const
{
	if (BaseRL != 0 || CurrentRL != 0 || RLTotal == 0)
	{
		return BaseRL;
	}

	return RLTotal;
}

int32 FWBUnitState::GetCurrentRLForRules() const
{
	if (BaseRL != 0 || CurrentRL != 0 || RLTotal == 0)
	{
		return CurrentRL;
	}

	return RLTotal;
}

int32 FWBUnitState::GetAvailableRLForRules() const
{
	return GetCurrentRLForRules() - RLUsed;
}

void FWBUnitState::SetCanonicalRL(
	const int32 InBaseRL,
	const int32 InCurrentRL,
	const int32 InRLUsed)
{
	BaseRL = InBaseRL;
	CurrentRL = InCurrentRL;
	RLTotal = InCurrentRL;
	RLUsed = InRLUsed;
}

int32 FWBUnitState::GetMaxArmor() const
{
	return FMath::Max(MaxArmor, 0);
}

int32 FWBUnitState::GetCurrentArmor() const
{
	return FMath::Clamp(CurrentArmor, 0, GetMaxArmor());
}

void FWBUnitState::SetArmorForTest(const int32 InCurrentArmor, const int32 InMaxArmor)
{
	MaxArmor = FMath::Max(InMaxArmor, 0);
	CurrentArmor = FMath::Clamp(InCurrentArmor, 0, MaxArmor);
}

void FWBUnitState::MarkUnitDefeated()
{
	bDefeated = true;
}

void FWBUnitState::RemoveUnitFromBoard()
{
	bRemovedFromBoard = true;
	X = -1;
	Y = -1;
}

void FWBUnitState::AddStatus(
	const FName StatusId,
	const int32 TurnsRemaining,
	const FWBStatusSourceProvenance& Source)
{
	if (StatusId.IsNone())
	{
		return;
	}

	const FName CanonicalStatusId =
		WBStatusSemantics::CanonicalizeStatusId(StatusId);
	if (!WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId))
	{
		Statuses.Add(StatusId);
		if (TurnsRemaining > 0)
		{
			StatusTurnsRemaining.Add(StatusId, TurnsRemaining);
		}
		else
		{
			StatusTurnsRemaining.Remove(StatusId);
		}
		return;
	}

	NormalizeStatusStateForRules();
	FWBStatusInstanceState* Existing = GetMutableStatusState(CanonicalStatusId);
	if (Existing == nullptr)
	{
		FWBStatusInstanceState State;
		State.TargetUnitId = UnitId;
		State.StatusId = CanonicalStatusId;
		State.Duration = FMath::Max(TurnsRemaining, 0);
		State.Source = Source;
		StatusStates.Add(MoveTemp(State));
		StatusStates.Sort(StatusStateLess);
	}
	else
	{
		Existing->TargetUnitId = UnitId;
		Existing->Duration = FMath::Max(TurnsRemaining, 0);
		Existing->Source = Source;
	}
	RebuildCanonicalStatusMirrors(*this);
}

void FWBUnitState::RemoveStatus(const FName StatusId)
{
	const FName CanonicalStatusId =
		WBStatusSemantics::CanonicalizeStatusId(StatusId);
	if (!WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId))
	{
		Statuses.Remove(StatusId);
		StatusTurnsRemaining.Remove(StatusId);
		return;
	}

	NormalizeStatusStateForRules();
	StatusStates.RemoveAll([CanonicalStatusId](const FWBStatusInstanceState& State)
	{
		return State.StatusId == CanonicalStatusId;
	});
	RebuildCanonicalStatusMirrors(*this);
}

int32 FWBUnitState::GetStatusTurnsRemaining(const FName StatusId) const
{
	const FName CanonicalStatusId =
		WBStatusSemantics::CanonicalizeStatusId(StatusId);
	if (WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId))
	{
		const int32 Index =
			FindCanonicalStatusStateIndex(StatusStates, CanonicalStatusId);
		if (Index != INDEX_NONE)
		{
			return StatusStates[Index].Duration;
		}
		return GetLegacyStatusDuration(*this, CanonicalStatusId);
	}
	if (const int32* TurnsRemaining = StatusTurnsRemaining.Find(StatusId))
	{
		return *TurnsRemaining;
	}

	return 0;
}

void FWBUnitState::SetStatusTurnsRemaining(const FName StatusId, const int32 TurnsRemaining)
{
	const FName CanonicalStatusId =
		WBStatusSemantics::CanonicalizeStatusId(StatusId);
	if (!WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId))
	{
		if (Statuses.Contains(StatusId))
		{
			if (TurnsRemaining > 0)
			{
				StatusTurnsRemaining.Add(StatusId, TurnsRemaining);
			}
			else
			{
				StatusTurnsRemaining.Remove(StatusId);
			}
		}
		return;
	}

	NormalizeStatusStateForRules();
	FWBStatusInstanceState* Status = GetMutableStatusState(CanonicalStatusId);
	if (Status == nullptr)
	{
		return;
	}
	Status->Duration = FMath::Max(TurnsRemaining, 0);
	RebuildCanonicalStatusMirrors(*this);
}

const FWBStatusInstanceState* FWBUnitState::GetStatusState(
	const FName StatusId) const

{
	const FName CanonicalStatusId =
		WBStatusSemantics::CanonicalizeStatusId(StatusId);
	const int32 Index =
		FindCanonicalStatusStateIndex(StatusStates, CanonicalStatusId);
	return Index == INDEX_NONE ? nullptr : &StatusStates[Index];
}

FWBStatusInstanceState* FWBUnitState::GetMutableStatusState(
	const FName StatusId)

{
	const FName CanonicalStatusId =
		WBStatusSemantics::CanonicalizeStatusId(StatusId);
	const int32 Index =
		FindCanonicalStatusStateIndex(StatusStates, CanonicalStatusId);
	return Index == INDEX_NONE ? nullptr : &StatusStates[Index];
}

void FWBUnitState::NormalizeStatusStateForRules()

{
	TArray<FWBStatusInstanceState> Normalized;
	for (const FWBStatusInstanceState& Existing : StatusStates)
	{
		const FName CanonicalStatusId =
			WBStatusSemantics::CanonicalizeStatusId(Existing.StatusId);
		if (!WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId))
		{
			continue;
		}
		FWBStatusInstanceState State = Existing;
		State.TargetUnitId = UnitId;
		State.StatusId = CanonicalStatusId;
		State.Duration = FMath::Max(State.Duration, 0);
		const int32 ExistingIndex =
			FindCanonicalStatusStateIndex(Normalized, CanonicalStatusId);
		if (ExistingIndex == INDEX_NONE)
		{
			Normalized.Add(MoveTemp(State));
		}
		else
		{
			FWBStatusInstanceState& Current = Normalized[ExistingIndex];
			Current.Duration = Current.Duration == 0 || State.Duration == 0
				? 0 : FMath::Max(Current.Duration, State.Duration);
		}
	}

	TArray<FName> LegacyStatuses = Statuses.Array();
	LegacyStatuses.Sort(FNameLexicalLess());
	for (const FName LegacyStatusId : LegacyStatuses)
	{
		const FName CanonicalStatusId =
			WBStatusSemantics::CanonicalizeStatusId(LegacyStatusId);
		if (!WBStatusSemantics::IsCanonicalStatusId(CanonicalStatusId))
		{
			continue;
		}
		const int32 LegacyDuration =
			GetLegacyStatusDuration(*this, CanonicalStatusId);
		const int32 ExistingIndex =
			FindCanonicalStatusStateIndex(Normalized, CanonicalStatusId);
		if (ExistingIndex == INDEX_NONE)
		{
			FWBStatusInstanceState State;
			State.TargetUnitId = UnitId;
			State.StatusId = CanonicalStatusId;
			State.Duration = LegacyDuration;
			Normalized.Add(MoveTemp(State));
		}
		else if (Normalized[ExistingIndex].Duration != LegacyDuration)
		{
			Normalized[ExistingIndex].Duration =
				Normalized[ExistingIndex].Duration == 0 || LegacyDuration == 0
					? 0
					: FMath::Max(
						Normalized[ExistingIndex].Duration,
						LegacyDuration);
		}
	}

	Normalized.Sort(StatusStateLess);
	StatusStates = MoveTemp(Normalized);
	RebuildCanonicalStatusMirrors(*this);
}

TArray<FWBStatusInstanceState> FWBUnitState::GetSortedStatusStatesForRules() const

{
	TArray<FWBStatusInstanceState> Sorted = StatusStates;
	Sorted.Sort(StatusStateLess);
	return Sorted;
}

TArray<FName> FWBUnitState::GetSortedStatusIdsForTrace() const
{
	TArray<FName> SortedStatusIds;
	for (const FWBStatusInstanceState& Status : StatusStates)
	{
		SortedStatusIds.AddUnique(Status.StatusId);
	}
	for (const FName StatusId : Statuses)
	{
		SortedStatusIds.AddUnique(
			WBStatusSemantics::IsCanonicalStatusId(StatusId)
				? WBStatusSemantics::CanonicalizeStatusId(StatusId)
				: StatusId);
	}
	SortedStatusIds.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});

	return SortedStatusIds;
}

bool FWBGameStateData::IsValidPlayerId(const int32 PlayerId)
{
	return PlayerId == 0 || PlayerId == 1;
}

bool FWBReactionWindowState::IsOpen() const
{
	return Kind != EWBReactionWindowKind::None;
}

void FWBReactionWindowState::Reset()
{
	*this = FWBReactionWindowState();
}

int32 FWBGameStateData::TileToIndex(const FWBTile& Tile)
{
	constexpr int32 BoardWidth = 9;
	return Tile.Y * BoardWidth + Tile.X;
}

int32 FWBGameStateData::GetCurrentPlayerId() const
{
	return CurrentPlayer;
}

int32 FWBGameStateData::GetActionPriorityPlayerId() const
{
	return PriorityPlayer;
}

const FWBPlayerStateData* FWBGameStateData::GetPlayerById(const int32 PlayerId) const
{
	for (const FWBPlayerStateData& Player : Players)
	{
		if (Player.PlayerId == PlayerId)
		{
			return &Player;
		}
	}

	return nullptr;
}

FWBPlayerStateData* FWBGameStateData::GetMutablePlayerById(const int32 PlayerId)
{
	for (FWBPlayerStateData& Player : Players)
	{
		if (Player.PlayerId == PlayerId)
		{
			return &Player;
		}
	}

	return nullptr;
}

const FWBPlayerStateData* FWBGameStateData::GetCurrentPlayer() const
{
	return GetPlayerById(CurrentPlayer);
}

FWBPlayerStateData* FWBGameStateData::GetMutableCurrentPlayer()
{
	return GetMutablePlayerById(CurrentPlayer);
}

const FWBCardZoneState& FWBGameStateData::GetCardZoneState() const
{
	return CardZoneState;
}

FWBCardZoneState& FWBGameStateData::GetMutableCardZoneStateForTest()
{
	return CardZoneState;
}

void FWBGameStateData::ClearCardZoneStateForTest()
{
	CardZoneState = FWBCardZoneState();
}

TArray<const FWBUnitState*> FWBGameStateData::GetUnitsForPlayer(const int32 PlayerId) const
{
	return GetUnitsControlledByPlayer(PlayerId);
}

TArray<FWBUnitState*> FWBGameStateData::GetMutableUnitsForPlayer(const int32 PlayerId)
{
	return GetMutableUnitsControlledByPlayer(PlayerId);
}

TArray<const FWBUnitState*> FWBGameStateData::GetUnitsControlledByPlayer(
	const int32 PlayerId) const
{
	TArray<const FWBUnitState*> ControlledUnits;
	for (const FWBUnitState& Unit : Units)
	{
		if (Unit.GetControllerPlayerIdForRules() == PlayerId && Unit.IsUnitOnBoard())
		{
			ControlledUnits.Add(&Unit);
		}
	}

	return ControlledUnits;
}

TArray<FWBUnitState*> FWBGameStateData::GetMutableUnitsControlledByPlayer(
	const int32 PlayerId)
{
	TArray<FWBUnitState*> ControlledUnits;
	for (FWBUnitState& Unit : Units)
	{
		if (Unit.GetControllerPlayerIdForRules() == PlayerId && Unit.IsUnitOnBoard())
		{
			ControlledUnits.Add(&Unit);
		}
	}

	return ControlledUnits;
}

TArray<const FWBUnitState*> FWBGameStateData::GetUnitsOwnedByPlayer(
	const int32 PlayerId) const
{
	TArray<const FWBUnitState*> OwnedUnits;
	for (const FWBUnitState& Unit : Units)
	{
		if (Unit.GetOwnerPlayerIdForRules() == PlayerId && Unit.IsUnitOnBoard())
		{
			OwnedUnits.Add(&Unit);
		}
	}

	return OwnedUnits;
}

bool FWBGameStateData::IsNormalTurnPhase() const
{
	return Phase == EWBGamePhase::NormalTurn;
}

bool FWBGameStateData::IsResponsePhase() const
{
	return Phase == EWBGamePhase::Response;
}

bool FWBGameStateData::HasOpenReactionWindow() const
{
	return ReactionWindow.IsOpen();
}

void FWBGameStateData::ClearReactionWindow()
{
	ReactionWindow.Reset();
}

void FWBGameStateData::AdvanceTurnBasic()
{
	const int32 PreviousPlayer = CurrentPlayer;
	CurrentPlayer = PreviousPlayer == 0 ? 1 : 0;
	PriorityPlayer = CurrentPlayer;
	Phase = EWBGamePhase::NormalTurn;
	TurnNumber += 1;
	// Compatibility transition only; the coordinator owns the full turn-start sequence.
}

bool FWBGameStateData::ResetActionResourcesForPlayer(const int32 PlayerId, FString& OutReason)
{
	FWBPlayerStateData* Player = GetMutablePlayerById(PlayerId);
	if (Player == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	Player->WallsLeft = 1;
	Player->WallRemovalsLeft = TurnNumber >= 30 ? 1 : 0;

	for (FWBUnitState* Unit : GetMutableUnitsForPlayer(PlayerId))
	{
		if (Unit != nullptr)
		{
			Unit->AttacksLeft = FMath::Max(Unit->MaxAttacksPerTurn, 0);
		}
	}

	OutReason.Reset();
	return true;
}

bool FWBGameStateData::ApplyTurnStartResourceSetupForPlayer(
	const int32 PlayerId,
	const int32 ExplicitMPRoll,
	FString& OutReason)
{
	if (!ApplyTurnStartMPRollForPlayer(
		PlayerId,
		ExplicitMPRoll,
		OutReason))
	{
		return false;
	}
	return ResetTurnStartResourcesForPlayer(
		PlayerId,
		OutReason);
}

bool FWBGameStateData::ApplyTurnStartMPRollForPlayer(
	const int32 PlayerId,
	const int32 ExplicitMPRoll,
	FString& OutReason)
{
	if (!IsValidPlayerId(PlayerId))
	{
		OutReason = TEXT("bad_player");
		return false;
	}

	if (ExplicitMPRoll < 1 || ExplicitMPRoll > 6)
	{
		OutReason = TEXT("invalid_mp_roll");
		return false;
	}

	FWBPlayerStateData* Player = GetMutablePlayerById(PlayerId);
	if (Player == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	CurrentPlayer = PlayerId;
	PriorityPlayer = PlayerId;
	Phase = EWBGamePhase::NormalTurn;
	Player->LastMPRoll = ExplicitMPRoll;
	Player->RemainingMP = ExplicitMPRoll;
	OutReason.Reset();
	return true;
}

bool FWBGameStateData::ResetTurnStartResourcesForPlayer(
	const int32 PlayerId,
	FString& OutReason)
{
	if (!IsValidPlayerId(PlayerId))
	{
		OutReason = TEXT("bad_player");
		return false;
	}
	if (GetPlayerById(PlayerId) == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	ClearActivationUsageKeysForPlayer(PlayerId);
	return ResetActionResourcesForPlayer(
		PlayerId,
		OutReason);
}

bool FWBGameStateData::HasActivationUsageKeyThisTurn(const int32 PlayerId, const FString& Key) const
{
	if (!IsValidPlayerId(PlayerId) || Key.IsEmpty())
	{
		return false;
	}

	const TSet<FString>* Keys = ActivationUsageKeysThisTurn.Find(PlayerId);
	return Keys != nullptr && Keys->Contains(Key);
}

void FWBGameStateData::MarkActivationUsageKeyForTest(const int32 PlayerId, const FString& Key)
{
	if (!IsValidPlayerId(PlayerId) || Key.IsEmpty())
	{
		return;
	}

	TSet<FString>& Keys = ActivationUsageKeysThisTurn.FindOrAdd(PlayerId);
	Keys.Add(Key);
}

void FWBGameStateData::ClearActivationUsageKeysForPlayer(const int32 PlayerId)
{
	if (!IsValidPlayerId(PlayerId))
	{
		return;
	}

	ActivationUsageKeysThisTurn.Remove(PlayerId);
}

bool FWBGameStateData::HasPendingAttack() const
{
	return PendingAttack.bActive;
}

void FWBGameStateData::ClearPendingAttack()
{
	PendingAttack = FWBPendingAttackState();
}

bool FWBGameStateData::HasPendingMandatoryDeckChoice() const
{
	return PendingMandatoryDeckChoice.bActive;
}

void FWBGameStateData::ClearPendingMandatoryDeckChoice()
{
	PendingMandatoryDeckChoice.Reset();
}

void FWBGameStateData::SetPendingAttackForTest(const FWBPendingAttackState& InPendingAttack)
{
	PendingAttack = InPendingAttack;
}

const FWBUnitState* FWBGameStateData::GetUnitById(const int32 UnitId) const
{
	for (const FWBUnitState& Unit : Units)
	{
		if (Unit.UnitId == UnitId)
		{
			return &Unit;
		}
	}

	return nullptr;
}

FWBUnitState* FWBGameStateData::GetMutableUnitById(const int32 UnitId)
{
	for (FWBUnitState& Unit : Units)
	{
		if (Unit.UnitId == UnitId)
		{
			return &Unit;
		}
	}

	return nullptr;
}

int32 FWBGameStateData::UnitIdAt(const FWBTile& Tile) const
{
	for (const FWBUnitState& Unit : Units)
	{
		if (Unit.IsUnitOnBoard() && FWBTile(Unit.X, Unit.Y) == Tile)
		{
			return Unit.UnitId;
		}
	}

	return -1;
}

bool FWBGameStateData::IsTileOccupied(const FWBTile& Tile) const
{
	return UnitIdAt(Tile) != -1;
}

bool FWBGameStateData::AddUnitForTest(const FWBUnitState& Unit)
{
	const FWBTile Tile(Unit.X, Unit.Y);
	if (Unit.UnitId < 0 || GetUnitById(Unit.UnitId) != nullptr)
	{
		return false;
	}

	if (!Unit.bRemovedFromBoard && !WBRules::IsTileInBounds(Tile))
	{
		return false;
	}

	if (Unit.IsUnitOnBoard() && IsTileOccupied(Tile))
	{
		return false;
	}

	FWBUnitState NormalizedUnit = Unit;
	NormalizedUnit.NormalizeIdentityForRules();
	NormalizedUnit.NormalizeStatusStateForRules();
	FindOrAddTestPlayerState(
		*this,
		NormalizedUnit.GetControllerPlayerIdForRules(),
		NormalizedUnit.MPRemaining);
	NormalizedUnit.SetArmorForTest(Unit.CurrentArmor, Unit.MaxArmor);
	Units.Add(NormalizedUnit);
	return true;
}

void FWBGameStateData::AddWallForTest(const FWBWallEdge& Edge)
{
	if (!WBRules::IsValidWallEdge(Edge))
	{
		return;
	}

	const FWBWallEdge NormalizedEdge = WBRules::NormalizeWallEdge(Edge);
	for (const FWBWallEdge& ExistingEdge : Walls)
	{
		if (WBRules::AreSameWallEdge(ExistingEdge, NormalizedEdge))
		{
			return;
		}
	}

	Walls.Add(NormalizedEdge);
}

bool FWBGameStateData::RemoveWallForTest(const FWBWallEdge& Edge)
{
	for (int32 Index = 0; Index < Walls.Num(); ++Index)
	{
		if (WBRules::AreSameWallEdge(Walls[Index], Edge))
		{
			Walls.RemoveAt(Index);
			return true;
		}
	}

	return false;
}

FName FWBGameStateData::GetTerrainAt(const FWBTile& Tile) const
{
	if (!WBRules::IsTileInBounds(Tile))
	{
		return DefaultTerrainId;
	}

	const FName* TerrainId = TerrainByTileIndex.Find(TileToIndex(Tile));
	return TerrainId != nullptr ? *TerrainId : DefaultTerrainId;
}

bool FWBGameStateData::SetTerrainAt(const FWBTile& Tile, const FName TerrainId)
{
	if (!WBRules::IsTileInBounds(Tile) || TerrainId.IsNone())
	{
		return false;
	}

	if (TerrainId.GetPlainNameString().Equals(DefaultTerrainId.GetPlainNameString(), ESearchCase::IgnoreCase))
	{
		TerrainByTileIndex.Remove(TileToIndex(Tile));
		return true;
	}

	TerrainByTileIndex.Add(TileToIndex(Tile), TerrainId);
	return true;
}

void FWBGameStateData::SetTerrainForTest(const FWBTile& Tile, const FName TerrainId)
{
	SetTerrainAt(Tile, TerrainId);
}

void FWBGameStateData::ClearTerrainForTest(const FWBTile& Tile)
{
	if (!WBRules::IsTileInBounds(Tile))
	{
		return;
	}

	TerrainByTileIndex.Remove(TileToIndex(Tile));
}
