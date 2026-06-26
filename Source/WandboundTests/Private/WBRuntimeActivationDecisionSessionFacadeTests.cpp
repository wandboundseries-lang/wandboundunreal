#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBReplayTrace.h"
#include "WBRuntimeActivationDecisionSessionFacadeComponent.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 SessionSourceUnitId = 1;
constexpr int32 SessionTargetUnitId = 2;

UWBRuntimeActivationDecisionSessionFacadeComponent* MakeSessionFacade()
{
	return NewObject<UWBRuntimeActivationDecisionSessionFacadeComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointOwnerComponent* MakeSessionOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeSessionCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeSessionTurnModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeActivationPresentationModelComponent* MakeSessionActivationModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

bool MakeSessionPath(
	UWBRuntimeActivationDecisionSessionFacadeComponent*& OutFacade,
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel)
{
	OutFacade = MakeSessionFacade();
	OutOwner = MakeSessionOwner();
	OutCoordinator = MakeSessionCoordinator();
	OutTurnModel = MakeSessionTurnModel();
	OutActivationModel = MakeSessionActivationModel();

	if (OutFacade == nullptr
		|| OutOwner == nullptr
		|| OutCoordinator == nullptr
		|| OutTurnModel == nullptr
		|| OutActivationModel == nullptr)
	{
		return false;
	}

	OutCoordinator->SetTurnInteractionModel(OutTurnModel);
	OutCoordinator->SetActivationPresentationModel(OutActivationModel);
	OutOwner->SetDecisionPointCoordinator(OutCoordinator);
	OutFacade->SetDecisionPointOwner(OutOwner);
	return true;
}

FWBPlayerStateData MakeSessionPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeSessionUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId,
	const int32 HP = 5,
	const int32 RLTotal = 4,
	const int32 RLUsed = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = 5;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeSessionState(
	const int32 SourceRLUsed = 1,
	const int32 TargetHP = 5)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 14;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeSessionPlayer(0, 2));
	State.Players.Add(MakeSessionPlayer(1, 0));
	State.AddUnitForTest(MakeSessionUnit(
		SessionSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("visible_session_source"),
		5,
		4,
		SourceRLUsed));
	State.AddUnitForTest(MakeSessionUnit(
		SessionTargetUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_session_target"),
		TargetHP,
		0,
		0));
	return State;
}

FWBGenericEffectPayload MakeSessionDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.TargetUnitId = -1;
	Payload.DamageEffect.SourceUnitId = -1;
	Payload.DamageEffect.SourcePlayerId = -1;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("runtime_activation_session_test"));
	return Payload;
}

FWBCardActivationCommand MakeSessionCommand(
	const int32 DamageAmount = 1,
	const bool bPayCost = false,
	const int32 RequiredRR = 0,
	const bool bMarkUsage = false,
	const FString& UsageKey = TEXT("runtime_activation_session_usage"),
	const FString& SourceEffectId = TEXT("runtime_activation_session_effect"),
	const FString& DebugActivationId = TEXT("runtime_activation_session_debug"),
	const FName CostKind = FName(TEXT("RR")))
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = SessionSourceUnitId;
	Command.Source.SourceCardId = TEXT("runtime_activation_session_card");
	Command.Source.SourceEffectId = SourceEffectId;
	Command.EffectRequest.Target.TargetUnitId = SessionTargetUnitId;
	Command.EffectRequest.Payloads.Add(MakeSessionDamagePayload(DamageAmount));
	Command.DebugActivationId = DebugActivationId;

	Command.CostPaymentCommit.bPayCostOnSuccess = bPayCost;
	Command.CostPaymentCommit.PlayerId = 0;
	Command.CostPaymentCommit.SourceUnitId = SessionSourceUnitId;
	Command.CostPaymentCommit.RequiredRR = RequiredRR;
	Command.CostPaymentCommit.CostKind = CostKind;

	Command.UsageCommit.bMarkUsageOnSuccess = bMarkUsage;
	Command.UsageCommit.PlayerId = 0;
	Command.UsageCommit.UsageKey = UsageKey;
	return Command;
}

