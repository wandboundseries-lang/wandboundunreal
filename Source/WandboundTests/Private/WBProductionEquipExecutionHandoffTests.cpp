#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBGameStateData.h"
#include "WBProductionEquipExecutionHandoff.h"
#include "WBProductionSummonEquipDataProvider.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 EquipHandoffViewerId = 0;
constexpr int32 EquipHandoffOpponentId = 1;
constexpr int32 EquipHandoffHeroUnitId = 10;
constexpr int32 EquipHandoffOpponentHeroUnitId = 20;

FWBPlayerStateData MakeEquipHandoffPlayer(const int32 PlayerId, const int32 HeroUnitId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = HeroUnitId;
	return Player;
}

FWBUnitState MakeEquipHandoffUnit(
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

FWBPlayerCardZoneState MakeEquipHandoffZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	return Zones;
}

FWBGameStateData MakeEquipHandoffState()
{
	FWBGameStateData State;
	State.CurrentPlayer = EquipHandoffViewerId;
	State.PriorityPlayer = EquipHandoffViewerId;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeEquipHandoffPlayer(EquipHandoffViewerId, EquipHandoffHeroUnitId));
	State.Players.Add(MakeEquipHandoffPlayer(EquipHandoffOpponentId, EquipHandoffOpponentHeroUnitId));
	State.AddUnitForTest(MakeEquipHandoffUnit(EquipHandoffHeroUnitId, EquipHandoffViewerId, TEXT("hero_card"), FWBTile(4, 4), 3));
	State.AddUnitForTest(MakeEquipHandoffUnit(EquipHandoffOpponentHeroUnitId, EquipHandoffOpponentId, TEXT("enemy_hero"), FWBTile(8, 8), 3));
	State.CardZoneState.PlayerZones.Add(MakeEquipHandoffZones(EquipHandoffViewerId));
	State.CardZoneState.PlayerZones.Add(MakeEquipHandoffZones(EquipHandoffOpponentId));
	return State;
}

FWBCardDefinition MakeEquipHandoffWandDefinition(
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

FWBCardDefinitionRepository MakeEquipHandoffRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("equip_execution_handoff_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBZoneCardEntry MakeEquipHandoffZoneEntry(
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

FWBPlayerCardZoneState* FindEquipHandoffZones(FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), PlayerId);
}

void AddEquipHandoffHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindEquipHandoffZones(State, PlayerId))
	{
		Zones->Hand.Add(MakeEquipHandoffZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Hand, ZoneIndex));
	}
}

void AddEquipHandoffDeckCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones = FindEquipHandoffZones(State, PlayerId))
	{
		Zones->Deck.Add(MakeEquipHandoffZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Deck, ZoneIndex));
	}
}

void AddEquipHandoffMarker(
	FWBGameStateData& State,
	const FString& InternalCardId)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 51;
	Marker.OwnerPlayerId = EquipHandoffOpponentId;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = InternalCardId;
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

FWBProductionSummonEquipDecisionData BuildEquipHandoffDecisionData(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	return FWBProductionSummonEquipDataProvider().BuildDecisionData(
		State,
		Repository,
		EquipHandoffViewerId);
}

const FWBProductionEquipOption* FindEquipHandoffOption(
	const FWBProductionSummonEquipDecisionData& Data,
	const FString& InstanceId)
{
	return Data.EquipOptions.FindByPredicate([&InstanceId](const FWBProductionEquipOption& Option)
	{
		return Option.SourceInstanceId == InstanceId;
	});
}

FWBProductionEquipExecutionRequest MakeEquipHandoffRequest(
	const FWBProductionEquipOption& Option,
	const int32 TargetUnitId)
{
	FWBProductionEquipExecutionRequest Request;
	Request.ViewerPlayerId = EquipHandoffViewerId;
	Request.SourceInstanceId = Option.SourceInstanceId;
	Request.SourceCardId = Option.SourceCardId;
	Request.TargetUnitId = TargetUnitId;
	return Request;
}

