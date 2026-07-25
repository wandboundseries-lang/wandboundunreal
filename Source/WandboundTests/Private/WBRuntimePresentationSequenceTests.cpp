#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimePresentationSequenceComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBRuntimePresentationEvent MakeEvent(
	const EWBRuntimePresentationEventType Type,
	const int32 SequenceIndex,
	const float Duration = 1.0f)
{
	FWBRuntimePresentationEvent Event;
	Event.Type = Type;
	Event.SequenceIndex = SequenceIndex;
	Event.SuggestedDurationSeconds = Duration;
	return Event;
}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSequenceEmptyTest,
	"Wandbound.Runtime.Presentation.Sequence.EmptyQueueCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSequenceEmptyTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationSequenceComponent* Sequence =
		NewObject<UWBRuntimePresentationSequenceComponent>(GetTransientPackage());
	TestTrue(TEXT("Empty enqueue succeeds"), Sequence->EnqueueEvents({}, 1, 2).bOk);
	TestTrue(TEXT("Empty begin succeeds"), Sequence->BeginSequence().bOk);
	TestEqual(TEXT("Sequence completed"), Sequence->GetPlaybackState(), EWBRuntimePresentationPlaybackState::Completed);
	TestFalse(TEXT("Sequence inactive"), Sequence->IsSequenceActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSequenceOrderTest,
	"Wandbound.Runtime.Presentation.Sequence.AdvancesInOrderWithStableCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSequenceOrderTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationSequenceComponent* Sequence =
		NewObject<UWBRuntimePresentationSequenceComponent>(GetTransientPackage());
	const TArray<FWBRuntimePresentationEvent> Events = {
		MakeEvent(EWBRuntimePresentationEventType::UnitMoved, 0),
		MakeEvent(EWBRuntimePresentationEventType::AttackDeclared, 1),
		MakeEvent(EWBRuntimePresentationEventType::AttackImpact, 2)
	};
	TestTrue(TEXT("Enqueue succeeds"), Sequence->EnqueueEvents(Events, 2, 4).bOk);
	TestEqual(TEXT("All pending before begin"), Sequence->GetPendingEventCount(), 3);
	TestTrue(TEXT("Begin succeeds"), Sequence->BeginSequence().bOk);
	TestEqual(TEXT("First event current"), Sequence->GetCurrentEvent().SequenceIndex, 0);
	TestEqual(TEXT("Two pending"), Sequence->GetPendingEventCount(), 2);
	TestTrue(TEXT("Advance succeeds"), Sequence->AdvanceSequence().bOk);
	TestEqual(TEXT("Second event current"), Sequence->GetCurrentEvent().SequenceIndex, 1);
	TestEqual(TEXT("One pending"), Sequence->GetPendingEventCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSequenceSkipCurrentTest,
	"Wandbound.Runtime.Presentation.Sequence.SkipCurrentAdvancesExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSequenceSkipCurrentTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationSequenceComponent* Sequence =
		NewObject<UWBRuntimePresentationSequenceComponent>(GetTransientPackage());
	Sequence->EnqueueEvents({
		MakeEvent(EWBRuntimePresentationEventType::UnitMoved, 0),
		MakeEvent(EWBRuntimePresentationEventType::DamageApplied, 1)
	}, 1, 1);
	Sequence->BeginSequence();
	TestTrue(TEXT("Skip succeeds"), Sequence->SkipCurrentEvent().bOk);
	TestEqual(TEXT("Second event current"), Sequence->GetCurrentEvent().SequenceIndex, 1);
	TestEqual(TEXT("No pending"), Sequence->GetPendingEventCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSequenceSkipAllTest,
	"Wandbound.Runtime.Presentation.Sequence.SkipAllCompletesAndClearsActivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSequenceSkipAllTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationSequenceComponent* Sequence =
		NewObject<UWBRuntimePresentationSequenceComponent>(GetTransientPackage());
	Sequence->EnqueueEvents({
		MakeEvent(EWBRuntimePresentationEventType::UnitMoved, 0),
		MakeEvent(EWBRuntimePresentationEventType::GameOver, 1)
	}, 1, 1);
	Sequence->BeginSequence();
	TestTrue(TEXT("Skip all succeeds"), Sequence->SkipAll().bOk);
	TestFalse(TEXT("Sequence inactive"), Sequence->IsSequenceActive());
	TestEqual(TEXT("Sequence completed"), Sequence->GetPlaybackState(), EWBRuntimePresentationPlaybackState::Completed);
	TestEqual(TEXT("No pending"), Sequence->GetPendingEventCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSequenceCancelTest,
	"Wandbound.Runtime.Presentation.Sequence.CancelIsIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSequenceCancelTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationSequenceComponent* Sequence =
		NewObject<UWBRuntimePresentationSequenceComponent>(GetTransientPackage());
	Sequence->EnqueueEvents({ MakeEvent(EWBRuntimePresentationEventType::UnitMoved, 0) }, 1, 1);
	Sequence->BeginSequence();
	Sequence->CancelSequence();
	Sequence->CancelSequence();
	TestFalse(TEXT("Sequence inactive"), Sequence->IsSequenceActive());
	TestEqual(TEXT("Cancelled state"), Sequence->GetPlaybackState(), EWBRuntimePresentationPlaybackState::Cancelled);
	TestEqual(TEXT("Queue cleared"), Sequence->GetPendingEventCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSequenceGenerationTest,
	"Wandbound.Runtime.Presentation.Sequence.StaleGenerationAndDuplicateBeginRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSequenceGenerationTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationSequenceComponent* Sequence =
		NewObject<UWBRuntimePresentationSequenceComponent>(GetTransientPackage());
	Sequence->EnqueueEvents({ MakeEvent(EWBRuntimePresentationEventType::UnitMoved, 0) }, 3, 8);
	Sequence->BeginSequence();
	TestFalse(TEXT("Duplicate begin rejected"), Sequence->BeginSequence().bOk);
	Sequence->CancelSequence();
	const FWBRuntimePresentationSequenceResult Stale =
		Sequence->EnqueueEvents({}, 2, 1);
	TestFalse(TEXT("Stale generation rejected"), Stale.bOk);
	TestEqual(TEXT("Stale reason"), Stale.Reason, FString(TEXT("stale_match_generation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSequenceSpeedTest,
	"Wandbound.Runtime.Presentation.Sequence.SpeedChangesTimingOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSequenceSpeedTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationSequenceComponent* Sequence =
		NewObject<UWBRuntimePresentationSequenceComponent>(GetTransientPackage());
	const FWBRuntimePresentationEvent Event = MakeEvent(EWBRuntimePresentationEventType::DamageApplied, 17, 0.4f);
	Sequence->EnqueueEvents({ Event }, 1, 1);
	Sequence->SetPlaybackSpeed(2.0f);
	TestEqual(TEXT("Speed retained"), Sequence->GetPlaybackSpeed(), 2.0f);
	TestEqual(TEXT("Event unchanged"), Sequence->GetCurrentEvent().SequenceIndex, INDEX_NONE);
	Sequence->SetPlaybackSpeed(0.0f);
	TestTrue(TEXT("Instant begin succeeds"), Sequence->BeginSequence().bOk);
	TestEqual(TEXT("Instant playback completes"), Sequence->GetPlaybackState(), EWBRuntimePresentationPlaybackState::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationHostInputLockTest,
	"Wandbound.Runtime.Presentation.Host.ActionSubmissionLocksUntilSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationHostInputLockTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host =
		NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	TestTrue(TEXT("Development match initializes"), Host->InitializeDevelopmentMatch(0, false).bOk);
	Host->SetPresentationPlaybackSpeed(1.0f);
	const TArray<FWBRuntimeLegalActionPresentation> InitialActions = Host->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* Move =
		InitialActions.FindByPredicate([](const FWBRuntimeLegalActionPresentation& Action)
		{
			return Action.Family == EWBRuntimeMatchActionFamily::Move;
		});
	TestNotNull(TEXT("Move exists"), Move);
	if (Move == nullptr) return false;
	const FString ActionId = Move->ActionId;
	const int32 OldGeneration = Move->MatchGeneration;
	const int32 OldRevision = Move->DecisionRevision;
	const FWBRuntimeMatchCommandResult Submit =
		Host->SubmitLegalActionAtRevision(ActionId, OldGeneration, OldRevision);
	TestTrue(TEXT("Move accepted"), Submit.bOk);
	TestTrue(TEXT("Presentation active"), Host->IsPresentationSequenceActive());
	const FWBRuntimeMatchCommandResult Locked =
		Host->SubmitLegalActionAtRevision(ActionId, OldGeneration, OldRevision);
	TestFalse(TEXT("Second submit rejected"), Locked.bOk);
	TestEqual(TEXT("Typed lock reason"), Locked.Reason, FString(TEXT("presentation_sequence_active")));
	TestTrue(TEXT("Skip all succeeds"), Host->SkipAllPresentationEvents().bOk);
	TestFalse(TEXT("Presentation unlocks"), Host->IsPresentationSequenceActive());
	const FWBRuntimeMatchCommandResult Stale =
		Host->SubmitLegalActionAtRevision(ActionId, OldGeneration, OldRevision);
	TestEqual(TEXT("Old action is stale after playback"), Stale.Reason, FString(TEXT("stale_decision_revision")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationHostSkipStateTest,
	"Wandbound.Runtime.Presentation.Host.SkipDoesNotChangeCommittedCoreState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationHostSkipStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host =
		NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	TestTrue(TEXT("Development match initializes"), Host->InitializeDevelopmentMatch(0, false).bOk);
	const TArray<FWBRuntimeLegalActionPresentation> InitialActions = Host->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* Move =
		InitialActions.FindByPredicate([](const FWBRuntimeLegalActionPresentation& Action)
		{
			return Action.Family == EWBRuntimeMatchActionFamily::Move;
		});
	if (Move == nullptr) return false;
	const int32 UnitId = Move->SourceUnitId;
	const FIntPoint ExpectedTile = Move->TargetTile;
	Host->SetPresentationPlaybackSpeed(1.0f);
	TestTrue(TEXT("Move accepted"), Host->SubmitLegalActionById(Move->ActionId).bOk);
	const FWBRuntimeUnitPresentation* CommittedUnit =
		Host->GetCurrentUnits().FindByPredicate([UnitId](const FWBRuntimeUnitPresentation& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	TestNotNull(TEXT("Committed unit remains visible"), CommittedUnit);
	if (CommittedUnit == nullptr) return false;
	TestEqual(TEXT("Final observation already committed"), CommittedUnit->Tile, ExpectedTile);
	TestTrue(TEXT("Skip succeeds"), Host->SkipAllPresentationEvents().bOk);
	const FWBRuntimeUnitPresentation* AfterSkip =
		Host->GetCurrentUnits().FindByPredicate([UnitId](const FWBRuntimeUnitPresentation& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	TestNotNull(TEXT("Unit remains after skip"), AfterSkip);
	if (AfterSkip != nullptr) TestEqual(TEXT("Skip leaves state unchanged"), AfterSkip->Tile, ExpectedTile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSourceGuardTest,
	"Wandbound.Runtime.Presentation.SourceGuards.NoGameplayAuthorityOrHiddenZoneInspection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const TArray<FString> Files = {
		Root / TEXT("Source/WandboundRuntime/Private/WBRuntimeTracePresentationTranslator.cpp"),
		Root / TEXT("Source/WandboundRuntime/Private/WBRuntimePresentationSequenceComponent.cpp")
	};
	for (const FString& File : Files)
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("Reads %s"), *File), FFileHelper::LoadFileToString(Source, *File));
		TestFalse(TEXT("No rules executor"), Source.Contains(TEXT("WBRules")));
		TestFalse(TEXT("No effect runner"), Source.Contains(TEXT("WBEffectRunner")));
		TestFalse(TEXT("No game state ownership"), Source.Contains(TEXT("FWBGameStateData")));
		TestFalse(TEXT("No hidden zone inspection"), Source.Contains(TEXT("CardZoneState")));
		TestFalse(TEXT("No gameplay RNG"), Source.Contains(TEXT("RandomStream")));
	}
	return true;
}

#endif
