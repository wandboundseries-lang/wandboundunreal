#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBGameStateData.h"
#include "WBProductionSummonEquipDataProvider.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 ViewerPlayerId = 0;
constexpr int32 OpponentPlayerId = 1;
constexpr int32 HeroUnitId = 10;
constexpr int32 OpponentUnitId = 20;

FWBPlayerStateData MakeProviderPlayer(const int32 PlayerId, const int32 HeroId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = HeroId;
	return Player;
}

FWBUnitState MakeProviderUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 RLTotal = 3,
	const int32 RLUsed = 0)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("provider_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	return Unit;
}

FWBGameStateData MakeProviderState()
{
	FWBGameStateData State;
	State.CurrentPlayer = ViewerPlayerId;
	State.PriorityPlayer = ViewerPlayerId;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeProviderPlayer(ViewerPlayerId, HeroUnitId));
	State.Players.Add(MakeProviderPlayer(OpponentPlayerId, OpponentUnitId));
	State.AddUnitForTest(MakeProviderUnit(HeroUnitId, ViewerPlayerId, FWBTile(4, 4)));
	State.AddUnitForTest(MakeProviderUnit(OpponentUnitId, OpponentPlayerId, FWBTile(8, 8)));
	return State;
}

FWBCardCharacterStatsDefinition MakeCharacterStats(
	const int32 HP = 3,
	const int32 ATK = 1,
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

FWBCardDefinition MakeCharacterDefinition(
	const FString& CardId = TEXT("character_alpha"),
	const FString& PublicName = TEXT("Brave Student"),
	const FWBCardCharacterStatsDefinition Stats = MakeCharacterStats())
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats = Stats;
	return Definition;
}

FWBCardDefinition MakeWandDefinition(
	const FString& CardId = TEXT("wand_alpha"),
	const FString& PublicName = TEXT("Copper Wand"),
	const int32 RR = 2)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = RR;
	return Definition;
}

FWBCardDefinition MakeUnsupportedDefinition(
	const FString& CardId = TEXT("fixture_alpha"),
	const FString& PublicName = TEXT("Fixture Alpha"))
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	return Definition;
}

FWBCardDefinitionRepository MakeProviderRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("summon_equip_provider_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBPlayerCardZoneState& EnsureProviderZones(FWBGameStateData& State, const int32 PlayerId)
{
	FWBCardZoneState& ZoneState = State.GetMutableCardZoneStateForTest();
	if (FWBPlayerCardZoneState* Existing = WBCardZoneState::FindMutablePlayerZones(ZoneState, PlayerId))
	{
		return *Existing;
	}

	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	ZoneState.PlayerZones.Add(Zones);
	return ZoneState.PlayerZones.Last();
}

FWBZoneCardEntry MakeZoneEntry(
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

void AddHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	EnsureProviderZones(State, PlayerId).Hand.Add(
		MakeZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Hand, ZoneIndex));
}

void AddDeckCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	EnsureProviderZones(State, PlayerId).Deck.Add(
		MakeZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Deck, ZoneIndex));
}

void AddHiddenMarker(
	FWBGameStateData& State,
	const FString& InternalCardId)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 100;
	Marker.OwnerPlayerId = OpponentPlayerId;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = InternalCardId;
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

FWBProductionSummonEquipDecisionData RunProvider(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 PlayerId = ViewerPlayerId)
{
	return FWBProductionSummonEquipDataProvider().BuildDecisionData(State, Repository, PlayerId);
}

const FWBProductionSummonOption* FindSummonOption(
	const FWBProductionSummonEquipDecisionData& Data,
	const FString& InstanceId)
{
	return Data.SummonOptions.FindByPredicate([&InstanceId](const FWBProductionSummonOption& Option)
	{
		return Option.SourceInstanceId == InstanceId;
	});
}

const FWBProductionEquipOption* FindEquipOption(
	const FWBProductionSummonEquipDecisionData& Data,
	const FString& InstanceId)
{
	return Data.EquipOptions.FindByPredicate([&InstanceId](const FWBProductionEquipOption& Option)
	{
		return Option.SourceInstanceId == InstanceId;
	});
}

