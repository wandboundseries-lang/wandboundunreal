#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBRuntimeActivationExecutionHandoff.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 HandoffSourceUnitId = 1;
constexpr int32 HandoffTargetUnitId = 2;

UWBRuntimeActivationPresentationModelComponent* MakeHandoffModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeHandoffCoordinator()
{
	return NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointOwnerComponent* MakeHandoffOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

FWBEffectTargetRef MakeHandoffUnitTarget()
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = HandoffTargetUnitId;
	return Target;
}

FWBCardActivationLegalAction MakeHandoffAction(
	const FString& ActionId = TEXT("activate:p0:u1:handoff:t2"),
	const FString& PublicLabel = TEXT("Handoff Damage"))
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActionId;
	Action.PublicLabel = PublicLabel;
	Action.PlayerId = 0;
	Action.SourceUnitId = HandoffSourceUnitId;
	Action.Target = MakeHandoffUnitTarget();
	Action.Candidate.ActivationCandidateId = TEXT("internal_handoff_candidate");
	Action.Candidate.SourceCardId = TEXT("internal_handoff_candidate_card");
	Action.Candidate.SourceEffectId = TEXT("internal_handoff_candidate_effect");
	Action.Command.Source.PlayerId = 0;
	Action.Command.Source.SourceUnitId = HandoffSourceUnitId;
	Action.Command.Source.SourceCardId = TEXT("internal_handoff_command_card");
	Action.Command.Source.SourceEffectId = TEXT("internal_handoff_command_effect");
	Action.Command.DebugActivationId = TEXT("internal_handoff_debug");
	Action.Command.CostPaymentCommit.PlayerId = 0;
	Action.Command.CostPaymentCommit.SourceUnitId = HandoffSourceUnitId;
	Action.Command.CostPaymentCommit.RequiredRR = 1;
	Action.Command.CostPaymentCommit.CostKind = FName(TEXT("internal_handoff_cost"));
	Action.Command.UsageCommit.PlayerId = 0;
	Action.Command.UsageCommit.UsageKey = TEXT("internal_handoff_usage");
	return Action;
}

FWBCardActivationLegalActionSet MakeHandoffActionSet(
	const FWBCardActivationLegalAction& Action = MakeHandoffAction())
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);
	return Set;
}

FWBPublicUnitBoardSummary MakeHandoffPublicUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile)
{
	FWBPublicUnitBoardSummary Unit;
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
	return Unit;
}

FWBPublicBoardSummary MakeHandoffPublicSummary()
{
	FWBPublicBoardSummary Summary;
	Summary.Units.Add(MakeHandoffPublicUnit(
		HandoffSourceUnitId,
		0,
		TEXT("visible_handoff_source"),
		FWBTile(4, 4)));
	Summary.Units.Add(MakeHandoffPublicUnit(
		HandoffTargetUnitId,
		1,
		TEXT("visible_handoff_target"),
		FWBTile(5, 4)));
	return Summary;
}

FWBPlayerStateData MakeHandoffPlayer(
	const int32 PlayerId,
	const int32 RemainingMP = 0)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeHandoffUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = UnitId == HandoffTargetUnitId ? 4 : 5;
	Unit.MaxHP = 5;
	Unit.CurrentArmor = UnitId == HandoffTargetUnitId ? 2 : 1;
	Unit.MaxArmor = 2;
	Unit.RLTotal = 4;
	Unit.RLUsed = UnitId == HandoffSourceUnitId ? 1 : 0;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	if (UnitId == HandoffTargetUnitId)
	{
		Unit.AddStatus(FName(TEXT("Burn")), 2);
	}
	return Unit;
}

FWBGameStateData MakeHandoffState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 8;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeHandoffPlayer(0, 2));
	State.Players.Add(MakeHandoffPlayer(1, 0));
	State.AddUnitForTest(MakeHandoffUnit(
		HandoffSourceUnitId,
		0,
		TEXT("visible_handoff_source"),
		FWBTile(4, 4)));
	State.AddUnitForTest(MakeHandoffUnit(
		HandoffTargetUnitId,
		1,
		TEXT("visible_handoff_target"),
		FWBTile(5, 4)));
	return State;
}

