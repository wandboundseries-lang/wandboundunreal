#include "Misc/AutomationTest.h"

#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeControllerFacadeComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeDecisionPointOwnerComponent* MakeActivationOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeActivationOwnerCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeActivationOwnerTurnModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeActivationPresentationModelComponent* MakeActivationOwnerPresentationModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

UWBRuntimeControllerFacadeComponent* MakeActivationOwnerFacade()
{
	return NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
}

bool MakeActivationOwnerPath(
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel,
	UWBRuntimeControllerFacadeComponent*& OutFacade)
{
	OutOwner = MakeActivationOwner();
	OutCoordinator = MakeActivationOwnerCoordinator();
	OutTurnModel = MakeActivationOwnerTurnModel();
	OutActivationModel = MakeActivationOwnerPresentationModel();
	OutFacade = MakeActivationOwnerFacade();

	if (OutOwner == nullptr
		|| OutCoordinator == nullptr
		|| OutTurnModel == nullptr
		|| OutActivationModel == nullptr
		|| OutFacade == nullptr)
	{
		return false;
	}

	OutCoordinator->SetTurnInteractionModel(OutTurnModel);
	OutCoordinator->SetActivationPresentationModel(OutActivationModel);
	OutOwner->SetDecisionPointCoordinator(OutCoordinator);
	OutOwner->SetRuntimeControllerFacade(OutFacade);
	return true;
}

FWBPlayerStateData MakeActivationOwnerPlayer(
	const int32 PlayerId,
	const int32 RemainingMP = 0)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeActivationOwnerUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeActivationOwnerState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 4;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeActivationOwnerPlayer(0, 2));
	State.Players.Add(MakeActivationOwnerPlayer(1, 0));
	State.AddUnitForTest(MakeActivationOwnerUnit(1, 0, FWBTile(4, 4), TEXT("visible_owner_activation_source")));
	State.AddUnitForTest(MakeActivationOwnerUnit(2, 1, FWBTile(6, 4), TEXT("visible_owner_activation_target")));
	return State;
}

FWBAction MakeActivationOwnerMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBPublicBoardSummary MakeActivationOwnerSummary()
{
	FWBPublicBoardSummary Summary;

	FWBPublicUnitBoardSummary SourceUnit;
	SourceUnit.UnitId = 1;
	SourceUnit.OwnerId = 0;
	SourceUnit.CardId = TEXT("visible_owner_activation_source");
	SourceUnit.X = 4;
	SourceUnit.Y = 4;
	Summary.Units.Add(SourceUnit);

	FWBPublicUnitBoardSummary TargetUnit;
	TargetUnit.UnitId = 2;
	TargetUnit.OwnerId = 1;
	TargetUnit.CardId = TEXT("visible_owner_activation_target");
	TargetUnit.X = 6;
	TargetUnit.Y = 4;
	Summary.Units.Add(TargetUnit);

	return Summary;
}

