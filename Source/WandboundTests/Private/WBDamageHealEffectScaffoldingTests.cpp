#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBDamageEffect.h"
#include "WBEffectRequest.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBHealEffect.h"
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeDamageHealPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeDamageHealUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP,
	const int32 MaxHP,
	const int32 CurrentArmor = 0,
	const int32 MaxArmor = 0)
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
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetArmorForTest(CurrentArmor, MaxArmor);
	return Unit;
}

FWBGameStateData MakeDamageHealState(
	const int32 TargetHP = 5,
	const int32 TargetMaxHP = 5,
	const int32 TargetCurrentArmor = 0,
	const int32 TargetMaxArmor = 0)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 8;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeDamageHealPlayer(0, 2));
	State.Players.Add(MakeDamageHealPlayer(1, 0));
	State.AddUnitForTest(MakeDamageHealUnit(1, 0, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeDamageHealUnit(2, 1, FWBTile(5, 4), TargetHP, TargetMaxHP, TargetCurrentArmor, TargetMaxArmor));
	return State;
}

FWBDamageEffectRequest MakeDamageEffectRequest(
	const int32 Amount,
	const bool bBypassArmor = false,
	const int32 TargetUnitId = 2)
{
	FWBDamageEffectRequest Request;
	Request.TargetUnitId = TargetUnitId;
	Request.SourceUnitId = 1;
	Request.SourcePlayerId = 0;
	Request.Amount = Amount;
	Request.bBypassArmor = bBypassArmor;
	Request.DamageCause = FName(TEXT("Effect"));
	Request.SourceReason = FName(TEXT("test"));
	return Request;
}

FWBHealEffectRequest MakeHealEffectRequest(
	const int32 Amount,
	const int32 TargetUnitId = 2)
{
	FWBHealEffectRequest Request;
	Request.TargetUnitId = TargetUnitId;
	Request.SourceUnitId = 1;
	Request.SourcePlayerId = 0;
	Request.Amount = Amount;
	Request.SourceReason = FName(TEXT("test"));
	return Request;
}

FWBGenericEffectPayload MakeDamagePayload(
	const int32 Amount,
	const bool bBypassArmor = false,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect = MakeDamageEffectRequest(Amount, bBypassArmor, TargetUnitId);
	if (TargetUnitId == -1)
	{
		Payload.DamageEffect.TargetUnitId = -1;
	}
	return Payload;
}

FWBGenericEffectPayload MakeHealPayload(
	const int32 Amount,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect = MakeHealEffectRequest(Amount, TargetUnitId);
	if (TargetUnitId == -1)
	{
		Payload.HealEffect.TargetUnitId = -1;
	}
	return Payload;
}

FWBGenericEffectPayload MakeArmorPayload(
	const EWBArmorEffectOp Operation,
	const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = Operation;
	Payload.ArmorEffect.TargetUnitId = -1;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeStatusPayload(
	const EWBStatusEffectOp Operation,
	const FName StatusId,
	const int32 Duration)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = Operation;
	Payload.StatusEffect.TargetUnitId = -1;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBEffectRequest MakeDamageHealEffectRequest()
{
	FWBEffectRequest Request;
	Request.Source.PlayerId = 0;
	Request.Source.SourceUnitId = 1;
	Request.Source.SourceCardId = TEXT("secret_source_card_token");
	Request.Source.SourceEffectId = TEXT("secret_source_effect_token");
	Request.Target.TargetUnitId = 2;
	return Request;
}

FWBAction MakeDamageHealMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FWBAction MakeDamageHealAttackAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakePlayerOnlyAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString DamageHealFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadDamageHealFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *DamageHealFixturePath(FileName)))
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

bool TryReadBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool& bOutValue)
{
	return Object.IsValid() && Object->TryGetBoolField(FieldName, bOutValue);
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

void CompareOptionalIntegerTraceField(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TCHAR* FieldName,
	const int32 ActualValue)
{
	int32 ExpectedValue = 0;
	if (TryReadIntegerField(Expected, FieldName, ExpectedValue))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s %s"), *Label, FieldName), ActualValue, ExpectedValue);
	}
}

