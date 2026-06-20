#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBBoardSummaryBridge.h"
#include "WBRuntimeVisualControllerComponent.generated.h"

class UWBBoardViewStateApplierComponent;
struct FWBRuntimeSelectedActionResult;

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeVisualControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeVisualControllerComponent();

	UPROPERTY(EditAnywhere, Category = "Runtime Visual Controller")
	TObjectPtr<UWBBoardViewStateApplierComponent> BoardViewStateApplier;

	UPROPERTY(EditAnywhere, Category = "Runtime Visual Controller")
	bool bAutoApplyPublicBoardSummaries = true;

	void SetBoardViewStateApplier(UWBBoardViewStateApplierComponent* InApplier);
	UWBBoardViewStateApplierComponent* GetBoardViewStateApplier() const;

	FWBBoardViewRefreshResult ApplyPublicBoardSummary(const FWBPublicBoardSummary& Summary);
	FWBBoardViewRefreshResult ApplyRuntimeSelectedActionResult(const FWBRuntimeSelectedActionResult& Result);

	bool HasReceivedPublicBoardSummary() const;
	FWBBoardViewRefreshResult GetLastBoardRefreshResult() const;

private:
	FWBBoardViewRefreshResult LastBoardRefreshResult;
	bool bHasReceivedPublicBoardSummary = false;
};
