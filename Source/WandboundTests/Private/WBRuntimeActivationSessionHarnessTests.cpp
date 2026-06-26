#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardActivationSourceGate.h"
#include "WBEffectRunner.h"
#include "WBReplayTrace.h"
#include "WBRules.h"
#include "WBRuntimeActivationExecutionBridge.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRuntimeActivationSessionTestHarness.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 HarnessSourceUnitId = 1;
constexpr int32 HarnessTargetUnitId = 2;
constexpr int32 HarnessSecondTargetUnitId = 3;

UWBRuntimeActivationDecisionSessionFacadeComponent* MakeHarnessFacade()
{
	return NewObject<UWBRuntimeActivationDecisionSessionFacadeComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointOwnerComponent* MakeHarnessOwner()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeHarnessCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeHarnessTurnModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeActivationPresentationModelComponent* MakeHarnessActivationModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

bool MakeHarnessSessionPath(
	UWBRuntimeActivationDecisionSessionFacadeComponent*& OutFacade,
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutTurnModel,
	UWBRuntimeActivationPresentationModelComponent*& OutActivationModel)
{
	OutFacade = MakeHarnessFacade();
	OutOwner = MakeHarnessOwner();
	OutCoordinator = MakeHarnessCoordinator();
	OutTurnModel = MakeHarnessTurnModel();
	OutActivationModel = MakeHarnessActivationModel();

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

FWBPlayerStateData MakeHarnessPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeHarnessUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId,
	const int32 HP = 6,
	const int32 RLTotal = 4,
	const int32 RLUsed = 0)
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
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeHarnessState(
	const int32 SourceRLTotal = 4,
	const int32 SourceRLUsed = 0,
	const int32 TargetHP = 6)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 18;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeHarnessPlayer(0, 2));
	State.Players.Add(MakeHarnessPlayer(1, 0));
	State.AddUnitForTest(MakeHarnessUnit(
		HarnessSourceUnitId,
		0,
		FWBTile(4, 4),
		TEXT("visible_harness_card"),
		6,
		SourceRLTotal,
		SourceRLUsed));
	State.AddUnitForTest(MakeHarnessUnit(
		HarnessTargetUnitId,
		1,
		FWBTile(6, 4),
		TEXT("visible_harness_target"),
		TargetHP,
		0,
		0));
	State.AddUnitForTest(MakeHarnessUnit(
		HarnessSecondTargetUnitId,
		1,
		FWBTile(4, 6),
		TEXT("visible_harness_second_target"),
		TargetHP,
		0,
		0));
	return State;
}

FWBGenericEffectPayload MakeHarnessDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.TargetUnitId = -1;
	Payload.DamageEffect.SourceUnitId = -1;
	Payload.DamageEffect.SourcePlayerId = -1;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Activation"));
	Payload.DamageEffect.SourceReason = FName(TEXT("runtime_activation_session_harness"));
	return Payload;
}

FWBEffectTargetRef MakeHarnessUnitTarget(const int32 TargetUnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = TargetUnitId;
	return Target;
}

FWBCardActivationSourceGateDefinition MakeHarnessBoardGate(
	const bool bOncePerTurn = false,
	const int32 RequiredRR = 0)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Board;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresSourceUnit = true;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.bRequiresFixtureZoneOwnership = true;
	Gate.CostGate.bRequiresExternalAffordability = RequiredRR > 0;
	Gate.CostGate.RequiredRR = RequiredRR;
	Gate.CostGate.CostKind = RequiredRR > 0 ? FName(TEXT("RR")) : FName();
	Gate.bOncePerTurn = bOncePerTurn;
	Gate.OncePerTurnKey = bOncePerTurn ? FString(TEXT("runtime_activation_session_once")) : FString();
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBCardEffectDefinition MakeHarnessEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const int32 DamageAmount,
	const FWBCardActivationSourceGateDefinition& Gate)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = Gate;
	Effect.Payloads.Add(MakeHarnessDamagePayload(DamageAmount));
	return Effect;
}

