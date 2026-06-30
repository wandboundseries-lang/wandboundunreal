#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBEquipExecution.h"
#include "WBGameStateData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 EquipPlayerId = 0;
constexpr int32 EquipOpponentId = 1;
constexpr int32 EquipHeroUnitId = 10;
constexpr int32 EquipOpponentHeroUnitId = 20;

FWBPlayerStateData MakeEquipPlayer(const int32 PlayerId, const int32 HeroUnitId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = HeroUnitId;
	return Player;
}

FWBUnitState MakeEquipUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile,
	const int32 RL = 3,
	const int32 RLUsed = 0)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = RL;
	Unit.RLUsed = RLUsed;
	return Unit;
}

FWBPlayerCardZoneState MakeEquipZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	return Zones;
}

FWBGameStateData MakeEquipState()
{
	FWBGameStateData State;
	State.CurrentPlayer = EquipPlayerId;
	State.PriorityPlayer = EquipPlayerId;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeEquipPlayer(EquipPlayerId, EquipHeroUnitId));
	State.Players.Add(MakeEquipPlayer(EquipOpponentId, EquipOpponentHeroUnitId));
	State.AddUnitForTest(MakeEquipUnit(EquipHeroUnitId, EquipPlayerId, TEXT("hero_card"), FWBTile(4, 4), 3));
	State.AddUnitForTest(MakeEquipUnit(EquipOpponentHeroUnitId, EquipOpponentId, TEXT("enemy_hero"), FWBTile(8, 8), 3));
	State.CardZoneState.PlayerZones.Add(MakeEquipZones(EquipPlayerId));
	State.CardZoneState.PlayerZones.Add(MakeEquipZones(EquipOpponentId));
	return State;
}

FWBCardDefinition MakeEquipWandDefinition(
	const FString& CardId = TEXT("equip_wand"),
	const FString& PublicName = TEXT("Copper Wand"),
	const int32 RR = 1)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = RR;
	return Definition;
}

FWBCardDefinition MakeEquipCharacterDefinition(
	const FString& CardId = TEXT("not_a_wand"))
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = TEXT("Not A Wand");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 4;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 2;
	return Definition;
}

FWBCardDefinitionRepository MakeEquipRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("equip_execution_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBZoneCardEntry MakeEquipZoneEntry(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerPlayerId,
	const EWBCardZone Zone,
	const int32 ZoneIndex)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OwnerPlayerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	return Entry;
}

FWBPlayerCardZoneState* FindEquipZones(FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), PlayerId);
}

const FWBPlayerCardZoneState* FindEquipZones(const FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), PlayerId);
}

void AddEquipHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindEquipZones(State, PlayerId))
	{
		Zones->Hand.Add(MakeEquipZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Hand, ZoneIndex));
	}
}

void AddEquipDeckCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindEquipZones(State, PlayerId))
	{
		Zones->Deck.Add(MakeEquipZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Deck, ZoneIndex));
	}
}

FWBEquipExecutionRequest MakeEquipRequest(
	const FString& InstanceId = TEXT("hand_wand"),
	const FString& CardId = TEXT("equip_wand"),
	const int32 TargetUnitId = EquipHeroUnitId)
{
	FWBEquipExecutionRequest Request;
	Request.PlayerId = EquipPlayerId;
	Request.SourceInstanceId = InstanceId;
	Request.SourceCardId = CardId;
	Request.TargetUnitId = TargetUnitId;
	return Request;
}

FWBEquipExecutionResult ExecuteDefaultEquip(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	return WBEquipExecution::ExecuteWandEquipFromHand(
		State,
		Repository,
		MakeEquipRequest());
}