void CompareOptionalBoolTraceField(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TCHAR* FieldName,
	const bool bActualValue)
{
	bool bExpectedValue = false;
	if (TryReadBoolField(Expected, FieldName, bExpectedValue))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s %s"), *Label, FieldName), bActualValue, bExpectedValue);
	}
}

bool ExpectTraceFieldsFromFixture(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<FWBTraceEvent>& TraceEvents)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TArray<TSharedPtr<FJsonValue>>* TraceFields = GetArrayField(Expected, TEXT("trace_fields"));
	if (TraceFields == nullptr)
	{
		return true;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s trace field count"), *Label), TraceEvents.Num(), TraceFields->Num());
	const int32 NumToCompare = FMath::Min(TraceEvents.Num(), TraceFields->Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		const TSharedPtr<FJsonObject> ExpectedTrace = (*TraceFields)[Index].IsValid() ? (*TraceFields)[Index]->AsObject() : nullptr;
		if (!ExpectedTrace.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s trace %d malformed expected trace fields"), *Label, Index));
			return false;
		}

		const FWBTraceEvent& ActualTrace = TraceEvents[Index];
		const FString TraceLabel = FString::Printf(TEXT("%s trace %d"), *Label, Index);
		FString ExpectedKind;
		if (ExpectedTrace->TryGetStringField(TEXT("kind"), ExpectedKind))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s kind"), *TraceLabel), ActualTrace.Kind.ToString(), ExpectedKind);
		}

		FString ExpectedDamageCause;
		if (ExpectedTrace->TryGetStringField(TEXT("damage_cause"), ExpectedDamageCause))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s damage cause"), *TraceLabel), ActualTrace.DamageCause.ToString(), ExpectedDamageCause);
		}

		FString ExpectedStatusId;
		if (ExpectedTrace->TryGetStringField(TEXT("status_id"), ExpectedStatusId))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s status id"), *TraceLabel), ActualTrace.StatusId.ToString(), ExpectedStatusId);
		}

		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("player_id"), ActualTrace.PlayerId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("source_unit_id"), ActualTrace.SourceUnitId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("target_unit_id"), ActualTrace.TargetUnitId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("damage_amount"), ActualTrace.DamageAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_hp"), ActualTrace.PreviousHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_hp"), ActualTrace.NewHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_armor"), ActualTrace.PreviousArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_armor"), ActualTrace.NewArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("armor_absorbed_amount"), ActualTrace.ArmorAbsorbedAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("hp_damage_amount"), ActualTrace.HPDamageAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("heal_amount"), ActualTrace.HealAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("effective_heal_amount"), ActualTrace.EffectiveHealAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_status_turns"), ActualTrace.PreviousStatusTurns);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_status_turns"), ActualTrace.NewStatusTurns);
		CompareOptionalBoolTraceField(Test, TraceLabel, ExpectedTrace, TEXT("bypassed_armor"), ActualTrace.bBypassedArmor);
		CompareOptionalBoolTraceField(Test, TraceLabel, ExpectedTrace, TEXT("at_or_below_zero_hp"), ActualTrace.bAtOrBelowZeroHP);
	}

	return TraceEvents.Num() == TraceFields->Num();
}

