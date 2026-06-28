#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBProductionActivationDataProvider.h"
#include "WBProductionActivationTargetSelectionBridge.h"
#include "WBRuntimeActivationDataProviderAdapter.h"
#include "WBRuntimeActivationDecisionSessionFacadeComponent.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 BridgeSourceUnitId = 11;
constexpr int32 BridgeEnemyUnitId = 22;

FWBPlayerStateData MakeBridgePlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeBridgeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 6;
	Unit.MaxHP = 6;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 4;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeBridgeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 40;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeBridgePlayer(0));
	State.Players.Add(MakeBridgePlayer(1, 0));
	State.AddUnitForTest(MakeBridgeUnit(
		BridgeSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("provider_board_card")));
	State.AddUnitForTest(MakeBridgeUnit(
		BridgeEnemyUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_enemy_card")));
	return State;
}

FWBZoneCardEntry MakeBridgeZoneEntry(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerId,
	const EWBCardZone Zone,
	const int32 ZoneIndex)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OwnerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	return Entry;
}

void AddBridgeOwnHandCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId)
{
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = 0;
	PlayerZones.Hand.Add(MakeBridgeZoneEntry(InstanceId, CardId, 0, EWBCardZone::Hand, 0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(PlayerZones);
}

void AddBridgeOpponentSecretHandCard(FWBGameStateData& State)
{
	FWBPlayerCardZoneState OpponentZones;
	OpponentZones.PlayerId = 1;
	OpponentZones.Hand.Add(MakeBridgeZoneEntry(
		TEXT("SECRET_OPPONENT_HAND_INSTANCE_DO_NOT_LEAK"),
		TEXT("SECRET_OPPONENT_HAND_CARD_DO_NOT_LEAK"),
		1,
		EWBCardZone::Hand,
		0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(OpponentZones);
}

void AddBridgeSecretDeckCard(FWBGameStateData& State)
{
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = 0;
	PlayerZones.Deck.Add(MakeBridgeZoneEntry(
		TEXT("SECRET_DECK_INSTANCE_DO_NOT_LEAK"),
		TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"),
		0,
		EWBCardZone::Deck,
		0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(PlayerZones);
}

void AddBridgeSecretMarker(FWBGameStateData& State)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 77;
	Marker.OwnerPlayerId = 1;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_MARKER_DO_NOT_LEAK");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

FWBCardActivationSourceGateDefinition MakeBridgeSourceGate(
	const EWBCardActivationSourceZone Zone)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.bHasExplicitSourceGate = true;
	Gate.RequiredZone = Zone;
	Gate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
	Gate.bRequiresSourceUnit = Zone == EWBCardActivationSourceZone::Board;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.bBlockedByStunned = true;
	Gate.bBlockedByFrozen = false;
	Gate.bRequiresFixtureZoneOwnership = Zone == EWBCardActivationSourceZone::Board
		|| Zone == EWBCardActivationSourceZone::Hand;
	return Gate;
}

FWBGenericEffectPayload MakeBridgePayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = 1;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("production_target_selection_bridge_test"));
	return Payload;
}

FWBCardEffectDefinition MakeBridgeEffect(
	const FString& EffectId,
	const FString& Label,
	const EWBCardActivationSourceZone SourceZone,
	const EWBCardEffectTargetRequirement TargetRequirement)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = Label;
	Effect.TargetRequirement = TargetRequirement;
	Effect.SourceGate = MakeBridgeSourceGate(SourceZone);
	Effect.Payloads.Add(MakeBridgePayload());
	return Effect;
}

FWBCardDefinition MakeBridgeDefinition(
	const FString& CardId,
	const FString& PublicName,
	const EWBCardActivationSourceZone SourceZone,
	const FString& EffectId,
	const FString& Label,
	const EWBCardEffectTargetRequirement TargetRequirement)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeBridgeEffect(
		EffectId,
		Label,
		SourceZone,
		TargetRequirement));
	return Definition;
}

