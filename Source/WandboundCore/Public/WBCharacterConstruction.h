#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinition.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBCharacterConstructionRequest
{
	int32 UnitId = INDEX_NONE;
	int32 OwnerPlayerId = INDEX_NONE;
	int32 ControllerPlayerId = INDEX_NONE;
	FString CardId;
	FWBTile Tile;
};

struct WANDBOUNDCORE_API FWBCharacterConstructionResult
{
	bool bOk = false;
	FString Reason;
	FWBUnitState Unit;
};

class WANDBOUNDCORE_API WBCharacterConstruction
{
public:
	static int32 AllocateNextUnitId(const FWBGameStateData& State);

	static bool IsValidCharacterDefinition(
		const FWBCardDefinition& Definition);

	static FWBCharacterConstructionResult BuildUnit(
		const FWBCardDefinition& Definition,
		const FWBCharacterConstructionRequest& Request);
};
