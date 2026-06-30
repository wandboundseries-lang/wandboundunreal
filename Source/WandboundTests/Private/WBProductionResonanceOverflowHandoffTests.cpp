#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBGameStateData.h"
#include "WBProductionResonanceOverflowHandoff.h"
#include "WBProductionSummonEquipDataProvider.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 RuntimeOverflowPlayerId = 0;
constexpr int32 RuntimeOverflowOpponentId = 1;
constexpr int32 RuntimeOverflowUnitId = 10;
constexpr int32 RuntimeOverflowOpponentUnitId = 20;

FWBGameStateData MakeRuntimeOverflowState(const int32 RLTotal = 3, const int32 RLUsed = 0)
{
	FWBGameStateData State;
	FWBPlayerStateData Player;
	Player.PlayerId = RuntimeOverflowPlayerId;
	Player.HeroUnitId = RuntimeOverflowUnitId;
	State.Players.Add(Player);
	FWBPlayerStateData Opponent;
	Opponent.PlayerId = RuntimeOverflowOpponentId;
	Opponent.HeroUnitId = RuntimeOverflowOpponentUnitId;
	State.Players.Add(Opponent);

	FWBUnitState Unit;
	Unit.UnitId = RuntimeOverflowUnitId;
	Unit.OwnerId = RuntimeOverflowPlayerId;
	Unit.CardId = TEXT("hero_card");
	Unit.X = 4;
	Unit.Y = 4;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	State.Units.Add(Unit);

	FWBUnitState OpponentUnit = Unit;
	OpponentUnit.UnitId = RuntimeOverflowOpponentUnitId;
	OpponentUnit.OwnerId = RuntimeOverflowOpponentId;
	OpponentUnit.X = 8;
	OpponentUnit.Y = 8;
	OpponentUnit.RLUsed = 0;
	State.Units.Add(OpponentUnit);

	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = RuntimeOverflowPlayerId;
	State.CardZoneState.PlayerZones.Add(Zones);
	FWBPlayerCardZoneState OpponentZones;
	OpponentZones.PlayerId = RuntimeOverflowOpponentId;
	State.CardZoneState.PlayerZones.Add(OpponentZones);
	return State;
}

FWBCardDefinition MakeRuntimeOverflowWandDefinition(const FString& CardId, const int32 RR)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = RR;
	return Definition;
}

FWBCardDefinitionRepository MakeRuntimeOverflowRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("runtime_overflow_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

void AddRuntimeEquippedWand(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId,
	const FString& SlotId,
	const int32 EquipOrder)
{
	FWBEquippedCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = RuntimeOverflowPlayerId;
	Entry.EquippedToUnitId = RuntimeOverflowUnitId;
	Entry.SlotId = SlotId;
	Entry.EquipOrder = EquipOrder;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Entry);
}

void AddRuntimeHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), PlayerId);
	if (Zones == nullptr)
	{
		return;
	}

	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = PlayerId;
	Entry.Zone = EWBCardZone::Hand;
	Entry.ZoneIndex = ZoneIndex;
	Zones->Hand.Add(Entry);
}

void AddRuntimeDeckCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId)
{
	FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), PlayerId);
	if (Zones == nullptr)
	{
		return;
	}

	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = PlayerId;
	Entry.Zone = EWBCardZone::Deck;
	Entry.ZoneIndex = 0;
	Zones->Deck.Add(Entry);
}

FWBProductionSummonEquipDecisionData BuildRuntimeOverflowDecisionData(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ViewerPlayerId = RuntimeOverflowPlayerId)
{
	return FWBProductionSummonEquipDataProvider().BuildDecisionData(State, Repository, ViewerPlayerId);
}

FWBProductionResonanceOverflowRequest MakeRuntimeOverflowRequest(
	const int32 ExpectedRLUsed = INDEX_NONE)
{
	FWBProductionResonanceOverflowRequest Request;
	Request.ViewerPlayerId = RuntimeOverflowPlayerId;
	Request.UnitId = RuntimeOverflowUnitId;
	Request.ExpectedRLUsedBeforeResolution = ExpectedRLUsed;
	return Request;
}

