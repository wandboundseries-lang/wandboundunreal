#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBRules.h"
#include "WBRuntimeActivationContractDataProvidersForTests.h"
#include "WBRuntimeActivationDataProviderAdapter.h"
#include "WBRuntimeActivationDataProviderContractVerifier.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 ContractSourceUnitId = 1;
constexpr int32 ContractTargetUnitId = 2;

UWBRuntimeActivationDecisionSessionFacadeComponent* MakeContractFacade()
{
	return NewObject<UWBRuntimeActivationDecisionSessionFacadeComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointOwnerComponent* MakeContractOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeContractCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeContractTurnModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeActivationPresentationModelComponent* MakeContractActivationModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

bool MakeContractSessionPath(
	UWBRuntimeActivationDecisionSessionFacadeComponent*& OutFacade,
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel)
{
	OutFacade = MakeContractFacade();
	OutOwner = MakeContractOwner();
	OutCoordinator = MakeContractCoordinator();
	OutTurnModel = MakeContractTurnModel();
	OutActivationModel = MakeContractActivationModel();

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

FWBPlayerStateData MakeContractPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeContractUnit(
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

FWBGameStateData MakeContractState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 25;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeContractPlayer(0));
	State.Players.Add(MakeContractPlayer(1, 0));
	State.AddUnitForTest(MakeContractUnit(
		ContractSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("visible_contract_source")));
	State.AddUnitForTest(MakeContractUnit(
		ContractTargetUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_contract_target")));
	return State;
}

FWBAction MakeContractMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = ContractSourceUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBPublicUnitBoardSummary MakeContractPublicUnit(
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

FWBPublicBoardSummary MakeContractPublicSummary(const int32 UnitCount = 2)
{
	FWBPublicBoardSummary Summary;
	if (UnitCount >= 1)
	{
		Summary.Units.Add(MakeContractPublicUnit(
			ContractSourceUnitId,
			0,
			FWBTile(4, 4),
			TEXT("visible_contract_source")));
	}
	if (UnitCount >= 2)
	{
		Summary.Units.Add(MakeContractPublicUnit(
			ContractTargetUnitId,
			1,
			FWBTile(6, 4),
			TEXT("visible_contract_target")));
	}
	return Summary;
}

FWBGenericEffectPayload MakeContractDamagePayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.TargetUnitId = -1;
	Payload.DamageEffect.SourceUnitId = -1;
	Payload.DamageEffect.SourcePlayerId = -1;
	Payload.DamageEffect.Amount = 1;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("runtime_activation_provider_contract_test"));
	return Payload;
}

FWBCardActivationCommand MakeContractActivationCommand()
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = ContractSourceUnitId;
	Command.Source.SourceCardId = TEXT("visible_contract_source");
	Command.Source.SourceEffectId = TEXT("contract_effect");
	Command.EffectRequest.Source.PlayerId = 0;
	Command.EffectRequest.Source.SourceUnitId = ContractSourceUnitId;
	Command.EffectRequest.Source.SourceCardId = TEXT("visible_contract_source");
	Command.EffectRequest.Source.SourceEffectId = TEXT("contract_effect");
	Command.EffectRequest.Target.TargetUnitId = ContractTargetUnitId;
	Command.EffectRequest.Payloads.Add(MakeContractDamagePayload());
	return Command;
}

FWBCardActivationLegalAction MakeContractActivationAction(
	const FString& ActivationActionId = TEXT("activate:p0:u1:contract:t2"),
	const FString& PublicLabel = TEXT("Contract Strike"))
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActivationActionId;
	Action.PlayerId = 0;
	Action.SourceUnitId = ContractSourceUnitId;
	Action.PublicLabel = PublicLabel;
	Action.Target.TargetUnitId = ContractTargetUnitId;
	Action.Command = MakeContractActivationCommand();
	return Action;
}

