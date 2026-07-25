#include "Misc/AutomationTest.h"

#include "WBRuntimeTracePresentationTranslator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBTraceEvent MakeTrace(const TCHAR* Kind)
{
	FWBTraceEvent Trace;
	Trace.Kind = FName(Kind);
	Trace.bOk = true;
	return Trace;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationMoveTranslationTest,
	"Wandbound.Runtime.Presentation.Translation.MovementPreservesSourceAndDestination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationMoveTranslationTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Trace = MakeTrace(TEXT("move"));
	Trace.SourceUnitId = 7;
	Trace.FromTile = FWBTile(1, 2);
	Trace.ToTile = FWBTile(2, 2);
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Trace });
	TestTrue(TEXT("Translation succeeds"), Result.bOk);
	TestEqual(TEXT("One presentation event"), Result.Events.Num(), 1);
	TestEqual(TEXT("Movement type"), Result.Events[0].Type, EWBRuntimePresentationEventType::UnitMoved);
	TestEqual(TEXT("Source tile"), Result.Events[0].SourceTile, FIntPoint(1, 2));
	TestEqual(TEXT("Destination tile"), Result.Events[0].DestinationTile, FIntPoint(2, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationMalformedMoveTest,
	"Wandbound.Runtime.Presentation.Translation.MalformedMovementFailsClearly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationMalformedMoveTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Trace = MakeTrace(TEXT("move"));
	Trace.SourceUnitId = 7;
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Trace });
	TestFalse(TEXT("Malformed movement rejected"), Result.bOk);
	TestTrue(TEXT("Reason names malformed movement"), Result.Reason.Contains(TEXT("movement_trace_malformed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAttackTranslationTest,
	"Wandbound.Runtime.Presentation.Translation.AttackDeclarationPrecedesImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAttackTranslationTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Trace = MakeTrace(TEXT("attack_declared"));
	Trace.SourceUnitId = 4;
	Trace.TargetUnitId = 9;
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Trace });
	TestTrue(TEXT("Translation succeeds"), Result.bOk);
	TestEqual(TEXT("Two visual stages"), Result.Events.Num(), 2);
	TestEqual(TEXT("Declaration first"), Result.Events[0].Type, EWBRuntimePresentationEventType::AttackDeclared);
	TestEqual(TEXT("Impact second"), Result.Events[1].Type, EWBRuntimePresentationEventType::AttackImpact);
	TestEqual(TEXT("Stable sequence zero"), Result.Events[0].SequenceIndex, 0);
	TestEqual(TEXT("Stable sequence one"), Result.Events[1].SequenceIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationDamageTranslationTest,
	"Wandbound.Runtime.Presentation.Translation.DamagePreservesPublicHPAndArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationDamageTranslationTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Trace = MakeTrace(TEXT("attack_damage_resolved"));
	Trace.TargetUnitId = 9;
	Trace.FinalDamageAmount = 4;
	Trace.PreviousHP = 8;
	Trace.NewHP = 6;
	Trace.PreviousArmor = 2;
	Trace.NewArmor = 0;
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Trace });
	TestTrue(TEXT("Translation succeeds"), Result.bOk);
	TestEqual(TEXT("Damage plus Armor plus HP"), Result.Events.Num(), 3);
	TestEqual(TEXT("Damage value preserved"), Result.Events[0].DamageAmount, 4);
	TestEqual(TEXT("Armor event"), Result.Events[1].Type, EWBRuntimePresentationEventType::ArmorChanged);
	TestEqual(TEXT("HP event"), Result.Events[2].Type, EWBRuntimePresentationEventType::HPChanged);
	TestEqual(TEXT("HP before preserved"), Result.Events[2].PreviousHP, 8);
	TestEqual(TEXT("HP after preserved"), Result.Events[2].NewHP, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationSummonEquipActivationTest,
	"Wandbound.Runtime.Presentation.Translation.SummonEquipActivationPreserveOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationSummonEquipActivationTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Summon = MakeTrace(TEXT("summon_unit"));
	Summon.SourceUnitId = 20;
	Summon.CardId = TEXT("public_student");
	Summon.ToTile = FWBTile(3, 4);
	FWBTraceEvent Equip = MakeTrace(TEXT("equip_wand"));
	Equip.TargetUnitId = 20;
	Equip.PreviousRLUsed = 0;
	Equip.NewRLUsed = 2;
	FWBTraceEvent Activation = MakeTrace(TEXT("card_activation_resolved"));
	Activation.SourceUnitId = 20;

	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Summon, Equip, Activation });
	TestEqual(TEXT("Three events"), Result.Events.Num(), 3);
	TestEqual(TEXT("Summon first"), Result.Events[0].Type, EWBRuntimePresentationEventType::UnitSummoned);
	TestEqual(TEXT("Public summoned definition retained"), Result.Events[0].PublicDefinitionId, FString(TEXT("public_student")));
	TestEqual(TEXT("Equip second"), Result.Events[1].Type, EWBRuntimePresentationEventType::WandEquipped);
	TestTrue(TEXT("Equip identity remains omitted"), Result.Events[1].PublicDefinitionId.IsEmpty());
	TestEqual(TEXT("Activation third"), Result.Events[2].Type, EWBRuntimePresentationEventType::ActivationResolved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationMarkerPrivacyTest,
	"Wandbound.Runtime.Presentation.Translation.MarkerIdentityOnlyAppearsAtReveal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationMarkerPrivacyTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Triggered = MakeTrace(TEXT("marker_triggered"));
	Triggered.MarkerId = 3;
	Triggered.MarkerType = FName(TEXT("Trap"));
	FWBTraceEvent Revealed = Triggered;
	Revealed.Kind = FName(TEXT("marker_revealed"));
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Triggered, Revealed });
	TestEqual(TEXT("Internal trigger skipped"), Result.Events.Num(), 1);
	TestEqual(TEXT("Reveal event"), Result.Events[0].Type, EWBRuntimePresentationEventType::MarkerRevealed);
	TestEqual(TEXT("Type appears only at reveal"), Result.Events[0].PublicMarkerType, FName(TEXT("Trap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationTrapTranslationTest,
	"Wandbound.Runtime.Presentation.Translation.TrapImpactPrecedesDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationTrapTranslationTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Trap = MakeTrace(TEXT("trap_damage_resolved"));
	Trap.MarkerId = 2;
	Trap.TargetUnitId = 8;
	Trap.FinalDamageAmount = 2;
	Trap.PreviousHP = 5;
	Trap.NewHP = 3;
	Trap.PreviousArmor = 0;
	Trap.NewArmor = 0;
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Trap });
	TestEqual(TEXT("Trap impact plus damage plus HP"), Result.Events.Num(), 3);
	TestEqual(TEXT("Trap first"), Result.Events[0].Type, EWBRuntimePresentationEventType::TrapTriggered);
	TestEqual(TEXT("Damage second"), Result.Events[1].Type, EWBRuntimePresentationEventType::DamageApplied);
	TestEqual(TEXT("HP third"), Result.Events[2].Type, EWBRuntimePresentationEventType::HPChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationNPCOrderTest,
	"Wandbound.Runtime.Presentation.Translation.NPCSpawnMoveAttackOrderPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationNPCOrderTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Spawn = MakeTrace(TEXT("npc_spawn_succeeded"));
	Spawn.TargetUnitId = 30;
	Spawn.CardId = TEXT("public_npc");
	Spawn.ToTile = FWBTile(4, 4);
	FWBTraceEvent Move = MakeTrace(TEXT("npc_moved"));
	Move.SourceUnitId = 30;
	Move.FromTile = FWBTile(4, 4);
	Move.ToTile = FWBTile(4, 5);
	FWBTraceEvent Attack = MakeTrace(TEXT("npc_attack_declared"));
	Attack.SourceUnitId = 30;
	Attack.TargetUnitId = 0;
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Spawn, Move, Attack });
	TestEqual(TEXT("Spawn, move, attack, impact"), Result.Events.Num(), 4);
	TestEqual(TEXT("Spawn first"), Result.Events[0].Type, EWBRuntimePresentationEventType::NPCSpawned);
	TestEqual(TEXT("Move second"), Result.Events[1].Type, EWBRuntimePresentationEventType::NPCMoved);
	TestEqual(TEXT("Attack third"), Result.Events[2].Type, EWBRuntimePresentationEventType::NPCAttacked);
	TestEqual(TEXT("Impact fourth"), Result.Events[3].Type, EWBRuntimePresentationEventType::AttackImpact);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationDeathOrderTest,
	"Wandbound.Runtime.Presentation.Translation.EquipmentDeathHeroTerminalOrderPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationDeathOrderTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Equipment = MakeTrace(TEXT("equipped_card_discarded_on_death"));
	Equipment.TargetUnitId = 0;
	FWBTraceEvent Unit = MakeTrace(TEXT("unit_defeated"));
	Unit.TargetUnitId = 0;
	FWBTraceEvent Hero = MakeTrace(TEXT("hero_defeated"));
	Hero.TargetUnitId = 0;
	FWBTraceEvent GameOver = MakeTrace(TEXT("game_over"));
	GameOver.WinningPlayerId = 1;
	const FWBRuntimePresentationTranslationResult Result =
		WBRuntimeTracePresentationTranslator::Translate({ Equipment, Unit, Hero, GameOver });
	TestEqual(TEXT("Four events"), Result.Events.Num(), 4);
	TestEqual(TEXT("Equipment first"), Result.Events[0].Type, EWBRuntimePresentationEventType::EquipmentDiscarded);
	TestEqual(TEXT("Unit defeat second"), Result.Events[1].Type, EWBRuntimePresentationEventType::UnitDefeated);
	TestEqual(TEXT("Hero defeat third"), Result.Events[2].Type, EWBRuntimePresentationEventType::HeroDefeated);
	TestEqual(TEXT("Game over last"), Result.Events[3].Type, EWBRuntimePresentationEventType::GameOver);
	TestTrue(TEXT("Game over terminal"), Result.Events[3].bTerminal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationUnknownAndDeterminismTest,
	"Wandbound.Runtime.Presentation.Translation.UnknownSkippedAndEquivalentInputDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationUnknownAndDeterminismTest::RunTest(const FString& Parameters)
{
	FWBTraceEvent Unknown = MakeTrace(TEXT("internal_only_event"));
	FWBTraceEvent Move = MakeTrace(TEXT("move"));
	Move.SourceUnitId = 5;
	Move.FromTile = FWBTile(2, 3);
	Move.ToTile = FWBTile(2, 4);
	const FWBRuntimePresentationTranslationResult A =
		WBRuntimeTracePresentationTranslator::Translate({ Unknown, Move });
	const FWBRuntimePresentationTranslationResult B =
		WBRuntimeTracePresentationTranslator::Translate({ Unknown, Move });
	TestEqual(TEXT("Unknown skipped"), A.Events.Num(), 1);
	TestEqual(TEXT("Equivalent count"), A.Events.Num(), B.Events.Num());
	TestEqual(TEXT("Equivalent type"), A.Events[0].Type, B.Events[0].Type);
	TestEqual(TEXT("Equivalent trace index"), A.Events[0].SourceTraceIndex, B.Events[0].SourceTraceIndex);
	TestEqual(TEXT("Equivalent sequence index"), A.Events[0].SequenceIndex, B.Events[0].SequenceIndex);
	TestEqual(TEXT("Equivalent destination"), A.Events[0].DestinationTile, B.Events[0].DestinationTile);
	return true;
}

#endif
