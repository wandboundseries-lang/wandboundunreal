#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBReplayTrace.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimePostActivationRefreshSequencer.h"
#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 SequencerSourceUnitId = 1;
constexpr int32 SequencerTargetUnitId = 2;

UWBRuntimeDecisionPointOwnerComponent* MakeSequencerOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeSequencerCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeSequencerTurnModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeActivationPresentationModelComponent* MakeSequencerActivationModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

bool MakeSequencerOwnerPath(
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel,
	const bool bWithTurnModel = true,
	const bool bWithActivationModel = true)
{
	OutOwner = MakeSequencerOwner();
	OutCoordinator = MakeSequencerCoordinator();
	OutTurnModel = bWithTurnModel ? MakeSequencerTurnModel() : nullptr;
	OutActivationModel = bWithActivationModel ? MakeSequencerActivationModel() : nullptr;

	if (OutOwner == nullptr || OutCoordinator == nullptr)
	{
		return false;
	}

	if (OutTurnModel != nullptr)
	{
		OutCoordinator->SetTurnInteractionModel(OutTurnModel);
	}

	if (OutActivationModel != nullptr)
	{
		OutCoordinator->SetActivationPresentationModel(OutActivationModel);
	}

	OutOwner->SetDecisionPointCoordinator(OutCoordinator);
	return true;
}

FWBPlayerStateData MakeSequencerPlayer(const int32 PlayerId, const int32 RemainingMP = 1)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeSequencerUnit(
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

FWBGameStateData MakeSequencerState(
	const int32 SourceRLTotal = 4,
	const int32 SourceRLUsed = 1,
	const int32 TargetHP = 5)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 12;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeSequencerPlayer(0, 2));
	State.Players.Add(MakeSequencerPlayer(1, 0));
	State.AddUnitForTest(MakeSequencerUnit(
		SequencerSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("visible_sequencer_source"),
		5,
		SourceRLTotal,
		SourceRLUsed));
	State.AddUnitForTest(MakeSequencerUnit(
		SequencerTargetUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_sequencer_target"),
		TargetHP,
		0,
		0));
	return State;
}

FWBGenericEffectPayload MakeSequencerDamagePayload(
	const int32 Amount,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.TargetUnitId = TargetUnitId;
	Payload.DamageEffect.SourceUnitId = -1;
	Payload.DamageEffect.SourcePlayerId = -1;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("runtime_post_activation_refresh_test"));
	return Payload;
}

FWBCardActivationCommand MakeSequencerCommand(
	const int32 DamageAmount = 1,
	const bool bPayCost = false,
	const int32 RequiredRR = 0,
	const bool bMarkUsage = false,
	const FString& UsageKey = TEXT("runtime_post_activation_usage"),
	const FString& SourceEffectId = TEXT("runtime_post_activation_effect"),
	const FString& DebugActivationId = TEXT("runtime_post_activation_debug"),
	const FName CostKind = FName(TEXT("RR")))
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = SequencerSourceUnitId;
	Command.Source.SourceCardId = TEXT("runtime_post_activation_card");
	Command.Source.SourceEffectId = SourceEffectId;
	Command.EffectRequest.Target.TargetUnitId = SequencerTargetUnitId;
	Command.EffectRequest.Payloads.Add(MakeSequencerDamagePayload(DamageAmount));
	Command.DebugActivationId = DebugActivationId;

	Command.CostPaymentCommit.bPayCostOnSuccess = bPayCost;
	Command.CostPaymentCommit.PlayerId = 0;
	Command.CostPaymentCommit.SourceUnitId = SequencerSourceUnitId;
	Command.CostPaymentCommit.RequiredRR = RequiredRR;
	Command.CostPaymentCommit.CostKind = CostKind;

	Command.UsageCommit.bMarkUsageOnSuccess = bMarkUsage;
	Command.UsageCommit.PlayerId = 0;
	Command.UsageCommit.UsageKey = UsageKey;
	return Command;
}