bool HasProviderDiagnostic(
	const FWBProductionSummonEquipDecisionData& Data,
	const FString& Code)
{
	return Data.Diagnostics.ContainsByPredicate([&Code](const FWBProductionSummonEquipProviderDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == Code;
	});
}

FString BuildProviderScanText(const FWBProductionSummonEquipDecisionData& Data)
{
	FString Text = FString::Printf(TEXT("viewer=%d;"), Data.ViewerPlayerId);
	for (const FWBProductionSummonOption& Option : Data.SummonOptions)
	{
		Text += FString::Printf(
			TEXT("summon|instance=%s|card=%s|name=%s|owner=%d|hp=%d|atk=%d|ar=%d|rl=%d;"),
			*Option.SourceInstanceId,
			*Option.SourceCardId,
			*Option.PublicName,
			Option.OwnerPlayerId,
			Option.CharacterStats.HP,
			Option.CharacterStats.ATK,
			Option.CharacterStats.AR,
			Option.CharacterStats.RL);
		for (const FWBTile& Tile : Option.LegalTiles)
		{
			Text += FString::Printf(TEXT("tile=%s;"), *Tile.ToString());
		}
	}
	for (const FWBProductionEquipOption& Option : Data.EquipOptions)
	{
		Text += FString::Printf(
			TEXT("equip|instance=%s|card=%s|name=%s|owner=%d|rr=%d;"),
			*Option.SourceInstanceId,
			*Option.SourceCardId,
			*Option.PublicName,
			Option.OwnerPlayerId,
			Option.RR);
		for (const int32 UnitId : Option.EligibleUnitIds)
		{
			Text += FString::Printf(TEXT("eligible=%d;"), UnitId);
		}
	}
	for (const FWBProductionSummonEquipProviderDiagnostic& Diagnostic : Data.Diagnostics)
	{
		Text += FString::Printf(
			TEXT("diag=%s|card=%s|instance=%s|unit=%d;"),
			*Diagnostic.Code,
			*Diagnostic.CardId,
			*Diagnostic.InstanceId,
			Diagnostic.UnitId);
	}
	return Text;
}

