#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBGameStateData.h"
#include "WBSummonExecution.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 SummonPlayerId = 0;
constexpr int32 SummonOpponentId = 1;
constexpr int32 SummonHeroUnitId = 10;
constexpr int32 SummonOpponentHeroUnitId = 20;

FWBPlayerStateData MakeSummonPlayer(const int32 PlayerId, const int32 HeroUnitId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = HeroUnitId;
	return Player;
}

FWBUnitState MakeSummonUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile,
	const int32 RL = 3)
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
	Unit.RLUsed = 0;
	return Unit;
}

FWBPlayerCardZoneState MakeSummonZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	return Zones;
}

FWBGameStateData MakeSummonState()
{
	FWBGameStateData State;
	State.CurrentPlayer = SummonPlayerId;
	State.PriorityPlayer = SummonPlayerId;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeSummonPlayer(SummonPlayerId, SummonHeroUnitId));
	State.Players.Add(MakeSummonPlayer(SummonOpponentId, SummonOpponentHeroUnitId));
	State.AddUnitForTest(MakeSummonUnit(SummonHeroUnitId, SummonPlayerId, TEXT("hero_card"), FWBTile(4, 4)));
	State.AddUnitForTest(MakeSummonUnit(SummonOpponentHeroUnitId, SummonOpponentId, TEXT("enemy_hero"), FWBTile(8, 8)));
	State.CardZoneState.PlayerZones.Add(MakeSummonZones(SummonPlayerId));
	State.CardZoneState.PlayerZones.Add(MakeSummonZones(SummonOpponentId));
	return State;
}

FWBCardCharacterStatsDefinition MakeSummonCharacterStats(
	const int32 HP = 4,
	const int32 ATK = 2,
	const int32 AR = 1,
	const int32 RL = 2)
{
	FWBCardCharacterStatsDefinition Stats;
	Stats.HP = HP;
	Stats.ATK = ATK;
	Stats.AR = AR;
	Stats.RL = RL;
	return Stats;
}

FWBCardDefinition MakeSummonCharacterDefinition(
	const FString& CardId = TEXT("summon_character"),
	const FString& PublicName = TEXT("Summoned Student"),
	const FWBCardCharacterStatsDefinition Stats = MakeSummonCharacterStats())
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats = Stats;
	return Definition;
}

FWBCardDefinition MakeSummonWandDefinition()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("wand_not_character");
	Definition.PublicName = TEXT("Plain Wand");
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = 1;
	return Definition;
}

FWBCardDefinitionRepository MakeSummonRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("summon_execution_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBZoneCardEntry MakeSummonZoneEntry(
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

FWBPlayerCardZoneState* FindSummonZones(FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), PlayerId);
}

const FWBPlayerCardZoneState* FindSummonZones(const FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), PlayerId);
}

void AddSummonHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindSummonZones(State, PlayerId))
	{
		Zones->Hand.Add(MakeSummonZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Hand, ZoneIndex));
	}
}

void AddSummonDeckCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindSummonZones(State, PlayerId))
	{
		Zones->Deck.Add(MakeSummonZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Deck, ZoneIndex));
	}
}

void AddSummonMarker(
	FWBGameStateData& State,
	const FWBTile& Tile,
	const FString& InternalMarkerCardId = TEXT("SECRET_MARKER_DO_NOT_LEAK"))
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 31;
	Marker.OwnerPlayerId = SummonOpponentId;
	Marker.Tile = Tile;
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = InternalMarkerCardId;
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

FWBSummonExecutionRequest MakeSummonRequest(
	const FString& InstanceId = TEXT("hand_character"),
	const FString& CardId = TEXT("summon_character"),
	const FWBTile& TargetTile = FWBTile(4, 3))
{
	FWBSummonExecutionRequest Request;
	Request.PlayerId = SummonPlayerId;
	Request.SourceInstanceId = InstanceId;
	Request.SourceCardId = CardId;
	Request.TargetTile = TargetTile;
	return Request;
}

FWBSummonExecutionResult ExecuteDefaultSummon(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	return WBSummonExecution::ExecuteCharacterSummonFromHand(
		State,
		Repository,
		MakeSummonRequest());
}

