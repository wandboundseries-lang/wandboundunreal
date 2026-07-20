#include "WBRuntimePlayerController.h"

#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "WBBoardViewActor.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimeMatchHUDWidget.h"
#include "WBRuntimeUnitPresentationActor.h"

AWBRuntimePlayerController::AWBRuntimePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	HUDWidgetClass = UWBRuntimeMatchHUDWidget::StaticClass();
}

void AWBRuntimePlayerController::BeginPlay()
{
	Super::BeginPlay();
	ResolveRuntimeReferences();
	if (bCreateHUDOnBeginPlay && RuntimeHUD == nullptr && IsLocalController())
	{
		RuntimeHUD = CreateWidget<UWBRuntimeMatchHUDWidget>(this, HUDWidgetClass);
		if (RuntimeHUD != nullptr) RuntimeHUD->AddToViewport();
	}
	if (RuntimeHUD != nullptr)
	{
		RuntimeHUD->bAllowDevelopmentViewerSwitch = bEnableDevelopmentViewerSwitch;
		RuntimeHUD->BindToMatchHost(MatchHost);
	}
}

void AWBRuntimePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent == nullptr) return;
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AWBRuntimePlayerController::HandlePrimaryClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AWBRuntimePlayerController::HandleClearInput);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AWBRuntimePlayerController::HandleClearInput);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AWBRuntimePlayerController::HandleSubmitInput);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AWBRuntimePlayerController::HandleEndTurnInput);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AWBRuntimePlayerController::HandleViewerSwitchInput);
}

void AWBRuntimePlayerController::SetRuntimeReferences(
	UWBRuntimeMatchHostComponent* InMatchHost,
	AWBBoardViewActor* InBoardActor,
	UWBRuntimeMatchHUDWidget* InHUDWidget)
{
	MatchHost = InMatchHost;
	BoardActor = InBoardActor;
	RuntimeHUD = InHUDWidget;
	if (RuntimeHUD != nullptr)
	{
		RuntimeHUD->bAllowDevelopmentViewerSwitch = bEnableDevelopmentViewerSwitch;
		RuntimeHUD->BindToMatchHost(MatchHost);
	}
}

UWBRuntimeMatchHUDWidget* AWBRuntimePlayerController::GetRuntimeHUD() const { return RuntimeHUD; }

FWBRuntimeMatchCommandResult AWBRuntimePlayerController::ForwardTileSelection(const FIntPoint Tile)
{
	return RuntimeHUD != nullptr ? RuntimeHUD->SelectTileTarget(Tile) : FWBRuntimeMatchCommandResult();
}

FWBRuntimeMatchCommandResult AWBRuntimePlayerController::ForwardUnitSelection(const int32 UnitId)
{
	if (RuntimeHUD == nullptr) return FWBRuntimeMatchCommandResult();
	const FWBRuntimeSelectionPresentation Selection = RuntimeHUD->GetDisplayedSelection();
	return Selection.SelectedUnitId >= 0 || !Selection.SelectedCardInstanceId.IsEmpty()
		? RuntimeHUD->SelectUnitTarget(UnitId)
		: RuntimeHUD->SelectUnitSource(UnitId);
}

void AWBRuntimePlayerController::ForwardClearSelection()
{
	if (RuntimeHUD != nullptr) RuntimeHUD->ClearSelection();
}

FWBRuntimeMatchCommandResult AWBRuntimePlayerController::ForwardSubmit()
{
	return RuntimeHUD != nullptr ? RuntimeHUD->SubmitResolvedAction() : FWBRuntimeMatchCommandResult();
}

FWBRuntimeMatchCommandResult AWBRuntimePlayerController::ForwardEndTurn()
{
	return RuntimeHUD != nullptr ? RuntimeHUD->RequestEndTurn() : FWBRuntimeMatchCommandResult();
}

void AWBRuntimePlayerController::ResolveRuntimeReferences()
{
	UWorld* World = GetWorld();
	if (World == nullptr) return;
	if (BoardActor == nullptr)
	{
		for (TActorIterator<AWBBoardViewActor> It(World); It; ++It)
		{
			BoardActor = *It;
			break;
		}
	}
	if (MatchHost == nullptr)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UWBRuntimeMatchHostComponent* Found = It->FindComponentByClass<UWBRuntimeMatchHostComponent>())
			{
				MatchHost = Found;
				break;
			}
		}
	}
}

void AWBRuntimePlayerController::HandlePrimaryClick()
{
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return;
	if (const AWBRuntimeUnitPresentationActor* UnitActor = Cast<AWBRuntimeUnitPresentationActor>(Hit.GetActor()))
	{
		ForwardUnitSelection(UnitActor->GetStableUnitId());
		return;
	}
	if (BoardActor != nullptr)
	{
		FIntPoint Tile;
		if (BoardActor->WorldToTile(Hit.Location, Tile)) ForwardTileSelection(Tile);
	}
}

void AWBRuntimePlayerController::HandleClearInput() { ForwardClearSelection(); }
void AWBRuntimePlayerController::HandleSubmitInput() { ForwardSubmit(); }
void AWBRuntimePlayerController::HandleEndTurnInput() { ForwardEndTurn(); }
void AWBRuntimePlayerController::HandleViewerSwitchInput()
{
	if (bEnableDevelopmentViewerSwitch && RuntimeHUD != nullptr) RuntimeHUD->RequestViewerSwitch();
}
