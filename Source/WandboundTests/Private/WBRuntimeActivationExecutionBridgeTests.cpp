#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBReplayTrace.h"
#include "WBRuntimeActivationExecutionBridge.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeVisualControllerComponent.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 BridgeSourceUnitId = 1;
constexpr int32 BridgeTargetUnitId = 2;

UWBRuntimeActivationPresentationModelComponent* MakeBridgeModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeBridgeCoordinator()
{
	return NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointOwnerComponent* MakeBridgeOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeVisualControllerComponent* MakeBridgeVisualController()
{
	return NewObject<UWBRuntimeVisualControllerComponent>(GetTransientPackage());
}

FWBPlayerStateData MakeBridgePlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	return Player;
}

FWBUnitState MakeBridgeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId,
	const int32 HP = 5,
	const int32 CurrentArmor = 0,
	const int32 MaxArmor = 3,
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
	Unit.CurrentArmor = CurrentArmor;
	Unit.MaxArmor = MaxArmor;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeBridgeState(
	const int32 SourceRLTotal = 4,
	const int32 SourceRLUsed = 1,
	const int32 TargetHP = 5)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeBridgePlayer(0));
	State.Players.Add(MakeBridgePlayer(1));
	State.AddUnitForTest(MakeBridgeUnit(
		BridgeSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("visible_bridge_source"),
		5,
		0,
		3,
		SourceRLTotal,
		SourceRLUsed));
	State.AddUnitForTest(MakeBridgeUnit(
		BridgeTargetUnitId,
		1,
		FWBTile(5, 4),
		TEXT("visible_bridge_target"),
		TargetHP,
		0,
		3,
		0,
		0));
	return State;
}

FWBGenericEffectPayload MakeBridgeDamagePayload(
	const int32 Amount,
	const bool bBypassArmor = true,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.TargetUnitId = TargetUnitId;
	Payload.DamageEffect.SourceUnitId = -1;
	Payload.DamageEffect.SourcePlayerId = -1;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = bBypassArmor;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("runtime_bridge_test"));
	return Payload;
}

FWBGenericEffectPayload MakeBridgeHealPayload(
	const int32 Amount,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.TargetUnitId = TargetUnitId;
	Payload.HealEffect.SourceUnitId = -1;
	Payload.HealEffect.SourcePlayerId = -1;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("runtime_bridge_test"));
	return Payload;
}

FWBGenericEffectPayload MakeBridgeArmorPayload(
	const EWBArmorEffectOp Operation,
	const int32 Amount,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = Operation;
	Payload.ArmorEffect.TargetUnitId = TargetUnitId;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("runtime_bridge_test"));
	return Payload;
}

FWBGenericEffectPayload MakeBridgeStatusPayload(
	const EWBStatusEffectOp Operation,
	const FName StatusId,
	const int32 Duration,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = Operation;
	Payload.StatusEffect.TargetUnitId = TargetUnitId;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("runtime_bridge_test"));
	return Payload;
}

FWBCardActivationCommand MakeBridgeCommand(
	const TArray<FWBGenericEffectPayload>& Payloads,
	const int32 TargetUnitId = BridgeTargetUnitId,
	const bool bPayCost = false,
	const int32 RequiredRR = 0,
	const bool bMarkUsage = false,
	const FString& UsageKey = TEXT("runtime_bridge_usage"))
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = BridgeSourceUnitId;
	Command.Source.SourceCardId = TEXT("runtime_bridge_card");
	Command.Source.SourceEffectId = TEXT("runtime_bridge_effect");
	Command.EffectRequest.Target.TargetUnitId = TargetUnitId;
	Command.EffectRequest.Payloads = Payloads;
	Command.DebugActivationId = TEXT("runtime_bridge_debug");

	Command.CostPaymentCommit.bPayCostOnSuccess = bPayCost;
	Command.CostPaymentCommit.PlayerId = 0;
	Command.CostPaymentCommit.SourceUnitId = BridgeSourceUnitId;
	Command.CostPaymentCommit.RequiredRR = RequiredRR;
	Command.CostPaymentCommit.CostKind = FName(TEXT("RR"));

	Command.UsageCommit.bMarkUsageOnSuccess = bMarkUsage;
	Command.UsageCommit.PlayerId = 0;
	Command.UsageCommit.UsageKey = UsageKey;
	return Command;
}

