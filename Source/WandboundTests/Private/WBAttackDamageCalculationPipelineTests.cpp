#include "Misc/AutomationTest.h"

#include "WBDamageResolution.h"
#include "WBEffectRunner.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBGameStateData MakePipelineState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.Phase = EWBGamePhase::Response;

	FWBUnitState Attacker;
	Attacker.UnitId = 1;
	Attacker.OwnerId = 0;
	Attacker.X = 4;
	Attacker.Y = 4;
	Attacker.HP = 8;
	Attacker.MaxHP = 8;
	Attacker.ATK = 5;
	Attacker.AR = 8;
	Attacker.AttacksLeft = 1;
	State.AddUnitForTest(Attacker);

	FWBUnitState Defender;
	Defender.UnitId = 2;
	Defender.OwnerId = 1;
	Defender.X = 4;
	Defender.Y = 1;
	Defender.HP = 10;
	Defender.MaxHP = 10;
	Defender.SetArmorForTest(2, 2);
	State.AddUnitForTest(Defender);

	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.Stage = EWBAttackContinuationStage::CalculateDamage;
	Pending.AttackerUnitId = 1;
	Pending.DefenderUnitId = 2;
	Pending.OriginalAttackerUnitId = 1;
	Pending.OriginalDefenderUnitId = 2;
	Pending.AttackingPlayerId = 0;
	Pending.AttackerTile = FWBTile(4, 4);
	Pending.DefenderTile = FWBTile(4, 1);
	Pending.DeclarationActionId = TEXT("attack:p0:u1:t2");
	Pending.ContinuationId = TEXT("pipeline_attack");
	State.SetPendingAttackForTest(Pending);
	return State;
}
}

#define WB_DAMAGE_PIPELINE_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_DAMAGE_PIPELINE_TEST(FWBPureDamagePreviewTest,
	"Wandbound.AttackDamagePipeline.Calculate.PureDamageResolutionPreview")
bool FWBPureDamagePreviewTest::RunTest(const FString&)
{
	FWBGameStateData State = MakePipelineState();
	FWBDamageRequest Request;
	Request.DamageKind = EWBDamageKind::Attack;
	Request.SourceUnitId = 1;
	Request.TargetUnitId = 2;
	Request.SourcePlayerId = 0;
	Request.BaseDamage = 5;
	Request.DamageCause = FName(TEXT("Attack"));
	const FWBDamageResolutionResult Result =
		WBDamageResolution::CalculateDamageRequest(State, Request);
	TestTrue(TEXT("Preview succeeds"), Result.bOk);
	TestEqual(TEXT("Armor absorbs two"), Result.ArmorAbsorbedAmount, 2);
	TestEqual(TEXT("Calculated HP damage is three"), Result.HPDamageAmount, 3);
	TestEqual(TEXT("Preview does not mutate HP"), State.GetUnitById(2)->HP, 10);
	TestEqual(TEXT("Preview does not mutate Armor"),
		State.GetUnitById(2)->GetCurrentArmor(), 2);
	return true;
}

WB_DAMAGE_PIPELINE_TEST(FWBAttackCalculationStagePureTest,
	"Wandbound.AttackDamagePipeline.Calculate.StageDoesNotMutateGameplayState")
bool FWBAttackCalculationStagePureTest::RunTest(const FString&)
{
	FWBGameStateData State = MakePipelineState();
	const FWBApplyActionResult Result =
		WBEffectRunner::CalculatePendingAttackDamage(State);
	TestTrue(TEXT("Stage succeeds"), Result.bOk);
	TestEqual(TEXT("Hit identity is defender"),
		State.PendingAttack.DamageCalculation.HitUnitId, 2);
	TestEqual(TEXT("HP is unchanged"), State.GetUnitById(2)->HP, 10);
	TestEqual(TEXT("Armor is unchanged"),
		State.GetUnitById(2)->GetCurrentArmor(), 2);
	TestEqual(TEXT("Stage advances to substitution"),
		static_cast<int32>(State.PendingAttack.Stage),
		static_cast<int32>(EWBAttackContinuationStage::SubstituteDamage));
	return true;
}

WB_DAMAGE_PIPELINE_TEST(FWBFrozenCalculationPureTest,
	"Wandbound.AttackDamagePipeline.Calculate.FrozenBreakDoesNotMutateUntilApply")
bool FWBFrozenCalculationPureTest::RunTest(const FString&)
{
	FWBGameStateData State = MakePipelineState();
	State.GetMutableUnitById(2)->AddStatus(FName(TEXT("Frozen")), 1);
	TestTrue(TEXT("Calculate succeeds"),
		WBEffectRunner::CalculatePendingAttackDamage(State).bOk);
	TestTrue(TEXT("Frozen break is calculated"),
		State.PendingAttack.DamageCalculation.bFrozenBreak);
	TestEqual(TEXT("Frozen produces no HP damage"),
		State.PendingAttack.DamageCalculation.CalculatedHPDamage, 0);
	TestTrue(TEXT("Calculate leaves Frozen present"),
		State.GetUnitById(2)->HasStatus(FName(TEXT("Frozen"))));
	WBEffectRunner::ResolvePendingAttackDamageSubstitution(State);
	TestTrue(TEXT("Apply succeeds"),
		WBEffectRunner::ApplyCalculatedPendingAttackDamage(State, true).bOk);
	TestFalse(TEXT("Apply removes Frozen"),
		State.GetUnitById(2)->HasStatus(FName(TEXT("Frozen"))));
	return true;
}

WB_DAMAGE_PIPELINE_TEST(FWBDamageCalculationStaleApplyTest,
	"Wandbound.AttackDamagePipeline.Apply.StaleCalculationFailsClosed")
bool FWBDamageCalculationStaleApplyTest::RunTest(const FString&)
{
	FWBGameStateData State = MakePipelineState();
	WBEffectRunner::CalculatePendingAttackDamage(State);
	WBEffectRunner::ResolvePendingAttackDamageSubstitution(State);
	State.GetMutableUnitById(2)->HP = 9;
	const FWBApplyActionResult Result =
		WBEffectRunner::ApplyCalculatedPendingAttackDamage(State, true);
	TestFalse(TEXT("Stale apply fails"), Result.bOk);
	TestEqual(TEXT("Stale diagnostic"), Result.Reason,
		FString(TEXT("damage_calculation_stale")));
	TestEqual(TEXT("Failed apply does not mutate Armor"),
		State.GetUnitById(2)->GetCurrentArmor(), 2);
	return true;
}

#undef WB_DAMAGE_PIPELINE_TEST

#endif
