#pragma once

#include "CoreMinimal.h"

class WANDBOUNDCORE_API WBDeterministicRandom
{
public:
	static uint32 NextUInt(uint32& InOutState);
	static bool FlipCoinHeads(uint32& InOutState);
};