void RefreshHandoffModel(
	UWBRuntimeActivationPresentationModelComponent* Model,
	const FWBCardActivationLegalActionSet& ActionSet = MakeHandoffActionSet())
{
	if (Model != nullptr)
	{
		Model->SetCurrentActivationPresentationState(
			ActionSet,
			MakeHandoffPublicSummary());
	}
}

FWBRuntimeActivationSelectionResolution MakeResolvedHandoffSelection()
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeHandoffModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeHandoffActionSet();
	RefreshHandoffModel(Model, ActionSet);
	return Model->ResolveSelectedActivationActionId(ActionSet.Actions[0].ActivationActionId);
}

FString CombineHandoffPublicText(
	const FWBRuntimeActivationExecutionHandoffResult& Result)
{
	FString Text;
	Text += Result.Reason;
	if (Result.bHasActivationPresentationEntry)
	{
		Text += Result.ActivationPresentationEntry.ActivationActionId;
		Text += Result.ActivationPresentationEntry.PublicLabel;
		Text += Result.ActivationPresentationEntry.SourcePublicCardId;
		Text += Result.ActivationPresentationEntry.TargetPublicCardId;
	}
	if (Result.bHasTargetPresentationEntry)
	{
		Text += Result.TargetPresentationEntry.ActivationActionId;
		Text += Result.TargetPresentationEntry.PublicActivationLabel;
		Text += Result.TargetPresentationEntry.PublicTargetLabel;
		Text += Result.TargetPresentationEntry.SourcePublicCardId;
		Text += Result.TargetPresentationEntry.TargetPublicCardId;
	}
	return Text;
}

bool LoadHandoffSource(FString& OutSource)
{
	return FFileHelper::LoadFileToString(
		OutSource,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source"),
			TEXT("WandboundRuntime"),
			TEXT("Private"),
			TEXT("WBRuntimeActivationExecutionHandoff.cpp")));
}

TArray<FString> NormalActionIdsForPlayer(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	TArray<FString> ActionIds;
	for (const FWBAction& Action : WBRules::GenerateLegalActionsForPlayer(State, PlayerId))
	{
		ActionIds.Add(WBActionCodec::MakeActionId(Action));
	}
	return ActionIds;
}

