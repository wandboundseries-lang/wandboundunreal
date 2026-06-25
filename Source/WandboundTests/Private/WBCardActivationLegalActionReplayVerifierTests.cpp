#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCostPaymentVerifier.h"
#include "WBCardActivationLegalActionReplayVerifier.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeReplayPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeReplayUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 RLTotal = 3,
	const int32 RLUsed = 0)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeReplayState(
	const int32 SourceRLTotal = 3,
	const int32 SourceRLUsed = 0,
	const int32 TargetHP = 2,
	const int32 TargetMaxHP = 5)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeReplayPlayer(0, 2));
	State.Players.Add(MakeReplayPlayer(1, 0));
	State.AddUnitForTest(MakeReplayUnit(1, 0, FWBTile(4, 4), 5, 5, SourceRLTotal, SourceRLUsed));
	State.AddUnitForTest(MakeReplayUnit(2, 1, FWBTile(5, 4), TargetHP, TargetMaxHP, 4, 1));
	return State;
}

FWBEffectTargetRef MakeUnitTarget(const int32 TargetUnitId = 2)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = TargetUnitId;
	return Target;
}

FWBGenericEffectPayload MakeHealPayload(const int32 Amount = 1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeDamagePayload(const int32 Amount = 1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardActivationCommand MakeReplayCommand(
	const int32 RequiredRR = 2,
	TArray<FWBGenericEffectPayload> Payloads = TArray<FWBGenericEffectPayload>{ MakeHealPayload() },
	const bool bWithUsageCommit = false,
	const bool bWithHiddenMetadata = false)
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = 1;
	Command.Source.SourceCardId = bWithHiddenMetadata
		? FString(TEXT("SECRET_SOURCE_CARD_DO_NOT_LEAK"))
		: FString(TEXT("fixture_replay_card"));
	Command.Source.SourceEffectId = bWithHiddenMetadata
		? FString(TEXT("SECRET_SOURCE_EFFECT_DO_NOT_LEAK"))
		: FString(TEXT("fixture_replay_effect"));
	Command.DebugActivationId = bWithHiddenMetadata
		? FString(TEXT("SECRET_DEBUG_DO_NOT_LEAK"))
		: FString(TEXT("fixture_replay_activation"));
	Command.EffectRequest.Target = MakeUnitTarget(2);
	Command.EffectRequest.Payloads = Payloads;
	Command.CostPaymentCommit.bPayCostOnSuccess = true;
	Command.CostPaymentCommit.PlayerId = 0;
	Command.CostPaymentCommit.SourceUnitId = 1;
	Command.CostPaymentCommit.RequiredRR = RequiredRR;
	Command.CostPaymentCommit.CostKind = FName(TEXT("RR"));
	if (bWithUsageCommit)
	{
		Command.UsageCommit.bMarkUsageOnSuccess = true;
		Command.UsageCommit.PlayerId = 0;
		Command.UsageCommit.UsageKey = bWithHiddenMetadata
			? FString(TEXT("SECRET_USAGE_KEY_DO_NOT_LEAK"))
			: FString(TEXT("fixture_replay_usage_key"));
	}
	return Command;
}

FWBCardActivationLegalAction MakeReplayLegalAction(
	const FString& ActivationActionId,
	const FWBCardActivationCommand& Command)
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActivationActionId;
	Action.PlayerId = Command.Source.PlayerId;
	Action.SourceUnitId = Command.Source.SourceUnitId;
	Action.PublicLabel = TEXT("Activate");
	Action.Target = Command.EffectRequest.Target;
	Action.Command = Command;
	return Action;
}

FWBCardActivationLegalActionSet MakeReplayActionSet(const FWBCardActivationLegalAction& Action)
{
	FWBCardActivationLegalActionSet ActionSet;
	ActionSet.Actions.Add(Action);
	return ActionSet;
}

TArray<FString> TraceKinds(const TArray<FWBTraceEvent>& TraceEvents)
{
	TArray<FString> Kinds;
	for (const FWBTraceEvent& Event : TraceEvents)
	{
		Kinds.Add(Event.Kind.ToString());
	}
	return Kinds;
}

