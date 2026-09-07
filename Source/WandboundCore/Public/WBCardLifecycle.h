#pragma once

#include "CoreMinimal.h"
#include "WBCardZoneTransition.h"
#include "WBGameStateData.h"

enum class EWBCardLifecycleResultCode : uint8
{
	Success,
	GameStateMissing,
	InvalidPlayer,
	PlayerZonesMissing,
	DeckEmpty,
	CardInstanceMissing,
	CardNotInExpectedZone,
	DuplicateInstanceId,
	InvalidZoneState,
	UnsupportedLifecycleOperation,
	FirstPlayerFirstTurnDrawSkipped,
	TransitionSnapshotInvalid
};

struct WANDBOUNDCORE_API FWBCardLifecycleResult
{
	bool bOk = false;
	EWBCardLifecycleResultCode Code = EWBCardLifecycleResultCode::UnsupportedLifecycleOperation;
	FString Reason;

	int32 PlayerId = -1;
	FString CardInstanceId;
	FString CardId;

	int32 SourceZoneCountAfter = 0;
	int32 DestinationZoneCountAfter = 0;
	TArray<FWBCardZoneTransitionSnapshot> TransitionEvents;
};

class WANDBOUNDCORE_API WBCardLifecycle
{
public:
	static FWBCardLifecycleResult DrawOneCard(
		FWBGameStateData& State,
		int32 PlayerId,
		FWBCardZoneTransitionContext Context =
			FWBCardZoneTransitionContext());

	static FWBCardLifecycleResult DrawCards(
		FWBGameStateData& State,
		int32 PlayerId,
		int32 Count,
		FWBCardZoneTransitionContext Context =
			FWBCardZoneTransitionContext());

	static FWBCardLifecycleResult MoveHandCardToDiscard(
		FWBGameStateData& State,
		int32 PlayerId,
		const FString& CardInstanceId,
		FWBCardZoneTransitionContext Context =
			FWBCardZoneTransitionContext());

	static FWBCardLifecycleResult TransferExactCard(
		FWBGameStateData& State,
		const FWBCardZoneTransferRequest& Request,
		FWBCardZoneTransitionContext Context =
			FWBCardZoneTransitionContext());

	static FWBCardLifecycleResult MoveEquippedCardToDiscard(
		FWBGameStateData& State,
		int32 PlayerId,
		const FString& CardInstanceId);

	static FWBCardLifecycleResult RemoveExactCardFromDeck(
		FWBGameStateData& State,
		int32 PlayerId,
		const FString& CardInstanceId);

	static FWBCardLifecycleResult RemoveExactCardFromHand(
		FWBGameStateData& State,
		int32 PlayerId,
		const FString& CardInstanceId);

	static FWBCardLifecycleResult ApplySetupDraw(FWBGameStateData& State, int32 PlayerId, int32 Count);

	static FWBCardLifecycleResult ApplyTurnStartDraw(
		FWBGameStateData& State,
		int32 ActivePlayerId,
		int32 TurnNumber,
		int32 FirstPlayerId,
		FWBCardZoneTransitionContext Context =
			FWBCardZoneTransitionContext());

	static FString ResultCodeToString(EWBCardLifecycleResultCode Code);
};