bool CompareActionIdArrays(
	FAutomationTestBase& Test,
	const FString& Label,
	const TArray<FString>& Actual,
	const TArray<FString>& Expected)
{
	Test.TestEqual(*FString::Printf(TEXT("%s count"), *Label), Actual.Num(), Expected.Num());
	const int32 NumToCompare = FMath::Min(Actual.Num(), Expected.Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s action %d"), *Label, Index), Actual[Index], Expected[Index]);
	}

	return Actual == Expected;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageEffectBasicReducesHPTest, "Wandbound.Core.DamageHealEffectScaffolding.DamageBasicReducesHP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageEffectBasicReducesHPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDamageEffect(State, MakeDamageEffectRequest(2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("HP reduced"), State.GetUnitById(2)->HP, 3);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("damage_effect_resolved")));
		TestEqual(TEXT("Trace damage"), Result.TraceEvents[0].DamageAmount, 2);
		TestEqual(TEXT("Trace HP damage"), Result.TraceEvents[0].HPDamageAmount, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageEffectUsesArmorTest, "Wandbound.Core.DamageHealEffectScaffolding.DamageUsesArmor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageEffectUsesArmorTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5, 2, 2);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDamageEffect(State, MakeDamageEffectRequest(3, false));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Armor absorbed"), State.GetUnitById(2)->GetCurrentArmor(), 0);
	TestEqual(TEXT("HP reduced by spillover"), State.GetUnitById(2)->HP, 4);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Previous armor"), Result.TraceEvents[0].PreviousArmor, 2);
		TestEqual(TEXT("New armor"), Result.TraceEvents[0].NewArmor, 0);
		TestEqual(TEXT("Absorbed"), Result.TraceEvents[0].ArmorAbsorbedAmount, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageEffectBypassesArmorTest, "Wandbound.Core.DamageHealEffectScaffolding.DamageBypassesArmor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageEffectBypassesArmorTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5, 2, 2);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDamageEffect(State, MakeDamageEffectRequest(3, true));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 2);
	TestEqual(TEXT("HP reduced"), State.GetUnitById(2)->HP, 2);
	if (Result.TraceEvents.Num() == 1)
	{
		TestTrue(TEXT("Trace bypass"), Result.TraceEvents[0].bBypassedArmor);
		TestEqual(TEXT("HP damage"), Result.TraceEvents[0].HPDamageAmount, 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageEffectLethalCleanupTest, "Wandbound.Core.DamageHealEffectScaffolding.DamageLethalRunsZeroHPCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageEffectLethalCleanupTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(2, 5);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDamageEffect(State, MakeDamageEffectRequest(3));
	TestTrue(TEXT("Result ok"), Result.bOk);
	const FWBUnitState* Target = State.GetUnitById(2);
	TestTrue(TEXT("Target defeated"), Target->bDefeated);
	TestTrue(TEXT("Target removed"), Target->bRemovedFromBoard);
	TestEqual(TEXT("Target HP zero"), Target->HP, 0);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Damage trace"), Result.TraceEvents[0].Kind, FName(TEXT("damage_effect_resolved")));
		TestEqual(TEXT("Defeated trace"), Result.TraceEvents[1].Kind, FName(TEXT("unit_defeated")));
		TestEqual(TEXT("Removed trace"), Result.TraceEvents[2].Kind, FName(TEXT("unit_removed_from_board")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageEffectZeroAmountNoopTest, "Wandbound.Core.DamageHealEffectScaffolding.DamageZeroAmountNoop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageEffectZeroAmountNoopTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5, 2, 2);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDamageEffect(State, MakeDamageEffectRequest(0));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 2);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace damage amount"), Result.TraceEvents[0].DamageAmount, 0);
		TestEqual(TEXT("Trace HP damage"), Result.TraceEvents[0].HPDamageAmount, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageEffectMissingTargetFailsTest, "Wandbound.Core.DamageHealEffectScaffolding.DamageMissingTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageEffectMissingTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5, 1, 1);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDamageEffect(State, MakeDamageEffectRequest(2, false, 99));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_target_unit")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageEffectRemovedTargetFailsTest, "Wandbound.Core.DamageHealEffectScaffolding.DamageRemovedTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageEffectRemovedTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5, 1, 1);
	State.GetMutableUnitById(2)->MarkUnitDefeated();
	State.GetMutableUnitById(2)->RemoveUnitFromBoard();
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDamageEffect(State, MakeDamageEffectRequest(2));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("target_unit_removed")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBHealEffectBasicRestoresHPTest, "Wandbound.Core.DamageHealEffectScaffolding.HealBasicRestoresHP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBHealEffectBasicRestoresHPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(2, 5);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyHealEffect(State, MakeHealEffectRequest(2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("HP healed"), State.GetUnitById(2)->HP, 4);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("heal_effect_resolved")));
		TestEqual(TEXT("Heal amount"), Result.TraceEvents[0].HealAmount, 2);
		TestEqual(TEXT("Effective heal"), Result.TraceEvents[0].EffectiveHealAmount, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBHealEffectClampsToMaxHPTest, "Wandbound.Core.DamageHealEffectScaffolding.HealClampsToMaxHP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBHealEffectClampsToMaxHPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(4, 5);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyHealEffect(State, MakeHealEffectRequest(3));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("HP clamps"), State.GetUnitById(2)->HP, 5);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Effective heal"), Result.TraceEvents[0].EffectiveHealAmount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBHealEffectZeroAmountNoopTest, "Wandbound.Core.DamageHealEffectScaffolding.HealZeroAmountNoop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBHealEffectZeroAmountNoopTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(2, 5);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyHealEffect(State, MakeHealEffectRequest(0));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 2);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Effective heal"), Result.TraceEvents[0].EffectiveHealAmount, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBHealEffectMissingTargetFailsTest, "Wandbound.Core.DamageHealEffectScaffolding.HealMissingTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBHealEffectMissingTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(2, 5);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyHealEffect(State, MakeHealEffectRequest(2, 99));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_target_unit")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 2);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBHealEffectRemovedTargetFailsTest, "Wandbound.Core.DamageHealEffectScaffolding.HealRemovedTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBHealEffectRemovedTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(0, 5);
	State.GetMutableUnitById(2)->MarkUnitDefeated();
	State.GetMutableUnitById(2)->RemoveUnitFromBoard();
	const FWBApplyActionResult Result = WBEffectRunner::ApplyHealEffect(State, MakeHealEffectRequest(2));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("target_unit_removed")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 0);
	TestTrue(TEXT("Still defeated"), State.GetUnitById(2)->bDefeated);
	TestTrue(TEXT("Still removed"), State.GetUnitById(2)->bRemovedFromBoard);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestDamagePayloadTest, "Wandbound.Core.DamageHealEffectScaffolding.EffectRequestDamagePayloadSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestDamagePayloadTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5);
	FWBEffectRequest Request = MakeDamageHealEffectRequest();
	Request.Payloads.Add(MakeDamagePayload(2));
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("HP reduced"), State.GetUnitById(2)->HP, 3);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestHealPayloadTest, "Wandbound.Core.DamageHealEffectScaffolding.EffectRequestHealPayloadSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestHealPayloadTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(2, 5);
	FWBEffectRequest Request = MakeDamageHealEffectRequest();
	Request.Payloads.Add(MakeHealPayload(2));
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("HP healed"), State.GetUnitById(2)->HP, 4);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestDamageThenHealSuccessTest, "Wandbound.Core.DamageHealEffectScaffolding.EffectRequestDamageThenHealAtomicSuccess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestDamageThenHealSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5);
	FWBEffectRequest Request = MakeDamageHealEffectRequest();
	Request.Payloads.Add(MakeDamagePayload(3));
	Request.Payloads.Add(MakeHealPayload(2));
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Final HP"), State.GetUnitById(2)->HP, 4);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Parent trace"), Result.TraceEvents[0].Kind, FName(TEXT("effect_request_resolved")));
		TestEqual(TEXT("Damage trace"), Result.TraceEvents[1].Kind, FName(TEXT("damage_effect_resolved")));
		TestEqual(TEXT("Heal trace"), Result.TraceEvents[2].Kind, FName(TEXT("heal_effect_resolved")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestDamageThenHealFailureTest, "Wandbound.Core.DamageHealEffectScaffolding.EffectRequestDamageThenHealAtomicFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestDamageThenHealFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5);
	FWBEffectRequest Request = MakeDamageHealEffectRequest();
	Request.Payloads.Add(MakeDamagePayload(2));
	Request.Payloads.Add(MakeHealPayload(2, 99));
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("effect_payload_target_mismatch")));
	TestEqual(TEXT("HP rolled back"), State.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestMixedAtomicSuccessTest, "Wandbound.Core.DamageHealEffectScaffolding.MixedArmorStatusDamageHealAtomicSuccess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestMixedAtomicSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5, 1, 3);
	FWBEffectRequest Request = MakeDamageHealEffectRequest();
	Request.Payloads.Add(MakeArmorPayload(EWBArmorEffectOp::AddCurrentArmor, 1));
	Request.Payloads.Add(MakeStatusPayload(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), 2));
	Request.Payloads.Add(MakeDamagePayload(3));
	Request.Payloads.Add(MakeHealPayload(2));
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestTrue(TEXT("Result ok"), Result.bOk);
	const FWBUnitState* Target = State.GetUnitById(2);
	TestEqual(TEXT("Final HP"), Target->HP, 5);
	TestEqual(TEXT("Final armor"), Target->GetCurrentArmor(), 0);
	TestTrue(TEXT("Burn applied"), Target->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Burn turns"), Target->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 2);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageHealHiddenDataExcludedTest, "Wandbound.Core.DamageHealEffectScaffolding.HiddenDataExcludedFromRuntimeSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageHealHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeDamageHealState(5, 5);
	State.Players[0].Deck.Add(TEXT("secret_deck_card_token"));
	State.Players[0].Hand.Add(TEXT("secret_hand_card_token"));
	State.Players[0].Discard.Add(TEXT("secret_discard_card_token"));

	FWBEffectRequest Request = MakeDamageHealEffectRequest();
	Request.Payloads.Add(MakeDamagePayload(2));
	Request.Payloads.Add(MakeHealPayload(1));
	const FWBEffectRequestResult EffectResult = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestTrue(TEXT("Effect request succeeds"), EffectResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("effect_request_test"));
	Envelope.SelectedActionId = TEXT("effect_request_test");
	Envelope.ApplyResult.bOk = EffectResult.bOk;
	Envelope.ApplyResult.Reason = EffectResult.Reason;
	Envelope.ApplyResult.TraceEvents = EffectResult.TraceEvents;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	const TArray<FString> ForbiddenSubstrings = {
		TEXT("secret_deck_card_token"),
		TEXT("secret_hand_card_token"),
		TEXT("secret_discard_card_token"),
		TEXT("secret_source_card_token"),
		TEXT("secret_source_effect_token"),
		TEXT("\"deck\""),
		TEXT("\"hand\""),
		TEXT("\"discard\""),
		TEXT("pending_attack")
	};
	for (const FString& Forbidden : ForbiddenSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Forbidden substring excluded: %s"), *Forbidden), SerializedJson.Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageHealLegalGenerationUnchangedTest, "Wandbound.Core.DamageHealEffectScaffolding.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageHealLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeDamageHealState(5, 5);
	FWBGameStateData MutatedState = MakeDamageHealState(5, 5);
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(BaselineState, 0));
	FWBEffectRequest Request = MakeDamageHealEffectRequest();
	Request.Payloads.Add(MakeDamagePayload(1));
	Request.Payloads.Add(MakeHealPayload(1));
	WBEffectRunner::ApplyEffectRequest(MutatedState, Request);
	const TArray<FString> MutatedIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MutatedState, 0));
	CompareActionIdArrays(*this, TEXT("legal action ids"), MutatedIds, BaselineIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageHealActionCodecUnchangedTest, "Wandbound.Core.DamageHealEffectScaffolding.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageHealActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeDamageHealMoveAction()), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeDamageHealAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageHealFixtureScenariosTest, "Wandbound.Core.DamageHealEffectScaffolding.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageHealFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("damage_effect_basic_reduces_hp.json"),
		TEXT("damage_effect_armor_absorbs_damage.json"),
		TEXT("damage_effect_bypass_armor.json"),
		TEXT("damage_effect_lethal_runs_zero_hp_cleanup.json"),
		TEXT("damage_effect_missing_target_fails_no_mutation.json"),
		TEXT("heal_effect_basic_restores_hp.json"),
		TEXT("heal_effect_clamps_to_max_hp.json"),
		TEXT("heal_effect_removed_target_fails_no_mutation.json"),
		TEXT("heal_effect_zero_amount_noop.json"),
		TEXT("effect_request_damage_effect_basic.json"),
		TEXT("effect_request_heal_effect_basic.json"),
		TEXT("effect_request_damage_then_heal_atomic_success.json"),
		TEXT("effect_request_damage_then_heal_atomic_failure.json"),
		TEXT("effect_request_mixed_armor_status_damage_heal_atomic_success.json"),
		TEXT("runtime_result_serialization_after_damage_heal_effect_request.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadDamageHealFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(
			*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason),
			BuildGameStateFromFixture(Fixture, State, StateReason));
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
		TestTrue(*FString::Printf(TEXT("%s trace fields"), *FixtureName), ExpectTraceFieldsFromFixture(*this, FixtureName, Fixture, ApplyResult.TraceEvents));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDamageHealRuntimeSerializationFixtureTest, "Wandbound.Core.DamageHealEffectScaffolding.RuntimeSerializationFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDamageHealRuntimeSerializationFixtureTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("runtime_result_serialization_after_damage_heal_effect_request.json");
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Runtime fixture loads"), LoadDamageHealFixture(FixtureName, Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(TEXT("State builds"), BuildGameStateFromFixture(Fixture, State, StateReason));
	if (!StateReason.IsEmpty())
	{
		return false;
	}

	FWBApplyActionResult ApplyResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString ApplyReason;
	TestTrue(TEXT("Effect request applies"), ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));
	TestTrue(TEXT("Effect request succeeds"), ApplyResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("effect_request_test"));
	Envelope.SelectedActionId = TEXT("effect_request_test");
	Envelope.ApplyResult = ApplyResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));

	const TSharedPtr<FJsonObject> Root = ParseJsonObject(SerializedJson);
	const TSharedPtr<FJsonObject> ExpectedJson = GetObjectField(Fixture, TEXT("expected_json"));
	int32 ExpectedUnitId = -1;
	int32 ExpectedHP = -1;
	int32 ExpectedMaxHP = -1;
	TestTrue(TEXT("Expected unit id reads"), TryReadIntegerField(ExpectedJson, TEXT("unit_id"), ExpectedUnitId));
	TestTrue(TEXT("Expected hp reads"), TryReadIntegerField(ExpectedJson, TEXT("hp"), ExpectedHP));
	TestTrue(TEXT("Expected max hp reads"), TryReadIntegerField(ExpectedJson, TEXT("max_hp"), ExpectedMaxHP));

	const TSharedPtr<FJsonObject> UnitObject = FindPublicUnitObject(Root, ExpectedUnitId);
	TestTrue(TEXT("Public unit exists"), UnitObject.IsValid());
	if (UnitObject.IsValid())
	{
		int32 HP = -1;
		int32 MaxHP = -1;
		TestTrue(TEXT("Public HP reads"), TryReadIntegerField(UnitObject, TEXT("hp"), HP));
		TestTrue(TEXT("Public MaxHP reads"), TryReadIntegerField(UnitObject, TEXT("max_hp"), MaxHP));
		TestEqual(TEXT("Public HP"), HP, ExpectedHP);
		TestEqual(TEXT("Public MaxHP"), MaxHP, ExpectedMaxHP);
	}

	const TArray<TSharedPtr<FJsonValue>>* ForbiddenSubstrings = GetArrayField(ExpectedJson, TEXT("forbidden_substrings"));
	if (ForbiddenSubstrings != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ForbiddenSubstrings)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				continue;
			}

			const FString Forbidden = Value->AsString();
			TestFalse(*FString::Printf(TEXT("Forbidden substring excluded: %s"), *Forbidden), SerializedJson.Contains(Forbidden));
		}
	}

	return true;
}

#endif
