#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBCardActivationCommand.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBEquipExecution.h"
#include "WBMarkerResolution.h"
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayTrace.h"
#include "WBSummonExecution.h"

enum class EWBMatchLoopPhase : uint8
{
	Uninitialized,
	Setup,
	TurnStart,
	Action,
	Response,
	TurnEnd,
	NPCPhase,
	GameOver
};

enum class EWBMatchActionFamily : uint8
{
	CoreAction,
	Summon,
	Equip,
	Activation,
	Discard
};

struct WANDBOUNDCORE_API FWBMatchPlayerSetup
{
	int32 PlayerId = -1;
	FString HeroInstanceId;
	FString HeroCardId;
	TArray<FWBCardInstanceRef> OrderedDeck;
};

struct WANDBOUNDCORE_API FWBMatchInitializationRequest
{
	int32 Seed = 1;
	int32 FirstPlayerId = INDEX_NONE;
	FWBCardDefinitionRepository Repository;
	TArray<FWBMatchPlayerSetup> Players;
	TArray<FWBSetupMarkerPlacement> MarkerPlacements;
};

struct WANDBOUNDCORE_API FWBMatchLegalAction
{
	EWBMatchActionFamily Family = EWBMatchActionFamily::CoreAction;
	FString ActionId;
	int32 PlayerId = -1;
	FWBAction CoreAction;
	FWBSummonExecutionRequest SummonRequest;
	FWBEquipExecutionRequest EquipRequest;
	FWBCardActivationCommand ActivationCommand;
	FString DiscardCardInstanceId;
};

struct WANDBOUNDCORE_API FWBMatchLegalActionGenerationResult
{
	bool bOk = false;
	FString Reason;
	TArray<FWBMatchLegalAction> Actions;
};

struct WANDBOUNDCORE_API FWBMatchOperationResult
{
	bool bOk = false;
	FString Reason;
	FString SubmittedActionId;
	TArray<FWBTraceEvent> TraceEvents;
	TArray<FWBMatchLegalAction> NextLegalActions;
	bool bGameOver = false;
	int32 WinnerPlayerId = -1;
};

struct WANDBOUNDCORE_API FWBMatchObservation
{
	int32 ViewerPlayerId = -1;
	EWBMatchLoopPhase MatchPhase = EWBMatchLoopPhase::Uninitialized;
	FWBPublicTurnSummary PublicTurn;
	FWBPublicBoardSummary PublicBoard;
	FWBCardZonePlayerObservation CardZones;
	TArray<FWBMatchLegalAction> LegalActions;
};

class WANDBOUNDCORE_API WBMatchCoordinator
{
public:
	FWBMatchOperationResult InitializeMatch(const FWBMatchInitializationRequest& Request);
	FWBMatchLegalActionGenerationResult EnumerateLegalActions() const;
	FWBMatchOperationResult SubmitActionId(int32 PlayerId, const FString& ActionId);
	FWBMatchObservation BuildObservation(int32 ViewerPlayerId) const;

	bool IsInitialized() const;
	EWBMatchLoopPhase GetMatchPhase() const;
	FName GetMatchPhaseName() const;
	int32 GetFirstPlayerId() const;
	const FWBGameStateData& GetState() const;
	const FWBCardDefinitionRepository& GetRepository() const;
	const TArray<FWBTraceEvent>& GetTraceLog() const;
	FWBGameStateData& GetMutableStateForTest();

private:
	FWBMatchLegalActionGenerationResult EnumerateLegalActionsForState(
		const FWBGameStateData& InState,
		EWBMatchLoopPhase InPhase) const;

	bool ApplyAutomaticResolution(
		FWBGameStateData& WorkingState,
		TArray<FWBTraceEvent>& OutTraceEvents,
		FString& OutReason) const;

	bool ApplyTurnTransition(
		FWBGameStateData& WorkingState,
		uint32& WorkingRandomState,
		EWBMatchLoopPhase& WorkingPhase,
		TArray<FWBTraceEvent>& OutTraceEvents,
		FString& OutReason) const;

	static int32 RollD6(uint32& InOutRandomState);
	static FName PhaseToName(EWBMatchLoopPhase Phase);

	bool bInitialized = false;
	int32 FirstPlayerId = INDEX_NONE;
	uint32 RandomState = 0;
	EWBMatchLoopPhase MatchPhase = EWBMatchLoopPhase::Uninitialized;
	FWBGameStateData State;
	FWBCardDefinitionRepository Repository;
	TArray<FWBTraceEvent> TraceLog;
};
