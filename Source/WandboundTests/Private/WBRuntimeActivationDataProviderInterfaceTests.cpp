#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBReplayTrace.h"
#include "WBRules.h"
#include "WBRuntimeActivationDataProviderAdapter.h"
#include "WBRuntimeActivationFixedDataProviderForTests.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 ProviderSourceUnitId = 1;
constexpr int32 ProviderTargetUnitId = 2;

UWBRuntimeActivationDecisionSessionFacadeComponent* MakeProviderFacade()
{
	return NewObject<UWBRuntimeActivationDecisionSessionFacadeComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointOwnerComponent* MakeProviderOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeProviderCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeProviderTurnModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeActivationPresentationModelComponent* MakeProviderActivationModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

bool MakeProviderSessionPath(
	UWBRuntimeActivationDecisionSessionFacadeComponent*& OutFacade,
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel)
{
	OutFacade = MakeProviderFacade();
	OutOwner = MakeProviderOwner();
	OutCoordinator = MakeProviderCoordinator();
	OutTurnModel = MakeProviderTurnModel();
	OutActivationModel = MakeProviderActivationModel();

	if (OutFacade == nullptr
		|| OutOwner == nullptr
		|| OutCoordinator == nullptr
		|| OutTurnModel == nullptr
		|| OutActivationModel == nullptr)
	{
		return false;
	}

	OutCoordinator->SetTurnInteractionModel(OutTurnModel);
	OutCoordinator->SetActivationPresentationModel(OutActivationModel);
	OutOwner->SetDecisionPointCoordinator(OutCoordinator);
	OutFacade->SetDecisionPointOwner(OutOwner);
	return true;
}

FWBPlayerStateData MakeProviderPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeProviderUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId,
	const int32 HP = 6)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = 6;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 4;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeProviderState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 21;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeProviderPlayer(0));
	State.Players.Add(MakeProviderPlayer(1, 0));
	State.AddUnitForTest(MakeProviderUnit(
		ProviderSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("visible_provider_source")));
	State.AddUnitForTest(MakeProviderUnit(
		ProviderTargetUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_provider_target")));
	return State;
}

FWBAction MakeProviderMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = ProviderSourceUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBPublicUnitBoardSummary MakeProviderPublicUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId,
	const int32 HP = 6)
{
	FWBPublicUnitBoardSummary Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = 6;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 4;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	return Unit;
}

FWBPublicBoardSummary MakeProviderPublicSummary(const int32 UnitCount = 2)
{
	FWBPublicBoardSummary Summary;
	if (UnitCount >= 1)
	{
		Summary.Units.Add(MakeProviderPublicUnit(
			ProviderSourceUnitId,
			0,
			FWBTile(4, 4),
			TEXT("visible_provider_source")));
	}
	if (UnitCount >= 2)
	{
		Summary.Units.Add(MakeProviderPublicUnit(
			ProviderTargetUnitId,
			1,
			FWBTile(6, 4),
			TEXT("visible_provider_target")));
	}
	return Summary;
}

FWBGenericEffectPayload MakeProviderDamagePayload(const int32 Amount = 1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.TargetUnitId = -1;
	Payload.DamageEffect.SourceUnitId = -1;
	Payload.DamageEffect.SourcePlayerId = -1;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("runtime_activation_data_provider_test"));
	return Payload;
}

FWBCardActivationCommand MakeProviderActivationCommand(
	const FString& SourceEffectId = TEXT("provider_effect"))
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = ProviderSourceUnitId;
	Command.Source.SourceCardId = TEXT("visible_provider_source");
	Command.Source.SourceEffectId = SourceEffectId;
	Command.EffectRequest.Source.PlayerId = 0;
	Command.EffectRequest.Source.SourceUnitId = ProviderSourceUnitId;
	Command.EffectRequest.Source.SourceCardId = TEXT("visible_provider_source");
	Command.EffectRequest.Source.SourceEffectId = SourceEffectId;
	Command.EffectRequest.Target.TargetUnitId = ProviderTargetUnitId;
	Command.EffectRequest.Payloads.Add(MakeProviderDamagePayload());
	Command.DebugActivationId = TEXT("provider_debug");
	return Command;
}