FWBCardActivationLegalActionSet MakeActivationOwnerActionSet(const FString& ActionId = TEXT("activate:p0:u1:owner:t2"))
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = 2;

	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActionId;
	Action.PublicLabel = TEXT("Owner Activation");
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.Target = Target;

	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);
	return Set;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerActivationPresentationDelegatesTest, "Wandbound.Runtime.DecisionPointOwnerActivationPresentation.DelegatesRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerActivationPresentationDelegatesTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeActivationOwnerPath(Owner, Coordinator, TurnModel, ActivationModel, Facade));

	const FWBCardActivationLegalActionSet ActionSet = MakeActivationOwnerActionSet();
	const FWBRuntimeActivationPresentationRefreshResult Result =
		Owner->RefreshActivationPresentationFromExternalData(
			ActionSet,
			MakeActivationOwnerSummary());

	TestTrue(TEXT("Activation refresh succeeds"), Result.bOk);
	TestTrue(TEXT("Activation model has state"), ActivationModel->HasCurrentActivationPresentationState());
	TestEqual(TEXT("Action count"), Result.ActivationActionCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Coordinator activation status count"), Coordinator->GetCurrentStatus().ActivationActionCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Owner activation status count"), Owner->GetCurrentStatus().ActivationActionCount, ActionSet.Actions.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerActivationPresentationMissingCoordinatorTest, "Wandbound.Runtime.DecisionPointOwnerActivationPresentation.MissingCoordinatorFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerActivationPresentationMissingCoordinatorTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeActivationOwner();
	TestNotNull(TEXT("Owner"), Owner);
	if (Owner == nullptr)
	{
		return false;
	}

	const FWBRuntimeActivationPresentationRefreshResult Result =
		Owner->RefreshActivationPresentationFromExternalData(
			MakeActivationOwnerActionSet(),
			MakeActivationOwnerSummary());

	TestFalse(TEXT("Activation refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("decision_point_coordinator_missing")));
	TestFalse(TEXT("No execution stored"), Owner->HasLastExecutionResult());
	TestFalse(TEXT("No current normal decision point"), Owner->HasCurrentDecisionPoint());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerActivationPresentationDoesNotExecuteTest, "Wandbound.Runtime.DecisionPointOwnerActivationPresentation.RefreshDoesNotExecuteActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerActivationPresentationDoesNotExecuteTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationOwnerState();
	const FWBGameStateData Before = State;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeActivationOwnerPath(Owner, Coordinator, TurnModel, ActivationModel, Facade));

	const FWBRuntimeActivationPresentationRefreshResult Result =
		Owner->RefreshActivationPresentationFromExternalData(
			MakeActivationOwnerActionSet(),
			MakeActivationOwnerSummary());

	TestTrue(TEXT("Activation refresh succeeds"), Result.bOk);
	TestFalse(TEXT("No selected-action execution result"), Owner->HasLastExecutionResult());
	TestEqual(TEXT("Source unit x unchanged"), State.GetUnitById(1)->X, Before.GetUnitById(1)->X);
	TestEqual(TEXT("Source unit y unchanged"), State.GetUnitById(1)->Y, Before.GetUnitById(1)->Y);
	TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(2)->HP, Before.GetUnitById(2)->HP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerActivationPresentationStalePolicyTest, "Wandbound.Runtime.DecisionPointOwnerActivationPresentation.StaleStatePolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerActivationPresentationStalePolicyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeActivationOwnerPath(Owner, Coordinator, TurnModel, ActivationModel, Facade));

	const FWBAction MoveAction = MakeActivationOwnerMoveAction();
	TestTrue(TEXT("Normal refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		{ MoveAction },
		MakeActivationOwnerSummary()).bOk);
	const FString ActivationActionIdBefore = MakeActivationOwnerActionSet().Actions[0].ActivationActionId;
	TestTrue(TEXT("Activation refresh succeeds"), Owner->RefreshActivationPresentationFromExternalData(
		MakeActivationOwnerActionSet(),
		MakeActivationOwnerSummary()).bOk);

	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeActionSelectionExecutionResult ExecutionResult =
		Owner->ExecuteSelectedActionId(
			State,
			WBActionCodec::MakeActionId(MoveAction),
			Context);

	TestTrue(TEXT("Normal move selection succeeds"), ExecutionResult.Selection.bOk);
	TestTrue(TEXT("Normal move executes"), ExecutionResult.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Activation presentation remains stale until explicit clear/refresh"), ActivationModel->HasCurrentActivationPresentationState());
	TestEqual(TEXT("Activation snapshot count unchanged"), ActivationModel->GetCurrentTargetPresentationSnapshot().Entries.Num(), 1);
	TestEqual(TEXT("Activation id unchanged"), ActivationModel->GetCurrentTargetPresentationSnapshot().Entries[0].ActivationActionId, ActivationActionIdBefore);
	TestEqual(TEXT("Unit moved by normal execution"), State.GetUnitById(1)->X, 5);

	const FWBCardActivationLegalActionSet FreshActivationSet =
		MakeActivationOwnerActionSet(TEXT("activate:p0:u1:owner:fresh:t2"));
	TestTrue(TEXT("Explicit activation refresh succeeds"), Owner->RefreshActivationPresentationFromExternalData(
		FreshActivationSet,
		MakeActivationOwnerSummary()).bOk);
	TestEqual(TEXT("Activation id updates only after explicit refresh"), ActivationModel->GetCurrentTargetPresentationSnapshot().Entries[0].ActivationActionId, FreshActivationSet.Actions[0].ActivationActionId);

	Owner->ClearActivationPresentation();
	TestFalse(TEXT("Activation state clears explicitly"), ActivationModel->HasCurrentActivationPresentationState());
	return true;
}

#endif
