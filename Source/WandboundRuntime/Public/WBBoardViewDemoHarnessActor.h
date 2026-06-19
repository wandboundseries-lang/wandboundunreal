#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WBBoardSummaryBridge.h"
#include "WBBoardViewDemoHarnessActor.generated.h"

class AWBBoardViewActor;

UCLASS()
class WANDBOUNDRUNTIME_API AWBBoardViewDemoHarnessActor : public AActor
{
	GENERATED_BODY()

public:
	AWBBoardViewDemoHarnessActor();

	UPROPERTY(EditAnywhere, Category = "Board View Demo")
	TObjectPtr<AWBBoardViewActor> BoardViewActor;

	UPROPERTY(EditAnywhere, Category = "Board View Demo")
	bool bRenderDemoOnBeginPlay = false;

	UPROPERTY(EditAnywhere, Category = "Board View Demo")
	bool bFindBoardViewActorIfMissing = false;

	FWBBoardViewRefreshResult RenderDemoBoard();
	void SetBoardViewActor(AWBBoardViewActor* InBoardViewActor);
	AWBBoardViewActor* GetBoardViewActor() const;

protected:
	virtual void BeginPlay() override;

private:
	AWBBoardViewActor* ResolveBoardViewActor();
};
