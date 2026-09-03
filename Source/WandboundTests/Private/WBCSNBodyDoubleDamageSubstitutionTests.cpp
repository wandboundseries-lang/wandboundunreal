#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBCardDefinitionRepository.h"
#include "WBEffectRunner.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCSNBodyDoubleSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile Tile,
	const int32 HP = 8,
	const int32 ATK = 0,
	const int32 Armor = 0)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = HP;
	Unit.ATK = ATK;
	Unit.AR = 8;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetArmorForTest(Armor, Armor);
	return Unit;
}

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.TurnNumber = 2;
	State.Phase = EWBGamePhase::Response;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = 10;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 20;
	State.Players = { Player0, Player1 };

	State.AddUnitForTest(MakeUnit(10, 0, TEXT("attacker"), FWBTile(4, 4), 8, 3));
	State.AddUnitForTest(MakeUnit(20, 1, TEXT("hero"), FWBTile(4, 1), 8));
	State.AddUnitForTest(MakeUnit(30, 1, TEXT("csn"), FWBTile(0, 8), 8));
	State.AddUnitForTest(MakeUnit(31, 1, TEXT("csn"), FWBTile(8, 8), 8));

	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.AuthorityKind = EWBAttackAuthorityKind::Player;
	Pending.Stage = EWBAttackContinuationStage::PreHit;
	Pending.AttackerUnitId = 10;
	Pending.DefenderUnitId = 20;
	Pending.OriginalAttackerUnitId = 10;
	Pending.OriginalDefenderUnitId = 20;
	Pending.AttackingPlayerId = 0;
	Pending.AttackerTile = FWBTile(4, 4);
	Pending.DefenderTile = FWBTile(4, 1);
	Pending.DeclarationActionId = TEXT("attack:p0:u10:t20");
	Pending.ContinuationId = TEXT("body_double_continuation");
	State.SetPendingAttackForTest(Pending);
	State.ReactionWindow.Kind = EWBReactionWindowKind::PreHit;
	State.ReactionWindow.OriginatingPlayerId = 0;
	State.ReactionWindow.SourceActionId = Pending.DeclarationActionId;
	State.ReactionWindow.SourceUnitId = 10;
	State.ReactionWindow.TargetUnitId = 20;
	return State;
}

const FWBTraceEvent* FindTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.FindByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}
}

#define WB_BODY_DOUBLE_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleGenericLegalityTest,
	"Wandbound.CSNBodyDouble.Generic.ExactContinuationAndLiveRecipient")
bool FWBCSNBodyDoubleGenericLegalityTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Live recipient accepted"),
		WBRules::CanRegisterPendingAttackDamageSubstitution(
			State, TEXT("body_double_continuation"), 30).bOk);
	TestEqual(TEXT("Stale continuation denied"),
		WBRules::CanRegisterPendingAttackDamageSubstitution(
			State, TEXT("stale"), 30).Reason,
		FString(TEXT("pending_attack_target_mismatch")));
	State.GetMutableUnitById(30)->RemoveUnitFromBoard();
	TestEqual(TEXT("Removed recipient denied"),
		WBRules::CanRegisterPendingAttackDamageSubstitution(
			State, TEXT("body_double_continuation"), 30).Reason,
		FString(TEXT("damage_recipient_removed")));
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleIdentityAndLatestWinsTest,
	"Wandbound.CSNBodyDouble.Generic.DefenderIdentityAndLatestWins")
