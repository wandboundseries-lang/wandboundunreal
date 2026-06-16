#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

enum class EWBTurnCommandMode : uint8
{
	BasicEndTurn,
	DeterministicFullTransition
};

struct WANDBOUNDCORE_API FWBTurnCommand
{
	EWBTurnCommandMode Mode = EWBTurnCommandMode::BasicEndTurn;
	int32 ActingPlayerId = -1;
	int32 NextPlayerExplicitMPRoll = 0;
};

class WANDBOUNDCORE_API WBTurnController
{
public:
	static bool CanApplyTurnCommand(const FWBGameStateData& State, const FWBTurnCommand& Command, FString& OutReason);
	static FWBApplyActionResult ApplyTurnCommand(FWBGameStateData& State, const FWBTurnCommand& Command);
};
