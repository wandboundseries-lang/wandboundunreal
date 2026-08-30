#include "Misc/AutomationTest.h"

#include "WBCardActivationExpansion.h"
#include "WBCardZoneState.h"
#include "WBEffectRunner.h"
#include "WBProductionMatchReplay.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakePlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = 3;
	return Player;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerPlayerId,
	const int32 ControllerPlayerId,
	const FWBTile Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.SetOwnerAndControllerForRules(OwnerPlayerId, ControllerPlayerId);
	Unit.CardId = FString::Printf(TEXT("vocabulary_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.MPRemaining = 3;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeState(const int32 PriorityPlayerId = 1)
{
	FWBGameStateData State;
	State.CurrentPlayer = PriorityPlayerId;
	State.PriorityPlayer = PriorityPlayerId;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakePlayer(0));
	State.Players.Add(MakePlayer(1));
	return State;
}

FWBAction MakeMove(const int32 PlayerId, const FWBTile To)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = 10;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = To;
	return Action;
}

FWBAction MakeAttack(const int32 PlayerId, const int32 TargetUnitId)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = 10;
	Action.TargetUnitId = TargetUnitId;
	return Action;
}

FWBCardDefinition MakeActivationDefinition()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("vocabulary_activation");
	Definition.PublicName = TEXT("Vocabulary Activation");
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	FWBCardEffectDefinition Effect;
	Effect.EffectId = TEXT("choose_unit");
	Effect.PublicLabel = TEXT("Choose Unit");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Effect.Payloads.Add(Payload);
	Definition.ActivatedEffects.Add(Effect);
	return Definition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyNormalIdentityTest,
	"Wandbound.Vocabulary.OwnerController.NormalUnit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyNormalIdentityTest::RunTest(const FString&)
{
	const FWBUnitState Unit = MakeUnit(1, 0, 0, FWBTile(1, 1));
	TestEqual(TEXT("Owner is explicit"), Unit.GetOwnerPlayerIdForRules(), 0);
	TestEqual(TEXT("Controller is explicit"), Unit.GetControllerPlayerIdForRules(), 0);
	TestEqual(TEXT("Legacy mirror is controller"), Unit.OwnerId, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularySplitIdentityTest,
	"Wandbound.Vocabulary.OwnerController.OpponentControlledUnit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularySplitIdentityTest::RunTest(const FString&)
{
	FWBUnitState Unit = MakeUnit(10, 0, 1, FWBTile(4, 4));
	const FString CardId = Unit.CardId;
	Unit.SetControllerPlayerIdForRules(0);
	TestEqual(TEXT("Owner remains stable"), Unit.GetOwnerPlayerIdForRules(), 0);
	TestEqual(TEXT("Controller changes independently"), Unit.GetControllerPlayerIdForRules(), 0);
	TestEqual(TEXT("Exact card identity remains stable"), Unit.CardId, CardId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyCardInstanceOwnershipTest,
	"Wandbound.Vocabulary.OwnerController.CardInstanceOwnershipStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyCardInstanceOwnershipTest::RunTest(const FString&)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = TEXT("owned_instance_0");
	Card.CardId = TEXT("vocabulary_unit_10");
	Card.OwnerPlayerId = 0;
	FWBUnitState Unit = MakeUnit(10, Card.OwnerPlayerId, 1, FWBTile(4, 4));

	const FString OriginalInstanceId = Card.InstanceId;
	Unit.SetControllerPlayerIdForRules(0);
	TestEqual(TEXT("Card owner remains immutable"), Card.OwnerPlayerId, 0);
	TestEqual(TEXT("Exact instance remains unchanged"), Card.InstanceId, OriginalInstanceId);
	TestEqual(TEXT("Unit owner remains card owner"), Unit.GetOwnerPlayerIdForRules(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyControlledUnitQueriesTest,
	"Wandbound.Vocabulary.OwnerController.ControlledUnitQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyControlledUnitQueriesTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(10, 0, 1, FWBTile(4, 4)));
	TestEqual(TEXT("Controller cap membership"), State.GetUnitsControlledByPlayer(1).Num(), 1);
	TestEqual(TEXT("Not counted for owner cap"), State.GetUnitsControlledByPlayer(0).Num(), 0);
	TestEqual(TEXT("Owner query retains original owner"), State.GetUnitsOwnedByPlayer(0).Num(), 1);
	TestEqual(TEXT("Controller is not owner"), State.GetUnitsOwnedByPlayer(1).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyMoveAuthorityTest,
	"Wandbound.Vocabulary.OwnerController.ControllerDirectsMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyMoveAuthorityTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState(1);
	State.AddUnitForTest(MakeUnit(10, 0, 1, FWBTile(4, 4)));
	TestTrue(TEXT("Controller may move"), WBRules::QueryMove(
		State, MakeMove(1, FWBTile(4, 3))).bOk);
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	TestFalse(TEXT("Owner may not direct controlled unit"), WBRules::QueryMove(
		State, MakeMove(0, FWBTile(4, 3))).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyControllerRelationshipTest,
	"Wandbound.Vocabulary.OwnerController.FriendlyEnemyUsesController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyControllerRelationshipTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState(1);
	State.AddUnitForTest(MakeUnit(10, 0, 1, FWBTile(4, 4)));
	State.AddUnitForTest(MakeUnit(11, 0, 1, FWBTile(5, 4)));
	TestEqual(TEXT("Same controller is friendly"), WBRules::CanDeclareAttack(
		State, MakeAttack(1, 11)).Reason, FString(TEXT("friendly_target")));
	State.GetMutableUnitById(11)->SetControllerPlayerIdForRules(0);
	TestTrue(TEXT("Different controller is enemy"), WBRules::CanDeclareAttack(
		State, MakeAttack(1, 11)).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyPublicIdentityTest,
	"Wandbound.Vocabulary.OwnerController.PublicSummaryPreservesBoth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyPublicIdentityTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(10, 0, 1, FWBTile(4, 4)));
	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("One public unit"), Summary.Units.Num(), 1);
	if (Summary.Units.IsEmpty()) return false;
	TestEqual(TEXT("Public owner"), Summary.Units[0].GetOwnerPlayerId(), 0);
	TestEqual(TEXT("Public controller"), Summary.Units[0].GetControllerPlayerId(), 1);
	TestEqual(TEXT("Legacy public owner mirrors controller"), Summary.Units[0].OwnerId, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyDeclaredAttackTest,
	"Wandbound.Vocabulary.Declaration.PlayerAttackAndTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyDeclaredAttackTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState(1);
	State.AddUnitForTest(MakeUnit(10, 0, 1, FWBTile(4, 4)));
	State.AddUnitForTest(MakeUnit(11, 1, 0, FWBTile(5, 4)));
	const FWBApplyActionResult Result = WBEffectRunner::ApplyAttackDeclare(
		State, MakeAttack(1, 11));
	TestTrue(TEXT("Attack accepted"), Result.bOk);
	TestTrue(TEXT("Attack declared"), WBIsPlayerDeclared(
		State.PendingAttack.AttackDeclaration));
	TestTrue(TEXT("Target declared"), WBIsPlayerDeclared(
		State.PendingAttack.TargetDeclaration));
	TestTrue(TEXT("Trace attack declared"), Result.TraceEvents[0].bDeclaredAttack);
	TestTrue(TEXT("Trace target declared"), Result.TraceEvents[0].bDeclaredTarget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyAutomaticAttackTest,
	"Wandbound.Vocabulary.Declaration.AutomaticAttackTargetNotDeclared",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyAutomaticAttackTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState(0);
	State.AddUnitForTest(MakeUnit(20, INDEX_NONE, INDEX_NONE, FWBTile(4, 4)));
	State.AddUnitForTest(MakeUnit(21, 0, 0, FWBTile(5, 4)));
	FWBAction Action = MakeAttack(INDEX_NONE, 21);
	Action.SourceUnitId = 20;
	const FWBApplyActionResult Result = WBEffectRunner::ApplyNPCAttackDeclare(State, Action);
	TestTrue(TEXT("Automatic attack accepted"), Result.bOk);
	TestFalse(TEXT("Automatic attack is not declared"), WBIsPlayerDeclared(
		State.PendingAttack.AttackDeclaration));
	TestFalse(TEXT("Automatic target is not declared"), WBIsPlayerDeclared(
		State.PendingAttack.TargetDeclaration));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyActivationCasterTest,
	"Wandbound.Vocabulary.Caster.DeclaredCharacterActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyActivationCasterTest::RunTest(const FString&)
{
	FWBCardActivationExpansionRequest Request;
	Request.PlayerId = 0;
	Request.SourceUnitId = 10;
	Request.CardDefinition = MakeActivationDefinition();
	Request.EffectId = TEXT("choose_unit");
	Request.Target.TargetUnitId = 11;
	const FWBCardActivationExpansionResult Result =
		WBCardActivationExpansion::BuildActivationCommand(Request);
	TestTrue(TEXT("Expansion succeeds"), Result.bOk);
	TestTrue(TEXT("Activation exists"), WBIsActivation(
		Result.Command.Source.ActivationProvenance));
	TestTrue(TEXT("Activation declared"), WBIsPlayerDeclaredActivation(
		Result.Command.Source.ActivationProvenance));
	TestTrue(TEXT("Target independently declared"), WBIsPlayerDeclared(
		Result.Command.EffectRequest.Target.TargetDeclaration));
	TestEqual(TEXT("Activating unit is caster"), Result.Command.Source.GetCasterUnitId(), 10);
	TestEqual(TEXT("Effect source agrees on caster"),
		Result.Command.EffectRequest.Source.GetCasterUnitId(), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyEquippedWandCasterTest,
	"Wandbound.Vocabulary.Caster.EquippedWandUsesEquippedUnit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyEquippedWandCasterTest::RunTest(const FString&)
{
	FWBCardActivationExpansionRequest Request;
	Request.PlayerId = 0;
	Request.SourceUnitId = 10;
	Request.CardDefinition = MakeActivationDefinition();
	Request.EffectId = TEXT("choose_unit");
	Request.Target.TargetUnitId = 11;
	Request.SourceGateContext.SourceZone = EWBCardActivationSourceZone::Equipped;
	Request.SourceGateContext.SourceCardInstanceId = TEXT("equipped_wand_instance");
	const FWBCardActivationExpansionResult Result =
		WBCardActivationExpansion::BuildActivationCommand(Request);

	TestTrue(TEXT("Expansion succeeds"), Result.bOk);
	TestEqual(TEXT("Equipped source preserved"), Result.Command.Source.SourceZone,
		EWBCardZone::Equipped);
	TestEqual(TEXT("Equipped unit is caster"), Result.Command.Source.GetCasterUnitId(), 10);
	TestEqual(TEXT("Effect source uses equipped unit as caster"),
		Result.Command.EffectRequest.Source.GetCasterUnitId(), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyAutomaticUnitActivationCasterTest,
	"Wandbound.Vocabulary.Caster.AutomaticUnitActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyAutomaticUnitActivationCasterTest::RunTest(const FString&)
{
	FWBEffectSourceRef Source;
	Source.SourceUnitId = 42;
	Source.ActivationProvenance = EWBActivationProvenance::AutomaticActivation;
	TestTrue(TEXT("Automatic activation exists"),
		WBIsActivation(Source.ActivationProvenance));
	TestFalse(TEXT("Automatic activation is not declared"),
		WBIsPlayerDeclaredActivation(Source.ActivationProvenance));
	TestEqual(TEXT("Automatically activating unit is caster"),
		Source.GetCasterUnitId(), 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyResolutionOnlySourceNoCasterTest,
	"Wandbound.Vocabulary.Caster.ResolutionOnlySourceHasNoCaster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyResolutionOnlySourceNoCasterTest::RunTest(const FString&)
{
	FWBEffectSourceRef Source;
	Source.SourceUnitId = 42;
	TestFalse(TEXT("Resolution-only source is not an activation"),
		WBIsActivation(Source.ActivationProvenance));
	TestFalse(TEXT("Resolution-only source is not declared"),
		WBIsPlayerDeclaredActivation(Source.ActivationProvenance));
	TestEqual(TEXT("Source unit alone does not imply caster"),
		Source.GetCasterUnitId(), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyNonUnitDeclaredActivationNoCasterTest,
	"Wandbound.Vocabulary.Caster.DeclaredNonUnitActivationHasNoCaster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyNonUnitDeclaredActivationNoCasterTest::RunTest(const FString&)
{
	FWBEffectSourceRef Source;
	Source.ActivationProvenance = EWBActivationProvenance::PlayerDeclared;
	TestTrue(TEXT("Declared activation exists"),
		WBIsActivation(Source.ActivationProvenance));
	TestTrue(TEXT("Activation is declared"),
		WBIsPlayerDeclaredActivation(Source.ActivationProvenance));
	TestEqual(TEXT("Declared non-unit source has no caster"),
		Source.GetCasterUnitId(), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyTraceSerializationTest,
	"Wandbound.Vocabulary.Declaration.TraceSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyTraceSerializationTest::RunTest(const FString&)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("vocabulary_declaration"));
	Event.bDeclaredAttack = true;
	Event.bDeclaredActivation = true;
	Event.bDeclaredTarget = true;
	const FString Json = WBReplayTrace::SerializeEvent(Event);
	TestTrue(TEXT("Declared attack serialized"),
		Json.Contains(TEXT("\"declared_attack\"")));
	TestTrue(TEXT("Declared activation serialized"),
		Json.Contains(TEXT("\"declared_activation\"")));
	TestTrue(TEXT("Declared target serialized"),
		Json.Contains(TEXT("\"declared_target\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBVocabularyStateDigestSplitIdentityTest,
	"Wandbound.Vocabulary.Replay.OwnerControllerAffectsStateDigest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBVocabularyStateDigestSplitIdentityTest::RunTest(const FString&)
{
	FWBGameStateData Controlled = MakeState();
	Controlled.AddUnitForTest(MakeUnit(10, 0, 1, FWBTile(4, 4)));
	FWBGameStateData Owned = Controlled;
	Owned.GetMutableUnitById(10)->SetOwnerAndControllerForRules(1, 1);
	TestNotEqual(TEXT("Owner participates when distinct"),
		WBProductionMatchReplay::BuildGameStateDigest(Controlled),
		WBProductionMatchReplay::BuildGameStateDigest(Owned));
	return true;
}

#endif