bool ExpectTraceKinds(
	FAutomationTestBase& Test,
	const FString& Label,
	const TArray<FWBTraceEvent>& TraceEvents,
	const TArray<FString>& ExpectedKinds)
{
	const TArray<FString> ActualKinds = TraceKinds(TraceEvents);
	Test.TestEqual(*FString::Printf(TEXT("%s trace count"), *Label), ActualKinds.Num(), ExpectedKinds.Num());
	const int32 CompareCount = FMath::Min(ActualKinds.Num(), ExpectedKinds.Num());
	for (int32 Index = 0; Index < CompareCount; ++Index)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s trace %d"), *Label, Index), ActualKinds[Index], ExpectedKinds[Index]);
	}
	return ActualKinds == ExpectedKinds;
}

FString GoldenFixturePath(const FString& FileName)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Reference"),
		TEXT("GodotCanon"),
		TEXT("GoldenScenarios"),
		FileName);
}

bool LoadGoldenFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GoldenFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, OutFixture);
	return OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetExpectedObject(const TSharedPtr<FJsonObject>& Fixture)
{
	const TSharedPtr<FJsonObject>* Expected = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expected"), Expected)
		&& Expected != nullptr
		&& Expected->IsValid())
	{
		return *Expected;
	}
	return nullptr;
}

bool HasArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	return Object.IsValid() && Object->TryGetArrayField(FieldName, Values) && Values != nullptr;
}

bool ApplySelectedIdFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const EWBFixtureOperationKind ExpectedOperationKind = EWBFixtureOperationKind::ApplyCardActivationLegalActionById)
{
	TSharedPtr<FJsonObject> Fixture;
	if (!Test.TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenFixture(FixtureName, Fixture)))
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	if (!Test.TestTrue(
		*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason),
		BuildGameStateFromFixture(Fixture, State, StateReason)))
	{
		return false;
	}

	FWBApplyActionResult Result;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString OperationReason;
	if (!Test.TestTrue(
		*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
		ApplyFixtureOperation(Fixture, State, Result, OperationKind, OperationReason)))
	{
		return false;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, ExpectedOperationKind);

	const TSharedPtr<FJsonObject> Expected = GetExpectedObject(Fixture);
	if (!Expected.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("%s missing expected"), *FixtureName));
		return false;
	}

	bool bExpectedOk = Result.bOk;
	if (Expected->TryGetBoolField(TEXT("ok"), bExpectedOk))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), Result.bOk, bExpectedOk);
	}

	FString ReasonContains;
	if (Expected->TryGetStringField(TEXT("reason_contains"), ReasonContains))
	{
		Test.TestTrue(
			*FString::Printf(TEXT("%s reason contains %s, got %s"), *FixtureName, *ReasonContains, *Result.Reason),
			Result.Reason.Contains(ReasonContains));
	}

	if (HasArrayField(Expected, TEXT("expected_trace_order")))
	{
		FString TraceReason;
		Test.TestTrue(
			*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *TraceReason),
			ExpectTraceOrder(Fixture, Result.TraceEvents, TraceReason));
	}

	if (HasArrayField(Expected, TEXT("final_units")))
	{
		FString UnitReason;
		Test.TestTrue(
			*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *UnitReason),
			ExpectUnitStatusSummary(Fixture, State, UnitReason));
	}

	FString PaymentReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s payment verifier expectations: %s"), *FixtureName, *PaymentReason),
		ExpectActivationCostPaymentVerifierExpectations(Fixture, State, Result.TraceEvents, PaymentReason));

	return true;
}

FWBAction MakeCodecAction(const EWBActionType Type)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

bool ActionIdsContainPrefix(const TArray<FString>& ActionIds, const FString& Prefix)
{
	for (const FString& ActionId : ActionIds)
	{
		if (ActionId.StartsWith(Prefix))
		{
			return true;
		}
	}
	return false;
}

