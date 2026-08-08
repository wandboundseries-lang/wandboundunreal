#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBCardActivationCommand.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBEquipExecution.h"
#include "WBInitialHeroSetup.h"
#include "WBHybridSummon.h"
#include "WBMarkerResolution.h"
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayTrace.h"
#include "WBProductionMatchReplay.h"
#include "WBSummonExecution.h"
#include "WBTurnStartSequence.h"

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
	Discard,
	TurnStartTrigger,
	Count
};

struct WANDBOUNDCORE_API FWBMatchPlayerSetup
{
	int32 PlayerId = -1;
	FString HeroInstanceId;
	FString HeroCardId;
	FWBTile HeroSpawnTile = FWBTile(-1, -1);
	TArray<FWBCardInstanceRef> OrderedDeck;
};

struct WANDBOUNDCORE_API FWBMatchInitializationRequest
{
	int32 Seed = 1;
	int32 FirstPlayerId = INDEX_NONE;
	int32 ExpectedFirstPlayerId = INDEX_NONE;
	bool bDeriveFirstPlayerFromSeed = false;
	bool bShuffleDecksAtMatchStart = false;
	FWBCardDefinitionRepository Repository;
	TArray<FWBMatchPlayerSetup> Players;
	TArray<FWBSetupMarkerPlacement> MarkerPlacements;
	TMap<int32, TArray<FString>> SetupTriggerOrderChoices;
};

struct WANDBOUNDCORE_API FWBMatchLegalAction
{
	EWBMatchActionFamily Family = EWBMatchActionFamily::CoreAction;
	FString ActionId;
	int32 PlayerId = -1;
	FWBAction CoreAction;
	FWBSummonExecutionRequest SummonRequest;
	bool bHybridSummon = false;
	bool bHybridHeroReplacement = false;
	FWBHybridSummonPlan HybridSummonPlan;
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
	bool bCompleted = false;
	bool bTerminal = false;
	bool bPendingDecision = false;
	int32 PendingPlayerId = -1;
	int32 ActivePlayerId = -1;
	int32 TurnNumber = 0;
	int32 TraceBeginIndex = 0;
	int32 TraceEndIndex = 0;
	bool bGameOver = false;
	int32 WinnerPlayerId = -1;
	int32 LoserPlayerId = -1;
	FName TerminalReason;
	FName TerminalSource;
	int32 TerminalTurnNumber = -1;
	int32 TerminalRevision = -1;
	int32 TerminalTraceIndex = -1;
	int32 CoordinatorGeneration = 0;
	int32 CoordinatorRevision = 0;
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
	bool WasHeroSpawnBatchCommitted() const;
	bool WereHeroSetupTriggersResolved() const;
	bool WereOpeningHandsDrawn() const;
	bool WasTurnStartCompleted() const;
	bool IsTurnTransitionInProgress() const;
	bool HasPendingTurnStartDecision() const;
	int32 GetPendingTurnStartDecisionPlayerId() const;
	const FWBTurnStartSequenceState& GetTurnStartSequenceState() const;
	const FWBGameStateData& GetState() const;
	const FWBCardDefinitionRepository& GetRepository() const;
	const TArray<FWBTraceEvent>& GetTraceLog() const;
	int32 GetCoordinatorGeneration() const;
	int32 GetCoordinatorRevision() const;
	const FString& GetInitialStateDigest() const;
	const FString& GetInitialTraceDigest() const;
	FString GetCurrentStateDigest() const;
	FString GetCurrentTraceDigest() const;
	const TArray<FWBMatchCommittedActionRecord>& GetCommittedActionRecords() const;
	static bool ClassifyReplayActionFamily(
		const FWBMatchLegalAction& Action,
		FString& OutFamily);
	FWBGameStateData& GetMutableStateForTest();

	// Compatibility-only bridge for old fixtures and callers that still own raw
	// state. Production turn transitions must use SubmitActionId.
	static FWBApplyActionResult ApplyLegacyCompatibilityTurnTransition(
		FWBGameStateData& InOutState,
		int32 EndingPlayerId,
		int32 NextPlayerExplicitMPRoll);

private:
	FWBMatchLegalActionGenerationResult EnumerateLegalActionsForState(
		const FWBGameStateData& InState,
		EWBMatchLoopPhase InPhase,
		const FWBTurnStartSequenceState* InTurnStartSequence = nullptr) const;

	bool ApplyAutomaticResolution(
		FWBGameStateData& WorkingState,
		TArray<FWBTraceEvent>& OutTraceEvents,
		FString& OutReason) const;

	bool ApplyTurnTransition(
		FWBGameStateData& WorkingState,
		uint32& WorkingRandomState,
		EWBMatchLoopPhase& WorkingPhase,
		FWBTurnStartSequenceState& WorkingTurnStartSequence,
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
	bool bHeroSpawnBatchCommitted = false;
	bool bHeroSetupTriggersResolved = false;
	bool bOpeningHandsDrawn = false;
	FWBTurnStartSequenceState TurnStartSequence;
	int32 CoordinatorGeneration = 0;
	int32 CoordinatorRevision = 0;
	FString InitialStateDigest;
	FString InitialTraceDigest;
	TArray<FWBMatchCommittedActionRecord> CommittedActionRecords;
};
