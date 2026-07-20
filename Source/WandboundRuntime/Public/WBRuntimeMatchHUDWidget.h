#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "WBRuntimeMatchPresentation.h"
#include "WBRuntimeMatchHUDWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UPanelWidget;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWBRuntimeMatchHostComponent;
class UWBRuntimeMatchHUDWidget;

UENUM()
enum class EWBRuntimeHUDOptionKind : uint8
{
	HandCard,
	ActionFamily,
	ActionCandidate
};

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeHUDOptionButton : public UButton
{
	GENERATED_BODY()

public:
	void InitializeOption(
		UWBRuntimeMatchHUDWidget* InOwner,
		EWBRuntimeHUDOptionKind InKind,
		const FString& InStableId,
		EWBRuntimeMatchActionFamily InFamily,
		const FString& InLabel);
	void ClearOptionData();

private:
	UFUNCTION()
	void HandlePressed();

	UPROPERTY(Transient)
	TObjectPtr<UWBRuntimeMatchHUDWidget> HUDOwner;

	EWBRuntimeHUDOptionKind OptionKind = EWBRuntimeHUDOptionKind::HandCard;
	EWBRuntimeMatchActionFamily Family = EWBRuntimeMatchActionFamily::CoreAction;
	FString StableId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWBRuntimeHUDEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWBRuntimeHUDCommandEvent, FWBRuntimeMatchCommandResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWBRuntimeHUDStringEvent, const FString&, StableId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWBRuntimeHUDUnitEvent, int32, UnitId);

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeMatchHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	bool BindToMatchHost(UWBRuntimeMatchHostComponent* InHost);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	void UnbindFromMatchHost();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	void RefreshFromHost();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult SelectHandCard(const FString& CardInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult SelectUnitSource(int32 UnitId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult SelectTileTarget(FIntPoint Tile);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult SelectUnitTarget(int32 UnitId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult SelectActionFamily(EWBRuntimeMatchActionFamily Family);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult ChooseAmbiguousAction(const FString& ActionId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult SubmitResolvedAction();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult SubmitActionOption(const FWBRuntimeLegalActionPresentation& Action);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult RequestEndTurn();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult RequestViewerSwitch();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|HUD")
	void ClearSelection();

	void EnsureWidgetTreeBuilt();
	void HandleOptionPressed(EWBRuntimeHUDOptionKind Kind, const FString& StableId, EWBRuntimeMatchActionFamily Family);

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	UWBRuntimeMatchHostComponent* GetBoundMatchHost() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	TArray<FWBRuntimeHandCardPresentation> GetDisplayedHand() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	TArray<FWBRuntimeLegalActionPresentation> GetDisplayedActions() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	FWBRuntimeSelectionPresentation GetDisplayedSelection() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	FWBRuntimeMatchCommandResult GetLastCommandResult() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	bool IsTerminalOverlayVisible() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	bool AreRequiredRegionsBuilt() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	int32 GetDisplayedHandWidgetCount() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	int32 GetAmbiguityWidgetCount() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|HUD")
	FString GetFeedbackText() const;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnHUDBound;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnHandRefreshed;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDStringEvent OnCardSelected;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDUnitEvent OnUnitSelected;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnTargetHighlightsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnAmbiguityOpened;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnAmbiguityResolved;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnActionSubmitted;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDCommandEvent OnActionAccepted;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDCommandEvent OnActionRejected;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDCommandEvent OnStaleDecisionDetected;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnViewerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|HUD")
	FWBRuntimeHUDEvent OnTerminalShown;

	bool bAllowDevelopmentViewerSwitch = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWBRuntimeMatchHostComponent> MatchHost;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MatchHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> HandPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectionText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ActionFamilyPanel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> AmbiguityPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TerminalOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TerminalText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SubmitButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ClearButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ViewerSwitchButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EndTurnButton;

	UPROPERTY(Transient)
	TArray<FWBRuntimeHandCardPresentation> DisplayedHand;

	UPROPERTY(Transient)
	TArray<FWBRuntimeLegalActionPresentation> DisplayedActions;

	UPROPERTY(Transient)
	FWBRuntimeSelectionPresentation DisplayedSelection;

	UPROPERTY(Transient)
	FWBRuntimeMatchCommandResult LastCommandResult;

	int32 TerminalShownGeneration = INDEX_NONE;

	UFUNCTION()
	void HandleHostRefresh();

	UFUNCTION()
	void HandleHostActionRejected();

	UFUNCTION()
	void HandleClearPressed();

	UFUNCTION()
	void HandleSubmitPressed();

	UFUNCTION()
	void HandleEndTurnPressed();

	UFUNCTION()
	void HandleRefreshPressed();

	UFUNCTION()
	void HandleViewerSwitchPressed();

	void BuildStaticLayout();
	void RebuildHandWidgets();
	void RebuildActionWidgets();
	void RebuildTerminalState(const FWBRuntimeMatchPresentation& Presentation);
	void ApplyCommandResult(const FWBRuntimeMatchCommandResult& Result, bool bSubmission);
	void SanitizeDynamicPanel(UPanelWidget* Panel);
	UWBRuntimeHUDOptionButton* AddOptionButton(
		UPanelWidget* Panel,
		EWBRuntimeHUDOptionKind Kind,
		const FString& StableId,
		EWBRuntimeMatchActionFamily Family,
		const FString& Label);
};