void FindSourceFiles(const FString& RootRelativePath, TArray<FString>& OutFiles)
{
	const FString Root = FPaths::Combine(FPaths::ProjectDir(), RootRelativePath);
	IFileManager::Get().FindFilesRecursive(OutFiles, *Root, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(OutFiles, *Root, TEXT("*.cpp"), true, false);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierResolveTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.Resolve", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierResolveTest::RunTest(const FString& Parameters)
{
	const FString ActionId = TEXT("activate:external:known");
	const FWBCardActivationLegalAction Action = MakeReplayLegalAction(ActionId, MakeReplayCommand());
	const FWBCardActivationLegalActionSet ActionSet = MakeReplayActionSet(Action);

	const FWBCardActivationLegalActionReplaySelection Selection =
		FWBCardActivationLegalActionReplayVerifier::ResolveActivationActionId(ActionSet, ActionId);
	TestTrue(TEXT("Selection ok"), Selection.bOk);
	TestEqual(TEXT("Selected id"), Selection.SelectedActivationActionId, ActionId);
	TestEqual(TEXT("Selected action id"), Selection.SelectedAction.ActivationActionId, ActionId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierMissingAndAmbiguousTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.MissingAndAmbiguous", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierMissingAndAmbiguousTest::RunTest(const FString& Parameters)
{
	const FString ActionId = TEXT("activate:external:known");
	const FWBCardActivationLegalAction Action = MakeReplayLegalAction(ActionId, MakeReplayCommand());
	const FWBCardActivationLegalActionSet ActionSet = MakeReplayActionSet(Action);

	FWBGameStateData MissingState = MakeReplayState();
	const int32 MissingInitialRLUsed = MissingState.GetUnitById(1)->RLUsed;
	const FWBCardActivationLegalActionReplayResult MissingResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(
			MissingState,
			ActionSet,
			TEXT("activate:external:missing"));
	TestFalse(TEXT("Missing selection fails"), MissingResult.Selection.bOk);
	TestEqual(TEXT("Missing reason"), MissingResult.Selection.Reason, FString(TEXT("activation_action_id_not_found")));
	TestFalse(TEXT("Missing activation result fails"), MissingResult.ActivationResult.bOk);
	TestEqual(TEXT("Missing no mutation"), MissingState.GetUnitById(1)->RLUsed, MissingInitialRLUsed);

	FWBCardActivationLegalActionSet AmbiguousSet;
	AmbiguousSet.Actions.Add(Action);
	AmbiguousSet.Actions.Add(Action);
	FWBGameStateData AmbiguousState = MakeReplayState();
	const int32 AmbiguousInitialRLUsed = AmbiguousState.GetUnitById(1)->RLUsed;
	const FWBCardActivationLegalActionReplayResult AmbiguousResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(
			AmbiguousState,
			AmbiguousSet,
			ActionId);
	TestFalse(TEXT("Ambiguous selection fails"), AmbiguousResult.Selection.bOk);
	TestEqual(TEXT("Ambiguous reason"), AmbiguousResult.Selection.Reason, FString(TEXT("activation_action_id_ambiguous")));
	TestFalse(TEXT("Ambiguous activation result fails"), AmbiguousResult.ActivationResult.bOk);
	TestEqual(TEXT("Ambiguous no mutation"), AmbiguousState.GetUnitById(1)->RLUsed, AmbiguousInitialRLUsed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierPaymentTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.Payment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierPaymentTest::RunTest(const FString& Parameters)
{
	const FString ActionId = TEXT("activate:external:payment");
	const FWBCardActivationLegalAction Action = MakeReplayLegalAction(ActionId, MakeReplayCommand(2, { MakeHealPayload() }));
	const FWBCardActivationLegalActionSet ActionSet = MakeReplayActionSet(Action);

	FWBGameStateData State = MakeReplayState(3, 0, 2, 5);
	const FWBCardActivationLegalActionReplayResult Result =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(State, ActionSet, ActionId);
	TestTrue(TEXT("Apply selected id succeeds"), Result.ActivationResult.bOk);
	TestEqual(TEXT("RLUsed paid"), State.GetUnitById(1)->RLUsed, 2);
	TestTrue(
		TEXT("Cost paid trace exists"),
		FWBCardActivationCostPaymentVerifier::ContainsTraceKind(
			Result.ActivationResult.TraceEvents,
			FName(TEXT("card_activation_cost_paid"))));

	FString Reason;
	TestTrue(
		TEXT("Cost paid trace fields match"),
		FWBCardActivationCostPaymentVerifier::VerifyCostPaidTrace(
			Result.ActivationResult.TraceEvents,
			0,
			1,
			2,
			0,
			2,
			3,
			1,
			Reason));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierUsageTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.UsageOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierUsageTest::RunTest(const FString& Parameters)
{
	const FString ActionId = TEXT("activate:external:usage");
	const FWBCardActivationLegalAction Action = MakeReplayLegalAction(
		ActionId,
		MakeReplayCommand(2, { MakeHealPayload() }, true));
	const FWBCardActivationLegalActionSet ActionSet = MakeReplayActionSet(Action);

	FWBGameStateData State = MakeReplayState(3, 0, 2, 5);
	const FWBCardActivationLegalActionReplayResult Result =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(State, ActionSet, ActionId);
	TestTrue(TEXT("Apply selected id with usage succeeds"), Result.ActivationResult.bOk);
	TestEqual(TEXT("RLUsed paid"), State.GetUnitById(1)->RLUsed, 2);
	TestTrue(TEXT("Usage marked"), State.HasActivationUsageKeyThisTurn(0, TEXT("fixture_replay_usage_key")));
	ExpectTraceKinds(
		*this,
		TEXT("Payment usage order"),
		Result.ActivationResult.TraceEvents,
		{
			TEXT("card_activation_resolved"),
			TEXT("card_activation_cost_paid"),
			TEXT("effect_request_resolved"),
			TEXT("heal_effect_resolved"),
			TEXT("card_activation_usage_marked")
		});

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierFailureAtomicityTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.FailureAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierFailureAtomicityTest::RunTest(const FString& Parameters)
{
	const FString PaymentFailureId = TEXT("activate:external:payment_failure");
	const FWBCardActivationLegalAction PaymentFailureAction = MakeReplayLegalAction(
		PaymentFailureId,
		MakeReplayCommand(2, { MakeHealPayload() }, true));
	const FWBCardActivationLegalActionSet PaymentFailureSet = MakeReplayActionSet(PaymentFailureAction);

	FWBGameStateData PaymentFailureState = MakeReplayState(3, 2, 2, 5);
	const FWBCardActivationLegalActionReplayResult PaymentFailureResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(
			PaymentFailureState,
			PaymentFailureSet,
			PaymentFailureId);
	TestFalse(TEXT("Payment failure result fails"), PaymentFailureResult.ActivationResult.bOk);
	TestEqual(TEXT("Payment failure reason"), PaymentFailureResult.ActivationResult.Reason, FString(TEXT("cost_not_affordable")));
	TestEqual(TEXT("Payment failure RL unchanged"), PaymentFailureState.GetUnitById(1)->RLUsed, 2);
	TestFalse(TEXT("Payment failure usage not marked"), PaymentFailureState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_replay_usage_key")));
	TestFalse(
		TEXT("Payment failure has no cost trace"),
		FWBCardActivationCostPaymentVerifier::ContainsTraceKind(
			PaymentFailureResult.ActivationResult.TraceEvents,
			FName(TEXT("card_activation_cost_paid"))));

	const FString EffectFailureId = TEXT("activate:external:effect_failure");
	const FWBCardActivationLegalAction EffectFailureAction = MakeReplayLegalAction(
		EffectFailureId,
		MakeReplayCommand(2, { MakeHealPayload(), MakeDamagePayload(-1) }, true));
	const FWBCardActivationLegalActionSet EffectFailureSet = MakeReplayActionSet(EffectFailureAction);

	FWBGameStateData EffectFailureState = MakeReplayState(3, 0, 2, 5);
	const FWBCardActivationLegalActionReplayResult EffectFailureResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(
			EffectFailureState,
			EffectFailureSet,
			EffectFailureId);
	TestFalse(TEXT("Effect failure result fails"), EffectFailureResult.ActivationResult.bOk);
	TestEqual(TEXT("Effect failure reason"), EffectFailureResult.ActivationResult.Reason, FString(TEXT("negative_damage_effect_amount")));
	TestEqual(TEXT("Effect failure RL unchanged"), EffectFailureState.GetUnitById(1)->RLUsed, 0);
	TestEqual(TEXT("Effect failure target HP unchanged"), EffectFailureState.GetUnitById(2)->HP, 2);
	TestFalse(TEXT("Effect failure usage not marked"), EffectFailureState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_replay_usage_key")));
	TestFalse(
		TEXT("Effect failure has no cost trace"),
		FWBCardActivationCostPaymentVerifier::ContainsTraceKind(
			EffectFailureResult.ActivationResult.TraceEvents,
			FName(TEXT("card_activation_cost_paid"))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierHiddenMetadataTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.HiddenMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierHiddenMetadataTest::RunTest(const FString& Parameters)
{
	const FString ActionId = TEXT("activate:external:hidden");
	const FWBCardActivationLegalAction Action = MakeReplayLegalAction(
		ActionId,
		MakeReplayCommand(2, { MakeHealPayload() }, true, true));
	const FWBCardActivationLegalActionSet ActionSet = MakeReplayActionSet(Action);

	FWBGameStateData State = MakeReplayState(3, 0, 2, 5);
	const FWBCardActivationLegalActionReplayResult Result =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(State, ActionSet, ActionId);
	TestTrue(TEXT("Hidden metadata activation succeeds"), Result.ActivationResult.bOk);

	FString Reason;
	TestTrue(
		TEXT("Hidden metadata excluded from trace JSON"),
		FWBCardActivationCostPaymentVerifier::TraceJsonExcludesForbiddenSubstrings(
			Result.ActivationResult.TraceEvents,
			{
				TEXT("SECRET_SOURCE_CARD_DO_NOT_LEAK"),
				TEXT("SECRET_SOURCE_EFFECT_DO_NOT_LEAK"),
				TEXT("SECRET_DEBUG_DO_NOT_LEAK"),
				TEXT("SECRET_USAGE_KEY_DO_NOT_LEAK"),
				TEXT("source_card_id"),
				TEXT("source_effect_id"),
				TEXT("debug_activation_id"),
				TEXT("usage_key")
			},
			Reason));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierCodecBoundaryTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.CodecBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierCodecBoundaryTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::PassResponse)), FString(TEXT("pass_response:p0")));

	FWBGameStateData State = MakeReplayState(3, 0, 2, 5);
	const TArray<FString> CoreActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActions(State));
	TestFalse(TEXT("Core legal action ids exclude activate prefix"), ActionIdsContainPrefix(CoreActionIds, TEXT("activate:")));

	FString CodecSource;
	TestTrue(
		TEXT("WBActionCodec source loads"),
		FFileHelper::LoadFileToString(
			CodecSource,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"))));
	TestFalse(TEXT("WBActionCodec source excludes activate ids"), CodecSource.Contains(TEXT("activate:"), ESearchCase::IgnoreCase));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierFixtureTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.Fixtures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierFixtureTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_replay_selected_action_id_cost_paid_success.json"),
		TEXT("card_activation_replay_selected_action_id_missing_fails.json"),
		TEXT("card_activation_replay_selected_action_id_ambiguous_fails.json"),
		TEXT("card_activation_replay_selected_action_id_payment_and_usage_success.json"),
		TEXT("card_activation_replay_selected_action_id_payment_failure_no_mutation.json"),
		TEXT("card_activation_replay_selected_action_id_effect_failure_rolls_back_payment.json"),
		TEXT("card_activation_replay_selected_action_id_hidden_metadata_excluded.json"),
		TEXT("card_activation_replay_selected_action_id_not_in_wbactioncodec.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		ApplySelectedIdFixture(*this, FixtureName);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionReplayVerifierSourceGuardTest, "Wandbound.Core.CardActivationLegalActionReplayVerifier.TestOnlySourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionReplayVerifierSourceGuardTest::RunTest(const FString& Parameters)
{
	TArray<FString> SourceFiles;
	FindSourceFiles(TEXT("Source/WandboundCore"), SourceFiles);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), SourceFiles);
	TestTrue(TEXT("Core/runtime source files found"), SourceFiles.Num() > 0);

	for (const FString& SourceFile : SourceFiles)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *SourceFile))
		{
			AddError(FString::Printf(TEXT("Failed to read %s"), *SourceFile));
			continue;
		}

		TestFalse(
			*FString::Printf(TEXT("%s excludes legal-action replay verifier"), *SourceFile),
			Source.Contains(TEXT("WBCardActivationLegalActionReplayVerifier"), ESearchCase::IgnoreCase));
	}

	return true;
}

#endif
