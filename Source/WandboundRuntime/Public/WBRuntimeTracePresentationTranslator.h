#pragma once

#include "CoreMinimal.h"
#include "WBReplayTrace.h"
#include "WBRuntimePresentationEvent.h"

class WANDBOUNDRUNTIME_API WBRuntimeTracePresentationTranslator
{
public:
	static FWBRuntimePresentationTranslationResult Translate(const TArray<FWBTraceEvent>& TraceEvents);
};