bool CompareHandoffStringArrays(
	FAutomationTestBase& Test,
	const FString& Label,
	const TArray<FString>& Actual,
	const TArray<FString>& Expected)
{
	Test.TestEqual(*FString::Printf(TEXT("%s count"), *Label), Actual.Num(), Expected.Num());
	const int32 NumToCompare = FMath::Min(Actual.Num(), Expected.Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s %d"), *Label, Index), Actual[Index], Expected[Index]);
	}

	return Actual == Expected;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffUnresolvedSelectionTest, "Wandbound.Runtime.ActivationExecutionHandoff.UnresolvedSelectionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffUnresolvedSelectionTest::RunTest(const FString& Parameters)
{
	FWBRuntimeActivationSelectionResolution Selection;
	Selection.bOk = false;
	Selection.SelectedActivationActionId = TEXT("activate:missing");
	Selection.Reason = TEXT("activation_action_id_not_found");

	const FWBRuntimeActivationExecutionHandoffResult Result =
		WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(Selection);

	TestFalse(TEXT("Handoff fails"), Result.bHandoffOk);
	TestFalse(TEXT("Selection unresolved"), Result.bSelectionResolved);
	TestFalse(TEXT("Execution not implemented"), Result.bExecutionImplemented);
	TestFalse(TEXT("Execution not attempted"), Result.bExecutionAttempted);
	TestEqual(TEXT("Reason preserved"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	TestEqual(TEXT("Selected id retained"), Result.SelectedActivationActionId, FString(TEXT("activate:missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffResolvedSelectionTest, "Wandbound.Runtime.ActivationExecutionHandoff.ResolvedSelectionReturnsNotImplemented", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffResolvedSelectionTest::RunTest(const FString& Parameters)
{
	const FWBRuntimeActivationSelectionResolution Selection = MakeResolvedHandoffSelection();
	const FWBRuntimeActivationExecutionHandoffResult Result =
		WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(Selection);

	TestTrue(TEXT("Handoff succeeds"), Result.bHandoffOk);
	TestTrue(TEXT("Selection resolved"), Result.bSelectionResolved);
	TestFalse(TEXT("Execution not implemented"), Result.bExecutionImplemented);
	TestFalse(TEXT("Execution not attempted"), Result.bExecutionAttempted);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_execution_not_implemented")));
	TestEqual(TEXT("Activation action copied"), Result.ActivationAction.ActivationActionId, Selection.ActivationAction.ActivationActionId);
	TestEqual(TEXT("Internal command retained"), Result.ActivationAction.Command.DebugActivationId, FString(TEXT("internal_handoff_debug")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffActivationPresentationTest, "Wandbound.Runtime.ActivationExecutionHandoff.IncludesActivationPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffActivationPresentationTest::RunTest(const FString& Parameters)
{
	const FWBRuntimeActivationExecutionHandoffResult Result =
		WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(MakeResolvedHandoffSelection());

	TestTrue(TEXT("Has activation presentation"), Result.bHasActivationPresentationEntry);
	TestEqual(TEXT("Activation label"), Result.ActivationPresentationEntry.PublicLabel, FString(TEXT("Handoff Damage")));
	TestEqual(TEXT("Source public card"), Result.ActivationPresentationEntry.SourcePublicCardId, FString(TEXT("visible_handoff_source")));
	TestEqual(TEXT("Target public card"), Result.ActivationPresentationEntry.TargetPublicCardId, FString(TEXT("visible_handoff_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffTargetPresentationTest, "Wandbound.Runtime.ActivationExecutionHandoff.IncludesTargetPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffTargetPresentationTest::RunTest(const FString& Parameters)
{
	const FWBRuntimeActivationExecutionHandoffResult Result =
		WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(MakeResolvedHandoffSelection());

	TestTrue(TEXT("Has target presentation"), Result.bHasTargetPresentationEntry);
	TestEqual(TEXT("Target kind"), Result.TargetPresentationEntry.TargetKind, EWBCardActivationTargetPresentationKind::Unit);
	TestEqual(TEXT("Target label"), Result.TargetPresentationEntry.PublicTargetLabel, FString(TEXT("Choose Unit")));
	TestEqual(TEXT("Source public card"), Result.TargetPresentationEntry.SourcePublicCardId, FString(TEXT("visible_handoff_source")));
	TestEqual(TEXT("Target public card"), Result.TargetPresentationEntry.TargetPublicCardId, FString(TEXT("visible_handoff_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffDoesNotExecuteTest, "Wandbound.Runtime.ActivationExecutionHandoff.DoesNotExecuteOrMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffDoesNotExecuteTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBGameStateData Before = State;
	const TArray<FString> BeforeNormalActionIds = NormalActionIdsForPlayer(State, 0);

	const FWBRuntimeActivationExecutionHandoffResult Result =
		WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(MakeResolvedHandoffSelection());

	const TArray<FString> AfterNormalActionIds = NormalActionIdsForPlayer(State, 0);
	TestTrue(TEXT("Handoff succeeds"), Result.bHandoffOk);
	TestFalse(TEXT("Execution not attempted"), Result.bExecutionAttempted);
	CompareHandoffStringArrays(
		*this,
		TEXT("Normal action ids unchanged"),
		AfterNormalActionIds,
		BeforeNormalActionIds);
	TestEqual(TEXT("Source x unchanged"), State.GetUnitById(HandoffSourceUnitId)->X, Before.GetUnitById(HandoffSourceUnitId)->X);
	TestEqual(TEXT("Source y unchanged"), State.GetUnitById(HandoffSourceUnitId)->Y, Before.GetUnitById(HandoffSourceUnitId)->Y);
	TestEqual(TEXT("Source RLUsed unchanged"), State.GetUnitById(HandoffSourceUnitId)->RLUsed, Before.GetUnitById(HandoffSourceUnitId)->RLUsed);
	TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(HandoffTargetUnitId)->HP, Before.GetUnitById(HandoffTargetUnitId)->HP);
	TestEqual(TEXT("Target armor unchanged"), State.GetUnitById(HandoffTargetUnitId)->CurrentArmor, Before.GetUnitById(HandoffTargetUnitId)->CurrentArmor);
	TestEqual(TEXT("Target status count unchanged"), State.GetUnitById(HandoffTargetUnitId)->Statuses.Num(), Before.GetUnitById(HandoffTargetUnitId)->Statuses.Num());
	TestEqual(TEXT("Target Burn duration unchanged"), State.GetUnitById(HandoffTargetUnitId)->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffModelConvenienceTest, "Wandbound.Runtime.ActivationExecutionHandoff.ModelConvenience", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffModelConvenienceTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeHandoffModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeHandoffActionSet();
	RefreshHandoffModel(Model, ActionSet);

	const FWBRuntimeActivationExecutionHandoffResult Result =
		Model->CreateExecutionHandoffForActivationActionId(ActionSet.Actions[0].ActivationActionId);

	TestTrue(TEXT("Handoff succeeds"), Result.bHandoffOk);
	TestTrue(TEXT("Selection resolved"), Result.bSelectionResolved);
	TestFalse(TEXT("Execution not implemented"), Result.bExecutionImplemented);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_execution_not_implemented")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffModelMissingIdTest, "Wandbound.Runtime.ActivationExecutionHandoff.ModelMissingId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffModelMissingIdTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeHandoffModel();
	RefreshHandoffModel(Model);

	const FWBRuntimeActivationExecutionHandoffResult Result =
		Model->CreateExecutionHandoffForActivationActionId(TEXT("activate:missing"));

	TestFalse(TEXT("Handoff fails"), Result.bHandoffOk);
	TestFalse(TEXT("Selection unresolved"), Result.bSelectionResolved);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffCoordinatorConvenienceTest, "Wandbound.Runtime.ActivationExecutionHandoff.CoordinatorConvenience", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffCoordinatorConvenienceTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeHandoffCoordinator();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeHandoffModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeHandoffActionSet();
	Coordinator->SetActivationPresentationModel(Model);
	Coordinator->RefreshActivationPresentationFromExternalData(ActionSet, MakeHandoffPublicSummary());

	const FWBRuntimeActivationExecutionHandoffResult Result =
		Coordinator->CreateActivationExecutionHandoff(ActionSet.Actions[0].ActivationActionId);

	TestTrue(TEXT("Handoff succeeds"), Result.bHandoffOk);
	TestEqual(TEXT("Activation action id"), Result.ActivationAction.ActivationActionId, ActionSet.Actions[0].ActivationActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffCoordinatorMissingModelTest, "Wandbound.Runtime.ActivationExecutionHandoff.CoordinatorMissingModel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffCoordinatorMissingModelTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeHandoffCoordinator();
	const FWBRuntimeActivationExecutionHandoffResult Result =
		Coordinator->CreateActivationExecutionHandoff(TEXT("activate:p0:u1:handoff:t2"));

	TestFalse(TEXT("Handoff fails"), Result.bHandoffOk);
	TestFalse(TEXT("Selection unresolved"), Result.bSelectionResolved);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_presentation_model_missing")));
	TestFalse(TEXT("Execution not attempted"), Result.bExecutionAttempted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffOwnerConvenienceTest, "Wandbound.Runtime.ActivationExecutionHandoff.OwnerConvenience", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffOwnerConvenienceTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeHandoffOwner();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeHandoffCoordinator();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeHandoffModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeHandoffActionSet();
	Coordinator->SetActivationPresentationModel(Model);
	Owner->SetDecisionPointCoordinator(Coordinator);
	Owner->RefreshActivationPresentationFromExternalData(ActionSet, MakeHandoffPublicSummary());

	const FWBRuntimeActivationExecutionHandoffResult Result =
		Owner->CreateActivationExecutionHandoff(ActionSet.Actions[0].ActivationActionId);

	TestTrue(TEXT("Handoff succeeds"), Result.bHandoffOk);
	TestEqual(TEXT("Activation action id"), Result.ActivationAction.ActivationActionId, ActionSet.Actions[0].ActivationActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffOwnerMissingCoordinatorTest, "Wandbound.Runtime.ActivationExecutionHandoff.OwnerMissingCoordinator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffOwnerMissingCoordinatorTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeHandoffOwner();
	const FWBRuntimeActivationExecutionHandoffResult Result =
		Owner->CreateActivationExecutionHandoff(TEXT("activate:p0:u1:handoff:t2"));

	TestFalse(TEXT("Handoff fails"), Result.bHandoffOk);
	TestFalse(TEXT("Selection unresolved"), Result.bSelectionResolved);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("decision_point_coordinator_missing")));
	TestFalse(TEXT("Execution not attempted"), Result.bExecutionAttempted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffHiddenMetadataTest, "Wandbound.Runtime.ActivationExecutionHandoff.HiddenMetadataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBCardActivationLegalAction Action = MakeHandoffAction();
	Action.Candidate.SourceEffectId = TEXT("SECRET_HANDOFF_SOURCE_EFFECT");
	Action.Command.Source.SourceEffectId = TEXT("SECRET_HANDOFF_COMMAND_EFFECT");
	Action.Command.DebugActivationId = TEXT("SECRET_HANDOFF_DEBUG");
	Action.Command.UsageCommit.UsageKey = TEXT("SECRET_HANDOFF_USAGE");
	Action.Command.CostPaymentCommit.CostKind = FName(TEXT("SECRET_HANDOFF_COST"));

	UWBRuntimeActivationPresentationModelComponent* Model = MakeHandoffModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeHandoffActionSet(Action);
	RefreshHandoffModel(Model, ActionSet);

	const FWBRuntimeActivationExecutionHandoffResult Result =
		Model->CreateExecutionHandoffForActivationActionId(Action.ActivationActionId);
	const FString PublicText = CombineHandoffPublicText(Result);

	TestTrue(TEXT("Handoff succeeds"), Result.bHandoffOk);
	TestEqual(TEXT("Internal debug retained"), Result.ActivationAction.Command.DebugActivationId, FString(TEXT("SECRET_HANDOFF_DEBUG")));
	const TArray<FString> Forbidden = {
		TEXT("SECRET_HANDOFF_SOURCE_EFFECT"),
		TEXT("SECRET_HANDOFF_COMMAND_EFFECT"),
		TEXT("SECRET_HANDOFF_DEBUG"),
		TEXT("SECRET_HANDOFF_USAGE"),
		TEXT("SECRET_HANDOFF_COST"),
		TEXT("EffectRunner"),
		TEXT("WBRules"),
		TEXT("schema"),
		TEXT("hook")
	};
	for (const FString& Token : Forbidden)
	{
		TestFalse(*FString::Printf(TEXT("Public text excludes %s"), *Token), PublicText.Contains(Token, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("Visible source card remains public"), PublicText.Contains(TEXT("visible_handoff_source")));
	TestTrue(TEXT("Visible target card remains public"), PublicText.Contains(TEXT("visible_handoff_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffSourceGuardTest, "Wandbound.Runtime.ActivationExecutionHandoff.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Handoff source loads"), LoadHandoffSource(Source));
	TestFalse(TEXT("Does not call WBEffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Does not execute activation command"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	TestFalse(TEXT("Does not call WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call candidate generator"), Source.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("Does not call legal action generator"), Source.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("Does not inspect game state"), Source.Contains(TEXT("FWBGameStateData")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionHandoffSeparateFromFWBActionTest, "Wandbound.Runtime.ActivationExecutionHandoff.SeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionHandoffSeparateFromFWBActionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const TArray<FString> BeforeActionIds = NormalActionIdsForPlayer(State, 0);
	const FWBRuntimeActivationExecutionHandoffResult Result =
		WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(MakeResolvedHandoffSelection());
	const TArray<FString> AfterActionIds = NormalActionIdsForPlayer(State, 0);

	TestTrue(TEXT("Handoff succeeds"), Result.bHandoffOk);
	CompareHandoffStringArrays(
		*this,
		TEXT("FWBAction ids unchanged"),
		AfterActionIds,
		BeforeActionIds);
	for (const FString& ActionId : AfterActionIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction id not activation: %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	FString CodecSource;
	TestTrue(
		TEXT("Codec source loads"),
		FFileHelper::LoadFileToString(
			CodecSource,
			*FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Source"),
				TEXT("WandboundCore"),
				TEXT("Private"),
				TEXT("WBActionCodec.cpp"))));
	TestFalse(TEXT("WBActionCodec has no activation ids"), CodecSource.Contains(TEXT("activate:"), ESearchCase::IgnoreCase));
	return true;
}

#endif
