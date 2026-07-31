#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBRelocationStep
{
	int32 UnitId = -1;
	FWBTile FromTile;
	FWBTile ToTile;
};

struct WANDBOUNDCORE_API FWBTurnOneRestrictionQuery
{
	bool bOk = false;
	FString Reason;

	static FWBTurnOneRestrictionQuery Allow();
	static FWBTurnOneRestrictionQuery Deny(const TCHAR* Reason);
};

class WANDBOUNDCORE_API WBTurnOneRestrictions
{
public:
	static bool IsFirstPlayerTurnOneRestrictionActive(
		const FWBGameStateData& State);

	static FWBTurnOneRestrictionQuery QuerySummonPlacement(
		const FWBGameStateData& State,
		int32 SummoningPlayerId,
		const FWBTile& Destination);

	static FWBTurnOneRestrictionQuery QueryRelocation(
		const FWBGameStateData& State,
		const TArray<FWBRelocationStep>& Steps);

	static FWBTurnOneRestrictionQuery QueryAttackTarget(
		const FWBGameStateData& State,
		int32 AttackingPlayerId,
		int32 TargetOwnerId);
};
