#pragma once

#include "CoreMinimal.h"
#include "WBCardZoneState.h"
#include "WBTypes.h"

struct WANDBOUNDCORE_API FWBUnitState
{
	int32 UnitId = -1;
	int32 OwnerId = -1;
	FString CardId;
	int32 X = -1;
	int32 Y = -1;
	int32 HP = 1;
	int32 MaxHP = 1;
	int32 CurrentArmor = 0;
	int32 MaxArmor = 0;
	int32 ATK = 1;
	int32 AR = 1;
	int32 RLTotal = 0;
	int32 RLUsed = 0;
	int32 AttacksLeft = 0;
	int32 MaxAttacksPerTurn = 1;
	int32 MPRemaining = 0;
	bool bDefeated = false;
	bool bRemovedFromBoard = false;
	TSet<FName> Statuses;
	TMap<FName, int32> StatusTurnsRemaining;
	TSet<FName> Passives;

	bool IsUnitOnBoard() const;
	int32 GetCurrentArmor() const;
	int32 GetMaxArmor() const;
	void SetArmorForTest(int32 InCurrentArmor, int32 InMaxArmor);
	void MarkUnitDefeated();
	void RemoveUnitFromBoard();
	bool HasStatus(FName StatusId) const;
	void AddStatus(FName StatusId, int32 TurnsRemaining = 0);
	void RemoveStatus(FName StatusId);
	int32 GetStatusTurnsRemaining(FName StatusId) const;
	void SetStatusTurnsRemaining(FName StatusId, int32 TurnsRemaining);
	TArray<FName> GetSortedStatusIdsForTrace() const;
};

enum class EWBGamePhase : uint8
{
	NormalTurn,
	Response
};

struct WANDBOUNDCORE_API FWBPlayerStateData
{
	int32 PlayerId = -1;
	int32 HeroUnitId = -1;
	int32 WallsLeft = 0;
	int32 WallRemovalsLeft = 0;
	int32 RemainingMP = 0;
	int32 LastMPRoll = 0;
	TArray<FString> Deck;
	TArray<FString> Hand;
	TArray<FString> Discard;
};

struct WANDBOUNDCORE_API FWBPendingAttackState
{
	bool bActive = false;
	int32 AttackerUnitId = -1;
	int32 DefenderUnitId = -1;
	int32 AttackingPlayerId = -1;
	FWBTile AttackerTile;
	FWBTile DefenderTile;
	FString DeclarationActionId;
};

struct WANDBOUNDCORE_API FWBGameStateData
{
	int32 CurrentPlayer = 0;
	int32 PriorityPlayer = 0;
	int32 TurnNumber = 1;
	EWBGamePhase Phase = EWBGamePhase::NormalTurn;
	bool bGameOver = false;
	int32 WinnerPlayerId = -1;
	TArray<FWBUnitState> Units;
	TArray<FWBWallEdge> Walls;
	FName DefaultTerrainId = FName(TEXT("Normal"));
	TMap<int32, FName> TerrainByTileIndex;
	TArray<FWBPlayerStateData> Players;
	FWBPendingAttackState PendingAttack;
	TMap<int32, TSet<FString>> ActivationUsageKeysThisTurn;
	FWBCardZoneState CardZoneState;

	static bool IsValidPlayerId(int32 PlayerId);
	static int32 TileToIndex(const FWBTile& Tile);
	int32 GetCurrentPlayerId() const;
	int32 GetActionPriorityPlayerId() const;
	const FWBPlayerStateData* GetPlayerById(int32 PlayerId) const;
	FWBPlayerStateData* GetMutablePlayerById(int32 PlayerId);
	const FWBPlayerStateData* GetCurrentPlayer() const;
	FWBPlayerStateData* GetMutableCurrentPlayer();
	const FWBCardZoneState& GetCardZoneState() const;
	FWBCardZoneState& GetMutableCardZoneStateForTest();
	void ClearCardZoneStateForTest();
	TArray<const FWBUnitState*> GetUnitsForPlayer(int32 PlayerId) const;
	TArray<FWBUnitState*> GetMutableUnitsForPlayer(int32 PlayerId);
	bool IsNormalTurnPhase() const;
	bool IsResponsePhase() const;
	void AdvanceTurnBasic();
	bool ResetActionResourcesForPlayer(int32 PlayerId, FString& OutReason);
	bool ApplyTurnStartResourceSetupForPlayer(int32 PlayerId, int32 ExplicitMPRoll, FString& OutReason);
	bool HasActivationUsageKeyThisTurn(int32 PlayerId, const FString& Key) const;
	void MarkActivationUsageKeyForTest(int32 PlayerId, const FString& Key);
	void ClearActivationUsageKeysForPlayer(int32 PlayerId);
	bool HasPendingAttack() const;
	void ClearPendingAttack();
	void SetPendingAttackForTest(const FWBPendingAttackState& InPendingAttack);
	const FWBUnitState* GetUnitById(int32 UnitId) const;
	FWBUnitState* GetMutableUnitById(int32 UnitId);
	int32 UnitIdAt(const FWBTile& Tile) const;
	bool IsTileOccupied(const FWBTile& Tile) const;
	bool AddUnitForTest(const FWBUnitState& Unit);
	void AddWallForTest(const FWBWallEdge& Edge);
	bool RemoveWallForTest(const FWBWallEdge& Edge);
	FName GetTerrainAt(const FWBTile& Tile) const;
	void SetTerrainForTest(const FWBTile& Tile, FName TerrainId);
	void ClearTerrainForTest(const FWBTile& Tile);
};
