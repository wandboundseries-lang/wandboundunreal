#pragma once

#include "CoreMinimal.h"
#include "WBRuntimeTurnResolutionAdapter.h"

class FJsonObject;

class WANDBOUNDCORE_API WBRuntimeResultSerializer
{
public:
	static constexpr int32 RuntimeSelectedActionResultSchemaVersion = 1;

	static TSharedRef<FJsonObject> RuntimeSelectedActionResultToJsonObject(
		const FWBRuntimeSelectedActionResult& Result);

	static bool RuntimeSelectedActionResultToJsonString(
		const FWBRuntimeSelectedActionResult& Result,
		FString& OutJson);
};
