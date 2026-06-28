#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardDefinitionFixtureLoader.h"
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
constexpr int32 ProductionProviderSourceUnitId = 11;
constexpr int32 ProductionProviderTargetUnitId = 22;

FWBPlayerStateData MakeProductionProviderPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeProductionProviderUnit(
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

FWBZoneCardEntry MakeProductionProviderZoneEntry(
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

FWBGameStateData MakeProductionProviderState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 30;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeProductionProviderPlayer(0));
	State.Players.Add(MakeProductionProviderPlayer(1, 0));
	State.AddUnitForTest(MakeProductionProviderUnit(
		ProductionProviderSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("provider_board_card")));
	State.AddUnitForTest(MakeProductionProviderUnit(
		ProductionProviderTargetUnitId,
		1,
		FWBTile(6, 4),
		TEXT("provider_target_card")));
	return State;
}

FWBCardActivationSourceGateDefinition MakeProductionProviderSourceGate(
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

FWBGenericEffectPayload MakeProductionProviderPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = 1;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("production_provider_test"));
	return Payload;
}

FWBCardEffectDefinition MakeProductionProviderEffect(
	const FString& EffectId,
	const FString& Label,
	const EWBCardActivationSourceZone SourceZone,
	const EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::None)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = Label;
	Effect.TargetRequirement = TargetRequirement;
	Effect.SourceGate = MakeProductionProviderSourceGate(SourceZone);
	Effect.Payloads.Add(MakeProductionProviderPayload());
	return Effect;
}

FWBCardDefinition MakeProductionProviderDefinition(
	const FString& CardId,
	const FString& PublicName,
	const EWBCardActivationSourceZone SourceZone,
	const FString& EffectId,
	const FString& Label,
	const EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::None)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeProductionProviderEffect(
		EffectId,
		Label,
		SourceZone,
		TargetRequirement));
	return Definition;
}

