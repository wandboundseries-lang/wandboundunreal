#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeActivationPresentationRefreshAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardActivationLegalActionSet MakeRuntimeActivationRefreshActionSet()
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = 2;

	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = TEXT("activate:p0:u1:refresh:t2");
	Action.PublicLabel = TEXT("Refresh Damage");
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.Target = Target;

	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);
	return Set;
}

FWBPublicBoardSummary MakeRuntimeActivationRefreshSummary()
{
	FWBPublicBoardSummary Summary;

	FWBPublicUnitBoardSummary SourceUnit;
	SourceUnit.UnitId = 1;
	SourceUnit.OwnerId = 0;
	SourceUnit.CardId = TEXT("visible_refresh_source");
	SourceUnit.X = 4;
	SourceUnit.Y = 4;
	Summary.Units.Add(SourceUnit);

	FWBPublicUnitBoardSummary TargetUnit;
	TargetUnit.UnitId = 2;
	TargetUnit.OwnerId = 1;
	TargetUnit.CardId = TEXT("visible_refresh_target");
	TargetUnit.X = 5;
	TargetUnit.Y = 4;
	Summary.Units.Add(TargetUnit);

	return Summary;
}

bool LoadRuntimeActivationRefreshAdapterSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Public"),
		TEXT("WBRuntimeActivationPresentationRefreshAdapter.h"));
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Private"),
		TEXT("WBRuntimeActivationPresentationRefreshAdapter.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationRefreshAdapterNullModelTest, "Wandbound.Runtime.ActivationPresentationRefreshAdapter.NullModelFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationRefreshAdapterNullModelTest::RunTest(const FString& Parameters)
{
	const FWBCardActivationLegalActionSet ActionSet = MakeRuntimeActivationRefreshActionSet();
	const FWBRuntimeActivationPresentationRefreshResult Result =
		WBRuntimeActivationPresentationRefreshAdapter::RefreshActivationPresentation(
			ActionSet,
			MakeRuntimeActivationRefreshSummary(),
			nullptr);

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("activation_presentation_model_missing")));
	TestFalse(TEXT("Not refreshed"), Result.bActivationPresentationRefreshed);
	TestEqual(TEXT("Action count reported"), Result.ActivationActionCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Activation presentation count"), Result.ActivationPresentationEntryCount, 0);
	TestEqual(TEXT("Target presentation count"), Result.TargetPresentationEntryCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationRefreshAdapterSuccessTest, "Wandbound.Runtime.ActivationPresentationRefreshAdapter.Success", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationRefreshAdapterSuccessTest::RunTest(const FString& Parameters)
{
	UWBRuntimeActivationPresentationModelComponent* Model =
		NewObject<UWBRuntimeActivationPresentationModelComponent>(GetTransientPackage());
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBCardActivationLegalActionSet ActionSet = MakeRuntimeActivationRefreshActionSet();
	const FWBRuntimeActivationPresentationRefreshResult Result =
		WBRuntimeActivationPresentationRefreshAdapter::RefreshActivationPresentation(
			ActionSet,
			MakeRuntimeActivationRefreshSummary(),
			Model);

	TestTrue(TEXT("Refresh succeeds"), Result.bOk);
	TestTrue(TEXT("Refreshed"), Result.bActivationPresentationRefreshed);
	TestTrue(TEXT("Model has state"), Model->HasCurrentActivationPresentationState());
	TestEqual(TEXT("Action count"), Result.ActivationActionCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Activation presentation count"), Result.ActivationPresentationEntryCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Target presentation count"), Result.TargetPresentationEntryCount, ActionSet.Actions.Num());
	TestEqual(TEXT("Model action count"), Model->GetCurrentActivationActionSet().Actions.Num(), ActionSet.Actions.Num());
	TestEqual(TEXT("Model activation snapshot count"), Model->GetCurrentActivationPresentationSnapshot().Entries.Num(), ActionSet.Actions.Num());
	TestEqual(TEXT("Model target snapshot count"), Model->GetCurrentTargetPresentationSnapshot().Entries.Num(), ActionSet.Actions.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActivationPresentationRefreshAdapterSourceGuardTest, "Wandbound.Runtime.ActivationPresentationRefreshAdapter.SourceGuards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActivationPresentationRefreshAdapterSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Refresh adapter source loads"), LoadRuntimeActivationRefreshAdapterSource(Source));
	TestFalse(TEXT("Does not generate candidates"), Source.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("Does not generate activation legal actions"), Source.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("Does not call WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not call WBEffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Does not inspect game state"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("Does not execute activation commands"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	return true;
}

#endif