FWBCardDefinitionRepository MakeBridgeRepository(
	const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("production_target_selection_bridge_repo");
	Repository.SourceVersion = TEXT("target_selection_bridge_test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBCardDefinitionRepository MakeBridgeBoardRepository(
	const EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::Unit)
{
	return MakeBridgeRepository({
		MakeBridgeDefinition(
			TEXT("provider_board_card"),
			TEXT("Provider Board Card"),
			EWBCardActivationSourceZone::Board,
			TEXT("unit_target"),
			TEXT("Choose Unit Burst"),
			TargetRequirement)
	});
}

FWBCardDefinitionRepository MakeBridgeHandRepository(
	const EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::Unit)
{
	return MakeBridgeRepository({
		MakeBridgeDefinition(
			TEXT("provider_hand_card"),
			TEXT("Provider Hand Card"),
			EWBCardActivationSourceZone::Hand,
			TEXT("hand_unit_target"),
			TEXT("Hand Unit Burst"),
			TargetRequirement)
	});
}

FWBProductionActivationDataProviderInput MakeBridgeInput(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	FWBProductionActivationDataProviderInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 0;
	return Input;
}

FWBRuntimeActivationDataProviderRequest MakeBridgeProviderRequest()
{
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint;
	Request.PlayerId = 0;
	Request.DecisionPointId = TEXT("production_target_selection_bridge_decision");
	Request.RequestReason = TEXT("production_target_selection_bridge_test");
	return Request;
}

FWBRuntimeActivationDataProviderResult RunBridgeProvider(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBProductionActivationDataProvider& Provider)
{
	Provider.Configure(MakeBridgeInput(State, Repository), FWBProductionActivationDataProviderConfig());
	return Provider.GetActivationDecisionData(MakeBridgeProviderRequest());
}

const FWBCardActivationLegalAction* FirstBridgeAction(
	const FWBRuntimeActivationDataProviderResult& Result)
{
	return Result.RefreshInput.ActivationActionSet.Actions.Num() > 0
		? &Result.RefreshInput.ActivationActionSet.Actions[0]
		: nullptr;
}

const FWBCardActivationTargetOption* FindBridgeTargetOption(
	const FWBCardActivationLegalAction& Action,
	const int32 TargetUnitId)
{
	for (const FWBCardActivationTargetOption& Option : Action.TargetOptions)
	{
		if (Option.TargetUnitId == TargetUnitId)
		{
			return &Option;
		}
	}

	return nullptr;
}

FWBProductionActivationTargetSelectionRequest MakeBridgeSelectionRequest(
	const FWBCardActivationLegalAction& Action,
	const FWBCardActivationTargetOption* TargetOption = nullptr,
	const FString& SourceInstanceId = FString())
{
	FWBProductionActivationTargetSelectionRequest Request;
	Request.ActivationEntryId = Action.ActivationActionId;
	Request.SourceInstanceId = SourceInstanceId;
	Request.SourceCardId = Action.Candidate.SourceCardId;
	Request.EffectId = Action.Candidate.SourceEffectId;
	if (TargetOption != nullptr)
	{
		Request.bHasSelectedTarget = true;
		Request.SelectedTargetOption = *TargetOption;
	}
	return Request;
}

FWBProductionActivationTargetSelectionBridge MakeConfiguredBridge(
	const FWBRuntimeActivationDataProviderResult& Result)
{
	FWBProductionActivationTargetSelectionBridge Bridge;
	Bridge.ConfigureFromProviderData(Result.RefreshInput.ActivationActionSet.Actions);
	return Bridge;
}

FString BuildBridgeResultScanText(const FWBProductionActivationTargetSelectionResult& Result)
{
	FString Text;
	Text += Result.Reason;
	Text += Result.SourceCardId;
	Text += Result.EffectId;
	Text += FString::FromInt(Result.TargetUnitId);
	Text += Result.Command.Source.SourceCardId;
	Text += Result.Command.Source.SourceEffectId;
	Text += Result.Command.EffectRequest.Source.SourceCardId;
	Text += Result.Command.EffectRequest.Source.SourceEffectId;
	Text += FString::FromInt(Result.Command.EffectRequest.Target.TargetUnitId);
	Text += Result.Command.DebugActivationId;
	Text += Result.Command.UsageCommit.UsageKey;
	return Text;
}

bool LoadBridgeSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(OutSource, *Path);
}

void AssertBridgeResult(
	FAutomationTestBase& Test,
	const FWBProductionActivationTargetSelectionResult& Result,
	const EWBProductionActivationTargetSelectionResultCode ExpectedCode,
	const FString& Label)
{
	Test.TestEqual(
		*FString::Printf(TEXT("%s code"), *Label),
		Result.Code,
		ExpectedCode);
	Test.TestEqual(
		*FString::Printf(TEXT("%s reason"), *Label),
		Result.Reason,
		FWBProductionActivationTargetSelectionBridge::ResultCodeToString(ExpectedCode));
	Test.TestEqual(
		*FString::Printf(TEXT("%s ok"), *Label),
		Result.bOk,
		ExpectedCode == EWBProductionActivationTargetSelectionResultCode::Success);
}

