#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBUnitState MakeAttackDamageUnit(
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

FWBGameStateData MakeAttackDamageState(
	const int32 AttackerATK = 2,
	const int32 DefenderHP = 5,
	const int32 DefenderOwnerId = 1)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.RemainingMP = 2;
	State.Players.Add(Player0);
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	State.Players.Add(Player1);
	State.AddUnitForTest(MakeAttackDamageUnit(1, 0, FWBTile(4, 4), 5, AttackerATK, 1, 2));
	State.AddUnitForTest(MakeAttackDamageUnit(2, DefenderOwnerId, FWBTile(5, 4), DefenderHP, 1, 1, 1));
	return State;
}

FWBAction MakeAttackDamageAction(const int32 PlayerId = 0, const int32 AttackerId = 1, const int32 DefenderId = 2)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = AttackerId;
	Action.TargetUnitId = DefenderId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBPendingAttackState MakePendingAttack(
	const int32 AttackerId = 1,
	const int32 DefenderId = 2,
	const int32 PlayerId = 0,
	const FString& ActionId = TEXT("attack:p0:u1:t2"))
{
	FWBPendingAttackState PendingAttack;
	PendingAttack.bActive = true;
	PendingAttack.AttackerUnitId = AttackerId;
	PendingAttack.DefenderUnitId = DefenderId;
	PendingAttack.AttackingPlayerId = PlayerId;
	PendingAttack.AttackerTile = FWBTile(4, 4);
	PendingAttack.DefenderTile = FWBTile(5, 4);
	PendingAttack.DeclarationActionId = ActionId;
	return PendingAttack;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString GoldenScenarioPath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadAttackDamageFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
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

const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, Values))
	{
		return Values;
	}

	return nullptr;
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	return true;
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

bool ExpectPendingAttack(
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

	Test.TestEqual(*FString::Printf(TEXT("%s pending active"), *Label), State.HasPendingAttack(), bExpectedActive);
	if (!bExpectedActive)
	{
		return !State.HasPendingAttack();
	}

	const FWBPendingAttackState& Actual = State.PendingAttack;
	int32 ExpectedAttackerId = Actual.AttackerUnitId;
	int32 ExpectedDefenderId = Actual.DefenderUnitId;
	int32 ExpectedPlayerId = Actual.AttackingPlayerId;
	TryReadIntegerField(Pending, TEXT("attacker_unit_id"), ExpectedAttackerId);
	TryReadIntegerField(Pending, TEXT("defender_unit_id"), ExpectedDefenderId);
	TryReadIntegerField(Pending, TEXT("attacking_player_id"), ExpectedPlayerId);
	Test.TestEqual(*FString::Printf(TEXT("%s pending attacker"), *Label), Actual.AttackerUnitId, ExpectedAttackerId);
	Test.TestEqual(*FString::Printf(TEXT("%s pending defender"), *Label), Actual.DefenderUnitId, ExpectedDefenderId);
	Test.TestEqual(*FString::Printf(TEXT("%s pending player"), *Label), Actual.AttackingPlayerId, ExpectedPlayerId);

	FString ExpectedActionId = Actual.DeclarationActionId;
	Pending->TryGetStringField(TEXT("declaration_action_id"), ExpectedActionId);
	Test.TestEqual(*FString::Printf(TEXT("%s pending action id"), *Label), Actual.DeclarationActionId, ExpectedActionId);
	return true;
}