FWBCardActivationLegalAction MakeProviderActivationAction(
	const FString& ActivationActionId = TEXT("activate:p0:u1:provider:t2"),
	const FString& PublicLabel = TEXT("Provider Strike"))
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActivationActionId;
	Action.PlayerId = 0;
	Action.SourceUnitId = ProviderSourceUnitId;
	Action.PublicLabel = PublicLabel;
	Action.Target.TargetUnitId = ProviderTargetUnitId;
	Action.Command = MakeProviderActivationCommand();
	return Action;
}

FWBRuntimeActivationDecisionSessionRefreshInput MakeProviderRefreshInput(
	const int32 NormalActionCount = 1,
	const int32 ActivationActionCount = 1,
	const int32 PublicUnitCount = 2)
{
	FWBRuntimeActivationDecisionSessionRefreshInput Input;
	for (int32 Index = 0; Index < NormalActionCount; ++Index)
	{
		FWBAction Action = MakeProviderMoveAction();
		Action.ToTile = Index == 0 ? FWBTile(5, 4) : FWBTile(4, 5);
		Input.NormalLegalActions.Add(Action);
	}

	for (int32 Index = 0; Index < ActivationActionCount; ++Index)
	{
		Input.ActivationActionSet.Actions.Add(MakeProviderActivationAction(
			Index == 0
				? FString(TEXT("activate:p0:u1:provider:t2"))
				: FString::Printf(TEXT("activate:p0:u1:provider:post:%d"), Index),
			Index == 0
				? FString(TEXT("Provider Strike"))
				: FString::Printf(TEXT("Provider Post %d"), Index)));
	}

	Input.PublicBoardSummary = MakeProviderPublicSummary(PublicUnitCount);
	return Input;
}

FWBRuntimeActivationDataProviderRequest MakeProviderRequest(
	const EWBRuntimeActivationDataRequestKind Kind,
	const FString& SelectedActivationActionId = FString())
{
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = Kind;
	Request.PlayerId = 0;
	Request.DecisionPointId = TEXT("provider_decision_point");
	Request.SelectedActivationActionId = SelectedActivationActionId;
	Request.RequestReason = TEXT("provider_test");
	return Request;
}

FWBRuntimeActivationFixedDataProviderForTests MakeFixedProvider(
	const bool bOk,
	const FString& Reason,
	const FWBRuntimeActivationDecisionSessionRefreshInput& RefreshInput)
{
	FWBRuntimeActivationFixedDataProviderForTests Provider;
	Provider.FixedResult.bOk = bOk;
	Provider.FixedResult.Reason = Reason;
	Provider.FixedResult.RefreshInput = RefreshInput;
	return Provider;
}

FString CombineProviderActivationPublicText(const UWBRuntimeActivationPresentationModelComponent* Model)
{
	FString Combined;
	if (Model == nullptr)
	{
		return Combined;
	}

	for (const FWBCardActivationLegalActionPresentationEntry& Entry :
		Model->GetCurrentActivationPresentationSnapshot().Entries)
	{
		Combined += Entry.ActivationActionId;
		Combined += Entry.PublicLabel;
		Combined += Entry.SourcePublicCardId;
		Combined += Entry.TargetPublicCardId;
	}

	for (const FWBCardActivationTargetPresentationEntry& Entry :
		Model->GetCurrentTargetPresentationSnapshot().Entries)
	{
		Combined += Entry.ActivationActionId;
		Combined += Entry.PublicActivationLabel;
		Combined += Entry.PublicTargetLabel;
		Combined += Entry.SourcePublicCardId;
		Combined += Entry.TargetPublicCardId;
	}

	return Combined;
}

bool LoadProviderSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(OutSource, *Path);
}

bool LoadProviderRuntimeSourceText(FString& OutSource)
{
	OutSource.Reset();
	TArray<FString> FileNames;
	IFileManager::Get().FindFilesRecursive(
		FileNames,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime")),
		TEXT("*.h"),
		true,
		false);
	IFileManager::Get().FindFilesRecursive(
		FileNames,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime")),
		TEXT("*.cpp"),
		true,
		false);

	for (const FString& FileName : FileNames)
	{
		FString FileText;
		if (!FFileHelper::LoadFileToString(FileText, *FileName))
		{
			return false;
		}
		OutSource += FileText;
		OutSource += TEXT("\n");
	}

	return FileNames.Num() > 0;
}