FString BuildEquipHandoffScanText(const FWBProductionEquipExecutionHandoffResult& Result)
{
	FString Text = FString::Printf(
		TEXT("ok=%d|reason=%s|equipped=%d|instance=%s|card=%s|slot=%s|rr=%d|before=%d|after=%d;"),
		Result.bOk ? 1 : 0,
		*Result.Reason,
		Result.EquippedToUnitId,
		*Result.SourceInstanceId,
		*Result.SourceCardId,
		*Result.SlotId,
		Result.RR,
		Result.RLUsedBefore,
		Result.RLUsedAfter);

	for (const FWBEquipExecutionTraceEvent& Event : Result.TraceEvents)
	{
		Text += FString::Printf(
			TEXT("trace=%s|player=%d|instance=%s|card=%s|unit=%d|slot=%s|rr=%d|before=%d|after=%d;"),
			*Event.EventType,
			Event.PlayerId,
			*Event.SourceInstanceId,
			*Event.SourceCardId,
			Event.EquippedToUnitId,
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

bool LoadEquipHandoffSourceFile(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(OutSource, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionEquipExecutionHandoffSuccessTest, "Wandbound.Runtime.ProductionEquipExecutionHandoff.ProviderOptionEquipsAndRefreshes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionEquipExecutionHandoffSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipHandoffState();
	AddEquipHandoffHandCard(State, EquipHandoffViewerId, TEXT("hand_wand"), TEXT("equip_wand"));
	const FWBCardDefinitionRepository Repository = MakeEquipHandoffRepository({ MakeEquipHandoffWandDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildEquipHandoffDecisionData(State, Repository);
	const FWBProductionEquipOption* Option = FindEquipHandoffOption(DecisionData, TEXT("hand_wand"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBProductionEquipExecutionHandoffResult Result =
		FWBProductionEquipExecutionHandoff().ExecuteEquipFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeEquipHandoffRequest(*Option, EquipHandoffHeroUnitId));

	TestTrue(TEXT("Handoff succeeds"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("success")));
	TestEqual(TEXT("Equipped target"), Result.EquippedToUnitId, EquipHandoffHeroUnitId);
	TestEqual(TEXT("RL used"), State.GetUnitById(EquipHandoffHeroUnitId)->RLUsed, 1);
	TestEqual(TEXT("Equipped cards"), State.GetCardZoneState().EquippedCards.Num(), 1);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	TestEqual(TEXT("Refreshed data viewer"), Result.RefreshedSummonEquipData.ViewerPlayerId, EquipHandoffViewerId);
	TestEqual(TEXT("Hand source removed from refreshed options"), Result.RefreshedSummonEquipData.EquipOptions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionEquipExecutionHandoffRejectsProviderMismatchTest, "Wandbound.Runtime.ProductionEquipExecutionHandoff.RejectsTargetOrSourceMissingFromProviderData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionEquipExecutionHandoffRejectsProviderMismatchTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipHandoffState();
	AddEquipHandoffHandCard(State, EquipHandoffViewerId, TEXT("hand_wand"), TEXT("equip_wand"));
	const FWBCardDefinitionRepository Repository = MakeEquipHandoffRepository({ MakeEquipHandoffWandDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildEquipHandoffDecisionData(State, Repository);
	const FWBProductionEquipOption* Option = FindEquipHandoffOption(DecisionData, TEXT("hand_wand"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const int32 BeforeHand = FindEquipHandoffZones(State, EquipHandoffViewerId)->Hand.Num();
	const int32 BeforeEquipped = State.GetCardZoneState().EquippedCards.Num();

	FWBProductionEquipExecutionRequest BadTargetRequest = MakeEquipHandoffRequest(*Option, EquipHandoffOpponentHeroUnitId);
	const FWBProductionEquipExecutionHandoffResult BadTargetResult =
		FWBProductionEquipExecutionHandoff().ExecuteEquipFromProviderOption(
			State,
			Repository,
			DecisionData,
			BadTargetRequest);

	FWBProductionEquipExecutionRequest MissingSourceRequest = BadTargetRequest;
	MissingSourceRequest.SourceInstanceId = TEXT("missing_source");
	MissingSourceRequest.TargetUnitId = EquipHandoffHeroUnitId;
	const FWBProductionEquipExecutionHandoffResult MissingSourceResult =
		FWBProductionEquipExecutionHandoff().ExecuteEquipFromProviderOption(
			State,
			Repository,
			DecisionData,
			MissingSourceRequest);

	TestFalse(TEXT("Bad target rejected"), BadTargetResult.bOk);
	TestEqual(TEXT("Bad target reason"), BadTargetResult.Reason, FString(TEXT("target_unit_not_allowed")));
	TestFalse(TEXT("Missing source rejected"), MissingSourceResult.bOk);
	TestEqual(TEXT("Missing source reason"), MissingSourceResult.Reason, FString(TEXT("equip_option_missing")));
	TestEqual(TEXT("Hand unchanged"), FindEquipHandoffZones(State, EquipHandoffViewerId)->Hand.Num(), BeforeHand);
	TestEqual(TEXT("Equipped unchanged"), State.GetCardZoneState().EquippedCards.Num(), BeforeEquipped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionEquipExecutionHandoffStaleOptionTest, "Wandbound.Runtime.ProductionEquipExecutionHandoff.RevalidatesStaleProviderOption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionEquipExecutionHandoffStaleOptionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipHandoffState();
	AddEquipHandoffHandCard(State, EquipHandoffViewerId, TEXT("hand_wand"), TEXT("equip_wand"));
	const FWBCardDefinitionRepository Repository = MakeEquipHandoffRepository({ MakeEquipHandoffWandDefinition(TEXT("equip_wand"), TEXT("Copper Wand"), 2) });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildEquipHandoffDecisionData(State, Repository);
	const FWBProductionEquipOption* Option = FindEquipHandoffOption(DecisionData, TEXT("hand_wand"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	State.GetMutableUnitById(EquipHandoffHeroUnitId)->RLUsed = 2;
	const int32 BeforeHand = FindEquipHandoffZones(State, EquipHandoffViewerId)->Hand.Num();

	const FWBProductionEquipExecutionHandoffResult Result =
		FWBProductionEquipExecutionHandoff().ExecuteEquipFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeEquipHandoffRequest(*Option, EquipHandoffHeroUnitId));

	TestFalse(TEXT("Stale option rejected by core"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("insufficient_rl")));
	TestEqual(TEXT("Hand unchanged"), FindEquipHandoffZones(State, EquipHandoffViewerId)->Hand.Num(), BeforeHand);
	TestEqual(TEXT("Equipped unchanged"), State.GetCardZoneState().EquippedCards.Num(), 0);
	TestEqual(TEXT("RL unchanged"), State.GetUnitById(EquipHandoffHeroUnitId)->RLUsed, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionEquipExecutionHandoffRefreshEligibilityTest, "Wandbound.Runtime.ProductionEquipExecutionHandoff.RefreshReflectsRemainingRL", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionEquipExecutionHandoffRefreshEligibilityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipHandoffState();
	State.AddUnitForTest(MakeEquipHandoffUnit(11, EquipHandoffViewerId, TEXT("ally_card"), FWBTile(3, 4), 3));
	AddEquipHandoffHandCard(State, EquipHandoffViewerId, TEXT("hand_wand_a"), TEXT("equip_wand"), 0);
	AddEquipHandoffHandCard(State, EquipHandoffViewerId, TEXT("hand_wand_b"), TEXT("equip_wand"), 1);
	const FWBCardDefinitionRepository Repository = MakeEquipHandoffRepository({ MakeEquipHandoffWandDefinition(TEXT("equip_wand"), TEXT("Copper Wand"), 2) });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildEquipHandoffDecisionData(State, Repository);
	const FWBProductionEquipOption* Option = FindEquipHandoffOption(DecisionData, TEXT("hand_wand_a"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBProductionEquipExecutionHandoffResult Result =
		FWBProductionEquipExecutionHandoff().ExecuteEquipFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeEquipHandoffRequest(*Option, EquipHandoffHeroUnitId));
	const FWBProductionEquipOption* RemainingOption = FindEquipHandoffOption(Result.RefreshedSummonEquipData, TEXT("hand_wand_b"));

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestNotNull(TEXT("Remaining wand still offered"), RemainingOption);
	TestFalse(TEXT("Hero no longer eligible for RR 2"), RemainingOption != nullptr && RemainingOption->EligibleUnitIds.Contains(EquipHandoffHeroUnitId));
	TestTrue(TEXT("Other allied unit remains eligible"), RemainingOption != nullptr && RemainingOption->EligibleUnitIds.Contains(11));
	TestTrue(TEXT("Opponent unit not eligible"), RemainingOption != nullptr && !RemainingOption->EligibleUnitIds.Contains(EquipHandoffOpponentHeroUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionEquipExecutionHandoffHiddenInfoTest, "Wandbound.Runtime.ProductionEquipExecutionHandoff.HiddenInfoNotLeaked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionEquipExecutionHandoffHiddenInfoTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipHandoffState();
	AddEquipHandoffHandCard(State, EquipHandoffViewerId, TEXT("hand_wand"), TEXT("equip_wand"));
	AddEquipHandoffHandCard(State, EquipHandoffOpponentId, TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_CARD_DO_NOT_LEAK"));
	AddEquipHandoffDeckCard(State, EquipHandoffViewerId, TEXT("SECRET_DECK_DO_NOT_LEAK"), TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	AddEquipHandoffMarker(State, TEXT("SECRET_MARKER_DO_NOT_LEAK"));
	const FWBCardDefinitionRepository Repository = MakeEquipHandoffRepository({ MakeEquipHandoffWandDefinition() });
	const FWBProductionSummonEquipDecisionData DecisionData = BuildEquipHandoffDecisionData(State, Repository);
	const FWBProductionEquipOption* Option = FindEquipHandoffOption(DecisionData, TEXT("hand_wand"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	const FWBProductionEquipExecutionHandoffResult Result =
		FWBProductionEquipExecutionHandoff().ExecuteEquipFromProviderOption(
			State,
			Repository,
			DecisionData,
			MakeEquipHandoffRequest(*Option, EquipHandoffHeroUnitId));
	const FString Scan = BuildEquipHandoffScanText(Result);

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestFalse(TEXT("Opponent hand hidden"), Scan.Contains(TEXT("SECRET_OPPONENT_HAND_DO_NOT_LEAK")));
	TestFalse(TEXT("Deck hidden"), Scan.Contains(TEXT("SECRET_DECK_DO_NOT_LEAK")));
	TestFalse(TEXT("Marker hidden"), Scan.Contains(TEXT("SECRET_MARKER_DO_NOT_LEAK")));
	TestFalse(TEXT("Debug activation hidden"), Scan.Contains(TEXT("DebugActivationId")));
	TestFalse(TEXT("Usage key hidden"), Scan.Contains(TEXT("usage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionEquipExecutionHandoffRepositoryMutationTest, "Wandbound.Runtime.ProductionEquipExecutionHandoff.DoesNotMutateRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionEquipExecutionHandoffRepositoryMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEquipHandoffState();
	AddEquipHandoffHandCard(State, EquipHandoffViewerId, TEXT("hand_wand"), TEXT("equip_wand"));
	FWBCardDefinitionRepository Repository = MakeEquipHandoffRepository({ MakeEquipHandoffWandDefinition() });
	const TArray<FString> BeforeIds = WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository);
	const FWBProductionSummonEquipDecisionData DecisionData = BuildEquipHandoffDecisionData(State, Repository);
	const FWBProductionEquipOption* Option = FindEquipHandoffOption(DecisionData, TEXT("hand_wand"));
	if (!TestNotNull(TEXT("Provider option"), Option))
	{
		return false;
	}

	FWBProductionEquipExecutionHandoff().ExecuteEquipFromProviderOption(
		State,
		Repository,
		DecisionData,
		MakeEquipHandoffRequest(*Option, EquipHandoffHeroUnitId));

	TestTrue(TEXT("Repository ids unchanged"), BeforeIds == WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository));
	TestEqual(TEXT("RR unchanged"), Repository.Definitions[0].WandStats.RR, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionEquipExecutionHandoffSourceGuardTest, "Wandbound.Runtime.ProductionEquipExecutionHandoff.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionEquipExecutionHandoffSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Header;
	FString Source;
	TestTrue(TEXT("Header loads"), LoadEquipHandoffSourceFile(TEXT("Source/WandboundRuntime/Public/WBProductionEquipExecutionHandoff.h"), Header));
	TestTrue(TEXT("Source loads"), LoadEquipHandoffSourceFile(TEXT("Source/WandboundRuntime/Private/WBProductionEquipExecutionHandoff.cpp"), Source));
	const FString Combined = Header + Source;

	TestFalse(TEXT("No legal action generation"), Combined.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No FWBAction"), Combined.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No WBActionCodec"), Combined.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No WBEffectRunner"), Combined.Contains(TEXT("WBEffectRunner")));
	return true;
}

#endif
