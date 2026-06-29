#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBProductionActivationDataProvider.h"
#include "WBProductionActivationExecutionHandoff.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 HandoffSourceUnitId = 11;
constexpr int32 HandoffEnemyUnitId = 22;

FWBPlayerStateData MakeHandoffPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeHandoffUnit(
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
	Unit.CurrentArmor = 0;
	Unit.MaxArmor = 0;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 4;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeHandoffState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 50;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeHandoffPlayer(0));
	State.Players.Add(MakeHandoffPlayer(1, 0));
	State.AddUnitForTest(MakeHandoffUnit(
		HandoffSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("handoff_board_card")));
	State.AddUnitForTest(MakeHandoffUnit(
		HandoffEnemyUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_enemy_card")));
	return State;
}

FWBZoneCardEntry MakeHandoffZoneEntry(
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

FWBPlayerCardZoneState& FindOrAddHandoffPlayerZones(
	FWBGameStateData& State,
	const int32 PlayerId)
{
	FWBCardZoneState& ZoneState = State.GetMutableCardZoneStateForTest();
	if (FWBPlayerCardZoneState* Existing = WBCardZoneState::FindMutablePlayerZones(ZoneState, PlayerId))
	{
		return *Existing;
	}

	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = PlayerId;
	return ZoneState.PlayerZones.Add_GetRef(PlayerZones);
}

void AddOwnHandCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId)
{
	FWBPlayerCardZoneState& PlayerZones = FindOrAddHandoffPlayerZones(State, 0);
	PlayerZones.Hand.Add(MakeHandoffZoneEntry(
		InstanceId,
		CardId,
		0,
		EWBCardZone::Hand,
		PlayerZones.Hand.Num()));
}

void AddOpponentSecretHandCard(FWBGameStateData& State)
{
	FWBPlayerCardZoneState& OpponentZones = FindOrAddHandoffPlayerZones(State, 1);
	OpponentZones.Hand.Add(MakeHandoffZoneEntry(
		TEXT("SECRET_OPPONENT_HAND_INSTANCE_DO_NOT_LEAK"),
		TEXT("SECRET_OPPONENT_HAND_CARD_DO_NOT_LEAK"),
		1,
		EWBCardZone::Hand,
		OpponentZones.Hand.Num()));
}

void AddSecretDeckCard(FWBGameStateData& State)
{
	FWBPlayerCardZoneState& PlayerZones = FindOrAddHandoffPlayerZones(State, 0);
	PlayerZones.Deck.Add(MakeHandoffZoneEntry(
		TEXT("SECRET_DECK_INSTANCE_DO_NOT_LEAK"),
		TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"),
		0,
		EWBCardZone::Deck,
		PlayerZones.Deck.Num()));
}

void AddSecretMarker(FWBGameStateData& State)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 101;
	Marker.OwnerPlayerId = 1;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_MARKER_DO_NOT_LEAK");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

FWBCardActivationSourceGateDefinition MakeHandoffSourceGate(
	const EWBCardActivationSourceZone Zone,
	const bool bOncePerTurn = false,
	const FString& OncePerTurnKey = FString())
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
	Gate.bOncePerTurn = bOncePerTurn;
	Gate.OncePerTurnKey = OncePerTurnKey;
	return Gate;
}

FWBGenericEffectPayload MakeDamagePayload(const int32 Amount = 1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("production_activation_execution_handoff_test"));
	return Payload;
}

FWBCardEffectDefinition MakeHandoffEffect(
	const FString& EffectId,
	const FString& Label,
	const EWBCardActivationSourceZone SourceZone,
	const EWBCardEffectTargetRequirement TargetRequirement,
	const bool bOncePerTurn = false,
	const FString& OncePerTurnKey = FString())
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = Label;
	Effect.TargetRequirement = TargetRequirement;
	Effect.SourceGate = MakeHandoffSourceGate(SourceZone, bOncePerTurn, OncePerTurnKey);
	Effect.Payloads.Add(MakeDamagePayload());
	return Effect;
}

