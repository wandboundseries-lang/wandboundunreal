#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBSelectedActionVisualHarness.h"
#include "WBRuntimeControllerFacadeComponent.generated.h"

class UWBRuntimeVisualControllerComponent;

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeControllerFacadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeControllerFacadeComponent();

	UPROPERTY(EditAnywhere, Category = "Runtime Controller Facade")
	TObjectPtr<UWBRuntimeVisualControllerComponent> VisualController;

	UPROPERTY(EditAnywhere, Category = "Runtime Controller Facade")
	bool bRefreshVisualsOnActionSuccess = true;

	UPROPERTY(EditAnywhere, Category = "Runtime Controller Facade")
	bool bRefreshVisualsOnActionFailure = false;

	void SetVisualController(UWBRuntimeVisualControllerComponent* InVisualController);
	UWBRuntimeVisualControllerComponent* GetVisualController() const;

	FWBSelectedActionVisualHarnessResult ExecuteSelectedAction(
		FWBGameStateData& State,
		const FWBAction& SelectedAction,
		FWBRuntimeTurnResolutionContext& RuntimeContext);

	bool HasExecutedAction() const;
	const FWBRuntimeSelectedActionResult& GetLastRuntimeResult() const;
	FWBBoardViewRefreshResult GetLastVisualRefreshResult() const;
	void ClearLastResults();

private:
	bool bHasExecutedAction = false;
	FWBRuntimeSelectedActionResult LastRuntimeResult;
	FWBBoardViewRefreshResult LastVisualRefreshResult;
};