bool FWBCSNBodyDoubleIdentityAndLatestWinsTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	const FWBApplyActionResult First =
		WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
			State, TEXT("body_double_continuation"), 30);
	const FWBApplyActionResult Second =
		WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
			State, TEXT("body_double_continuation"), 31);
	TestTrue(TEXT("First substitution succeeds"), First.bOk);
	TestTrue(TEXT("Second substitution succeeds"), Second.bOk);
	TestEqual(TEXT("Latest recipient wins"),
		State.PendingAttack.DamageSubstitution.SubstituteUnitId, 31);
	TestEqual(TEXT("Protected unit is captured at resolution"),
		State.PendingAttack.DamageSubstitution.ProtectedUnitId, 20);
	TestEqual(TEXT("Defender remains Hero"), State.PendingAttack.DefenderUnitId, 20);
	TestEqual(TEXT("Original defender remains Hero"), State.PendingAttack.OriginalDefenderUnitId, 20);
	TestEqual(TEXT("Reaction target remains Hero"), State.ReactionWindow.TargetUnitId, 20);
	const FWBTraceEvent* Trace = FindTrace(
		Second.TraceEvents,
		FName(TEXT("pending_attack_damage_substitution_registered")));
	TestNotNull(TEXT("Generic substitution trace emitted"), Trace);
	if (Trace != nullptr)
	{
		TestEqual(TEXT("Previous recipient captured"), Trace->PreviousTargetUnitId, 30);
		TestEqual(TEXT("Attack defender captured"), Trace->AttackDefenderUnitId, 20);
		TestEqual(TEXT("New recipient captured"), Trace->DamageRecipientUnitId, 31);
	}
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleDamageArmorTest,
	"Wandbound.CSNBodyDouble.Damage.HeroArmorCalculatedBeforeSubstituteArmorIgnored")
bool FWBCSNBodyDoubleDamageArmorTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(20)->SetArmorForTest(2, 2);
	State.GetMutableUnitById(30)->SetArmorForTest(7, 7);
	TestTrue(TEXT("Substitution succeeds"),
		WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
			State, TEXT("body_double_continuation"), 30).bOk);
	const int32 HeroHPBefore = State.GetUnitById(20)->HP;
	const int32 HeroArmorBefore = State.GetUnitById(20)->GetCurrentArmor();
	const int32 SubstituteHPBefore = State.GetUnitById(30)->HP;
	const int32 SubstituteArmorBefore = State.GetUnitById(30)->GetCurrentArmor();
	State.PendingAttack.Stage = EWBAttackContinuationStage::CalculateDamage;
	const FWBApplyActionResult Calculation =
		WBEffectRunner::CalculatePendingAttackDamage(State);
	TestTrue(TEXT("Calculation succeeds"), Calculation.bOk);
	TestEqual(TEXT("Calculation hit unit is Hero"),
		State.PendingAttack.DamageCalculation.HitUnitId, 20);
	TestEqual(TEXT("Hero armor determines transferred HP damage"),
		State.PendingAttack.DamageCalculation.CalculatedHPDamage, 1);
	TestEqual(TEXT("Calculation does not mutate Hero HP"),
		State.GetUnitById(20)->HP, HeroHPBefore);
	TestEqual(TEXT("Calculation does not mutate Hero armor"),
		State.GetUnitById(20)->GetCurrentArmor(), HeroArmorBefore);
	TestTrue(TEXT("Substitute stage succeeds"),
		WBEffectRunner::ResolvePendingAttackDamageSubstitution(State).bOk);
	const FWBApplyActionResult Damage =
		WBEffectRunner::ApplyCalculatedPendingAttackDamage(State, true);
	TestTrue(TEXT("Damage resolves"), Damage.bOk);
	TestEqual(TEXT("Hero HP unchanged"), State.GetUnitById(20)->HP, HeroHPBefore);
	TestEqual(TEXT("Hero armor is consumed at ApplyDamage"),
		State.GetUnitById(20)->GetCurrentArmor(), 0);
	TestEqual(TEXT("Substitute armor is ignored"),
		State.GetUnitById(30)->GetCurrentArmor(), SubstituteArmorBefore);
	TestEqual(TEXT("Substitute takes exact calculated HP damage"),
		State.GetUnitById(30)->HP, SubstituteHPBefore - 1);
	TestEqual(TEXT("PostHit defender remains Hero"), State.PendingAttack.DefenderUnitId, 20);
	TestEqual(TEXT("Final recipient records substitute"),
		State.PendingAttack.FinalDamageRecipientUnitId, 30);
	const FWBTraceEvent* Trace = FindTrace(
		Damage.TraceEvents, FName(TEXT("attack_damage_resolved")));
	TestNotNull(TEXT("Damage trace emitted"), Trace);
	if (Trace != nullptr)
	{
		TestEqual(TEXT("Trace target is recipient"), Trace->TargetUnitId, 30);
		TestEqual(TEXT("Trace defender remains Hero"), Trace->AttackDefenderUnitId, 20);
	}
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleFrozenTest,
	"Wandbound.CSNBodyDouble.Damage.FinalRecipientFrozenBreaksOriginalDefenderFrozenRemains")
bool FWBCSNBodyDoubleFrozenTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(20)->AddStatus(FName(TEXT("Frozen")), 1);
	State.GetMutableUnitById(30)->AddStatus(FName(TEXT("Frozen")), 1);
	WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
		State, TEXT("body_double_continuation"), 30);
	State.PendingAttack.Stage = EWBAttackContinuationStage::Damage;
	const FWBApplyActionResult Damage =
		WBEffectRunner::ApplyPendingAttackDamage(State, true);
	TestTrue(TEXT("Frozen resolution succeeds"), Damage.bOk);
	TestFalse(TEXT("Final recipient Frozen breaks"),
		State.GetUnitById(30)->HasStatus(FName(TEXT("Frozen"))));
	TestTrue(TEXT("Original defender Frozen remains"),
		State.GetUnitById(20)->HasStatus(FName(TEXT("Frozen"))));
	TestEqual(TEXT("Recipient takes no HP damage"), State.GetUnitById(30)->HP, 8);
	TestEqual(TEXT("Hero takes no HP damage"), State.GetUnitById(20)->HP, 8);
	TestTrue(TEXT("Hit-unit Frozen suppresses counter eligibility"),
		State.PendingAttack.bFrozenBroken);
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleFallbackTest,
	"Wandbound.CSNBodyDouble.Damage.RemovedRecipientFallsBackToHero")
bool FWBCSNBodyDoubleFallbackTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
		State, TEXT("body_double_continuation"), 30);
	State.GetMutableUnitById(30)->RemoveUnitFromBoard();
	State.PendingAttack.Stage = EWBAttackContinuationStage::Damage;
	const FWBApplyActionResult Damage =
		WBEffectRunner::ApplyPendingAttackDamage(State, true);
	TestTrue(TEXT("Attack does not fizzle"), Damage.bOk);
	TestEqual(TEXT("Hero receives ordinary damage"), State.GetUnitById(20)->HP, 5);
	TestNotNull(TEXT("Fallback trace emitted"), FindTrace(
		Damage.TraceEvents,
		FName(TEXT("pending_attack_damage_substitution_fallback"))));
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleDigestTest,
	"Wandbound.CSNBodyDouble.Replay.ActiveRecipientChangesPrivateDigestOnly")
bool FWBCSNBodyDoubleDigestTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
	State.PendingAttack.DamageSubstitution.bActive = true;
	State.PendingAttack.DamageSubstitution.ProtectedUnitId = 20;
	State.PendingAttack.DamageSubstitution.SubstituteUnitId = 30;
	const FString Active = WBProductionMatchReplay::BuildGameStateDigest(State);
	State.PendingAttack.DamageSubstitution = {};
	const FString Restored = WBProductionMatchReplay::BuildGameStateDigest(State);
	TestNotEqual(TEXT("Active substitution changes digest"), Active, Before);
	TestEqual(TEXT("Absent substitution preserves baseline digest"), Restored, Before);
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleProductionDefinitionTest,
	"Wandbound.CSNBodyDouble.CardDB.TypedProductionDefinitionLoads")
