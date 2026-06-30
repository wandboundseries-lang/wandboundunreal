#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBGameStateData.h"
#include "WBProductionSummonEquipDataProvider.h"
#include "WBProductionSummonExecutionHandoff.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 HandoffViewerId = 0;
constexpr int32 HandoffOpponentId = 1;
constexpr int32 HandoffHeroUnitId = 10;
constexpr int32 HandoffOpponentHeroUnitId = 20;

FWBPlayerStateData MakeHandoffPlayer(const int32 PlayerId, const int32 HeroUnitId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = HeroUnitId;
	return Player;
}

FWBUnitState MakeHandoffUnit(
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

FWBPlayerCardZoneState MakeHandoffZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	return Zones;
}

FWBGameStateData MakeHandoffState()
{
	FWBGameStateData State;
	State.CurrentPlayer = HandoffViewerId;
	State.PriorityPlayer = HandoffViewerId;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeHandoffPlayer(HandoffViewerId, HandoffHeroUnitId));
	State.Players.Add(MakeHandoffPlayer(HandoffOpponentId, HandoffOpponentHeroUnitId));
	State.AddUnitForTest(MakeHandoffUnit(HandoffHeroUnitId, HandoffViewerId, TEXT("hero_card"), FWBTile(4, 4)));
	State.AddUnitForTest(MakeHandoffUnit(HandoffOpponentHeroUnitId, HandoffOpponentId, TEXT("enemy_hero"), FWBTile(8, 8)));
	State.CardZoneState.PlayerZones.Add(MakeHandoffZones(HandoffViewerId));
	State.CardZoneState.PlayerZones.Add(MakeHandoffZones(HandoffOpponentId));
	return State;
}

FWBCardCharacterStatsDefinition MakeHandoffCharacterStats(
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

FWBCardDefinition MakeHandoffCharacterDefinition(
	const FString& CardId = TEXT("summon_character"),
	const FString& PublicName = TEXT("Summoned Student"),
	const FWBCardCharacterStatsDefinition Stats = MakeHandoffCharacterStats())
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats = Stats;
	return Definition;
}

FWBCardDefinition MakeHandoffWandDefinition(
	const FString& CardId = TEXT("summon_wand"),
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

FWBCardDefinitionRepository MakeHandoffRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("summon_execution_handoff_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBZoneCardEntry MakeHandoffZoneEntry(
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

FWBPlayerCardZoneState* FindHandoffZones(FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), PlayerId);
}

void AddHandoffHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindHandoffZones(State, PlayerId))
	{
		Zones->Hand.Add(MakeHandoffZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Hand, ZoneIndex));
	}
}

void AddHandoffDeckCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindHandoffZones(State, PlayerId))
	{
		Zones->Deck.Add(MakeHandoffZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Deck, ZoneIndex));
	}
}

void AddHandoffMarker(
	FWBGameStateData& State,
	const FString& InternalCardId)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 41;
	Marker.OwnerPlayerId = HandoffOpponentId;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = InternalCardId;
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

FWBProductionSummonEquipDecisionData BuildHandoffDecisionData(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	return FWBProductionSummonEquipDataProvider().BuildDecisionData(
		State,
		Repository,
		HandoffViewerId);
}

FWBProductionSummonExecutionRequest MakeHandoffRequest(
	const FWBProductionSummonOption& Option,
	const FWBTile& TargetTile)
{
	FWBProductionSummonExecutionRequest Request;
	Request.ViewerPlayerId = HandoffViewerId;
	Request.SourceInstanceId = Option.SourceInstanceId;
	Request.SourceCardId = Option.SourceCardId;
	Request.TargetTile = TargetTile;
	return Request;
}

const FWBProductionSummonOption* FindHandoffSummonOption(
	const FWBProductionSummonEquipDecisionData& Data,
	const FString& InstanceId)
{
	return Data.SummonOptions.FindByPredicate([&InstanceId](const FWBProductionSummonOption& Option)
	{
		return Option.SourceInstanceId == InstanceId;
	});
}