FWBRuntimeActivationDecisionSessionRefreshInput MakeContractRefreshInput(
	const int32 NormalActionCount = 1,
	const int32 ActivationActionCount = 1,
	const int32 PublicUnitCount = 2)
{
	FWBRuntimeActivationDecisionSessionRefreshInput Input;
	for (int32 Index = 0; Index < NormalActionCount; ++Index)
	{
		FWBAction Action = MakeContractMoveAction();
		Action.ToTile = Index == 0 ? FWBTile(5, 4) : FWBTile(4, 5);
		Input.NormalLegalActions.Add(Action);
	}

	for (int32 Index = 0; Index < ActivationActionCount; ++Index)
	{
		Input.ActivationActionSet.Actions.Add(MakeContractActivationAction(
			Index == 0
				? FString(TEXT("activate:p0:u1:contract:t2"))
				: FString::Printf(TEXT("activate:p0:u1:contract:%d"), Index),
			Index == 0
				? FString(TEXT("Contract Strike"))
				: FString::Printf(TEXT("Contract Strike %d"), Index)));
	}

	Input.PublicBoardSummary = MakeContractPublicSummary(PublicUnitCount);
	return Input;
}

FWBRuntimeActivationDataProviderRequest MakeContractRequest(
	const EWBRuntimeActivationDataRequestKind Kind,
	const FString& SelectedActivationActionId = FString())
{
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = Kind;
	Request.PlayerId = 0;
	Request.DecisionPointId = TEXT("contract_decision");
	Request.SelectedActivationActionId = SelectedActivationActionId;
	Request.RequestReason = TEXT("contract_test");
	return Request;
}

FWBRuntimeActivationDataProviderResult MakeContractProviderResult(
	const EWBRuntimeActivationDataRequestKind Kind,
	const FWBRuntimeActivationDecisionSessionRefreshInput& Input,
	const FString& SelectedActivationActionId = FString())
{
	FWBRuntimeActivationDataProviderResult Result;
	Result.bOk = true;
	Result.Request = MakeContractRequest(Kind, SelectedActivationActionId);
	Result.RefreshInput = Input;
	return Result;
}

FWBDeterministicActivationDataProviderForTests MakeContractProvider(
	const EWBRuntimeActivationDataRequestKind Kind,
	const FWBRuntimeActivationDecisionSessionRefreshInput& Input)
{
	FWBDeterministicActivationDataProviderForTests Provider;
	Provider.FixedResult = MakeContractProviderResult(Kind, Input);
	return Provider;
}

FWBRuntimeActivationProviderContractExpectations MakeStrictExpectations()
{
	FWBRuntimeActivationProviderContractExpectations Expectations;
	Expectations.ExpectedMinNormalLegalActions = 1;
	Expectations.ExpectedMinActivationActions = 1;
	Expectations.ExpectedMinPublicUnits = 1;
	Expectations.ForbiddenSubstrings = {
		TEXT("SECRET_CONTRACT"),
		TEXT("EffectRunner"),
		TEXT("schema"),
		TEXT("raw effect.type"),
		TEXT("from_tile"),
		TEXT("to_tile")
	};
	return Expectations;
}

bool LoadContractSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(OutSource, *Path);
}

bool LoadContractRuntimeSourceText(FString& OutSource)
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

