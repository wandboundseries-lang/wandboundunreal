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
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

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

bool RunCardActivationReplayFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const EWBFixtureOperationKind ExpectedOperationKind)
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

	FString CostReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s cost verifier expectations: %s"), *FixtureName, *CostReason),
		ExpectActivationCostPaymentVerifierExpectations(Fixture, State, Result.TraceEvents, CostReason));

	return true;
}

FWBUnitState MakeVerifierUnit(const int32 UnitId, const int32 RLTotal, const int32 RLUsed)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = 0;
	Unit.CardId = TEXT("fixture_verifier_unit");
	Unit.X = 2;
	Unit.Y = 2;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	return Unit;
}

FWBTraceEvent MakeCostPaidTrace()
{
	FWBTraceEvent Trace;
	Trace.Kind = FName(TEXT("card_activation_cost_paid"));
	Trace.PlayerId = 0;
	Trace.SourceUnitId = 1;
	Trace.CostAmount = 2;
	Trace.PreviousRLUsed = 1;
	Trace.NewRLUsed = 3;
	Trace.AvailableRLBefore = 2;
	Trace.AvailableRLAfter = 0;
	Trace.CostKind = FName(TEXT("RR"));
	Trace.bOk = true;
	return Trace;
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

bool GeneratedActionIdsContainPrefix(const TArray<FString>& ActionIds, const FString& Prefix)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentReplayVerifierHelperTest, "Wandbound.Core.CardActivationCostPaymentReplayVerifier.Helpers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentReplayVerifierHelperTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.AddUnitForTest(MakeVerifierUnit(1, 3, 3));

	FWBExpectedActivationCostPaymentState ExpectedState;
	ExpectedState.SourceUnitId = 1;
	ExpectedState.ExpectedRLTotal = 3;
	ExpectedState.ExpectedRLUsed = 3;
	ExpectedState.ExpectedAvailableRL = 0;

	FString Reason;
	TestTrue(TEXT("RL state verifier succeeds"), FWBCardActivationCostPaymentVerifier::VerifyUnitRLState(State, ExpectedState, Reason));

	ExpectedState.ExpectedRLUsed = 2;
	TestFalse(TEXT("RL state verifier fails clearly"), FWBCardActivationCostPaymentVerifier::VerifyUnitRLState(State, ExpectedState, Reason));
	TestTrue(TEXT("RL state mismatch reason"), Reason.Contains(TEXT("rl_used_expected_2_got_3")));

	const TArray<FWBTraceEvent> TraceEvents = { MakeCostPaidTrace() };
	TestTrue(
		TEXT("Cost-paid trace verifier succeeds"),
		FWBCardActivationCostPaymentVerifier::VerifyCostPaidTrace(TraceEvents, 0, 1, 2, 1, 3, 2, 0, Reason));
	TestTrue(
		TEXT("ContainsTraceKind finds cost-paid trace"),
		FWBCardActivationCostPaymentVerifier::ContainsTraceKind(TraceEvents, FName(TEXT("card_activation_cost_paid"))));
	TestFalse(
		TEXT("ContainsTraceKind misses absent trace"),
		FWBCardActivationCostPaymentVerifier::ContainsTraceKind(TraceEvents, FName(TEXT("missing_trace"))));

	TestFalse(
		TEXT("Cost-paid trace mismatch fails"),
		FWBCardActivationCostPaymentVerifier::VerifyCostPaidTrace(TraceEvents, 0, 1, 3, 1, 3, 2, 0, Reason));
	TestEqual(TEXT("Cost-paid trace mismatch reason"), Reason, FString(TEXT("card_activation_cost_paid_trace_mismatch")));

	TestTrue(
		TEXT("Trace JSON hidden substring verifier succeeds"),
		FWBCardActivationCostPaymentVerifier::TraceJsonExcludesForbiddenSubstrings(TraceEvents, { TEXT("SECRET_DO_NOT_LEAK") }, Reason));

	FWBTraceEvent HiddenTrace = MakeCostPaidTrace();
	HiddenTrace.Reason = TEXT("SECRET_DO_NOT_LEAK");
	TestFalse(
		TEXT("Trace JSON hidden substring verifier fails"),
		FWBCardActivationCostPaymentVerifier::TraceJsonExcludesForbiddenSubstrings({ HiddenTrace }, { TEXT("SECRET_DO_NOT_LEAK") }, Reason));
	TestEqual(TEXT("Hidden substring reason"), Reason, FString(TEXT("forbidden_trace_substring:SECRET_DO_NOT_LEAK")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentReplayVerifierFixtureApplicationTest, "Wandbound.Core.CardActivationCostPaymentReplayVerifier.FixtureApplication", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentReplayVerifierFixtureApplicationTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_replay_cost_paid_trace_parity.json"),
		TEXT("card_activation_replay_final_rl_used_parity.json"),
		TEXT("card_activation_replay_payment_not_paid_on_failure.json"),
		TEXT("card_activation_replay_payment_rollback_on_effect_failure.json"),
		TEXT("card_activation_replay_payment_and_usage_success_order.json"),
		TEXT("card_activation_replay_payment_metadata_hidden.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		RunCardActivationReplayFixture(*this, FixtureName, EWBFixtureOperationKind::ApplyCardActivationCommand);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentReplayVerifierLegalFixtureTest, "Wandbound.Core.CardActivationCostPaymentReplayVerifier.LegalActionFixtures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentReplayVerifierLegalFixtureTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_replay_legal_action_preserves_payment_commit.json"),
		TEXT("card_activation_replay_payment_not_in_fwbaction_codec.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		if (!TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGoldenFixture(FixtureName, Fixture)))
		{
			continue;
		}

		FWBGameStateData State;
		FString StateReason;
		if (!TestTrue(
			*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason),
			BuildGameStateFromFixture(Fixture, State, StateReason)))
		{
			continue;
		}

		const FWBUnitState* SourceBefore = State.GetUnitById(1);
		const int32 InitialRLUsed = SourceBefore != nullptr ? SourceBefore->RLUsed : -1;

		FWBApplyActionResult Result;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, Result, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationLegalActions);
		TestTrue(*FString::Printf(TEXT("%s operation ok"), *FixtureName), Result.bOk);

		const FWBUnitState* SourceAfter = State.GetUnitById(1);
		TestTrue(*FString::Printf(TEXT("%s source unit remains"), *FixtureName), SourceAfter != nullptr);
		if (SourceAfter != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("%s legal generation does not pay RL"), *FixtureName), SourceAfter->RLUsed, InitialRLUsed);
		}

		FString CostReason;
		TestTrue(
			*FString::Printf(TEXT("%s verifier expectations: %s"), *FixtureName, *CostReason),
			ExpectActivationCostPaymentVerifierExpectations(Fixture, State, Result.TraceEvents, CostReason));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentReplayVerifierCodecBoundaryTest, "Wandbound.Core.CardActivationCostPaymentReplayVerifier.CodecBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentReplayVerifierCodecBoundaryTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::PassResponse)), FString(TEXT("pass_response:p0")));

	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Codec boundary fixture loads"), LoadGoldenFixture(TEXT("card_activation_replay_payment_not_in_fwbaction_codec.json"), Fixture));

	FWBGameStateData State;
	FString StateReason;
	TestTrue(TEXT("Codec boundary fixture builds state"), BuildGameStateFromFixture(Fixture, State, StateReason));

	const TArray<FString> CoreLegalActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActions(State));
	TestFalse(TEXT("Core WBRules legal actions do not contain activation ids"), GeneratedActionIdsContainPrefix(CoreLegalActionIds, TEXT("activate:")));

	FString CodecSource;
	TestTrue(
		TEXT("WBActionCodec source loads"),
		FFileHelper::LoadFileToString(
			CodecSource,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"))));
	TestFalse(TEXT("WBActionCodec source has no activation id encoder"), CodecSource.Contains(TEXT("activate:"), ESearchCase::IgnoreCase));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentReplayVerifierSourceGuardTest, "Wandbound.Core.CardActivationCostPaymentReplayVerifier.TestOnlySourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentReplayVerifierSourceGuardTest::RunTest(const FString& Parameters)
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
			*FString::Printf(TEXT("%s excludes test-only verifier"), *SourceFile),
			Source.Contains(TEXT("WBCardActivationCostPaymentVerifier"), ESearchCase::IgnoreCase));
	}

	return true;
}

#endif
