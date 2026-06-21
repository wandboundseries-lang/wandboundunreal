#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBRuntimeLegalActionPresentation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBAction MakePresentationMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakePresentationAttackAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(6, 4);
	return Action;
}

FWBAction MakePresentationEndTurnAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = 0;
	return Action;
}

FWBAction MakePresentationPassAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Pass;
	Action.PlayerId = 0;
	return Action;
}

FWBAction MakePresentationPassResponseAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::PassResponse;
	Action.PlayerId = 1;
	return Action;
}

FWBPublicUnitBoardSummary MakePublicUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId)
{
	FWBPublicUnitBoardSummary Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.CardId = CardId;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	return Unit;
}

FWBPublicBoardSummary MakePresentationPublicSummary()
{
	FWBPublicBoardSummary Summary;
	Summary.Units.Add(MakePublicUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_presenter_player")));
	Summary.Units.Add(MakePublicUnit(2, 1, FWBTile(6, 4), TEXT("char_visible_presenter_target")));
	return Summary;
}

void ExpectEntryTile(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBTile& Actual,
	const FWBTile& Expected)
{
	Test.TestEqual(*FString::Printf(TEXT("%s x"), *Label), Actual.X, Expected.X);
	Test.TestEqual(*FString::Printf(TEXT("%s y"), *Label), Actual.Y, Expected.Y);
}

FString CombineEntryPublicText(const FWBRuntimeLegalActionPresentationSnapshot& Snapshot)
{
	FString Combined;
	for (const FWBRuntimeActionPresentationEntry& Entry : Snapshot.Entries)
	{
		Combined += Entry.ActionId;
		Combined += Entry.PublicLabel;
		Combined += Entry.SourcePublicCardId;
		Combined += Entry.TargetPublicCardId;
	}

	return Combined;
}

bool LoadPresentationSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeLegalActionPresentation.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeLegalActionPresentation.cpp"));

	FString Header;
	FString Source;
	if (!FFileHelper::LoadFileToString(Header, *HeaderPath) || !FFileHelper::LoadFileToString(Source, *SourcePath))
	{
		return false;
	}

	OutSource = Header + Source;
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationOrderTest, "Wandbound.Runtime.LegalActionPresentation.PreservesActionOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationOrderTest::RunTest(const FString& Parameters)
{
	const TArray<FWBAction> Actions = {
		MakePresentationEndTurnAction(),
		MakePresentationMoveAction(),
		MakePresentationAttackAction(),
		MakePresentationPassAction()
	};

	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot(Actions, MakePresentationPublicSummary());

	TestEqual(TEXT("Entry count"), Snapshot.Entries.Num(), Actions.Num());
	TestEqual(TEXT("Entry 0"), Snapshot.Entries[0].ActionId, WBActionCodec::MakeActionId(Actions[0]));
	TestEqual(TEXT("Entry 1"), Snapshot.Entries[1].ActionId, WBActionCodec::MakeActionId(Actions[1]));
	TestEqual(TEXT("Entry 2"), Snapshot.Entries[2].ActionId, WBActionCodec::MakeActionId(Actions[2]));
	TestEqual(TEXT("Entry 3"), Snapshot.Entries[3].ActionId, WBActionCodec::MakeActionId(Actions[3]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationMoveEntryTest, "Wandbound.Runtime.LegalActionPresentation.MoveEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationMoveEntryTest::RunTest(const FString& Parameters)
{
	const FWBAction MoveAction = MakePresentationMoveAction();
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ MoveAction }, MakePresentationPublicSummary());

	TestEqual(TEXT("Entry count"), Snapshot.Entries.Num(), 1);
	const FWBRuntimeActionPresentationEntry& Entry = Snapshot.Entries[0];
	TestEqual(TEXT("Action ID"), Entry.ActionId, WBActionCodec::MakeActionId(MoveAction));
	TestEqual(TEXT("Type"), static_cast<int32>(Entry.Type), static_cast<int32>(EWBRuntimeActionPresentationType::Move));
	TestEqual(TEXT("Label"), Entry.PublicLabel, FString(TEXT("Move")));
	TestEqual(TEXT("Player id"), Entry.PlayerId, 0);
	TestEqual(TEXT("Source unit id"), Entry.SourceUnitId, 1);
	ExpectEntryTile(*this, TEXT("From tile"), Entry.FromTile, FWBTile(4, 4));
	ExpectEntryTile(*this, TEXT("To tile"), Entry.ToTile, FWBTile(5, 4));
	TestTrue(TEXT("Source public unit found"), Entry.bHasSourcePublicUnit);
	TestEqual(TEXT("Source public card"), Entry.SourcePublicCardId, FString(TEXT("char_visible_presenter_player")));
	TestFalse(TEXT("No target public unit"), Entry.bHasTargetPublicUnit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationAttackEntryTest, "Wandbound.Runtime.LegalActionPresentation.AttackEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationAttackEntryTest::RunTest(const FString& Parameters)
{
	const FWBAction AttackAction = MakePresentationAttackAction();
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ AttackAction }, MakePresentationPublicSummary());

	TestEqual(TEXT("Entry count"), Snapshot.Entries.Num(), 1);
	const FWBRuntimeActionPresentationEntry& Entry = Snapshot.Entries[0];
	TestEqual(TEXT("Action ID"), Entry.ActionId, FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("Type"), static_cast<int32>(Entry.Type), static_cast<int32>(EWBRuntimeActionPresentationType::Attack));
	TestEqual(TEXT("Label"), Entry.PublicLabel, FString(TEXT("Attack")));
	TestEqual(TEXT("Source unit id"), Entry.SourceUnitId, 1);
	TestEqual(TEXT("Target unit id"), Entry.TargetUnitId, 2);
	TestTrue(TEXT("Source public unit found"), Entry.bHasSourcePublicUnit);
	TestTrue(TEXT("Target public unit found"), Entry.bHasTargetPublicUnit);
	TestEqual(TEXT("Source public card"), Entry.SourcePublicCardId, FString(TEXT("char_visible_presenter_player")));
	TestEqual(TEXT("Target public card"), Entry.TargetPublicCardId, FString(TEXT("char_visible_presenter_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationEndTurnEntryTest, "Wandbound.Runtime.LegalActionPresentation.EndTurnEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationEndTurnEntryTest::RunTest(const FString& Parameters)
{
	const FWBAction EndTurnAction = MakePresentationEndTurnAction();
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ EndTurnAction }, MakePresentationPublicSummary());

	const FWBRuntimeActionPresentationEntry& Entry = Snapshot.Entries[0];
	TestEqual(TEXT("Action ID"), Entry.ActionId, FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Type"), static_cast<int32>(Entry.Type), static_cast<int32>(EWBRuntimeActionPresentationType::EndTurn));
	TestEqual(TEXT("Label"), Entry.PublicLabel, FString(TEXT("End Turn")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationPassEntriesTest, "Wandbound.Runtime.LegalActionPresentation.PassEntries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationPassEntriesTest::RunTest(const FString& Parameters)
{
	const FWBAction PassAction = MakePresentationPassAction();
	const FWBAction PassResponseAction = MakePresentationPassResponseAction();
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ PassAction, PassResponseAction }, MakePresentationPublicSummary());

	TestEqual(TEXT("Pass ID"), Snapshot.Entries[0].ActionId, FString(TEXT("pass:p0")));
	TestEqual(TEXT("Pass type"), static_cast<int32>(Snapshot.Entries[0].Type), static_cast<int32>(EWBRuntimeActionPresentationType::Pass));
	TestEqual(TEXT("Pass label"), Snapshot.Entries[0].PublicLabel, FString(TEXT("Pass")));
	TestEqual(TEXT("PassResponse ID"), Snapshot.Entries[1].ActionId, FString(TEXT("pass_response:p1")));
	TestEqual(TEXT("PassResponse type"), static_cast<int32>(Snapshot.Entries[1].Type), static_cast<int32>(EWBRuntimeActionPresentationType::PassResponse));
	TestEqual(TEXT("PassResponse label"), Snapshot.Entries[1].PublicLabel, FString(TEXT("Pass")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationMissingSourceTest, "Wandbound.Runtime.LegalActionPresentation.MissingPublicSourceUnit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationMissingSourceTest::RunTest(const FString& Parameters)
{
	FWBAction MoveAction = MakePresentationMoveAction();
	MoveAction.SourceUnitId = 99;
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ MoveAction }, MakePresentationPublicSummary());

	const FWBRuntimeActionPresentationEntry& Entry = Snapshot.Entries[0];
	TestFalse(TEXT("Source public unit missing"), Entry.bHasSourcePublicUnit);
	TestTrue(TEXT("Source card empty"), Entry.SourcePublicCardId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationMissingTargetTest, "Wandbound.Runtime.LegalActionPresentation.RemovedTargetNotInPublicSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationMissingTargetTest::RunTest(const FString& Parameters)
{
	FWBAction AttackAction = MakePresentationAttackAction();
	AttackAction.TargetUnitId = 77;
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ AttackAction }, MakePresentationPublicSummary());

	const FWBRuntimeActionPresentationEntry& Entry = Snapshot.Entries[0];
	TestTrue(TEXT("Source public unit found"), Entry.bHasSourcePublicUnit);
	TestFalse(TEXT("Target public unit missing"), Entry.bHasTargetPublicUnit);
	TestTrue(TEXT("Target card empty"), Entry.TargetPublicCardId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationTryFindSuccessTest, "Wandbound.Runtime.LegalActionPresentation.TryFindSuccess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationTryFindSuccessTest::RunTest(const FString& Parameters)
{
	const FWBAction MoveAction = MakePresentationMoveAction();
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ MoveAction, MakePresentationEndTurnAction() }, MakePresentationPublicSummary());

	FWBRuntimeActionPresentationEntry Entry;
	TestTrue(TEXT("Find succeeds"), WBRuntimeLegalActionPresentation::TryFindEntryByActionId(Snapshot, WBActionCodec::MakeActionId(MoveAction), Entry));
	TestEqual(TEXT("Found action id"), Entry.ActionId, WBActionCodec::MakeActionId(MoveAction));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationTryFindMissingTest, "Wandbound.Runtime.LegalActionPresentation.TryFindMissing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationTryFindMissingTest::RunTest(const FString& Parameters)
{
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ MakePresentationMoveAction() }, MakePresentationPublicSummary());

	FWBRuntimeActionPresentationEntry Entry;
	TestFalse(TEXT("Find missing fails"), WBRuntimeLegalActionPresentation::TryFindEntryByActionId(Snapshot, TEXT("end_turn:p0"), Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationTryFindDuplicateTest, "Wandbound.Runtime.LegalActionPresentation.TryFindDuplicate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationTryFindDuplicateTest::RunTest(const FString& Parameters)
{
	const FWBAction MoveAction = MakePresentationMoveAction();
	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ MoveAction, MoveAction }, MakePresentationPublicSummary());

	FWBRuntimeActionPresentationEntry Entry;
	TestFalse(TEXT("Find duplicate fails"), WBRuntimeLegalActionPresentation::TryFindEntryByActionId(Snapshot, WBActionCodec::MakeActionId(MoveAction), Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationNoActionGenerationSourceGuardTest, "Wandbound.Runtime.LegalActionPresentation.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Presentation source loads"), LoadPresentationSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not mention WBRules"), Source.Contains(TEXT("WBRules")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationNoHiddenStateApiTest, "Wandbound.Runtime.LegalActionPresentation.NoHiddenStateApi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationNoHiddenStateApiTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Presentation source loads"), LoadPresentationSource(Source));
	TestFalse(TEXT("API does not accept FWBGameStateData"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand access"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));

	FWBPlayerStateData Player;
	Player.PlayerId = 0;
	Player.Deck.Add(TEXT("SECRET_PRESENTATION_DECK"));
	Player.Hand.Add(TEXT("SECRET_PRESENTATION_HAND"));
	Player.Discard.Add(TEXT("SECRET_PRESENTATION_DISCARD"));

	const FWBRuntimeLegalActionPresentationSnapshot Snapshot =
		WBRuntimeLegalActionPresentation::BuildSnapshot({ MakePresentationMoveAction() }, MakePresentationPublicSummary());
	const FString PublicText = CombineEntryPublicText(Snapshot);
	TestFalse(TEXT("Deck secret absent"), PublicText.Contains(TEXT("SECRET_PRESENTATION_DECK")));
	TestFalse(TEXT("Hand secret absent"), PublicText.Contains(TEXT("SECRET_PRESENTATION_HAND")));
	TestFalse(TEXT("Discard secret absent"), PublicText.Contains(TEXT("SECRET_PRESENTATION_DISCARD")));
	TestTrue(TEXT("Visible source card present"), PublicText.Contains(TEXT("char_visible_presenter_player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeLegalActionPresentationActionIdsStableTest, "Wandbound.Runtime.LegalActionPresentation.ActionIdsStable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLegalActionPresentationActionIdsStableTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move ID stable"), WBActionCodec::MakeActionId(MakePresentationMoveAction()), FString(TEXT("move:p0:u1:4,4->5,4")));
	TestEqual(TEXT("Attack ID stable"), WBActionCodec::MakeActionId(MakePresentationAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn ID stable"), WBActionCodec::MakeActionId(MakePresentationEndTurnAction()), FString(TEXT("end_turn:p0")));
	return true;
}

#endif
