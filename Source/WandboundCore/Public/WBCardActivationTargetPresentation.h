#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationLegalAction.h"

struct FWBPublicBoardSummary;

enum class EWBCardActivationTargetPresentationKind : uint8
{
	Unknown,
	None,
	Unit,
	Tile,
	WallEdge
};

struct WANDBOUNDCORE_API FWBCardActivationTargetPresentationEntry
{
	FString ActivationActionId;
	FString PublicActivationLabel;

	EWBCardActivationTargetPresentationKind TargetKind = EWBCardActivationTargetPresentationKind::Unknown;

	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	int32 TargetUnitId = -1;
	int32 TargetOptionCount = 0;

	FWBTile TargetTile;
	FWBWallEdge TargetWallEdge;

	bool bHasPublicSourceUnit = false;
	bool bHasPublicTargetUnit = false;

	FString SourcePublicCardId;
	FString TargetPublicCardId;

	FString PublicTargetLabel;
};

struct WANDBOUNDCORE_API FWBCardActivationTargetPresentationSnapshot
{
	TArray<FWBCardActivationTargetPresentationEntry> Entries;
};

class WANDBOUNDCORE_API WBCardActivationTargetPresentation
{
public:
	static FWBCardActivationTargetPresentationSnapshot BuildSnapshot(
		const FWBCardActivationLegalActionSet& ActionSet,
		const FWBPublicBoardSummary& PublicBoardSummary);

	static bool TryFindEntryByActivationActionId(
		const FWBCardActivationTargetPresentationSnapshot& Snapshot,
		const FString& ActivationActionId,
		FWBCardActivationTargetPresentationEntry& OutEntry);
};