FWBCardActivationLegalAction MakeBridgeAction(
	const FWBCardActivationCommand& Command,
	const FString& ActionId = TEXT("activate:p0:u1:bridge:t2"))
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActionId;
	Action.PublicLabel = TEXT("Bridge Activation");
	Action.PlayerId = 0;
	Action.SourceUnitId = BridgeSourceUnitId;
	Action.Target.TargetUnitId = BridgeTargetUnitId;
	Action.Command = Command;
	return Action;
}

FWBCardActivationLegalActionSet MakeBridgeActionSet(const FWBCardActivationLegalAction& Action)
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);
	return Set;
}

FWBPublicBoardSummary MakeBridgePublicSummary()
{
	FWBPublicBoardSummary Summary;

	FWBPublicUnitBoardSummary SourceUnit;
	SourceUnit.UnitId = BridgeSourceUnitId;
	SourceUnit.OwnerId = 0;
	SourceUnit.CardId = TEXT("visible_bridge_source");
	SourceUnit.X = 4;
	SourceUnit.Y = 4;
	Summary.Units.Add(SourceUnit);

	FWBPublicUnitBoardSummary TargetUnit;
	TargetUnit.UnitId = BridgeTargetUnitId;
	TargetUnit.OwnerId = 1;
	TargetUnit.CardId = TEXT("visible_bridge_target");
	TargetUnit.X = 5;
	TargetUnit.Y = 4;
	Summary.Units.Add(TargetUnit);

	return Summary;
}

void RefreshBridgeModel(
	UWBRuntimeActivationPresentationModelComponent* Model,
	const FWBCardActivationLegalAction& Action)
{
	if (Model != nullptr)
	{
		Model->SetCurrentActivationPresentationState(
			MakeBridgeActionSet(Action),
			MakeBridgePublicSummary());
	}
}

FWBRuntimeActivationExecutionHandoffResult MakeBridgeHandoff(
	UWBRuntimeActivationPresentationModelComponent* Model,
	const FWBCardActivationLegalAction& Action)
{
	RefreshBridgeModel(Model, Action);
	return Model->CreateExecutionHandoffForActivationActionId(Action.ActivationActionId);
}

const FWBTraceEvent* FindBridgeTraceByKind(
	const TArray<FWBTraceEvent>& TraceEvents,
	const FName Kind)
{
	for (const FWBTraceEvent& Event : TraceEvents)
	{
		if (Event.Kind == Kind)
		{
			return &Event;
		}
	}
	return nullptr;
}

TArray<FString> BridgeTraceKinds(const TArray<FWBTraceEvent>& TraceEvents)
{
	TArray<FString> Kinds;
	for (const FWBTraceEvent& Event : TraceEvents)
	{
		Kinds.Add(Event.Kind.ToString());
	}
	return Kinds;
}

bool CompareBridgeStringArrays(
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

bool LoadBridgeSource(FString& OutSource)
{
	return FFileHelper::LoadFileToString(
		OutSource,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source"),
			TEXT("WandboundRuntime"),
			TEXT("Private"),
			TEXT("WBRuntimeActivationExecutionBridge.cpp")));
}

