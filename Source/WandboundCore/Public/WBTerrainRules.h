#pragma once

#include "CoreMinimal.h"

enum class EWBTerrainAttackWallBehavior : uint8
{
	BlockedByWalls,
	IgnoreWalls
};

struct WANDBOUNDCORE_API FWBTerrainRuleDefinition
{
	int32 EntryMPCostModifier = 0;
	int32 OccupantARModifier = 0;
	EWBTerrainAttackWallBehavior AttackWallBehavior =
		EWBTerrainAttackWallBehavior::BlockedByWalls;
};

class WANDBOUNDCORE_API WBTerrainRules
{
public:
	static FWBTerrainRuleDefinition GetRules(FName TerrainId);
	static int32 GetEntryMPCostModifier(FName TerrainId);
	static int32 GetOccupantARModifier(FName TerrainId);
	static bool AttacksIgnoreWalls(FName TerrainId);
	static bool IsSupportedTerrain(FName TerrainId);
};
