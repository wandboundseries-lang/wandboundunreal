#pragma once

#include "CoreMinimal.h"

enum class EWBTerminalReason : uint8
{
	None,
	HeroDefeatedWithoutReplacement
};

enum class EWBTerminalSource : uint8
{
	None,
	Attack,
	Status,
	Trap,
	NPC,
	Effect,
	Unknown
};

struct WANDBOUNDCORE_API FWBTerminalOutcome
{
	bool bTerminal = false;
	int32 WinnerPlayerId = -1;
	int32 LoserPlayerId = -1;
	EWBTerminalReason Reason = EWBTerminalReason::None;
	EWBTerminalSource Source = EWBTerminalSource::None;
	int32 TurnNumber = -1;
	int32 CoordinatorRevision = -1;
	int32 TraceIndex = -1;
};

class WANDBOUNDCORE_API WBTerminalOutcomeNames
{
public:
	static FName ReasonToName(EWBTerminalReason Reason);
	static FName SourceToName(EWBTerminalSource Source);
	static EWBTerminalReason ReasonFromName(FName Name);
	static EWBTerminalSource SourceFromName(FName Name);
};