FString BuildRuntimeOverflowScanText(const FWBProductionResonanceOverflowHandoffResult& Result)
{
	FString Text = FString::Printf(
		TEXT("ok=%d|resolved=%d|reason=%s|unit=%d|before=%d|after=%d;"),
		Result.bOk ? 1 : 0,
		Result.bResolvedOverflow ? 1 : 0,
		*Result.Reason,
		Result.UnitId,
		Result.RLUsedBefore,
		Result.RLUsedAfter);

	for (const FWBResonanceOverflowTraceEvent& Event : Result.TraceEvents)
	{
		Text += FString::Printf(
			TEXT("trace=%s|unit=%d|instance=%s|card=%s|slot=%s|rr=%d|before=%d|after=%d;"),
			*Event.EventType,
			Event.UnitId,
			*Event.SourceInstanceId,
			*Event.SourceCardId,
			*Event.SlotId,
			Event.RR,
			Event.RLUsedBefore,
			Event.RLUsedAfter);
	}

	for (const FWBProductionEquipOption& Option : Result.RefreshedSummonEquipData.EquipOptions)
	{
		Text += FString::Printf(
			TEXT("equip=%s|card=%s|name=%s|rr=%d;"),
			*Option.SourceInstanceId,
			*Option.SourceCardId,
			*Option.PublicName,
			Option.RR);
	}

	return Text;
}

