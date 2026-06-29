#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBResonanceLoadSummary
{
	bool bOk = false;
	FString Reason;

	int32 UnitId = -1;
	int32 OwnerPlayerId = -1;

	int32 CurrentRL = 0;
	int32 RLUsed = 0;
	int32 AvailableRL = 0;
};

class WANDBOUNDCORE_API WBResonanceLoad
{
public:
	static FWBResonanceLoadSummary BuildUnitLoadSummary(
		const FWBGameStateData& State,
		int32 UnitId);

	static bool CanPayRR(
		const FWBGameStateData& State,
		int32 UnitId,
		int32 RR,
		FWBResonanceLoadSummary& OutSummary);

	static FString ResultReasonToString(const FString& Reason);
};