TArray<FString> NormalActionIdsForBridgeState(
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

void ExpectBridgeStateUnchanged(
	FAutomationTestBase& Test,
	const FWBGameStateData& State,
	const FWBGameStateData& Before)
{
	Test.TestEqual(TEXT("Source RLUsed unchanged"), State.GetUnitById(BridgeSourceUnitId)->RLUsed, Before.GetUnitById(BridgeSourceUnitId)->RLUsed);
	Test.TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(BridgeTargetUnitId)->HP, Before.GetUnitById(BridgeTargetUnitId)->HP);
	Test.TestEqual(TEXT("Target armor unchanged"), State.GetUnitById(BridgeTargetUnitId)->CurrentArmor, Before.GetUnitById(BridgeTargetUnitId)->CurrentArmor);
	Test.TestEqual(TEXT("Target status count unchanged"), State.GetUnitById(BridgeTargetUnitId)->Statuses.Num(), Before.GetUnitById(BridgeTargetUnitId)->Statuses.Num());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeUnresolvedHandoffTest, "Wandbound.Runtime.ActivationExecutionBridge.UnresolvedHandoffFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeUnresolvedHandoffTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	const FWBGameStateData Before = State;
	FWBRuntimeActivationExecutionHandoffResult Handoff;
	Handoff.bHandoffOk = false;
	Handoff.Reason = TEXT("activation_action_id_not_found");
	Handoff.SelectedActivationActionId = TEXT("activate:missing");

	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(State, Handoff);

	TestFalse(TEXT("Execution fails"), Result.bOk);
	TestFalse(TEXT("Execution not attempted"), Result.bExecutionAttempted);
	TestFalse(TEXT("Activation not resolved"), Result.bActivationResolved);
	TestEqual(TEXT("Reason preserved"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	ExpectBridgeStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeSuccessTest, "Wandbound.Runtime.ActivationExecutionBridge.ExecutesResolvedHandoff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationCommand Command = MakeBridgeCommand({
		MakeBridgeDamagePayload(2),
		MakeBridgeArmorPayload(EWBArmorEffectOp::AddCurrentArmor, 1),
		MakeBridgeStatusPayload(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), 2)
	});
	const FWBCardActivationLegalAction Action = MakeBridgeAction(Command);
	const FWBRuntimeActivationExecutionHandoffResult Handoff = MakeBridgeHandoff(Model, Action);

	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(State, Handoff);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestTrue(TEXT("Execution attempted"), Result.bExecutionAttempted);
	TestTrue(TEXT("Activation resolved"), Result.bActivationResolved);
	TestEqual(TEXT("Selected id"), Result.SelectedActivationActionId, Action.ActivationActionId);
	TestEqual(TEXT("Target HP damaged"), State.GetUnitById(BridgeTargetUnitId)->HP, 3);
	TestEqual(TEXT("Target armor added"), State.GetUnitById(BridgeTargetUnitId)->CurrentArmor, 1);
	TestTrue(TEXT("Target status applied"), State.GetUnitById(BridgeTargetUnitId)->HasStatus(FName(TEXT("Burn"))));
	TestNotNull(TEXT("Activation trace"), FindBridgeTraceByKind(Result.ActivationResult.TraceEvents, FName(TEXT("card_activation_resolved"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgePaysCostTest, "Wandbound.Runtime.ActivationExecutionBridge.PaysCost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgePaysCostTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState(4, 1, 3);
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationCommand Command = MakeBridgeCommand(
		{ MakeBridgeHealPayload(1) },
		BridgeTargetUnitId,
		true,
		2);
	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			State,
			MakeBridgeHandoff(Model, MakeBridgeAction(Command)));

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestEqual(TEXT("RLUsed increased"), State.GetUnitById(BridgeSourceUnitId)->RLUsed, 3);
	const FWBTraceEvent* CostTrace = FindBridgeTraceByKind(
		Result.ActivationResult.TraceEvents,
		FName(TEXT("card_activation_cost_paid")));
	TestNotNull(TEXT("Cost trace present"), CostTrace);
	if (CostTrace != nullptr)
	{
		TestEqual(TEXT("Cost amount"), CostTrace->CostAmount, 2);
		TestEqual(TEXT("Previous RLUsed"), CostTrace->PreviousRLUsed, 1);
		TestEqual(TEXT("New RLUsed"), CostTrace->NewRLUsed, 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeMarksUsageTest, "Wandbound.Runtime.ActivationExecutionBridge.MarksUsage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeMarksUsageTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState(4, 1, 3);
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FString UsageKey = TEXT("runtime_bridge_once");
	const FWBCardActivationCommand Command = MakeBridgeCommand(
		{ MakeBridgeHealPayload(1) },
		BridgeTargetUnitId,
		false,
		0,
		true,
		UsageKey);
	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			State,
			MakeBridgeHandoff(Model, MakeBridgeAction(Command)));

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestTrue(TEXT("Usage marked"), State.HasActivationUsageKeyThisTurn(0, UsageKey));
	TestNotNull(TEXT("Usage trace present"), FindBridgeTraceByKind(Result.ActivationResult.TraceEvents, FName(TEXT("card_activation_usage_marked"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeTraceOrderTest, "Wandbound.Runtime.ActivationExecutionBridge.PaymentUsageTraceOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeTraceOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState(4, 1, 3);
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationCommand Command = MakeBridgeCommand(
		{ MakeBridgeHealPayload(1) },
		BridgeTargetUnitId,
		true,
		2,
		true,
		TEXT("runtime_bridge_payment_usage"));
	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			State,
			MakeBridgeHandoff(Model, MakeBridgeAction(Command)));

	const TArray<FString> ExpectedKinds = {
		TEXT("card_activation_resolved"),
		TEXT("card_activation_cost_paid"),
		TEXT("effect_request_resolved"),
		TEXT("heal_effect_resolved"),
		TEXT("card_activation_usage_marked")
	};
	TestTrue(TEXT("Execution ok"), Result.bOk);
	CompareBridgeStringArrays(*this, TEXT("Trace order"), BridgeTraceKinds(Result.ActivationResult.TraceEvents), ExpectedKinds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeFailureRollsBackPaymentTest, "Wandbound.Runtime.ActivationExecutionBridge.FailureRollsBackPayment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeFailureRollsBackPaymentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState(4, 1, 5);
	const FWBGameStateData Before = State;
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationCommand Command = MakeBridgeCommand(
		{ MakeBridgeDamagePayload(-1) },
		BridgeTargetUnitId,
		true,
		2,
		true,
		TEXT("runtime_bridge_rollback"));
	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			State,
			MakeBridgeHandoff(Model, MakeBridgeAction(Command)));

	TestFalse(TEXT("Execution fails"), Result.bOk);
	TestTrue(TEXT("Execution attempted"), Result.bExecutionAttempted);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("negative_damage_effect_amount")));
	ExpectBridgeStateUnchanged(*this, State, Before);
	TestFalse(TEXT("Usage not marked"), State.HasActivationUsageKeyThisTurn(0, TEXT("runtime_bridge_rollback")));
	TestEqual(TEXT("No traces"), Result.ActivationResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeUnaffordableFailsTest, "Wandbound.Runtime.ActivationExecutionBridge.UnaffordableFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeUnaffordableFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState(4, 3, 3);
	const FWBGameStateData Before = State;
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationCommand Command = MakeBridgeCommand(
		{ MakeBridgeHealPayload(1) },
		BridgeTargetUnitId,
		true,
		2);
	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			State,
			MakeBridgeHandoff(Model, MakeBridgeAction(Command)));

	TestFalse(TEXT("Execution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("cost_not_affordable")));
	ExpectBridgeStateUnchanged(*this, State, Before);
	TestNull(TEXT("No cost trace"), FindBridgeTraceByKind(Result.ActivationResult.TraceEvents, FName(TEXT("card_activation_cost_paid"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeHiddenMetadataTest, "Wandbound.Runtime.ActivationExecutionBridge.HiddenMetadataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState(4, 1, 5);
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	FWBCardActivationCommand Command = MakeBridgeCommand(
		{ MakeBridgeDamagePayload(1) },
		BridgeTargetUnitId,
		false,
		0,
		true,
		TEXT("SECRET_RUNTIME_BRIDGE_USAGE"));
	Command.Source.SourceEffectId = TEXT("SECRET_RUNTIME_BRIDGE_EFFECT");
	Command.DebugActivationId = TEXT("SECRET_RUNTIME_BRIDGE_DEBUG");
	Command.CostPaymentCommit.CostKind = FName(TEXT("SECRET_RUNTIME_BRIDGE_COST"));
	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			State,
			MakeBridgeHandoff(Model, MakeBridgeAction(Command)));

	TestTrue(TEXT("Execution ok"), Result.bOk);
	const FString SerializedTrace = WBReplayTrace::SerializeEvents(Result.ActivationResult.TraceEvents);
	const TArray<FString> Forbidden = {
		TEXT("SECRET_RUNTIME_BRIDGE_EFFECT"),
		TEXT("SECRET_RUNTIME_BRIDGE_USAGE"),
		TEXT("SECRET_RUNTIME_BRIDGE_DEBUG"),
		TEXT("SECRET_RUNTIME_BRIDGE_COST"),
		TEXT("SourceEffectId"),
		TEXT("UsageKey"),
		TEXT("DebugActivationId")
	};
	for (const FString& Token : Forbidden)
	{
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Token), SerializedTrace.Contains(Token, ESearchCase::IgnoreCase));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeModelConvenienceTest, "Wandbound.Runtime.ActivationExecutionBridge.ModelConvenience", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeModelConvenienceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationLegalAction Action = MakeBridgeAction(
		MakeBridgeCommand({ MakeBridgeDamagePayload(2) }));
	RefreshBridgeModel(Model, Action);

	const FWBRuntimeActivationExecutionResult Result =
		Model->ExecuteActivationActionId(State, Action.ActivationActionId);
	TestTrue(TEXT("Model execution ok"), Result.bOk);
	TestEqual(TEXT("Target HP changed"), State.GetUnitById(BridgeTargetUnitId)->HP, 3);

	FWBGameStateData MissingIdState = MakeBridgeState();
	const FWBGameStateData MissingIdBefore = MissingIdState;
	const FWBRuntimeActivationExecutionResult MissingResult =
		Model->ExecuteActivationActionId(MissingIdState, TEXT("activate:missing"));
	TestFalse(TEXT("Missing id fails"), MissingResult.bOk);
	TestFalse(TEXT("Missing id not attempted"), MissingResult.bExecutionAttempted);
	TestEqual(TEXT("Missing id reason"), MissingResult.Reason, FString(TEXT("activation_action_id_not_found")));
	ExpectBridgeStateUnchanged(*this, MissingIdState, MissingIdBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeCoordinatorConvenienceTest, "Wandbound.Runtime.ActivationExecutionBridge.CoordinatorConvenience", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeCoordinatorConvenienceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeBridgeCoordinator();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationLegalAction Action = MakeBridgeAction(
		MakeBridgeCommand({ MakeBridgeDamagePayload(2) }));
	Coordinator->SetActivationPresentationModel(Model);
	Coordinator->RefreshActivationPresentationFromExternalData(MakeBridgeActionSet(Action), MakeBridgePublicSummary());

	const FWBRuntimeActivationExecutionResult Result =
		Coordinator->ExecuteActivationActionId(State, Action.ActivationActionId);
	TestTrue(TEXT("Coordinator execution ok"), Result.bOk);
	TestEqual(TEXT("Target HP changed"), State.GetUnitById(BridgeTargetUnitId)->HP, 3);

	FWBGameStateData MissingModelState = MakeBridgeState();
	const FWBGameStateData MissingModelBefore = MissingModelState;
	UWBRuntimeDecisionPointCoordinatorComponent* MissingModelCoordinator = MakeBridgeCoordinator();
	const FWBRuntimeActivationExecutionResult MissingModelResult =
		MissingModelCoordinator->ExecuteActivationActionId(
			MissingModelState,
			Action.ActivationActionId);
	TestFalse(TEXT("Missing model fails"), MissingModelResult.bOk);
	TestEqual(TEXT("Missing model reason"), MissingModelResult.Reason, FString(TEXT("activation_presentation_model_missing")));
	ExpectBridgeStateUnchanged(*this, MissingModelState, MissingModelBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeOwnerConvenienceTest, "Wandbound.Runtime.ActivationExecutionBridge.OwnerConvenience", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeOwnerConvenienceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeBridgeOwner();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeBridgeCoordinator();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationLegalAction Action = MakeBridgeAction(
		MakeBridgeCommand({ MakeBridgeDamagePayload(2) }));
	Coordinator->SetActivationPresentationModel(Model);
	Owner->SetDecisionPointCoordinator(Coordinator);
	Owner->RefreshActivationPresentationFromExternalData(MakeBridgeActionSet(Action), MakeBridgePublicSummary());

	const FWBRuntimeActivationExecutionResult Result =
		Owner->ExecuteActivationActionId(State, Action.ActivationActionId);
	TestTrue(TEXT("Owner execution ok"), Result.bOk);
	TestTrue(TEXT("Owner stores activation result"), Owner->HasLastActivationExecutionResult());
	TestEqual(TEXT("Stored selected id"), Owner->GetLastActivationExecutionResult().SelectedActivationActionId, Action.ActivationActionId);
	TestEqual(TEXT("Target HP changed"), State.GetUnitById(BridgeTargetUnitId)->HP, 3);

	FWBGameStateData MissingCoordinatorState = MakeBridgeState();
	const FWBGameStateData MissingCoordinatorBefore = MissingCoordinatorState;
	UWBRuntimeDecisionPointOwnerComponent* MissingCoordinatorOwner = MakeBridgeOwner();
	const FWBRuntimeActivationExecutionResult MissingCoordinatorResult =
		MissingCoordinatorOwner->ExecuteActivationActionId(
			MissingCoordinatorState,
			Action.ActivationActionId);
	TestFalse(TEXT("Missing coordinator fails"), MissingCoordinatorResult.bOk);
	TestEqual(TEXT("Missing coordinator reason"), MissingCoordinatorResult.Reason, FString(TEXT("decision_point_coordinator_missing")));
	ExpectBridgeStateUnchanged(*this, MissingCoordinatorState, MissingCoordinatorBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeStaleStateTest, "Wandbound.Runtime.ActivationExecutionBridge.StalePresentationAndNoVisualRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeStaleStateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeBridgeCoordinator();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	UWBRuntimeVisualControllerComponent* VisualController = MakeBridgeVisualController();
	const FWBCardActivationLegalAction Action = MakeBridgeAction(
		MakeBridgeCommand({ MakeBridgeDamagePayload(2) }));
	Coordinator->SetActivationPresentationModel(Model);
	Coordinator->SetVisualController(VisualController);
	Coordinator->RefreshActivationPresentationFromExternalData(MakeBridgeActionSet(Action), MakeBridgePublicSummary());

	const int32 ActivationEntryCountBefore = Model->GetCurrentActivationPresentationSnapshot().Entries.Num();
	const int32 TargetEntryCountBefore = Model->GetCurrentTargetPresentationSnapshot().Entries.Num();
	const FWBRuntimeDecisionPointStatus StatusBefore = Coordinator->GetCurrentStatus();
	const FWBRuntimeActivationExecutionResult Result =
		Coordinator->ExecuteActivationActionId(State, Action.ActivationActionId);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestEqual(TEXT("Activation entry count unchanged"), Model->GetCurrentActivationPresentationSnapshot().Entries.Num(), ActivationEntryCountBefore);
	TestEqual(TEXT("Target entry count unchanged"), Model->GetCurrentTargetPresentationSnapshot().Entries.Num(), TargetEntryCountBefore);
	TestEqual(TEXT("Coordinator activation status unchanged"), Coordinator->GetCurrentStatus().ActivationActionCount, StatusBefore.ActivationActionCount);
	TestFalse(TEXT("Visual controller not refreshed"), VisualController->HasReceivedPublicBoardSummary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeSourceGuardTest, "Wandbound.Runtime.ActivationExecutionBridge.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSource(Source));
	TestTrue(TEXT("Bridge calls ApplyCardActivationCommand"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	TestTrue(TEXT("Bridge may mention WBEffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Does not generate normal legal actions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call candidate generator"), Source.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("Does not call legal action generator"), Source.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("Does not call WBActionCodec"), Source.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationExecutionBridgeSeparateFromFWBActionTest, "Wandbound.Runtime.ActivationExecutionBridge.SeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationExecutionBridgeSeparateFromFWBActionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	UWBRuntimeActivationPresentationModelComponent* Model = MakeBridgeModel();
	const FWBCardActivationLegalAction Action = MakeBridgeAction(
		MakeBridgeCommand({ MakeBridgeDamagePayload(1) }));
	const TArray<FString> BeforeActionIds = NormalActionIdsForBridgeState(State, 0);
	const FWBRuntimeActivationExecutionResult Result =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			State,
			MakeBridgeHandoff(Model, Action));
	const TArray<FString> AfterActionIds = NormalActionIdsForBridgeState(State, 0);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	for (const FString& ActionId : AfterActionIds)
	{
		TestFalse(*FString::Printf(TEXT("Normal action id not activation: %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}
	TestEqual(TEXT("Normal action id count unchanged for damage-only activation"), AfterActionIds.Num(), BeforeActionIds.Num());

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