bool LoadSummonSourceFile(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(OutSource, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionSuccessTest, "Wandbound.Core.SummonExecution.CharacterSummonFromHandSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSummonState();
	AddSummonHandCard(State, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	const FWBCardDefinitionRepository Repository = MakeSummonRepository({ MakeSummonCharacterDefinition() });

	const FWBSummonExecutionResult Result = ExecuteDefaultSummon(State, Repository);
	const FWBUnitState* CreatedUnit = State.GetUnitById(Result.CreatedUnitId);
	const FWBPlayerCardZoneState* Zones = FindSummonZones(State, SummonPlayerId);

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("success")));
	TestNotNull(TEXT("Created unit"), CreatedUnit);
	TestEqual(TEXT("Hand card removed"), Zones != nullptr ? Zones->Hand.Num() : -1, 0);
	TestEqual(TEXT("Unit tile X"), CreatedUnit != nullptr ? CreatedUnit->X : -1, 4);
	TestEqual(TEXT("Unit tile Y"), CreatedUnit != nullptr ? CreatedUnit->Y : -1, 3);
	TestEqual(TEXT("Unit HP"), CreatedUnit != nullptr ? CreatedUnit->HP : -1, 4);
	TestEqual(TEXT("Unit MaxHP"), CreatedUnit != nullptr ? CreatedUnit->MaxHP : -1, 4);
	TestEqual(TEXT("Unit ATK"), CreatedUnit != nullptr ? CreatedUnit->ATK : -1, 2);
	TestEqual(TEXT("Unit AR"), CreatedUnit != nullptr ? CreatedUnit->AR : -1, 1);
	TestEqual(TEXT("Unit RL total"), CreatedUnit != nullptr ? CreatedUnit->RLTotal : -1, 2);
	TestEqual(TEXT("Unit RL used"), CreatedUnit != nullptr ? CreatedUnit->RLUsed : -1, 0);
	TestEqual(TEXT("Unit CardId"), CreatedUnit != nullptr ? CreatedUnit->CardId : FString(), FString(TEXT("summon_character")));
	TestEqual(TEXT("Owner"), CreatedUnit != nullptr ? CreatedUnit->OwnerId : -1, SummonPlayerId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionUnitIdTest, "Wandbound.Core.SummonExecution.UnitIdAllocationDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionUnitIdTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSummonState();
	State.AddUnitForTest(MakeSummonUnit(42, SummonPlayerId, TEXT("late_unit"), FWBTile(0, 0)));
	State.AddUnitForTest(MakeSummonUnit(7, SummonPlayerId, TEXT("early_unit"), FWBTile(1, 0)));
	AddSummonHandCard(State, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));

	const FWBSummonExecutionResult Result =
		ExecuteDefaultSummon(State, MakeSummonRepository({ MakeSummonCharacterDefinition() }));

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestEqual(TEXT("Max UnitId plus one"), Result.CreatedUnitId, 43);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionHandIndexesTest, "Wandbound.Core.SummonExecution.HandIndexesNormalizeAfterSummon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionHandIndexesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSummonState();
	AddSummonHandCard(State, SummonPlayerId, TEXT("late_remaining"), TEXT("summon_character"), 8);
	AddSummonHandCard(State, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"), 3);
	AddSummonHandCard(State, SummonPlayerId, TEXT("early_remaining"), TEXT("summon_character"), 1);
	const FWBCardDefinitionRepository Repository = MakeSummonRepository({ MakeSummonCharacterDefinition() });

	const FWBSummonExecutionResult Result = ExecuteDefaultSummon(State, Repository);
	const FWBPlayerCardZoneState* Zones = FindSummonZones(State, SummonPlayerId);

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestEqual(TEXT("Two cards remain"), Zones != nullptr ? Zones->Hand.Num() : -1, 2);
	TestEqual(TEXT("First remaining sorted"), Zones != nullptr ? Zones->Hand[0].Card.InstanceId : FString(), FString(TEXT("early_remaining")));
	TestEqual(TEXT("First index"), Zones != nullptr ? Zones->Hand[0].ZoneIndex : -1, 0);
	TestEqual(TEXT("Second remaining sorted"), Zones != nullptr ? Zones->Hand[1].Card.InstanceId : FString(), FString(TEXT("late_remaining")));
	TestEqual(TEXT("Second index"), Zones != nullptr ? Zones->Hand[1].ZoneIndex : -1, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionBoardReferenceTest, "Wandbound.Core.SummonExecution.BoardReferenceAppearsAfterSummon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionBoardReferenceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSummonState();
	AddSummonHandCard(State, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	const FWBSummonExecutionResult Result =
		ExecuteDefaultSummon(State, MakeSummonRepository({ MakeSummonCharacterDefinition() }));
	const TArray<FWBBoardCardReference> References =
		WBCardZoneState::BuildBoardCardReferencesForTest(State);

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestTrue(TEXT("Board references include new unit"), References.ContainsByPredicate([&Result](const FWBBoardCardReference& Reference)
	{
		return Reference.UnitId == Result.CreatedUnitId
			&& Reference.CardId == TEXT("summon_character")
			&& Reference.OwnerPlayerId == SummonPlayerId;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionObservationTest, "Wandbound.Core.SummonExecution.ObservationUpdatesAfterSummon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionObservationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSummonState();
	AddSummonHandCard(State, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	AddSummonHandCard(State, SummonOpponentId, TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_CARD_DO_NOT_LEAK"));
	AddSummonDeckCard(State, SummonPlayerId, TEXT("SECRET_DECK_DO_NOT_LEAK"), TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	const FWBSummonExecutionResult Result =
		ExecuteDefaultSummon(State, MakeSummonRepository({ MakeSummonCharacterDefinition() }));
	const FWBCardZonePublicSummary PublicSummary = WBCardZoneObservation::BuildPublicSummary(State);
	const FWBCardZonePlayerObservation OwnerObservation =
		WBCardZoneObservation::BuildObservationForPlayer(State, SummonPlayerId);
	const FWBCardZonePlayerObservation OpponentObservation =
		WBCardZoneObservation::BuildObservationForPlayer(State, SummonOpponentId);

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestEqual(TEXT("Own hand removed"), OwnerObservation.OwnHand.Count, 0);
	TestFalse(TEXT("Opponent cannot see former hand instance"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(OpponentObservation, TEXT("hand_character")));
	TestFalse(TEXT("Opponent hand hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_OPPONENT_HAND")));
	TestFalse(TEXT("Deck identity hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_DECK")));
	TestTrue(TEXT("Board card public"), PublicSummary.BoardCardReferences.ContainsByPredicate([&Result](const FWBBoardCardReference& Reference)
	{
		return Reference.UnitId == Result.CreatedUnitId && Reference.CardId == TEXT("summon_character");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionFailureNoMutationTest, "Wandbound.Core.SummonExecution.FailuresDoNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionFailureNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSummonState();
	AddSummonHandCard(State, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	const int32 BeforeUnits = State.Units.Num();
	const int32 BeforeHand = FindSummonZones(State, SummonPlayerId)->Hand.Num();

	FWBSummonExecutionRequest Request = MakeSummonRequest(TEXT("missing_instance"));
	const FWBSummonExecutionResult Result =
		WBSummonExecution::ExecuteCharacterSummonFromHand(
			State,
			MakeSummonRepository({ MakeSummonCharacterDefinition() }),
			Request);

	TestFalse(TEXT("Summon fails"), Result.bOk);
	TestEqual(TEXT("Missing source code"), Result.Code, EWBSummonExecutionResultCode::SourceCardMissing);
	TestEqual(TEXT("Units unchanged"), State.Units.Num(), BeforeUnits);
	TestEqual(TEXT("Hand unchanged"), FindSummonZones(State, SummonPlayerId)->Hand.Num(), BeforeHand);
	TestEqual(TEXT("No trace on failure"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionValidationCasesTest, "Wandbound.Core.SummonExecution.ValidationCasesFailWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionValidationCasesTest::RunTest(const FString& Parameters)
{
	auto ExpectFailure = [this](
		const TCHAR* Label,
		FWBGameStateData State,
		const FWBCardDefinitionRepository& Repository,
		const FWBSummonExecutionRequest& Request,
		const EWBSummonExecutionResultCode ExpectedCode)
	{
		const int32 BeforeUnits = State.Units.Num();
		const int32 BeforeHand = FindSummonZones(State, SummonPlayerId) != nullptr
			? FindSummonZones(State, SummonPlayerId)->Hand.Num()
			: -1;
		const FWBSummonExecutionResult Result =
			WBSummonExecution::ExecuteCharacterSummonFromHand(State, Repository, Request);
		TestFalse(*FString::Printf(TEXT("%s fails"), Label), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s code"), Label), Result.Code, ExpectedCode);
		TestEqual(*FString::Printf(TEXT("%s units unchanged"), Label), State.Units.Num(), BeforeUnits);
		if (BeforeHand >= 0)
		{
			TestEqual(*FString::Printf(TEXT("%s hand unchanged"), Label), FindSummonZones(State, SummonPlayerId)->Hand.Num(), BeforeHand);
		}
		TestEqual(*FString::Printf(TEXT("%s no trace"), Label), Result.TraceEvents.Num(), 0);
	};

	FWBGameStateData MismatchState = MakeSummonState();
	AddSummonHandCard(MismatchState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("mismatch"),
		MismatchState,
		MakeSummonRepository({ MakeSummonCharacterDefinition() }),
		MakeSummonRequest(TEXT("hand_character"), TEXT("different_card")),
		EWBSummonExecutionResultCode::SourceCardIdMismatch);

	FWBGameStateData MissingDefinitionState = MakeSummonState();
	AddSummonHandCard(MissingDefinitionState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("missing definition"),
		MissingDefinitionState,
		MakeSummonRepository({}),
		MakeSummonRequest(),
		EWBSummonExecutionResultCode::CardDefinitionNotFound);

	FWBGameStateData NonCharacterState = MakeSummonState();
	AddSummonHandCard(NonCharacterState, SummonPlayerId, TEXT("hand_character"), TEXT("wand_not_character"));
	ExpectFailure(
		TEXT("non-character"),
		NonCharacterState,
		MakeSummonRepository({ MakeSummonWandDefinition() }),
		MakeSummonRequest(TEXT("hand_character"), TEXT("wand_not_character")),
		EWBSummonExecutionResultCode::SourceCardNotCharacter);

	FWBGameStateData InvalidStatsState = MakeSummonState();
	AddSummonHandCard(InvalidStatsState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("invalid stats"),
		InvalidStatsState,
		MakeSummonRepository({ MakeSummonCharacterDefinition(TEXT("summon_character"), TEXT("Bad Student"), MakeSummonCharacterStats(0)) }),
		MakeSummonRequest(),
		EWBSummonExecutionResultCode::InvalidCharacterStats);

	FWBGameStateData MissingHeroState = MakeSummonState();
	MissingHeroState.GetMutablePlayerById(SummonPlayerId)->HeroUnitId = -1;
	AddSummonHandCard(MissingHeroState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("missing hero"),
		MissingHeroState,
		MakeSummonRepository({ MakeSummonCharacterDefinition() }),
		MakeSummonRequest(),
		EWBSummonExecutionResultCode::HeroNotFound);

	FWBGameStateData UnitCapState = MakeSummonState();
	UnitCapState.AddUnitForTest(MakeSummonUnit(11, SummonPlayerId, TEXT("ally_1"), FWBTile(0, 0)));
	UnitCapState.AddUnitForTest(MakeSummonUnit(12, SummonPlayerId, TEXT("ally_2"), FWBTile(1, 0)));
	UnitCapState.AddUnitForTest(MakeSummonUnit(13, SummonPlayerId, TEXT("ally_3"), FWBTile(2, 0)));
	AddSummonHandCard(UnitCapState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("unit cap"),
		UnitCapState,
		MakeSummonRepository({ MakeSummonCharacterDefinition() }),
		MakeSummonRequest(),
		EWBSummonExecutionResultCode::UnitCapReached);

	FWBGameStateData NotAdjacentState = MakeSummonState();
	AddSummonHandCard(NotAdjacentState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("not adjacent"),
		NotAdjacentState,
		MakeSummonRepository({ MakeSummonCharacterDefinition() }),
		MakeSummonRequest(TEXT("hand_character"), TEXT("summon_character"), FWBTile(1, 1)),
		EWBSummonExecutionResultCode::TargetTileNotAdjacentToHero);

	FWBGameStateData OutOfBoundsState = MakeSummonState();
	AddSummonHandCard(OutOfBoundsState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("out of bounds"),
		OutOfBoundsState,
		MakeSummonRepository({ MakeSummonCharacterDefinition() }),
		MakeSummonRequest(TEXT("hand_character"), TEXT("summon_character"), FWBTile(-1, 4)),
		EWBSummonExecutionResultCode::TargetTileOutOfBounds);

	FWBGameStateData OccupiedState = MakeSummonState();
	OccupiedState.AddUnitForTest(MakeSummonUnit(11, SummonPlayerId, TEXT("ally_1"), FWBTile(4, 3)));
	AddSummonHandCard(OccupiedState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("occupied"),
		OccupiedState,
		MakeSummonRepository({ MakeSummonCharacterDefinition() }),
		MakeSummonRequest(),
		EWBSummonExecutionResultCode::TargetTileOccupied);

	FWBGameStateData MarkerState = MakeSummonState();
	AddSummonMarker(MarkerState, FWBTile(4, 3));
	AddSummonHandCard(MarkerState, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	ExpectFailure(
		TEXT("marker"),
		MarkerState,
		MakeSummonRepository({ MakeSummonCharacterDefinition() }),
		MakeSummonRequest(),
		EWBSummonExecutionResultCode::MarkerTriggerDeferred);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionTraceJsonTest, "Wandbound.Core.SummonExecution.TraceEventDeterministicAndSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionTraceJsonTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSummonState();
	AddSummonHandCard(State, SummonPlayerId, TEXT("hand_character"), TEXT("summon_character"));
	AddSummonHandCard(State, SummonOpponentId, TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_CARD_DO_NOT_LEAK"));
	AddSummonDeckCard(State, SummonPlayerId, TEXT("SECRET_DECK_DO_NOT_LEAK"), TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	AddSummonMarker(State, FWBTile(2, 2));

	const FWBSummonExecutionResult Result =
		ExecuteDefaultSummon(State, MakeSummonRepository({ MakeSummonCharacterDefinition() }));
	FString Json;
	TestTrue(TEXT("Serialize result"), WBSummonExecution::SummonExecutionResultToJsonStringForTest(Result, Json));

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestEqual(TEXT("One trace event"), Result.TraceEvents.Num(), 1);
	TestEqual(TEXT("Trace event type"), Result.TraceEvents[0].EventType, FString(TEXT("summon_unit")));
	TestEqual(TEXT("Trace created id"), Result.TraceEvents[0].CreatedUnitId, Result.CreatedUnitId);
	TestEqual(TEXT("Trace target tile"), Result.TraceEvents[0].TargetTile, FWBTile(4, 3));
	TestTrue(TEXT("Trace event serialized"), Json.Contains(TEXT("summon_unit")));
	TestTrue(TEXT("Created id serialized"), Json.Contains(FString::FromInt(Result.CreatedUnitId)));
	TestFalse(TEXT("Opponent hand hidden"), Json.Contains(TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK")));
	TestFalse(TEXT("Deck hidden"), Json.Contains(TEXT("SECRET_DECK_DO_NOT_LEAK")));
	TestFalse(TEXT("Marker hidden"), Json.Contains(TEXT("SECRET_MARKER_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSummonExecutionSourceGuardTest, "Wandbound.Core.SummonExecution.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonExecutionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Header;
	FString Source;
	TestTrue(TEXT("Header loads"), LoadSummonSourceFile(TEXT("Source/WandboundCore/Public/WBSummonExecution.h"), Header));
	TestTrue(TEXT("Source loads"), LoadSummonSourceFile(TEXT("Source/WandboundCore/Private/WBSummonExecution.cpp"), Source));
	const FString Combined = Header + Source;

	TestFalse(TEXT("No legal action generation"), Combined.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No FWBAction"), Combined.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No WBActionCodec"), Combined.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No WBEffectRunner"), Combined.Contains(TEXT("WBEffectRunner")));
	return true;
}

#endif
