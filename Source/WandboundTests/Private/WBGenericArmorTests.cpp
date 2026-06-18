#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBDamageResolution.h"
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

FWBUnitState MakeArmorUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 ATK = 2,
	const int32 AR = 1,
	const int32 CurrentArmor = 0,
	const int32 MaxArmor = 0,
	const int32 AttacksLeft = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("test_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = ATK;
	Unit.AR = AR;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = FMath::Max(AttacksLeft, 1);
	Unit.SetArmorForTest(CurrentArmor, MaxArmor);
	return Unit;
}

FWBPlayerStateData MakeArmorPlayer(const int32 PlayerId, const int32 RemainingMP = 2, const int32 HeroUnitId = -1)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	Player.HeroUnitId = HeroUnitId;
	return Player;
}

FWBGameStateData MakeArmorState(
	const int32 AttackerATK = 2,
	const int32 DefenderHP = 5,
	const int32 DefenderCurrentArmor = 0,
	const int32 DefenderMaxArmor = 0,
	const int32 DefenderOwnerId = 1)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 5;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeArmorPlayer(0, 2, 1));
	State.Players.Add(MakeArmorPlayer(1, 0, 3));
	State.AddUnitForTest(MakeArmorUnit(1, 0, FWBTile(4, 4), 5, 5, AttackerATK, 1, 0, 0, 2));
	State.AddUnitForTest(MakeArmorUnit(2, DefenderOwnerId, FWBTile(5, 4), DefenderHP, 5, 1, 1, DefenderCurrentArmor, DefenderMaxArmor, 1));
	State.AddUnitForTest(MakeArmorUnit(3, 1, FWBTile(7, 4), 5, 5, 1, 1, 0, 0, 1));
	return State;
}

FWBPendingAttackState MakeArmorPendingAttack(
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

FWBAction MakeArmorAttackAction()
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

FWBAction MakeArmorMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
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

FWBDamageRequest MakeArmorDamageRequest(
	const int32 BaseDamage,
	const int32 TargetUnitId = 2,
	const bool bBypassArmor = false,
	const FName DamageCause = FName(TEXT("Attack")))
{
	FWBDamageRequest Request;
	Request.DamageKind = EWBDamageKind::Attack;
	Request.SourceUnitId = 1;
	Request.TargetUnitId = TargetUnitId;
	Request.SourcePlayerId = 0;
	Request.BaseDamage = BaseDamage;
	Request.bBypassArmor = bBypassArmor;
	Request.DamageCause = DamageCause;
	return Request;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString ArmorFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadArmorFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ArmorFixturePath(FileName)))
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
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Boolean)
	{
		return false;
	}

	bOutValue = (*Value)->AsBool();
	return true;
}

bool ReadExpectedOk(const TSharedPtr<FJsonObject>& Fixture, bool& bOutOk)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	return Expected.IsValid() && Expected->TryGetBoolField(TEXT("ok"), bOutOk);
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
	return State.HasPendingAttack() == bExpectedActive;
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

		FString ExpectedStatusId;
		if (ExpectedTrace->TryGetStringField(TEXT("status_id"), ExpectedStatusId))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s status id"), *TraceLabel), ActualTrace.StatusId.ToString(), ExpectedStatusId);
		}

		FString ExpectedDamageCause;
		if (ExpectedTrace->TryGetStringField(TEXT("damage_cause"), ExpectedDamageCause))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s damage cause"), *TraceLabel), ActualTrace.DamageCause.ToString(), ExpectedDamageCause);
		}

		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("player_id"), ActualTrace.PlayerId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("damage_amount"), ActualTrace.DamageAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_hp"), ActualTrace.PreviousHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_hp"), ActualTrace.NewHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_max_hp"), ActualTrace.PreviousMaxHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_max_hp"), ActualTrace.NewMaxHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_armor"), ActualTrace.PreviousArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_armor"), ActualTrace.NewArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("armor_absorbed_amount"), ActualTrace.ArmorAbsorbedAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("hp_damage_amount"), ActualTrace.HPDamageAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("final_damage_amount"), ActualTrace.FinalDamageAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("prevented_damage_amount"), ActualTrace.PreventedDamageAmount);
		CompareOptionalBoolTraceField(Test, TraceLabel, ExpectedTrace, TEXT("bypassed_armor"), ActualTrace.bBypassedArmor);
	}

	return TraceEvents.Num() == TraceFields->Num();
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

