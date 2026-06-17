#include "WBMPRollSource.h"

namespace
{
bool IsValidMPRoll(const int32 Roll)
{
	return Roll >= 1 && Roll <= 6;
}
}

FWBFixedMPRollSource::FWBFixedMPRollSource(const int32 InRoll)
	: Roll(InRoll)
{
}

bool FWBFixedMPRollSource::TryGetNextMPRoll(
	const int32 PlayerId,
	int32& OutRoll,
	FString& OutReason)
{
	(void)PlayerId;
	OutRoll = 0;

	if (!IsValidMPRoll(Roll))
	{
		OutReason = TEXT("invalid_mp_roll");
		return false;
	}

	OutRoll = Roll;
	OutReason.Reset();
	return true;
}

void FWBQueuedMPRollSource::EnqueueRoll(const int32 Roll)
{
	Rolls.Add(Roll);
}

int32 FWBQueuedMPRollSource::NumRemainingRolls() const
{
	return Rolls.Num();
}

bool FWBQueuedMPRollSource::TryGetNextMPRoll(
	const int32 PlayerId,
	int32& OutRoll,
	FString& OutReason)
{
	(void)PlayerId;
	OutRoll = 0;

	if (Rolls.Num() == 0)
	{
		OutReason = TEXT("mp_roll_queue_empty");
		return false;
	}

	const int32 NextRoll = Rolls[0];
	if (!IsValidMPRoll(NextRoll))
	{
		OutReason = TEXT("invalid_mp_roll");
		return false;
	}

	OutRoll = NextRoll;
	Rolls.RemoveAt(0);
	OutReason.Reset();
	return true;
}
