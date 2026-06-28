#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationCandidate.h"

struct FWBPublicBoardSummary;

enum class EWBCardActivationTargetOptionType : uint8
{
	Unknown,
	Unit
};

struct WANDBOUNDCORE_API FWBCardActivationTargetOption
{
	EWBCardActivationTargetOptionType Type = EWBCardActivationTargetOptionType::Unknown;

	int32 TargetUnitId = -1;
	int32 TargetOwnerPlayerId = -1;
	FWBTile TargetTile;
	FString TargetCardId;
	FString PublicLabel;
};

struct WANDBOUNDCORE_API FWBCardActivationLegalAction
{
	FString ActivationActionId;

	int32 PlayerId = -1;
	int32 SourceUnitId = -1;

	FString PublicLabel;

	FWBEffectTargetRef Target;
	TArray<FWBCardActivationTargetOption> TargetOptions;
	FWBCardActivationCandidate Candidate;
	FWBCardActivationCommand Command;
};

struct WANDBOUNDCORE_API FWBCardActivationLegalActionSet
{
	TArray<FWBCardActivationLegalAction> Actions;
};

struct WANDBOUNDCORE_API FWBCardActivationLegalActionGenerationResult
{
	bool bOk = false;
	FString Reason;
	FWBCardActivationLegalActionSet ActionSet;
};

struct WANDBOUNDCORE_API FWBCardActivationLegalActionPresentationEntry
{
	FString ActivationActionId;
	FString PublicLabel;

	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	int32 TargetUnitId = -1;

	bool bHasPublicSourceUnit = false;
	bool bHasPublicTargetUnit = false;

	FString SourcePublicCardId;
	FString TargetPublicCardId;
};

struct WANDBOUNDCORE_API FWBCardActivationLegalActionPresentationSnapshot
{
	TArray<FWBCardActivationLegalActionPresentationEntry> Entries;
};

class WANDBOUNDCORE_API WBCardActivationLegalActionPresentation
{
public:
	static FWBCardActivationLegalActionPresentationSnapshot BuildSnapshot(
		const FWBCardActivationLegalActionSet& ActionSet,
		const FWBPublicBoardSummary& PublicBoardSummary);

	static bool TryFindEntryByActivationActionId(
		const FWBCardActivationLegalActionPresentationSnapshot& Snapshot,
		const FString& ActivationActionId,
		FWBCardActivationLegalActionPresentationEntry& OutEntry);
};