FWBCardActivationLegalAction MakeSequencerActivationAction(
	const FString& ActivationActionId = TEXT("activate:p0:u1:sequencer:t2"),
	const FString& PublicLabel = TEXT("Sequencer Activation"),
	const bool bWithCommand = true)
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActivationActionId;
	Action.PublicLabel = PublicLabel;
	Action.PlayerId = 0;
	Action.SourceUnitId = SequencerSourceUnitId;
	Action.Target.TargetUnitId = SequencerTargetUnitId;
	if (bWithCommand)
	{
		Action.Command = MakeSequencerCommand();
	}
	return Action;
}

FWBCardActivationLegalActionSet MakeSequencerActivationActionSet(
	const TArray<FWBCardActivationLegalAction>& Actions)
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions = Actions;
	return Set;
}

FWBCardActivationLegalActionSet MakeSequencerActivationActionSet(
	const FWBCardActivationLegalAction& Action)
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);
	return Set;
}

FWBAction MakeSequencerMoveAction(
	const FString& LabelSeed = TEXT(""))
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = SequencerSourceUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = LabelSeed.IsEmpty() ? FWBTile(5, 4) : FWBTile(4, 3);
	return Action;
}

TArray<FWBAction> MakeSequencerPostActions(const int32 Count = 1)
{
	TArray<FWBAction> Actions;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Actions.Add(MakeSequencerMoveAction(Index == 0 ? TEXT("") : TEXT("alternate")));
	}
	return Actions;
}

FWBPublicBoardSummary MakeSequencerSummary(
	const int32 UnitCount = 2,
	const FString& CardPrefix = TEXT("visible_sequencer_post"))
{
	FWBPublicBoardSummary Summary;
	for (int32 Index = 0; Index < UnitCount; ++Index)
	{
		FWBPublicUnitBoardSummary Unit;
		Unit.UnitId = Index == 0 ? SequencerSourceUnitId : 90 + Index;
		Unit.OwnerId = Index % 2;
		Unit.CardId = FString::Printf(TEXT("%s_%d"), *CardPrefix, Index);
		Unit.X = Index;
		Unit.Y = Index + 1;
		Summary.Units.Add(Unit);
	}
	return Summary;
}

FWBRuntimePostActivationRefreshInput MakeSequencerRefreshInput(
	const int32 NormalActionCount = 1,
	const int32 ActivationActionCount = 1,
	const int32 PublicUnitCount = 2)
{
	FWBRuntimePostActivationRefreshInput Input;
	Input.PostActionPrecomputedLegalActions = MakeSequencerPostActions(NormalActionCount);
	Input.PostActionPublicBoardSummary = MakeSequencerSummary(PublicUnitCount);

	TArray<FWBCardActivationLegalAction> ActivationActions;
	for (int32 Index = 0; Index < ActivationActionCount; ++Index)
	{
		ActivationActions.Add(MakeSequencerActivationAction(
			FString::Printf(TEXT("activate:p0:u1:post:%d"), Index),
			FString::Printf(TEXT("Post Activation %d"), Index),
			false));
	}
	Input.PostActionActivationActionSet = MakeSequencerActivationActionSet(ActivationActions);
	return Input;
}

void RefreshInitialActivation(
	UWBRuntimeDecisionPointOwnerComponent* Owner,
	const FWBCardActivationLegalAction& Action)
{
	if (Owner != nullptr)
	{
		Owner->RefreshActivationPresentationFromExternalData(
			MakeSequencerActivationActionSet(Action),
			MakeSequencerSummary());
	}
}

void RefreshInitialNormal(
	UWBRuntimeDecisionPointOwnerComponent* Owner,
	const int32 ActionCount = 1,
	const int32 PublicUnitCount = 2)
{
	if (Owner != nullptr)
	{
		Owner->RefreshDecisionPointFromExternalData(
			MakeSequencerPostActions(ActionCount),
			MakeSequencerSummary(PublicUnitCount));
	}
}

void ExpectSequencerStateUnchanged(
	FAutomationTestBase& Test,
	const FWBGameStateData& State,
	const FWBGameStateData& Before)
{
	Test.TestEqual(TEXT("Source RLUsed unchanged"), State.GetUnitById(SequencerSourceUnitId)->RLUsed, Before.GetUnitById(SequencerSourceUnitId)->RLUsed);
	Test.TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(SequencerTargetUnitId)->HP, Before.GetUnitById(SequencerTargetUnitId)->HP);
	Test.TestEqual(TEXT("Usage key count unchanged"), State.ActivationUsageKeysThisTurn.Num(), Before.ActivationUsageKeysThisTurn.Num());
}

