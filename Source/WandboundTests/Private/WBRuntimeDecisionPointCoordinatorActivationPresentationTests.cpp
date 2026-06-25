#include "Misc/AutomationTest.h"

#include "WBActionCodec.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeDecisionPointCoordinatorComponent* MakeActivationCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeActivationCoordinatorTurnModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeActivationPresentationModelComponent* MakeActivationCoordinatorModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

FWBAction MakeActivationCoordinatorMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBPublicBoardSummary MakeActivationCoordinatorSummary()
{
	FWBPublicBoardSummary Summary;

	FWBPublicUnitBoardSummary SourceUnit;
	SourceUnit.UnitId = 1;
	SourceUnit.OwnerId = 0;
	SourceUnit.CardId = TEXT("visible_coordinator_activation_source");
	SourceUnit.X = 4;
	SourceUnit.Y = 4;
	Summary.Units.Add(SourceUnit);

	FWBPublicUnitBoardSummary TargetUnit;
	TargetUnit.UnitId = 2;
	TargetUnit.OwnerId = 1;
	TargetUnit.CardId = TEXT("visible_coordinator_activation_target");
	TargetUnit.X = 5;
	TargetUnit.Y = 4;
	Summary.Units.Add(TargetUnit);

	return Summary;
}

FWBCardActivationLegalActionSet MakeActivationCoordinatorActionSet()
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = 2;

	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = TEXT("activate:p0:u1:coordinator:t2");
	Action.PublicLabel = TEXT("Coordinator Activation");
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.Target = Target;

	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);
	return Set;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorActivationPresentationSeparateRefreshTest, "Wandbound.Runtime.DecisionPointCoordinatorActivationPresentation.SeparateRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorActivationPresentationSeparateRefreshTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeActivationCoordinator();
	UWBRuntimeTurnInteractionModelComponent* TurnModel = MakeActivationCoordinatorTurnModel();
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = MakeActivationCoordinatorModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Turn model"), TurnModel);
	TestNotNull(TEXT("Activation model"), ActivationModel);
	if (Coordinator == nullptr || TurnModel == nullptr || ActivationModel == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(TurnModel);
	Coordinator->SetActivationPresentationModel(ActivationModel);
	const FWBAction MoveAction = MakeActivationCoordinatorMoveAction();
	const FWBRuntimeInteractionRefreshResult NormalResult =
		Coordinator->RefreshDecisionPoint({ MoveAction }, MakeActivationCoordinatorSummary());
	TestTrue(TEXT("Normal refresh succeeds"), NormalResult.bOk);
	TestTrue(TEXT("Current decision point exists"), Coordinator->HasCurrentDecisionPoint());
	TestEqual(TEXT("Normal snapshot count"), Coordinator->GetCurrentPresentationSnapshot()->Entries.Num(), 1);

	const FWBCardActivationLegalActionSet ActivationSet = MakeActivationCoordinatorActionSet();
	const FWBRuntimeActivationPresentationRefreshResult ActivationResult =
		Coordinator->RefreshActivationPresentationFromExternalData(
			ActivationSet,
			MakeActivationCoordinatorSummary());

	TestTrue(TEXT("Activation refresh succeeds"), ActivationResult.bOk);
	TestTrue(TEXT("Activation model has state"), ActivationModel->HasCurrentActivationPresentationState());
	TestEqual(TEXT("Activation action count"), ActivationResult.ActivationActionCount, ActivationSet.Actions.Num());
	TestEqual(TEXT("Activation status count"), Coordinator->GetCurrentStatus().ActivationActionCount, ActivationSet.Actions.Num());
	TestEqual(TEXT("Target status count"), Coordinator->GetCurrentStatus().ActivationTargetPresentationEntryCount, ActivationSet.Actions.Num());
	TestEqual(TEXT("Normal snapshot still one"), Coordinator->GetCurrentPresentationSnapshot()->Entries.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorActivationPresentationMissingModelTest, "Wandbound.Runtime.DecisionPointCoordinatorActivationPresentation.MissingModelDoesNotBreakNormalRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorActivationPresentationMissingModelTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeActivationCoordinator();
	UWBRuntimeTurnInteractionModelComponent* TurnModel = MakeActivationCoordinatorTurnModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Turn model"), TurnModel);
	if (Coordinator == nullptr || TurnModel == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeActivationCoordinatorMoveAction();
	Coordinator->SetTurnInteractionModel(TurnModel);
	TestTrue(TEXT("Normal refresh succeeds"), Coordinator->RefreshDecisionPoint({ MoveAction }, MakeActivationCoordinatorSummary()).bOk);
	const FString NormalActionIdBefore = Coordinator->GetCurrentPresentationSnapshot()->Entries[0].ActionId;

	const FWBRuntimeActivationPresentationRefreshResult ActivationResult =
		Coordinator->RefreshActivationPresentationFromExternalData(
			MakeActivationCoordinatorActionSet(),
			MakeActivationCoordinatorSummary());

	TestFalse(TEXT("Activation refresh fails"), ActivationResult.bOk);
	TestEqual(TEXT("Activation failure reason"), ActivationResult.Reason, FString(TEXT("activation_presentation_model_missing")));
	TestTrue(TEXT("Normal decision point remains"), Coordinator->HasCurrentDecisionPoint());
	TestNotNull(TEXT("Normal snapshot remains"), Coordinator->GetCurrentPresentationSnapshot());
	TestEqual(TEXT("Normal action id unchanged"), Coordinator->GetCurrentPresentationSnapshot()->Entries[0].ActionId, NormalActionIdBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorActivationPresentationNormalNoActivationModelTest, "Wandbound.Runtime.DecisionPointCoordinatorActivationPresentation.NormalRefreshDoesNotRequireActivationModel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorActivationPresentationNormalNoActivationModelTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeActivationCoordinator();
	UWBRuntimeTurnInteractionModelComponent* TurnModel = MakeActivationCoordinatorTurnModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Turn model"), TurnModel);
	if (Coordinator == nullptr || TurnModel == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(TurnModel);
	const FWBRuntimeInteractionRefreshResult Result =
		Coordinator->RefreshDecisionPoint(
			{ MakeActivationCoordinatorMoveAction() },
			MakeActivationCoordinatorSummary());

	TestTrue(TEXT("Normal refresh succeeds"), Result.bOk);
	TestTrue(TEXT("Current decision point exists"), Coordinator->HasCurrentDecisionPoint());
	TestNull(TEXT("No activation model required"), Coordinator->GetActivationPresentationModel());
	TestEqual(TEXT("Normal presentation count"), Coordinator->GetCurrentStatus().PresentationEntryCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorActivationPresentationDoesNotMutateNormalSnapshotTest, "Wandbound.Runtime.DecisionPointCoordinatorActivationPresentation.DoesNotMutateNormalSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorActivationPresentationDoesNotMutateNormalSnapshotTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeActivationCoordinator();
	UWBRuntimeTurnInteractionModelComponent* TurnModel = MakeActivationCoordinatorTurnModel();
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = MakeActivationCoordinatorModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Turn model"), TurnModel);
	TestNotNull(TEXT("Activation model"), ActivationModel);
	if (Coordinator == nullptr || TurnModel == nullptr || ActivationModel == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeActivationCoordinatorMoveAction();
	Coordinator->SetTurnInteractionModel(TurnModel);
	Coordinator->SetActivationPresentationModel(ActivationModel);
	Coordinator->RefreshDecisionPoint({ MoveAction }, MakeActivationCoordinatorSummary());
	const FString NormalActionIdBefore = Coordinator->GetCurrentPresentationSnapshot()->Entries[0].ActionId;
	const FWBTile ToTileBefore = Coordinator->GetCurrentPresentationSnapshot()->Entries[0].ToTile;

	Coordinator->RefreshActivationPresentationFromExternalData(
		MakeActivationCoordinatorActionSet(),
		MakeActivationCoordinatorSummary());

	TestEqual(TEXT("Normal snapshot count unchanged"), Coordinator->GetCurrentPresentationSnapshot()->Entries.Num(), 1);
	TestEqual(TEXT("Normal action id unchanged"), Coordinator->GetCurrentPresentationSnapshot()->Entries[0].ActionId, NormalActionIdBefore);
	TestEqual(TEXT("Normal to tile unchanged"), Coordinator->GetCurrentPresentationSnapshot()->Entries[0].ToTile, ToTileBefore);
	TestEqual(TEXT("Normal move codec unchanged"), WBActionCodec::MakeActionId(MoveAction), NormalActionIdBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorActivationPresentationClearPolicyTest, "Wandbound.Runtime.DecisionPointCoordinatorActivationPresentation.ClearPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorActivationPresentationClearPolicyTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeActivationCoordinator();
	UWBRuntimeTurnInteractionModelComponent* TurnModel = MakeActivationCoordinatorTurnModel();
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = MakeActivationCoordinatorModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Turn model"), TurnModel);
	TestNotNull(TEXT("Activation model"), ActivationModel);
	if (Coordinator == nullptr || TurnModel == nullptr || ActivationModel == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(TurnModel);
	Coordinator->SetActivationPresentationModel(ActivationModel);
	TestTrue(TEXT("Normal refresh succeeds"), Coordinator->RefreshDecisionPoint(
		{ MakeActivationCoordinatorMoveAction() },
		MakeActivationCoordinatorSummary()).bOk);
	TestTrue(TEXT("Activation refresh succeeds"), Coordinator->RefreshActivationPresentationFromExternalData(
		MakeActivationCoordinatorActionSet(),
		MakeActivationCoordinatorSummary()).bOk);
	TestTrue(TEXT("Activation state exists"), ActivationModel->HasCurrentActivationPresentationState());

	Coordinator->ClearDecisionPoint();
	TestFalse(TEXT("Normal decision point cleared"), Coordinator->HasCurrentDecisionPoint());
	TestNull(TEXT("Normal snapshot cleared"), Coordinator->GetCurrentPresentationSnapshot());
	TestTrue(TEXT("Activation state remains after normal clear"), ActivationModel->HasCurrentActivationPresentationState());

	Coordinator->ClearActivationPresentation();
	TestFalse(TEXT("Activation state cleared explicitly"), ActivationModel->HasCurrentActivationPresentationState());
	TestEqual(TEXT("Activation status count cleared"), Coordinator->GetCurrentStatus().ActivationActionCount, 0);
	TestEqual(TEXT("Target status count cleared"), Coordinator->GetCurrentStatus().ActivationTargetPresentationEntryCount, 0);
	return true;
}

#endif
