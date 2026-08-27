#include "WBDeterministicRandom.h"

uint32 WBDeterministicRandom::NextUInt(uint32& InOutState)
{
	InOutState = InOutState * 1664525u + 1013904223u;
	return InOutState;
}

bool WBDeterministicRandom::FlipCoinHeads(uint32& InOutState)
{
	return NextUInt(InOutState) % 2u == 0u;
}