FWBCardDefinition MakeHarnessCardDefinition(
	const FString& CardId,
	const TArray<FWBCardEffectDefinition>& Effects)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = TEXT("Visible Harness Card");
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects = Effects;
	return Definition;
}

FWBCardActivationSourceGateContext MakeHarnessGateContext(
	const FWBGameStateData& State,
	const int32 RequiredRR = 0)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = HarnessSourceUnitId;
	Context.SourceCardId = TEXT("visible_harness_card");
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.bCostsSatisfiedExternally = true;
	Context.ActivationUsageKey = TEXT("runtime_activation_session_once");
	Context.bHasExplicitSourceGateContext = true;

	if (RequiredRR > 0)
	{
		const FWBUnitState* SourceUnit = State.GetUnitById(HarnessSourceUnitId);
		const int32 AvailableRL = SourceUnit != nullptr
			? FMath::Max(SourceUnit->RLTotal - SourceUnit->RLUsed, 0)
			: 0;
		Context.CostContext.bHasExternalAffordability = true;
		Context.CostContext.bExternallyAffordable = AvailableRL >= RequiredRR;
		Context.CostContext.SuppliedRequiredRR = RequiredRR;
		Context.CostContext.SuppliedAvailableRL = AvailableRL;
		Context.CostContext.CostKind = FName(TEXT("RR"));
	}

	return Context;
}

FWBCardActivationCandidateSource MakeHarnessSource(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 RequiredRR = 0)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = HarnessSourceUnitId;
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	Source.SourceGateContext = MakeHarnessGateContext(State, RequiredRR);
	return Source;
}

TArray<FWBCardActivationCandidateSource> MakeSingleActivationSources(
	const FWBGameStateData& State,
	const FString& PublicLabel = TEXT("Harness Strike"),
	const bool bOncePerTurn = false,
	const int32 RequiredRR = 0,
	const int32 TargetUnitId = HarnessTargetUnitId)
{
	const FWBCardActivationSourceGateDefinition Gate =
		MakeHarnessBoardGate(bOncePerTurn, RequiredRR);
	const FWBCardDefinition Definition = MakeHarnessCardDefinition(
		TEXT("visible_harness_card"),
		{ MakeHarnessEffect(TEXT("strike"), PublicLabel, 1, Gate) });
	return { MakeHarnessSource(State, Definition, { MakeHarnessUnitTarget(TargetUnitId) }, RequiredRR) };
}

TArray<FWBCardActivationCandidateSource> MakeTwoActivationSources(const FWBGameStateData& State)
{
	const FWBCardActivationSourceGateDefinition Gate = MakeHarnessBoardGate();
	const FWBCardDefinition Definition = MakeHarnessCardDefinition(
		TEXT("visible_harness_card"),
		{
			MakeHarnessEffect(TEXT("first_strike"), TEXT("First Strike"), 1, Gate),
			MakeHarnessEffect(TEXT("second_strike"), TEXT("Second Strike"), 1, Gate)
		});
	return {
		MakeHarnessSource(
			State,
			Definition,
			{
				MakeHarnessUnitTarget(HarnessTargetUnitId),
				MakeHarnessUnitTarget(HarnessSecondTargetUnitId)
			})
	};
}

FWBRuntimeActivationDecisionSessionRefreshInput ToRefreshInput(
	const FWBActivationSessionExternalDataForTest& ExternalData)
{
	FWBRuntimeActivationDecisionSessionRefreshInput Input;
	Input.NormalLegalActions = ExternalData.NormalLegalActions;
	Input.ActivationActionSet = ExternalData.ActivationActionSet;
	Input.PublicBoardSummary = ExternalData.PublicBoardSummary;
	return Input;
}

