#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"
#include "WBStatusEffect.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeStatusEffectPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeStatusEffectUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 ATK = 2,
	const int32 AR = 1,
	const int32 AttacksLeft = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = 5;
	Unit.ATK = ATK;
	Unit.AR = AR;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = FMath::Max(AttacksLeft, 1);
	return Unit;
}

FWBGameStateData MakeStatusEffectState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 7;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeStatusEffectPlayer(0, 2));
	State.Players.Add(MakeStatusEffectPlayer(1, 0));
	State.AddUnitForTest(MakeStatusEffectUnit(1, 0, FWBTile(4, 4), 5, 2, 1, 1));
	State.AddUnitForTest(MakeStatusEffectUnit(2, 1, FWBTile(5, 4), 5, 1, 1, 1));
	return State;
}

FWBStatusEffectRequest MakeStatusEffectRequest(
	const EWBStatusEffectOp Operation,
	const FName StatusId,
	const int32 Duration,
	const int32 TargetUnitId = 1)
{
	FWBStatusEffectRequest Request;
	Request.Operation = Operation;
	Request.TargetUnitId = TargetUnitId;
	Request.StatusId = StatusId;
	Request.Duration = Duration;
	Request.SourceReason = FName(TEXT("test"));
	return Request;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString StatusEffectFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadStatusEffectFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *StatusEffectFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.IsValid()
		&& Object->TryGetObjectField(FieldName, Child)
		&& Child != nullptr
		&& Child->IsValid())
	{
		return *Child;
	}

	return nullptr;
}

bool ReadExpectedOk(const TSharedPtr<FJsonObject>& Fixture, bool& bOutOk)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	return Expected.IsValid() && Expected->TryGetBoolField(TEXT("ok"), bOutOk);
}

FString ReadExpectedReasonContains(const TSharedPtr<FJsonObject>& Fixture)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	FString ReasonContains;
	if (Expected.IsValid())
	{
		Expected->TryGetStringField(TEXT("reason_contains"), ReasonContains);
	}
	return ReasonContains;
}

