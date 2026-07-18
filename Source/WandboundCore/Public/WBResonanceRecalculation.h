#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBResonanceRecalculationResult
{
	bool bSucceeded = false;
	int32 UnitId = INDEX_NONE;
	int32 OwnerPlayerId = INDEX_NONE;

	int32 PreviousBaseRL = 0;
	int32 PreviousCurrentRL = 0;
	int32 PreviousRLUsed = 0;

	int32 RecalculatedBaseRL = 0;
	int32 RecalculatedCurrentRL = 0;
	int32 RecalculatedRLUsed = 0;
	int32 AvailableRL = 0;

	bool bIsOverflowing = false;
	FString FailureReason;

	TArray<FString> OrderedModifierSourceIds;
	TArray<FString> OrderedEquippedCardInstanceIds;
};

class WANDBOUNDCORE_API WBResonanceRecalculation
{
public:
	static FWBResonanceRecalculationResult CalculateUnit(
		const FWBGameStateData& State,
		int32 UnitId,
		const FWBCardDefinitionRepository& Repository);

	static FWBResonanceRecalculationResult RecalculateUnit(
		FWBGameStateData& State,
		int32 UnitId,
		const FWBCardDefinitionRepository& Repository);

	static TArray<FWBResonanceRecalculationResult> RecalculateAllUnits(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository);

	static bool RecalculationResultToJsonStringForTest(
		const FWBResonanceRecalculationResult& Result,
		FString& OutJson);
};
