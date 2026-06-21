#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBPublicBoardSummary.h"

enum class EWBRuntimeActionPresentationType : uint8
{
	Unknown,
	Move,
	Attack,
	EndTurn,
	Pass,
	PassResponse
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActionPresentationEntry
{
	FString ActionId;
	EWBRuntimeActionPresentationType Type = EWBRuntimeActionPresentationType::Unknown;
	FString PublicLabel;

	int32 PlayerId = -1;

	int32 SourceUnitId = -1;
	int32 TargetUnitId = -1;

	FWBTile FromTile;
	FWBTile ToTile;

	FString SourcePublicCardId;
	FString TargetPublicCardId;

	bool bHasSourcePublicUnit = false;
	bool bHasTargetPublicUnit = false;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeLegalActionPresentationSnapshot
{
	TArray<FWBRuntimeActionPresentationEntry> Entries;
};

class WANDBOUNDRUNTIME_API WBRuntimeLegalActionPresentation
{
public:
	static FWBRuntimeLegalActionPresentationSnapshot BuildSnapshot(
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FWBPublicBoardSummary& PublicBoardSummary);

	static bool TryFindEntryByActionId(
		const FWBRuntimeLegalActionPresentationSnapshot& Snapshot,
		const FString& ActionId,
		FWBRuntimeActionPresentationEntry& OutEntry);
};
