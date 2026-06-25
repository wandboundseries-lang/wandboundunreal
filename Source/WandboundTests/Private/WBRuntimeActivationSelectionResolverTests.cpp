#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeActivationSelectionResolver.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 ResolverSourceUnitId = 1;
constexpr int32 ResolverTargetUnitId = 2;

UWBRuntimeActivationPresentationModelComponent* MakeResolverModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeResolverCoordinator()
{
	return NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointOwnerComponent* MakeResolverOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

FWBEffectTargetRef MakeResolverUnitTarget()
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = ResolverTargetUnitId;
	return Target;
}

FWBCardActivationLegalAction MakeResolverAction(
	const FString& ActionId = TEXT("activate:p0:u1:resolver:t2"),
	const FString& PublicLabel = TEXT("Resolver Damage"))
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActionId;
	Action.PublicLabel = PublicLabel;
	Action.PlayerId = 0;
	Action.SourceUnitId = ResolverSourceUnitId;
	Action.Target = MakeResolverUnitTarget();
	Action.Command.Source.PlayerId = 0;
	Action.Command.Source.SourceUnitId = ResolverSourceUnitId;
	Action.Command.Source.SourceCardId = TEXT("internal_resolver_source_card");
	Action.Command.Source.SourceEffectId = TEXT("internal_resolver_effect");
	Action.Command.DebugActivationId = TEXT("internal_resolver_debug");
	return Action;
}

FWBCardActivationLegalActionSet MakeResolverActionSet()
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(MakeResolverAction());
	return Set;
}

FWBCardActivationLegalActionSet MakeResolverDuplicateActionSet()
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(MakeResolverAction(TEXT("activate:duplicate"), TEXT("First Duplicate")));
	Set.Actions.Add(MakeResolverAction(TEXT("activate:duplicate"), TEXT("Second Duplicate")));
	return Set;
}

FWBPublicUnitBoardSummary MakeResolverPublicUnit(
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

FWBPublicBoardSummary MakeResolverPublicSummary()
{
	FWBPublicBoardSummary Summary;
	Summary.Units.Add(MakeResolverPublicUnit(
		ResolverSourceUnitId,
		0,
		TEXT("visible_resolver_source"),
		FWBTile(4, 4)));
	Summary.Units.Add(MakeResolverPublicUnit(
		ResolverTargetUnitId,
		1,
		TEXT("visible_resolver_target"),
		FWBTile(5, 4)));
	return Summary;
}

FWBPlayerStateData MakeResolverPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	return Player;
}

FWBUnitState MakeResolverUnit(
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
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.CurrentArmor = 1;
	Unit.MaxArmor = 2;
	Unit.RLTotal = 4;
	Unit.RLUsed = 1;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeResolverState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 7;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeResolverPlayer(0));
	State.Players.Add(MakeResolverPlayer(1));
	State.AddUnitForTest(MakeResolverUnit(
		ResolverSourceUnitId,
		0,
		TEXT("visible_resolver_source"),
		FWBTile(4, 4)));
	State.AddUnitForTest(MakeResolverUnit(
		ResolverTargetUnitId,
		1,
		TEXT("visible_resolver_target"),
		FWBTile(5, 4)));
	return State;
}

void RefreshResolverModel(
	UWBRuntimeActivationPresentationModelComponent* Model,
	const FWBCardActivationLegalActionSet& ActionSet = MakeResolverActionSet())
{
	if (Model != nullptr)
	{
		Model->SetCurrentActivationPresentationState(
			ActionSet,
			MakeResolverPublicSummary());
	}
}

FString CombineResolverPresentationText(
	const FWBCardActivationLegalActionPresentationEntry& ActivationEntry,
	const FWBCardActivationTargetPresentationEntry& TargetEntry)
{
	FString Text;
	Text += ActivationEntry.ActivationActionId;
	Text += ActivationEntry.PublicLabel;
	Text += ActivationEntry.SourcePublicCardId;
	Text += ActivationEntry.TargetPublicCardId;
	Text += TargetEntry.ActivationActionId;
	Text += TargetEntry.PublicActivationLabel;
	Text += TargetEntry.PublicTargetLabel;
	Text += TargetEntry.SourcePublicCardId;
	Text += TargetEntry.TargetPublicCardId;
	return Text;
}

