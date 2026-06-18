#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBDamageResolution.h"
#include "WBDeathResolution.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBUnitState MakePreventionTestUnit(
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
	Unit.CardId = FString::Printf(TEXT("test_unit_%d"), UnitId);
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

FWBGameStateData MakePreventionState(const int32 AttackerATK = 2, const int32 DefenderHP = 5)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = 1;
	Player0.RemainingMP = 2;
	State.Players.Add(Player0);

	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 3;
	State.Players.Add(Player1);

	State.AddUnitForTest(MakePreventionTestUnit(1, 0, FWBTile(4, 4), 5, AttackerATK, 1, 1));
	State.AddUnitForTest(MakePreventionTestUnit(2, 1, FWBTile(5, 4), DefenderHP, 1, 1, 1));
	State.AddUnitForTest(MakePreventionTestUnit(3, 1, FWBTile(7, 4), 5, 1, 1, 1));
	return State;
}

FWBPendingAttackState MakePreventionPendingAttack()
{
	FWBPendingAttackState PendingAttack;
	PendingAttack.bActive = true;
	PendingAttack.AttackerUnitId = 1;
	PendingAttack.DefenderUnitId = 2;
	PendingAttack.AttackingPlayerId = 0;
	PendingAttack.AttackerTile = FWBTile(4, 4);
	PendingAttack.DefenderTile = FWBTile(5, 4);
	PendingAttack.DeclarationActionId = TEXT("attack:p0:u1:t2");
	return PendingAttack;
}

FWBDamageRequest MakeAttackDamageRequest(const int32 BaseDamage, const int32 TargetUnitId = 2)
{
	FWBDamageRequest Request;
	Request.DamageKind = EWBDamageKind::Attack;
	Request.SourceUnitId = 1;
	Request.TargetUnitId = TargetUnitId;
	Request.SourcePlayerId = 0;
	Request.BaseDamage = BaseDamage;
	return Request;
}

FString GoldenScenarioPath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

bool LoadDamageFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GoldenScenarioPath(FileName)))
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

