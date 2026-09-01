#include "WBTerrainRules.h"

namespace
{
FString NormalizeTerrainId(const FName TerrainId)
{
	return TerrainId.GetPlainNameString().ToLower();
}
}

FWBTerrainRuleDefinition WBTerrainRules::GetRules(const FName TerrainId)
{
	FWBTerrainRuleDefinition Result;
	if (NormalizeTerrainId(TerrainId) == TEXT("highground"))
	{
		Result.EntryMPCostModifier = 1;
		Result.OccupantARModifier = 1;
		Result.AttackWallBehavior =
			EWBTerrainAttackWallBehavior::IgnoreWalls;
	}
	return Result;
}

int32 WBTerrainRules::GetEntryMPCostModifier(const FName TerrainId)
{
	return GetRules(TerrainId).EntryMPCostModifier;
}

int32 WBTerrainRules::GetOccupantARModifier(const FName TerrainId)
{
	return GetRules(TerrainId).OccupantARModifier;
}

bool WBTerrainRules::AttacksIgnoreWalls(const FName TerrainId)
{
	return GetRules(TerrainId).AttackWallBehavior
		== EWBTerrainAttackWallBehavior::IgnoreWalls;
}

bool WBTerrainRules::IsSupportedTerrain(const FName TerrainId)
{
	const FString Id = NormalizeTerrainId(TerrainId);
	return Id == TEXT("normal") || Id == TEXT("mud")
		|| Id == TEXT("lava") || Id == TEXT("ice")
		|| Id == TEXT("water") || Id == TEXT("highground");
}