bool LoadResolverSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Public"),
		TEXT("WBRuntimeActivationSelectionResolver.h"));
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Private"),
		TEXT("WBRuntimeActivationSelectionResolver.cpp"));

	FString Header;
	FString Source;
	if (!FFileHelper::LoadFileToString(Header, *HeaderPath)
		|| !FFileHelper::LoadFileToString(Source, *SourcePath))
	{
		return false;
	}

	OutSource = Header + Source;
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverNullModelTest, "Wandbound.Runtime.ActivationSelectionResolver.NullModelFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverNullModelTest::RunTest(const FString& Parameters)
{
	const FWBRuntimeActivationSelectionResolution Result =
		WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
			nullptr,
			TEXT("activate:p0:u1:resolver:t2"));

	TestFalse(TEXT("Resolution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_presentation_model_missing")));
	TestEqual(TEXT("Selected id retained"), Result.SelectedActivationActionId, FString(TEXT("activate:p0:u1:resolver:t2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverNoStateTest, "Wandbound.Runtime.ActivationSelectionResolver.NoCurrentStateFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverNoStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBRuntimeActivationSelectionResolution Result =
		WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
			Model,
			TEXT("activate:p0:u1:resolver:t2"));

	TestFalse(TEXT("Resolution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("no_current_activation_presentation_state")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverMissingIdTest, "Wandbound.Runtime.ActivationSelectionResolver.MissingIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverMissingIdTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	RefreshResolverModel(Model);

	const FWBRuntimeActivationSelectionResolution Result =
		WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
			Model,
			TEXT("activate:missing"));

	TestFalse(TEXT("Resolution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverAmbiguousIdTest, "Wandbound.Runtime.ActivationSelectionResolver.AmbiguousIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverAmbiguousIdTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	RefreshResolverModel(Model, MakeResolverDuplicateActionSet());

	const FWBRuntimeActivationSelectionResolution Result =
		WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
			Model,
			TEXT("activate:duplicate"));

	TestFalse(TEXT("Resolution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_action_id_ambiguous")));
	TestFalse(TEXT("No activation presentation on ambiguous action"), Result.bHasActivationPresentationEntry);
	TestFalse(TEXT("No target presentation on ambiguous action"), Result.bHasTargetPresentationEntry);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverSuccessActionTest, "Wandbound.Runtime.ActivationSelectionResolver.SuccessReturnsActivationAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverSuccessActionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeResolverActionSet();
	RefreshResolverModel(Model, ActionSet);

	const FWBRuntimeActivationSelectionResolution Result =
		WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
			Model,
			ActionSet.Actions[0].ActivationActionId);

	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestEqual(TEXT("Activation action id"), Result.ActivationAction.ActivationActionId, ActionSet.Actions[0].ActivationActionId);
	TestEqual(TEXT("Command retained internally"), Result.ActivationAction.Command.DebugActivationId, FString(TEXT("internal_resolver_debug")));
	TestEqual(TEXT("Command source retained internally"), Result.ActivationAction.Command.Source.SourceEffectId, FString(TEXT("internal_resolver_effect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverSuccessPresentationTest, "Wandbound.Runtime.ActivationSelectionResolver.SuccessIncludesPresentationEntries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverSuccessPresentationTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeResolverActionSet();
	RefreshResolverModel(Model, ActionSet);

	const FWBRuntimeActivationSelectionResolution Result =
		Model->ResolveSelectedActivationActionId(ActionSet.Actions[0].ActivationActionId);

	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestTrue(TEXT("Has activation presentation"), Result.bHasActivationPresentationEntry);
	TestEqual(TEXT("Activation label"), Result.ActivationPresentationEntry.PublicLabel, FString(TEXT("Resolver Damage")));
	TestTrue(TEXT("Has target presentation"), Result.bHasTargetPresentationEntry);
	TestEqual(TEXT("Target kind"), Result.TargetPresentationEntry.TargetKind, EWBCardActivationTargetPresentationKind::Unit);
	TestEqual(TEXT("Target label"), Result.TargetPresentationEntry.PublicTargetLabel, FString(TEXT("Choose Unit")));
	TestEqual(TEXT("Source public card"), Result.TargetPresentationEntry.SourcePublicCardId, FString(TEXT("visible_resolver_source")));
	TestEqual(TEXT("Target public card"), Result.TargetPresentationEntry.TargetPublicCardId, FString(TEXT("visible_resolver_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverNoExecutionTest, "Wandbound.Runtime.ActivationSelectionResolver.DoesNotExecuteActivation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverNoExecutionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeResolverState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeResolverActionSet();
	RefreshResolverModel(Model, ActionSet);

	const FWBRuntimeActivationSelectionResolution Result =
		Model->ResolveSelectedActivationActionId(ActionSet.Actions[0].ActivationActionId);

	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestEqual(TEXT("Source x unchanged"), State.GetUnitById(ResolverSourceUnitId)->X, Before.GetUnitById(ResolverSourceUnitId)->X);
	TestEqual(TEXT("Source y unchanged"), State.GetUnitById(ResolverSourceUnitId)->Y, Before.GetUnitById(ResolverSourceUnitId)->Y);
	TestEqual(TEXT("Source RLUsed unchanged"), State.GetUnitById(ResolverSourceUnitId)->RLUsed, Before.GetUnitById(ResolverSourceUnitId)->RLUsed);
	TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(ResolverTargetUnitId)->HP, Before.GetUnitById(ResolverTargetUnitId)->HP);
	TestEqual(TEXT("Target armor unchanged"), State.GetUnitById(ResolverTargetUnitId)->CurrentArmor, Before.GetUnitById(ResolverTargetUnitId)->CurrentArmor);
	TestEqual(TEXT("Target status count unchanged"), State.GetUnitById(ResolverTargetUnitId)->Statuses.Num(), Before.GetUnitById(ResolverTargetUnitId)->Statuses.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverCoordinatorDelegateTest, "Wandbound.Runtime.ActivationSelectionResolver.CoordinatorDelegate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverCoordinatorDelegateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeResolverCoordinator();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	const FWBCardActivationLegalActionSet ActionSet = MakeResolverActionSet();
	Coordinator->SetActivationPresentationModel(Model);
	Coordinator->RefreshActivationPresentationFromExternalData(ActionSet, MakeResolverPublicSummary());

	const FWBRuntimeActivationSelectionResolution Result =
		Coordinator->ResolveSelectedActivationActionId(ActionSet.Actions[0].ActivationActionId);
	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestEqual(TEXT("Activation action id"), Result.ActivationAction.ActivationActionId, ActionSet.Actions[0].ActivationActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverCoordinatorMissingModelTest, "Wandbound.Runtime.ActivationSelectionResolver.CoordinatorMissingModelFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverCoordinatorMissingModelTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeResolverCoordinator();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	if (Coordinator == nullptr)
	{
		return false;
	}

	const FWBRuntimeActivationSelectionResolution Result =
		Coordinator->ResolveSelectedActivationActionId(TEXT("activate:p0:u1:resolver:t2"));
	TestFalse(TEXT("Resolution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_presentation_model_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverOwnerDelegateTest, "Wandbound.Runtime.ActivationSelectionResolver.OwnerDelegate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverOwnerDelegateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeResolverOwner();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeResolverCoordinator();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	TestNotNull(TEXT("Owner"), Owner);
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Owner == nullptr || Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	const FWBCardActivationLegalActionSet ActionSet = MakeResolverActionSet();
	Coordinator->SetActivationPresentationModel(Model);
	Owner->SetDecisionPointCoordinator(Coordinator);
	Owner->RefreshActivationPresentationFromExternalData(ActionSet, MakeResolverPublicSummary());

	const FWBRuntimeActivationSelectionResolution Result =
		Owner->ResolveSelectedActivationActionId(ActionSet.Actions[0].ActivationActionId);
	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestEqual(TEXT("Activation action id"), Result.ActivationAction.ActivationActionId, ActionSet.Actions[0].ActivationActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverOwnerMissingCoordinatorTest, "Wandbound.Runtime.ActivationSelectionResolver.OwnerMissingCoordinatorFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverOwnerMissingCoordinatorTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeResolverOwner();
	TestNotNull(TEXT("Owner"), Owner);
	if (Owner == nullptr)
	{
		return false;
	}

	const FWBRuntimeActivationSelectionResolution Result =
		Owner->ResolveSelectedActivationActionId(TEXT("activate:p0:u1:resolver:t2"));
	TestFalse(TEXT("Resolution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("decision_point_coordinator_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverHiddenMetadataTest, "Wandbound.Runtime.ActivationSelectionResolver.HiddenMetadataExcludedFromPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBCardActivationLegalAction Action = MakeResolverAction();
	Action.Candidate.SourceEffectId = TEXT("SECRET_RESOLVER_SOURCE_EFFECT");
	Action.Command.DebugActivationId = TEXT("SECRET_RESOLVER_DEBUG");
	Action.Command.UsageCommit.UsageKey = TEXT("SECRET_RESOLVER_USAGE");
	Action.Command.CostPaymentCommit.CostKind = FName(TEXT("SECRET_RESOLVER_COST"));
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);

	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	RefreshResolverModel(Model, Set);
	const FWBRuntimeActivationSelectionResolution Result =
		Model->ResolveSelectedActivationActionId(Action.ActivationActionId);

	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestTrue(TEXT("Internal command retains debug metadata"), Result.ActivationAction.Command.DebugActivationId == TEXT("SECRET_RESOLVER_DEBUG"));
	TestTrue(TEXT("Has activation presentation"), Result.bHasActivationPresentationEntry);
	TestTrue(TEXT("Has target presentation"), Result.bHasTargetPresentationEntry);

	const FString PublicText = CombineResolverPresentationText(
		Result.ActivationPresentationEntry,
		Result.TargetPresentationEntry);
	const TArray<FString> Forbidden = {
		TEXT("SECRET_RESOLVER_SOURCE_EFFECT"),
		TEXT("SECRET_RESOLVER_DEBUG"),
		TEXT("SECRET_RESOLVER_USAGE"),
		TEXT("SECRET_RESOLVER_COST")
	};
	for (const FString& Token : Forbidden)
	{
		TestFalse(*FString::Printf(TEXT("Public presentation excludes %s"), *Token), PublicText.Contains(Token, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("Public source card remains visible"), PublicText.Contains(TEXT("visible_resolver_source")));
	TestTrue(TEXT("Public target card remains visible"), PublicText.Contains(TEXT("visible_resolver_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverSourceGuardTest, "Wandbound.Runtime.ActivationSelectionResolver.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Resolver source loads"), LoadResolverSource(Source));
	TestFalse(TEXT("Does not call WBEffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Does not call WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call candidate generator"), Source.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("Does not call legal action generator"), Source.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("Does not inspect game state"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("Does not execute activation command"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSelectionResolverSeparateFromFWBActionTest, "Wandbound.Runtime.ActivationSelectionResolver.SeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSelectionResolverSeparateFromFWBActionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeResolverState();
	const TArray<FWBAction> BeforeActions = WBRules::GenerateLegalActionsForPlayer(State, 0);

	UWBRuntimeActivationPresentationModelComponent* Model = MakeResolverModel();
	const FWBCardActivationLegalActionSet ActionSet = MakeResolverActionSet();
	RefreshResolverModel(Model, ActionSet);
	const FWBRuntimeActivationSelectionResolution Result =
		Model->ResolveSelectedActivationActionId(ActionSet.Actions[0].ActivationActionId);
	TestTrue(TEXT("Resolution succeeds"), Result.bOk);

	const TArray<FWBAction> AfterActions = WBRules::GenerateLegalActionsForPlayer(State, 0);
	TestEqual(TEXT("FWBAction count unchanged"), AfterActions.Num(), BeforeActions.Num());
	const int32 NumToCompare = FMath::Min(BeforeActions.Num(), AfterActions.Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		const FString BeforeId = WBActionCodec::MakeActionId(BeforeActions[Index]);
		const FString AfterId = WBActionCodec::MakeActionId(AfterActions[Index]);
		TestEqual(*FString::Printf(TEXT("FWBAction id %d unchanged"), Index), AfterId, BeforeId);
		TestFalse(*FString::Printf(TEXT("FWBAction id %d not activation"), Index), AfterId.StartsWith(TEXT("activate:")));
	}

	FString CodecSource;
	TestTrue(
		TEXT("Codec source loads"),
		FFileHelper::LoadFileToString(
			CodecSource,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"))));
	TestFalse(TEXT("WBActionCodec has no activation ids"), CodecSource.Contains(TEXT("activate:"), ESearchCase::IgnoreCase));
	return true;
}

#endif