const FWBProductionEquipOption* FindHandoffEquipOption(
	const FWBProductionSummonEquipDecisionData& Data,
	const FString& InstanceId)
{
	return Data.EquipOptions.FindByPredicate([&InstanceId](const FWBProductionEquipOption& Option)
	{
		return Option.SourceInstanceId == InstanceId;
	});
}

bool HasHandoffDiagnostic(
	const FWBProductionSummonEquipDecisionData& Data,
	const FString& Code)
{
	return Data.Diagnostics.ContainsByPredicate([&Code](const FWBProductionSummonEquipProviderDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == Code;
	});
}

FString BuildHandoffScanText(const FWBProductionSummonExecutionHandoffResult& Result)
{
	FString Text = FString::Printf(
		TEXT("ok=%d|reason=%s|created=%d|instance=%s|card=%s|tile=%s;"),
		Result.bOk ? 1 : 0,
		*Result.Reason,
		Result.CreatedUnitId,
		*Result.SourceInstanceId,
		*Result.SourceCardId,
		*Result.TargetTile.ToString());

	for (const FWBSummonExecutionTraceEvent& Event : Result.TraceEvents)
	{
		Text += FString::Printf(
			TEXT("trace=%s|player=%d|instance=%s|card=%s|unit=%d|tile=%s;"),
			*Event.EventType,
			Event.PlayerId,
			*Event.SourceInstanceId,
			*Event.SourceCardId,
			Event.CreatedUnitId,
			*Event.TargetTile.ToString());
	}

	for (const FWBProductionSummonOption& Option : Result.RefreshedSummonEquipData.SummonOptions)
	{
		Text += FString::Printf(
			TEXT("summon=%s|card=%s|name=%s;"),
			*Option.SourceInstanceId,
			*Option.SourceCardId,
			*Option.PublicName);
	}

	for (const FWBProductionEquipOption& Option : Result.RefreshedSummonEquipData.EquipOptions)
	{
		Text += FString::Printf(
			TEXT("equip=%s|card=%s|name=%s|rr=%d;"),
			*Option.SourceInstanceId,
			*Option.SourceCardId,
			*Option.PublicName,
			Option.RR);
		for (const int32 UnitId : Option.EligibleUnitIds)
		{
			Text += FString::Printf(TEXT("eligible=%d;"), UnitId);
		}
	}

	for (const FWBProductionSummonEquipProviderDiagnostic& Diagnostic : Result.RefreshedSummonEquipData.Diagnostics)
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

bool LoadHandoffSourceFile(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(OutSource, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffSuccessTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.ProviderOptionSummonsAndRefreshes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character"), TEXT("summon_character"));
	const FWBCardDefinitionRepository Repository = MakeHandoffRepository({ MakeHandoffCharacterDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildHandoffDecisionData(State, Repository);
	const FWBProductionSummonOption* Option = FindHandoffSummonOption(DecisionData, TEXT("hand_character"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBProductionSummonExecutionHandoffResult Result =
		FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeHandoffRequest(*Option, Option->LegalTiles[0]));

	TestTrue(TEXT("Handoff succeeds"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("success")));
	TestNotNull(TEXT("Created unit"), State.GetUnitById(Result.CreatedUnitId));
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	TestEqual(TEXT("Refreshed data viewer"), Result.RefreshedSummonEquipData.ViewerPlayerId, HandoffViewerId);
	TestEqual(TEXT("Hand source removed from refreshed options"), Result.RefreshedSummonEquipData.SummonOptions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffRejectsProviderMismatchTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.RejectsTileOrSourceMissingFromProviderData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffRejectsProviderMismatchTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character"), TEXT("summon_character"));
	const FWBCardDefinitionRepository Repository = MakeHandoffRepository({ MakeHandoffCharacterDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildHandoffDecisionData(State, Repository);
	const FWBProductionSummonOption* Option = FindHandoffSummonOption(DecisionData, TEXT("hand_character"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const int32 BeforeUnits = State.Units.Num();
	const int32 BeforeHand = FindHandoffZones(State, HandoffViewerId)->Hand.Num();

	FWBProductionSummonExecutionRequest BadTileRequest = MakeHandoffRequest(*Option, FWBTile(0, 0));
	const FWBProductionSummonExecutionHandoffResult BadTileResult =
		FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
			State,
			Repository,
			DecisionData,
			BadTileRequest);

	FWBProductionSummonExecutionRequest MissingSourceRequest = BadTileRequest;
	MissingSourceRequest.SourceInstanceId = TEXT("missing_source");
	MissingSourceRequest.TargetTile = Option->LegalTiles[0];
	const FWBProductionSummonExecutionHandoffResult MissingSourceResult =
		FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
			State,
			Repository,
			DecisionData,
			MissingSourceRequest);

	TestFalse(TEXT("Bad tile rejected"), BadTileResult.bOk);
	TestEqual(TEXT("Bad tile reason"), BadTileResult.Reason, FString(TEXT("target_tile_not_allowed")));
	TestFalse(TEXT("Missing source rejected"), MissingSourceResult.bOk);
	TestEqual(TEXT("Missing source reason"), MissingSourceResult.Reason, FString(TEXT("summon_option_missing")));
	TestEqual(TEXT("Units unchanged"), State.Units.Num(), BeforeUnits);
	TestEqual(TEXT("Hand unchanged"), FindHandoffZones(State, HandoffViewerId)->Hand.Num(), BeforeHand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffStaleOptionTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.RevalidatesStaleProviderOption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffStaleOptionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character"), TEXT("summon_character"));
	const FWBCardDefinitionRepository Repository = MakeHandoffRepository({ MakeHandoffCharacterDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildHandoffDecisionData(State, Repository);
	const FWBProductionSummonOption* Option = FindHandoffSummonOption(DecisionData, TEXT("hand_character"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBTile PreviouslyLegalTile = Option->LegalTiles[0];
	State.AddUnitForTest(MakeHandoffUnit(30, HandoffViewerId, TEXT("new_blocker"), PreviouslyLegalTile));
	const int32 BeforeUnits = State.Units.Num();
	const int32 BeforeHand = FindHandoffZones(State, HandoffViewerId)->Hand.Num();

	const FWBProductionSummonExecutionHandoffResult Result =
		FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeHandoffRequest(*Option, PreviouslyLegalTile));

	TestFalse(TEXT("Stale option rejected by core"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("target_tile_occupied")));
	TestEqual(TEXT("No additional unit created"), State.Units.Num(), BeforeUnits);
	TestEqual(TEXT("Hand unchanged"), FindHandoffZones(State, HandoffViewerId)->Hand.Num(), BeforeHand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffRefreshUnitCapTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.RefreshReflectsUnitCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffRefreshUnitCapTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	State.AddUnitForTest(MakeHandoffUnit(11, HandoffViewerId, TEXT("ally_1"), FWBTile(0, 0)));
	State.AddUnitForTest(MakeHandoffUnit(12, HandoffViewerId, TEXT("ally_2"), FWBTile(1, 0)));
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character_a"), TEXT("summon_character"), 0);
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character_b"), TEXT("summon_character"), 1);
	const FWBCardDefinitionRepository Repository = MakeHandoffRepository({ MakeHandoffCharacterDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildHandoffDecisionData(State, Repository);
	const FWBProductionSummonOption* Option = FindHandoffSummonOption(DecisionData, TEXT("hand_character_a"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBProductionSummonExecutionHandoffResult Result =
		FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeHandoffRequest(*Option, Option->LegalTiles[0]));

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestEqual(TEXT("At cap after summon no further summon options"), Result.RefreshedSummonEquipData.SummonOptions.Num(), 0);
	TestTrue(TEXT("Unit cap diagnostic for remaining character"), HasHandoffDiagnostic(Result.RefreshedSummonEquipData, TEXT("unit_cap_reached")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffRefreshEquipEligibilityTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.NewUnitCanBecomeEquipEligible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffRefreshEquipEligibilityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character"), TEXT("summon_character"), 0);
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_wand"), TEXT("summon_wand"), 1);
	const FWBCardDefinitionRepository Repository = MakeHandoffRepository({
		MakeHandoffCharacterDefinition(TEXT("summon_character"), TEXT("Summoned Student"), MakeHandoffCharacterStats(4, 2, 1, 2)),
		MakeHandoffWandDefinition(TEXT("summon_wand"), TEXT("Copper Wand"), 1)
	});
	const FWBProductionSummonEquipDecisionData DecisionData = BuildHandoffDecisionData(State, Repository);
	const FWBProductionSummonOption* Option = FindHandoffSummonOption(DecisionData, TEXT("hand_character"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBProductionSummonExecutionHandoffResult Result =
		FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeHandoffRequest(*Option, Option->LegalTiles[0]));
	const FWBProductionEquipOption* EquipOption = FindHandoffEquipOption(Result.RefreshedSummonEquipData, TEXT("hand_wand"));

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestNotNull(TEXT("Wand remains offered"), EquipOption);
	TestTrue(TEXT("New unit eligible"), EquipOption != nullptr && EquipOption->EligibleUnitIds.Contains(Result.CreatedUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffHiddenInfoTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.HiddenInfoNotLeaked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffHiddenInfoTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character"), TEXT("summon_character"));
	AddHandoffHandCard(State, HandoffOpponentId, TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_CARD_DO_NOT_LEAK"));
	AddHandoffDeckCard(State, HandoffViewerId, TEXT("SECRET_DECK_DO_NOT_LEAK"), TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	AddHandoffMarker(State, TEXT("SECRET_MARKER_DO_NOT_LEAK"));
	const FWBCardDefinitionRepository Repository = MakeHandoffRepository({ MakeHandoffCharacterDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildHandoffDecisionData(State, Repository);
	const FWBProductionSummonOption* Option = FindHandoffSummonOption(DecisionData, TEXT("hand_character"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBProductionSummonExecutionHandoffResult Result =
		FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeHandoffRequest(*Option, Option->LegalTiles[0]));
	const FString Scan = BuildHandoffScanText(Result);

	TestTrue(TEXT("Summon succeeds"), Result.bOk);
	TestFalse(TEXT("Opponent hand hidden"), Scan.Contains(TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK")));
	TestFalse(TEXT("Deck hidden"), Scan.Contains(TEXT("SECRET_DECK_DO_NOT_LEAK")));
	TestFalse(TEXT("Marker hidden"), Scan.Contains(TEXT("SECRET_MARKER_DO_NOT_LEAK")));
	TestFalse(TEXT("Debug activation hidden"), Scan.Contains(TEXT("DebugActivationId")));
	TestFalse(TEXT("Usage key hidden"), Scan.Contains(TEXT("usage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffRepositoryMutationTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.DoesNotMutateRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffRepositoryMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddHandoffHandCard(State, HandoffViewerId, TEXT("hand_character"), TEXT("summon_character"));
	FWBCardDefinitionRepository Repository = MakeHandoffRepository({ MakeHandoffCharacterDefinition() });
	const TArray<FString> BeforeIds = WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository);
	const FWBProductionSummonEquipDecisionData DecisionData = BuildHandoffDecisionData(State, Repository);
	const FWBProductionSummonOption* Option = FindHandoffSummonOption(DecisionData, TEXT("hand_character"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	FWBProductionSummonExecutionHandoff().ExecuteSummonFromProviderOption(
		State,
		Repository,
		DecisionData,
		MakeHandoffRequest(*Option, Option->LegalTiles[0]));

	TestTrue(TEXT("Repository ids unchanged"), BeforeIds == WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository));
	TestEqual(TEXT("Stats unchanged"), Repository.Definitions[0].CharacterStats.HP, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionSummonExecutionHandoffSourceGuardTest, "Wandbound.Runtime.ProductionSummonExecutionHandoff.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionSummonExecutionHandoffSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Header;
	FString Source;
	TestTrue(TEXT("Header loads"), LoadHandoffSourceFile(TEXT("Source/WandboundRuntime/Public/WBProductionSummonExecutionHandoff.h"), Header));
	TestTrue(TEXT("Source loads"), LoadHandoffSourceFile(TEXT("Source/WandboundRuntime/Private/WBProductionSummonExecutionHandoff.cpp"), Source));
	const FString Combined = Header + Source;

	TestFalse(TEXT("No legal action generation"), Combined.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No FWBAction"), Combined.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No WBActionCodec"), Combined.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No WBEffectRunner"), Combined.Contains(TEXT("WBEffectRunner")));
	return true;
}

#endif
