#pragma once

#include "CoreMinimal.h"
#include "WBCardZoneState.h"

struct FWBGameStateData;

enum class EWBCardZoneMutationResultCode : uint8
{
	Success,
	InvalidPlayer,
	PlayerZonesMissing,
	CardInstanceMissing,
	CardNotInExpectedZone,
	DuplicateInstanceId,
	OwnerMismatch,
	CardIdentityMismatch,
	InvalidZoneState,
	UnsupportedSourceZone,
	UnsupportedDestinationZone,
	SameZoneTransferUnsupported,
	InvalidDestinationPlacement
};

enum class EWBOrderedZonePlacement : uint8
{
	Unknown,
	Append
};

struct WANDBOUNDCORE_API FWBCardZoneTransferRequest
{
	int32 PlayerId = -1;
	EWBCardZone SourceZone = EWBCardZone::Unknown;
	EWBCardZone DestinationZone = EWBCardZone::Unknown;
	FString CardInstanceId;
	FString ExpectedCardId;
	EWBOrderedZonePlacement DestinationPlacement =
		EWBOrderedZonePlacement::Append;
};

struct WANDBOUNDCORE_API FWBCardZoneExtractRequest
{
	int32 PlayerId = -1;
	EWBCardZone SourceZone = EWBCardZone::Unknown;
	FString CardInstanceId;
	FString ExpectedCardId;
};

struct WANDBOUNDCORE_API FWBCardZoneMutationResult
{
	bool bOk = false;
	EWBCardZoneMutationResultCode Code =
		EWBCardZoneMutationResultCode::InvalidZoneState;
	FString Reason;
	int32 PlayerId = -1;
	EWBCardZone SourceZone = EWBCardZone::Unknown;
	EWBCardZone DestinationZone = EWBCardZone::Unknown;
	FWBCardInstanceRef Card;
	int32 SourceZoneCountAfter = 0;
	int32 DestinationZoneCountAfter = 0;
};

class WANDBOUNDCORE_API WBCardZoneMutation
{
public:
	static FWBCardZoneMutationResult TransferExact(
		FWBGameStateData& State,
		const FWBCardZoneTransferRequest& Request);

	static FWBCardZoneMutationResult ExtractExact(
		FWBGameStateData& State,
		const FWBCardZoneExtractRequest& Request);

	static FString ResultCodeToString(EWBCardZoneMutationResultCode Code);
};
