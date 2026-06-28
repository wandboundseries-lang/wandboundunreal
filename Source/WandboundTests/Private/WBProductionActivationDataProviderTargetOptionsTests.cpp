#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBProductionActivationDataProvider.h"
#include "WBRules.h"
#include "WBRuntimeActivationDataProviderAdapter.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 TargetOptionSourceUnitId = 11;
constexpr int32 TargetOptionEnemyUnitId = 22;
constexpr int32 TargetOptionFriendlyUnitId = 33;

FWBPlayerStateData MakeTargetOptionPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeTargetOptionUnit(
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

FWBGameStateData MakeTargetOptionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 40;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeTargetOptionPlayer(0));
	State.Players.Add(MakeTargetOptionPlayer(1, 0));
	State.AddUnitForTest(MakeTargetOptionUnit(
		TargetOptionSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("provider_board_card")));
	State.AddUnitForTest(MakeTargetOptionUnit(
		TargetOptionEnemyUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_enemy_card")));
	return State;
}

FWBGameStateData MakeTargetOptionStateWithoutUnits()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 40;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeTargetOptionPlayer(0));
	State.Players.Add(MakeTargetOptionPlayer(1, 0));
	return State;
}

FWBZoneCardEntry MakeTargetOptionZoneEntry(
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

void AddOwnHandCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId)
{
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = 0;
	PlayerZones.Hand.Add(MakeTargetOptionZoneEntry(InstanceId, CardId, 0, EWBCardZone::Hand, 0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(PlayerZones);
}

void AddOpponentSecretHandCard(FWBGameStateData& State)
{
	FWBPlayerCardZoneState OpponentZones;
	OpponentZones.PlayerId = 1;
	OpponentZones.Hand.Add(MakeTargetOptionZoneEntry(
		TEXT("SECRET_OPPONENT_HAND_INSTANCE_DO_NOT_LEAK"),
		TEXT("SECRET_OPPONENT_HAND_CARD_DO_NOT_LEAK"),
		1,
		EWBCardZone::Hand,
		0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(OpponentZones);
}

void AddSecretDeckCard(FWBGameStateData& State)
{
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = 0;
	PlayerZones.Deck.Add(MakeTargetOptionZoneEntry(
		TEXT("SECRET_DECK_INSTANCE_DO_NOT_LEAK"),
		TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"),
		0,
		EWBCardZone::Deck,
		0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(PlayerZones);
}

void AddSecretMarker(FWBGameStateData& State)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 77;
	Marker.OwnerPlayerId = 1;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_MARKER_DO_NOT_LEAK");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

FWBCardActivationSourceGateDefinition MakeTargetOptionSourceGate(
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

FWBGenericEffectPayload MakeTargetOptionPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = 1;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("production_target_option_test"));
	return Payload;
}

FWBCardEffectDefinition MakeTargetOptionEffect(
	const FString& EffectId,
	const FString& Label,
	const EWBCardActivationSourceZone SourceZone,
	const EWBCardEffectTargetRequirement TargetRequirement)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = Label;
	Effect.TargetRequirement = TargetRequirement;
	Effect.SourceGate = MakeTargetOptionSourceGate(SourceZone);
	Effect.Payloads.Add(MakeTargetOptionPayload());
	return Effect;
}

FWBCardDefinition MakeTargetOptionDefinition(
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
	Definition.ActivatedEffects.Add(MakeTargetOptionEffect(
		EffectId,
		Label,
		SourceZone,
		TargetRequirement));
	return Definition;
}

FWBCardDefinitionRepository MakeTargetOptionRepository(
	const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("production_target_option_repo");
	Repository.SourceVersion = TEXT("target_option_test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBProductionActivationDataProviderInput MakeTargetOptionInput(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	FWBProductionActivationDataProviderInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 0;
	return Input;
}

FWBRuntimeActivationDataProviderRequest MakeTargetOptionRequest()
{
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint;
	Request.PlayerId = 0;
	Request.DecisionPointId = TEXT("production_target_option_decision");
	Request.RequestReason = TEXT("production_target_option_test");
	return Request;
}

FWBRuntimeActivationDataProviderResult RunTargetOptionProvider(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBProductionActivationDataProvider& Provider)
{
	Provider.Configure(MakeTargetOptionInput(State, Repository), FWBProductionActivationDataProviderConfig());
	return Provider.GetActivationDecisionData(MakeTargetOptionRequest());
}

bool HasTargetOptionDiagnostic(
	const FWBProductionActivationDataProvider& Provider,
	const FString& Code)
{
	for (const FWBProductionActivationDataProviderDiagnostic& Diagnostic : Provider.GetDiagnosticsForTest())
	{
		if (Diagnostic.Code == Code)
		{
			return true;
		}
	}
	return false;
}

const FWBCardActivationLegalAction* FirstTargetOptionAction(
	const FWBRuntimeActivationDataProviderResult& Result)
{
	return Result.RefreshInput.ActivationActionSet.Actions.Num() > 0
		? &Result.RefreshInput.ActivationActionSet.Actions[0]
		: nullptr;
}

bool HasTargetUnitOption(
	const FWBCardActivationLegalAction& Action,
	const int32 UnitId)
{
	for (const FWBCardActivationTargetOption& Option : Action.TargetOptions)
	{
		if (Option.TargetUnitId == UnitId)
		{
			return true;
		}
	}
	return false;
}

TArray<int32> TargetOptionUnitIds(const FWBCardActivationLegalAction& Action)
{
	TArray<int32> UnitIds;
	for (const FWBCardActivationTargetOption& Option : Action.TargetOptions)
	{
		UnitIds.Add(Option.TargetUnitId);
	}
	return UnitIds;
}

FString BuildTargetOptionOutputScanText(
	const FWBRuntimeActivationDataProviderResult& Result,
	const FWBProductionActivationDataProvider& Provider)
{
	FString Text;
	Text += Result.Reason;
	for (const FWBCardActivationLegalAction& Action : Result.RefreshInput.ActivationActionSet.Actions)
	{
		Text += Action.ActivationActionId;
		Text += Action.PublicLabel;
		Text += Action.Candidate.SourceCardId;
		Text += Action.Candidate.SourceEffectId;
		Text += Action.Command.Source.SourceCardId;
		Text += Action.Command.Source.SourceEffectId;
		for (const FWBCardActivationTargetOption& Option : Action.TargetOptions)
		{
			Text += FString::FromInt(Option.TargetUnitId);
			Text += FString::FromInt(Option.TargetOwnerPlayerId);
			Text += FString::FromInt(Option.TargetTile.X);
			Text += FString::FromInt(Option.TargetTile.Y);
			Text += Option.TargetCardId;
			Text += Option.PublicLabel;
		}
	}

	for (const FWBProductionActivationDataProviderDiagnostic& Diagnostic : Provider.GetDiagnosticsForTest())
	{
		Text += Diagnostic.Code;
		Text += Diagnostic.CardId;
		Text += Diagnostic.InstanceId;
		Text += FString::FromInt(Diagnostic.UnitId);
	}

	for (const FWBPublicUnitBoardSummary& Unit : Result.RefreshInput.PublicBoardSummary.Units)
	{
		Text += Unit.CardId;
	}
	return Text;
}

FWBCardDefinitionRepository MakeBoardUnitTargetRepository()
{
	return MakeTargetOptionRepository({
		MakeTargetOptionDefinition(
			TEXT("provider_board_card"),
			TEXT("Provider Board Card"),
			EWBCardActivationSourceZone::Board,
			TEXT("unit_target"),
			TEXT("Choose Unit Burst"),
			EWBCardEffectTargetRequirement::Unit)
	});
}

FWBCardDefinitionRepository MakeHandUnitTargetRepository()
{
	return MakeTargetOptionRepository({
		MakeTargetOptionDefinition(
			TEXT("provider_hand_card"),
			TEXT("Provider Hand Card"),
			EWBCardActivationSourceZone::Hand,
			TEXT("hand_unit_target"),
			TEXT("Hand Unit Burst"),
			EWBCardEffectTargetRequirement::Unit)
	});
}

bool LoadTargetOptionSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(OutSource, *Path);
}

UWBRuntimeActivationDecisionSessionFacadeComponent* MakeTargetOptionFacade()
{
	return NewObject<UWBRuntimeActivationDecisionSessionFacadeComponent>(GetTransientPackage());
}

bool MakeTargetOptionSessionPath(
	UWBRuntimeActivationDecisionSessionFacadeComponent*& OutFacade,
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel)
{
	OutFacade = MakeTargetOptionFacade();
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

TArray<FString> NormalActionIdsForTargetOptionState(const FWBGameStateData& State)
{
	return WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsBoardSourceTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.BoardSourceUnitTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsBoardSourceTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	const FWBCardDefinitionRepository Repository = MakeBoardUnitTargetRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunTargetOptionProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestNotNull(TEXT("Action emitted"), Action);
	TestEqual(TEXT("Target option count"), Action != nullptr ? Action->TargetOptions.Num() : -1, 2);
	TestTrue(TEXT("Source target available"), Action != nullptr && HasTargetUnitOption(*Action, TargetOptionSourceUnitId));
	TestTrue(TEXT("Enemy target available"), Action != nullptr && HasTargetUnitOption(*Action, TargetOptionEnemyUnitId));
	TestFalse(TEXT("Unit target not deferred"), HasTargetOptionDiagnostic(Provider, TEXT("target_options_deferred")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsOwnHandTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.OwnHandUnitTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsOwnHandTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	AddOwnHandCard(State, TEXT("hand_instance_0"), TEXT("provider_hand_card"));
	const FWBCardDefinitionRepository Repository = MakeHandUnitTargetRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunTargetOptionProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestNotNull(TEXT("Action emitted"), Action);
	TestEqual(TEXT("Hand source action"), Action != nullptr ? Action->SourceUnitId : -2, -1);
	TestEqual(TEXT("Target option count"), Action != nullptr ? Action->TargetOptions.Num() : -1, 2);
	TestTrue(TEXT("Enemy target available"), Action != nullptr && HasTargetUnitOption(*Action, TargetOptionEnemyUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsNoneTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.NoTargetHasNoOptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsNoneTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	const FWBCardDefinitionRepository Repository = MakeTargetOptionRepository({
		MakeTargetOptionDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("none_target"), TEXT("Activate Burst"), EWBCardEffectTargetRequirement::None)
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunTargetOptionProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestNotNull(TEXT("Action emitted"), Action);
	TestEqual(TEXT("No options"), Action != nullptr ? Action->TargetOptions.Num() : -1, 0);
	TestFalse(TEXT("No deferred diagnostic"), HasTargetOptionDiagnostic(Provider, TEXT("target_options_deferred")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsTileDeferredTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.TileTargetDeferred", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsTileDeferredTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	const FWBCardDefinitionRepository Repository = MakeTargetOptionRepository({
		MakeTargetOptionDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("tile_target"), TEXT("Choose Tile Burst"), EWBCardEffectTargetRequirement::Tile)
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunTargetOptionProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("No options"), Action != nullptr ? Action->TargetOptions.Num() : -1, 0);
	TestTrue(TEXT("Deferred diagnostic"), HasTargetOptionDiagnostic(Provider, TEXT("target_options_deferred")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsWallDeferredTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.WallTargetDeferred", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsWallDeferredTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	const FWBCardDefinitionRepository Repository = MakeTargetOptionRepository({
		MakeTargetOptionDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("wall_target"), TEXT("Choose Wall Burst"), EWBCardEffectTargetRequirement::WallEdge)
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunTargetOptionProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("No options"), Action != nullptr ? Action->TargetOptions.Num() : -1, 0);
	TestTrue(TEXT("Deferred diagnostic"), HasTargetOptionDiagnostic(Provider, TEXT("target_options_deferred")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsUnsupportedTargetTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.UnsupportedTargetRequirement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsUnsupportedTargetTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	const FWBCardDefinitionRepository Repository = MakeTargetOptionRepository({
		MakeTargetOptionDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("unknown_target"), TEXT("Choose Target Burst"), static_cast<EWBCardEffectTargetRequirement>(255))
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunTargetOptionProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("No options"), Action != nullptr ? Action->TargetOptions.Num() : -1, 0);
	TestTrue(TEXT("Unsupported diagnostic"), HasTargetOptionDiagnostic(Provider, TEXT("unsupported_target_requirement")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsRemovedDefeatedExcludedTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.RemovedDefeatedUnitsExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsRemovedDefeatedExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	State.AddUnitForTest(MakeTargetOptionUnit(44, 1, FWBTile(1, 1), TEXT("removed_enemy_card")));
	State.AddUnitForTest(MakeTargetOptionUnit(55, 1, FWBTile(2, 2), TEXT("defeated_enemy_card")));
	State.GetMutableUnitById(44)->RemoveUnitFromBoard();
	State.GetMutableUnitById(55)->MarkUnitDefeated();
	const FWBCardDefinitionRepository Repository = MakeBoardUnitTargetRepository();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunTargetOptionProvider(State, Repository, Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestFalse(TEXT("Removed excluded"), Action != nullptr && HasTargetUnitOption(*Action, 44));
	TestFalse(TEXT("Defeated excluded"), Action != nullptr && HasTargetUnitOption(*Action, 55));
	TestTrue(TEXT("Still has enemy"), Action != nullptr && HasTargetUnitOption(*Action, TargetOptionEnemyUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsEnemyIncludedTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.GenericUnitIncludesEnemy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsEnemyIncludedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Enemy included"), Action != nullptr && HasTargetUnitOption(*Action, TargetOptionEnemyUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsFriendlyIncludedTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.GenericUnitIncludesFriendly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsFriendlyIncludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	State.AddUnitForTest(MakeTargetOptionUnit(TargetOptionFriendlyUnitId, 0, FWBTile(2, 4), TEXT("visible_friendly_card")));
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Friendly included"), Action != nullptr && HasTargetUnitOption(*Action, TargetOptionFriendlyUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsSelfIncludedTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.GenericUnitIncludesSelf", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsSelfIncludedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Self included"), Action != nullptr && HasTargetUnitOption(*Action, TargetOptionSourceUnitId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsOrderingTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.DeterministicTargetOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	State.AddUnitForTest(MakeTargetOptionUnit(44, 1, FWBTile(5, 0), TEXT("visible_enemy_early")));
	State.AddUnitForTest(MakeTargetOptionUnit(TargetOptionFriendlyUnitId, 0, FWBTile(1, 1), TEXT("visible_friendly_card")));
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);
	const TArray<int32> UnitIds = Action != nullptr ? TargetOptionUnitIds(*Action) : TArray<int32>();

	TestEqual(TEXT("Target count"), UnitIds.Num(), 4);
	TestEqual(TEXT("First owner 0 y1"), UnitIds.Num() > 0 ? UnitIds[0] : -1, TargetOptionFriendlyUnitId);
	TestEqual(TEXT("Second source owner 0 y4"), UnitIds.Num() > 1 ? UnitIds[1] : -1, TargetOptionSourceUnitId);
	TestEqual(TEXT("Third owner 1 y0"), UnitIds.Num() > 2 ? UnitIds[2] : -1, 44);
	TestEqual(TEXT("Fourth owner 1 y4"), UnitIds.Num() > 3 ? UnitIds[3] : -1, TargetOptionEnemyUnitId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsOpponentHandHiddenTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.OpponentHandHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsOpponentHandHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	AddOpponentSecretHandCard(State);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);

	TestFalse(TEXT("Opponent hand secret absent"), BuildTargetOptionOutputScanText(Result, Provider).Contains(TEXT("SECRET_OPPONENT_HAND")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsDeckHiddenTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.DeckIdentityHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsDeckHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	AddSecretDeckCard(State);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);

	TestFalse(TEXT("Deck secret absent"), BuildTargetOptionOutputScanText(Result, Provider).Contains(TEXT("SECRET_DECK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsMarkerHiddenTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.HiddenMarkerIdentityHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsMarkerHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	AddSecretMarker(State);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);

	TestFalse(TEXT("Marker secret absent"), BuildTargetOptionOutputScanText(Result, Provider).Contains(TEXT("SECRET_MARKER")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsVisibleBoardOnlyTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.VisibleBoardCardOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsVisibleBoardOnlyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	AddSecretDeckCard(State);
	AddOpponentSecretHandCard(State);
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);
	const FString ScanText = BuildTargetOptionOutputScanText(Result, Provider);

	TestTrue(TEXT("Visible board card appears"), ScanText.Contains(TEXT("visible_enemy_card")));
	TestFalse(TEXT("Hidden hand absent"), ScanText.Contains(TEXT("SECRET_OPPONENT_HAND")));
	TestFalse(TEXT("Hidden deck absent"), ScanText.Contains(TEXT("SECRET_DECK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsNoUnitsAvailableTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.NoUnitTargetsAvailable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsNoUnitsAvailableTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionStateWithoutUnits();
	AddOwnHandCard(State, TEXT("hand_instance_0"), TEXT("provider_hand_card"));
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		RunTargetOptionProvider(State, MakeHandUnitTargetRepository(), Provider);
	const FWBCardActivationLegalAction* Action = FirstTargetOptionAction(Result);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestNotNull(TEXT("Hand action emitted"), Action);
	TestEqual(TEXT("No target options"), Action != nullptr ? Action->TargetOptions.Num() : -1, 0);
	TestTrue(TEXT("No unit diagnostic"), HasTargetOptionDiagnostic(Provider, TEXT("no_unit_targets_available")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsNoGameStateMutationTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.DoesNotMutateGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsNoGameStateMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	const FWBGameStateData Before = State;
	FWBProductionActivationDataProvider Provider;
	RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);

	TestEqual(TEXT("Unit count unchanged"), State.Units.Num(), Before.Units.Num());
	TestEqual(TEXT("Source HP unchanged"), State.GetUnitById(TargetOptionSourceUnitId)->HP, Before.GetUnitById(TargetOptionSourceUnitId)->HP);
	TestEqual(TEXT("Enemy HP unchanged"), State.GetUnitById(TargetOptionEnemyUnitId)->HP, Before.GetUnitById(TargetOptionEnemyUnitId)->HP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsNoRepositoryMutationTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.DoesNotMutateRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsNoRepositoryMutationTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	FWBCardDefinitionRepository Repository = MakeBoardUnitTargetRepository();
	const TArray<FString> BeforeIds = WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository);
	FWBProductionActivationDataProvider Provider;
	RunTargetOptionProvider(State, Repository, Provider);

	TestTrue(TEXT("Card ids unchanged"), BeforeIds == WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository));
	TestEqual(TEXT("Effect id unchanged"), Repository.Definitions[0].ActivatedEffects[0].EffectId, FString(TEXT("unit_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsRuntimeSessionTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.RuntimeSessionReceivesExternalTargetOptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsRuntimeSessionTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeTargetOptionState();
	const FWBCardDefinitionRepository Repository = MakeBoardUnitTargetRepository();
	FWBProductionActivationDataProvider Provider;
	Provider.Configure(MakeTargetOptionInput(State, Repository), FWBProductionActivationDataProviderConfig());

	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeTargetOptionSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeTargetOptionRequest());

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestEqual(TEXT("Action count"), ActivationModel->GetCurrentActivationActionSet().Actions.Num(), 1);
	TestEqual(TEXT("Target options carried"), ActivationModel->GetCurrentActivationActionSet().Actions[0].TargetOptions.Num(), 2);
	TestEqual(TEXT("Target presentation sees unit choice"), ActivationModel->GetCurrentTargetPresentationSnapshot().Entries[0].TargetKind, EWBCardActivationTargetPresentationKind::Unit);
	TestEqual(TEXT("Target option count presented"), ActivationModel->GetCurrentTargetPresentationSnapshot().Entries[0].TargetOptionCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsSessionNoComputeTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.RuntimeSessionDoesNotComputeTargetOptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsSessionNoComputeTest::RunTest(const FString& Parameters)
{
	for (const FString& RelativePath : {
		FString(TEXT("Source/WandboundRuntime/Private/WBRuntimeActivationDecisionSessionFacadeComponent.cpp")),
		FString(TEXT("Source/WandboundRuntime/Private/WBRuntimeDecisionPointOwnerComponent.cpp")),
		FString(TEXT("Source/WandboundRuntime/Private/WBRuntimeDecisionPointCoordinatorComponent.cpp")),
		FString(TEXT("Source/WandboundRuntime/Private/WBRuntimeActivationPresentationModelComponent.cpp")) })
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("%s loads"), *RelativePath), LoadTargetOptionSourceFile({ RelativePath }, Source));
		TestFalse(*FString::Printf(TEXT("%s does not build target options"), *RelativePath), Source.Contains(TEXT("BuildUnitTargetOptions")));
		TestFalse(*FString::Printf(TEXT("%s does not own target diagnostics"), *RelativePath), Source.Contains(TEXT("no_unit_targets_available")));
		TestFalse(*FString::Printf(TEXT("%s does not build public summary"), *RelativePath), Source.Contains(TEXT("WBPublicBoardSummary::Build")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsRulesUnchangedTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.WBRulesLegalActionsUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsRulesUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetOptionState();
	const TArray<FString> BeforeIds = NormalActionIdsForTargetOptionState(State);
	FWBProductionActivationDataProvider Provider;
	RunTargetOptionProvider(State, MakeBoardUnitTargetRepository(), Provider);
	const TArray<FString> AfterIds = NormalActionIdsForTargetOptionState(State);

	TestTrue(TEXT("Normal legal ids unchanged"), BeforeIds == AfterIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(TEXT("No activation normal action"), ActionId.StartsWith(TEXT("activate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsActionCodecUnchangedTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.WBActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	FWBAction Move;
	Move.Type = EWBActionType::Move;
	Move.PlayerId = 0;
	Move.SourceUnitId = TargetOptionSourceUnitId;
	Move.FromTile = FWBTile(4, 4);
	Move.ToTile = FWBTile(5, 4);

	TestEqual(TEXT("Move id unchanged"), WBActionCodec::MakeActionId(Move), FString(TEXT("move:p0:u11:4,4->5,4")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsNoEffectExecutionSourceGuardTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.ProviderDoesNotExecuteEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsNoEffectExecutionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Provider source loads"), LoadTargetOptionSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationDataProvider.cpp") }, Source));
	TestFalse(TEXT("No EffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No effect execution"), Source.Contains(TEXT("ApplyEffect")));
	TestFalse(TEXT("No damage execution"), Source.Contains(TEXT("ApplyDamageEffect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationTargetOptionsNoFWBActionSourceGuardTest, "Wandbound.Runtime.ProductionActivationDataProviderTargetOptions.ProviderDoesNotCreateFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationTargetOptionsNoFWBActionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Provider source loads"), LoadTargetOptionSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationDataProvider.cpp") }, Source));
	TestTrue(TEXT("Provider header loads"), LoadTargetOptionSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationDataProvider.h") }, Header));
	TestFalse(TEXT("Source does not mention FWBAction"), Source.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("Header does not mention FWBAction"), Header.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No WBRules generation"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No ActionCodec dependency"), Source.Contains(TEXT("WBActionCodec")));
	return true;
}

#endif
