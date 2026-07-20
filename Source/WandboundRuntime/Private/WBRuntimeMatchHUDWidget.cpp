#include "WBRuntimeMatchHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "WBRuntimeMatchHostComponent.h"

namespace
{
FString FamilyLabel(const EWBRuntimeMatchActionFamily Family)
{
	switch (Family)
	{
	case EWBRuntimeMatchActionFamily::Move: return TEXT("Move");
	case EWBRuntimeMatchActionFamily::Attack: return TEXT("Attack");
	case EWBRuntimeMatchActionFamily::Pass: return TEXT("Pass");
	case EWBRuntimeMatchActionFamily::PassResponse: return TEXT("Pass Response");
	case EWBRuntimeMatchActionFamily::EndTurn: return TEXT("End Turn");
	case EWBRuntimeMatchActionFamily::Summon: return TEXT("Summon");
	case EWBRuntimeMatchActionFamily::Equip: return TEXT("Equip");
	case EWBRuntimeMatchActionFamily::Activation: return TEXT("Activation");
	case EWBRuntimeMatchActionFamily::Discard: return TEXT("Discard");
	default: return TEXT("Action");
	}
}

FString HandCardLabel(const FWBRuntimeHandCardPresentation& Card)
{
	FString Label = FString::Printf(TEXT("%s  [%s]"), *Card.DisplayName, *Card.CardType.ToString());
	if (Card.RR > 0) Label += FString::Printf(TEXT("  RR %d"), Card.RR);
	if (Card.MPCost > 0) Label += FString::Printf(TEXT("  MP %d"), Card.MPCost);
	if (Card.bSelected) Label = TEXT("> ") + Label;
	return Label;
}
}

void UWBRuntimeHUDOptionButton::InitializeOption(
	UWBRuntimeMatchHUDWidget* InOwner,
	const EWBRuntimeHUDOptionKind InKind,
	const FString& InStableId,
	const EWBRuntimeMatchActionFamily InFamily,
	const FString& InLabel)
{
	HUDOwner = InOwner;
	OptionKind = InKind;
	StableId = InStableId;
	Family = InFamily;
	OnClicked.RemoveAll(this);
	OnClicked.AddDynamic(this, &UWBRuntimeHUDOptionButton::HandlePressed);
	UTextBlock* Label = NewObject<UTextBlock>(this);
	Label->SetText(FText::FromString(InLabel));
	SetContent(Label);
}

void UWBRuntimeHUDOptionButton::ClearOptionData()
{
	OnClicked.RemoveAll(this);
	HUDOwner = nullptr;
	StableId.Reset();
	SetContent(nullptr);
}

void UWBRuntimeHUDOptionButton::HandlePressed()
{
	if (HUDOwner != nullptr) HUDOwner->HandleOptionPressed(OptionKind, StableId, Family);
}

void UWBRuntimeMatchHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTreeBuilt();
	RefreshFromHost();
}

void UWBRuntimeMatchHUDWidget::NativeDestruct()
{
	UnbindFromMatchHost();
	Super::NativeDestruct();
}

bool UWBRuntimeMatchHUDWidget::BindToMatchHost(UWBRuntimeMatchHostComponent* InHost)
{
	EnsureWidgetTreeBuilt();
	if (InHost == nullptr) return false;
	UnbindFromMatchHost();
	MatchHost = InHost;
	MatchHost->OnDecisionChanged.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleHostRefresh);
	MatchHost->OnSelectionChanged.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleHostRefresh);
	MatchHost->OnActionRejected.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleHostActionRejected);
	RefreshFromHost();
	OnHUDBound.Broadcast();
	return true;
}

void UWBRuntimeMatchHUDWidget::UnbindFromMatchHost()
{
	if (MatchHost != nullptr)
	{
		MatchHost->OnDecisionChanged.RemoveAll(this);
		MatchHost->OnSelectionChanged.RemoveAll(this);
		MatchHost->OnActionRejected.RemoveAll(this);
	}
	MatchHost = nullptr;
	DisplayedHand.Reset();
	DisplayedActions.Reset();
	DisplayedSelection = FWBRuntimeSelectionPresentation();
	SanitizeDynamicPanel(HandPanel);
	SanitizeDynamicPanel(ActionFamilyPanel);
	SanitizeDynamicPanel(AmbiguityPanel);
}

