#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBRuntimeInteractionRefreshAdapter.h"
#include "WBRuntimeLegalActionPresentation.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.generated.h"

class UWBRuntimeTurnInteractionModelComponent;
class UWBRuntimeVisualControllerComponent;

struct WANDBOUNDRUNTIME_API FWBRuntimeDecisionPointStatus
{
	bool bReady = false;
	bool bHasActions = false;
	int32 LegalActionCount = 0;
	int32 PresentationEntryCount = 0;
	int32 PublicUnitCount = 0;
	int32 PublicWallCount = 0;
	int32 PublicTerrainCount = 0;
	FString LastRefreshReason;
};

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeDecisionPointCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeDecisionPointCoordinatorComponent();

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	TObjectPtr<UWBRuntimeTurnInteractionModelComponent> TurnInteractionModel;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	TObjectPtr<UWBRuntimeVisualControllerComponent> VisualController;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	bool bRefreshVisualController = true;

	UPROPERTY(EditAnywhere, Category = "Runtime Decision Point")
	bool bRefreshTurnInteractionModel = true;

	void SetTurnInteractionModel(UWBRuntimeTurnInteractionModelComponent* InModel);
	UWBRuntimeTurnInteractionModelComponent* GetTurnInteractionModel() const;

	void SetVisualController(UWBRuntimeVisualControllerComponent* InVisualController);
	UWBRuntimeVisualControllerComponent* GetVisualController() const;

	FWBRuntimeInteractionRefreshResult RefreshDecisionPoint(
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FWBPublicBoardSummary& PublicBoardSummary);

	void ClearDecisionPoint();

	bool HasCurrentDecisionPoint() const;
	FWBRuntimeDecisionPointStatus GetCurrentStatus() const;
	FWBRuntimeInteractionRefreshResult GetLastRefreshResult() const;

	const FWBRuntimeLegalActionPresentationSnapshot* GetCurrentPresentationSnapshot() const;

	bool TryFindPresentationEntryByActionId(
		const FString& ActionId,
		FWBRuntimeActionPresentationEntry& OutEntry) const;

private:
	FWBRuntimeInteractionRefreshResult LastRefreshResult;
	FWBRuntimeDecisionPointStatus CurrentStatus;
	bool bHasCurrentDecisionPoint = false;
};