bool FWBCSNBodyDoubleProductionDefinitionTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNBodyDoubleFixture/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	if (!Loaded.bOk)
	{
		for (const FWBProductionCardDBDiagnostic& Diagnostic : Loaded.Diagnostics)
		{
			AddError(FString::Printf(
				TEXT("%s | %s | %s"),
				*Diagnostic.Code,
				*Diagnostic.FieldPath,
				*Diagnostic.Message));
		}
	}
	TestTrue(TEXT("Body Double fixture loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid())
	{
		return false;
	}
	AddInfo(FString::Printf(
		TEXT("CSN_BODY_DOUBLE_BUNDLE_DIGEST=%s"),
		*Loaded.Snapshot->ContentDigest));
	const FWBProductionCardRecord* Record =
		Loaded.Snapshot->FindRecord(TEXT("effect_react_csn_body_double"));
	TestNotNull(TEXT("Body Double production record exists"), Record);
	if (Record == nullptr || Record->CoreDefinition.ActivatedEffects.Num() != 1)
	{
		return false;
	}
	const FWBCardEffectDefinition& Effect =
		Record->CoreDefinition.ActivatedEffects[0];
	TestEqual(TEXT("RR remains two"), Effect.SourceGate.CostGate.RequiredRR, 2);
	TestEqual(TEXT("Source is Hand"),
		static_cast<int32>(Effect.SourceGate.RequiredZone),
		static_cast<int32>(EWBCardActivationSourceZone::Hand));
	TestEqual(TEXT("Timing is response window"),
		static_cast<int32>(Effect.SourceGate.Timing),
		static_cast<int32>(EWBCardActivationTimingRequirement::ResponseWindow));
	TestEqual(TEXT("Requires current own Hero defender"),
		static_cast<int32>(Effect.ActivationCondition.AttackDefender),
		static_cast<int32>(EWBCardEffectAttackDefenderRequirement::OwnHeroCurrentDefender));
	TestEqual(TEXT("Requires CSN faction"),
		Effect.ActivationCondition.RequiredTargetFaction,
		FString(TEXT("csn")));
	TestEqual(TEXT("Uses damage substitution, not Redirect"),
		static_cast<int32>(Effect.Payloads[0].Operation),
		static_cast<int32>(EWBGenericEffectOp::RegisterPendingAttackHPDamageSubstitution));
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleNoRedirectGeometryTest,
	"Wandbound.CSNBodyDouble.TargetSelection.RedirectGeometryIsIrrelevant")
bool FWBCSNBodyDoubleNoRedirectGeometryTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	const FWBActionQueryResult Redirect = WBRules::CanRedirectPendingAttack(
		State, TEXT("body_double_continuation"), 30);
	TestFalse(TEXT("Substitute is not a legal Redirect target"), Redirect.bOk);
	TestEqual(TEXT("Attacker alignment blocks Redirect"),
		Redirect.Reason, FString(TEXT("not_in_line")));
	TestTrue(TEXT("Damage substitution ignores attacker geometry"),
		WBRules::CanRegisterPendingAttackDamageSubstitution(
			State, TEXT("body_double_continuation"), 30).bOk);
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleSubstituteFrozenTransferTest,
	"Wandbound.CSNBodyDouble.Damage.SubstituteFrozenBlocksTransferredHPDamage")
bool FWBCSNBodyDoubleSubstituteFrozenTransferTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(30)->AddStatus(FName(TEXT("Frozen")), 1);
	WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
		State, TEXT("body_double_continuation"), 30);
	State.PendingAttack.Stage = EWBAttackContinuationStage::Damage;
	const FWBApplyActionResult Result =
		WBEffectRunner::ApplyPendingAttackDamage(State, true);
	TestTrue(TEXT("Attack resolves"), Result.bOk);
	TestEqual(TEXT("Hero HP is protected"), State.GetUnitById(20)->HP, 8);
	TestEqual(TEXT("Substitute HP is protected by Frozen"), State.GetUnitById(30)->HP, 8);
	TestFalse(TEXT("Substitute Frozen breaks"),
		State.GetUnitById(30)->HasStatus(FName(TEXT("Frozen"))));
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleRedirectAwayTest,
	"Wandbound.CSNBodyDouble.Redirect.RedirectAwayFromHeroMakesSubstitutionInert")