TArray<FString> NormalActionIdsForContractState(const FWBGameStateData& State)
{
	return WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractCurrentDecisionValidTest, "Wandbound.Runtime.ActivationDataProviderContract.ValidCurrentDecision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractCurrentDecisionValidTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
			MakeContractRefreshInput(1, 1, 2));

	TestTrue(
		TEXT("Current decision contract passes"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestTrue(TEXT("Reason empty"), Reason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractPostActivationValidTest, "Wandbound.Runtime.ActivationDataProviderContract.ValidPostActivation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractPostActivationValidTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::PostActivation,
			MakeContractRefreshInput(2, 1, 1),
			TEXT("activate:p0:u1:contract:t2"));

	TestTrue(
		TEXT("Post activation contract passes"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractUnknownKindTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsUnknownKind", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractUnknownKindTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::Unknown,
			MakeContractRefreshInput());

	TestFalse(
		TEXT("Unknown kind fails"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("provider_request_kind_unknown")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractMissingPlayerIdTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsMissingPlayerId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractMissingPlayerIdTest::RunTest(const FString& Parameters)
{
	FString Reason;
	FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
			MakeContractRefreshInput());
	Result.Request.PlayerId = -1;

	TestFalse(
		TEXT("Missing player id fails"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("provider_player_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractMissingDecisionPointIdTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsMissingDecisionPointId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractMissingDecisionPointIdTest::RunTest(const FString& Parameters)
{
	FString Reason;
	FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
			MakeContractRefreshInput());
	Result.Request.DecisionPointId.Reset();

	TestFalse(
		TEXT("Missing decision point id fails"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("provider_decision_point_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractMissingSelectedPostActivationIdTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsMissingPostActivationSelectedId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractMissingSelectedPostActivationIdTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::PostActivation,
			MakeContractRefreshInput());

	TestFalse(
		TEXT("Missing post-activation selected id fails"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("provider_selected_activation_action_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractMissingNormalActionsTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsMissingNormalActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractMissingNormalActionsTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
			MakeContractRefreshInput(0, 1, 2));

	TestFalse(
		TEXT("Missing normal actions fail"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("normal_legal_actions_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractMissingActivationActionsTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsMissingActivationActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractMissingActivationActionsTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
			MakeContractRefreshInput(1, 0, 2));

	TestFalse(
		TEXT("Missing activation actions fail"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("activation_actions_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractMissingPublicSummaryTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsMissingPublicSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractMissingPublicSummaryTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(
			EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
			MakeContractRefreshInput(1, 1, 0));

	TestFalse(
		TEXT("Missing public summary units fail"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("public_board_summary_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractEmptyActivationIdTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsEmptyActivationId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractEmptyActivationIdTest::RunTest(const FString& Parameters)
{
	FString Reason;
	FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeContractRefreshInput();
	Input.ActivationActionSet.Actions[0].ActivationActionId.Reset();
	const FWBRuntimeActivationDataProviderResult Result =
		MakeContractProviderResult(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint, Input);

	TestFalse(
		TEXT("Empty activation id fails"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("activation_action_id_missing:0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractHiddenSubstringTest, "Wandbound.Runtime.ActivationDataProviderContract.RejectsHiddenSubstrings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractHiddenSubstringTest::RunTest(const FString& Parameters)
{
	FString Reason;
	FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeContractRefreshInput();
	Input.ActivationActionSet.Actions[0].PublicLabel = TEXT("SECRET_CONTRACT_LABEL");
	const FWBRuntimeActivationDataProviderResult HiddenResult =
		MakeContractProviderResult(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint, Input);
	FWBHiddenTokenActivationDataProviderForTests Provider;
	Provider.HiddenResult = HiddenResult;

	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(MakeContractRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));
	TestFalse(
		TEXT("Hidden substring fails"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyProviderResultContract(
			Result,
			MakeStrictExpectations(),
			Reason));
	TestTrue(TEXT("Reason identifies hidden substring"), Reason.StartsWith(TEXT("hidden_substring_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractDeterministicPassTest, "Wandbound.Runtime.ActivationDataProviderContract.DeterministicProviderPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractDeterministicPassTest::RunTest(const FString& Parameters)
{
	FString Reason;
	const FWBDeterministicActivationDataProviderForTests Provider =
		MakeContractProvider(
			EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
			MakeContractRefreshInput(2, 2, 2));

	TestTrue(
		TEXT("Deterministic provider passes"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyDeterministicProviderOutput(
			Provider,
			MakeContractRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint),
			Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractDeterministicFailTest, "Wandbound.Runtime.ActivationDataProviderContract.DeterministicProviderFailsForChangingOutput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractDeterministicFailTest::RunTest(const FString& Parameters)
{
	FString Reason;
	FWBChangingActivationDataProviderForTests Provider;
	Provider.BaseResult = MakeContractProviderResult(
		EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint,
		MakeContractRefreshInput());

	TestFalse(
		TEXT("Changing provider fails"),
		FWBRuntimeActivationDataProviderContractVerifier::VerifyDeterministicProviderOutput(
			Provider,
			MakeContractRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint),
			Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("provider_output_not_deterministic")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractFailingProviderTest, "Wandbound.Runtime.ActivationDataProviderContract.FailingProviderPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractFailingProviderTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeContractSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	FWBFailingActivationDataProviderForTests Provider;
	Provider.FailureReason = TEXT("contract_provider_failed");
	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeContractRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));

	TestFalse(TEXT("Adapter fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("contract_provider_failed")));
	TestFalse(TEXT("Session not refreshed"), Facade->HasSessionState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractAdapterRefreshValidTest, "Wandbound.Runtime.ActivationDataProviderContract.AdapterRefreshesValidProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractAdapterRefreshValidTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeContractSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput Input = MakeContractRefreshInput(2, 2, 2);
	const FWBDeterministicActivationDataProviderForTests Provider =
		MakeContractProvider(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint, Input);
	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeContractRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));

	TestTrue(TEXT("Adapter refresh ok"), Result.bOk);
	TestEqual(TEXT("Normal count"), Result.SessionRefreshResult.Status.NormalLegalActionCount, Input.NormalLegalActions.Num());
	TestEqual(TEXT("Activation count"), Result.SessionRefreshResult.Status.ActivationActionCount, Input.ActivationActionSet.Actions.Num());
	TestEqual(TEXT("Public unit count"), Owner->GetCurrentStatus().PublicUnitCount, Input.PublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractAdapterRejectsFailingProviderTest, "Wandbound.Runtime.ActivationDataProviderContract.AdapterRejectsFailingProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractAdapterRejectsFailingProviderTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeContractSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	FWBFailingActivationDataProviderForTests Provider;
	Provider.FailureReason = TEXT("contract_invalid");
	const FWBRuntimeActivationProviderRefreshResult Result =
		WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider(
			Facade,
			Provider,
			MakeContractRequest(EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint));

	TestFalse(TEXT("Adapter refresh fails"), Result.bOk);
	TestFalse(TEXT("Session remains empty"), Facade->HasSessionState());
	TestFalse(TEXT("Activation model remains empty"), ActivationModel->HasCurrentActivationPresentationState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractAdapterExecuteValidTest, "Wandbound.Runtime.ActivationDataProviderContract.AdapterExecuteRefreshesValidProvider", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractAdapterExecuteValidTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeContractState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeContractSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput InitialInput = MakeContractRefreshInput(1, 1, 2);
	TestTrue(TEXT("Initial refresh ok"), Facade->RefreshSessionFromExternalData(InitialInput).bOk);
	const FWBRuntimeActivationDecisionSessionRefreshInput PostInput = MakeContractRefreshInput(2, 0, 1);
	const FWBDeterministicActivationDataProviderForTests Provider =
		MakeContractProvider(EWBRuntimeActivationDataRequestKind::PostActivation, PostInput);

	const FWBRuntimeActivationProviderExecutionResult Result =
		WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
			State,
			Facade,
			InitialInput.ActivationActionSet.Actions[0].ActivationActionId,
			Provider,
			MakeContractRequest(
				EWBRuntimeActivationDataRequestKind::PostActivation,
				InitialInput.ActivationActionSet.Actions[0].ActivationActionId));

	TestTrue(TEXT("Execute ok"), Result.bOk);
	TestEqual(TEXT("Target damaged"), State.GetUnitById(ContractTargetUnitId)->HP, 5);
	TestEqual(TEXT("Normal post count"), Result.SessionExecutionResult.Status.NormalLegalActionCount, PostInput.NormalLegalActions.Num());
	TestEqual(TEXT("Activation post count"), Result.SessionExecutionResult.Status.ActivationActionCount, 0);
	TestEqual(TEXT("Public post count"), Owner->GetCurrentStatus().PublicUnitCount, PostInput.PublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractAdapterExecuteFailureStopsTest, "Wandbound.Runtime.ActivationDataProviderContract.AdapterExecuteProviderFailureStops", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractAdapterExecuteFailureStopsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeContractState();
	const FWBGameStateData Before = State;
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeContractSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput InitialInput = MakeContractRefreshInput();
	TestTrue(TEXT("Initial refresh ok"), Facade->RefreshSessionFromExternalData(InitialInput).bOk);
	FWBFailingActivationDataProviderForTests Provider;
	Provider.FailureReason = TEXT("contract_post_failed");
	const FWBRuntimeActivationProviderExecutionResult Result =
		WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
			State,
			Facade,
			InitialInput.ActivationActionSet.Actions[0].ActivationActionId,
			Provider,
			MakeContractRequest(EWBRuntimeActivationDataRequestKind::PostActivation));

	TestFalse(TEXT("Execute fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("contract_post_failed")));
	TestEqual(TEXT("State unchanged"), State.GetUnitById(ContractTargetUnitId)->HP, Before.GetUnitById(ContractTargetUnitId)->HP);
	TestFalse(TEXT("No execution attempted"), Result.SessionExecutionResult.PostActivationRefreshResult.bActivationExecutionAttempted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractRuntimeSourceGuardTest, "Wandbound.Runtime.ActivationDataProviderContract.RuntimeSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractRuntimeSourceGuardTest::RunTest(const FString& Parameters)
{
	FString RuntimeSource;
	TestTrue(TEXT("Runtime source loads"), LoadContractRuntimeSourceText(RuntimeSource));
	TestFalse(TEXT("Runtime does not include contract verifier"), RuntimeSource.Contains(TEXT("WBRuntimeActivationDataProviderContractVerifier")));
	TestFalse(TEXT("Runtime does not include contract test providers"), RuntimeSource.Contains(TEXT("WBRuntimeActivationContractDataProvidersForTests")));
	TestFalse(TEXT("Runtime does not generate normal legal actions"), RuntimeSource.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Runtime does not generate activation candidates"), RuntimeSource.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("Runtime does not generate activation legal actions"), RuntimeSource.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractInterfaceSourceGuardTest, "Wandbound.Runtime.ActivationDataProviderContract.InterfaceNoGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractInterfaceSourceGuardTest::RunTest(const FString& Parameters)
{
	FString ProviderHeader;
	TestTrue(
		TEXT("Provider header loads"),
		LoadContractSourceFile(
			{ TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeActivationDataProvider.h") },
			ProviderHeader));
	TestFalse(TEXT("Provider interface does not accept state"), ProviderHeader.Contains(TEXT("FWBGameStateData")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationProviderContractActionBoundaryTest, "Wandbound.Runtime.ActivationDataProviderContract.ActivationSeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationProviderContractActionBoundaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeContractState();
	const TArray<FString> BeforeActionIds = NormalActionIdsForContractState(State);
	const FString FirstActionIdBefore = BeforeActionIds.Num() > 0 ? BeforeActionIds[0] : FString();

	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeContractSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBRuntimeActivationDecisionSessionRefreshInput InitialInput = MakeContractRefreshInput();
	TestTrue(TEXT("Initial refresh ok"), Facade->RefreshSessionFromExternalData(InitialInput).bOk);
	const FWBDeterministicActivationDataProviderForTests Provider =
		MakeContractProvider(EWBRuntimeActivationDataRequestKind::PostActivation, MakeContractRefreshInput(1, 0, 2));
	const FWBRuntimeActivationProviderExecutionResult Result =
		WBRuntimeActivationDataProviderAdapter::ExecuteActivationAndRefreshFromProvider(
			State,
			Facade,
			InitialInput.ActivationActionSet.Actions[0].ActivationActionId,
			Provider,
			MakeContractRequest(EWBRuntimeActivationDataRequestKind::PostActivation));

	TestTrue(TEXT("Execute ok"), Result.bOk);
	const TArray<FString> AfterActionIds = NormalActionIdsForContractState(State);
	TestEqual(TEXT("Normal action count unchanged"), AfterActionIds.Num(), BeforeActionIds.Num());
	for (const FString& ActionId : AfterActionIds)
	{
		TestFalse(TEXT("Normal action id is not activation"), ActionId.StartsWith(TEXT("activate:")));
	}
	TestEqual(TEXT("WBActionCodec output unchanged"), AfterActionIds.Num() > 0 ? AfterActionIds[0] : FString(), FirstActionIdBefore);
	return true;
}

#endif
