#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardZoneState.h"
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

FWBUnitState MakeZeroHPUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 ATK = 1,
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

FWBGameStateData MakeZeroHPState()
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

	State.AddUnitForTest(MakeZeroHPUnit(1, 0, FWBTile(4, 4), 5, 2, 1, 1));
	State.AddUnitForTest(MakeZeroHPUnit(2, 1, FWBTile(5, 4), 5, 1, 1, 1));
	State.AddUnitForTest(MakeZeroHPUnit(3, 1, FWBTile(7, 4), 5, 1, 1, 1));
	return State;
}

void AddZeroHPPlayerZones(FWBGameStateData& State)
{
	if (State.GetCardZoneState().PlayerZones.Num() > 0)
	{
		return;
	}

	FWBPlayerCardZoneState Player0Zones;
	Player0Zones.PlayerId = 0;
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(Player0Zones);

	FWBPlayerCardZoneState Player1Zones;
	Player1Zones.PlayerId = 1;
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(Player1Zones);
}

void AddZeroHPEquippedCard(
	FWBGameStateData& State,
	const int32 OwnerPlayerId,
	const int32 UnitId,
	const FString& InstanceId,
	const FString& CardId,
	const FString& SlotId,
	const int32 EquipOrder)
{
	FWBEquippedCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OwnerPlayerId;
	Entry.EquippedToUnitId = UnitId;
	Entry.SlotId = SlotId;
	Entry.EquipOrder = EquipOrder;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Entry);
}

const FWBPlayerCardZoneState* GetZeroHPPlayerZones(const FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), PlayerId);
}

FWBAction MakeMoveAction(const int32 UnitId, const FWBTile& FromTile, const FWBTile& ToTile)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = UnitId;
	Action.FromTile = FromTile;
	Action.ToTile = ToTile;
	return Action;
}

FWBAction MakeAttackAction(const int32 AttackerId = 1, const int32 DefenderId = 2)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = AttackerId;
	Action.TargetUnitId = DefenderId;
	return Action;
}

