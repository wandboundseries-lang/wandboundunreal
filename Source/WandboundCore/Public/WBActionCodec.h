#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBActionDecodeResult
{
	bool bOk = false;
	FString Reason;
	FWBAction Action;
};

class WANDBOUNDCORE_API WBActionCodec
{
public:
	static FString GetActionKind(EWBActionType Type);
	static FString MakeActionId(const FWBAction& Action);
	static TArray<FString> MakeActionIds(const TArray<FWBAction>& Actions);
	static FString SerializeAction(const FWBAction& Action);
	static FWBActionDecodeResult DecodeAction(const FString& SerializedAction, const FWBGameStateData& State);
};