bool FWBCSNBodyDoubleRedirectAwayTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(40, 1, TEXT("redirect_target"), FWBTile(4, 5), 8));
	WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
		State, TEXT("body_double_continuation"), 30);
	TestTrue(TEXT("True Redirect resolves"),
		WBEffectRunner::ApplyPendingAttackRedirect(
			State, TEXT("body_double_continuation"), 40).bOk);
	State.PendingAttack.Stage = EWBAttackContinuationStage::Damage;
	const FWBApplyActionResult Result =
		WBEffectRunner::ApplyPendingAttackDamage(State, true);
	TestTrue(TEXT("Redirected attack resolves"), Result.bOk);
	TestEqual(TEXT("Redirect target takes ordinary damage"), State.GetUnitById(40)->HP, 5);
	TestEqual(TEXT("Hero remains undamaged"), State.GetUnitById(20)->HP, 8);
	TestEqual(TEXT("Substitute remains undamaged"), State.GetUnitById(30)->HP, 8);
	TestEqual(TEXT("Calculation subject is final defender"),
		State.PendingAttack.DamageCalculation.HitUnitId, 40);
	TestNull(TEXT("No transfer trace is emitted"), FindTrace(
		Result.TraceEvents, FName(TEXT("attack_damage_substituted"))));
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleRedirectBackTest,
	"Wandbound.CSNBodyDouble.Redirect.RedirectBackToHeroAllowsSubstitution")
bool FWBCSNBodyDoubleRedirectBackTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(40, 1, TEXT("redirect_target"), FWBTile(4, 5), 8));
	WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
		State, TEXT("body_double_continuation"), 30);
	WBEffectRunner::ApplyPendingAttackRedirect(
		State, TEXT("body_double_continuation"), 40);
	TestTrue(TEXT("Redirect back resolves"),
		WBEffectRunner::ApplyPendingAttackRedirect(
			State, TEXT("body_double_continuation"), 20).bOk);
	State.PendingAttack.Stage = EWBAttackContinuationStage::Damage;
	const FWBApplyActionResult Result =
		WBEffectRunner::ApplyPendingAttackDamage(State, true);
	TestTrue(TEXT("Attack resolves"), Result.bOk);
	TestEqual(TEXT("Hero remains undamaged"), State.GetUnitById(20)->HP, 8);
	TestEqual(TEXT("Substitute receives transferred damage"), State.GetUnitById(30)->HP, 5);
	TestEqual(TEXT("Final calculation subject is Hero"),
		State.PendingAttack.DamageCalculation.HitUnitId, 20);
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoublePreventTest,
	"Wandbound.CSNBodyDouble.Damage.PreventedAttackProducesNoTransfer")
bool FWBCSNBodyDoublePreventTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
		State, TEXT("body_double_continuation"), 30);
	State.PendingAttack.bPrevented = true;
	State.PendingAttack.Stage = EWBAttackContinuationStage::Damage;
	const FWBApplyActionResult Result =
		WBEffectRunner::ApplyPendingAttackDamage(State, true);
	TestTrue(TEXT("Prevented attack resolves automatically"), Result.bOk);
	TestTrue(TEXT("Calculation records prevent"),
		State.PendingAttack.DamageCalculation.bPrevented);
	TestEqual(TEXT("Prevent produces zero HP damage"),
		State.PendingAttack.DamageCalculation.CalculatedHPDamage, 0);
	TestEqual(TEXT("Hero HP unchanged"), State.GetUnitById(20)->HP, 8);
	TestEqual(TEXT("Substitute HP unchanged"), State.GetUnitById(30)->HP, 8);
	return true;
}

WB_BODY_DOUBLE_TEST(FWBCSNBodyDoubleProductionSmokeTest,
	"Wandbound.CSNBodyDouble.Fixture.ProductionSmokeAndFreshReplay")
bool FWBCSNBodyDoubleProductionSmokeTest::RunTest(const FString&)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNBodyDoubleFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionCSNBodyDoubleSmokeResult Result =
		WBProductionCSNBodyDoubleSmoke::Run(Request);
	if (!Result.bOk)
	{
		AddError(FString::Printf(
			TEXT("Production Body Double smoke failed: %s"),
			*Result.Reason));
	}
	TestTrue(TEXT("Production Body Double smoke succeeds"), Result.bOk);
	TestTrue(TEXT("Replay records verified"), Result.RecordsVerified > 0);
	TestFalse(TEXT("Final state digest present"), Result.FinalStateDigest.IsEmpty());
	TestFalse(TEXT("Final trace digest present"), Result.FinalTraceDigest.IsEmpty());
	return true;
}

#undef WB_BODY_DOUBLE_TEST

#endif