FWBCardActivationLegalAction MakeSessionActivationAction(
	const FString& ActivationActionId = TEXT("activate:p0:u1:session:t2"),
	const FString& PublicLabel = TEXT("Session Activation"),
	const bool bWithCommand = true)
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActivationActionId;
	Action.PublicLabel = PublicLabel;
	Action.PlayerId = 0;
	Action.SourceUnitId = SessionSourceUnitId;
	Action.Target.TargetUnitId = SessionTargetUnitId;
	if (bWithCommand)
	{
		Action.Command = MakeSessionCommand();
	}
	return Action;
}

FWBCardActivationLegalActionSet MakeSessionActivationActionSet(
	const TArray<FWBCardActivationLegalAction>& Actions)
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions = Actions;
	return Set;
}

FWBCardActivationLegalActionSet MakeSessionActivationActionSet(
	const FWBCardActivationLegalAction& Action)
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);
	return Set;
}

FWBAction MakeSessionMoveAction(const int32 Index = 0)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = SessionSourceUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = Index == 0 ? FWBTile(5, 4) : FWBTile(4, 3);
	return Action;
}

TArray<FWBAction> MakeSessionNormalActions(const int32 Count = 1)
{
	TArray<FWBAction> Actions;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Actions.Add(MakeSessionMoveAction(Index));
	}
	return Actions;
}

FWBPublicBoardSummary MakeSessionSummary(
	const int32 UnitCount = 2,
	const FString& CardPrefix = TEXT("visible_session_public"))
{
	FWBPublicBoardSummary Summary;
	for (int32 Index = 0; Index < UnitCount; ++Index)
	{
		FWBPublicUnitBoardSummary Unit;
		Unit.UnitId = Index == 0 ? SessionSourceUnitId : 80 + Index;
		Unit.OwnerId = Index % 2;
		Unit.CardId = FString::Printf(TEXT("%s_%d"), *CardPrefix, Index);
		Unit.X = Index + 1;
		Unit.Y = Index + 2;
		Summary.Units.Add(Unit);
	}
	return Summary;
}

FWBRuntimeActivationDecisionSessionRefreshInput MakeSessionInput(
	const int32 NormalActionCount = 1,
	const int32 ActivationActionCount = 1,
	const int32 PublicUnitCount = 2)
{
	FWBRuntimeActivationDecisionSessionRefreshInput Input;
	Input.NormalLegalActions = MakeSessionNormalActions(NormalActionCount);
	Input.PublicBoardSummary = MakeSessionSummary(PublicUnitCount);

	TArray<FWBCardActivationLegalAction> ActivationActions;
	for (int32 Index = 0; Index < ActivationActionCount; ++Index)
	{
		ActivationActions.Add(MakeSessionActivationAction(
			Index == 0
				? FString(TEXT("activate:p0:u1:session:t2"))
				: FString::Printf(TEXT("activate:p0:u1:session:post:%d"), Index),
			Index == 0
				? FString(TEXT("Session Activation"))
				: FString::Printf(TEXT("Post Session Activation %d"), Index),
			Index == 0));
	}
	Input.ActivationActionSet = MakeSessionActivationActionSet(ActivationActions);
	return Input;
}

FWBRuntimeActivationDecisionSessionRefreshInput MakeSessionPostInput(
	const int32 NormalActionCount = 2,
	const int32 ActivationActionCount = 2,
	const int32 PublicUnitCount = 1)
{
	FWBRuntimeActivationDecisionSessionRefreshInput Input;
	Input.NormalLegalActions = MakeSessionNormalActions(NormalActionCount);
	Input.PublicBoardSummary = MakeSessionSummary(PublicUnitCount, TEXT("visible_session_post"));

	TArray<FWBCardActivationLegalAction> ActivationActions;
	for (int32 Index = 0; Index < ActivationActionCount; ++Index)
	{
		ActivationActions.Add(MakeSessionActivationAction(
			FString::Printf(TEXT("activate:p0:u1:session:after:%d"), Index),
			FString::Printf(TEXT("After Session Activation %d"), Index),
			false));
	}
	Input.ActivationActionSet = MakeSessionActivationActionSet(ActivationActions);
	return Input;
}

