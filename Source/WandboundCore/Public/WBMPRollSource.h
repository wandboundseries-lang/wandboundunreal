#pragma once

#include "CoreMinimal.h"

class WANDBOUNDCORE_API IWBMPRollSource
{
public:
	virtual ~IWBMPRollSource() = default;
	virtual bool TryGetNextMPRoll(int32 PlayerId, int32& OutRoll, FString& OutReason) = 0;
};

class WANDBOUNDCORE_API FWBFixedMPRollSource final : public IWBMPRollSource
{
public:
	explicit FWBFixedMPRollSource(int32 InRoll);
	virtual bool TryGetNextMPRoll(int32 PlayerId, int32& OutRoll, FString& OutReason) override;

private:
	int32 Roll = 0;
};

class WANDBOUNDCORE_API FWBQueuedMPRollSource final : public IWBMPRollSource
{
public:
	void EnqueueRoll(int32 Roll);
	int32 NumRemainingRolls() const;
	virtual bool TryGetNextMPRoll(int32 PlayerId, int32& OutRoll, FString& OutReason) override;

private:
	TArray<int32> Rolls;
};