FWBPendingAttackState MakePendingAttack(
	const int32 AttackerId = 1,
	const int32 DefenderId = 2,
	const int32 PlayerId = 0)
{
	FWBPendingAttackState PendingAttack;
	PendingAttack.bActive = true;
	PendingAttack.AttackerUnitId = AttackerId;
	PendingAttack.DefenderUnitId = DefenderId;
	PendingAttack.AttackingPlayerId = PlayerId;
	PendingAttack.AttackerTile = FWBTile(4, 4);
	PendingAttack.DefenderTile = FWBTile(5, 4);
	PendingAttack.DeclarationActionId = FString::Printf(TEXT("attack:p%d:u%d:t%d"), PlayerId, AttackerId, DefenderId);
	return PendingAttack;
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

bool LoadFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
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

bool IsUnitInPublicBoardSummary(const FWBPublicBoardSummary& Summary, const int32 UnitId)
{
	for (const FWBPublicUnitBoardSummary& Unit : Summary.Units)
	{
		if (Unit.UnitId == UnitId)
		{
			return true;
		}
	}

	return false;
}

bool ExpectPublicBoardAbsentUnits(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBPublicBoardSummary& Summary,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TArray<TSharedPtr<FJsonValue>>* AbsentUnitIds = nullptr;
	if (!Expected.IsValid()
		|| !Expected->TryGetArrayField(TEXT("public_board_absent_unit_ids"), AbsentUnitIds)
		|| AbsentUnitIds == nullptr)
	{
		OutReason.Reset();
		return true;
	}

	for (const TSharedPtr<FJsonValue>& UnitIdValue : *AbsentUnitIds)
	{
		if (!UnitIdValue.IsValid() || UnitIdValue->Type != EJson::Number)
		{
			OutReason = TEXT("malformed_public_board_absent_unit_ids");
			return false;
		}

		const int32 UnitId = static_cast<int32>(UnitIdValue->AsNumber());
		if (IsUnitInPublicBoardSummary(Summary, UnitId))
		{
			OutReason = FString::Printf(TEXT("public_board_unit_%d_should_be_absent"), UnitId);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPNonHeroRemovedTest, "Wandbound.Core.ZeroHPDeathRemoval.NonHeroRemoved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPNonHeroRemovedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutableUnitById(2)->HP = 0;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestTrue(TEXT("Cleanup succeeds"), Result.bOk);
	const FWBUnitState* Unit = State.GetUnitById(2);
	TestTrue(TEXT("Unit record remains"), Unit != nullptr);
	TestTrue(TEXT("Unit defeated"), Unit != nullptr && Unit->bDefeated);
	TestTrue(TEXT("Unit removed"), Unit != nullptr && Unit->bRemovedFromBoard);
	TestFalse(TEXT("Unit not on board"), Unit != nullptr && Unit->IsUnitOnBoard());
	TestEqual(TEXT("X cleared"), Unit != nullptr ? Unit->X : 99, -1);
	TestEqual(TEXT("Y cleared"), Unit != nullptr ? Unit->Y : 99, -1);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("First trace"), Result.TraceEvents[0].Kind, FName(TEXT("unit_defeated")));
		TestEqual(TEXT("Second trace"), Result.TraceEvents[1].Kind, FName(TEXT("unit_removed_from_board")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPNoUnitsFailsTest, "Wandbound.Core.ZeroHPDeathRemoval.NoUnitsFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPNoUnitsFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	const FWBGameStateData Before = State;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestFalse(TEXT("Cleanup fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("no_zero_hp_units")));
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	TestEqual(TEXT("Defender HP unchanged"), State.GetUnitById(2)->HP, Before.GetUnitById(2)->HP);
	TestFalse(TEXT("Defender not defeated"), State.GetUnitById(2)->bDefeated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPMultipleOrderTest, "Wandbound.Core.ZeroHPDeathRemoval.MultipleOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPMultipleOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = 1;
	State.Players.Add(Player0);
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 99;
	State.Players.Add(Player1);
	State.AddUnitForTest(MakeZeroHPUnit(4, 1, FWBTile(6, 6), 0));
	State.AddUnitForTest(MakeZeroHPUnit(2, 1, FWBTile(3, 4), 0));
	State.AddUnitForTest(MakeZeroHPUnit(3, 1, FWBTile(2, 4), 0));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestTrue(TEXT("Cleanup succeeds"), Result.bOk);
	TestEqual(TEXT("Six traces"), Result.TraceEvents.Num(), 6);
	if (Result.TraceEvents.Num() >= 5)
	{
		TestEqual(TEXT("First removed unit"), Result.TraceEvents[0].TargetUnitId, 2);
		TestEqual(TEXT("Second removed unit"), Result.TraceEvents[2].TargetUnitId, 3);
		TestEqual(TEXT("Third removed unit"), Result.TraceEvents[4].TargetUnitId, 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPEquippedCardsDiscardedTest, "Wandbound.Core.ZeroHPDeathRemoval.EquippedCardsDiscarded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPEquippedCardsDiscardedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	AddZeroHPPlayerZones(State);
	AddZeroHPEquippedCard(State, 1, 2, TEXT("wand_b"), TEXT("test_wand_b"), TEXT("wand_2"), 2);
	AddZeroHPEquippedCard(State, 1, 2, TEXT("wand_a"), TEXT("test_wand_a"), TEXT("wand_1"), 1);
	AddZeroHPEquippedCard(State, 0, 1, TEXT("survivor_wand"), TEXT("test_survivor_wand"), TEXT("wand_1"), 1);

	FWBUnitState* DefeatedUnit = State.GetMutableUnitById(2);
	DefeatedUnit->HP = 0;
	DefeatedUnit->SetCanonicalRL(3, 5, 2);
	FWBResonanceModifierState Modifier;
	Modifier.SourceId = TEXT("temporary_rl");
	Modifier.Amount = 2;
	DefeatedUnit->ResonanceModifiers.Add(Modifier);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestTrue(TEXT("Cleanup succeeds"), Result.bOk);
	TestEqual(TEXT("Survivor equipment remains"), State.GetCardZoneState().EquippedCards.Num(), 1);
	if (State.GetCardZoneState().EquippedCards.Num() == 1)
	{
		TestEqual(TEXT("Survivor equipment identity"), State.GetCardZoneState().EquippedCards[0].Card.InstanceId, FString(TEXT("survivor_wand")));
	}

	const FWBPlayerCardZoneState* PlayerZones = GetZeroHPPlayerZones(State, 1);
	TestTrue(TEXT("Defeated owner zones remain"), PlayerZones != nullptr);
	if (PlayerZones != nullptr)
	{
		TestEqual(TEXT("Two cards discarded"), PlayerZones->Discard.Num(), 2);
		if (PlayerZones->Discard.Num() == 2)
		{
			TestEqual(TEXT("First discard follows equip order"), PlayerZones->Discard[0].Card.InstanceId, FString(TEXT("wand_a")));
			TestEqual(TEXT("Second discard follows equip order"), PlayerZones->Discard[1].Card.InstanceId, FString(TEXT("wand_b")));
		}
	}

	DefeatedUnit = State.GetMutableUnitById(2);
	TestEqual(TEXT("RL used cleared"), DefeatedUnit->RLUsed, 0);
	TestEqual(TEXT("Current RL reset to base"), DefeatedUnit->CurrentRL, DefeatedUnit->BaseRL);
	TestEqual(TEXT("RL total mirror reset"), DefeatedUnit->RLTotal, DefeatedUnit->BaseRL);
	TestEqual(TEXT("Temporary RL modifiers cleared"), DefeatedUnit->ResonanceModifiers.Num(), 0);

	TestEqual(TEXT("Four cleanup traces"), Result.TraceEvents.Num(), 4);
	if (Result.TraceEvents.Num() == 4)
	{
		TestEqual(TEXT("First card trace"), Result.TraceEvents[0].Kind, FName(TEXT("equipped_card_discarded_on_death")));
		TestEqual(TEXT("First card identity"), Result.TraceEvents[0].CardInstanceId, FString(TEXT("wand_a")));
		TestEqual(TEXT("First discard index"), Result.TraceEvents[0].DiscardIndex, 0);
		TestEqual(TEXT("Second card identity"), Result.TraceEvents[1].CardInstanceId, FString(TEXT("wand_b")));
		TestEqual(TEXT("Defeat follows equipment"), Result.TraceEvents[2].Kind, FName(TEXT("unit_defeated")));
		TestEqual(TEXT("Removal is last"), Result.TraceEvents[3].Kind, FName(TEXT("unit_removed_from_board")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPEquipmentCleanupDeterministicTest, "Wandbound.Core.ZeroHPDeathRemoval.EquipmentCleanupDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPEquipmentCleanupDeterministicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData StateA = MakeZeroHPState();
	FWBGameStateData StateB = MakeZeroHPState();
	AddZeroHPPlayerZones(StateA);
	AddZeroHPPlayerZones(StateB);
	AddZeroHPEquippedCard(StateA, 1, 2, TEXT("wand_b"), TEXT("test_wand_b"), TEXT("wand_2"), 2);
	AddZeroHPEquippedCard(StateA, 1, 2, TEXT("wand_a"), TEXT("test_wand_a"), TEXT("wand_1"), 1);
	AddZeroHPEquippedCard(StateB, 1, 2, TEXT("wand_a"), TEXT("test_wand_a"), TEXT("wand_1"), 1);
	AddZeroHPEquippedCard(StateB, 1, 2, TEXT("wand_b"), TEXT("test_wand_b"), TEXT("wand_2"), 2);
	StateA.GetMutableUnitById(2)->HP = 0;
	StateB.GetMutableUnitById(2)->HP = 0;

	const FWBApplyActionResult ResultA = WBEffectRunner::ApplyZeroHPDeathRemoval(StateA);
	const FWBApplyActionResult ResultB = WBEffectRunner::ApplyZeroHPDeathRemoval(StateB);
	TestTrue(TEXT("First cleanup succeeds"), ResultA.bOk);
	TestTrue(TEXT("Second cleanup succeeds"), ResultB.bOk);
	TestEqual(TEXT("Trace serialization is insertion-order independent"), WBReplayTrace::SerializeEvents(ResultA.TraceEvents), WBReplayTrace::SerializeEvents(ResultB.TraceEvents));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPEquipmentCleanupFailureAtomicTest, "Wandbound.Core.ZeroHPDeathRemoval.EquipmentCleanupFailureAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPEquipmentCleanupFailureAtomicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	AddZeroHPPlayerZones(State);
	AddZeroHPEquippedCard(State, 1, 2, TEXT("duplicate_wand"), TEXT("test_wand_a"), TEXT("wand_1"), 1);
	AddZeroHPEquippedCard(State, 1, 2, TEXT("duplicate_wand"), TEXT("test_wand_b"), TEXT("wand_2"), 2);
	State.GetMutableUnitById(2)->HP = 0;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestFalse(TEXT("Cleanup fails closed"), Result.bOk);
	TestTrue(TEXT("Failure identifies duplicate instance"), Result.Reason.Contains(TEXT("duplicate_instance_id")));
	const FWBUnitState* Unit = State.GetUnitById(2);
	TestTrue(TEXT("Unit remains on board"), Unit != nullptr && Unit->IsUnitOnBoard());
	TestFalse(TEXT("Unit remains undefeated"), Unit != nullptr && Unit->bDefeated);
	TestEqual(TEXT("Equipped cards unchanged"), State.GetCardZoneState().EquippedCards.Num(), 2);
	const FWBPlayerCardZoneState* PlayerZones = GetZeroHPPlayerZones(State, 1);
	TestEqual(TEXT("Discard remains unchanged"), PlayerZones != nullptr ? PlayerZones->Discard.Num() : -1, 0);
	TestEqual(TEXT("No partial traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPPendingAttackClearedTest, "Wandbound.Core.ZeroHPDeathRemoval.PendingAttackCleared", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPPendingAttackClearedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutableUnitById(2)->HP = 0;
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestTrue(TEXT("Cleanup succeeds"), Result.bOk);
	TestFalse(TEXT("Pending attack involving removed unit is cleared"), State.HasPendingAttack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPSimultaneousHeroDeathFailsClosedTest, "Wandbound.Core.ZeroHPDeathRemoval.SimultaneousHeroDeathFailsClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPSimultaneousHeroDeathFailsClosedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutablePlayerById(0)->HeroUnitId = 1;
	State.GetMutablePlayerById(1)->HeroUnitId = 2;
	State.GetMutableUnitById(1)->HP = 0;
	State.GetMutableUnitById(2)->HP = 0;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestFalse(TEXT("Unsupported simultaneous hero death fails"), Result.bOk);
	TestEqual(TEXT("Explicit reason"), Result.Reason, FString(TEXT("simultaneous_hero_death_unsupported")));
	TestTrue(TEXT("First hero remains on board"), State.GetUnitById(1)->IsUnitOnBoard());
	TestTrue(TEXT("Second hero remains on board"), State.GetUnitById(2)->IsUnitOnBoard());
	TestFalse(TEXT("Game over is not partially committed"), State.bGameOver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPBurnInvokesEquipmentCleanupTest, "Wandbound.Core.ZeroHPDeathRemoval.BurnInvokesEquipmentCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPBurnInvokesEquipmentCleanupTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	AddZeroHPPlayerZones(State);
	AddZeroHPEquippedCard(State, 1, 2, TEXT("burn_wand"), TEXT("test_burn_wand"), TEXT("wand_1"), 1);
	FWBUnitState* Unit = State.GetMutableUnitById(2);
	Unit->HP = 1;
	Unit->AddStatus(FName(TEXT("Burn")), 1);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);
	TestTrue(TEXT("End-turn ticks succeed"), Result.bOk);
	Unit = State.GetMutableUnitById(2);
	TestTrue(TEXT("Burned unit defeated"), Unit != nullptr && Unit->bDefeated);
	TestTrue(TEXT("Burned unit removed"), Unit != nullptr && Unit->bRemovedFromBoard);
	const FWBPlayerCardZoneState* PlayerZones = GetZeroHPPlayerZones(State, 1);
	TestEqual(TEXT("Equipment discarded"), PlayerZones != nullptr ? PlayerZones->Discard.Num() : -1, 1);
	TestEqual(TEXT("No equipped cards remain"), State.GetCardZoneState().EquippedCards.Num(), 0);

	bool bSawStatusTick = false;
	bool bSawEquipmentCleanup = false;
	bool bSawUnitRemoval = false;
	for (const FWBTraceEvent& Event : Result.TraceEvents)
	{
		bSawStatusTick |= Event.Kind == FName(TEXT("status_tick"));
		bSawEquipmentCleanup |= Event.Kind == FName(TEXT("equipped_card_discarded_on_death"));
		bSawUnitRemoval |= Event.Kind == FName(TEXT("unit_removed_from_board"));
	}
	TestTrue(TEXT("Burn trace emitted"), bSawStatusTick);
	TestTrue(TEXT("Equipment cleanup trace emitted"), bSawEquipmentCleanup);
	TestTrue(TEXT("Removal trace emitted"), bSawUnitRemoval);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRemovedDoesNotOccupyTest, "Wandbound.Core.ZeroHPDeathRemoval.RemovedDoesNotOccupy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRemovedDoesNotOccupyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	FWBUnitState* Unit = State.GetMutableUnitById(2);
	Unit->MarkUnitDefeated();
	Unit->RemoveUnitFromBoard();

	TestFalse(TEXT("Former tile unoccupied"), State.IsTileOccupied(FWBTile(5, 4)));
	TestEqual(TEXT("Former tile has no unit"), State.UnitIdAt(FWBTile(5, 4)), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRemovedDoesNotBlockMovementTest, "Wandbound.Core.ZeroHPDeathRemoval.RemovedDoesNotBlockMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRemovedDoesNotBlockMovementTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	FWBUnitState* Removed = State.GetMutableUnitById(2);
	Removed->MarkUnitDefeated();
	Removed->RemoveUnitFromBoard();

	FString Reason;
	TestTrue(TEXT("Move into former tile legal"), WBRules::CanMoveUnit(State, MakeMoveAction(1, FWBTile(4, 4), FWBTile(5, 4)), Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRemovedCannotMoveTest, "Wandbound.Core.ZeroHPDeathRemoval.RemovedCannotMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRemovedCannotMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	FWBUnitState* Unit = State.GetMutableUnitById(1);
	Unit->MarkUnitDefeated();
	Unit->RemoveUnitFromBoard();

	FString Reason;
	TestFalse(TEXT("Removed unit cannot move"), WBRules::CanMoveUnit(State, MakeMoveAction(1, FWBTile(-1, -1), FWBTile(5, 4)), Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("unit_removed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRemovedCannotAttackTest, "Wandbound.Core.ZeroHPDeathRemoval.RemovedCannotAttack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRemovedCannotAttackTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	FWBUnitState* Unit = State.GetMutableUnitById(1);
	Unit->MarkUnitDefeated();
	Unit->RemoveUnitFromBoard();

	const FWBActionQueryResult Query = WBRules::CanDeclareAttack(State, MakeAttackAction(1, 2));
	TestFalse(TEXT("Removed attacker cannot attack"), Query.bOk);
	TestEqual(TEXT("Reason"), Query.Reason, FString(TEXT("attacker_removed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRemovedCannotBeAttackedTest, "Wandbound.Core.ZeroHPDeathRemoval.RemovedCannotBeAttacked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRemovedCannotBeAttackedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	FWBUnitState* Unit = State.GetMutableUnitById(2);
	Unit->MarkUnitDefeated();
	Unit->RemoveUnitFromBoard();

	const FWBActionQueryResult Query = WBRules::CanDeclareAttack(State, MakeAttackAction(1, 2));
	TestFalse(TEXT("Removed defender cannot be attacked"), Query.bOk);
	TestEqual(TEXT("Reason"), Query.Reason, FString(TEXT("target_removed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRemovedExcludedFromLegalActionsTest, "Wandbound.Core.ZeroHPDeathRemoval.RemovedExcludedFromLegalActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRemovedExcludedFromLegalActionsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	FWBUnitState* RemovedFriendly = State.GetMutableUnitById(1);
	RemovedFriendly->MarkUnitDefeated();
	RemovedFriendly->RemoveUnitFromBoard();
	FWBUnitState Replacement = MakeZeroHPUnit(4, 0, FWBTile(3, 4), 5, 2, 4, 1);
	TestTrue(TEXT("Replacement active unit added"), State.AddUnitForTest(Replacement));
	FWBUnitState* RemovedTarget = State.GetMutableUnitById(2);
	RemovedTarget->MarkUnitDefeated();
	RemovedTarget->RemoveUnitFromBoard();

	const TArray<FString> ActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	for (const FString& ActionId : ActionIds)
	{
		TestFalse(*FString::Printf(TEXT("No removed source action %s"), *ActionId), ActionId.Contains(TEXT("u1")));
		TestFalse(*FString::Printf(TEXT("No removed target action %s"), *ActionId), ActionId.Contains(TEXT("t2")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRemovedExcludedFromPublicBoardTest, "Wandbound.Core.ZeroHPDeathRemoval.RemovedExcludedFromPublicBoard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRemovedExcludedFromPublicBoardTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	FWBUnitState* Unit = State.GetMutableUnitById(2);
	Unit->MarkUnitDefeated();
	Unit->RemoveUnitFromBoard();

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestFalse(TEXT("Removed unit absent"), IsUnitInPublicBoardSummary(Summary, 2));
	TestTrue(TEXT("Active unit present"), IsUnitInPublicBoardSummary(Summary, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPLethalAttackRemovesDefenderTest, "Wandbound.Core.ZeroHPDeathRemoval.LethalAttackRemovesDefender", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPLethalAttackRemovesDefenderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutableUnitById(1)->ATK = 8;
	State.GetMutableUnitById(2)->HP = 3;
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	TestFalse(TEXT("Pending cleared"), State.HasPendingAttack());
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestTrue(TEXT("Defender defeated"), Defender->bDefeated);
	TestTrue(TEXT("Defender removed"), Defender->bRemovedFromBoard);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Damage first"), Result.TraceEvents[0].Kind, FName(TEXT("attack_damage_resolved")));
		TestEqual(TEXT("Defeat second"), Result.TraceEvents[1].Kind, FName(TEXT("unit_defeated")));
		TestEqual(TEXT("Removal third"), Result.TraceEvents[2].Kind, FName(TEXT("unit_removed_from_board")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPNonlethalAttackKeepsDefenderTest, "Wandbound.Core.ZeroHPDeathRemoval.NonlethalAttackKeepsDefender", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPNonlethalAttackKeepsDefenderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutableUnitById(1)->ATK = 2;
	State.GetMutableUnitById(2)->HP = 5;
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestEqual(TEXT("Defender HP"), Defender->HP, 3);
	TestFalse(TEXT("Defender not defeated"), Defender->bDefeated);
	TestFalse(TEXT("Defender not removed"), Defender->bRemovedFromBoard);
	TestEqual(TEXT("Damage trace only"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPFrozenBreakKeepsHealthyDefenderTest, "Wandbound.Core.ZeroHPDeathRemoval.FrozenBreakKeepsHealthyDefender", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPFrozenBreakKeepsHealthyDefenderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutableUnitById(2)->AddStatus(FName(TEXT("Frozen")), 1);
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), Result.bOk);
	const FWBUnitState* Defender = State.GetUnitById(2);
	TestEqual(TEXT("HP unchanged"), Defender->HP, 5);
	TestFalse(TEXT("Defender not defeated"), Defender->bDefeated);
	TestFalse(TEXT("Defender not removed"), Defender->bRemovedFromBoard);
	TestEqual(TEXT("Status removed and damage trace only"), Result.TraceEvents.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPHeroSetsGameOverTest, "Wandbound.Core.ZeroHPDeathRemoval.HeroSetsGameOver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPHeroSetsGameOverTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutablePlayerById(1)->HeroUnitId = 2;
	State.GetMutableUnitById(2)->HP = 0;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestTrue(TEXT("Cleanup succeeds"), Result.bOk);
	TestTrue(TEXT("Game over"), State.bGameOver);
	TestEqual(TEXT("Winner"), State.WinnerPlayerId, 0);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() == 3)
	{
		TestEqual(TEXT("Hero trace last"), Result.TraceEvents[2].Kind, FName(TEXT("hero_defeated")));
		TestEqual(TEXT("Winning player trace"), Result.TraceEvents[2].WinningPlayerId, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPGameOverLegalActionsTest, "Wandbound.Core.ZeroHPDeathRemoval.GameOverLegalActionsEmpty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPGameOverLegalActionsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutablePlayerById(1)->HeroUnitId = 2;
	State.GetMutableUnitById(2)->HP = 0;
	const FWBApplyActionResult Result = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
	TestTrue(TEXT("Cleanup succeeds"), Result.bOk);

	TestEqual(TEXT("Player 0 no legal actions"), WBRules::GenerateLegalActionsForPlayer(State, 0).Num(), 0);
	TestEqual(TEXT("Player 1 no legal actions"), WBRules::GenerateLegalActionsForPlayer(State, 1).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPPendingAttackRemovedUnitFailsTest, "Wandbound.Core.ZeroHPDeathRemoval.PendingAttackRemovedUnitFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPPendingAttackRemovedUnitFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutableUnitById(2)->MarkUnitDefeated();
	State.GetMutableUnitById(2)->RemoveUnitFromBoard();
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestFalse(TEXT("Damage fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("defender_removed")));
	TestTrue(TEXT("Pending remains"), State.HasPendingAttack());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPRuntimeSummaryExcludesRemovedDefenderTest, "Wandbound.Core.ZeroHPDeathRemoval.RuntimeSummaryExcludesRemovedDefender", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPRuntimeSummaryExcludesRemovedDefenderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZeroHPState();
	State.GetMutableUnitById(1)->ATK = 8;
	State.GetMutableUnitById(2)->HP = 3;
	State.SetPendingAttackForTest(MakePendingAttack());

	const FWBApplyActionResult DamageResult = WBEffectRunner::ApplyPendingAttackDamage(State);
	TestTrue(TEXT("Damage succeeds"), DamageResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("attack_resolution"));
	Envelope.SelectedActionId = TEXT("attack_resolution");
	Envelope.ApplyResult = DamageResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	TestFalse(TEXT("Removed defender absent from summary"), IsUnitInPublicBoardSummary(Envelope.FinalPublicBoardSummary, 2));
	TestFalse(TEXT("Serialized public summary omits removed unit id"), SerializedJson.Contains(TEXT("\"unit_id\":2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBZeroHPFixtureScenariosTest, "Wandbound.Core.ZeroHPDeathRemoval.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBZeroHPFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("zero_hp_nonhero_removed_from_board.json"),
		TEXT("zero_hp_cleanup_no_units_fails.json"),
		TEXT("attack_damage_lethal_removes_defender.json"),
		TEXT("attack_damage_nonlethal_keeps_defender.json"),
		TEXT("removed_unit_no_longer_blocks_movement.json"),
		TEXT("removed_unit_not_attack_target.json"),
		TEXT("removed_unit_not_in_public_board_summary.json"),
		TEXT("zero_hp_hero_sets_game_over.json"),
		TEXT("attack_damage_lethal_hero_sets_game_over.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadFixture(FixtureName, Fixture));
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
		TestTrue(*FString::Printf(TEXT("%s final state: %s"), *FixtureName, *ExpectReason), ExpectFinalTurnState(Fixture, State, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s pending attack"), *FixtureName), ExpectPendingAttackActive(*this, FixtureName, Fixture, State));
		TestTrue(
			*FString::Printf(TEXT("%s public board absence: %s"), *FixtureName, *ExpectReason),
			ExpectPublicBoardAbsentUnits(Fixture, WBPublicBoardSummary::Build(State), ExpectReason));
	}

	return true;
}

#endif