bool ExpectRuntimeArmorJson(
	FAutomationTestBase& Test,
	const FString& Label,
	const FString& SerializedJson,
	const int32 UnitId,
	const int32 ExpectedCurrentArmor,
	const int32 ExpectedMaxArmor)
{
	const TSharedPtr<FJsonObject> Root = ParseJsonObject(SerializedJson);
	const TSharedPtr<FJsonObject> UnitObject = FindPublicUnitObject(Root, UnitId);
	Test.TestTrue(*FString::Printf(TEXT("%s unit json found"), *Label), UnitObject.IsValid());
	if (!UnitObject.IsValid())
	{
		return false;
	}

	int32 CurrentArmor = -1;
	int32 MaxArmor = -1;
	Test.TestTrue(*FString::Printf(TEXT("%s current armor reads"), *Label), TryReadIntegerField(UnitObject, TEXT("current_armor"), CurrentArmor));
	Test.TestTrue(*FString::Printf(TEXT("%s max armor reads"), *Label), TryReadIntegerField(UnitObject, TEXT("max_armor"), MaxArmor));
	Test.TestEqual(*FString::Printf(TEXT("%s current armor"), *Label), CurrentArmor, ExpectedCurrentArmor);
	Test.TestEqual(*FString::Printf(TEXT("%s max armor"), *Label), MaxArmor, ExpectedMaxArmor);
	return CurrentArmor == ExpectedCurrentArmor && MaxArmor == ExpectedMaxArmor;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorAttackAbsorbsAllDamageTest, "Wandbound.Core.GenericArmor.AttackAbsorbsAllDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorAttackAbsorbsAllDamageTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorState(2, 5, 3, 3);
	State.SetPendingAttackForTest(MakeArmorPendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("Defender HP unchanged"), Defender != nullptr ? Defender->HP : -1, 5);
	TestEqual(TEXT("Defender current armor reduced"), Defender != nullptr ? Defender->GetCurrentArmor() : -1, 1);
	TestEqual(TEXT("Defender max armor unchanged"), Defender != nullptr ? Defender->GetMaxArmor() : -1, 3);
	TestFalse(TEXT("Pending attack cleared"), State.HasPendingAttack());
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Trace = Result.TraceEvents[0];
		TestEqual(TEXT("Trace armor absorbed"), Trace.ArmorAbsorbedAmount, 2);
		TestEqual(TEXT("Trace HP damage"), Trace.HPDamageAmount, 0);
		TestEqual(TEXT("Trace previous armor"), Trace.PreviousArmor, 3);
		TestEqual(TEXT("Trace new armor"), Trace.NewArmor, 1);
		TestFalse(TEXT("Attack does not bypass armor"), Trace.bBypassedArmor);
		TestEqual(TEXT("Damage cause"), Trace.DamageCause, FName(TEXT("Attack")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorAttackPartiallyAbsorbsDamageTest, "Wandbound.Core.GenericArmor.AttackPartiallyAbsorbsDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorAttackPartiallyAbsorbsDamageTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorState(5, 5, 2, 2);
	State.SetPendingAttackForTest(MakeArmorPendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("Armor depleted"), Defender != nullptr ? Defender->GetCurrentArmor() : -1, 0);
	TestEqual(TEXT("HP reduced by damage after armor"), Defender != nullptr ? Defender->HP : -1, 2);
	TestEqual(TEXT("Trace armor absorbed"), Result.TraceEvents.Num() > 0 ? Result.TraceEvents[0].ArmorAbsorbedAmount : -1, 2);
	TestEqual(TEXT("Trace HP damage"), Result.TraceEvents.Num() > 0 ? Result.TraceEvents[0].HPDamageAmount : -1, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorAttackNoArmorSameAsBeforeTest, "Wandbound.Core.GenericArmor.AttackNoArmorSameAsBefore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorAttackNoArmorSameAsBeforeTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorState(2, 5, 0, 0);
	State.SetPendingAttackForTest(MakeArmorPendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("HP reduced by full attack"), Defender != nullptr ? Defender->HP : -1, 3);
	TestEqual(TEXT("Armor stays zero"), Defender != nullptr ? Defender->GetCurrentArmor() : -1, 0);
	TestEqual(TEXT("Trace armor absorbed"), Result.TraceEvents.Num() > 0 ? Result.TraceEvents[0].ArmorAbsorbedAmount : -1, 0);
	TestEqual(TEXT("Trace HP damage"), Result.TraceEvents.Num() > 0 ? Result.TraceEvents[0].HPDamageAmount : -1, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorCannotGoNegativeTest, "Wandbound.Core.GenericArmor.ArmorCannotGoNegative", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorCannotGoNegativeTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorState(2, 5, 1, 1);

	const FWBDamageResolutionResult Result = WBDamageResolution::ResolveDamageRequest(State, MakeArmorDamageRequest(5));
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestTrue(TEXT("Damage resolves"), Result.bOk);
	TestEqual(TEXT("Armor clamps at zero"), Defender != nullptr ? Defender->GetCurrentArmor() : -1, 0);
	TestEqual(TEXT("Armor absorbed only available armor"), Result.ArmorAbsorbedAmount, 1);
	TestEqual(TEXT("HP damage is remainder"), Result.HPDamageAmount, 4);
	TestEqual(TEXT("HP reduced"), Defender != nullptr ? Defender->HP : -1, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorHPClampsAndCleanupRemovesUnitTest, "Wandbound.Core.GenericArmor.HPClampsAndCleanupRemovesUnit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorHPClampsAndCleanupRemovesUnitTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorState(6, 2, 1, 1);
	State.SetPendingAttackForTest(MakeArmorPendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("HP clamps to zero"), Defender != nullptr ? Defender->HP : -1, 0);
	TestEqual(TEXT("Armor depleted"), Defender != nullptr ? Defender->GetCurrentArmor() : -1, 0);
	TestTrue(TEXT("Defender defeated"), Defender != nullptr && Defender->bDefeated);
	TestTrue(TEXT("Defender removed"), Defender != nullptr && Defender->bRemovedFromBoard);
	TestEqual(TEXT("Trace count includes cleanup"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Damage first"), Result.TraceEvents[0].Kind, FName(TEXT("attack_damage_resolved")));
		TestEqual(TEXT("Defeated second"), Result.TraceEvents[1].Kind, FName(TEXT("unit_defeated")));
		TestEqual(TEXT("Removed third"), Result.TraceEvents[2].Kind, FName(TEXT("unit_removed_from_board")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorBurnBypassesArmorTest, "Wandbound.Core.GenericArmor.BurnBypassesArmor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorBurnBypassesArmorTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 5;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeArmorPlayer(0, 1));
	State.Players.Add(MakeArmorPlayer(1, 1));
	FWBUnitState Unit = MakeArmorUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1, 3, 3);
	Unit.AddStatus(FName(TEXT("Burn")), 2);
	State.AddUnitForTest(Unit);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);
	const FWBUnitState* BurnedUnit = State.GetUnitById(1);
	TestTrue(TEXT("End status succeeds"), Result.bOk);
	TestEqual(TEXT("HP reduced by Burn"), BurnedUnit != nullptr ? BurnedUnit->HP : -1, 4);
	TestEqual(TEXT("Armor unchanged"), BurnedUnit != nullptr ? BurnedUnit->GetCurrentArmor() : -1, 3);
	TestEqual(TEXT("Burn duration decays"), BurnedUnit != nullptr ? BurnedUnit->GetStatusTurnsRemaining(FName(TEXT("Burn"))) : -1, 1);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		const FWBTraceEvent& BurnTrace = Result.TraceEvents[1];
		TestEqual(TEXT("Burn trace status"), BurnTrace.StatusId, FName(TEXT("Burn")));
		TestTrue(TEXT("Burn bypasses armor"), BurnTrace.bBypassedArmor);
		TestEqual(TEXT("Burn armor absorbed"), BurnTrace.ArmorAbsorbedAmount, 0);
		TestEqual(TEXT("Burn HP damage"), BurnTrace.HPDamageAmount, 1);
		TestEqual(TEXT("Burn previous armor"), BurnTrace.PreviousArmor, 3);
		TestEqual(TEXT("Burn new armor"), BurnTrace.NewArmor, 3);
		TestEqual(TEXT("Burn cause"), BurnTrace.DamageCause, FName(TEXT("Burn")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorPoisonDoesNotUseArmorTest, "Wandbound.Core.GenericArmor.PoisonDoesNotUseArmor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorPoisonDoesNotUseArmorTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 5;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeArmorPlayer(0, 1));
	State.Players.Add(MakeArmorPlayer(1, 1));
	FWBUnitState Unit = MakeArmorUnit(1, 0, FWBTile(4, 4), 3, 5, 1, 1, 3, 3);
	Unit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(Unit);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, 0);
	const FWBUnitState* PoisonedUnit = State.GetUnitById(1);
	TestTrue(TEXT("Start status succeeds"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), PoisonedUnit != nullptr ? PoisonedUnit->HP : -1, 3);
	TestEqual(TEXT("MaxHP reduced"), PoisonedUnit != nullptr ? PoisonedUnit->MaxHP : -1, 4);
	TestEqual(TEXT("Armor unchanged"), PoisonedUnit != nullptr ? PoisonedUnit->GetCurrentArmor() : -1, 3);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		const FWBTraceEvent& PoisonTrace = Result.TraceEvents[1];
		TestEqual(TEXT("Poison trace status"), PoisonTrace.StatusId, FName(TEXT("Poison")));
		TestEqual(TEXT("Poison has no armor absorbed trace"), PoisonTrace.ArmorAbsorbedAmount, -1);
		TestEqual(TEXT("Poison has no HP damage trace"), PoisonTrace.HPDamageAmount, -1);
		TestFalse(TEXT("Poison not marked bypass"), PoisonTrace.bBypassedArmor);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorPublicSummaryAndRuntimeJsonTest, "Wandbound.Core.GenericArmor.PublicSummaryAndRuntimeJsonIncludeArmor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorPublicSummaryAndRuntimeJsonTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 5;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeArmorPlayer(0, 1));
	State.Players.Add(MakeArmorPlayer(1, 0));
	State.AddUnitForTest(MakeArmorUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1, 2, 3));

	const FWBPublicBoardSummary BoardSummary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("One public unit"), BoardSummary.Units.Num(), 1);
	if (BoardSummary.Units.Num() == 1)
	{
		TestEqual(TEXT("Summary current armor"), BoardSummary.Units[0].CurrentArmor, 2);
		TestEqual(TEXT("Summary max armor"), BoardSummary.Units[0].MaxArmor, 3);
	}

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("armor_summary"));
	Envelope.SelectedActionId = TEXT("armor_summary");
	Envelope.ApplyResult.bOk = true;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = BoardSummary;

	FString SerializedJson;
	TestTrue(TEXT("Runtime serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	TestTrue(TEXT("Runtime armor JSON matches"), ExpectRuntimeArmorJson(*this, TEXT("runtime armor"), SerializedJson, 1, 2, 3));
	TestFalse(TEXT("Pending attack excluded"), SerializedJson.Contains(TEXT("pending_attack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorActionIdsUnchangedTest, "Wandbound.Core.GenericArmor.ActionIdsUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorActionIdsUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeArmorMoveAction()), FString(TEXT("move:p0:u1:4,4->5,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeArmorAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorLegalGenerationUnchangedTest, "Wandbound.Core.GenericArmor.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData NoArmorState = MakeArmorState(2, 5, 0, 0);
	const FWBGameStateData ArmorState = MakeArmorState(2, 5, 3, 3);
	const TArray<FString> NoArmorActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(NoArmorState, 0));
	const TArray<FString> ArmorActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(ArmorState, 0));
	CompareActionIdArrays(*this, TEXT("legal action ids"), ArmorActionIds, NoArmorActionIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorAttackTraceSerializesArmorFieldsTest, "Wandbound.Core.GenericArmor.AttackTraceSerializesArmorFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorAttackTraceSerializesArmorFieldsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorState(5, 5, 2, 2);
	State.SetPendingAttackForTest(MakeArmorPendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() != 1)
	{
		return false;
	}

	const FString SerializedTrace = WBReplayTrace::SerializeEvent(Result.TraceEvents[0]);
	const TSharedPtr<FJsonObject> TraceObject = ParseJsonObject(SerializedTrace);
	int32 PreviousArmor = -1;
	int32 NewArmor = -1;
	int32 ArmorAbsorbed = -1;
	int32 HPDamage = -1;
	FString DamageCause;
	TestTrue(TEXT("previous_armor serializes"), TryReadIntegerField(TraceObject, TEXT("previous_armor"), PreviousArmor));
	TestTrue(TEXT("new_armor serializes"), TryReadIntegerField(TraceObject, TEXT("new_armor"), NewArmor));
	TestTrue(TEXT("armor_absorbed_amount serializes"), TryReadIntegerField(TraceObject, TEXT("armor_absorbed_amount"), ArmorAbsorbed));
	TestTrue(TEXT("hp_damage_amount serializes"), TryReadIntegerField(TraceObject, TEXT("hp_damage_amount"), HPDamage));
	TestTrue(TEXT("damage_cause serializes"), TraceObject.IsValid() && TraceObject->TryGetStringField(TEXT("damage_cause"), DamageCause));
	TestFalse(TEXT("False bypass flag omitted"), TraceObject.IsValid() && TraceObject->HasField(TEXT("bypassed_armor")));
	TestEqual(TEXT("Serialized previous armor"), PreviousArmor, 2);
	TestEqual(TEXT("Serialized new armor"), NewArmor, 0);
	TestEqual(TEXT("Serialized armor absorbed"), ArmorAbsorbed, 2);
	TestEqual(TEXT("Serialized HP damage"), HPDamage, 3);
	TestEqual(TEXT("Serialized damage cause"), DamageCause, FString(TEXT("Attack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorFixtureScenariosTest, "Wandbound.Core.GenericArmor.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("armor_attack_absorbs_all_damage.json"),
		TEXT("armor_attack_absorbs_partial_damage.json"),
		TEXT("armor_attack_no_armor_same_as_before.json"),
		TEXT("armor_burn_bypasses_armor.json"),
		TEXT("armor_poison_does_not_use_armor.json"),
		TEXT("armor_lethal_after_partial_absorb_removes_unit.json"),
		TEXT("damage_trace_armor_absorption_fields.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadArmorFixture(FixtureName, Fixture));
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
		TestTrue(*FString::Printf(TEXT("%s pending attack"), *FixtureName), ExpectPendingAttack(*this, FixtureName, Fixture, State));
		TestTrue(*FString::Printf(TEXT("%s trace fields"), *FixtureName), ExpectTraceFieldsFromFixture(*this, FixtureName, Fixture, ApplyResult.TraceEvents));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBGenericArmorRuntimeSerializationFixtureTest, "Wandbound.Core.GenericArmor.RuntimeSerializationFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBGenericArmorRuntimeSerializationFixtureTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("runtime_result_serialization_armor_fields.json");
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Runtime armor fixture loads"), LoadArmorFixture(FixtureName, Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(*FString::Printf(TEXT("State builds: %s"), *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
	if (!StateReason.IsEmpty())
	{
		return false;
	}

	FWBRuntimeSelectedActionResult Envelope;
	FString SerializedJson;
	int32 RollSourceRemainingCount = -1;
	FString ApplyReason;
	const bool bApplied = ApplyRuntimeSelectedActionWithResultAndSerializeFixture(
		Fixture,
		State,
		Envelope,
		SerializedJson,
		RollSourceRemainingCount,
		ApplyReason);
	TestTrue(*FString::Printf(TEXT("Runtime fixture applies and serializes: %s"), *ApplyReason), bApplied);
	if (!bApplied)
	{
		return false;
	}

	FString ExpectReason;
	TestTrue(*FString::Printf(TEXT("Trace order: %s"), *ExpectReason), ExpectTraceOrder(Fixture, Envelope.ApplyResult.TraceEvents, ExpectReason));
	TestTrue(*FString::Printf(TEXT("Final units: %s"), *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
	TestTrue(TEXT("Runtime armor JSON matches"), ExpectRuntimeArmorJson(*this, FixtureName, SerializedJson, 1, 2, 3));
	TestEqual(TEXT("Queued roll remains"), RollSourceRemainingCount, 1);
	TestFalse(TEXT("Pending attack excluded"), SerializedJson.Contains(TEXT("pending_attack")));
	return true;
}

#endif