TArray<FString> NormalActionIdsForProviderState(const FWBGameStateData& State)
{
	return WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationDataProviderDefaultsTest, "Wandbound.Runtime.ActivationDataProvider.Defaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationDataProviderDefaultsTest::RunTest(const FString& Parameters)
{
	FWBRuntimeActivationDataProviderRequest Request;
	FWBRuntimeActivationDataProviderResult Result;

	TestTrue(TEXT("Default kind"), Request.Kind == EWBRuntimeActivationDataRequestKind::Unknown);
	TestEqual(TEXT("Default player"), Request.PlayerId, -1);
	TestFalse(TEXT("Default result not ok"), Result.bOk);
	TestTrue(TEXT("Default reason empty"), Result.Reason.IsEmpty());
	TestEqual(TEXT("Default refresh normal count"), Result.RefreshInput.NormalLegalActions.Num(), 0);
	TestEqual(TEXT("Default refresh activation count"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationFixedDataProviderTest, "Wandbound.Runtime.ActivationDataProvider.FixedProviderReturnsData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationFixedDataProviderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	const int32 TargetHPBefore = State.GetUnitById(ProviderTargetUnitId)->HP;
	const FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeProviderRefreshInput(2, 1, 2);
	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(true, FString(), Input);
	const FWBRuntimeActivationDataProviderRequest Request =
		MakeProviderRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint);

	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(Request);

	TestTrue(TEXT("Provider result ok"), Result.bOk);
	TestTrue(TEXT("Request copied"), Result.Request.Kind == Request.Kind);
	TestEqual(TEXT("Normal action count"), Result.RefreshInput.NormalLegalActions.Num(), 2);
	TestEqual(TEXT("Activation action count"), Result.RefreshInput.ActivationActionSet.Actions.Num(), 1);
	TestEqual(TEXT("Public unit count"), Result.RefreshInput.PublicBoardSummary.Units.Num(), 2);
	TestEqual(TEXT("State untouched"), State.GetUnitById(ProviderTargetUnitId)->HP, TargetHPBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderRefreshNullFacadeTest, "Wandbound.Runtime.ActivationDataProvider.RefreshNullFacadeFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderRefreshNullFacadeTest::RunTest(const FString& Parameters)
{
	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(true, FString(), MakeProviderRefreshInput());
	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			nullptr,
			Provider,
			MakeProviderRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("session_facade_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderRefreshFailureTest, "Wandbound.Runtime.ActivationDataProvider.RefreshProviderFailureStops", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderRefreshFailureTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeProviderSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(false, TEXT("provider_unavailable"), MakeProviderRefreshInput());
	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeProviderRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("provider_unavailable")));
	TestFalse(TEXT("Session not refreshed"), Facade->HasSessionState());
	TestFalse(TEXT("Activation model not refreshed"), ActivationModel->HasCurrentActivationPresentationState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderRefreshSuccessTest, "Wandbound.Runtime.ActivationDataProvider.RefreshSuccess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderRefreshSuccessTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeProviderSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeProviderRefreshInput(2, 2, 2);
	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(true, FString(), Input);
	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeProviderRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestTrue(TEXT("Session refresh ok"), Result.SessionRefreshResult.bOk);
	TestEqual(TEXT("Normal count"), Result.SessionRefreshResult.Status.NormalLegalActionCount, Input.NormalLegalActions.Num());
	TestEqual(TEXT("Activation count"), Result.SessionRefreshResult.Status.ActivationActionCount, Input.ActivationActionSet.Actions.Num());
	TestEqual(TEXT("Public unit count"), Owner->GetCurrentStatus().PublicUnitCount, Input.PublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderExecuteNullFacadeTest, "Wandbound.Runtime.ActivationDataProvider.ExecuteNullFacadeFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderExecuteNullFacadeTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	const FWBGameStateData Before = State;
	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(true, FString(), MakeProviderRefreshInput());
	const FWBRuntimeActivationProviderExecutionResult Result =
		WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
			State,
			nullptr,
			TEXT("activate:p0:u1:provider:t2"),
			Provider,
			MakeProviderRequest(EWBRuntimeActivationDataRequestKind::PostActivation));

	TestFalse(TEXT("Execution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("session_facade_missing")));
	TestEqual(TEXT("State target HP unchanged"), State.GetUnitById(ProviderTargetUnitId)->HP, Before.GetUnitById(ProviderTargetUnitId)->HP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderExecuteProviderFailureTest, "Wandbound.Runtime.ActivationDataProvider.ExecuteProviderFailureStops", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderExecuteProviderFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeProviderSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(MakeProviderRefreshInput()).bOk);

	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(false, TEXT("provider_post_data_missing"), MakeProviderRefreshInput());
	const FWBRuntimeActivationProviderExecutionResult Result =
		WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
			State,
			Facade,
			TEXT("activate:p0:u1:provider:t2"),
			Provider,
			MakeProviderRequest(EWBRuntimeActivationDataRequestKind::PostActivation));

	TestFalse(TEXT("Execution fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("provider_post_data_missing")));
	TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(ProviderTargetUnitId)->HP, Before.GetUnitById(ProviderTargetUnitId)->HP);
	TestFalse(TEXT("No session execution result"), Result.SessionExecutionResult.PostActivationRefreshResult.bActivationExecutionAttempted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderExecuteSuccessTest, "Wandbound.Runtime.ActivationDataProvider.ExecuteSuccessUsesProviderPostData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderExecuteSuccessTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeProviderSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput InitialInput = MakeProviderRefreshInput(1, 1, 2);
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(InitialInput).bOk);
	FWBRuntimeActivationDecisionSessionRefreshInput PostInput = MakeProviderRefreshInput(2, 0, 1);
	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(true, FString(), PostInput);
	const FWBRuntimeActivationProviderExecutionResult Result =
		WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
			State,
			Facade,
			InitialInput.ActivationActionSet.Actions[0].ActivationActionId,
			Provider,
			MakeProviderRequest(
				EWBRuntimeActivationDataRequestKind::PostActivation,
				InitialInput.ActivationActionSet.Actions[0].ActivationActionId));

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestTrue(TEXT("Provider ok"), Result.PostActivationProviderResult.bOk);
	TestTrue(TEXT("Session execution ok"), Result.SessionExecutionResult.bOk);
	TestEqual(TEXT("Target damaged through facade path"), State.GetUnitById(ProviderTargetUnitId)->HP, 5);
	TestEqual(TEXT("Normal count from provider post data"), Result.SessionExecutionResult.Status.NormalLegalActionCount, PostInput.NormalLegalActions.Num());
	TestEqual(TEXT("Activation count from provider post data"), Result.SessionExecutionResult.Status.ActivationActionCount, 0);
	TestEqual(TEXT("Public unit count from provider post data"), Owner->GetCurrentStatus().PublicUnitCount, PostInput.PublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderAdapterSourceGuardTest, "Wandbound.Runtime.ActivationDataProvider.AdapterSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderAdapterSourceGuardTest::RunTest(const FString& Parameters)
{
	FString AdapterSource;
	TestTrue(
		TEXT("Adapter source loads"),
		LoadProviderSourceFile(
			{ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeActivationDataProviderAdapter.cpp") },
			AdapterSource));

	TestFalse(TEXT("No normal legal generation"), AdapterSource.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No activation candidate generator"), AdapterSource.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("No activation legal action generator"), AdapterSource.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("No public summary build"), AdapterSource.Contains(TEXT("WBPublicBoardSummary::Build")));
	TestFalse(TEXT("No direct effect runner"), AdapterSource.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No action codec"), AdapterSource.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderInterfaceSourceGuardTest, "Wandbound.Runtime.ActivationDataProvider.InterfaceSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderInterfaceSourceGuardTest::RunTest(const FString& Parameters)
{
	FString ProviderHeader;
	TestTrue(
		TEXT("Provider header loads"),
		LoadProviderSourceFile(
			{ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeActivationDataProvider.h") },
			ProviderHeader));

	TestFalse(TEXT("Provider interface does not accept game state"), ProviderHeader.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("Provider interface does not expose hidden zones"), ProviderHeader.Contains(TEXT("Hand")) || ProviderHeader.Contains(TEXT("Deck")) || ProviderHeader.Contains(TEXT("Discard")));

	FString RuntimeSource;
	TestTrue(TEXT("Runtime source loads"), LoadProviderRuntimeSourceText(RuntimeSource));
	TestFalse(TEXT("Runtime does not include fixed test provider"), RuntimeSource.Contains(TEXT("WBRuntimeActivationFixedDataProviderForTests")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderHiddenMetadataTest, "Wandbound.Runtime.ActivationDataProvider.HiddenMetadataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderHiddenMetadataTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeProviderSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FString SecretSourceEffect = TEXT("SECRET_PROVIDER_SOURCE_EFFECT_DO_NOT_LEAK");
	const FString SecretUsageKey = TEXT("SECRET_PROVIDER_USAGE_KEY_DO_NOT_LEAK");
	const FString SecretDebugId = TEXT("SECRET_PROVIDER_DEBUG_ID_DO_NOT_LEAK");
	const FString SecretCostKind = TEXT("SECRET_PROVIDER_COST_KIND_DO_NOT_LEAK");

	FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeProviderRefreshInput();
	FWBCardActivationLegalAction& Action = Input.ActivationActionSet.Actions[0];
	Action.PublicLabel = TEXT("Safe Provider Strike");
	Action.Command.Source.SourceEffectId = SecretSourceEffect;
	Action.Command.EffectRequest.Source.SourceEffectId = SecretSourceEffect;
	Action.Command.DebugActivationId = SecretDebugId;
	Action.Command.UsageCommit.bMarkUsageOnSuccess = true;
	Action.Command.UsageCommit.PlayerId = 0;
	Action.Command.UsageCommit.UsageKey = SecretUsageKey;
	Action.Command.CostPaymentCommit.CostKind = FName(*SecretCostKind);

	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(true, FString(), Input);
	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeProviderRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	const FString PublicText = CombineProviderActivationPublicText(ActivationModel);
	for (const FString& Token : { SecretSourceEffect, SecretUsageKey, SecretDebugId, SecretCostKind })
	{
		TestFalse(*FString::Printf(TEXT("Public text excludes %s"), *Token), PublicText.Contains(Token, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("Safe label present"), PublicText.Contains(TEXT("Safe Provider Strike")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderActionBoundaryTest, "Wandbound.Runtime.ActivationDataProvider.ActivationSeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderActionBoundaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeProviderState();
	const TArray<FString> BeforeActionIds = NormalActionIdsForProviderState(State);
	const FString FirstActionIdBefore = BeforeActionIds.Num() > 0 ? BeforeActionIds[0] : FString();

	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeProviderSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput InitialInput = MakeProviderRefreshInput();
	TestTrue(TEXT("Initial refresh"), Facade->RefreshSessionFromExternalData(InitialInput).bOk);
	const FWBRuntimeActivationFixedDataProviderForTests Provider =
		MakeFixedProvider(true, FString(), MakeProviderRefreshInput(1, 0, 2));
	const FWBRuntimeActivationProviderExecutionResult Result =
		WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
			State,
			Facade,
			InitialInput.ActivationActionSet.Actions[0].ActivationActionId,
			Provider,
			MakeProviderRequest(EWBRuntimeActivationDataRequestKind::PostActivation));

	TestTrue(TEXT("Execution ok"), Result.bOk);
	const TArray<FString> AfterActionIds = NormalActionIdsForProviderState(State);
	TestEqual(TEXT("Normal legal action count unchanged"), AfterActionIds.Num(), BeforeActionIds.Num());
	for (const FString& ActionId : AfterActionIds)
	{
		TestFalse(TEXT("Normal action id is not activation"), ActionId.StartsWith(TEXT("activate:")));
	}
	TestEqual(TEXT("WBActionCodec output unchanged"), AfterActionIds.Num() > 0 ? AfterActionIds[0] : FString(), FirstActionIdBefore);
	return true;
}

#endif