void UWBRuntimeMatchHUDWidget::RefreshFromHost()
{
	EnsureWidgetTreeBuilt();
	if (MatchHost == nullptr || !MatchHost->IsMatchInitialized())
	{
		DisplayedHand.Reset();
		DisplayedActions.Reset();
		DisplayedSelection = FWBRuntimeSelectionPresentation();
		SanitizeDynamicPanel(HandPanel);
		SanitizeDynamicPanel(ActionFamilyPanel);
		SanitizeDynamicPanel(AmbiguityPanel);
		MatchHeaderText->SetText(FText::FromString(TEXT("No match")));
		return;
	}

	const FWBRuntimeMatchPresentation Presentation = MatchHost->GetCurrentPresentation();
	DisplayedHand = MatchHost->GetCurrentHandCards();
	DisplayedActions = MatchHost->GetActionsForCurrentSelection();
	DisplayedSelection = MatchHost->GetCurrentSelection();
	MatchHeaderText->SetText(FText::FromString(FString::Printf(
		TEXT("Turn %d  Phase %s  Current P%d  Viewer P%d  MP %d"),
		Presentation.TurnNumber,
		*Presentation.Phase.ToString(),
		Presentation.CurrentPlayerId,
		Presentation.ViewerPlayerId,
		Presentation.RemainingMP)));

	SelectionText->SetText(FText::FromString(FString::Printf(
		TEXT("Unit: %d  Card: %s  Status: %s"),
		DisplayedSelection.SelectedUnitId,
		DisplayedSelection.SelectedCardInstanceId.IsEmpty() ? TEXT("none") : *DisplayedSelection.SelectedCardInstanceId,
		DisplayedSelection.StatusReason.IsEmpty() ? TEXT("ready") : *DisplayedSelection.StatusReason)));
	RebuildHandWidgets();
	RebuildActionWidgets();
	RebuildTerminalState(Presentation);
	SubmitButton->SetIsEnabled(!Presentation.bGameOver && !DisplayedSelection.ResolvedActionId.IsEmpty());
	EndTurnButton->SetIsEnabled(!Presentation.bGameOver && MatchHost->GetCurrentLegalActions().ContainsByPredicate([](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.Family == EWBRuntimeMatchActionFamily::EndTurn;
	}));
	OnHandRefreshed.Broadcast();
	OnTargetHighlightsChanged.Broadcast();
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::SelectHandCard(const FString& CardInstanceId)
{
	const FWBRuntimeMatchCommandResult Result = MatchHost != nullptr
		? MatchHost->SelectCardInstance(CardInstanceId)
		: FWBRuntimeMatchCommandResult();
	ApplyCommandResult(Result, false);
	if (Result.bOk) OnCardSelected.Broadcast(CardInstanceId);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::SelectUnitSource(const int32 UnitId)
{
	const FWBRuntimeMatchCommandResult Result = MatchHost != nullptr
		? MatchHost->SelectUnit(UnitId)
		: FWBRuntimeMatchCommandResult();
	ApplyCommandResult(Result, false);
	if (Result.bOk) OnUnitSelected.Broadcast(UnitId);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::SelectTileTarget(const FIntPoint Tile)
{
	const FWBRuntimeMatchCommandResult Result = MatchHost != nullptr
		? MatchHost->SelectTile(Tile)
		: FWBRuntimeMatchCommandResult();
	ApplyCommandResult(Result, false);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::SelectUnitTarget(const int32 UnitId)
{
	const FWBRuntimeMatchCommandResult Result = MatchHost != nullptr
		? MatchHost->SelectUnitTarget(UnitId)
		: FWBRuntimeMatchCommandResult();
	ApplyCommandResult(Result, false);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::SelectActionFamily(const EWBRuntimeMatchActionFamily Family)
{
	const FWBRuntimeMatchCommandResult Result = MatchHost != nullptr
		? MatchHost->SelectActionFamily(Family)
		: FWBRuntimeMatchCommandResult();
	ApplyCommandResult(Result, false);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::ChooseAmbiguousAction(const FString& ActionId)
{
	const FWBRuntimeMatchCommandResult Result = MatchHost != nullptr
		? MatchHost->ChooseActionCandidate(ActionId)
		: FWBRuntimeMatchCommandResult();
	ApplyCommandResult(Result, false);
	if (Result.bOk) OnAmbiguityResolved.Broadcast();
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::SubmitResolvedAction()
{
	if (MatchHost == nullptr)
	{
		FWBRuntimeMatchCommandResult Missing;
		Missing.Reason = TEXT("match_host_missing");
		return Missing;
	}
	const FWBRuntimeSelectionPresentation Selection = MatchHost->GetCurrentSelection();
	FWBRuntimeMatchCommandResult Result;
	if (!Selection.ResolvedActionId.IsEmpty())
	{
		const FWBRuntimeMatchPresentation Presentation = MatchHost->GetCurrentPresentation();
		OnActionSubmitted.Broadcast();
		Result = MatchHost->SubmitLegalActionAtRevision(
			Selection.ResolvedActionId,
			Presentation.MatchGeneration,
			Presentation.DecisionRevision);
	}
	else
	{
		Result.Reason = TEXT("selected_action_missing");
	}
	ApplyCommandResult(Result, true);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::SubmitActionOption(
	const FWBRuntimeLegalActionPresentation& Action)
{
	FWBRuntimeMatchCommandResult Result;
	if (MatchHost == nullptr) Result.Reason = TEXT("match_host_missing");
	else
	{
		OnActionSubmitted.Broadcast();
		Result = MatchHost->SubmitLegalActionAtRevision(
			Action.ActionId,
			Action.MatchGeneration,
			Action.DecisionRevision);
	}
	ApplyCommandResult(Result, true);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::RequestEndTurn()
{
	const FWBRuntimeMatchCommandResult Result = MatchHost != nullptr
		? MatchHost->EndTurn()
		: FWBRuntimeMatchCommandResult();
	ApplyCommandResult(Result, true);
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::RequestViewerSwitch()
{
	FWBRuntimeMatchCommandResult Result;
	if (MatchHost == nullptr) Result.Reason = TEXT("match_host_missing");
	else if (!bAllowDevelopmentViewerSwitch) Result.Reason = TEXT("viewer_switch_disabled");
	else
	{
		Result = MatchHost->SetCurrentViewerPlayerId(MatchHost->GetCurrentViewerPlayerId() == 0 ? 1 : 0);
		if (Result.bOk) OnViewerChanged.Broadcast();
	}
	ApplyCommandResult(Result, false);
	return Result;
}

void UWBRuntimeMatchHUDWidget::ClearSelection()
{
	if (MatchHost != nullptr) MatchHost->ClearSelection();
	RefreshFromHost();
}

void UWBRuntimeMatchHUDWidget::EnsureWidgetTreeBuilt()
{
	if (WidgetTree == nullptr) WidgetTree = NewObject<UWidgetTree>(this, TEXT("RuntimeWidgetTree"));
	if (WidgetTree->RootWidget == nullptr) BuildStaticLayout();
}

void UWBRuntimeMatchHUDWidget::HandleOptionPressed(
	const EWBRuntimeHUDOptionKind Kind,
	const FString& StableId,
	const EWBRuntimeMatchActionFamily Family)
{
	switch (Kind)
	{
	case EWBRuntimeHUDOptionKind::HandCard: SelectHandCard(StableId); break;
	case EWBRuntimeHUDOptionKind::ActionFamily: SelectActionFamily(Family); break;
	case EWBRuntimeHUDOptionKind::ActionCandidate: ChooseAmbiguousAction(StableId); break;
	}
}

UWBRuntimeMatchHostComponent* UWBRuntimeMatchHUDWidget::GetBoundMatchHost() const { return MatchHost.Get(); }
TArray<FWBRuntimeHandCardPresentation> UWBRuntimeMatchHUDWidget::GetDisplayedHand() const { return DisplayedHand; }
TArray<FWBRuntimeLegalActionPresentation> UWBRuntimeMatchHUDWidget::GetDisplayedActions() const { return DisplayedActions; }
FWBRuntimeSelectionPresentation UWBRuntimeMatchHUDWidget::GetDisplayedSelection() const { return DisplayedSelection; }
FWBRuntimeMatchCommandResult UWBRuntimeMatchHUDWidget::GetLastCommandResult() const { return LastCommandResult; }
bool UWBRuntimeMatchHUDWidget::IsTerminalOverlayVisible() const { return TerminalOverlay != nullptr && TerminalOverlay->GetVisibility() == ESlateVisibility::Visible; }
bool UWBRuntimeMatchHUDWidget::AreRequiredRegionsBuilt() const { return MatchHeaderText && HandPanel && SelectionText && ActionFamilyPanel && AmbiguityPanel && FeedbackText && TerminalOverlay; }
int32 UWBRuntimeMatchHUDWidget::GetDisplayedHandWidgetCount() const { return HandPanel != nullptr ? HandPanel->GetChildrenCount() : 0; }
int32 UWBRuntimeMatchHUDWidget::GetAmbiguityWidgetCount() const { return AmbiguityPanel != nullptr ? AmbiguityPanel->GetChildrenCount() : 0; }
FString UWBRuntimeMatchHUDWidget::GetFeedbackText() const { return FeedbackText != nullptr ? FeedbackText->GetText().ToString() : FString(); }

void UWBRuntimeMatchHUDWidget::HandleHostRefresh() { RefreshFromHost(); }
void UWBRuntimeMatchHUDWidget::HandleHostActionRejected()
{
	if (FeedbackText != nullptr) FeedbackText->SetText(FText::FromString(TEXT("action_rejected")));
	RefreshFromHost();
}
void UWBRuntimeMatchHUDWidget::HandleClearPressed() { ClearSelection(); }
void UWBRuntimeMatchHUDWidget::HandleSubmitPressed() { SubmitResolvedAction(); }
void UWBRuntimeMatchHUDWidget::HandleEndTurnPressed() { RequestEndTurn(); }
void UWBRuntimeMatchHUDWidget::HandleRefreshPressed()
{
	if (MatchHost != nullptr) ApplyCommandResult(MatchHost->RefreshPresentation(), false);
}
void UWBRuntimeMatchHUDWidget::HandleViewerSwitchPressed() { RequestViewerSwitch(); }

void UWBRuntimeMatchHUDWidget::BuildStaticLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HUDRoot"));
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainHUD"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Main->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Root->AddChild(Main);
	WidgetTree->RootWidget = Root;

	MatchHeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MatchHeader"));
	Main->AddChild(MatchHeaderText);
	UTextBlock* HandTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HandTitle"));
	HandTitle->SetText(FText::FromString(TEXT("Hand")));
	Main->AddChild(HandTitle);
	HandPanel = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HandPanel"));
	HandPanel->SetOrientation(Orient_Horizontal);
	Main->AddChild(HandPanel);
	SelectionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectionStatus"));
	Main->AddChild(SelectionText);
	ActionFamilyPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ActionFamilies"));
	Main->AddChild(ActionFamilyPanel);
	AmbiguityPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AmbiguityCandidates"));
	Main->AddChild(AmbiguityPanel);
	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionFeedback"));
	Main->AddChild(FeedbackText);

	UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionControls"));
	Main->AddChild(Controls);
	auto CreateControl = [this, Controls](const TCHAR* Name, const TCHAR* Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = NewObject<UTextBlock>(Button);
		Text->SetText(FText::FromString(FString(Label)));
		Button->SetContent(Text);
		Controls->AddChild(Button);
		return Button;
	};
	ClearButton = CreateControl(TEXT("ClearButton"), TEXT("Clear"));
	SubmitButton = CreateControl(TEXT("SubmitButton"), TEXT("Submit"));
	EndTurnButton = CreateControl(TEXT("EndTurnButton"), TEXT("End Turn"));
	RefreshButton = CreateControl(TEXT("RefreshButton"), TEXT("Refresh"));
	ViewerSwitchButton = CreateControl(TEXT("ViewerButton"), TEXT("Switch Viewer"));
	ClearButton->OnClicked.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleClearPressed);
	SubmitButton->OnClicked.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleSubmitPressed);
	EndTurnButton->OnClicked.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleEndTurnPressed);
	RefreshButton->OnClicked.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleRefreshPressed);
	ViewerSwitchButton->OnClicked.AddDynamic(this, &UWBRuntimeMatchHUDWidget::HandleViewerSwitchPressed);

	TerminalOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TerminalOverlay"));
	TerminalText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TerminalText"));
	TerminalOverlay->SetContent(TerminalText);
	TerminalOverlay->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChild(TerminalOverlay);
}

void UWBRuntimeMatchHUDWidget::RebuildHandWidgets()
{
	SanitizeDynamicPanel(HandPanel);
	for (const FWBRuntimeHandCardPresentation& Card : DisplayedHand)
	{
		UWBRuntimeHUDOptionButton* Button = AddOptionButton(
			HandPanel,
			EWBRuntimeHUDOptionKind::HandCard,
			Card.CardInstanceId,
			EWBRuntimeMatchActionFamily::CoreAction,
			HandCardLabel(Card));
		Button->SetIsEnabled(Card.bSelectable);
	}
}

void UWBRuntimeMatchHUDWidget::RebuildActionWidgets()
{
	SanitizeDynamicPanel(ActionFamilyPanel);
	SanitizeDynamicPanel(AmbiguityPanel);
	TArray<EWBRuntimeMatchActionFamily> Families;
	for (const FWBRuntimeLegalActionPresentation& Action : DisplayedActions) Families.AddUnique(Action.Family);
	Families.Sort([](const EWBRuntimeMatchActionFamily A, const EWBRuntimeMatchActionFamily B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});
	for (const EWBRuntimeMatchActionFamily Family : Families)
	{
		AddOptionButton(ActionFamilyPanel, EWBRuntimeHUDOptionKind::ActionFamily, FString(), Family, FamilyLabel(Family));
	}
	for (const FString& ActionId : DisplayedSelection.AmbiguousActionIds)
	{
		const FWBRuntimeLegalActionPresentation* Action = MatchHost != nullptr
			? MatchHost->GetCurrentLegalActions().FindByPredicate([&ActionId](const FWBRuntimeLegalActionPresentation& Candidate)
			{
				return Candidate.ActionId == ActionId;
			})
			: nullptr;
		AddOptionButton(
			AmbiguityPanel,
			EWBRuntimeHUDOptionKind::ActionCandidate,
			ActionId,
			Action != nullptr ? Action->Family : EWBRuntimeMatchActionFamily::CoreAction,
			Action != nullptr ? Action->PublicLabel : TEXT("Choose Action"));
	}
	if (!DisplayedSelection.AmbiguousActionIds.IsEmpty()) OnAmbiguityOpened.Broadcast();
}

void UWBRuntimeMatchHUDWidget::RebuildTerminalState(const FWBRuntimeMatchPresentation& Presentation)
{
	if (!Presentation.bGameOver)
	{
		TerminalOverlay->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	TerminalText->SetText(FText::FromString(FString::Printf(
		TEXT("Match Over\nWinner: Player %d\nDefeated: Player %d\nReason: %s\nFinal Turn: %d"),
		Presentation.WinnerPlayerId,
		Presentation.DefeatedPlayerId,
		*Presentation.TerminalReason,
		Presentation.FinalTurnNumber)));
	TerminalOverlay->SetVisibility(ESlateVisibility::Visible);
	if (TerminalShownGeneration != Presentation.MatchGeneration)
	{
		TerminalShownGeneration = Presentation.MatchGeneration;
		OnTerminalShown.Broadcast();
	}
}

void UWBRuntimeMatchHUDWidget::ApplyCommandResult(const FWBRuntimeMatchCommandResult& Result, const bool bSubmission)
{
	LastCommandResult = Result;
	if (FeedbackText != nullptr) FeedbackText->SetText(FText::FromString(Result.Reason));
	const bool bStale = Result.Reason == TEXT("stale_match_generation") || Result.Reason == TEXT("stale_decision_revision");
	if (bStale)
	{
		OnStaleDecisionDetected.Broadcast(Result);
		if (MatchHost != nullptr) MatchHost->RefreshPresentation();
	}
	if (bSubmission)
	{
		if (Result.bOk) OnActionAccepted.Broadcast(Result);
		else OnActionRejected.Broadcast(Result);
	}
	RefreshFromHost();
}

void UWBRuntimeMatchHUDWidget::SanitizeDynamicPanel(UPanelWidget* Panel)
{
	if (Panel == nullptr) return;
	for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
	{
		if (UWBRuntimeHUDOptionButton* Option = Cast<UWBRuntimeHUDOptionButton>(Panel->GetChildAt(Index)))
		{
			Option->ClearOptionData();
		}
	}
	Panel->ClearChildren();
}

UWBRuntimeHUDOptionButton* UWBRuntimeMatchHUDWidget::AddOptionButton(
	UPanelWidget* Panel,
	const EWBRuntimeHUDOptionKind Kind,
	const FString& StableId,
	const EWBRuntimeMatchActionFamily Family,
	const FString& Label)
{
	UWBRuntimeHUDOptionButton* Button = NewObject<UWBRuntimeHUDOptionButton>(Panel);
	Button->InitializeOption(this, Kind, StableId, Family, Label);
	Panel->AddChild(Button);
	return Button;
}
