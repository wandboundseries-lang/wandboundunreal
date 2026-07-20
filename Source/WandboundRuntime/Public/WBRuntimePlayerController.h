#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WBRuntimeMatchPresentation.h"
#include "WBRuntimePlayerController.generated.h"

class AWBBoardViewActor;
class AWBRuntimeMatchBootstrapActor;
class UWBRuntimeMatchHostComponent;
class UWBRuntimeMatchHUDWidget;

UCLASS()
class WANDBOUNDRUNTIME_API AWBRuntimePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWBRuntimePlayerController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Runtime")
	bool bCreateHUDOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Runtime")
	bool bEnableDevelopmentViewerSwitch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Runtime")
	TSubclassOf<UWBRuntimeMatchHUDWidget> HUDWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	void SetRuntimeReferences(
		UWBRuntimeMatchHostComponent* InMatchHost,
		AWBBoardViewActor* InBoardActor,
		UWBRuntimeMatchHUDWidget* InHUDWidget);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	void ClearRuntimeReferences(bool bRemoveHUDFromViewport = false);

	UFUNCTION(BlueprintPure, Category = "Wandbound|Runtime")
	UWBRuntimeMatchHUDWidget* GetRuntimeHUD() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Runtime")
	UWBRuntimeMatchHostComponent* GetRuntimeMatchHost() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Runtime")
	AWBBoardViewActor* GetRuntimeBoardActor() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Runtime")
	bool IsRuntimeInteractionBound() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Runtime")
	bool IsRuntimeInputModeConfigured() const;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	void SetRuntimeInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	FWBRuntimeMatchCommandResult ForwardTileSelection(FIntPoint Tile);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	FWBRuntimeMatchCommandResult ForwardUnitSelection(int32 UnitId);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	void ForwardClearSelection();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	FWBRuntimeMatchCommandResult ForwardSubmit();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Runtime")
	FWBRuntimeMatchCommandResult ForwardEndTurn();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWBRuntimeMatchHostComponent> MatchHost;

	UPROPERTY(Transient)
	TObjectPtr<AWBBoardViewActor> BoardActor;

	UPROPERTY(Transient)
	TObjectPtr<UWBRuntimeMatchHUDWidget> RuntimeHUD;

	bool bRuntimeInteractionEnabled = false;
	bool bRuntimeInputModeConfigured = false;

	void ResolveRuntimeReferences();
	FWBRuntimeMatchCommandResult MakeUnboundResult() const;
	void HandlePrimaryClick();
	void HandleClearInput();
	void HandleSubmitInput();
	void HandleEndTurnInput();
	void HandleViewerSwitchInput();
};
