#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBRuntimeActivationPresentationModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 RuntimeActivationSourceUnitId = 1;
constexpr int32 RuntimeActivationTargetUnitId = 2;

UWBRuntimeActivationPresentationModelComponent* MakeRuntimeActivationModel()
{
	return NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
}

FWBEffectTargetRef MakeRuntimeUnitTarget(const int32 UnitId = RuntimeActivationTargetUnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBEffectTargetRef MakeRuntimeTileTarget()
{
	FWBEffectTargetRef Target;
	Target.TargetTile = FWBTile(3, 4);
	return Target;
}

FWBCardActivationLegalAction MakeRuntimeActivationAction(
	const FString& ActionId,
	const FString& PublicLabel,
	const FWBEffectTargetRef& Target)
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActionId;
	Action.PublicLabel = PublicLabel;
	Action.PlayerId = 0;
	Action.SourceUnitId = RuntimeActivationSourceUnitId;
	Action.Target = Target;
	return Action;
}

FWBCardActivationLegalActionSet MakeRuntimeActivationActionSet()
{
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(MakeRuntimeActivationAction(
		TEXT("activate:p0:u1:cvisible_runtime_activation_source:edeal:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("Deal 1 damage"),
		MakeRuntimeUnitTarget()));
	Set.Actions.Add(MakeRuntimeActivationAction(
		TEXT("activate:p0:u1:cvisible_runtime_activation_source:eplace:x3:y4"),
		TEXT("Place Rune"),
		MakeRuntimeTileTarget()));
	return Set;
}

FWBPublicUnitBoardSummary MakeRuntimeActivationPublicUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId)
{
	FWBPublicUnitBoardSummary Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = UnitId == RuntimeActivationSourceUnitId ? 4 : 5;
	Unit.Y = 4;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	return Unit;
}

FWBPublicBoardSummary MakeRuntimeActivationPublicSummary()
{
	FWBPublicBoardSummary Summary;
	Summary.Units.Add(MakeRuntimeActivationPublicUnit(
		RuntimeActivationSourceUnitId,
		0,
		TEXT("visible_runtime_activation_source")));
	Summary.Units.Add(MakeRuntimeActivationPublicUnit(
		RuntimeActivationTargetUnitId,
		1,
		TEXT("visible_runtime_activation_target")));
	return Summary;
}

FString CombineRuntimeActivationPresentationText(
	const FWBCardActivationLegalActionPresentationSnapshot& ActivationSnapshot,
	const FWBCardActivationTargetPresentationSnapshot& TargetSnapshot)
{
	FString Text;
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : ActivationSnapshot.Entries)
	{
		Text += Entry.ActivationActionId;
		Text += Entry.PublicLabel;
		Text += Entry.SourcePublicCardId;
		Text += Entry.TargetPublicCardId;
	}
	for (const FWBCardActivationTargetPresentationEntry& Entry : TargetSnapshot.Entries)
	{
		Text += Entry.ActivationActionId;
		Text += Entry.PublicActivationLabel;
		Text += Entry.PublicTargetLabel;
		Text += Entry.SourcePublicCardId;
		Text += Entry.TargetPublicCardId;
	}
	return Text;
}

bool LoadRuntimeActivationModelSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Public"),
		TEXT("WBRuntimeActivationPresentationModelComponent.h"));
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Private"),
		TEXT("WBRuntimeActivationPresentationModelComponent.cpp"));

	FString Header;
	FString Source;
	if (!FFileHelper::LoadFileToString(Header, *HeaderPath)
		|| !FFileHelper::LoadFileToString(Source, *SourcePath))
	{
		return false;
	}

	OutSource = Header + Source;
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelClassExistsTest, "Wandbound.Runtime.ActivationPresentationModel.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Activation presentation model class"), UWBRuntimeActivationPresentationModelComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelInitialStateTest, "Wandbound.Runtime.ActivationPresentationModel.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelInitialStateTest::RunTest(const FString& Parameters)
{
	const UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBRuntimeActivationPresentationStatus Status = Model->GetCurrentActivationPresentationStatus();
	TestFalse(TEXT("No current activation presentation state"), Model->HasCurrentActivationPresentationState());
	TestFalse(TEXT("Status not ready"), Status.bReady);
	TestFalse(TEXT("No activation actions"), Status.bHasActivationActions);
	TestEqual(TEXT("Activation action count"), Status.ActivationActionCount, 0);
	TestEqual(TEXT("Activation presentation entry count"), Status.ActivationPresentationEntryCount, 0);
	TestEqual(TEXT("Target presentation entry count"), Status.TargetPresentationEntryCount, 0);
	TestEqual(TEXT("Stored actions empty"), Model->GetCurrentActivationActionSet().Actions.Num(), 0);
	TestEqual(TEXT("Activation snapshot empty"), Model->GetCurrentActivationPresentationSnapshot().Entries.Num(), 0);
	TestEqual(TEXT("Target snapshot empty"), Model->GetCurrentTargetPresentationSnapshot().Entries.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelBuildsSnapshotsTest, "Wandbound.Runtime.ActivationPresentationModel.SetStateBuildsSnapshots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelBuildsSnapshotsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBCardActivationLegalActionSet ActionSet = MakeRuntimeActivationActionSet();
	Model->SetCurrentActivationPresentationState(ActionSet, MakeRuntimeActivationPublicSummary());

	const FWBRuntimeActivationPresentationStatus Status = Model->GetCurrentActivationPresentationStatus();
	TestTrue(TEXT("Has activation presentation state"), Model->HasCurrentActivationPresentationState());
	TestTrue(TEXT("Status ready"), Status.bReady);
	TestTrue(TEXT("Has activation actions"), Status.bHasActivationActions);
	TestEqual(TEXT("Stored action count"), Model->GetCurrentActivationActionSet().Actions.Num(), ActionSet.Actions.Num());
	TestEqual(TEXT("Activation snapshot count"), Model->GetCurrentActivationPresentationSnapshot().Entries.Num(), ActionSet.Actions.Num());
	TestEqual(TEXT("Target snapshot count"), Model->GetCurrentTargetPresentationSnapshot().Entries.Num(), ActionSet.Actions.Num());
	TestEqual(TEXT("Status action count"), Status.ActivationActionCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Status activation entry count"), Status.ActivationPresentationEntryCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Status target entry count"), Status.TargetPresentationEntryCount, ActionSet.Actions.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelPreservesOrderTest, "Wandbound.Runtime.ActivationPresentationModel.PreservesOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelPreservesOrderTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	FWBCardActivationLegalActionSet ActionSet;
	ActionSet.Actions.Add(MakeRuntimeActivationAction(TEXT("activate:third"), TEXT("Third"), MakeRuntimeTileTarget()));
	ActionSet.Actions.Add(MakeRuntimeActivationAction(TEXT("activate:first"), TEXT("First"), MakeRuntimeUnitTarget()));
	ActionSet.Actions.Add(MakeRuntimeActivationAction(TEXT("activate:second"), TEXT("Second"), FWBEffectTargetRef()));
	Model->SetCurrentActivationPresentationState(ActionSet, MakeRuntimeActivationPublicSummary());

	for (int32 Index = 0; Index < ActionSet.Actions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Activation presentation order %d"), Index),
			Model->GetCurrentActivationPresentationSnapshot().Entries[Index].ActivationActionId,
			ActionSet.Actions[Index].ActivationActionId);
		TestEqual(
			*FString::Printf(TEXT("Target presentation order %d"), Index),
			Model->GetCurrentTargetPresentationSnapshot().Entries[Index].ActivationActionId,
			ActionSet.Actions[Index].ActivationActionId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelLookupTest, "Wandbound.Runtime.ActivationPresentationModel.TryFindActivationPresentationEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelLookupTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBCardActivationLegalActionSet ActionSet = MakeRuntimeActivationActionSet();
	Model->SetCurrentActivationPresentationState(ActionSet, MakeRuntimeActivationPublicSummary());

	FWBCardActivationLegalActionPresentationEntry Entry;
	TestTrue(TEXT("Find activation entry succeeds"), Model->TryFindActivationPresentationEntryByActionId(ActionSet.Actions[0].ActivationActionId, Entry));
	TestEqual(TEXT("Found activation id"), Entry.ActivationActionId, ActionSet.Actions[0].ActivationActionId);
	TestFalse(TEXT("Missing activation entry fails"), Model->TryFindActivationPresentationEntryByActionId(TEXT("missing_activation"), Entry));

	FWBCardActivationLegalActionSet DuplicateSet = ActionSet;
	DuplicateSet.Actions.Add(ActionSet.Actions[0]);
	Model->SetCurrentActivationPresentationState(DuplicateSet, MakeRuntimeActivationPublicSummary());
	TestFalse(TEXT("Duplicate activation entry fails"), Model->TryFindActivationPresentationEntryByActionId(ActionSet.Actions[0].ActivationActionId, Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelTargetLookupTest, "Wandbound.Runtime.ActivationPresentationModel.TryFindTargetPresentationEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelTargetLookupTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBCardActivationLegalActionSet ActionSet = MakeRuntimeActivationActionSet();
	Model->SetCurrentActivationPresentationState(ActionSet, MakeRuntimeActivationPublicSummary());

	FWBCardActivationTargetPresentationEntry Entry;
	TestTrue(TEXT("Find target entry succeeds"), Model->TryFindTargetPresentationEntryByActionId(ActionSet.Actions[0].ActivationActionId, Entry));
	TestEqual(TEXT("Found target id"), Entry.ActivationActionId, ActionSet.Actions[0].ActivationActionId);
	TestEqual(TEXT("Found target kind"), Entry.TargetKind, EWBCardActivationTargetPresentationKind::Unit);
	TestFalse(TEXT("Missing target entry fails"), Model->TryFindTargetPresentationEntryByActionId(TEXT("missing_activation"), Entry));

	FWBCardActivationLegalActionSet DuplicateSet = ActionSet;
	DuplicateSet.Actions.Add(ActionSet.Actions[0]);
	Model->SetCurrentActivationPresentationState(DuplicateSet, MakeRuntimeActivationPublicSummary());
	TestFalse(TEXT("Duplicate target entry fails"), Model->TryFindTargetPresentationEntryByActionId(ActionSet.Actions[0].ActivationActionId, Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelPublicCardIdsTest, "Wandbound.Runtime.ActivationPresentationModel.PublicCardIdsFromSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelPublicCardIdsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBCardActivationLegalActionSet ActionSet = MakeRuntimeActivationActionSet();
	FWBPublicBoardSummary Summary = MakeRuntimeActivationPublicSummary();
	Model->SetCurrentActivationPresentationState(ActionSet, Summary);

	TestEqual(TEXT("Activation source public card"), Model->GetCurrentActivationPresentationSnapshot().Entries[0].SourcePublicCardId, FString(TEXT("visible_runtime_activation_source")));
	TestEqual(TEXT("Activation target public card"), Model->GetCurrentActivationPresentationSnapshot().Entries[0].TargetPublicCardId, FString(TEXT("visible_runtime_activation_target")));
	TestEqual(TEXT("Target source public card"), Model->GetCurrentTargetPresentationSnapshot().Entries[0].SourcePublicCardId, FString(TEXT("visible_runtime_activation_source")));
	TestEqual(TEXT("Target target public card"), Model->GetCurrentTargetPresentationSnapshot().Entries[0].TargetPublicCardId, FString(TEXT("visible_runtime_activation_target")));

	Summary.Units.RemoveAll(
		[](const FWBPublicUnitBoardSummary& Unit)
		{
			return Unit.UnitId == RuntimeActivationTargetUnitId;
		});
	Model->SetCurrentActivationPresentationState(ActionSet, Summary);

	TestFalse(TEXT("Activation missing target flag"), Model->GetCurrentActivationPresentationSnapshot().Entries[0].bHasPublicTargetUnit);
	TestTrue(TEXT("Activation missing target card empty"), Model->GetCurrentActivationPresentationSnapshot().Entries[0].TargetPublicCardId.IsEmpty());
	TestFalse(TEXT("Target missing target flag"), Model->GetCurrentTargetPresentationSnapshot().Entries[0].bHasPublicTargetUnit);
	TestTrue(TEXT("Target missing target card empty"), Model->GetCurrentTargetPresentationSnapshot().Entries[0].TargetPublicCardId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelHiddenMetadataTest, "Wandbound.Runtime.ActivationPresentationModel.HiddenMetadataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelHiddenMetadataTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	FWBCardActivationLegalAction Action = MakeRuntimeActivationAction(
		TEXT("activate:hidden"),
		TEXT("Reveal Safe Label"),
		MakeRuntimeUnitTarget());
	Action.Candidate.SourceCardId = TEXT("SECRET_RUNTIME_ACTIVATION_SOURCE_CARD");
	Action.Candidate.SourceEffectId = TEXT("SECRET_RUNTIME_ACTIVATION_SOURCE_EFFECT");
	Action.Command.DebugActivationId = TEXT("SECRET_RUNTIME_ACTIVATION_DEBUG");
	Action.Command.UsageCommit.UsageKey = TEXT("SECRET_RUNTIME_ACTIVATION_USAGE");
	Action.Command.CostPaymentCommit.CostKind = FName(TEXT("SECRET_RUNTIME_ACTIVATION_COST"));
	FWBCardActivationLegalActionSet ActionSet;
	ActionSet.Actions.Add(Action);

	Model->SetCurrentActivationPresentationState(ActionSet, MakeRuntimeActivationPublicSummary());

	const FString PublicText = CombineRuntimeActivationPresentationText(
		Model->GetCurrentActivationPresentationSnapshot(),
		Model->GetCurrentTargetPresentationSnapshot());
	const TArray<FString> Forbidden = {
		TEXT("SECRET_RUNTIME_ACTIVATION_SOURCE_CARD"),
		TEXT("SECRET_RUNTIME_ACTIVATION_SOURCE_EFFECT"),
		TEXT("SECRET_RUNTIME_ACTIVATION_DEBUG"),
		TEXT("SECRET_RUNTIME_ACTIVATION_USAGE"),
		TEXT("SECRET_RUNTIME_ACTIVATION_COST"),
		TEXT("EffectRunner"),
		TEXT("WBRules"),
		TEXT("schema"),
		TEXT("hook")
	};

	for (const FString& Token : Forbidden)
	{
		TestFalse(*FString::Printf(TEXT("Public text excludes %s"), *Token), PublicText.Contains(Token, ESearchCase::IgnoreCase));
	}
	TestTrue(TEXT("Visible source card present"), PublicText.Contains(TEXT("visible_runtime_activation_source")));
	TestTrue(TEXT("Visible target card present"), PublicText.Contains(TEXT("visible_runtime_activation_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelClearTest, "Wandbound.Runtime.ActivationPresentationModel.Clear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelClearTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model = MakeRuntimeActivationModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	Model->SetCurrentActivationPresentationState(
		MakeRuntimeActivationActionSet(),
		MakeRuntimeActivationPublicSummary());
	TestTrue(TEXT("State exists before clear"), Model->HasCurrentActivationPresentationState());

	Model->ClearCurrentActivationPresentationState();
	TestFalse(TEXT("State cleared"), Model->HasCurrentActivationPresentationState());
	TestEqual(TEXT("Actions cleared"), Model->GetCurrentActivationActionSet().Actions.Num(), 0);
	TestEqual(TEXT("Activation snapshot cleared"), Model->GetCurrentActivationPresentationSnapshot().Entries.Num(), 0);
	TestEqual(TEXT("Target snapshot cleared"), Model->GetCurrentTargetPresentationSnapshot().Entries.Num(), 0);
	TestFalse(TEXT("Status reset"), Model->GetCurrentActivationPresentationStatus().bReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationModelSourceGuardTest, "Wandbound.Runtime.ActivationPresentationModel.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationModelSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Activation model source loads"), LoadRuntimeActivationModelSource(Source));
	TestTrue(TEXT("API accepts public board summary"), Source.Contains(TEXT("FWBPublicBoardSummary")));
	TestFalse(TEXT("Does not inspect game state"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("Does not generate candidates"), Source.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("Does not generate activation legal actions"), Source.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("Does not call WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not call WBEffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Does not call WBActionCodec"), Source.Contains(TEXT("WBActionCodec")));
	return true;
}

#endif