FWBCardDefinition MakeHandoffDefinition(
	const FString& CardId,
	const FString& PublicName,
	const EWBCardActivationSourceZone SourceZone,
	const FString& EffectId,
	const FString& Label,
	const EWBCardEffectTargetRequirement TargetRequirement,
	const bool bOncePerTurn = false,
	const FString& OncePerTurnKey = FString())
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeHandoffEffect(
		EffectId,
		Label,
		SourceZone,
		TargetRequirement,
		bOncePerTurn,
		OncePerTurnKey));
	return Definition;
}

FWBCardDefinitionRepository MakeHandoffRepository(
	const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("production_activation_execution_handoff_repo");
	Repository.SourceVersion = TEXT("execution_handoff_test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBCardDefinitionRepository MakeBoardRepository(
	const EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::Unit,
	const bool bOncePerTurn = false,
	const FString& OncePerTurnKey = FString())
{
	return MakeHandoffRepository({
		MakeHandoffDefinition(
			TEXT("handoff_board_card"),
			TEXT("Handoff Board Card"),
			EWBCardActivationSourceZone::Board,
			TEXT("board_burst"),
			TEXT("Board Burst"),
			TargetRequirement,
			bOncePerTurn,
			OncePerTurnKey)
	});
}

FWBCardDefinitionRepository MakeHandRepository()
{
	return MakeHandoffRepository({
		MakeHandoffDefinition(
			TEXT("handoff_hand_card"),
			TEXT("Handoff Hand Card"),
			EWBCardActivationSourceZone::Hand,
			TEXT("hand_burst"),
			TEXT("Hand Burst"),
			EWBCardEffectTargetRequirement::Unit)
	});
}

FWBProductionActivationDataProviderInput MakeProviderInput(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ViewerPlayerId = 0)
{
	FWBProductionActivationDataProviderInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = ViewerPlayerId;
	return Input;
}

FWBRuntimeActivationDataProviderRequest MakeProviderRequest()
{
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint;
	Request.PlayerId = 0;
	Request.DecisionPointId = TEXT("production_activation_execution_handoff_decision");
	Request.RequestReason = TEXT("production_activation_execution_handoff_test");
	return Request;
}

FWBRuntimeActivationDataProviderResult RunProvider(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBProductionActivationDataProvider& Provider,
	const int32 ViewerPlayerId = 0)
{
	Provider.Configure(
		MakeProviderInput(State, Repository, ViewerPlayerId),
		FWBProductionActivationDataProviderConfig());
	return Provider.GetActivationDecisionData(MakeProviderRequest());
}

const FWBCardActivationLegalAction* FirstAction(
	const FWBRuntimeActivationDataProviderResult& Result)
{
	return Result.RefreshInput.ActivationActionSet.Actions.Num() > 0
		? &Result.RefreshInput.ActivationActionSet.Actions[0]
		: nullptr;
}

const FWBCardActivationTargetOption* FindTargetOption(
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

FWBProductionActivationTargetSelectionRequest MakeSelectionRequest(
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

FWBProductionActivationExecutionHandoffInput MakeExecutionInput(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBProductionActivationDataProvider& Provider,
	const FWBProductionActivationTargetSelectionRequest& SelectionRequest,
	const int32 ViewerPlayerId = 0)
{
	FWBProductionActivationExecutionHandoffInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = ViewerPlayerId;
	Input.Provider = &Provider;
	Input.SelectionRequest = SelectionRequest;
	return Input;
}

FWBProductionActivationExecutionHandoffResult ExecuteFirstBoardUnitTarget(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBProductionActivationDataProvider& Provider)
{
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	const FWBCardActivationTargetOption* TargetOption =
		Action != nullptr ? FindTargetOption(*Action, HandoffEnemyUnitId) : nullptr;

	if (Action == nullptr || TargetOption == nullptr)
	{
		return FWBProductionActivationExecutionHandoffResult();
	}

	const FWBProductionActivationTargetSelectionRequest SelectionRequest =
		MakeSelectionRequest(*Action, TargetOption);
	return FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
		MakeExecutionInput(State, Repository, Provider, SelectionRequest));
}

FString BuildResultScanText(const FWBProductionActivationExecutionHandoffResult& Result)
{
	FString Text;
	Text += Result.Reason;
	Text += Result.SourceCardId;
	Text += Result.EffectId;
	Text += FString::FromInt(Result.TargetUnitId);

	for (const FString& Diagnostic : Result.Diagnostics)
	{
		Text += Diagnostic;
	}

	for (const FWBCardActivationLegalAction& Action : Result.RefreshedActivationEntries)
	{
		Text += Action.ActivationActionId;
		Text += Action.PublicLabel;
		Text += Action.Candidate.SourceCardId;
		Text += Action.Candidate.SourceEffectId;
		Text += Action.Command.Source.SourceCardId;
		Text += Action.Command.Source.SourceEffectId;
		Text += Action.Command.DebugActivationId;
		Text += Action.Command.UsageCommit.UsageKey;
		for (const FWBCardActivationTargetOption& TargetOption : Action.TargetOptions)
		{
			Text += FString::FromInt(TargetOption.TargetUnitId);
			Text += FString::FromInt(TargetOption.TargetOwnerPlayerId);
			Text += FString::FromInt(TargetOption.TargetTile.X);
			Text += FString::FromInt(TargetOption.TargetTile.Y);
			Text += TargetOption.TargetCardId;
			Text += TargetOption.PublicLabel;
		}
	}

	return Text;
}

bool LoadSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(OutSource, *Path);
}

void AssertResultCode(
	FAutomationTestBase& Test,
	const FWBProductionActivationExecutionHandoffResult& Result,
	const EWBProductionActivationExecutionHandoffResultCode ExpectedCode,
	const FString& Label)
{
	Test.TestEqual(
		*FString::Printf(TEXT("%s code"), *Label),
		Result.Code,
		ExpectedCode);
	Test.TestEqual(
		*FString::Printf(TEXT("%s reason"), *Label),
		Result.Reason,
		FWBProductionActivationExecutionHandoff::ResultCodeToString(ExpectedCode));
	Test.TestEqual(
		*FString::Printf(TEXT("%s ok"), *Label),
		Result.bOk,
		ExpectedCode == EWBProductionActivationExecutionHandoffResultCode::Success);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffMissingGameStateTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.MissingGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffMissingGameStateTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	FWBProductionActivationExecutionHandoffInput Input;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 0;
	Input.Provider = &Provider;

	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(Input);
	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::GameStateMissing,
		TEXT("Missing game state"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffMissingRepositoryTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.MissingRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffMissingRepositoryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	FWBProductionActivationDataProvider Provider;
	FWBProductionActivationExecutionHandoffInput Input;
	Input.GameState = &State;
	Input.ViewerPlayerId = 0;
	Input.Provider = &Provider;

	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(Input);
	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::RepositoryMissing,
		TEXT("Missing repository"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffMissingProviderTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.MissingProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffMissingProviderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationExecutionHandoffInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 0;

	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(Input);
	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::ProviderMissing,
		TEXT("Missing provider"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffUnconfiguredProviderTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.UnconfiguredProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffUnconfiguredProviderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	FWBProductionActivationExecutionHandoffInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 0;
	Input.Provider = &Provider;

	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(Input);
	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::ProviderMissing,
		TEXT("Unconfigured provider"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffInvalidViewerTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.InvalidViewerRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffInvalidViewerTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const int32 BeforeHP = State.GetUnitById(HandoffEnemyUnitId)->HP;
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	RunProvider(State, Repository, Provider);

	FWBProductionActivationExecutionHandoffInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 7;
	Input.Provider = &Provider;

	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(Input);
	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::ExecutionRejected,
		TEXT("Invalid viewer"));
	TestEqual(TEXT("No HP mutation"), State.GetUnitById(HandoffEnemyUnitId)->HP, BeforeHP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffSelectionFailureNoMutationTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.SelectionFailureNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffSelectionFailureNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const int32 BeforeHP = State.GetUnitById(HandoffEnemyUnitId)->HP;
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("Action emitted"), Action))
	{
		return false;
	}

	const FWBProductionActivationTargetSelectionRequest SelectionRequest =
		MakeSelectionRequest(*Action);
	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));

	TestEqual(TEXT("Selection failure code"), Result.Code, EWBProductionActivationExecutionHandoffResultCode::SelectionFailed);
	TestEqual(TEXT("Selection reason"), Result.Reason, FString(TEXT("target_required_but_missing")));
	TestFalse(TEXT("Execution not applied"), Result.bExecutionApplied);
	TestEqual(TEXT("No HP mutation"), State.GetUnitById(HandoffEnemyUnitId)->HP, BeforeHP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffMissingEntryTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.MissingActivationEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffMissingEntryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	RunProvider(State, Repository, Provider);

	FWBProductionActivationTargetSelectionRequest SelectionRequest;
	SelectionRequest.ActivationEntryId = TEXT("missing_activation_entry");
	SelectionRequest.SourceCardId = TEXT("handoff_board_card");
	SelectionRequest.EffectId = TEXT("board_burst");
	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));

	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::ActivationEntryMissing,
		TEXT("Missing entry"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffBoardSourceSuccessTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.BoardSourceUnitTargetExecutes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffBoardSourceSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const int32 BeforeHP = State.GetUnitById(HandoffEnemyUnitId)->HP;
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);

	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::Success,
		TEXT("Board source"));
	TestTrue(TEXT("Execution applied"), Result.bExecutionApplied);
	TestEqual(TEXT("Target HP changed"), State.GetUnitById(HandoffEnemyUnitId)->HP, BeforeHP - 1);
	TestTrue(TEXT("Refresh returned entries"), Result.RefreshedActivationEntries.Num() > 0);
	TestEqual(TEXT("Safe source card"), Result.SourceCardId, FString(TEXT("handoff_board_card")));
	TestEqual(TEXT("Safe effect id"), Result.EffectId, FString(TEXT("board_burst")));
	TestEqual(TEXT("Safe target unit"), Result.TargetUnitId, HandoffEnemyUnitId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffOwnHandSuccessTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.OwnHandUnitTargetExecutes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffOwnHandSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddOwnHandCard(State, TEXT("hand_instance_0"), TEXT("handoff_hand_card"));
	const int32 BeforeHP = State.GetUnitById(HandoffEnemyUnitId)->HP;
	const FWBCardDefinitionRepository Repository = MakeHandRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("Hand action emitted"), Action))
	{
		return false;
	}
	const FWBCardActivationTargetOption* TargetOption = FindTargetOption(*Action, HandoffEnemyUnitId);
	if (!TestNotNull(TEXT("Enemy target option"), TargetOption))
	{
		return false;
	}

	const FWBProductionActivationTargetSelectionRequest SelectionRequest =
		MakeSelectionRequest(*Action, TargetOption, TEXT("hand_instance_0"));
	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));

	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::Success,
		TEXT("Own hand source"));
	TestEqual(TEXT("Target HP changed"), State.GetUnitById(HandoffEnemyUnitId)->HP, BeforeHP - 1);
	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), 0);
	TestEqual(TEXT("Used card left hand"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 0);
	TestEqual(TEXT("Used card entered discard"), PlayerZones != nullptr ? PlayerZones->Discard.Num() : -1, 1);
	TestEqual(TEXT("Discarded instance"), PlayerZones != nullptr ? PlayerZones->Discard[0].Card.InstanceId : FString(), FString(TEXT("hand_instance_0")));
	TestEqual(TEXT("Refresh no longer exposes spent hand card"), Result.RefreshedActivationEntries.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffFailedOwnHandActivationDoesNotDiscardTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.FailedOwnHandActivationDoesNotDiscard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffFailedOwnHandActivationDoesNotDiscardTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddOwnHandCard(State, TEXT("hand_instance_0"), TEXT("handoff_hand_card"));
	const FWBCardDefinitionRepository Repository = MakeHandRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("Hand action emitted"), Action))
	{
		return false;
	}

	const FWBProductionActivationTargetSelectionRequest SelectionRequest =
		MakeSelectionRequest(*Action);
	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));
	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), 0);

	TestEqual(TEXT("Selection failure"), Result.Code, EWBProductionActivationExecutionHandoffResultCode::SelectionFailed);
	TestFalse(TEXT("Execution not applied"), Result.bExecutionApplied);
	TestEqual(TEXT("Card remains in hand"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 1);
	TestEqual(TEXT("Discard remains empty"), PlayerZones != nullptr ? PlayerZones->Discard.Num() : -1, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffBoardSourceDoesNotDiscardHandCardTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.BoardSourceDoesNotDiscardHandCard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffBoardSourceDoesNotDiscardHandCardTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddOwnHandCard(State, TEXT("unrelated_hand_instance"), TEXT("handoff_hand_card"));
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);
	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), 0);

	TestTrue(TEXT("Board execution succeeds"), Result.bOk);
	TestEqual(TEXT("Hand card remains in hand"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 1);
	TestEqual(TEXT("Discard remains empty"), PlayerZones != nullptr ? PlayerZones->Discard.Num() : -1, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffHandDiscardRefreshHidesSecretsTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.HandDiscardRefreshHidesSecrets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffHandDiscardRefreshHidesSecretsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddOwnHandCard(State, TEXT("hand_instance_0"), TEXT("handoff_hand_card"));
	AddOpponentSecretHandCard(State);
	AddSecretDeckCard(State);
	AddSecretMarker(State);
	const FWBCardDefinitionRepository Repository = MakeHandRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("Hand action emitted"), Action))
	{
		return false;
	}
	const FWBCardActivationTargetOption* TargetOption = FindTargetOption(*Action, HandoffEnemyUnitId);
	if (!TestNotNull(TEXT("Enemy target option"), TargetOption))
	{
		return false;
	}

	const FWBProductionActivationTargetSelectionRequest SelectionRequest =
		MakeSelectionRequest(*Action, TargetOption, TEXT("hand_instance_0"));
	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));
	const FWBCardZonePublicSummary PublicSummary = WBCardZoneObservation::BuildPublicSummary(State);

	TestTrue(TEXT("Hand execution succeeds"), Result.bOk);
	TestEqual(TEXT("Spent hand action absent after refresh"), Result.RefreshedActivationEntries.Num(), 0);
	const FString Scan = BuildResultScanText(Result);
	TestFalse(TEXT("Opponent hand secret hidden"), Scan.Contains(TEXT("SECRET_OPPONENT_HAND")));
	TestFalse(TEXT("Deck secret hidden"), Scan.Contains(TEXT("SECRET_DECK")));
	TestFalse(TEXT("Marker secret hidden"), Scan.Contains(TEXT("SECRET_MARKER")));
	TestFalse(TEXT("Public deck secret hidden"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_DECK")));
	TestFalse(TEXT("Public marker secret hidden"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_MARKER")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffNoTargetCurrentExecutionFailsSafelyTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.NoTargetCurrentExecutionFailsSafely", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffNoTargetCurrentExecutionFailsSafelyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const int32 BeforeHP = State.GetUnitById(HandoffEnemyUnitId)->HP;
	const FWBCardDefinitionRepository Repository = MakeBoardRepository(EWBCardEffectTargetRequirement::None);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("No-target action emitted"), Action))
	{
		return false;
	}

	const FWBProductionActivationTargetSelectionRequest SelectionRequest =
		MakeSelectionRequest(*Action);
	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));

	TestEqual(TEXT("Current execution fails through core path"), Result.Code, EWBProductionActivationExecutionHandoffResultCode::ExecutionFailed);
	TestEqual(TEXT("Core reason"), Result.Reason, FString(TEXT("missing_effect_target_unit")));
	TestFalse(TEXT("Execution not applied"), Result.bExecutionApplied);
	TestEqual(TEXT("No HP mutation"), State.GetUnitById(HandoffEnemyUnitId)->HP, BeforeHP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffTileUnsupportedTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.TileTargetUnsupported", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffTileUnsupportedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository(EWBCardEffectTargetRequirement::Tile);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("Tile action emitted"), Action))
	{
		return false;
	}

	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, MakeSelectionRequest(*Action)));
	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::UnsupportedTargetRequirement,
		TEXT("Tile unsupported"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffWallUnsupportedTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.WallTargetUnsupported", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffWallUnsupportedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository(EWBCardEffectTargetRequirement::WallEdge);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("Wall action emitted"), Action))
	{
		return false;
	}

	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, MakeSelectionRequest(*Action)));
	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::UnsupportedTargetRequirement,
		TEXT("Wall unsupported"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffProviderRefreshReflectsUsageTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.ProviderRefreshAfterSuccessReflectsUsage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffProviderRefreshReflectsUsageTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository(
		EWBCardEffectTargetRequirement::Unit,
		true,
		TEXT("SECRET_USAGE_KEY_DO_NOT_LEAK"));
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);

	AssertResultCode(
		*this,
		Result,
		EWBProductionActivationExecutionHandoffResultCode::Success,
		TEXT("Once per turn source"));
	TestEqual(TEXT("Refresh observes used activation"), Result.RefreshedActivationEntries.Num(), 0);
	TestFalse(TEXT("Usage key hidden"), BuildResultScanText(Result).Contains(TEXT("SECRET_USAGE_KEY")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffHiddenMetadataNotExposedTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.InternalMetadataHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffHiddenMetadataNotExposedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository(
		EWBCardEffectTargetRequirement::Unit,
		true,
		TEXT("SECRET_DEBUG_OR_USAGE_DO_NOT_LEAK"));
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);

	TestTrue(TEXT("Execution succeeds"), Result.bOk);
	const FString Scan = BuildResultScanText(Result);
	TestFalse(TEXT("No debug text"), Scan.Contains(TEXT("DebugActivationId")));
	TestFalse(TEXT("No usage text"), Scan.Contains(TEXT("UsageKey")));
	TestFalse(TEXT("No secret usage value"), Scan.Contains(TEXT("SECRET_DEBUG_OR_USAGE")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffOpponentHandHiddenAfterRefreshTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.OpponentHandHiddenAfterRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffOpponentHandHiddenAfterRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddOpponentSecretHandCard(State);
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);

	TestTrue(TEXT("Execution succeeds"), Result.bOk);
	TestFalse(TEXT("Opponent hand secret hidden"), BuildResultScanText(Result).Contains(TEXT("SECRET_OPPONENT_HAND")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffDeckHiddenAfterRefreshTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.DeckHiddenAfterRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffDeckHiddenAfterRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddSecretDeckCard(State);
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);

	TestTrue(TEXT("Execution succeeds"), Result.bOk);
	TestFalse(TEXT("Deck secret hidden"), BuildResultScanText(Result).Contains(TEXT("SECRET_DECK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffMarkerHiddenAfterRefreshTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.MarkerHiddenAfterRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffMarkerHiddenAfterRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	AddSecretMarker(State);
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);

	TestTrue(TEXT("Execution succeeds"), Result.bOk);
	TestFalse(TEXT("Marker secret hidden"), BuildResultScanText(Result).Contains(TEXT("SECRET_MARKER")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffRepositoryNotMutatedTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.DoesNotMutateRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffRepositoryNotMutatedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	FWBCardDefinitionRepository Repository = MakeBoardRepository();
	const TArray<FString> BeforeIds =
		WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository);
	const FString BeforeEffectId = Repository.Definitions[0].ActivatedEffects[0].EffectId;
	FWBProductionActivationDataProvider Provider;
	const FWBProductionActivationExecutionHandoffResult Result =
		ExecuteFirstBoardUnitTarget(State, Repository, Provider);

	TestTrue(TEXT("Execution succeeds"), Result.bOk);
	TestTrue(TEXT("Card ids unchanged"), BeforeIds == WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository));
	TestEqual(TEXT("Definition count unchanged"), Repository.Definitions.Num(), 1);
	TestEqual(TEXT("Effect id unchanged"), Repository.Definitions[0].ActivatedEffects[0].EffectId, BeforeEffectId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffFailedHandoffNoStateMutationTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.FailedHandoffNoStateMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffFailedHandoffNoStateMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const int32 BeforeHP = State.GetUnitById(HandoffEnemyUnitId)->HP;
	const FWBCardDefinitionRepository Repository = MakeHandoffRepository({});
	FWBProductionActivationDataProvider Provider;
	RunProvider(State, Repository, Provider);

	FWBProductionActivationTargetSelectionRequest SelectionRequest;
	SelectionRequest.ActivationEntryId = TEXT("activate_source:p0:zboard:u11:chandoff_board_card:eboard_burst");
	SelectionRequest.SourceCardId = TEXT("handoff_board_card");
	SelectionRequest.EffectId = TEXT("board_burst");
	const FWBProductionActivationExecutionHandoffResult Result =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));

	TestFalse(TEXT("Fails"), Result.bOk);
	TestEqual(TEXT("No HP mutation"), State.GetUnitById(HandoffEnemyUnitId)->HP, BeforeHP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffSourceGuardMutationPathTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.SourceGuardMutationPath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffSourceGuardMutationPathTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationExecutionHandoff.cpp") }, Source));
	TestFalse(TEXT("No mutable unit lookup"), Source.Contains(TEXT("GetMutableUnit")));
	TestFalse(TEXT("No direct HP field mutation"), Source.Contains(TEXT(".HP")));
	TestFalse(TEXT("No direct armor field mutation"), Source.Contains(TEXT("CurrentArmor")));
	TestFalse(TEXT("No direct statuses mutation"), Source.Contains(TEXT("Statuses")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffNoRulesGenerationSourceGuardTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.DoesNotCallRulesGenerateLegalActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffNoRulesGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationExecutionHandoff.cpp") }, Source));
	TestTrue(TEXT("Header loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationExecutionHandoff.h") }, Header));
	TestFalse(TEXT("No WBRules source"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("No WBRules header"), Header.Contains(TEXT("WBRules")));
	TestFalse(TEXT("No GenerateLegalActions source"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No GenerateLegalActions header"), Header.Contains(TEXT("GenerateLegalActions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffNoFWBActionSourceGuardTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.DoesNotCreateFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffNoFWBActionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationExecutionHandoff.cpp") }, Source));
	TestTrue(TEXT("Header loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationExecutionHandoff.h") }, Header));
	TestFalse(TEXT("No FWBAction source"), Source.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No FWBAction header"), Header.Contains(TEXT("FWBAction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffNoActionCodecSourceGuardTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.DoesNotDependOnWBActionCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffNoActionCodecSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationExecutionHandoff.cpp") }, Source));
	TestTrue(TEXT("Header loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationExecutionHandoff.h") }, Header));
	TestFalse(TEXT("No ActionCodec source"), Source.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No ActionCodec header"), Header.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffNoDirectEffectRunnerSourceGuardTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.DoesNotCallWBEffectRunnerDirectly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffNoDirectEffectRunnerSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationExecutionHandoff.cpp") }, Source));
	TestTrue(TEXT("Header loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationExecutionHandoff.h") }, Header));
	TestFalse(TEXT("No direct EffectRunner source"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No direct EffectRunner header"), Header.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No direct command apply"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffProviderRefreshNoLegalGenerationSourceGuardTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.ProviderRefreshDoesNotGenerateLegalActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffProviderRefreshNoLegalGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationExecutionHandoff.cpp") }, Source));
	TestFalse(TEXT("No GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No ActionCodec"), Source.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationExecutionHandoffRuntimeHarnessTest, "Wandbound.Runtime.ProductionActivationExecutionHandoff.RuntimeHarnessProviderSelectionHandoffRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationExecutionHandoffRuntimeHarnessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHandoffState();
	const FWBCardDefinitionRepository Repository = MakeBoardRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult ProviderResult =
		RunProvider(State, Repository, Provider);
	TestTrue(TEXT("Provider ok"), ProviderResult.bOk);
	TestEqual(TEXT("Provider action count"), ProviderResult.RefreshInput.ActivationActionSet.Actions.Num(), 1);

	const FWBCardActivationLegalAction* Action = FirstAction(ProviderResult);
	if (!TestNotNull(TEXT("Action emitted"), Action))
	{
		return false;
	}
	const FWBCardActivationTargetOption* TargetOption = FindTargetOption(*Action, HandoffEnemyUnitId);
	if (!TestNotNull(TEXT("Target option emitted"), TargetOption))
	{
		return false;
	}

	const FWBProductionActivationTargetSelectionRequest SelectionRequest =
		MakeSelectionRequest(*Action, TargetOption);
	const FWBProductionActivationExecutionHandoffResult HandoffResult =
		FWBProductionActivationExecutionHandoff().ExecuteSelectedActivation(
			MakeExecutionInput(State, Repository, Provider, SelectionRequest));

	AssertResultCode(
		*this,
		HandoffResult,
		EWBProductionActivationExecutionHandoffResultCode::Success,
		TEXT("Runtime harness"));
	TestTrue(TEXT("Refresh returned"), HandoffResult.RefreshedActivationEntries.Num() > 0);
	TestEqual(TEXT("Target bound"), HandoffResult.TargetUnitId, HandoffEnemyUnitId);
	return true;
}

#endif