bool LoadRuntimeOverflowSourceFile(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(OutSource, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionResonanceOverflowHandoffNoOverflowTest, "Wandbound.Runtime.ProductionResonanceOverflowHandoff.NoOverflowRefreshesProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionResonanceOverflowHandoffNoOverflowTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeOverflowState(3, 2);
	AddRuntimeHandCard(State, RuntimeOverflowPlayerId, TEXT("hand_wand"), TEXT("hand_wand"));
	const FWBCardDefinitionRepository Repository = MakeRuntimeOverflowRepository({ MakeRuntimeOverflowWandDefinition(TEXT("hand_wand"), 1) });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildRuntimeOverflowDecisionData(State, Repository);

	const FWBProductionResonanceOverflowHandoffResult Result =
		FWBProductionResonanceOverflowHandoff().ResolveOverflowAndRefresh(
			State,
			Repository,
			DecisionData,
			MakeRuntimeOverflowRequest(2));

	TestTrue(TEXT("Handoff succeeds"), Result.bOk);
	TestFalse(TEXT("No overflow"), Result.bResolvedOverflow);
	TestEqual(TEXT("No trace"), Result.TraceEvents.Num(), 0);
	TestEqual(TEXT("Provider refreshed"), Result.RefreshedSummonEquipData.ViewerPlayerId, RuntimeOverflowPlayerId);
	TestEqual(TEXT("Hand wand still offered"), Result.RefreshedSummonEquipData.EquipOptions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionResonanceOverflowHandoffResolveTest, "Wandbound.Runtime.ProductionResonanceOverflowHandoff.ResolvesOverflowAndRefreshesProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionResonanceOverflowHandoffResolveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeOverflowState(3, 5);
	AddRuntimeEquippedWand(State, TEXT("equipped_wand"), TEXT("equipped_wand"), TEXT("wand"), 0);
	AddRuntimeHandCard(State, RuntimeOverflowPlayerId, TEXT("hand_wand"), TEXT("hand_wand"));
	const FWBCardDefinitionRepository Repository = MakeRuntimeOverflowRepository({
		MakeRuntimeOverflowWandDefinition(TEXT("equipped_wand"), 3),
		MakeRuntimeOverflowWandDefinition(TEXT("hand_wand"), 1)
	});
	const FWBProductionSummonEquipDecisionData DecisionData = BuildRuntimeOverflowDecisionData(State, Repository);

	const FWBProductionResonanceOverflowHandoffResult Result =
		FWBProductionResonanceOverflowHandoff().ResolveOverflowAndRefresh(
			State,
			Repository,
			DecisionData,
			MakeRuntimeOverflowRequest(5));

	TestTrue(TEXT("Handoff succeeds"), Result.bOk);
	TestTrue(TEXT("Overflow resolved"), Result.bResolvedOverflow);
	TestEqual(TEXT("RL reduced"), State.GetUnitById(RuntimeOverflowUnitId)->RLUsed, 2);
	TestEqual(TEXT("Equipped removed"), State.GetCardZoneState().EquippedCards.Num(), 0);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 3);
	TestEqual(TEXT("Provider offers hand wand after RL recovers"), Result.RefreshedSummonEquipData.EquipOptions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionResonanceOverflowHandoffStaleTest, "Wandbound.Runtime.ProductionResonanceOverflowHandoff.StaleProviderDataRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionResonanceOverflowHandoffStaleTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeOverflowState(3, 5);
	AddRuntimeEquippedWand(State, TEXT("equipped_wand"), TEXT("equipped_wand"), TEXT("wand"), 0);
	const FWBCardDefinitionRepository Repository = MakeRuntimeOverflowRepository({ MakeRuntimeOverflowWandDefinition(TEXT("equipped_wand"), 3) });
	FWBProductionSummonEquipDecisionData WrongViewerData = BuildRuntimeOverflowDecisionData(State, Repository);
	WrongViewerData.ViewerPlayerId = RuntimeOverflowOpponentId;

	const FWBProductionResonanceOverflowHandoffResult WrongViewerResult =
		FWBProductionResonanceOverflowHandoff().ResolveOverflowAndRefresh(
			State,
			Repository,
			WrongViewerData,
			MakeRuntimeOverflowRequest(5));
	const FWBProductionResonanceOverflowHandoffResult WrongRLResult =
		FWBProductionResonanceOverflowHandoff().ResolveOverflowAndRefresh(
			State,
			Repository,
			BuildRuntimeOverflowDecisionData(State, Repository),
			MakeRuntimeOverflowRequest(4));

	TestFalse(TEXT("Wrong viewer rejected"), WrongViewerResult.bOk);
	TestEqual(TEXT("Wrong viewer reason"), WrongViewerResult.Reason, FString(TEXT("stale_provider_data")));
	TestFalse(TEXT("Wrong RL rejected"), WrongRLResult.bOk);
	TestEqual(TEXT("Wrong RL reason"), WrongRLResult.Reason, FString(TEXT("stale_provider_data")));
	TestEqual(TEXT("RL unchanged"), State.GetUnitById(RuntimeOverflowUnitId)->RLUsed, 5);
	TestEqual(TEXT("Equipped unchanged"), State.GetCardZoneState().EquippedCards.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionResonanceOverflowHandoffHiddenInfoTest, "Wandbound.Runtime.ProductionResonanceOverflowHandoff.HiddenInfoNotLeaked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionResonanceOverflowHandoffHiddenInfoTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeOverflowState(3, 4);
	AddRuntimeEquippedWand(State, TEXT("equipped_wand"), TEXT("equipped_wand"), TEXT("wand"), 0);
	AddRuntimeHandCard(State, RuntimeOverflowOpponentId, TEXT("SECRET_RUNTIME_OVERFLOW_OPPONENT_HAND"), TEXT("SECRET_RUNTIME_OVERFLOW_CARD"));
	AddRuntimeDeckCard(State, RuntimeOverflowPlayerId, TEXT("SECRET_RUNTIME_OVERFLOW_DECK"), TEXT("SECRET_RUNTIME_OVERFLOW_DECK_CARD"));
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 91;
	Marker.OwnerPlayerId = RuntimeOverflowOpponentId;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_RUNTIME_OVERFLOW_MARKER");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
	const FWBCardDefinitionRepository Repository = MakeRuntimeOverflowRepository({ MakeRuntimeOverflowWandDefinition(TEXT("equipped_wand"), 1) });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildRuntimeOverflowDecisionData(State, Repository);

	const FWBProductionResonanceOverflowHandoffResult Result =
		FWBProductionResonanceOverflowHandoff().ResolveOverflowAndRefresh(
			State,
			Repository,
			DecisionData,
			MakeRuntimeOverflowRequest(4));
	const FString Scan = BuildRuntimeOverflowScanText(Result);

	TestTrue(TEXT("Handoff succeeds"), Result.bOk);
	TestFalse(TEXT("Opponent hand hidden"), Scan.Contains(TEXT("SECRET_RUNTIME_OVERFLOW_OPPONENT_HAND")));
	TestFalse(TEXT("Deck hidden"), Scan.Contains(TEXT("SECRET_RUNTIME_OVERFLOW_DECK")));
	TestFalse(TEXT("Marker hidden"), Scan.Contains(TEXT("SECRET_RUNTIME_OVERFLOW_MARKER")));
	TestFalse(TEXT("Provider internals absent"), Scan.Contains(TEXT("DebugActivationId")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionResonanceOverflowHandoffSourceGuardTest, "Wandbound.Runtime.ProductionResonanceOverflowHandoff.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionResonanceOverflowHandoffSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Header;
	FString Source;
	TestTrue(TEXT("Header loads"), LoadRuntimeOverflowSourceFile(TEXT("Source/WandboundRuntime/Public/WBProductionResonanceOverflowHandoff.h"), Header));
	TestTrue(TEXT("Source loads"), LoadRuntimeOverflowSourceFile(TEXT("Source/WandboundRuntime/Private/WBProductionResonanceOverflowHandoff.cpp"), Source));
	const FString Combined = Header + Source;

	TestFalse(TEXT("No legal action generation"), Combined.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No FWBAction"), Combined.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No WBActionCodec"), Combined.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No WBEffectRunner"), Combined.Contains(TEXT("WBEffectRunner")));
	return true;
}

#endif