FString CombineSessionActivationPublicText(const UWBRuntimeActivationPresentationModelComponent* Model)
{
	FString Combined;
	if (Model == nullptr)
	{
		return Combined;
	}

	for (const FWBCardActivationLegalActionPresentationEntry& Entry :
		Model->GetCurrentActivationPresentationSnapshot().Entries)
	{
		Combined += Entry.ActivationActionId;
		Combined += Entry.PublicLabel;
		Combined += Entry.SourcePublicCardId;
		Combined += Entry.TargetPublicCardId;
	}

	for (const FWBCardActivationTargetPresentationEntry& Entry :
		Model->GetCurrentTargetPresentationSnapshot().Entries)
	{
		Combined += Entry.ActivationActionId;
		Combined += Entry.PublicActivationLabel;
		Combined += Entry.PublicTargetLabel;
		Combined += Entry.SourcePublicCardId;
		Combined += Entry.TargetPublicCardId;
	}

	return Combined;
}

bool LoadSessionFacadeSource(FString& OutSource)
{
	FString Header;
	FString Source;
	const bool bHeaderLoaded = FFileHelper::LoadFileToString(
		Header,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source"),
			TEXT("WandboundRuntime"),
			TEXT("Public"),
			TEXT("WBRuntimeActivationDecisionSessionFacadeComponent.h")));
	const bool bSourceLoaded = FFileHelper::LoadFileToString(
		Source,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source"),
			TEXT("WandboundRuntime"),
			TEXT("Private"),
			TEXT("WBRuntimeActivationDecisionSessionFacadeComponent.cpp")));
	OutSource = Header + Source;
	return bHeaderLoaded && bSourceLoaded;
}

void ExpectSessionStateUnchanged(
	FAutomationTestBase& Test,
	const FWBGameStateData& State,
	const FWBGameStateData& Before)
{
	Test.TestEqual(TEXT("Source RLUsed unchanged"), State.GetUnitById(SessionSourceUnitId)->RLUsed, Before.GetUnitById(SessionSourceUnitId)->RLUsed);
	Test.TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(SessionTargetUnitId)->HP, Before.GetUnitById(SessionTargetUnitId)->HP);
	Test.TestEqual(TEXT("Usage key count unchanged"), State.ActivationUsageKeysThisTurn.Num(), Before.ActivationUsageKeysThisTurn.Num());
}