FString CombineSequencerActivationPublicText(const UWBRuntimeActivationPresentationModelComponent* Model)
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

bool LoadSequencerSource(FString& OutSource)
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
			TEXT("WBRuntimePostActivationRefreshSequencer.h")));
	const bool bSourceLoaded = FFileHelper::LoadFileToString(
		Source,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source"),
			TEXT("WandboundRuntime"),
			TEXT("Private"),
			TEXT("WBRuntimePostActivationRefreshSequencer.cpp")));
	OutSource = Header + Source;
	return bHeaderLoaded && bSourceLoaded;
}

bool LoadOwnerSourceForSequencer(FString& OutSource)
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
			TEXT("WBRuntimeDecisionPointOwnerComponent.h")));
	const bool bSourceLoaded = FFileHelper::LoadFileToString(
		Source,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source"),
			TEXT("WandboundRuntime"),
			TEXT("Private"),
			TEXT("WBRuntimeDecisionPointOwnerComponent.cpp")));
	OutSource = Header + Source;
	return bHeaderLoaded && bSourceLoaded;
}

TArray<FString> NormalActionIdsForSequencerState(const FWBGameStateData& State)
{
	TArray<FString> ActionIds;
	for (const FWBAction& Action : WBRules::GenerateLegalActionsForPlayer(State, 0))
	{
		ActionIds.Add(WBActionCodec::MakeActionId(Action));
	}
	return ActionIds;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerNullOwnerTest, "Wandbound.Runtime.PostActivationRefreshSequencer.NullOwnerFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerNullOwnerTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	const FWBGameStateData Before = State;

	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			TEXT("activate:p0:u1:sequencer:t2"),
			nullptr,
			MakeSequencerRefreshInput());

	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("decision_point_owner_missing")));
	TestFalse(TEXT("No execution attempted"), Result.bActivationExecutionAttempted);
	TestFalse(TEXT("No refresh attempted"), Result.bPostActionRefreshAttempted);
	ExpectSequencerStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerNormalRefreshTest, "Wandbound.Runtime.PostActivationRefreshSequencer.SuccessRefreshesNormalDecisionPoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerNormalRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialActivation(Owner, Action);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimePostActivationRefreshInput Input = MakeSequencerRefreshInput(2, 0, 1);
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			Input,
			Options);

	TestTrue(TEXT("Result ok"), Result.bOk);
	TestTrue(TEXT("Execution ok"), Result.bActivationExecutionOk);
	TestTrue(TEXT("Normal refresh attempted"), Result.bPostActionRefreshAttempted);
	TestTrue(TEXT("Normal refreshed"), Result.bNormalDecisionPointRefreshed);
	TestFalse(TEXT("Activation refresh skipped"), Result.bActivationPresentationRefreshed);
	TestEqual(TEXT("Normal action count from supplied data"), Owner->GetCurrentStatus().LegalActionCount, Input.PostActionPrecomputedLegalActions.Num());
	TestEqual(TEXT("Public unit count from supplied summary"), Owner->GetCurrentStatus().PublicUnitCount, Input.PostActionPublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerActivationRefreshTest, "Wandbound.Runtime.PostActivationRefreshSequencer.SuccessRefreshesActivationPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerActivationRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialActivation(Owner, Action);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnSuccess = false;
	const FWBRuntimePostActivationRefreshInput Input = MakeSequencerRefreshInput(0, 3, 2);
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			Input,
			Options);

	TestTrue(TEXT("Result ok"), Result.bOk);
	TestFalse(TEXT("Normal refresh skipped"), Result.bNormalDecisionPointRefreshed);
	TestTrue(TEXT("Activation refreshed"), Result.bActivationPresentationRefreshed);
	TestEqual(TEXT("Activation action count from supplied data"), ActivationModel->GetCurrentActivationPresentationStatus().ActivationActionCount, Input.PostActionActivationActionSet.Actions.Num());
	TestEqual(TEXT("Owner activation status count"), Owner->GetCurrentStatus().ActivationActionCount, Input.PostActionActivationActionSet.Actions.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerDefaultRefreshesBothTest, "Wandbound.Runtime.PostActivationRefreshSequencer.SuccessRefreshesBothByDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerDefaultRefreshesBothTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialActivation(Owner, Action);

	const FWBRuntimePostActivationRefreshInput Input = MakeSequencerRefreshInput(2, 2, 1);
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			Input);

	TestTrue(TEXT("Result ok"), Result.bOk);
	TestTrue(TEXT("Normal refreshed"), Result.bNormalDecisionPointRefreshed);
	TestTrue(TEXT("Activation refreshed"), Result.bActivationPresentationRefreshed);
	TestEqual(TEXT("Normal action count"), Owner->GetCurrentStatus().LegalActionCount, Input.PostActionPrecomputedLegalActions.Num());
	TestEqual(TEXT("Activation action count"), Owner->GetCurrentStatus().ActivationActionCount, Input.PostActionActivationActionSet.Actions.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerSuccessRefreshDisabledTest, "Wandbound.Runtime.PostActivationRefreshSequencer.SuccessWithRefreshDisabledLeavesStalePresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerSuccessRefreshDisabledTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialNormal(Owner, 1, 2);
	RefreshInitialActivation(Owner, Action);
	const FWBRuntimeDecisionPointStatus BeforeStatus = Owner->GetCurrentStatus();

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnSuccess = false;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			MakeSequencerRefreshInput(3, 3, 1),
			Options);

	TestTrue(TEXT("Result ok"), Result.bOk);
	TestFalse(TEXT("No refresh attempted"), Result.bPostActionRefreshAttempted);
	TestEqual(TEXT("Normal count remains stale"), Owner->GetCurrentStatus().LegalActionCount, BeforeStatus.LegalActionCount);
	TestEqual(TEXT("Activation count remains stale"), Owner->GetCurrentStatus().ActivationActionCount, BeforeStatus.ActivationActionCount);
	TestEqual(TEXT("Target HP mutated by activation"), State.GetUnitById(SequencerTargetUnitId)->HP, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerFailureNoRefreshTest, "Wandbound.Runtime.PostActivationRefreshSequencer.FailedActivationDoesNotRefreshByDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerFailureNoRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	const FWBGameStateData Before = State;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	RefreshInitialNormal(Owner, 1, 2);
	RefreshInitialActivation(Owner, MakeSequencerActivationAction());
	const FWBRuntimeDecisionPointStatus BeforeStatus = Owner->GetCurrentStatus();

	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			TEXT("activate:missing"),
			Owner,
			MakeSequencerRefreshInput(3, 3, 1));

	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	TestFalse(TEXT("No refresh attempted"), Result.bPostActionRefreshAttempted);
	TestEqual(TEXT("Normal count unchanged"), Owner->GetCurrentStatus().LegalActionCount, BeforeStatus.LegalActionCount);
	TestEqual(TEXT("Activation count unchanged"), Owner->GetCurrentStatus().ActivationActionCount, BeforeStatus.ActivationActionCount);
	ExpectSequencerStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerFailureRefreshOptionsTest, "Wandbound.Runtime.PostActivationRefreshSequencer.FailedActivationCanRefreshWhenConfigured", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerFailureRefreshOptionsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	const FWBGameStateData Before = State;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	RefreshInitialActivation(Owner, MakeSequencerActivationAction());
	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnFailure = true;
	Options.bRefreshActivationPresentationOnFailure = true;
	const FWBRuntimePostActivationRefreshInput Input = MakeSequencerRefreshInput(2, 2, 1);
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			TEXT("activate:missing"),
			Owner,
			Input,
			Options);

	TestFalse(TEXT("Result still fails"), Result.bOk);
	TestEqual(TEXT("Reason remains execution failure"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	TestTrue(TEXT("Refresh attempted"), Result.bPostActionRefreshAttempted);
	TestTrue(TEXT("Normal refreshed"), Result.bNormalDecisionPointRefreshed);
	TestTrue(TEXT("Activation refreshed"), Result.bActivationPresentationRefreshed);
	TestEqual(TEXT("Normal count from supplied data"), Owner->GetCurrentStatus().LegalActionCount, Input.PostActionPrecomputedLegalActions.Num());
	TestEqual(TEXT("Activation count from supplied data"), Owner->GetCurrentStatus().ActivationActionCount, Input.PostActionActivationActionSet.Actions.Num());
	ExpectSequencerStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerNormalRefreshFailureTest, "Wandbound.Runtime.PostActivationRefreshSequencer.SuccessButNormalDecisionRefreshFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerNormalRefreshFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path without turn model"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel, false, true));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialActivation(Owner, Action);
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			MakeSequencerRefreshInput());

	TestFalse(TEXT("Result fails"), Result.bOk);
	TestTrue(TEXT("Execution still succeeds"), Result.bActivationExecutionOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("normal_decision_refresh_failed")));
	TestFalse(TEXT("Normal not refreshed"), Result.bNormalDecisionPointRefreshed);
	TestEqual(TEXT("Underlying normal refresh reason"), Result.NormalDecisionRefreshResult.Reason, FString(TEXT("turn_interaction_model_missing")));
	TestEqual(TEXT("Activation still mutates state"), State.GetUnitById(SequencerTargetUnitId)->HP, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerSuppliedSummaryTest, "Wandbound.Runtime.PostActivationRefreshSequencer.RefreshUsesSuppliedSummaryNotState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerSuppliedSummaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialActivation(Owner, Action);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimePostActivationRefreshInput Input = MakeSequencerRefreshInput(1, 0, 1);
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			Input,
			Options);

	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("State still has two units"), State.Units.Num(), 2);
	TestEqual(TEXT("Status uses supplied public summary count"), Owner->GetCurrentStatus().PublicUnitCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerPaymentUsageTest, "Wandbound.Runtime.PostActivationRefreshSequencer.PaymentAndUsageHandledByCore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerPaymentUsageTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	Action.Command = MakeSequencerCommand(
		1,
		true,
		2,
		true,
		TEXT("runtime_post_activation_usage"));
	RefreshInitialActivation(Owner, Action);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnSuccess = false;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			MakeSequencerRefreshInput(),
			Options);

	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("RLUsed paid by core"), State.GetUnitById(SequencerSourceUnitId)->RLUsed, 3);
	TestTrue(TEXT("Usage marked by core"), State.HasActivationUsageKeyThisTurn(0, TEXT("runtime_post_activation_usage")));
	TestNotNull(TEXT("Cost trace present"), Result.ActivationExecutionResult.ActivationResult.TraceEvents.FindByPredicate([](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(TEXT("card_activation_cost_paid"));
	}));
	TestNotNull(TEXT("Usage trace present"), Result.ActivationExecutionResult.ActivationResult.TraceEvents.FindByPredicate([](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(TEXT("card_activation_usage_marked"));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerHiddenMetadataTest, "Wandbound.Runtime.PostActivationRefreshSequencer.HiddenMetadataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FString SecretSourceEffect = TEXT("SECRET_SOURCE_EFFECT_DO_NOT_LEAK");
	const FString SecretUsageKey = TEXT("SECRET_USAGE_KEY_DO_NOT_LEAK");
	const FString SecretDebugId = TEXT("SECRET_DEBUG_ACTIVATION_DO_NOT_LEAK");
	const FString SecretCostKind = TEXT("SECRET_COST_KIND_DO_NOT_LEAK");

	FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	Action.PublicLabel = TEXT("Safe Public Activation");
	Action.Command = MakeSequencerCommand(
		1,
		false,
		0,
		true,
		SecretUsageKey,
		SecretSourceEffect,
		SecretDebugId,
		FName(*SecretCostKind));
	RefreshInitialActivation(Owner, Action);

	FWBRuntimePostActivationRefreshInput Input = MakeSequencerRefreshInput(1, 1, 2);
	Input.PostActionActivationActionSet.Actions[0].PublicLabel = TEXT("Safe Post Activation");
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			Input);

	TestTrue(TEXT("Result ok"), Result.bOk);
	const FString PublicText = CombineSequencerActivationPublicText(ActivationModel);
	const FString SerializedTrace = WBReplayTrace::SerializeEvents(
		Result.ActivationExecutionResult.ActivationResult.TraceEvents);

	for (const FString& Token : { SecretSourceEffect, SecretUsageKey, SecretDebugId, SecretCostKind })
	{
		TestFalse(*FString::Printf(TEXT("Public text excludes %s"), *Token), PublicText.Contains(Token, ESearchCase::IgnoreCase));
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Token), SerializedTrace.Contains(Token, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("Safe label present"), PublicText.Contains(TEXT("Safe Post Activation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerSourceGuardTest, "Wandbound.Runtime.PostActivationRefreshSequencer.SourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Sequencer source loads"), LoadSequencerSource(Source));
	TestFalse(TEXT("No legal generation"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No activation candidate generator"), Source.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("No activation legal action generator"), Source.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("No effect runner direct call"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No action codec"), Source.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No board summary bridge"), Source.Contains(TEXT("WBBoardSummaryBridge")));
	TestFalse(TEXT("No public summary build"), Source.Contains(TEXT("WBPublicBoardSummary::Build")));
	TestTrue(TEXT("Accepts external state by reference"), Source.Contains(TEXT("FWBGameStateData& State")));
	TestFalse(TEXT("No game state pointer storage"), Source.Contains(TEXT("FWBGameStateData*")));
	TestFalse(TEXT("No current game state member"), Source.Contains(TEXT("FWBGameStateData Current")));
	TestFalse(TEXT("No stored game state member"), Source.Contains(TEXT("FWBGameStateData Stored")));
	TestFalse(TEXT("No cached game state member"), Source.Contains(TEXT("FWBGameStateData Cached")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand access"), Source.Contains(TEXT(".Hand")) || Source.Contains(TEXT("->Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerOwnerConvenienceTest, "Wandbound.Runtime.PostActivationRefreshSequencer.OwnerConvenienceDelegates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerOwnerConvenienceTest::RunTest(const FString& Parameters)
{
	FString OwnerSource;
	TestTrue(TEXT("Owner source loads"), LoadOwnerSourceForSequencer(OwnerSource));
	TestTrue(
		TEXT("Owner delegates to sequencer"),
		OwnerSource.Contains(TEXT("WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData")));

	FWBGameStateData State = MakeSequencerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialActivation(Owner, Action);
	const FWBRuntimePostActivationRefreshInput Input = MakeSequencerRefreshInput(1, 1, 1);
	const FWBRuntimePostActivationRefreshResult Result =
		Owner->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Input);

	TestTrue(TEXT("Result ok"), Result.bOk);
	TestTrue(TEXT("Owner stores activation execution"), Owner->HasLastActivationExecutionResult());
	TestTrue(TEXT("Normal refreshed"), Result.bNormalDecisionPointRefreshed);
	TestTrue(TEXT("Activation refreshed"), Result.bActivationPresentationRefreshed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimePostActivationRefreshSequencerActionBoundaryTest, "Wandbound.Runtime.PostActivationRefreshSequencer.ActivationRemainsSeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePostActivationRefreshSequencerActionBoundaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSequencerState();
	const TArray<FString> LegalActionIdsBefore = NormalActionIdsForSequencerState(State);
	const FString MoveCodecIdBefore = WBActionCodec::MakeActionId(MakeSequencerMoveAction());

	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Owner path"), MakeSequencerOwnerPath(Owner, Coordinator, TurnModel, ActivationModel));

	const FWBCardActivationLegalAction Action = MakeSequencerActivationAction();
	RefreshInitialActivation(Owner, Action);
	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnSuccess = false;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimePostActivationRefreshResult Result =
		WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			Owner,
			MakeSequencerRefreshInput(),
			Options);

	TestTrue(TEXT("Activation succeeds"), Result.bOk);
	const TArray<FString> LegalActionIdsAfter = NormalActionIdsForSequencerState(State);
	TestEqual(TEXT("Normal legal action count unchanged"), LegalActionIdsAfter.Num(), LegalActionIdsBefore.Num());
	for (const FString& ActionId : LegalActionIdsAfter)
	{
		TestFalse(TEXT("Normal legal action id is not activation"), ActionId.StartsWith(TEXT("activate:")));
	}
	TestEqual(TEXT("Move codec unchanged"), WBActionCodec::MakeActionId(MakeSequencerMoveAction()), MoveCodecIdBefore);
	return true;
}

#endif
