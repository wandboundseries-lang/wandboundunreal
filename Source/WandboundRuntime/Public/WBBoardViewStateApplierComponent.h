#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBBoardSummaryBridge.h"
#include "WBBoardViewStateApplierComponent.generated.h"

class AWBBoardViewActor;

UCLASS()
class WANDBOUNDRUNTIME_API UWBBoardViewStateApplierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBBoardViewStateApplierComponent();

	UPROPERTY(EditAnywhere, Category = "Board View State")
	TObjectPtr<AWBBoardViewActor> BoardViewActor;

	UPROPERTY(EditAnywhere, Category = "Board View State")
	bool bApplyOnSummarySet = true;

	void SetBoardViewActor(AWBBoardViewActor* InBoardViewActor);
	AWBBoardViewActor* GetBoardViewActor() const;

	void SetLatestPublicBoardSummary(const FWBPublicBoardSummary& Summary);
	void SetLatestPublicBoardSummaryFromState(const FWBGameStateData& State);

	bool HasLatestPublicBoardSummary() const;
	const FWBPublicBoardSummary& GetLatestPublicBoardSummary() const;

	FWBBoardViewRefreshResult ApplyLatestPublicBoardSummary();
	FWBBoardViewRefreshResult GetLastRefreshResult() const;

private:
	FWBPublicBoardSummary LatestPublicBoardSummary;
	bool bHasLatestPublicBoardSummary = false;
	FWBBoardViewRefreshResult LastRefreshResult;
};