int32 CountActionsOfType(const TArray<FWBAction>& Actions, const EWBActionType Type)
{
	int32 Count = 0;
	for (const FWBAction& Action : Actions)
	{
		if (Action.Type == Type)
		{
			++Count;
		}
	}
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectApplyBurnDurationTest, "Wandbound.Core.StatusEffectScaffolding.ApplyBurnWithDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectApplyBurnDurationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	const FWBApplyActionResult Result = WBEffectRunner::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestTrue(TEXT("Burn active"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Burn duration"), State.GetUnitById(1)->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 2);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("status_modified")));
		TestEqual(TEXT("Trace operation"), Result.TraceEvents[0].StatusEffectOperation, FName(TEXT("apply_status")));
		TestEqual(TEXT("Trace status"), Result.TraceEvents[0].StatusId, FName(TEXT("Burn")));
		TestEqual(TEXT("Trace previous turns"), Result.TraceEvents[0].PreviousStatusTurns, 0);
		TestEqual(TEXT("Trace new turns"), Result.TraceEvents[0].NewStatusTurns, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectLowercaseAliasCanonicalTest, "Wandbound.Core.StatusEffectScaffolding.LowercaseAliasCanonicalizes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectLowercaseAliasCanonicalTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("burn")), 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Canonical request status"), Result.Request.StatusId, FName(TEXT("Burn")));
	TestEqual(TEXT("Single stored status"), State.GetUnitById(1)->Statuses.Num(), 1);
	TestTrue(TEXT("Canonical Burn stored"), State.GetUnitById(1)->Statuses.Contains(FName(TEXT("Burn"))));
	if (State.GetUnitById(1)->Statuses.Num() == 1)
	{
		TestEqual(TEXT("Stored status preserves canonical text"), State.GetUnitById(1)->Statuses.Array()[0].ToString(), FString(TEXT("Burn")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectRemoveStatusTest, "Wandbound.Core.StatusEffectScaffolding.RemoveStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectRemoveStatusTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Burn")), 2);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::RemoveStatus, FName(TEXT("Burn")), 0));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestFalse(TEXT("Burn absent"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Burn duration removed"), State.GetUnitById(1)->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 0);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("Removal trace"), Result.TraceEvents[1].Kind, FName(TEXT("status_removed")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectRemoveMissingNoOpTest, "Wandbound.Core.StatusEffectScaffolding.RemoveMissingStatusNoOp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectRemoveMissingNoOpTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	const FWBApplyActionResult Result = WBEffectRunner::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::RemoveStatus, FName(TEXT("Burn")), 0));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestFalse(TEXT("Burn absent"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Only status_modified trace"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectSetDurationTest, "Wandbound.Core.StatusEffectScaffolding.SetDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectSetDurationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Burn")), 1);
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::SetStatusDuration, FName(TEXT("Burn")), 3));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Duration"), State.GetUnitById(1)->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectAddDurationTest, "Wandbound.Core.StatusEffectScaffolding.AddDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectAddDurationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Burn")), 1);
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::AddStatusDuration, FName(TEXT("Burn")), 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Duration"), State.GetUnitById(1)->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectReduceDurationExpiresTest, "Wandbound.Core.StatusEffectScaffolding.ReduceDurationExpires", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectReduceDurationExpiresTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Burn")), 1);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::ReduceStatusDuration, FName(TEXT("Burn")), 1));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestFalse(TEXT("Burn expired"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("Removal trace"), Result.TraceEvents[1].Kind, FName(TEXT("status_removed")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectCleanseOneTest, "Wandbound.Core.StatusEffectScaffolding.CleanseOneStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectCleanseOneTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Burn")), 2);
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Poison")), 2);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::CleanseStatus, FName(TEXT("Burn")), 0));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestFalse(TEXT("Burn removed"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	TestTrue(TEXT("Poison remains"), State.GetUnitById(1)->HasStatus(FName(TEXT("Poison"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectCleanseAllTest, "Wandbound.Core.StatusEffectScaffolding.CleanseAllStatuses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectCleanseAllTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Rooted")), 1);
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Burn")), 2);
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Poison")), 3);
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::CleanseAllStatuses, NAME_None, 0));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("No statuses"), State.GetUnitById(1)->Statuses.Num(), 0);
	TestEqual(TEXT("No status durations"), State.GetUnitById(1)->StatusTurnsRemaining.Num(), 0);
	TestEqual(TEXT("Removed count"), Result.RemovedStatuses.Num(), 3);
	if (Result.RemovedStatuses.Num() == 3)
	{
		TestEqual(TEXT("Removed 0"), Result.RemovedStatuses[0], FName(TEXT("Burn")));
		TestEqual(TEXT("Removed 1"), Result.RemovedStatuses[1], FName(TEXT("Poison")));
		TestEqual(TEXT("Removed 2"), Result.RemovedStatuses[2], FName(TEXT("Rooted")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectMissingTargetFailsTest, "Wandbound.Core.StatusEffectScaffolding.MissingTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectMissingTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), 2, 99));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_target_unit")));
	TestFalse(TEXT("Unit unchanged"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectRemovedTargetFailsTest, "Wandbound.Core.StatusEffectScaffolding.RemovedTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectRemovedTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	State.GetMutableUnitById(1)->MarkUnitDefeated();
	State.GetMutableUnitById(1)->RemoveUnitFromBoard();
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), 2));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("target_unit_removed")));
	TestFalse(TEXT("Unit unchanged"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectNegativeDurationFailsTest, "Wandbound.Core.StatusEffectScaffolding.NegativeDurationFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectNegativeDurationFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), -1));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("negative_status_effect_duration")));
	TestFalse(TEXT("Unit unchanged"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectUnknownOperationFailsTest, "Wandbound.Core.StatusEffectScaffolding.UnknownOperationFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectUnknownOperationFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::Unknown, FName(TEXT("Burn")), 1));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("unknown_status_effect_operation")));
	TestFalse(TEXT("Unit unchanged"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectTickIntegrationTest, "Wandbound.Core.StatusEffectScaffolding.StatusTickIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectTickIntegrationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeStatusEffectState();
	const FWBApplyActionResult ApplyResult = WBEffectRunner::ApplyStatusEffect(State, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Poison")), 2));
	TestTrue(TEXT("Poison apply ok"), ApplyResult.bOk);

	const FWBApplyActionResult TickResult = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);
	TestTrue(TEXT("Tick ok"), TickResult.bOk);
	TestTrue(TEXT("Poison remains"), State.GetUnitById(1)->HasStatus(FName(TEXT("Poison"))));
	TestEqual(TEXT("Poison duration decayed"), State.GetUnitById(1)->GetStatusTurnsRemaining(FName(TEXT("Poison"))), 1);
	TestEqual(TEXT("Max HP reduced"), State.GetUnitById(1)->MaxHP, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectLegalIntegrationTest, "Wandbound.Core.StatusEffectScaffolding.MovementAndActionLegalityIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectLegalIntegrationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData RootedState = MakeStatusEffectState();
	WBEffectRunner::ApplyStatusEffect(RootedState, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Rooted")), 2));
	const TArray<FWBAction> RootedActions = WBRules::GenerateLegalActionsForPlayer(RootedState, 0);
	TestEqual(TEXT("Rooted blocks movement"), CountActionsOfType(RootedActions, EWBActionType::Move), 0);
	TestTrue(TEXT("Rooted still allows attack family if otherwise legal"), CountActionsOfType(RootedActions, EWBActionType::Attack) > 0);

	FWBGameStateData StunnedState = MakeStatusEffectState();
	WBEffectRunner::ApplyStatusEffect(StunnedState, MakeStatusEffectRequest(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Stunned")), 2));
	const TArray<FWBAction> StunnedActions = WBRules::GenerateLegalActionsForPlayer(StunnedState, 0);
	TestEqual(TEXT("Stunned blocks movement"), CountActionsOfType(StunnedActions, EWBActionType::Move), 0);
	TestEqual(TEXT("Stunned blocks attacks"), CountActionsOfType(StunnedActions, EWBActionType::Attack), 0);
	TestEqual(TEXT("End turn remains"), CountActionsOfType(StunnedActions, EWBActionType::EndTurn), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBStatusEffectFixtureScenariosTest, "Wandbound.Core.StatusEffectScaffolding.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStatusEffectFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("status_effect_apply_burn_duration.json"),
		TEXT("status_effect_remove_status.json"),
		TEXT("status_effect_set_duration.json"),
		TEXT("status_effect_add_duration.json"),
		TEXT("status_effect_reduce_duration_expires.json"),
		TEXT("status_effect_cleanse_one_status.json"),
		TEXT("status_effect_cleanse_all_statuses.json"),
		TEXT("status_effect_missing_target_fails_no_mutation.json"),
		TEXT("status_effect_removed_target_fails_no_mutation.json"),
		TEXT("status_effect_unknown_operation_fails_no_mutation.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadStatusEffectFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
		if (!StateReason.IsEmpty())
		{
			return false;
		}

		FWBApplyActionResult ApplyResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString ApplyReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *ApplyReason),
			ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::ApplyStatusEffect);

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), ReadExpectedOk(Fixture, bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), ApplyResult.bOk, bExpectedOk);

		const FString ExpectedReasonContains = ReadExpectedReasonContains(Fixture);
		if (!ExpectedReasonContains.IsEmpty())
		{
			TestTrue(
				*FString::Printf(TEXT("%s reason contains %s"), *FixtureName, *ExpectedReasonContains),
				ApplyResult.Reason.Contains(ExpectedReasonContains));
		}

		FString ExpectReason;
		TestTrue(*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
	}

	return true;
}

#endif