bool LoadEquipSourceFile(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(OutSource, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionSuccessTest, "Wandbound.Core.EquipExecution.WandEquipFromHandSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipState();
	AddEquipHandCard(State, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	const FWBCardDefinitionRepository Repository = MakeEquipRepository({ MakeEquipWandDefinition() });

	const FWBEquipExecutionResult Result = ExecuteDefaultEquip(State, Repository);
	const FWBPlayerCardZoneState* Zones = FindEquipZones(State, EquipPlayerId);
	const FWBUnitState* Unit = State.GetUnitById(EquipHeroUnitId);

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("success")));
	TestEqual(TEXT("Hand card removed"), Zones != nullptr ? Zones->Hand.Num() : -1, 0);
	TestEqual(TEXT("One equipped card"), State.GetCardZoneState().EquippedCards.Num(), 1);
	TestEqual(TEXT("Equipped instance"), State.GetCardZoneState().EquippedCards[0].Card.InstanceId, FString(TEXT("hand_wand")));
	TestEqual(TEXT("Equipped card"), State.GetCardZoneState().EquippedCards[0].Card.CardId, FString(TEXT("equip_wand")));
	TestEqual(TEXT("Equipped owner"), State.GetCardZoneState().EquippedCards[0].Card.OwnerPlayerId, EquipPlayerId);
	TestEqual(TEXT("Equipped target"), State.GetCardZoneState().EquippedCards[0].EquippedToUnitId, EquipHeroUnitId);
	TestEqual(TEXT("Slot"), State.GetCardZoneState().EquippedCards[0].SlotId, FString(TEXT("wand")));
	TestEqual(TEXT("RL used"), Unit != nullptr ? Unit->RLUsed : -1, 1);
	TestEqual(TEXT("Result RR"), Result.RR, 1);
	TestEqual(TEXT("Before RL"), Result.RLUsedBefore, 0);
	TestEqual(TEXT("After RL"), Result.RLUsedAfter, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionHandIndexesTest, "Wandbound.Core.EquipExecution.HandIndexesNormalizeAfterEquip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionHandIndexesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipState();
	AddEquipHandCard(State, EquipPlayerId, TEXT("late_remaining"), TEXT("equip_wand"), 8);
	AddEquipHandCard(State, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"), 3);
	AddEquipHandCard(State, EquipPlayerId, TEXT("early_remaining"), TEXT("equip_wand"), 1);
	const FWBCardDefinitionRepository Repository = MakeEquipRepository({ MakeEquipWandDefinition() });

	const FWBEquipExecutionResult Result = ExecuteDefaultEquip(State, Repository);
	const FWBPlayerCardZoneState* Zones = FindEquipZones(State, EquipPlayerId);

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestEqual(TEXT("Two cards remain"), Zones != nullptr ? Zones->Hand.Num() : -1, 2);
	TestEqual(TEXT("First remaining sorted"), Zones != nullptr ? Zones->Hand[0].Card.InstanceId : FString(), FString(TEXT("early_remaining")));
	TestEqual(TEXT("First index"), Zones != nullptr ? Zones->Hand[0].ZoneIndex : -1, 0);
	TestEqual(TEXT("Second remaining sorted"), Zones != nullptr ? Zones->Hand[1].Card.InstanceId : FString(), FString(TEXT("late_remaining")));
	TestEqual(TEXT("Second index"), Zones != nullptr ? Zones->Hand[1].ZoneIndex : -1, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionTraceTest, "Wandbound.Core.EquipExecution.TraceAndJsonDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionTraceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipState();
	AddEquipHandCard(State, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	const FWBCardDefinitionRepository Repository = MakeEquipRepository({ MakeEquipWandDefinition(TEXT("equip_wand"), TEXT("Copper Wand"), 2) });

	const FWBEquipExecutionResult Result = ExecuteDefaultEquip(State, Repository);
	FString Json;
	const bool bSerialized = WBEquipExecution::EquipExecutionResultToJsonStringForTest(Result, Json);

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestEqual(TEXT("One trace"), Result.TraceEvents.Num(), 1);
	TestEqual(TEXT("Trace event"), Result.TraceEvents[0].EventType, FString(TEXT("equip_wand")));
	TestEqual(TEXT("Trace unit"), Result.TraceEvents[0].EquippedToUnitId, EquipHeroUnitId);
	TestEqual(TEXT("Trace rr"), Result.TraceEvents[0].RR, 2);
	TestEqual(TEXT("Trace before"), Result.TraceEvents[0].RLUsedBefore, 0);
	TestEqual(TEXT("Trace after"), Result.TraceEvents[0].RLUsedAfter, 2);
	TestTrue(TEXT("Json serializes"), bSerialized);
	TestTrue(TEXT("Json includes event key"), Json.Contains(TEXT("event_type")));
	TestTrue(TEXT("Json includes event value"), Json.Contains(TEXT("equip_wand")));
	TestTrue(TEXT("Json includes slot key"), Json.Contains(TEXT("slot_id")));
	TestTrue(TEXT("Json includes slot value"), Json.Contains(TEXT("wand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionDeterministicSlotTest, "Wandbound.Core.EquipExecution.SlotSelectionDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionDeterministicSlotTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipState();
	AddEquipHandCard(State, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	FWBEquippedCardEntry Existing;
	Existing.Card.InstanceId = TEXT("already_equipped");
	Existing.Card.CardId = TEXT("old_wand");
	Existing.Card.OwnerPlayerId = EquipPlayerId;
	Existing.EquippedToUnitId = EquipHeroUnitId;
	Existing.SlotId = TEXT("wand");
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Existing);
	const FWBCardDefinitionRepository Repository = MakeEquipRepository({ MakeEquipWandDefinition() });

	const FWBEquipExecutionResult Result = ExecuteDefaultEquip(State, Repository);

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestEqual(TEXT("Next slot"), Result.SlotId, FString(TEXT("wand_1")));
	TestEqual(TEXT("Equipped sorted first"), State.GetCardZoneState().EquippedCards[0].Card.InstanceId, FString(TEXT("already_equipped")));
	TestEqual(TEXT("Equipped sorted second"), State.GetCardZoneState().EquippedCards[1].Card.InstanceId, FString(TEXT("hand_wand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionFailureNoMutationTest, "Wandbound.Core.EquipExecution.FailuresDoNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionFailureNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipState();
	AddEquipHandCard(State, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	const int32 BeforeHand = FindEquipZones(State, EquipPlayerId)->Hand.Num();
	const int32 BeforeEquipped = State.GetCardZoneState().EquippedCards.Num();
	const int32 BeforeRLUsed = State.GetUnitById(EquipHeroUnitId)->RLUsed;

	const FWBEquipExecutionResult Result =
		WBEquipExecution::ExecuteWandEquipFromHand(
			State,
			MakeEquipRepository({ MakeEquipWandDefinition() }),
			MakeEquipRequest(TEXT("missing_instance")));

	TestFalse(TEXT("Equip fails"), Result.bOk);
	TestEqual(TEXT("Missing source code"), Result.Code, EWBEquipExecutionResultCode::SourceCardMissing);
	TestEqual(TEXT("Hand unchanged"), FindEquipZones(State, EquipPlayerId)->Hand.Num(), BeforeHand);
	TestEqual(TEXT("Equipped unchanged"), State.GetCardZoneState().EquippedCards.Num(), BeforeEquipped);
	TestEqual(TEXT("RL unchanged"), State.GetUnitById(EquipHeroUnitId)->RLUsed, BeforeRLUsed);
	TestEqual(TEXT("No trace on failure"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionValidationCasesTest, "Wandbound.Core.EquipExecution.ValidationCasesFailWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionValidationCasesTest::RunTest(const FString& Parameters)
{
	auto ExpectFailure = [this](
		const TCHAR* Label,
		FWBGameStateData State,
		const FWBCardDefinitionRepository& Repository,
		const FWBEquipExecutionRequest& Request,
		const EWBEquipExecutionResultCode ExpectedCode)
	{
		const int32 BeforeHand = FindEquipZones(State, EquipPlayerId) != nullptr
			? FindEquipZones(State, EquipPlayerId)->Hand.Num()
			: -1;
		const int32 BeforeEquipped = State.GetCardZoneState().EquippedCards.Num();
		const FWBUnitState* BeforeUnit = State.GetUnitById(EquipHeroUnitId);
		const int32 BeforeRLUsed = BeforeUnit != nullptr ? BeforeUnit->RLUsed : -1;

		const FWBEquipExecutionResult Result =
			WBEquipExecution::ExecuteWandEquipFromHand(State, Repository, Request);

		TestFalse(*FString::Printf(TEXT("%s fails"), Label), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s code"), Label), Result.Code, ExpectedCode);
		if (BeforeHand >= 0)
		{
			TestEqual(*FString::Printf(TEXT("%s hand unchanged"), Label), FindEquipZones(State, EquipPlayerId)->Hand.Num(), BeforeHand);
		}
		TestEqual(*FString::Printf(TEXT("%s equipped unchanged"), Label), State.GetCardZoneState().EquippedCards.Num(), BeforeEquipped);
		if (State.GetUnitById(EquipHeroUnitId) != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("%s RL unchanged"), Label), State.GetUnitById(EquipHeroUnitId)->RLUsed, BeforeRLUsed);
		}
		TestEqual(*FString::Printf(TEXT("%s no trace"), Label), Result.TraceEvents.Num(), 0);
	};

	FWBGameStateData MismatchState = MakeEquipState();
	AddEquipHandCard(MismatchState, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	ExpectFailure(
		TEXT("mismatch"),
		MismatchState,
		MakeEquipRepository({ MakeEquipWandDefinition() }),
		MakeEquipRequest(TEXT("hand_wand"), TEXT("different_card")),
		EWBEquipExecutionResultCode::SourceCardIdMismatch);

	FWBGameStateData MissingDefinitionState = MakeEquipState();
	AddEquipHandCard(MissingDefinitionState, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	ExpectFailure(
		TEXT("missing definition"),
		MissingDefinitionState,
		MakeEquipRepository({}),
		MakeEquipRequest(),
		EWBEquipExecutionResultCode::CardDefinitionNotFound);

	FWBGameStateData NonWandState = MakeEquipState();
	AddEquipHandCard(NonWandState, EquipPlayerId, TEXT("hand_wand"), TEXT("not_a_wand"));
	ExpectFailure(
		TEXT("non-wand"),
		NonWandState,
		MakeEquipRepository({ MakeEquipCharacterDefinition() }),
		MakeEquipRequest(TEXT("hand_wand"), TEXT("not_a_wand")),
		EWBEquipExecutionResultCode::SourceCardNotWand);

	FWBGameStateData InvalidRRState = MakeEquipState();
	AddEquipHandCard(InvalidRRState, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	ExpectFailure(
		TEXT("invalid rr"),
		InvalidRRState,
		MakeEquipRepository({ MakeEquipWandDefinition(TEXT("equip_wand"), TEXT("Bad Wand"), -1) }),
		MakeEquipRequest(),
		EWBEquipExecutionResultCode::InvalidRR);

	FWBGameStateData InsufficientRLState = MakeEquipState();
	AddEquipHandCard(InsufficientRLState, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	ExpectFailure(
		TEXT("insufficient rl"),
		InsufficientRLState,
		MakeEquipRepository({ MakeEquipWandDefinition(TEXT("equip_wand"), TEXT("Heavy Wand"), 4) }),
		MakeEquipRequest(),
		EWBEquipExecutionResultCode::InsufficientRL);

	FWBGameStateData EnemyTargetState = MakeEquipState();
	AddEquipHandCard(EnemyTargetState, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	ExpectFailure(
		TEXT("enemy target"),
		EnemyTargetState,
		MakeEquipRepository({ MakeEquipWandDefinition() }),
		MakeEquipRequest(TEXT("hand_wand"), TEXT("equip_wand"), EquipOpponentHeroUnitId),
		EWBEquipExecutionResultCode::TargetUnitNotOwned);

	FWBGameStateData RemovedTargetState = MakeEquipState();
	RemovedTargetState.GetMutableUnitById(EquipHeroUnitId)->RemoveUnitFromBoard();
	AddEquipHandCard(RemovedTargetState, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	ExpectFailure(
		TEXT("removed target"),
		RemovedTargetState,
		MakeEquipRepository({ MakeEquipWandDefinition() }),
		MakeEquipRequest(),
		EWBEquipExecutionResultCode::TargetUnitNotFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionObservationTest, "Wandbound.Core.EquipExecution.ObservationUpdatesWithoutHiddenLeaks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionObservationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipState();
	AddEquipHandCard(State, EquipPlayerId, TEXT("hand_wand"), TEXT("equip_wand"));
	AddEquipHandCard(State, EquipOpponentId, TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_CARD_DO_NOT_LEAK"));
	AddEquipDeckCard(State, EquipPlayerId, TEXT("SECRET_DECK_DO_NOT_LEAK"), TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	const FWBEquipExecutionResult Result =
		ExecuteDefaultEquip(State, MakeEquipRepository({ MakeEquipWandDefinition() }));
	const FWBCardZonePublicSummary PublicSummary = WBCardZoneObservation::BuildPublicSummary(State);
	const FWBCardZonePlayerObservation OwnerObservation =
		WBCardZoneObservation::BuildObservationForPlayer(State, EquipPlayerId);
	const FWBCardZonePlayerObservation OpponentObservation =
		WBCardZoneObservation::BuildObservationForPlayer(State, EquipOpponentId);

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestEqual(TEXT("Own hand removed"), OwnerObservation.OwnHand.Count, 0);
	TestEqual(TEXT("Public equipped count"), PublicSummary.Equipped.Count, 1);
	TestFalse(TEXT("Opponent cannot see former hand instance"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(OpponentObservation, TEXT("hand_wand")));
	TestFalse(TEXT("Opponent hand hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_OPPONENT_HAND")));
	TestFalse(TEXT("Deck identity hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_DECK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEquipExecutionSourceGuardTest, "Wandbound.Core.EquipExecution.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEquipExecutionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Header;
	FString Source;
	TestTrue(TEXT("Header loads"), LoadEquipSourceFile(TEXT("Source/WandboundCore/Public/WBEquipExecution.h"), Header));
	TestTrue(TEXT("Source loads"), LoadEquipSourceFile(TEXT("Source/WandboundCore/Private/WBEquipExecution.cpp"), Source));
	const FString Combined = Header + Source;

	TestFalse(TEXT("No legal action generation"), Combined.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No FWBAction"), Combined.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No WBActionCodec"), Combined.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No WBEffectRunner"), Combined.Contains(TEXT("WBEffectRunner")));
	return true;
}

#endif