TArray<FString> NormalActionIdsForSessionState(const FWBGameStateData& State)
{
	TArray<FString> ActionIds;
	for (const FWBAction& Action : WBRules::GenerateLegalActionsForPlayer(State, 0))
	{
		ActionIds.Add(WBActionCodec::MakeActionId(Action));
	}
	return ActionIds;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeClassExistsTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Facade class"), UWBRuntimeActivationDecisionSessionFacadeComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeInitialStateTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeInitialStateTest::RunTest(const FString& Parameters)
{
	const UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = MakeSessionFacade();
	TestNotNull(TEXT("Facade"), Facade);
	if (Facade == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("No session state"), Facade->HasSessionState());
	TestFalse(TEXT("Status not ready"), Facade->GetCurrentStatus().bReady);
	TestFalse(TEXT("No last selection"), Facade->GetCurrentStatus().bHasLastActivationSelection);
	TestFalse(TEXT("No last execution"), Facade->GetCurrentStatus().bHasLastActivationExecution);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeMissingOwnerRefreshTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.MissingOwnerRefreshFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeMissingOwnerRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = MakeSessionFacade();
	TestNotNull(TEXT("Facade"), Facade);

	const FWBRuntimeActivationDecisionSessionRefreshResult Result =
		Facade->RefreshSessionFromExternalData(MakeSessionInput());

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("decision_point_owner_missing")));
	TestFalse(TEXT("No session state"), Facade->HasSessionState());
	ExpectSessionStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeRefreshSucceedsTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.RefreshSessionFromExternalDataSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeRefreshSucceedsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeSessionInput(2, 2, 2);
	const FWBRuntimeActivationDecisionSessionRefreshResult Result =
		Facade->RefreshSessionFromExternalData(Input);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestTrue(TEXT("Facade has state"), Facade->HasSessionState());
	TestTrue(TEXT("Status ready"), Result.Status.bReady);
	TestTrue(TEXT("Normal ready"), Result.Status.bNormalDecisionPointReady);
	TestTrue(TEXT("Activation ready"), Result.Status.bActivationPresentationReady);
	TestEqual(TEXT("Normal action count"), Result.Status.NormalLegalActionCount, Input.NormalLegalActions.Num());
	TestEqual(TEXT("Normal presentation count"), Result.Status.NormalPresentationEntryCount, Input.NormalLegalActions.Num());
	TestEqual(TEXT("Activation action count"), Result.Status.ActivationActionCount, Input.ActivationActionSet.Actions.Num());
	TestEqual(TEXT("Activation presentation count"), Result.Status.ActivationPresentationEntryCount, Input.ActivationActionSet.Actions.Num());
	TestEqual(TEXT("Activation target count"), Result.Status.ActivationTargetPresentationEntryCount, Input.ActivationActionSet.Actions.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeSourceGuardTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.SourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Facade source loads"), LoadSessionFacadeSource(Source));
	TestFalse(TEXT("No legal generation"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No activation candidate generator"), Source.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("No activation legal action generator"), Source.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("No effect runner direct call"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No ApplyCardActivationCommand"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	TestFalse(TEXT("No action codec"), Source.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No public summary build"), Source.Contains(TEXT("WBPublicBoardSummary::Build")));
	TestFalse(TEXT("No board summary bridge"), Source.Contains(TEXT("WBBoardSummaryBridge")));
	TestTrue(TEXT("External state by reference only"), Source.Contains(TEXT("FWBGameStateData& State")));
	TestFalse(TEXT("No game state pointer storage"), Source.Contains(TEXT("FWBGameStateData*")));
	TestFalse(TEXT("No cached game state"), Source.Contains(TEXT("FWBGameStateData Cached")));
	TestFalse(TEXT("No stored game state"), Source.Contains(TEXT("FWBGameStateData Stored")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(
		TEXT("No hand access"),
		Source.Contains(TEXT(".Hand."))
			|| Source.Contains(TEXT("->Hand."))
			|| Source.Contains(TEXT(".Hand["))
			|| Source.Contains(TEXT("->Hand["))
			|| Source.Contains(TEXT(".Hand ="))
			|| Source.Contains(TEXT("->Hand =")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeResolveDelegatesTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.ResolveActivationActionIdDelegates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeResolveDelegatesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput()).bOk);

	const FWBRuntimeActivationSelectionResolution Result =
		Facade->ResolveActivationActionId(TEXT("activate:p0:u1:session:t2"));

	TestTrue(TEXT("Resolve ok"), Result.bOk);
	TestTrue(TEXT("Has activation entry"), Result.bHasActivationPresentationEntry);
	TestTrue(TEXT("Has target entry"), Result.bHasTargetPresentationEntry);
	TestFalse(TEXT("No last selection stored by const resolver"), Facade->GetCurrentStatus().bHasLastActivationSelection);
	ExpectSessionStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeResolveMissingTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.ResolveMissingActivationActionIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeResolveMissingTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput()).bOk);

	const FWBRuntimeActivationSelectionResolution Result =
		Facade->ResolveActivationActionId(TEXT("activate:missing"));

	TestFalse(TEXT("Resolve fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeHandoffDelegatesTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.CreateActivationExecutionHandoffDelegates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeHandoffDelegatesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput()).bOk);

	const FWBRuntimeActivationExecutionHandoffResult Result =
		Facade->CreateActivationExecutionHandoff(TEXT("activate:p0:u1:session:t2"));

	TestTrue(TEXT("Handoff ok"), Result.bHandoffOk);
	TestTrue(TEXT("Selection resolved"), Result.bSelectionResolved);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_execution_not_implemented")));
	TestFalse(TEXT("No execution attempted"), Result.bExecutionAttempted);
	ExpectSessionStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeExecuteRefreshTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.ExecuteActivationAndRefreshSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeExecuteRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput()).bOk);

	const FWBRuntimeActivationDecisionSessionRefreshInput PostInput = MakeSessionPostInput(2, 3, 1);
	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			TEXT("activate:p0:u1:session:t2"),
			PostInput);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestTrue(TEXT("Selection ok"), Result.SelectionResult.bOk);
	TestTrue(TEXT("Handoff ok"), Result.HandoffResult.bHandoffOk);
	TestTrue(TEXT("Post refresh ok"), Result.PostActivationRefreshResult.bOk);
	TestEqual(TEXT("Target damaged"), State.GetUnitById(SessionTargetUnitId)->HP, 4);
	TestEqual(TEXT("Status normal count from post input"), Result.Status.NormalLegalActionCount, PostInput.NormalLegalActions.Num());
	TestEqual(TEXT("Status public count from post input"), Owner->GetCurrentStatus().PublicUnitCount, PostInput.PublicBoardSummary.Units.Num());
	TestEqual(TEXT("Status activation count from post input"), Result.Status.ActivationActionCount, PostInput.ActivationActionSet.Actions.Num());
	TestTrue(TEXT("Last selection stored"), Result.Status.bHasLastActivationSelection);
	TestTrue(TEXT("Last execution stored"), Result.Status.bHasLastActivationExecution);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeExecutionFailureNoRefreshTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.ExecutionFailureDoesNotRefreshByDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeExecutionFailureNoRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput(1, 1, 2)).bOk);
	const FWBRuntimeActivationDecisionSessionStatus BeforeStatus = Facade->GetCurrentStatus();

	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			TEXT("activate:missing"),
			MakeSessionPostInput(3, 3, 1));

	TestFalse(TEXT("Execution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	TestFalse(TEXT("Post refresh not attempted"), Result.PostActivationRefreshResult.bPostActionRefreshAttempted);
	TestEqual(TEXT("Normal count unchanged"), Result.Status.NormalLegalActionCount, BeforeStatus.NormalLegalActionCount);
	TestEqual(TEXT("Activation count unchanged"), Result.Status.ActivationActionCount, BeforeStatus.ActivationActionCount);
	ExpectSessionStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeFailureRefreshOptionTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.FailureRefreshOptionUsesSequencer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeFailureRefreshOptionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput(1, 1, 2)).bOk);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnFailure = true;
	Options.bRefreshActivationPresentationOnFailure = true;
	const FWBRuntimeActivationDecisionSessionRefreshInput PostInput = MakeSessionPostInput(3, 2, 1);
	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			TEXT("activate:missing"),
			PostInput,
			Options);

	TestFalse(TEXT("Execution still fails"), Result.bOk);
	TestTrue(TEXT("Post refresh attempted"), Result.PostActivationRefreshResult.bPostActionRefreshAttempted);
	TestTrue(TEXT("Normal refreshed"), Result.PostActivationRefreshResult.bNormalDecisionPointRefreshed);
	TestTrue(TEXT("Activation refreshed"), Result.PostActivationRefreshResult.bActivationPresentationRefreshed);
	TestEqual(TEXT("Normal count from supplied post data"), Result.Status.NormalLegalActionCount, PostInput.NormalLegalActions.Num());
	TestEqual(TEXT("Activation count from supplied post data"), Result.Status.ActivationActionCount, PostInput.ActivationActionSet.Actions.Num());
	ExpectSessionStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeSuppliedSummaryTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.UsesSuppliedPostSummaryNotState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeSuppliedSummaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput()).bOk);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimeActivationDecisionSessionRefreshInput PostInput = MakeSessionPostInput(1, 0, 1);
	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			TEXT("activate:p0:u1:session:t2"),
			PostInput,
			Options);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestEqual(TEXT("State still has two units"), State.Units.Num(), 2);
	TestEqual(TEXT("Owner status uses supplied public summary count"), Owner->GetCurrentStatus().PublicUnitCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeClearTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.ClearSessionClearsFacadeOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeClearTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput()).bOk);

	Facade->ClearSession();

	TestFalse(TEXT("Facade state cleared"), Facade->HasSessionState());
	TestFalse(TEXT("Facade status not ready"), Facade->GetCurrentStatus().bReady);
	TestTrue(TEXT("Owner normal state remains"), Owner->HasCurrentDecisionPoint());
	TestTrue(TEXT("Activation model state remains"), ActivationModel->HasCurrentActivationPresentationState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeHiddenMetadataTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.HiddenMetadataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FString SecretSourceEffect = TEXT("SECRET_SESSION_SOURCE_EFFECT_DO_NOT_LEAK");
	const FString SecretUsageKey = TEXT("SECRET_SESSION_USAGE_KEY_DO_NOT_LEAK");
	const FString SecretDebugId = TEXT("SECRET_SESSION_DEBUG_ID_DO_NOT_LEAK");
	const FString SecretCostKind = TEXT("SECRET_SESSION_COST_KIND_DO_NOT_LEAK");

	FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeSessionInput();
	Input.ActivationActionSet.Actions[0].PublicLabel = TEXT("Safe Session Activation");
	Input.ActivationActionSet.Actions[0].Command = MakeSessionCommand(
		1,
		false,
		0,
		true,
		SecretUsageKey,
		SecretSourceEffect,
		SecretDebugId,
		FName(*SecretCostKind));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(Input).bOk);

	FWBRuntimeActivationDecisionSessionRefreshInput PostInput = MakeSessionPostInput(1, 1, 2);
	PostInput.ActivationActionSet.Actions[0].PublicLabel = TEXT("Safe Post Session Activation");
	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			Input.ActivationActionSet.Actions[0].ActivationActionId,
			PostInput);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	const FString PublicText = CombineSessionActivationPublicText(ActivationModel);
	const FString SerializedTrace = WBReplayTrace::SerializeEvents(
		Result.PostActivationRefreshResult.ActivationExecutionResult.ActivationResult.TraceEvents);
	const FString ReasonText = Result.Reason
		+ Result.Status.LastReason
		+ Result.SelectionResult.Reason
		+ Result.HandoffResult.Reason;

	for (const FString& Token : { SecretSourceEffect, SecretUsageKey, SecretDebugId, SecretCostKind })
	{
		TestFalse(*FString::Printf(TEXT("Public text excludes %s"), *Token), PublicText.Contains(Token, ESearchCase::IgnoreCase));
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Token), SerializedTrace.Contains(Token, ESearchCase::IgnoreCase));
		TestFalse(*FString::Printf(TEXT("Reason excludes %s"), *Token), ReasonText.Contains(Token, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("Safe post label present"), PublicText.Contains(TEXT("Safe Post Session Activation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDecisionSessionFacadeActionBoundaryTest, "Wandbound.Runtime.ActivationDecisionSessionFacade.ActivationRemainsSeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDecisionSessionFacadeActionBoundaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSessionState();
	const TArray<FString> LegalActionIdsBefore = NormalActionIdsForSessionState(State);
	const FString MoveCodecIdBefore = WBActionCodec::MakeActionId(MakeSessionMoveAction());

	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeSessionInput()).bOk);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnSuccess = false;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			TEXT("activate:p0:u1:session:t2"),
			MakeSessionPostInput(),
			Options);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	const TArray<FString> LegalActionIdsAfter = NormalActionIdsForSessionState(State);
	TestEqual(TEXT("Normal action count unchanged"), LegalActionIdsAfter.Num(), LegalActionIdsBefore.Num());
	for (const FString& ActionId : LegalActionIdsAfter)
	{
		TestFalse(TEXT("Normal action id is not activation"), ActionId.StartsWith(TEXT("activate:")));
	}
	TestEqual(TEXT("Move codec unchanged"), WBActionCodec::MakeActionId(MakeSessionMoveAction()), MoveCodecIdBefore);
	return true;
}

#endif