bool ExpectPendingAttackActive(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TSharedPtr<FJsonObject> Pending = GetObjectField(Expected, TEXT("pending_attack"));
	if (!Pending.IsValid())
	{
		return true;
	}

	bool bExpectedActive = false;
	if (!Pending->TryGetBoolField(TEXT("active"), bExpectedActive))
	{
		Test.AddError(Label + TEXT(" missing expected.pending_attack.active"));
		return false;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s pending attack active"), *Label), State.HasPendingAttack(), bExpectedActive);
	return State.HasPendingAttack() == bExpectedActive;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamagePreventionDefaultResultTest, "Wandbound.Core.DamagePreventionScaffolding.DefaultDamagePreventionDoesNotPrevent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamagePreventionDefaultResultTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakePreventionState();
	const FWBDamagePreventionResult Result = WBDamageResolution::EvaluateDamagePrevention(State, MakeAttackDamageRequest(3));

	TestFalse(TEXT("Damage is not prevented"), Result.bPrevented);
	TestEqual(TEXT("No prevented amount"), Result.PreventedAmount, 0);
	TestEqual(TEXT("Final damage equals base damage"), Result.FinalDamage, 3);
	TestTrue(TEXT("No prevention reason"), Result.PreventionReason.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamagePreventionDefaultDeathResultTest, "Wandbound.Core.DamagePreventionScaffolding.DefaultDeathPreventionDoesNotPrevent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamagePreventionDefaultDeathResultTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakePreventionState();
	FWBDeathResolutionCandidate Candidate;
	Candidate.UnitId = 2;
	Candidate.OwnerId = 1;
	Candidate.bIsHero = false;

	const FWBDeathPreventionResult Result = WBDeathResolution::EvaluateDeathPrevention(State, Candidate);
	TestFalse(TEXT("Death is not prevented"), Result.bPrevented);
	TestTrue(TEXT("No death prevention reason"), Result.PreventionReason.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageResolutionReducesHPTest, "Wandbound.Core.DamagePreventionScaffolding.ResolveDamageRequestReducesHP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageResolutionReducesHPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePreventionState(2, 5);

	const FWBDamageResolutionResult Result = WBDamageResolution::ResolveDamageRequest(State, MakeAttackDamageRequest(2));
	TestTrue(TEXT("Damage resolves"), Result.bOk);
	TestEqual(TEXT("Previous HP"), Result.PreviousHP, 5);
	TestEqual(TEXT("New HP"), Result.NewHP, 3);
	TestEqual(TEXT("Target HP reduced"), State.GetUnitById(2)->HP, 3);
	TestFalse(TEXT("Target not defeated"), State.GetUnitById(2)->bDefeated);
	TestFalse(TEXT("Target not removed"), State.GetUnitById(2)->bRemovedFromBoard);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageResolutionClampsZeroWithoutRemovalTest, "Wandbound.Core.DamagePreventionScaffolding.ResolveDamageRequestClampsZeroWithoutRemoval", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageResolutionClampsZeroWithoutRemovalTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePreventionState(2, 1);

	const FWBDamageResolutionResult Result = WBDamageResolution::ResolveDamageRequest(State, MakeAttackDamageRequest(3));
	const FWBUnitState* Target = State.GetUnitById(2);
	TestTrue(TEXT("Damage resolves"), Result.bOk);
	TestEqual(TEXT("HP clamps to zero"), Target != nullptr ? Target->HP : -1, 0);
	TestTrue(TEXT("Zero HP flag set"), Result.bAtOrBelowZeroHP);
	TestTrue(TEXT("Target still on board for cleanup seam"), Target != nullptr && Target->IsUnitOnBoard());
	TestFalse(TEXT("Resolver alone does not defeat"), Target != nullptr && Target->bDefeated);
	TestFalse(TEXT("Resolver alone does not remove"), Target != nullptr && Target->bRemovedFromBoard);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageUsesResolverTraceDefaultsTest, "Wandbound.Core.DamagePreventionScaffolding.AttackDamageUsesResolverTraceDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageUsesResolverTraceDefaultsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePreventionState(2, 5);
	State.SetPendingAttackForTest(MakePreventionPendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Attack damage succeeds"), Result.bOk);
	TestEqual(TEXT("Defender HP reduced"), State.GetUnitById(2)->HP, 3);
	TestFalse(TEXT("Pending attack cleared"), State.HasPendingAttack());
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Trace = Result.TraceEvents[0];
		TestEqual(TEXT("Trace kind"), Trace.Kind, FName(TEXT("attack_damage_resolved")));
		TestEqual(TEXT("Damage amount"), Trace.DamageAmount, 2);
		TestFalse(TEXT("Damage not prevented"), Trace.bDamagePrevented);
		TestEqual(TEXT("Prevented amount"), Trace.PreventedDamageAmount, 0);
		TestEqual(TEXT("Final damage amount"), Trace.FinalDamageAmount, 2);
		TestTrue(TEXT("No prevention reason"), Trace.PreventionReason.IsNone());

		const FString SerializedTrace = WBReplayTrace::SerializeEvent(Trace);
		const TSharedPtr<FJsonObject> SerializedTraceObject = ParseJsonObject(SerializedTrace);
		double SerializedFinalDamageAmount = -1.0;
		double SerializedPreventedDamageAmount = -1.0;
		TestTrue(
			TEXT("Final damage serializes"),
			SerializedTraceObject.IsValid()
			&& SerializedTraceObject->TryGetNumberField(TEXT("final_damage_amount"), SerializedFinalDamageAmount));
		TestTrue(
			TEXT("Prevented amount serializes"),
			SerializedTraceObject.IsValid()
			&& SerializedTraceObject->TryGetNumberField(TEXT("prevented_damage_amount"), SerializedPreventedDamageAmount));
		TestEqual(TEXT("Serialized final damage amount"), static_cast<int32>(SerializedFinalDamageAmount), 2);
		TestEqual(TEXT("Serialized prevented damage amount"), static_cast<int32>(SerializedPreventedDamageAmount), 0);
		TestFalse(
			TEXT("False prevention flag omitted"),
			SerializedTraceObject.IsValid() && SerializedTraceObject->HasField(TEXT("damage_prevented")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamagePreventionFrozenBreakUnchangedTest, "Wandbound.Core.DamagePreventionScaffolding.FrozenBreakBehaviorUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamagePreventionFrozenBreakUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePreventionState(3, 5);
	State.GetMutableUnitById(2)->AddStatus(FName(TEXT("Frozen")), 1);
	State.SetPendingAttackForTest(MakePreventionPendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Frozen break succeeds"), Result.bOk);
	TestFalse(TEXT("Frozen removed"), State.GetUnitById(2)->HasStatus(FName(TEXT("Frozen"))));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 5);
	TestFalse(TEXT("Pending attack cleared"), State.HasPendingAttack());
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("First trace removes Frozen"), Result.TraceEvents[0].Kind, FName(TEXT("status_removed")));
		const FWBTraceEvent& DamageTrace = Result.TraceEvents[1];
		TestEqual(TEXT("Second trace resolves damage"), DamageTrace.Kind, FName(TEXT("attack_damage_resolved")));
		TestEqual(TEXT("Frozen damage amount"), DamageTrace.DamageAmount, 0);
		TestFalse(TEXT("Frozen is not prevention"), DamageTrace.bDamagePrevented);
		TestEqual(TEXT("Frozen prevented amount"), DamageTrace.PreventedDamageAmount, 0);
		TestEqual(TEXT("Frozen final damage"), DamageTrace.FinalDamageAmount, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeathResolutionDefaultCleanupRemovesUnitTest, "Wandbound.Core.DamagePreventionScaffolding.ZeroHPCleanupUsesDefaultDeathResolution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeathResolutionDefaultCleanupRemovesUnitTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePreventionState();
	State.GetMutableUnitById(2)->HP = 0;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	const FWBUnitState* Target = State.GetUnitById(2);
	TestTrue(TEXT("Cleanup succeeds"), Result.bOk);
	TestTrue(TEXT("Target defeated"), Target != nullptr && Target->bDefeated);
	TestTrue(TEXT("Target removed"), Target != nullptr && Target->bRemovedFromBoard);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("Defeated trace first"), Result.TraceEvents[0].Kind, FName(TEXT("unit_defeated")));
		TestEqual(TEXT("Removed trace second"), Result.TraceEvents[1].Kind, FName(TEXT("unit_removed_from_board")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeathResolutionDefaultHeroCleanupLosesGameTest, "Wandbound.Core.DamagePreventionScaffolding.HeroDeathUsesDefaultDeathResolution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeathResolutionDefaultHeroCleanupLosesGameTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePreventionState();
	State.GetMutablePlayerById(1)->HeroUnitId = 2;
	State.GetMutableUnitById(2)->HP = 0;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestTrue(TEXT("Hero cleanup succeeds"), Result.bOk);
	TestTrue(TEXT("Game over"), State.bGameOver);
	TestEqual(TEXT("Winner is opponent"), State.WinnerPlayerId, 0);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Hero defeated trace last"), Result.TraceEvents[2].Kind, FName(TEXT("hero_defeated")));
		TestEqual(TEXT("Winner trace"), Result.TraceEvents[2].WinningPlayerId, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamagePreventionFixtureScenariosTest, "Wandbound.Core.DamagePreventionScaffolding.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamagePreventionFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("damage_resolution_default_no_prevention.json"),
		TEXT("attack_damage_uses_damage_resolution_pipeline.json"),
		TEXT("zero_hp_cleanup_uses_death_resolution_pipeline.json"),
		TEXT("damage_prevention_trace_defaults_no_prevention.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadDamageFixture(FixtureName, Fixture));
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

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), ReadExpectedOk(Fixture, bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), ApplyResult.bOk, bExpectedOk);

		FString ExpectReason;
		TestTrue(*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s final state: %s"), *FixtureName, *ExpectReason), ExpectFinalTurnState(Fixture, State, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s pending attack"), *FixtureName), ExpectPendingAttackActive(*this, FixtureName, Fixture, State));
	}

	return true;
}

#endif