FWBCardDefinitionRepository MakeProductionProviderRepository(
	const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("production_provider_repo");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBProductionActivationDataProviderInput MakeProductionProviderInput(
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

FWBRuntimeActivationDataProviderRequest MakeProductionProviderRequest()
{
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint;
	Request.PlayerId = 0;
	Request.DecisionPointId = TEXT("production_provider_decision");
	Request.RequestReason = TEXT("production_provider_test");
	return Request;
}

FWBRuntimeActivationDataProviderResult RunProductionProvider(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBProductionActivationDataProvider& Provider,
	const FWBProductionActivationDataProviderConfig& Config = FWBProductionActivationDataProviderConfig())
{
	Provider.Configure(MakeProductionProviderInput(State, Repository), Config);
	return Provider.GetActivationDecisionData(MakeProductionProviderRequest());
}

bool HasProductionProviderDiagnostic(
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

FString BuildProductionProviderOutputScanText(
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

	for (const FWBProductionActivationDataProviderDiagnostic& Diagnostic : Provider.GetDiagnosticsForTest())
	{
		Text += Diagnostic.Code;
		Text += Diagnostic.CardId;
		Text += Diagnostic.InstanceId;
	}

	for (const FWBPublicUnitBoardSummary& Unit : Result.RefreshInput.PublicBoardSummary.Units)
	{
		Text += Unit.CardId;
	}
	return Text;
}

TArray<FString> ProductionProviderActivationIds(const FWBRuntimeActivationDataProviderResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationLegalAction& Action : Result.RefreshInput.ActivationActionSet.Actions)
	{
		Ids.Add(Action.ActivationActionId);
	}
	return Ids;
}

TArray<FString> ProductionProviderNormalActionIds(const FWBGameStateData& State)
{
	return WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
}

bool LoadProductionProviderSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(OutSource, *Path);
}

UWBRuntimeActivationDecisionSessionFacadeComponent* MakeProductionProviderFacade()
{
	return NewObject<UWBRuntimeActivationDecisionSessionFacadeComponent>(GetTransientPackage());
}

bool MakeProductionProviderSessionPath(
	UWBRuntimeActivationDecisionSessionFacadeComponent*& OutFacade,
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel)
{
	OutFacade = MakeProductionProviderFacade();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderUnconfiguredTest, "Wandbound.Runtime.ProductionActivationDataProvider.RequiresConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderUnconfiguredTest::RunTest(const FString& Parameters)
{
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(MakeProductionProviderRequest());
	TestFalse(TEXT("Provider fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("provider_not_configured")));
	TestEqual(TEXT("Empty data"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 0);
	TestTrue(TEXT("Diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("provider_not_configured")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderMissingGameStateTest, "Wandbound.Runtime.ProductionActivationDataProvider.MissingGameStateFailsClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderMissingGameStateTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({});
	FWBProductionActivationDataProvider Provider;
	FWBProductionActivationDataProviderInput Input;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 0;
	Provider.Configure(Input, FWBProductionActivationDataProviderConfig());

	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(MakeProductionProviderRequest());
	TestFalse(TEXT("Provider fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("game_state_missing")));
	TestTrue(TEXT("Diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("game_state_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderMissingRepositoryTest, "Wandbound.Runtime.ProductionActivationDataProvider.MissingRepositoryFailsClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderMissingRepositoryTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	FWBProductionActivationDataProvider Provider;
	FWBProductionActivationDataProviderInput Input;
	Input.GameState = &State;
	Input.ViewerPlayerId = 0;
	Provider.Configure(Input, FWBProductionActivationDataProviderConfig());

	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(MakeProductionProviderRequest());
	TestFalse(TEXT("Provider fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("repository_missing")));
	TestTrue(TEXT("Diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("repository_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderInvalidViewerTest, "Wandbound.Runtime.ProductionActivationDataProvider.InvalidViewerFailsClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderInvalidViewerTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({});
	FWBProductionActivationDataProvider Provider;
	Provider.Configure(MakeProductionProviderInput(State, Repository, 7), FWBProductionActivationDataProviderConfig());

	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(MakeProductionProviderRequest());
	TestFalse(TEXT("Provider fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("invalid_viewer_player")));
	TestTrue(TEXT("Diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("invalid_viewer_player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderBoardSourceExposedTest, "Wandbound.Runtime.ProductionActivationDataProvider.BoardSourceActivationExposed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderBoardSourceExposedTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("board_burst"), TEXT("Board Burst"))
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("One activation"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 1);
	TestEqual(TEXT("Board action id"), Result.RefreshInput.ActivationActionSet.Actions[0].ActivationActionId, FString(TEXT("activate_source:p0:zboard:u11:cprovider_board_card:eboard_burst")));
	TestEqual(TEXT("Source unit"), Result.RefreshInput.ActivationActionSet.Actions[0].SourceUnitId, ProductionProviderSourceUnitId);
	TestEqual(TEXT("Label"), Result.RefreshInput.ActivationActionSet.Actions[0].PublicLabel, FString(TEXT("Board Burst")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderMissingDefinitionTest, "Wandbound.Runtime.ProductionActivationDataProvider.BoardSourceMissingDefinitionIgnored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderMissingDefinitionTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestTrue(TEXT("Provider still ok"), Result.bOk);
	TestEqual(TEXT("No activation"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 0);
	TestTrue(TEXT("Missing definition diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("card_definition_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderBoardUnsupportedZoneTest, "Wandbound.Runtime.ProductionActivationDataProvider.BoardSourceUnsupportedZoneIgnored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderBoardUnsupportedZoneTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Hand, TEXT("hand_only"), TEXT("Hand Only"))
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("No activation"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 0);
	TestTrue(TEXT("Unsupported source diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("unsupported_source_zone")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderOwnHandExposedTest, "Wandbound.Runtime.ProductionActivationDataProvider.OwnHandActivationExposed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderOwnHandExposedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProductionProviderState();
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = 0;
	PlayerZones.Hand.Add(MakeProductionProviderZoneEntry(TEXT("hand_instance_0"), TEXT("provider_hand_card"), 0, EWBCardZone::Hand, 0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(PlayerZones);
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_hand_card"), TEXT("Provider Hand Card"), EWBCardActivationSourceZone::Hand, TEXT("hand_burst"), TEXT("Hand Burst"))
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("One activation"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 1);
	TestEqual(TEXT("Hand action id"), Result.RefreshInput.ActivationActionSet.Actions[0].ActivationActionId, FString(TEXT("activate_source:p0:zhand:ihand_instance_0:cprovider_hand_card:ehand_burst")));
	TestEqual(TEXT("No source unit for hand"), Result.RefreshInput.ActivationActionSet.Actions[0].SourceUnitId, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderOpponentHandHiddenTest, "Wandbound.Runtime.ProductionActivationDataProvider.OpponentHandHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderOpponentHandHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProductionProviderState();
	FWBPlayerCardZoneState OpponentZones;
	OpponentZones.PlayerId = 1;
	OpponentZones.Hand.Add(MakeProductionProviderZoneEntry(TEXT("SECRET_OPPONENT_INSTANCE_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_CARD_DO_NOT_LEAK"), 1, EWBCardZone::Hand, 0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(OpponentZones);
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestFalse(TEXT("No opponent secret in output"), BuildProductionProviderOutputScanText(Result, Provider).Contains(TEXT("SECRET")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderDeckHiddenTest, "Wandbound.Runtime.ProductionActivationDataProvider.DeckIdentityHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderDeckHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProductionProviderState();
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = 0;
	PlayerZones.Deck.Add(MakeProductionProviderZoneEntry(TEXT("SECRET_DECK_INSTANCE_DO_NOT_LEAK"), TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"), 0, EWBCardZone::Deck, 0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(PlayerZones);
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestFalse(TEXT("No deck secret in output"), BuildProductionProviderOutputScanText(Result, Provider).Contains(TEXT("SECRET")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderMarkerHiddenTest, "Wandbound.Runtime.ProductionActivationDataProvider.HiddenMarkerIdentityHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderMarkerHiddenTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProductionProviderState();
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 101;
	Marker.OwnerPlayerId = 1;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_MARKER_DO_NOT_LEAK");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestFalse(TEXT("No marker secret in output"), BuildProductionProviderOutputScanText(Result, Provider).Contains(TEXT("SECRET")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderPublicLabelGuardTest, "Wandbound.Runtime.ProductionActivationDataProvider.PublicLabelRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderPublicLabelGuardTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("bad_label"), TEXT("damage_effect"))
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("No unsafe activation"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 0);
	TestTrue(TEXT("Diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("public_label_rejected")));
	TestFalse(TEXT("Unsafe label excluded"), BuildProductionProviderOutputScanText(Result, Provider).Contains(TEXT("damage_effect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderDeterministicOrderingTest, "Wandbound.Runtime.ProductionActivationDataProvider.DeterministicOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderDeterministicOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProductionProviderState();
	State.AddUnitForTest(MakeProductionProviderUnit(33, 0, FWBTile(1, 1), TEXT("provider_board_alpha")));
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = 0;
	PlayerZones.Hand.Add(MakeProductionProviderZoneEntry(TEXT("hand_z"), TEXT("provider_hand_zeta"), 0, EWBCardZone::Hand, 2));
	PlayerZones.Hand.Add(MakeProductionProviderZoneEntry(TEXT("hand_a"), TEXT("provider_hand_alpha"), 0, EWBCardZone::Hand, 0));
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(PlayerZones);

	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_hand_zeta"), TEXT("Provider Hand Zeta"), EWBCardActivationSourceZone::Hand, TEXT("zeta"), TEXT("Hand Zeta")),
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("board_b"), TEXT("Board B")),
		MakeProductionProviderDefinition(TEXT("provider_hand_alpha"), TEXT("Provider Hand Alpha"), EWBCardActivationSourceZone::Hand, TEXT("alpha"), TEXT("Hand Alpha")),
		MakeProductionProviderDefinition(TEXT("provider_board_alpha"), TEXT("Provider Board Alpha"), EWBCardActivationSourceZone::Board, TEXT("board_a"), TEXT("Board A"))
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);
	const TArray<FString> Ids = ProductionProviderActivationIds(Result);

	TestEqual(TEXT("Action count"), Ids.Num(), 4);
	TestEqual(TEXT("First board by unit id"), Ids[0], FString(TEXT("activate_source:p0:zboard:u11:cprovider_board_card:eboard_b")));
	TestEqual(TEXT("Second board"), Ids[1], FString(TEXT("activate_source:p0:zboard:u33:cprovider_board_alpha:eboard_a")));
	TestEqual(TEXT("First hand by zone index"), Ids[2], FString(TEXT("activate_source:p0:zhand:ihand_a:cprovider_hand_alpha:ealpha")));
	TestEqual(TEXT("Second hand"), Ids[3], FString(TEXT("activate_source:p0:zhand:ihand_z:cprovider_hand_zeta:ezeta")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderTargetDeferredTest, "Wandbound.Runtime.ProductionActivationDataProvider.TargetOptionsDeferredSafely", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderTargetDeferredTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("tile_target"), TEXT("Choose Tile Burst"), EWBCardEffectTargetRequirement::Tile)
	});
	FWBProductionActivationDataProvider Provider;
	const FWBRuntimeActivationDataProviderResult Result = RunProductionProvider(State, Repository, Provider);

	TestTrue(TEXT("Provider ok"), Result.bOk);
	TestEqual(TEXT("Action emitted"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 1);
	TestEqual(TEXT("No target unit"), Result.RefreshInput.ActivationActionSet.Actions[0].Target.TargetUnitId, -1);
	TestEqual(TEXT("No target options"), Result.RefreshInput.ActivationActionSet.Actions[0].TargetOptions.Num(), 0);
	TestTrue(TEXT("Deferred diagnostic"), HasProductionProviderDiagnostic(Provider, TEXT("target_options_deferred")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderNoGameStateMutationTest, "Wandbound.Runtime.ProductionActivationDataProvider.DoesNotMutateGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderNoGameStateMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProductionProviderState();
	const FWBGameStateData Before = State;
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("board_burst"), TEXT("Board Burst"))
	});
	FWBProductionActivationDataProvider Provider;
	RunProductionProvider(State, Repository, Provider);

	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Unit count unchanged"), State.Units.Num(), Before.Units.Num());
	TestEqual(TEXT("Unit HP unchanged"), State.GetUnitById(ProductionProviderSourceUnitId)->HP, Before.GetUnitById(ProductionProviderSourceUnitId)->HP);
	TestEqual(TEXT("Zone player count unchanged"), State.GetCardZoneState().PlayerZones.Num(), Before.GetCardZoneState().PlayerZones.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderNoRepositoryMutationTest, "Wandbound.Runtime.ProductionActivationDataProvider.DoesNotMutateRepository", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderNoRepositoryMutationTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("board_burst"), TEXT("Board Burst"))
	});
	const TArray<FString> BeforeIds = WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository);
	FWBProductionActivationDataProvider Provider;
	RunProductionProvider(State, Repository, Provider);

	TestTrue(TEXT("Card ids unchanged"), BeforeIds == WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository));
	TestEqual(TEXT("Definition count unchanged"), Repository.Definitions.Num(), 1);
	TestEqual(TEXT("Effect id unchanged"), Repository.Definitions[0].ActivatedEffects[0].EffectId, FString(TEXT("board_burst")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderSessionAcceptsProviderTest, "Wandbound.Runtime.ProductionActivationDataProvider.RuntimeSessionAcceptsProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderSessionAcceptsProviderTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeProductionProviderState();
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("board_burst"), TEXT("Board Burst"))
	});
	FWBProductionActivationDataProvider Provider;
	Provider.Configure(MakeProductionProviderInput(State, Repository), FWBProductionActivationDataProviderConfig());

	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeProductionProviderSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeProductionProviderRequest());

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestEqual(TEXT("Session sees activation"), Result.SessionRefreshResult.Status.ActivationActionCount, 1);
	TestTrue(TEXT("Activation model has state"), ActivationModel->HasCurrentActivationPresentationState());
	TestEqual(TEXT("Model action count"), ActivationModel->GetCurrentActivationActionSet().Actions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderLegalActionsUnchangedTest, "Wandbound.Runtime.ProductionActivationDataProvider.WBRulesLegalActionsUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderLegalActionsUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProductionProviderState();
	const TArray<FString> BeforeIds = ProductionProviderNormalActionIds(State);
	const FWBCardDefinitionRepository Repository = MakeProductionProviderRepository({
		MakeProductionProviderDefinition(TEXT("provider_board_card"), TEXT("Provider Board Card"), EWBCardActivationSourceZone::Board, TEXT("board_burst"), TEXT("Board Burst"))
	});
	FWBProductionActivationDataProvider Provider;
	RunProductionProvider(State, Repository, Provider);
	const TArray<FString> AfterIds = ProductionProviderNormalActionIds(State);

	TestTrue(TEXT("Normal legal ids unchanged"), BeforeIds == AfterIds);
	for (const FString& Id : AfterIds)
	{
		TestFalse(TEXT("No activation FWBAction id"), Id.StartsWith(TEXT("activate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderActionCodecUnchangedTest, "Wandbound.Runtime.ProductionActivationDataProvider.WBActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	FWBAction Move;
	Move.Type = EWBActionType::Move;
	Move.PlayerId = 0;
	Move.SourceUnitId = 1;
	Move.FromTile = FWBTile(4, 4);
	Move.ToTile = FWBTile(5, 4);

	FWBAction EndTurn;
	EndTurn.Type = EWBActionType::EndTurn;
	EndTurn.PlayerId = 0;

	TestEqual(TEXT("Move id unchanged"), WBActionCodec::MakeActionId(Move), FString(TEXT("move:p0:u1:4,4->5,4")));
	TestEqual(TEXT("End turn id unchanged"), WBActionCodec::MakeActionId(EndTurn), FString(TEXT("end_turn:p0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderNoEffectExecutionSourceGuardTest, "Wandbound.Runtime.ProductionActivationDataProvider.DoesNotExecuteEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderNoEffectExecutionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Source loads"), LoadProductionProviderSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationDataProvider.cpp") }, Source));
	TestFalse(TEXT("No EffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No effect execution"), Source.Contains(TEXT("ApplyEffect")));
	TestFalse(TEXT("No damage execution"), Source.Contains(TEXT("ApplyDamageEffect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderNoActivationGenerationSourceGuardTest, "Wandbound.Runtime.ProductionActivationDataProvider.DoesNotGenerateActivationLegalActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderNoActivationGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Source loads"), LoadProductionProviderSourceFile({ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationDataProvider.cpp") }, Source));
	TestFalse(TEXT("No WBRules legal generation"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No ActionCodec dependency"), Source.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No FWBAction creation"), Source.Contains(TEXT("FWBAction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderCoreRuleFilesUnchangedTest, "Wandbound.Runtime.ProductionActivationDataProvider.CoreRuleFilesUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderCoreRuleFilesUnchangedTest::RunTest(const FString& Parameters)
{
	for (const FString& RelativePath : {
		FString(TEXT("Source/WandboundCore/Private/WBRules.cpp")),
		FString(TEXT("Source/WandboundCore/Private/WBEffectRunner.cpp")),
		FString(TEXT("Source/WandboundCore/Private/WBActionCodec.cpp")) })
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("%s loads"), *RelativePath), LoadProductionProviderSourceFile({ RelativePath }, Source));
		TestFalse(*FString::Printf(TEXT("%s does not mention production provider"), *RelativePath), Source.Contains(TEXT("WBProductionActivationDataProvider")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderExistingRuntimeTestsBoundaryTest, "Wandbound.Runtime.ProductionActivationDataProvider.ExistingRuntimeProviderSessionTestsBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderExistingRuntimeTestsBoundaryTest::RunTest(const FString& Parameters)
{
	FString InterfaceTests;
	FString SessionTests;
	TestTrue(TEXT("Interface tests load"), LoadProductionProviderSourceFile({ TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBRuntimeActivationDataProviderInterfaceTests.cpp") }, InterfaceTests));
	TestTrue(TEXT("Session tests load"), LoadProductionProviderSourceFile({ TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBRuntimeActivationSessionHarnessTests.cpp") }, SessionTests));
	TestTrue(TEXT("Interface tests present"), InterfaceTests.Contains(TEXT("Wandbound.Runtime.ActivationDataProvider")));
	TestTrue(TEXT("Session tests present"), SessionTests.Contains(TEXT("Wandbound.Runtime.ActivationSessionHarness")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderExistingObservationTestsBoundaryTest, "Wandbound.Runtime.ProductionActivationDataProvider.ExistingCardZoneObservationTestsBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderExistingObservationTestsBoundaryTest::RunTest(const FString& Parameters)
{
	FString ObservationTests;
	TestTrue(TEXT("Observation tests load"), LoadProductionProviderSourceFile({ TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardZoneObservationTests.cpp") }, ObservationTests));
	TestTrue(TEXT("Observation tests present"), ObservationTests.Contains(TEXT("Wandbound.Core.CardZoneObservation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderExistingRepositoryTestsBoundaryTest, "Wandbound.Runtime.ProductionActivationDataProvider.ExistingRepositoryAndFixtureLoaderTestsBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderExistingRepositoryTestsBoundaryTest::RunTest(const FString& Parameters)
{
	FString RepositoryTests;
	FString FixtureLoaderTests;
	TestTrue(TEXT("Repository tests load"), LoadProductionProviderSourceFile({ TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDefinitionRepositoryTests.cpp") }, RepositoryTests));
	TestTrue(TEXT("Fixture loader tests load"), LoadProductionProviderSourceFile({ TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDefinitionFixtureLoaderTests.cpp") }, FixtureLoaderTests));
	TestTrue(TEXT("Repository tests present"), RepositoryTests.Contains(TEXT("Wandbound.Core.CardDefinitionRepository")));
	TestTrue(TEXT("Fixture loader tests present"), FixtureLoaderTests.Contains(TEXT("Wandbound.Core.CardDefinitionFixtureLoader")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBProductionActivationDataProviderFixtureLoaderRepositorySmokeTest, "Wandbound.Runtime.ProductionActivationDataProvider.FixtureLoaderRepositorySmoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionActivationDataProviderFixtureLoaderRepositorySmokeTest::RunTest(const FString& Parameters)
{
	const FString FixturePath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Reference"),
		TEXT("GodotCanon"),
		TEXT("CardDefinitionRepositoryFixtures"),
		TEXT("valid_fixture_repository_damage.json"));
	const FWBCardDefinitionFixtureLoadResult LoadResult =
		WBCardDefinitionFixtureLoader::LoadRepositoryFromFile(FixturePath);
	TestTrue(TEXT("Fixture repository loads"), LoadResult.bOk);
	TestTrue(TEXT("Repository validates"), WBCardDefinitionRepository::ValidateRepository(LoadResult.Repository).bOk);
	return true;
}

#endif
