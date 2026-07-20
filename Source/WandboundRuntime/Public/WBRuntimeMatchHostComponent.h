#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBMatchCoordinator.h"
#include "WBRuntimeMatchPresentation.h"
#include "WBRuntimeMatchHostComponent.generated.h"

class AWBBoardViewActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWBRuntimeMatchEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWBRuntimeUnitPresentationEvent, int32, UnitId);

UCLASS(ClassGroup = (Wandbound), meta = (BlueprintSpawnableComponent))
class WANDBOUNDRUNTIME_API UWBRuntimeMatchHostComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeMatchHostComponent();
	virtual ~UWBRuntimeMatchHostComponent() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Match")
	TObjectPtr<AWBBoardViewActor> BoardActor;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnMatchInitialized;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnMatchInitializationFailed;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnObservationRefreshed;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnDecisionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnActionSubmitted;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnActionResolved;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnActionRejected;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnBoardPresentationRefreshed;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeUnitPresentationEvent OnUnitPresentationAdded;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeUnitPresentationEvent OnUnitPresentationUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeUnitPresentationEvent OnUnitPresentationRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnNPCPhaseResolved;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Match")
	FWBRuntimeMatchEvent OnGameOver;

	FWBRuntimeMatchCommandResult InitializeMatch(
		const FWBMatchInitializationRequest& Request,
		int32 InitialViewerPlayerId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match", meta = (DevelopmentOnly))
	FWBRuntimeMatchCommandResult InitializeDevelopmentMatch(
		int32 InitialViewerPlayerId = 0,
		bool bFragileFirstHero = false);

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	bool IsMatchInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	int32 GetCurrentViewerPlayerId() const;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SetCurrentViewerPlayerId(int32 ViewerPlayerId);

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	FWBRuntimeMatchPresentation GetCurrentPresentation() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	TArray<FWBRuntimeLegalActionPresentation> GetCurrentLegalActions() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	TArray<FWBRuntimeHandCardPresentation> GetCurrentHandCards() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	FWBRuntimeSelectionPresentation GetCurrentSelection() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	TArray<FWBRuntimeLegalActionPresentation> GetActionsForCurrentSelection() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	TArray<FWBRuntimeUnitPresentation> GetCurrentUnits() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	TArray<FWBRuntimeBoardTilePresentation> GetCurrentTiles() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	TArray<FString> GetSelectableCardInstanceIds() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	TArray<int32> GetSelectableUnitIds() const;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SelectUnit(int32 UnitId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SelectCardInstance(const FString& CardInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SelectTile(FIntPoint Tile);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SelectUnitTarget(int32 UnitId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SelectActionFamily(EWBRuntimeMatchActionFamily Family);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult ChooseActionCandidate(const FString& ActionId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	void ClearSelection();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SubmitSelectedAction();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SubmitLegalActionById(const FString& ActionId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult SubmitLegalActionAtRevision(
		const FString& ActionId,
		int32 ExpectedMatchGeneration,
		int32 ExpectedDecisionRevision);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult EndTurn();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Match")
	FWBRuntimeMatchCommandResult RefreshPresentation();

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	bool IsGameOver() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Match")
	int32 GetWinnerPlayerId() const;

	const FWBMatchObservation& GetCurrentObservation() const;
	const FWBMatchOperationResult& GetLatestOperationResult() const;
	const WBMatchCoordinator* GetCoordinatorForInspection() const;

private:
	TUniquePtr<WBMatchCoordinator> Coordinator;
	FWBMatchInitializationRequest InitializationRequest;
	FWBMatchObservation CurrentObservation;
	FWBMatchOperationResult LatestOperationResult;
	TArray<FWBMatchLegalAction> CurrentLegalDecisions;
	TSet<FString> HeroDefinitionIds;

	UPROPERTY(Transient)
	FWBRuntimeMatchPresentation CurrentPresentation;

	UPROPERTY(Transient)
	TArray<FWBRuntimeLegalActionPresentation> LegalActionPresentations;

	UPROPERTY(Transient)
	TArray<FWBRuntimeHandCardPresentation> HandCardPresentations;

	UPROPERTY(Transient)
	TArray<FWBRuntimeUnitPresentation> UnitPresentations;

	UPROPERTY(Transient)
	TArray<FWBRuntimeBoardTilePresentation> TilePresentations;

	int32 CurrentViewerPlayerId = -1;
	int32 MatchGeneration = 0;
	int32 DecisionRevision = 0;
	int32 PresentationRevision = 0;
	int32 TerminalBroadcastGeneration = INDEX_NONE;
	int32 SelectedUnitId = -1;
	FString SelectedCardInstanceId;
	FString PendingSelectedActionId;
	bool bHasSelectedActionFamily = false;
	EWBRuntimeMatchActionFamily SelectedActionFamily = EWBRuntimeMatchActionFamily::CoreAction;
	TArray<FString> AmbiguousActionIds;
	FString SelectionStatusReason;

	FWBRuntimeMatchCommandResult RefreshFromCoordinator(const FString& StatusMessage);
	FWBRuntimeMatchCommandResult MakeResult(bool bOk, const FString& Reason, const FString& ActionId = FString()) const;
	void RebuildPresentationModels(const FString& StatusMessage);
	void RebuildHandPresentations();
	void RebuildHighlights();
	void SynchronizeBoardActor();
	const FWBMatchLegalAction* FindCurrentAction(const FString& ActionId, int32& OutMatchCount) const;
	FIntPoint TargetTileForAction(const FWBMatchLegalAction& Action) const;
	bool ActionMatchesCurrentSelection(const FWBMatchLegalAction& Action) const;
	void ClearSelectionInternal(bool bBroadcast);
};
