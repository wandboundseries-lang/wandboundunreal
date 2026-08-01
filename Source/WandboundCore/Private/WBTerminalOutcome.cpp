#include "WBTerminalOutcome.h"

FName WBTerminalOutcomeNames::ReasonToName(const EWBTerminalReason Reason)
{
	switch (Reason)
	{
	case EWBTerminalReason::HeroDefeatedWithoutReplacement:
		return FName(TEXT("hero_defeated_without_replacement"));
	case EWBTerminalReason::None:
	default:
		return NAME_None;
	}
}

FName WBTerminalOutcomeNames::SourceToName(const EWBTerminalSource Source)
{
	switch (Source)
	{
	case EWBTerminalSource::Attack:
		return FName(TEXT("attack"));
	case EWBTerminalSource::Status:
		return FName(TEXT("status"));
	case EWBTerminalSource::Trap:
		return FName(TEXT("trap"));
	case EWBTerminalSource::NPC:
		return FName(TEXT("npc"));
	case EWBTerminalSource::Effect:
		return FName(TEXT("effect"));
	case EWBTerminalSource::Unknown:
		return FName(TEXT("unknown"));
	case EWBTerminalSource::None:
	default:
		return NAME_None;
	}
}

EWBTerminalReason WBTerminalOutcomeNames::ReasonFromName(const FName Name)
{
	return Name == FName(TEXT("hero_defeated_without_replacement"))
		? EWBTerminalReason::HeroDefeatedWithoutReplacement
		: EWBTerminalReason::None;
}

EWBTerminalSource WBTerminalOutcomeNames::SourceFromName(const FName Name)
{
	if (Name == FName(TEXT("attack"))) return EWBTerminalSource::Attack;
	if (Name == FName(TEXT("status"))) return EWBTerminalSource::Status;
	if (Name == FName(TEXT("trap"))) return EWBTerminalSource::Trap;
	if (Name == FName(TEXT("npc"))) return EWBTerminalSource::NPC;
	if (Name == FName(TEXT("effect"))) return EWBTerminalSource::Effect;
	if (Name == FName(TEXT("unknown"))) return EWBTerminalSource::Unknown;
	return EWBTerminalSource::None;
}