bool MakeBridgeSessionPath(
	UWBRuntimeActivationDecisionSessionFacadeComponent*& OutFacade,
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel)
{
	OutFacade = NewObject<UWBRuntimeActivationDecisionSessionFacadeComponent>(GetTransientPackage());
	OutOwner = NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
	OutCoordinator = NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	OutTurnModel = NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
	OutActivationModel = NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());

	if (OutFacade == nullptr
		|| OutOwner == nullptr
		|| OutCoordinator == nullptr
		|| OutTurnModel == nullptr
		|| OutActivationModel == nullptr)
	{
		return false;
	}

	OutCoordinator->bRefreshVisualController = false;
	OutCoordinator->SetTurnInteractionModel(OutTurnModel);
	OutCoordinator->SetActivationPresentationModel(OutActivationModel);
	OutOwner->SetDecisionPointCoordinator(OutCoordinator);
	OutFacade->SetDecisionPointOwner(OutOwner);
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeRequiresConfigTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.RequiresConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeRequiresConfigTest::RunTest(const FString& Parameters)
{
	FWBProductionActivationTargetSelectionBridge Bridge;
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(FWBProductionActivationTargetSelectionRequest());

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::BridgeNotConfigured,
		TEXT("Unconfigured bridge"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeMissingEntryTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.MissingActivationEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeMissingEntryTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);

	FWBProductionActivationTargetSelectionRequest Request;
	Request.ActivationEntryId = TEXT("missing_activation_entry");
	Request.SourceCardId = TEXT("provider_board_card");
	Request.EffectId = TEXT("unit_target");
	const FWBProductionActivationTargetSelectionResult Result = Bridge.BuildCommandForSelection(Request);

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::ActivationEntryMissing,
		TEXT("Missing entry"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeBoardSourceSuccessTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.BoardSourceUnitTargetSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeBoardSourceSuccessTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	TestNotNull(TEXT("Enemy target option"), TargetOption);

	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action, TargetOption));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::Success,
		TEXT("Board source"));
	TestEqual(TEXT("Source player"), Result.Command.Source.PlayerId, 0);
	TestEqual(TEXT("Source unit"), Result.Command.Source.SourceUnitId, BridgeSourceUnitId);
	TestEqual(TEXT("Source card"), Result.Command.Source.SourceCardId, FString(TEXT("provider_board_card")));
	TestEqual(TEXT("Effect id"), Result.Command.Source.SourceEffectId, FString(TEXT("unit_target")));
	TestEqual(TEXT("Bound target unit"), Result.Command.EffectRequest.Target.TargetUnitId, BridgeEnemyUnitId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeOwnHandSuccessTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.OwnHandUnitTargetSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeOwnHandSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	AddBridgeOwnHandCard(State, TEXT("hand_instance_0"), TEXT("provider_hand_card"));
	const FWBCardDefinitionRepository Repository = MakeBridgeHandRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	TestNotNull(TEXT("Enemy target option"), TargetOption);

	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action, TargetOption, TEXT("hand_instance_0")));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::Success,
		TEXT("Own hand source"));
	TestEqual(TEXT("Hand source unit"), Result.Command.Source.SourceUnitId, -1);
	TestEqual(TEXT("Source card"), Result.Command.Source.SourceCardId, FString(TEXT("provider_hand_card")));
	TestEqual(TEXT("Effect id"), Result.Command.Source.SourceEffectId, FString(TEXT("hand_unit_target")));
	TestEqual(TEXT("Bound target unit"), Result.Command.EffectRequest.Target.TargetUnitId, BridgeEnemyUnitId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeUnitTargetMissingTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.UnitTargetRequiredButMissing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeUnitTargetMissingTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);

	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::TargetRequiredButMissing,
		TEXT("Missing target"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeTargetNotAllowedTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.TargetNotAllowed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeTargetNotAllowedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	TestNotNull(TEXT("Enemy target option"), TargetOption);

	FWBCardActivationTargetOption TamperedOption = *TargetOption;
	TamperedOption.TargetUnitId = 999;
	FWBProductionActivationTargetSelectionRequest Request =
		MakeBridgeSelectionRequest(*Action, &TamperedOption);
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result = Bridge.BuildCommandForSelection(Request);

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::TargetNotAllowed,
		TEXT("Target not allowed"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeTargetTypeMismatchTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.TargetTypeMismatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeTargetTypeMismatchTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	TestNotNull(TEXT("Enemy target option"), TargetOption);

	FWBCardActivationTargetOption WrongTypeOption = *TargetOption;
	WrongTypeOption.Type = EWBCardActivationTargetOptionType::Unknown;
	FWBProductionActivationTargetSelectionRequest Request =
		MakeBridgeSelectionRequest(*Action, &WrongTypeOption);
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result = Bridge.BuildCommandForSelection(Request);

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::TargetTypeMismatch,
		TEXT("Target type mismatch"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeTargetStaleTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.TargetStaleOrMissing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeTargetStaleTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);

	FWBCardActivationTargetOption MissingOption;
	MissingOption.Type = EWBCardActivationTargetOptionType::Unit;
	FWBProductionActivationTargetSelectionRequest Request =
		MakeBridgeSelectionRequest(*Action, &MissingOption);
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result = Bridge.BuildCommandForSelection(Request);

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::TargetStaleOrMissing,
		TEXT("Target stale"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoTargetSuccessTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.NoTargetSucceedsWithoutTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoTargetSuccessTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository =
		MakeBridgeBoardRepository(EWBCardEffectTargetRequirement::None);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);

	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::Success,
		TEXT("No target"));
	TestEqual(TEXT("No target unit"), Result.Command.EffectRequest.Target.TargetUnitId, -1);
	TestEqual(TEXT("No target result"), Result.TargetUnitId, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoTargetProvidedFailsTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.TargetProvidedForNoTargetFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoTargetProvidedFailsTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository =
		MakeBridgeBoardRepository(EWBCardEffectTargetRequirement::None);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);

	FWBCardActivationTargetOption Option;
	Option.Type = EWBCardActivationTargetOptionType::Unit;
	Option.TargetUnitId = BridgeEnemyUnitId;
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action, &Option));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::TargetProvidedForNoTargetEffect,
		TEXT("No target with selected target"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeTileUnsupportedTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.TileTargetUnsupported", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeTileUnsupportedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository =
		MakeBridgeBoardRepository(EWBCardEffectTargetRequirement::Tile);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);

	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::UnsupportedTargetRequirement,
		TEXT("Tile unsupported"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeWallUnsupportedTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.WallTargetUnsupported", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeWallUnsupportedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository =
		MakeBridgeBoardRepository(EWBCardEffectTargetRequirement::WallEdge);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);

	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::UnsupportedTargetRequirement,
		TEXT("Wall unsupported"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeDeterministicTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DeterministicCommandResult", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	TestNotNull(TEXT("Enemy target option"), TargetOption);

	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionRequest Request =
		MakeBridgeSelectionRequest(*Action, TargetOption);
	const FWBProductionActivationTargetSelectionResult First = Bridge.BuildCommandForSelection(Request);
	const FWBProductionActivationTargetSelectionResult Second = Bridge.BuildCommandForSelection(Request);

	TestEqual(TEXT("Same code"), First.Code, Second.Code);
	TestEqual(TEXT("Same reason"), First.Reason, Second.Reason);
	TestEqual(TEXT("Same source card"), First.Command.Source.SourceCardId, Second.Command.Source.SourceCardId);
	TestEqual(TEXT("Same source effect"), First.Command.Source.SourceEffectId, Second.Command.Source.SourceEffectId);
	TestEqual(TEXT("Same target unit"), First.Command.EffectRequest.Target.TargetUnitId, Second.Command.EffectRequest.Target.TargetUnitId);
	TestEqual(TEXT("Same target tile"), First.Command.EffectRequest.Target.TargetTile, Second.Command.EffectRequest.Target.TargetTile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeOpponentHandHiddenTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.OpponentHandHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeOpponentHandHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	AddBridgeOpponentSecretHandCard(State);
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action, TargetOption));

	TestFalse(TEXT("Opponent hand secret absent"), BuildBridgeResultScanText(Result).Contains(TEXT("SECRET_OPPONENT_HAND")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeDeckHiddenTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DeckIdentityHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeDeckHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	AddBridgeSecretDeckCard(State);
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action, TargetOption));

	TestFalse(TEXT("Deck secret absent"), BuildBridgeResultScanText(Result).Contains(TEXT("SECRET_DECK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeMarkerHiddenTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.HiddenMarkerIdentityHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeMarkerHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	AddBridgeSecretMarker(State);
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action, TargetOption));

	TestFalse(TEXT("Marker secret absent"), BuildBridgeResultScanText(Result).Contains(TEXT("SECRET_MARKER")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeHiddenTargetRejectedTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.HiddenTargetRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeHiddenTargetRejectedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult = RunBridgeProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstBridgeAction(ProviderResult);
	TestNotNull(TEXT("Action emitted"), Action);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindBridgeTargetOption(*Action, BridgeEnemyUnitId) : nullptr;
	TestNotNull(TEXT("Enemy target option"), TargetOption);

	FWBCardActivationTargetOption HiddenOption = *TargetOption;
	HiddenOption.TargetCardId = TEXT("SECRET_TARGET_DO_NOT_LEAK");
	FWBProductionActivationTargetSelectionBridge Bridge = MakeConfiguredBridge(ProviderResult);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(*Action, &HiddenOption));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::HiddenTargetRejected,
		TEXT("Hidden target"));
	TestFalse(TEXT("Hidden target reason redacted"), BuildBridgeResultScanText(Result).Contains(TEXT("SECRET_TARGET")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoStateMutationSourceGuardTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DoesNotMutateGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoStateMutationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationTargetSelectionBridge.cpp") }, Source));
	TestTrue(TEXT("Bridge header loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationTargetSelectionBridge.h") }, Header));
	TestFalse(TEXT("No game state in source"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("No game state in header"), Header.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("No mutable state lookup"), Source.Contains(TEXT("GetMutable")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoRepositoryMutationSourceGuardTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DoesNotMutateRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoRepositoryMutationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationTargetSelectionBridge.cpp") }, Source));
	TestTrue(TEXT("Bridge header loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationTargetSelectionBridge.h") }, Header));
	TestFalse(TEXT("No repository in source"), Source.Contains(TEXT("FWBCardDefinitionRepository")));
	TestFalse(TEXT("No repository in header"), Header.Contains(TEXT("FWBCardDefinitionRepository")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoEffectExecutionSourceGuardTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DoesNotExecuteEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoEffectExecutionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationTargetSelectionBridge.cpp") }, Source));
	TestTrue(TEXT("Bridge header loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationTargetSelectionBridge.h") }, Header));
	TestFalse(TEXT("No EffectRunner in source"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No EffectRunner in header"), Header.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No card activation execution"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	TestFalse(TEXT("No effect request execution"), Source.Contains(TEXT("ApplyEffect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoRulesGenerationSourceGuardTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DoesNotCallRulesGenerateLegalActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoRulesGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationTargetSelectionBridge.cpp") }, Source));
	TestTrue(TEXT("Bridge header loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationTargetSelectionBridge.h") }, Header));
	TestFalse(TEXT("No WBRules source"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("No WBRules header"), Header.Contains(TEXT("WBRules")));
	TestFalse(TEXT("No GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoActionSourceGuardTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DoesNotCreateFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoActionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationTargetSelectionBridge.cpp") }, Source));
	TestTrue(TEXT("Bridge header loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationTargetSelectionBridge.h") }, Header));
	TestFalse(TEXT("No FWBAction source"), Source.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No FWBAction header"), Header.Contains(TEXT("FWBAction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeNoActionCodecSourceGuardTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.DoesNotDependOnWBActionCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeNoActionCodecSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationTargetSelectionBridge.cpp") }, Source));
	TestTrue(TEXT("Bridge header loads"), LoadBridgeSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationTargetSelectionBridge.h") }, Header));
	TestFalse(TEXT("No ActionCodec source"), Source.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No ActionCodec header"), Header.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetSelectionBridgeRuntimeHarnessTest, "Wandbound.Runtime.ProductionActivationTargetSelectionBridge.RuntimeHarnessConsumesBridgeOutput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetSelectionBridgeRuntimeHarnessTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeState();
	const FWBCardDefinitionRepository Repository = MakeBridgeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	Provider.Configure(MakeBridgeInput(State, Repository), FWBProductionActivationDataProviderConfig());

	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeBridgeSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationProviderRefreshResult RefreshResult =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeBridgeProviderRequest());
	TestTrue(TEXT("Refresh ok"), RefreshResult.bOk);
	TestEqual(TEXT("Session action count"), ActivationModel->GetCurrentActivationActionSet().Actions.Num(), 1);

	const FWBCardActivationLegalAction& Action =
		ActivationModel->GetCurrentActivationActionSet().Actions[0];
	const FWBCardActivationTargetOption* TargetOption =
		FindBridgeTargetOption(Action, BridgeEnemyUnitId);
	TestNotNull(TEXT("Enemy target option"), TargetOption);

	FWBProductionActivationTargetSelectionBridge Bridge;
	Bridge.ConfigureFromProviderData(ActivationModel->GetCurrentActivationActionSet().Actions);
	const FWBProductionActivationTargetSelectionResult Result =
		Bridge.BuildCommandForSelection(MakeBridgeSelectionRequest(Action, TargetOption));

	AssertBridgeResult(
		*this,
		Result,
		EWBProductionActivationTargetSelectionResultCode::Success,
		TEXT("Runtime harness bridge"));
	TestEqual(TEXT("Bound target unit"), Result.Command.EffectRequest.Target.TargetUnitId, BridgeEnemyUnitId);
	return true;
}

#endif