bool PreviewPostActionData(
	const FWBGameStateData& State,
	const FString& ActivationActionId,
	const TArray<FWBCardActivationCandidateSource>& InitialSources,
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade,
	FWBActivationSessionExternalDataForTest& OutPostData)
{
	if (Facade == nullptr)
	{
		return false;
	}

	FWBGameStateData PredictedState = State;
	const FWBRuntimeActivationExecutionHandoffResult Handoff =
		Facade->CreateActivationExecutionHandoff(ActivationActionId);
	const FWBRuntimeActivationExecutionResult Execution =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(PredictedState, Handoff);
	if (!Execution.bOk)
	{
		return false;
	}

	OutPostData = FWBActivationSessionTestHarness::BuildExternalDataForTest(
		PredictedState,
		0,
		InitialSources);
	return true;
}

FString CombineActivationPublicText(const UWBRuntimeActivationPresentationModelComponent* Model)
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

bool LoadSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(
		OutSource,
		*Path);
}

bool LoadRuntimeSourceText(FString& OutSource)
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

TArray<FString> NormalActionIdsForHarnessState(const FWBGameStateData& State)
{
	return WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessSourceGuardTest, "Wandbound.Runtime.ActivationSessionHarness.TestOnlySourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessSourceGuardTest::RunTest(const FString& Parameters)
{
	FString HarnessHeader;
	FString HarnessSource;
	TestTrue(
		TEXT("Harness header loads from WandboundTests"),
		LoadSourceFile(
			{ TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBRuntimeActivationSessionTestHarness.h") },
			HarnessHeader));
	TestTrue(
		TEXT("Harness source loads from WandboundTests"),
		LoadSourceFile(
			{ TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBRuntimeActivationSessionTestHarness.cpp") },
			HarnessSource));
	TestTrue(TEXT("Harness is test-only"), HarnessHeader.Contains(TEXT("Test-only helper")));
	TestTrue(TEXT("Harness simulates external owner"), HarnessSource.Contains(TEXT("external activation owner")));
	TestTrue(TEXT("Harness may generate normal legal actions in tests"), HarnessSource.Contains(TEXT("WBRules::GenerateLegalActionsForPlayer")));
	TestTrue(TEXT("Harness may generate activation candidates in tests"), HarnessSource.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestTrue(TEXT("Harness may generate activation legal actions in tests"), HarnessSource.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestTrue(TEXT("Harness may build public summaries in tests"), HarnessSource.Contains(TEXT("WBPublicBoardSummary::Build")));

	FString RuntimeSource;
	TestTrue(TEXT("Runtime source loads"), LoadRuntimeSourceText(RuntimeSource));
	TestFalse(TEXT("Runtime does not include test harness"), RuntimeSource.Contains(TEXT("WBRuntimeActivationSessionTestHarness")));
	TestFalse(TEXT("Runtime does not generate normal legal actions"), RuntimeSource.Contains(TEXT("WBRules::GenerateLegalActions")));
	TestFalse(TEXT("Runtime does not generate activation candidates"), RuntimeSource.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("Runtime does not generate activation legal actions"), RuntimeSource.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessBuildExternalDataTest, "Wandbound.Runtime.ActivationSessionHarness.BuildExternalData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessBuildExternalDataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	const TArray<FWBCardActivationCandidateSource> Sources = MakeSingleActivationSources(State);
	const FWBActivationSessionExternalDataForTest ExternalData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(State, 0, Sources);

	TestTrue(TEXT("Normal legal actions generated by test harness"), ExternalData.NormalLegalActions.Num() > 0);
	TestTrue(TEXT("Activation actions generated by test harness"), ExternalData.ActivationActionSet.Actions.Num() > 0);
	TestEqual(TEXT("Public summary unit count"), ExternalData.PublicBoardSummary.Units.Num(), State.Units.Num());
	TestEqual(TEXT("First activation label"), ExternalData.ActivationActionSet.Actions[0].PublicLabel, FString(TEXT("Harness Strike")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessOneDecisionTest, "Wandbound.Runtime.ActivationSessionHarness.OneDecisionRefreshExecuteRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessOneDecisionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const TArray<FWBCardActivationCandidateSource> Sources = MakeSingleActivationSources(State);
	const FWBActivationSessionHarnessStepResult Step =
		FWBActivationSessionTestHarness::RefreshAndExecuteFirstActivationWithLabel(
			State,
			0,
			Sources,
			TEXT("Harness Strike"),
			Facade);

	TestTrue(TEXT("Initial refresh ok"), Step.bRefreshOk);
	TestTrue(TEXT("Execution ok"), Step.bExecutionOk);
	TestFalse(TEXT("Selected activation id captured"), Step.SelectedActivationActionId.IsEmpty());
	TestEqual(TEXT("Target damaged"), State.GetUnitById(HarnessTargetUnitId)->HP, 5);
	TestEqual(TEXT("Status normal count from post data"), Step.ExecutionResult.Status.NormalLegalActionCount, Step.PostActionExternalData.NormalLegalActions.Num());
	TestEqual(TEXT("Status activation count from post data"), Step.ExecutionResult.Status.ActivationActionCount, Step.PostActionExternalData.ActivationActionSet.Actions.Num());
	TestTrue(TEXT("Activation model refreshed"), ActivationModel->HasCurrentActivationPresentationState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessTwoDecisionsTest, "Wandbound.Runtime.ActivationSessionHarness.TwoConsecutiveDecisionSessions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessTwoDecisionsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const TArray<FWBCardActivationCandidateSource> Sources = MakeTwoActivationSources(State);
	const FWBActivationSessionHarnessStepResult FirstStep =
		FWBActivationSessionTestHarness::RefreshAndExecuteFirstActivationWithLabel(
			State,
			0,
			Sources,
			TEXT("First Strike"),
			Facade);
	TestTrue(TEXT("First execution ok"), FirstStep.bExecutionOk);

	const FWBActivationSessionHarnessStepResult SecondStep =
		FWBActivationSessionTestHarness::RefreshAndExecuteFirstActivationWithLabel(
			State,
			0,
			Sources,
			TEXT("Second Strike"),
			Facade);

	TestTrue(TEXT("Second initial refresh ok"), SecondStep.bRefreshOk);
	TestTrue(TEXT("Second execution ok"), SecondStep.bExecutionOk);
	TestNotEqual(TEXT("Second selection is distinct"), SecondStep.SelectedActivationActionId, FirstStep.SelectedActivationActionId);
	TestEqual(TEXT("Target damaged twice across sessions"), State.GetUnitById(HarnessTargetUnitId)->HP, 4);
	TestEqual(TEXT("Second target remains untouched by first-target generator order"), State.GetUnitById(HarnessSecondTargetUnitId)->HP, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessOncePerTurnRefreshTest, "Wandbound.Runtime.ActivationSessionHarness.OncePerTurnRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessOncePerTurnRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const TArray<FWBCardActivationCandidateSource> Sources =
		MakeSingleActivationSources(State, TEXT("Once Strike"), true);
	const FWBActivationSessionExternalDataForTest BeforeData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(State, 0, Sources);
	TestEqual(TEXT("Before action count"), BeforeData.ActivationActionSet.Actions.Num(), 1);

	const FWBActivationSessionHarnessStepResult Step =
		FWBActivationSessionTestHarness::RefreshAndExecuteFirstActivationWithLabel(
			State,
			0,
			Sources,
			TEXT("Once Strike"),
			Facade);

	TestTrue(TEXT("Execution ok"), Step.bExecutionOk);
	TestTrue(TEXT("Usage key marked"), State.HasActivationUsageKeyThisTurn(0, TEXT("runtime_activation_session_once")));
	TestEqual(TEXT("Post refresh removes once action"), Step.ExecutionResult.Status.ActivationActionCount, 0);

	const FWBApplyActionResult TurnStartResult = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 4);
	TestTrue(TEXT("Turn start setup ok"), TurnStartResult.bOk);
	const FWBActivationSessionExternalDataForTest NextTurnData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(State, 0, Sources);
	TestEqual(TEXT("Action reappears after usage reset"), NextTurnData.ActivationActionSet.Actions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessCostRefreshTest, "Wandbound.Runtime.ActivationSessionHarness.CostRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessCostRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState(3, 0);
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const TArray<FWBCardActivationCandidateSource> BeforeSources =
		MakeSingleActivationSources(State, TEXT("Cost Strike"), false, 3);
	const FWBActivationSessionExternalDataForTest BeforeData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(State, 0, BeforeSources);
	TestEqual(TEXT("Cost action affordable before execution"), BeforeData.ActivationActionSet.Actions.Num(), 1);
	TestTrue(TEXT("Initial refresh ok"), Facade->RefreshSessionFromExternalData(ToRefreshInput(BeforeData)).bOk);

	FString SelectedActivationActionId;
	TestTrue(
		TEXT("Find cost action"),
		FWBActivationSessionTestHarness::FindFirstActivationActionIdWithLabel(
			BeforeData.ActivationActionSet,
			TEXT("Cost Strike"),
			SelectedActivationActionId));

	FWBGameStateData PredictedState = State;
	const FWBRuntimeActivationExecutionHandoffResult Handoff =
		Facade->CreateActivationExecutionHandoff(SelectedActivationActionId);
	TestTrue(
		TEXT("Preview execution ok"),
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(PredictedState, Handoff).bOk);
	const TArray<FWBCardActivationCandidateSource> AfterSources =
		MakeSingleActivationSources(PredictedState, TEXT("Cost Strike"), false, 3);
	const FWBActivationSessionExternalDataForTest AfterData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(PredictedState, 0, AfterSources);

	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			SelectedActivationActionId,
			ToRefreshInput(AfterData));

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestEqual(TEXT("RLUsed paid by core"), State.GetUnitById(HarnessSourceUnitId)->RLUsed, 3);
	TestEqual(TEXT("Post refresh removes unaffordable action"), Result.Status.ActivationActionCount, 0);
	TestEqual(TEXT("After external action count"), AfterData.ActivationActionSet.Actions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessStaleExplicitRefreshTest, "Wandbound.Runtime.ActivationSessionHarness.StaleUntilExplicitRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessStaleExplicitRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const TArray<FWBCardActivationCandidateSource> Sources =
		MakeSingleActivationSources(State, TEXT("Stale Strike"));
	const FWBActivationSessionExternalDataForTest BeforeData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(State, 0, Sources);
	TestTrue(TEXT("Initial refresh ok"), Facade->RefreshSessionFromExternalData(ToRefreshInput(BeforeData)).bOk);
	const int32 InitialPublicUnitCount = Owner->GetCurrentStatus().PublicUnitCount;

	FString SelectedActivationActionId;
	TestTrue(
		TEXT("Find stale action"),
		FWBActivationSessionTestHarness::FindFirstActivationActionIdWithLabel(
			BeforeData.ActivationActionSet,
			TEXT("Stale Strike"),
			SelectedActivationActionId));

	FWBActivationSessionExternalDataForTest PreviewData;
	TestTrue(TEXT("Preview post data"), PreviewPostActionData(State, SelectedActivationActionId, Sources, Facade, PreviewData));
	PreviewData.PublicBoardSummary.Units.Pop();

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnSuccess = false;
	Options.bRefreshActivationPresentationOnSuccess = false;
	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			SelectedActivationActionId,
			ToRefreshInput(PreviewData),
			Options);

	TestTrue(TEXT("Execution ok"), Result.bOk);
	TestFalse(TEXT("No post refresh attempted"), Result.PostActivationRefreshResult.bPostActionRefreshAttempted);
	TestEqual(TEXT("Public count remains stale"), Owner->GetCurrentStatus().PublicUnitCount, InitialPublicUnitCount);

	const FWBRuntimeActivationDecisionSessionRefreshResult ExplicitRefresh =
		Facade->RefreshSessionFromExternalData(ToRefreshInput(PreviewData));
	TestTrue(TEXT("Explicit refresh ok"), ExplicitRefresh.bOk);
	TestEqual(TEXT("Explicit refresh replaces public count"), Owner->GetCurrentStatus().PublicUnitCount, PreviewData.PublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessFailurePoliciesTest, "Wandbound.Runtime.ActivationSessionHarness.FailureRefreshPolicies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessFailurePoliciesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const TArray<FWBCardActivationCandidateSource> Sources =
		MakeSingleActivationSources(State, TEXT("Failure Strike"));
	const FWBActivationSessionExternalDataForTest BeforeData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(State, 0, Sources);
	TestTrue(TEXT("Initial refresh ok"), Facade->RefreshSessionFromExternalData(ToRefreshInput(BeforeData)).bOk);
	const FWBRuntimeActivationDecisionSessionStatus BeforeStatus = Facade->GetCurrentStatus();

	FWBActivationSessionExternalDataForTest EmptyPostData = BeforeData;
	EmptyPostData.NormalLegalActions.Reset();
	EmptyPostData.ActivationActionSet.Actions.Reset();
	EmptyPostData.PublicBoardSummary.Units.Pop();

	FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			TEXT("activate:missing"),
			ToRefreshInput(EmptyPostData));
	TestFalse(TEXT("Missing action fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("activation_action_id_not_found")));
	TestFalse(TEXT("No refresh by default"), Result.PostActivationRefreshResult.bPostActionRefreshAttempted);
	TestEqual(TEXT("Normal count unchanged"), Result.Status.NormalLegalActionCount, BeforeStatus.NormalLegalActionCount);
	TestEqual(TEXT("Activation count unchanged"), Result.Status.ActivationActionCount, BeforeStatus.ActivationActionCount);

	FWBRuntimePostActivationRefreshOptions Options;
	Options.bRefreshNormalDecisionPointOnFailure = true;
	Options.bRefreshActivationPresentationOnFailure = true;
	Result = Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
		State,
		TEXT("activate:missing"),
		ToRefreshInput(EmptyPostData),
		Options);

	TestFalse(TEXT("Failure still fails"), Result.bOk);
	TestTrue(TEXT("Failure refresh attempted"), Result.PostActivationRefreshResult.bPostActionRefreshAttempted);
	TestEqual(TEXT("Normal count from failure refresh"), Result.Status.NormalLegalActionCount, 0);
	TestEqual(TEXT("Activation count from failure refresh"), Result.Status.ActivationActionCount, 0);
	TestEqual(TEXT("Public count from failure refresh"), Owner->GetCurrentStatus().PublicUnitCount, EmptyPostData.PublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessHiddenMetadataTest, "Wandbound.Runtime.ActivationSessionHarness.HiddenMetadataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	FWBActivationSessionExternalDataForTest BeforeData =
		FWBActivationSessionTestHarness::BuildExternalDataForTest(
			State,
			0,
			MakeSingleActivationSources(State, TEXT("Safe Session Strike")));
	TestEqual(TEXT("One activation generated"), BeforeData.ActivationActionSet.Actions.Num(), 1);

	const FString SecretSourceEffect = TEXT("SECRET_HARNESS_SOURCE_EFFECT_DO_NOT_LEAK");
	const FString SecretUsageKey = TEXT("SECRET_HARNESS_USAGE_KEY_DO_NOT_LEAK");
	const FString SecretDebugId = TEXT("SECRET_HARNESS_DEBUG_ACTIVATION_DO_NOT_LEAK");
	const FString SecretCostKind = TEXT("SECRET_HARNESS_COST_KIND_DO_NOT_LEAK");
	FWBCardActivationLegalAction& Action = BeforeData.ActivationActionSet.Actions[0];
	Action.PublicLabel = TEXT("Safe Session Strike");
	Action.Command.Source.SourceEffectId = SecretSourceEffect;
	Action.Command.EffectRequest.Source.SourceEffectId = SecretSourceEffect;
	Action.Command.DebugActivationId = SecretDebugId;
	Action.Command.UsageCommit.bMarkUsageOnSuccess = true;
	Action.Command.UsageCommit.PlayerId = 0;
	Action.Command.UsageCommit.UsageKey = SecretUsageKey;
	Action.Command.CostPaymentCommit.CostKind = FName(*SecretCostKind);

	TestTrue(TEXT("Initial refresh ok"), Facade->RefreshSessionFromExternalData(ToRefreshInput(BeforeData)).bOk);
	FWBActivationSessionExternalDataForTest PostData = BeforeData;
	PostData.ActivationActionSet.Actions.Reset();
	const FWBRuntimeActivationDecisionSessionExecutionResult Result =
		Facade->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			Action.ActivationActionId,
			ToRefreshInput(PostData));

	TestTrue(TEXT("Execution ok"), Result.bOk);
	const FString PublicText = CombineActivationPublicText(ActivationModel);
	const FString SerializedTrace = WBReplayTrace::SerializeEvents(
		Result.PostActivationRefreshResult.ActivationExecutionResult.ActivationResult.TraceEvents);
	const FString ReasonText = Result.Reason
		+ Result.Status.LastReason
		+ Result.SelectionResult.Reason
		+ Result.HandoffResult.Reason;

	for (const FString& Token : { SecretSourceEffect, SecretUsageKey, SecretDebugId, SecretCostKind })
	{
		TestFalse(*FString::Printf(TEXT("Public text excludes %s"), *Token), PublicText.Contains(Token, ESearchCase::IgnoreCase));
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Token), SerializedTrace.Contains(Token, ESearchCase::IgnoreCase));
		TestFalse(*FString::Printf(TEXT("Reason excludes %s"), *Token), ReasonText.Contains(Token, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("Usage key is internal only"), State.HasActivationUsageKeyThisTurn(0, SecretUsageKey));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationSessionHarnessActionBoundaryTest, "Wandbound.Runtime.ActivationSessionHarness.ActivationSeparateFromFWBAction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationSessionHarnessActionBoundaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	const TArray<FString> BeforeActionIds = NormalActionIdsForHarnessState(State);
	const FString MoveActionIdBefore = BeforeActionIds.Num() > 0 ? BeforeActionIds[0] : FString();

	UWBRuntimeActivationDecisionSessionFacadeComponent* Facade = nullptr;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* TurnModel = nullptr;
	UWBRuntimeActivationPresentationModelComponent* ActivationModel = nullptr;
	TestTrue(TEXT("Session path"), MakeHarnessSessionPath(Facade, Owner, Coordinator, TurnModel, ActivationModel));

	const FWBActivationSessionHarnessStepResult Step =
		FWBActivationSessionTestHarness::RefreshAndExecuteFirstActivationWithLabel(
			State,
			0,
			MakeSingleActivationSources(State, TEXT("Boundary Strike")),
			TEXT("Boundary Strike"),
			Facade);

	TestTrue(TEXT("Execution ok"), Step.bExecutionOk);
	const TArray<FString> AfterActionIds = NormalActionIdsForHarnessState(State);
	TestEqual(TEXT("Normal legal action count unchanged by activation family"), AfterActionIds.Num(), BeforeActionIds.Num());
	for (const FString& ActionId : AfterActionIds)
	{
		TestFalse(TEXT("Normal action id is not activation"), ActionId.StartsWith(TEXT("activate:")));
	}
	TestEqual(TEXT("WBActionCodec move id unchanged"), AfterActionIds.Num() > 0 ? AfterActionIds[0] : FString(), MoveActionIdBefore);

	FString RuntimeSource;
	TestTrue(TEXT("Runtime source loads"), LoadRuntimeSourceText(RuntimeSource));
	TestFalse(TEXT("Runtime does not add activation action type"), RuntimeSource.Contains(TEXT("EWBActionType::Activate")));
	TestFalse(TEXT("Runtime does not modify WBActionCodec for activation"), RuntimeSource.Contains(TEXT("WBActionCodec::MakeActivation")));
	return true;
}

#endif
