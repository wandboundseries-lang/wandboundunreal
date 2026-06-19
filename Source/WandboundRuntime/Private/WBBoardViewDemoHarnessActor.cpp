#include "WBBoardViewDemoHarnessActor.h"

#include "EngineUtils.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewDemoData.h"

AWBBoardViewDemoHarnessActor::AWBBoardViewDemoHarnessActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWBBoardViewDemoHarnessActor::BeginPlay()
{
	Super::BeginPlay();

	if (bRenderDemoOnBeginPlay)
	{
		RenderDemoBoard();
	}
}

FWBBoardViewRefreshResult AWBBoardViewDemoHarnessActor::RenderDemoBoard()
{
	const FWBPublicBoardSummary DemoSummary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	return WBBoardSummaryBridge::RenderSummaryToBoardView(DemoSummary, ResolveBoardViewActor());
}

void AWBBoardViewDemoHarnessActor::SetBoardViewActor(AWBBoardViewActor* InBoardViewActor)
{
	BoardViewActor = InBoardViewActor;
}

AWBBoardViewActor* AWBBoardViewDemoHarnessActor::GetBoardViewActor() const
{
	return BoardViewActor.Get();
}

AWBBoardViewActor* AWBBoardViewDemoHarnessActor::ResolveBoardViewActor()
{
	if (BoardViewActor != nullptr || !bFindBoardViewActorIfMissing)
	{
		return BoardViewActor.Get();
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AWBBoardViewActor> It(World); It; ++It)
	{
		BoardViewActor = *It;
		break;
	}

	return BoardViewActor.Get();
}