FString LoadProviderProjectSourceText(const FString& RelativePath)
{
	FString Text;
	FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
	return Text;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipOwnHandCharacterTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.OwnHandCharacterProducesSummonOption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipOwnHandCharacterTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const FWBCardDefinitionRepository Repository = MakeProviderRepository({ MakeCharacterDefinition() });
	const FWBProductionSummonEquipDecisionData Data = RunProvider(State, Repository);

	TestEqual(TEXT("Summon option count"), Data.SummonOptions.Num(), 1);
	if (!TestNotNull(TEXT("Summon option"), FindSummonOption(Data, TEXT("own_character_1"))))
	{
		return false;
	}
	const FWBProductionSummonOption* Option = FindSummonOption(Data, TEXT("own_character_1"));
	TestEqual(TEXT("Source card"), Option->SourceCardId, FString(TEXT("character_alpha")));
	TestEqual(TEXT("Public name"), Option->PublicName, FString(TEXT("Brave Student")));
	TestEqual(TEXT("Owner"), Option->OwnerPlayerId, ViewerPlayerId);
	TestEqual(TEXT("Character HP"), Option->CharacterStats.HP, 3);
	TestEqual(TEXT("Character RL"), Option->CharacterStats.RL, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipSummonTilesAdjacentTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.SummonTilesOrthogonallyAdjacentToHero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipSummonTilesAdjacentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));
	const FWBProductionSummonOption* Option = FindSummonOption(Data, TEXT("own_character_1"));

	if (!TestNotNull(TEXT("Summon option"), Option))
	{
		return false;
	}

	const TArray<FWBTile> ExpectedTiles =
	{
		FWBTile(4, 3),
		FWBTile(3, 4),
		FWBTile(5, 4),
		FWBTile(4, 5)
	};
	TestTrue(TEXT("Legal tiles sorted Y then X"), Option->LegalTiles == ExpectedTiles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipOccupiedTilesExcludedTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.OccupiedAdjacentTilesExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipOccupiedTilesExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	State.AddUnitForTest(MakeProviderUnit(11, ViewerPlayerId, FWBTile(4, 3)));
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));
	const FWBProductionSummonOption* Option = FindSummonOption(Data, TEXT("own_character_1"));

	if (!TestNotNull(TEXT("Summon option"), Option))
	{
		return false;
	}

	TestFalse(TEXT("Occupied tile excluded"), Option->LegalTiles.Contains(FWBTile(4, 3)));
	TestEqual(TEXT("Remaining legal tile count"), Option->LegalTiles.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipOutOfBoundsTilesExcludedTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.OutOfBoundsAdjacentTilesExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipOutOfBoundsTilesExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	if (FWBUnitState* Hero = State.GetMutableUnitById(HeroUnitId))
	{
		Hero->X = 0;
		Hero->Y = 0;
	}
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));
	const FWBProductionSummonOption* Option = FindSummonOption(Data, TEXT("own_character_1"));

	if (!TestNotNull(TEXT("Summon option"), Option))
	{
		return false;
	}

	const TArray<FWBTile> ExpectedTiles = { FWBTile(1, 0), FWBTile(0, 1) };
	TestTrue(TEXT("Only in-bounds tiles remain"), Option->LegalTiles == ExpectedTiles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipUnitCapTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.UnitCapSuppressesSummonOption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipUnitCapTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	State.AddUnitForTest(MakeProviderUnit(11, ViewerPlayerId, FWBTile(0, 0)));
	State.AddUnitForTest(MakeProviderUnit(12, ViewerPlayerId, FWBTile(1, 0)));
	State.AddUnitForTest(MakeProviderUnit(13, ViewerPlayerId, FWBTile(2, 0)));
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));

	TestEqual(TEXT("No summon option"), Data.SummonOptions.Num(), 0);
	TestTrue(TEXT("Unit cap diagnostic"), HasProviderDiagnostic(Data, TEXT("unit_cap_reached")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipMissingHeroTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.MissingHeroSuppressesSummonOption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipMissingHeroTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	if (FWBPlayerStateData* Player = State.GetMutablePlayerById(ViewerPlayerId))
	{
		Player->HeroUnitId = -1;
	}
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));

	TestEqual(TEXT("No summon option"), Data.SummonOptions.Num(), 0);
	TestTrue(TEXT("Missing hero diagnostic"), HasProviderDiagnostic(Data, TEXT("hero_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipSummonHiddenZonesTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.OpponentAndDeckCharactersHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipSummonHiddenZonesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, OpponentPlayerId, TEXT("SECRET_OPPONENT_CHARACTER_INSTANCE"), TEXT("SECRET_OPPONENT_CHARACTER_CARD"));
	AddDeckCard(State, ViewerPlayerId, TEXT("SECRET_DECK_CHARACTER_INSTANCE"), TEXT("SECRET_DECK_CHARACTER_CARD"));
	const FWBCardDefinitionRepository Repository = MakeProviderRepository({
		MakeCharacterDefinition(TEXT("SECRET_OPPONENT_CHARACTER_CARD"), TEXT("Secret Opponent Character")),
		MakeCharacterDefinition(TEXT("SECRET_DECK_CHARACTER_CARD"), TEXT("Secret Deck Character"))
	});
	const FWBProductionSummonEquipDecisionData Data = RunProvider(State, Repository);
	const FString ScanText = BuildProviderScanText(Data);

	TestEqual(TEXT("No hidden summon options"), Data.SummonOptions.Num(), 0);
	TestFalse(TEXT("Opponent hand instance hidden"), ScanText.Contains(TEXT("SECRET_OPPONENT_CHARACTER_INSTANCE")));
	TestFalse(TEXT("Opponent hand card hidden"), ScanText.Contains(TEXT("SECRET_OPPONENT_CHARACTER_CARD")));
	TestFalse(TEXT("Deck instance hidden"), ScanText.Contains(TEXT("SECRET_DECK_CHARACTER_INSTANCE")));
	TestFalse(TEXT("Deck card hidden"), ScanText.Contains(TEXT("SECRET_DECK_CHARACTER_CARD")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipHiddenMarkerSafeTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.HiddenMarkerIdentityNotLeaked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipHiddenMarkerSafeTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	AddHiddenMarker(State, TEXT("SECRET_MARKER_CARD"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));

	TestFalse(TEXT("Hidden marker identity absent"), BuildProviderScanText(Data).Contains(TEXT("SECRET_MARKER_CARD")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipSummonOrderingTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.SummonOptionsDeterministicOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipSummonOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("z_instance"), TEXT("character_zeta"), 1);
	AddHandCard(State, ViewerPlayerId, TEXT("a_instance"), TEXT("character_alpha"), 0);
	const FWBProductionSummonEquipDecisionData Data = RunProvider(
		State,
		MakeProviderRepository({
			MakeCharacterDefinition(TEXT("character_zeta"), TEXT("Zeta Student")),
			MakeCharacterDefinition(TEXT("character_alpha"), TEXT("Alpha Student"))
		}));

	TestEqual(TEXT("Option count"), Data.SummonOptions.Num(), 2);
	TestEqual(TEXT("First follows hand order"), Data.SummonOptions[0].SourceInstanceId, FString(TEXT("a_instance")));
	TestEqual(TEXT("Second follows hand order"), Data.SummonOptions[1].SourceInstanceId, FString(TEXT("z_instance")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipNoStateMutationTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.DoesNotMutateGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipNoStateMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const int32 BeforeUnitCount = State.Units.Num();
	const int32 BeforeHandCount = EnsureProviderZones(State, ViewerPlayerId).Hand.Num();
	const int32 BeforeRLUsed = State.GetUnitById(HeroUnitId) != nullptr ? State.GetUnitById(HeroUnitId)->RLUsed : INDEX_NONE;

	RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));

	TestEqual(TEXT("Unit count unchanged"), State.Units.Num(), BeforeUnitCount);
	TestEqual(TEXT("Hand count unchanged"), EnsureProviderZones(State, ViewerPlayerId).Hand.Num(), BeforeHandCount);
	TestEqual(TEXT("RL used unchanged"), State.GetUnitById(HeroUnitId) != nullptr ? State.GetUnitById(HeroUnitId)->RLUsed : INDEX_NONE, BeforeRLUsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipNoRepositoryMutationTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.DoesNotMutateRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipNoRepositoryMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	FWBCardDefinitionRepository Repository = MakeProviderRepository({ MakeCharacterDefinition() });
	const TArray<FString> BeforeIds = WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository);
	const FWBCardCharacterStatsDefinition BeforeStats = Repository.Definitions[0].CharacterStats;

	RunProvider(State, Repository);

	TestTrue(TEXT("Card ids unchanged"), BeforeIds == WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository));
	TestEqual(TEXT("HP unchanged"), Repository.Definitions[0].CharacterStats.HP, BeforeStats.HP);
	TestEqual(TEXT("RL unchanged"), Repository.Definitions[0].CharacterStats.RL, BeforeStats.RL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipOwnHandWandTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.OwnHandWandProducesEquipOption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipOwnHandWandTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_wand_1"), TEXT("wand_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeWandDefinition() }));
	const FWBProductionEquipOption* Option = FindEquipOption(Data, TEXT("own_wand_1"));

	if (!TestNotNull(TEXT("Equip option"), Option))
	{
		return false;
	}

	TestEqual(TEXT("RR"), Option->RR, 2);
	TestTrue(TEXT("Hero eligible"), Option->EligibleUnitIds.Contains(HeroUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipWandTooExpensiveTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.WandNotOfferedWhenRRExceedsAvailable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipWandTooExpensiveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_wand_1"), TEXT("wand_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeWandDefinition(TEXT("wand_alpha"), TEXT("Copper Wand"), 4) }));

	TestEqual(TEXT("No equip option"), Data.EquipOptions.Num(), 0);
	TestTrue(TEXT("No eligible diagnostic"), HasProviderDiagnostic(Data, TEXT("no_eligible_equip_unit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipEligibleUnitsOwnedOnlyTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.EligibleUnitsViewerOwnedOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipEligibleUnitsOwnedOnlyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	State.AddUnitForTest(MakeProviderUnit(11, ViewerPlayerId, FWBTile(1, 1)));
	AddHandCard(State, ViewerPlayerId, TEXT("own_wand_1"), TEXT("wand_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeWandDefinition() }));
	const FWBProductionEquipOption* Option = FindEquipOption(Data, TEXT("own_wand_1"));

	if (!TestNotNull(TEXT("Equip option"), Option))
	{
		return false;
	}

	TestTrue(TEXT("Owned unit eligible"), Option->EligibleUnitIds.Contains(11));
	TestTrue(TEXT("Hero eligible"), Option->EligibleUnitIds.Contains(HeroUnitId));
	TestFalse(TEXT("Enemy not eligible"), Option->EligibleUnitIds.Contains(OpponentUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipEnemyUnitsNotEligibleTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.EnemyUnitsNotEligibleEquipTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipEnemyUnitsNotEligibleTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	if (FWBUnitState* Hero = State.GetMutableUnitById(HeroUnitId))
	{
		Hero->RLUsed = Hero->RLTotal;
	}
	AddHandCard(State, ViewerPlayerId, TEXT("own_wand_1"), TEXT("wand_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeWandDefinition() }));

	TestEqual(TEXT("No equip option when only enemy can afford"), Data.EquipOptions.Num(), 0);
	TestTrue(TEXT("No eligible diagnostic"), HasProviderDiagnostic(Data, TEXT("no_eligible_equip_unit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipEligibleUnitOrderingTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.EligibleUnitsDeterministicOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipEligibleUnitOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	State.AddUnitForTest(MakeProviderUnit(12, ViewerPlayerId, FWBTile(3, 1)));
	State.AddUnitForTest(MakeProviderUnit(11, ViewerPlayerId, FWBTile(1, 1)));
	AddHandCard(State, ViewerPlayerId, TEXT("own_wand_1"), TEXT("wand_alpha"));
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeWandDefinition() }));
	const FWBProductionEquipOption* Option = FindEquipOption(Data, TEXT("own_wand_1"));

	if (!TestNotNull(TEXT("Equip option"), Option))
	{
		return false;
	}

	const TArray<int32> ExpectedUnitIds = { 11, 12, HeroUnitId };
	TestTrue(TEXT("Eligible units sorted by tile then id"), Option->EligibleUnitIds == ExpectedUnitIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipWandHiddenZonesTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.OpponentAndDeckWandsHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipWandHiddenZonesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, OpponentPlayerId, TEXT("SECRET_OPPONENT_WAND_INSTANCE"), TEXT("SECRET_OPPONENT_WAND_CARD"));
	AddDeckCard(State, ViewerPlayerId, TEXT("SECRET_DECK_WAND_INSTANCE"), TEXT("SECRET_DECK_WAND_CARD"));
	AddHiddenMarker(State, TEXT("SECRET_MARKER_WAND_CARD"));
	const FWBCardDefinitionRepository Repository = MakeProviderRepository({
		MakeWandDefinition(TEXT("SECRET_OPPONENT_WAND_CARD"), TEXT("Secret Opponent Wand")),
		MakeWandDefinition(TEXT("SECRET_DECK_WAND_CARD"), TEXT("Secret Deck Wand"))
	});
	const FWBProductionSummonEquipDecisionData Data = RunProvider(State, Repository);
	const FString ScanText = BuildProviderScanText(Data);

	TestEqual(TEXT("No hidden equip options"), Data.EquipOptions.Num(), 0);
	TestFalse(TEXT("Opponent hand instance hidden"), ScanText.Contains(TEXT("SECRET_OPPONENT_WAND_INSTANCE")));
	TestFalse(TEXT("Opponent hand card hidden"), ScanText.Contains(TEXT("SECRET_OPPONENT_WAND_CARD")));
	TestFalse(TEXT("Deck instance hidden"), ScanText.Contains(TEXT("SECRET_DECK_WAND_INSTANCE")));
	TestFalse(TEXT("Deck card hidden"), ScanText.Contains(TEXT("SECRET_DECK_WAND_CARD")));
	TestFalse(TEXT("Marker identity hidden"), ScanText.Contains(TEXT("SECRET_MARKER_WAND_CARD")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipEquipNoMutationTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.EquipOptionDoesNotMutateRLOrHand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipEquipNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_wand_1"), TEXT("wand_alpha"));
	const int32 BeforeHandCount = EnsureProviderZones(State, ViewerPlayerId).Hand.Num();
	const int32 BeforeRLUsed = State.GetUnitById(HeroUnitId) != nullptr ? State.GetUnitById(HeroUnitId)->RLUsed : INDEX_NONE;

	RunProvider(State, MakeProviderRepository({ MakeWandDefinition() }));

	TestEqual(TEXT("Hand card remains"), EnsureProviderZones(State, ViewerPlayerId).Hand.Num(), BeforeHandCount);
	TestEqual(TEXT("RL used unchanged"), State.GetUnitById(HeroUnitId) != nullptr ? State.GetUnitById(HeroUnitId)->RLUsed : INDEX_NONE, BeforeRLUsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipDiagnosticsTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.DiagnosticsStable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipDiagnosticsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("missing_card_instance"), TEXT("missing_card"));
	AddHandCard(State, ViewerPlayerId, TEXT("fixture_instance"), TEXT("fixture_alpha"), 1);
	const FWBProductionSummonEquipDecisionData InvalidViewerData =
		RunProvider(State, MakeProviderRepository({ MakeUnsupportedDefinition() }), 42);
	const FWBProductionSummonEquipDecisionData Data =
		RunProvider(State, MakeProviderRepository({ MakeUnsupportedDefinition() }));

	TestTrue(TEXT("Invalid viewer diagnostic"), HasProviderDiagnostic(InvalidViewerData, TEXT("invalid_viewer_player")));
	TestTrue(TEXT("Missing definition diagnostic"), HasProviderDiagnostic(Data, TEXT("card_definition_not_found")));
	TestTrue(TEXT("Unsupported kind diagnostic"), HasProviderDiagnostic(Data, TEXT("unsupported_card_kind")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonEquipBoundaryTest, "Wandbound.Runtime.ProductionSummonEquipDataProvider.BoundariesUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonEquipBoundaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	AddHandCard(State, ViewerPlayerId, TEXT("own_character_1"), TEXT("character_alpha"));
	const TArray<FString> BeforeIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, ViewerPlayerId));
	RunProvider(State, MakeProviderRepository({ MakeCharacterDefinition() }));
	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, ViewerPlayerId));

	const FString ProviderHeader = LoadProviderProjectSourceText(TEXT("Source/WandboundRuntime/Public/WBProductionSummonEquipDataProvider.h"));
	const FString ProviderSource = LoadProviderProjectSourceText(TEXT("Source/WandboundRuntime/Private/WBProductionSummonEquipDataProvider.cpp"));
	const FString ProviderCombined = ProviderHeader + ProviderSource;
	const FString CodecSource = LoadProviderProjectSourceText(TEXT("Source/WandboundCore/Private/WBActionCodec.cpp"));

	TestTrue(TEXT("Legal action ids unchanged"), BeforeIds == AfterIds);
	TestFalse(TEXT("Provider does not call legal action generation"), ProviderCombined.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Provider does not include WBRules"), ProviderCombined.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Provider does not create FWBAction"), ProviderCombined.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("Provider does not call WBEffectRunner"), ProviderCombined.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Provider does not use card lifecycle"), ProviderCombined.Contains(TEXT("WBCardLifecycle")));
	TestFalse(TEXT("Provider does not mutate state through mutable helpers"), ProviderCombined.Contains(TEXT("GetMutable")));
	TestFalse(TEXT("Provider does not execute summon"), ProviderCombined.Contains(TEXT("ApplySummon")));
	TestFalse(TEXT("Provider does not execute equip"), ProviderCombined.Contains(TEXT("ApplyEquip")));
	TestFalse(TEXT("Action codec has no summon/equip provider dependency"), CodecSource.Contains(TEXT("WBProductionSummonEquip")));
	return true;
}

#endif