TSharedPtr<FJsonObject> FindPublicUnitObject(const TSharedPtr<FJsonObject>& Root, const int32 UnitId)
{
	const TSharedPtr<FJsonObject> BoardSummary = GetObjectField(Root, TEXT("final_public_board_summary"));
	const TArray<TSharedPtr<FJsonValue>>* Units = GetArrayField(BoardSummary, TEXT("units"));
	if (Units == nullptr)
	{
		return nullptr;
	}

	for (const TSharedPtr<FJsonValue>& UnitValue : *Units)
	{
		const TSharedPtr<FJsonObject> UnitObject = UnitValue.IsValid() ? UnitValue->AsObject() : nullptr;
		int32 CandidateUnitId = -1;
		if (TryReadIntegerField(UnitObject, TEXT("unit_id"), CandidateUnitId) && CandidateUnitId == UnitId)
		{
			return UnitObject;
		}
	}

	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclareSetsPendingAttackTest, "Wandbound.Core.AttackDamage.DeclareSetsPendingAttack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclareSetsPendingAttackTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState();
	const FWBApplyActionResult Result = WBEffectRunner::ApplyAttackDeclare(State, MakeAttackDamageAction());

	TestTrue(TEXT("Declaration succeeds"), Result.bOk);
	TestTrue(TEXT("Pending attack active"), State.HasPendingAttack());
	TestEqual(TEXT("Pending attacker"), State.PendingAttack.AttackerUnitId, 1);
	TestEqual(TEXT("Pending defender"), State.PendingAttack.DefenderUnitId, 2);
	TestEqual(TEXT("Pending attacking player"), State.PendingAttack.AttackingPlayerId, 0);
	TestEqual(TEXT("Pending declaration action id"), State.PendingAttack.DeclarationActionId, FString(TEXT("attack:p0:u1:t2")));
	TestTrue(TEXT("Pending attacker tile"), State.PendingAttack.AttackerTile == FWBTile(4, 4));
	TestTrue(TEXT("Pending defender tile"), State.PendingAttack.DefenderTile == FWBTile(5, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclareFailsWithPendingAttackTest, "Wandbound.Core.AttackDamage.DeclareFailsWithPendingAttack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclareFailsWithPendingAttackTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState();
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyAttackDeclare(State, MakeAttackDamageAction());
	TestFalse(TEXT("Declaration fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("pending_attack_already_active")));
	TestEqual(TEXT("Existing pending attacker unchanged"), State.PendingAttack.AttackerUnitId, 1);
	TestEqual(TEXT("AttacksLeft unchanged"), State.GetUnitById(1)->AttacksLeft, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageBasicReducesHPTest, "Wandbound.Core.AttackDamage.BasicReducesHP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageBasicReducesHPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState(2, 5);
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("Defender HP reduced"), State.GetUnitById(2)->HP, 3);
	TestFalse(TEXT("Pending attack cleared"), State.HasPendingAttack());
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Trace = Result.TraceEvents[0];
		TestEqual(TEXT("Trace kind"), Trace.Kind, FName(TEXT("attack_damage_resolved")));
		TestEqual(TEXT("Damage amount"), Trace.DamageAmount, 2);
		TestEqual(TEXT("Previous HP"), Trace.PreviousHP, 5);
		TestEqual(TEXT("New HP"), Trace.NewHP, 3);
		TestFalse(TEXT("Not at zero"), Trace.bAtOrBelowZeroHP);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageZeroATKNoHPChangeTest, "Wandbound.Core.AttackDamage.ZeroATKNoHPChange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageZeroATKNoHPChangeTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState(0, 5);
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("Defender HP unchanged"), State.GetUnitById(2)->HP, 5);
	TestFalse(TEXT("Pending attack cleared"), State.HasPendingAttack());
	TestEqual(TEXT("Damage amount trace"), Result.TraceEvents[0].DamageAmount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageClampsHPToZeroTest, "Wandbound.Core.AttackDamage.ClampsHPToZero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageClampsHPToZeroTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState(8, 3);
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("Defender HP clamped"), State.GetUnitById(2)->HP, 0);
	TestTrue(TEXT("Defender remains on board"), State.GetUnitById(2) != nullptr);
	TestTrue(TEXT("Trace at or below zero"), Result.TraceEvents[0].bAtOrBelowZeroHP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageMissingPendingFailsTest, "Wandbound.Core.AttackDamage.MissingPendingFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageMissingPendingFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState();

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestFalse(TEXT("Damage fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_pending_attack")));
	TestEqual(TEXT("Defender HP unchanged"), State.GetUnitById(2)->HP, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageMissingAttackerFailsTest, "Wandbound.Core.AttackDamage.MissingAttackerFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageMissingAttackerFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState();
	State.Units.RemoveAll([](const FWBUnitState& Unit) { return Unit.UnitId == 1; });
	State.SetPendingAttackForTest(MakePendingAttack(1, 2, 0));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestFalse(TEXT("Damage fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_attacker")));
	TestTrue(TEXT("Pending remains"), State.HasPendingAttack());
	TestEqual(TEXT("Defender HP unchanged"), State.GetUnitById(2)->HP, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageMissingDefenderFailsTest, "Wandbound.Core.AttackDamage.MissingDefenderFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageMissingDefenderFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState();
	State.Units.RemoveAll([](const FWBUnitState& Unit) { return Unit.UnitId == 2; });
	State.SetPendingAttackForTest(MakePendingAttack(1, 2, 0));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestFalse(TEXT("Damage fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_defender")));
	TestTrue(TEXT("Pending remains"), State.HasPendingAttack());
	TestEqual(TEXT("Attacker HP unchanged"), State.GetUnitById(1)->HP, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageFailedDoesNotMutateTest, "Wandbound.Core.AttackDamage.FailedDoesNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageFailedDoesNotMutateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState();
	State.SetPendingAttackForTest(MakePendingAttack(99, 2, 0, TEXT("attack:p0:u99:t2")));
	const int32 PreviousHP = State.GetUnitById(2)->HP;
	const FWBPendingAttackState PreviousPending = State.PendingAttack;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestFalse(TEXT("Damage fails"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, PreviousHP);
	TestTrue(TEXT("Pending remains active"), State.HasPendingAttack());
	TestEqual(TEXT("Pending attacker unchanged"), State.PendingAttack.AttackerUnitId, PreviousPending.AttackerUnitId);
	TestEqual(TEXT("Pending action unchanged"), State.PendingAttack.DeclarationActionId, PreviousPending.DeclarationActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageBreaksFrozenTargetTest, "Wandbound.Core.AttackDamage.BreaksFrozenTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageBreaksFrozenTargetTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState(2, 5);
	State.GetMutableUnitById(2)->AddStatus(FName(TEXT("Frozen")), 1);
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestFalse(TEXT("Frozen removed"), State.GetUnitById(2)->HasStatus(FName(TEXT("Frozen"))));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 5);
	TestFalse(TEXT("Pending attack cleared"), State.HasPendingAttack());
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("First trace removes status"), Result.TraceEvents[0].Kind, FName(TEXT("status_removed")));
		TestEqual(TEXT("Removed status"), Result.TraceEvents[0].StatusId, FName(TEXT("Frozen")));
		TestEqual(TEXT("Second trace resolves damage"), Result.TraceEvents[1].Kind, FName(TEXT("attack_damage_resolved")));
		TestEqual(TEXT("Frozen damage amount"), Result.TraceEvents[1].DamageAmount, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageFriendlyFrozenTargetPolicyTest, "Wandbound.Core.AttackDamage.FriendlyFrozenTargetPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageFriendlyFrozenTargetPolicyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData FriendlyNormal = MakeAttackDamageState(2, 5, 0);
	const FWBActionQueryResult FriendlyNormalQuery = WBRules::CanDeclareAttack(FriendlyNormal, MakeAttackDamageAction());
	TestFalse(TEXT("Friendly non-Frozen target stays illegal"), FriendlyNormalQuery.bOk);
	TestEqual(TEXT("Friendly non-Frozen reason"), FriendlyNormalQuery.Reason, FString(TEXT("friendly_target")));

	FWBGameStateData FriendlyFrozen = MakeAttackDamageState(2, 5, 0);
	FriendlyFrozen.GetMutableUnitById(2)->AddStatus(FName(TEXT("Frozen")), 1);
	TestTrue(TEXT("Friendly Frozen target is legal"), WBRules::CanDeclareAttack(FriendlyFrozen, MakeAttackDamageAction()).bOk);

	const FWBApplyActionResult DeclareResult = WBEffectRunner::ApplyAttackDeclare(FriendlyFrozen, MakeAttackDamageAction());
	TestTrue(TEXT("Friendly Frozen declaration applies"), DeclareResult.bOk);
	const FWBApplyActionResult DamageResult = WBEffectRunner::ApplyPendingAttackDamage(FriendlyFrozen);
	TestTrue(TEXT("Friendly Frozen damage resolution applies"), DamageResult.bOk);
	TestFalse(TEXT("Friendly target Frozen removed"), FriendlyFrozen.GetUnitById(2)->HasStatus(FName(TEXT("Frozen"))));
	TestEqual(TEXT("Friendly target HP unchanged"), FriendlyFrozen.GetUnitById(2)->HP, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageRuntimeSummaryReflectsDamageTest, "Wandbound.Core.AttackDamage.RuntimeSummaryReflectsDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageRuntimeSummaryReflectsDamageTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackDamageState(2, 5);
	State.SetPendingAttackForTest(MakePendingAttack());
	const FWBApplyActionResult DamageResult = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage applies"), DamageResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("attack_resolution"));
	Envelope.SelectedActionId = TEXT("attack_resolution");
	Envelope.ApplyResult = DamageResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(
		TEXT("Runtime damage result serializes"),
		WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	const TSharedPtr<FJsonObject> Root = ParseJsonObject(SerializedJson);
	const TSharedPtr<FJsonObject> Defender = FindPublicUnitObject(Root, 2);
	int32 DefenderHP = -1;
	TestTrue(TEXT("Defender summary found"), Defender.IsValid());
	TestTrue(TEXT("Defender HP reads"), TryReadIntegerField(Defender, TEXT("hp"), DefenderHP));
	TestEqual(TEXT("Defender HP serialized"), DefenderHP, 3);
	TestFalse(TEXT("Pending attack remains hidden"), SerializedJson.Contains(TEXT("pending_attack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageNoLegalActionGeneratedTest, "Wandbound.Core.AttackDamage.NoLegalActionGenerated", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageNoLegalActionGeneratedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeAttackDamageState();
	const TArray<FString> ActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	for (const FString& ActionId : ActionIds)
	{
		TestFalse(*FString::Printf(TEXT("No damage legal action: %s"), *ActionId), ActionId.Contains(TEXT("damage")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDamageFixtureScenariosTest, "Wandbound.Core.AttackDamage.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDamageFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("attack_damage_basic_atk_reduces_hp.json"),
		TEXT("attack_damage_zero_atk_no_hp_change.json"),
		TEXT("attack_damage_hp_clamps_to_zero.json"),
		TEXT("attack_damage_missing_pending_attack_fails.json"),
		TEXT("attack_damage_missing_attacker_fails.json"),
		TEXT("attack_damage_missing_defender_fails.json"),
		TEXT("attack_declare_sets_pending_attack.json"),
		TEXT("attack_declare_fails_if_pending_attack_exists.json"),
		TEXT("attack_damage_breaks_frozen_target.json"),
		TEXT("attack_declare_friendly_frozen_target_allowed_if_confirmed.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadAttackDamageFixture(FixtureName, Fixture));
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
		TestTrue(*FString::Printf(TEXT("%s pending attack"), *FixtureName), ExpectPendingAttack(*this, FixtureName, Fixture, State));
	}

	return true;
}

#endif
