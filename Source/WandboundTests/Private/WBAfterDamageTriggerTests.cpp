#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "WBAfterDamageTrigger.h"
#include "WBEffectRunner.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionCardDatabase.h"
#include "WBReplayTrace.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 AttackerId = 1;
constexpr int32 HitUnitId = 2;
constexpr int32 SubstituteId = 3;
constexpr int32 RedirectId = 4;

FWBGenericEffectPayload MakeStatusPayload(
	const FName StatusId = FName(TEXT("Rooted")))
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = EWBStatusEffectOp::ApplyStatus;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = 1;
	Payload.StatusEffect.SourceReason = FName(TEXT("after_damage_fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("AfterDamageFixture"));
	Payload.DamageEffect.SourceReason = FName(TEXT("after_damage_fixture"));
	return Payload;
}

FWBAfterDamageTriggerDefinition MakeTrigger(
	const FString& TriggerId,
	const EWBAfterDamageParticipantRole SourceRole,
	const EWBAfterDamageRequirement Requirement,
	const EWBAfterDamageTargetRole TargetRole,
	const FWBGenericEffectPayload& Payload = MakeStatusPayload())
{
	FWBAfterDamageTriggerDefinition Trigger;
	Trigger.TriggerId = TriggerId;
	Trigger.SourceRole = SourceRole;
	Trigger.DamageRequirement = Requirement;
	Trigger.TargetRole = TargetRole;
	Trigger.Payloads.Add(Payload);
	return Trigger;
}

FWBCardDefinition MakeUnitDefinition(
	const FString& CardId,
	const TArray<FWBAfterDamageTriggerDefinition>& Triggers = {})
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 8;
	Definition.CharacterStats.ATK = 3;
	Definition.CharacterStats.AR = 8;
	Definition.CharacterStats.RL = 1;
	Definition.AfterDamageTriggers = Triggers;
	return Definition;
}

FWBCardDefinition MakeWandDefinition(
	const FString& CardId,
	const TArray<FWBAfterDamageTriggerDefinition>& Triggers)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = 1;
	Definition.AfterDamageTriggers = Triggers;
	return Definition;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile,
	const int32 HP,
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

struct FScenarioOptions
{
	int32 AttackDamage = 3;
	int32 HitHP = 8;
	int32 HitArmor = 0;
	bool bPrevented = false;
	bool bFrozenHit = false;
	bool bBodyDouble = false;
	bool bRedirect = false;
	bool bCounter = false;
	bool bNPC = false;
	bool bRemoveHitBeforeCapture = false;
	bool bEquipHitWand = false;
	TArray<FWBAfterDamageTriggerDefinition> AttackerTriggers;
	TArray<FWBAfterDamageTriggerDefinition> HitTriggers;
	TArray<FWBAfterDamageTriggerDefinition> SubstituteTriggers;
	TArray<FWBAfterDamageTriggerDefinition> RedirectTriggers;
	TArray<FWBAfterDamageTriggerDefinition> WandTriggers;
};

struct FScenarioResult
{
	FWBGameStateData State;
	FWBCardDefinitionRepository Repository;
	FWBAfterDamageTriggerCollection Collection;
	FWBApplyActionResult Damage;
	FWBAfterDamageTriggerResolutionResult Resolution;
	FString Reason;
	bool bOk = false;
};

FScenarioResult RunScenario(const FScenarioOptions& Options)
{
	FScenarioResult Result;
	Result.Repository.RepositoryId = TEXT("after_damage_tests");
	Result.Repository.SourceVersion = TEXT("after_damage_v1");
	Result.Repository.Definitions =
	{
		MakeUnitDefinition(TEXT("after_attacker"), Options.AttackerTriggers),
		MakeUnitDefinition(TEXT("after_hit"), Options.HitTriggers),
		MakeUnitDefinition(TEXT("after_substitute"), Options.SubstituteTriggers),
		MakeUnitDefinition(TEXT("after_redirect"), Options.RedirectTriggers)
	};
	if (Options.bEquipHitWand)
	{
		Result.Repository.Definitions.Add(
			MakeWandDefinition(TEXT("after_wand"), Options.WandTriggers));
	}

	Result.State.CurrentPlayer = 0;
	Result.State.PriorityPlayer = 1;
	Result.State.TurnNumber = 2;
	Result.State.Phase = EWBGamePhase::Response;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = AttackerId;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 99;
	Result.State.Players = { Player0, Player1 };
	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	Result.State.GetMutableCardZoneStateForTest().PlayerZones =
		{ Zones0, Zones1 };

	Result.State.AddUnitForTest(MakeUnit(
		AttackerId,
		Options.bNPC ? -1 : 0,
		TEXT("after_attacker"),
		FWBTile(4, 4),
		8,
		Options.AttackDamage));
	Result.State.AddUnitForTest(MakeUnit(
		HitUnitId, 1, TEXT("after_hit"), FWBTile(4, 1),
		Options.HitHP, 0, Options.HitArmor));
	Result.State.AddUnitForTest(MakeUnit(
		SubstituteId, 1, TEXT("after_substitute"), FWBTile(3, 1), 8));
	Result.State.AddUnitForTest(MakeUnit(
		RedirectId, 1, TEXT("after_redirect"), FWBTile(4, 2), 8));
	if (Options.bFrozenHit)
	{
		Result.State.GetMutableUnitById(HitUnitId)->AddStatus(
			FName(TEXT("Frozen")), 1);
	}
	if (Options.bEquipHitWand)
	{
		FWBEquippedCardEntry Entry;
		Entry.Card.InstanceId = TEXT("after_wand_instance");
		Entry.Card.CardId = TEXT("after_wand");
		Entry.Card.OwnerPlayerId = 1;
		Entry.EquippedToUnitId = HitUnitId;
		Entry.SlotId = TEXT("wand_0");
		Entry.EquipOrder = 0;
		Result.State.GetMutableCardZoneStateForTest().EquippedCards.Add(Entry);
	}

	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.AuthorityKind = Options.bNPC
		? EWBAttackAuthorityKind::NeutralNPC
		: EWBAttackAuthorityKind::Player;
	Pending.Stage = EWBAttackContinuationStage::CalculateDamage;
	Pending.AttackerUnitId = AttackerId;
	Pending.DefenderUnitId = Options.bRedirect ? RedirectId : HitUnitId;
	Pending.OriginalAttackerUnitId = AttackerId;
	Pending.OriginalDefenderUnitId = HitUnitId;
	Pending.AttackingPlayerId = Options.bNPC ? -1 : 0;
	Pending.AttackerTile = FWBTile(4, 4);
	Pending.DefenderTile = Options.bRedirect
		? FWBTile(4, 2) : FWBTile(4, 1);
	Pending.DeclarationActionId = Options.bNPC
		? TEXT("npc_attack:u1:t2") : TEXT("attack:p0:u1:t2");
	Pending.ContinuationId = Options.bCounter
		? TEXT("after_counter_1") : TEXT("after_attack_1");
	Pending.bPrevented = Options.bPrevented;
	Pending.bCounter = Options.bCounter;
	if (Options.bBodyDouble)
	{
		Pending.DamageSubstitution.bActive = true;
		Pending.DamageSubstitution.ProtectedUnitId = HitUnitId;
		Pending.DamageSubstitution.SubstituteUnitId = SubstituteId;
	}
	Result.State.SetPendingAttackForTest(Pending);

	const FWBApplyActionResult Calculated =
		WBEffectRunner::CalculatePendingAttackDamage(Result.State);
	if (!Calculated.bOk)
	{
		Result.Reason = Calculated.Reason;
		return Result;
	}
	const FWBApplyActionResult Substituted =
		WBEffectRunner::ResolvePendingAttackDamageSubstitution(Result.State);
	if (!Substituted.bOk)
	{
		Result.Reason = Substituted.Reason;
		return Result;
	}
	if (Options.bRemoveHitBeforeCapture)
	{
		Result.State.GetMutableUnitById(Result.State.PendingAttack.DefenderUnitId)
			->RemoveUnitFromBoard();
	}
	Result.Collection = WBAfterDamageTrigger::CaptureBeforeDamage(
		Result.State, Result.Repository);
	if (!Result.Collection.bOk)
	{
		Result.Reason = Result.Collection.Reason;
		return Result;
	}
	if (Options.bRemoveHitBeforeCapture)
	{
		Result.bOk = true;
		return Result;
	}

	Result.Damage = WBEffectRunner::ApplyCalculatedPendingAttackDamage(
		Result.State, true);
	if (!Result.Damage.bOk)
	{
		Result.Reason = Result.Damage.Reason;
		return Result;
	}
	if (!WBAfterDamageTrigger::FinalizeContextAfterDamage(
		Result.State,
		Result.Damage.TraceEvents,
		Result.Collection,
		Result.Reason))
	{
		return Result;
	}
	Result.Resolution = WBAfterDamageTrigger::Resolve(
		Result.State, Result.Collection);
	Result.Reason = Result.Resolution.Reason;
	Result.bOk = Result.Resolution.bOk;
	return Result;
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

bool HasTriggerFrom(
	const FWBAfterDamageTriggerCollection& Collection,
	const int32 SourceUnitId,
	const FString& TriggerId)
{
	return Collection.Triggers.ContainsByPredicate(
		[SourceUnitId, &TriggerId](const FWBAfterDamageTriggerInstance& Trigger)
		{
			return Trigger.SourceUnitId == SourceUnitId
				&& Trigger.Definition.TriggerId == TriggerId;
		});
}
}

#define WB_AFTER_DAMAGE_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_AFTER_DAMAGE_TEST(FWBAfterDamageAfterApplyTest,
	"Wandbound.AfterDamage.Timing.AfterDamageOccursAfterApplyDamage")
bool FWBAfterDamageAfterApplyTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("root_attacker"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Applied HP damage is captured"),
		Result.Collection.Context.AppliedHPDamage, 3);
	TestEqual(TEXT("Damage is already applied before trigger"),
		Result.Collection.Context.FinalRecipientResultingHP, 5);
	TestTrue(TEXT("Trigger effect resolves after damage"),
		Result.State.GetUnitById(AttackerId)->HasStatus(FName(TEXT("Rooted"))));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageBeforePostHitTest,
	"Wandbound.AfterDamage.Timing.AfterDamageOccursBeforePostHit")
bool FWBAfterDamageBeforePostHitTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("root_attacker"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::DamageResolved,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Resolver advances to PostHit"),
		static_cast<int32>(Result.State.PendingAttack.Stage),
		static_cast<int32>(EWBAttackContinuationStage::PostHit));
	TestNotNull(TEXT("AfterDamage trace exists"), FindTrace(
		Result.Resolution.TraceEvents,
		FName(TEXT("after_damage_trigger_resolved"))));
	const FWBTraceEvent* Collected = FindTrace(
		Result.Resolution.TraceEvents,
		FName(TEXT("after_damage_trigger_collected")));
	const FWBTraceEvent* Resolved = FindTrace(
		Result.Resolution.TraceEvents,
		FName(TEXT("after_damage_trigger_resolved")));
	TestNotNull(TEXT("Collected trace exists"), Collected);
	TestTrue(TEXT("Collected trace precedes resolved trace"),
		Collected != nullptr && Resolved != nullptr && Collected < Resolved);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageBeforeCounterEligibilityTest,
	"Wandbound.AfterDamage.Timing.AfterDamageOccursBeforeCounterEligibility")
bool FWBAfterDamageBeforeCounterEligibilityTest::RunTest(const FString&)
{
	const FScenarioResult Result = RunScenario(FScenarioOptions());
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Automatic stage stops at existing PostHit checkpoint"),
		static_cast<int32>(Result.State.PendingAttack.Stage),
		static_cast<int32>(EWBAttackContinuationStage::PostHit));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageIdentityTest,
	"Wandbound.AfterDamage.Context.TracksAttackerHitAndFinalRecipient")
bool FWBAfterDamageIdentityTest::RunTest(const FString&)
{
	const FScenarioResult Result = RunScenario(FScenarioOptions());
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Attacker identity"), Result.Collection.Context.AttackerUnitId, AttackerId);
	TestEqual(TEXT("Hit identity"), Result.Collection.Context.HitUnitId, HitUnitId);
	TestEqual(TEXT("Final recipient identity"),
		Result.Collection.Context.FinalDamageRecipientUnitId, HitUnitId);
	TestEqual(TEXT("Attacker controller"), Result.Collection.Context.AttackerControllerId, 0);
	TestEqual(TEXT("Hit controller"), Result.Collection.Context.HitUnitControllerId, 1);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageBodyDoubleIdentityTest,
	"Wandbound.AfterDamage.Context.TracksBodyDoubleSubstitution")
bool FWBAfterDamageBodyDoubleIdentityTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bBodyDouble = true;
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Hero remains HitUnit"), Result.Collection.Context.HitUnitId, HitUnitId);
	TestEqual(TEXT("Substitute is final recipient"),
		Result.Collection.Context.FinalDamageRecipientUnitId, SubstituteId);
	TestTrue(TEXT("Substitution flag"), Result.Collection.Context.bDamageSubstituted);
	TestEqual(TEXT("Transferred applied HP damage"),
		Result.Collection.Context.AppliedHPDamage, 3);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageRedirectIdentityTest,
	"Wandbound.AfterDamage.Context.TracksRedirectedHitUnit")
bool FWBAfterDamageRedirectIdentityTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bRedirect = true;
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Redirect target is HitUnit"),
		Result.Collection.Context.HitUnitId, RedirectId);
	TestEqual(TEXT("Redirect target is recipient"),
		Result.Collection.Context.FinalDamageRecipientUnitId, RedirectId);
	TestFalse(TEXT("Redirect is not substitution"),
		Result.Collection.Context.bDamageSubstituted);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageOverkillAppliedDeltaTest,
	"Wandbound.AfterDamage.Context.AppliedDamageUsesClampedHPDelta")
bool FWBAfterDamageOverkillAppliedDeltaTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.AttackDamage = 9;
	Options.HitHP = 2;
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds after participant death"), Result.bOk);
	TestEqual(TEXT("Calculated damage remains nine"),
		Result.Collection.Context.CalculatedHPDamage, 9);
	TestEqual(TEXT("Applied damage is actual two HP delta"),
		Result.Collection.Context.AppliedHPDamage, 2);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamagePositiveRequirementTest,
	"Wandbound.AfterDamage.Requirement.PositiveHPDamageTriggerCollects")
bool FWBAfterDamagePositiveRequirementTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("positive"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Positive trigger resolves once"), Result.Resolution.ResolvedTriggerCount, 1);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageArmorZeroGateTest,
	"Wandbound.AfterDamage.Requirement.FullyArmoredHitDoesNotCollectPositiveTrigger")
bool FWBAfterDamageArmorZeroGateTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitArmor = 3;
	Options.HitTriggers.Add(MakeTrigger(TEXT("positive"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("No HP damage"), Result.Collection.Context.AppliedHPDamage, 0);
	TestEqual(TEXT("Positive trigger is filtered"), Result.Collection.Triggers.Num(), 0);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamagePreventZeroGateTest,
	"Wandbound.AfterDamage.Requirement.PreventedAttackDoesNotCollectPositiveTrigger")
bool FWBAfterDamagePreventZeroGateTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bPrevented = true;
	Options.HitTriggers.Add(MakeTrigger(TEXT("positive"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestTrue(TEXT("Prevention represented"), Result.Collection.Context.bPrevented);
	TestEqual(TEXT("Positive trigger is filtered"), Result.Collection.Triggers.Num(), 0);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageFrozenZeroGateTest,
	"Wandbound.AfterDamage.Requirement.FrozenBreakDoesNotCollectPositiveTrigger")
bool FWBAfterDamageFrozenZeroGateTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bFrozenHit = true;
	Options.AttackerTriggers.Add(MakeTrigger(TEXT("positive"),
		EWBAfterDamageParticipantRole::Attacker,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Self));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestTrue(TEXT("Frozen break represented"), Result.Collection.Context.bFrozenBreak);
	TestEqual(TEXT("Positive trigger is filtered"), Result.Collection.Triggers.Num(), 0);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageZeroAttackGateTest,
	"Wandbound.AfterDamage.Requirement.ZeroAttackDoesNotCollectPositiveTrigger")
bool FWBAfterDamageZeroAttackGateTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.AttackDamage = 0;
	Options.AttackerTriggers.Add(MakeTrigger(TEXT("positive"),
		EWBAfterDamageParticipantRole::Attacker,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Self));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Zero applied"), Result.Collection.Context.AppliedHPDamage, 0);
	TestEqual(TEXT("Positive trigger is filtered"), Result.Collection.Triggers.Num(), 0);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageResolvedZeroTest,
	"Wandbound.AfterDamage.Requirement.DamageResolvedCanObserveZeroHPDamage")
bool FWBAfterDamageResolvedZeroTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitArmor = 3;
	Options.HitTriggers.Add(MakeTrigger(TEXT("resolved"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::DamageResolved,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Resolved trigger retained"), Result.Collection.Triggers.Num(), 1);
	TestEqual(TEXT("Resolved trigger executes"), Result.Resolution.ResolvedTriggerCount, 1);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageLethalSnapshotTest,
	"Wandbound.AfterDamage.Snapshot.LethallyDestroyedHitUnitTriggerStillResolves")
bool FWBAfterDamageLethalSnapshotTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.AttackDamage = 3;
	Options.HitHP = 3;
	Options.HitTriggers.Add(MakeTrigger(TEXT("last_word"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestFalse(TEXT("Source is removed"), Result.State.GetUnitById(HitUnitId)->IsUnitOnBoard());
	TestTrue(TEXT("Snapshotted trigger still resolved"),
		Result.State.GetUnitById(AttackerId)->HasStatus(FName(TEXT("Rooted"))));
	TestFalse(TEXT("Removed defender clears continuation"), Result.State.HasPendingAttack());
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageRemovedBeforeCaptureTest,
	"Wandbound.AfterDamage.Snapshot.RemovedBeforeDamageSourceDoesNotTrigger")
bool FWBAfterDamageRemovedBeforeCaptureTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bRemoveHitBeforeCapture = true;
	Options.HitTriggers.Add(MakeTrigger(TEXT("removed"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::DamageResolved,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Capture completes"), Result.bOk);
	TestEqual(TEXT("Removed source not captured"), Result.Collection.Triggers.Num(), 0);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageBodyDoubleRolesTest,
	"Wandbound.AfterDamage.Roles.BodyDoubleKeepsHitAndFinalRecipientDistinct")
bool FWBAfterDamageBodyDoubleRolesTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bBodyDouble = true;
	Options.HitTriggers.Add(MakeTrigger(TEXT("hit_role"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	Options.SubstituteTriggers.Add(MakeTrigger(TEXT("recipient_role"),
		EWBAfterDamageParticipantRole::FinalDamageRecipient,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	Options.SubstituteTriggers.Add(MakeTrigger(TEXT("not_battle"),
		EWBAfterDamageParticipantRole::BattleParticipant,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestTrue(TEXT("Hit role sees protected Hero"),
		HasTriggerFrom(Result.Collection, HitUnitId, TEXT("hit_role")));
	TestTrue(TEXT("Final recipient role sees substitute"),
		HasTriggerFrom(Result.Collection, SubstituteId, TEXT("recipient_role")));
	TestFalse(TEXT("Substitute is not battle participant"),
		HasTriggerFrom(Result.Collection, SubstituteId, TEXT("not_battle")));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageOpposingTargetBodyDoubleTest,
	"Wandbound.AfterDamage.Target.OpposingBattleUnitExcludesBodyDoubleRecipient")
bool FWBAfterDamageOpposingTargetBodyDoubleTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bBodyDouble = true;
	Options.AttackerTriggers.Add(MakeTrigger(TEXT("opposing"),
		EWBAfterDamageParticipantRole::Attacker,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::OpposingBattleUnit));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	const FWBTraceEvent* Trace = FindTrace(Result.Resolution.TraceEvents,
		FName(TEXT("after_damage_trigger_resolved")));
	TestNotNull(TEXT("Trigger trace exists"), Trace);
	if (Trace != nullptr)
	{
		TestEqual(TEXT("Opposing battle unit is HitUnit Hero"), Trace->TargetUnitId, HitUnitId);
		TestNotEqual(TEXT("Body Double is excluded"), Trace->TargetUnitId, SubstituteId);
	}
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageRedirectRolesTest,
	"Wandbound.AfterDamage.Roles.RedirectUsesRedirectTargetOnly")
bool FWBAfterDamageRedirectRolesTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bRedirect = true;
	Options.HitTriggers.Add(MakeTrigger(TEXT("old_hit"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	Options.RedirectTriggers.Add(MakeTrigger(TEXT("redirect_hit"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	Options.SubstituteTriggers.Add(MakeTrigger(TEXT("unused_substitute"),
		EWBAfterDamageParticipantRole::FinalDamageRecipient,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestFalse(TEXT("Original target does not collect"),
		HasTriggerFrom(Result.Collection, HitUnitId, TEXT("old_hit")));
	TestTrue(TEXT("Redirect target collects"),
		HasTriggerFrom(Result.Collection, RedirectId, TEXT("redirect_hit")));
	TestFalse(TEXT("Unused substitute does not collect"),
		HasTriggerFrom(Result.Collection, SubstituteId, TEXT("unused_substitute")));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageOncePerTurnTest,
	"Wandbound.AfterDamage.Usage.OncePerTurnAfterDamageWorks")
bool FWBAfterDamageOncePerTurnTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	FWBAfterDamageTriggerDefinition Trigger = MakeTrigger(TEXT("once"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker);
	Trigger.bOncePerTurn = true;
	Options.HitTriggers.Add(Trigger);
	FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("First resolution succeeds"), Result.bOk);
	const FString Key = WBAfterDamageTrigger::BuildUsageKey(
		Result.Collection.Triggers[0], Result.Collection.Context);
	TestTrue(TEXT("Usage marked only after resolution"),
		Result.State.HasActivationUsageKeyThisTurn(1, Key));
	Result.State.PendingAttack.Stage = EWBAttackContinuationStage::AfterDamage;
	FWBAfterDamageTriggerResolutionResult Again =
		WBAfterDamageTrigger::Resolve(Result.State, Result.Collection);
	TestTrue(TEXT("Repeated event batch resolves deterministically"), Again.bOk);
	TestEqual(TEXT("Once-per-turn trigger is skipped"), Again.ResolvedTriggerCount, 0);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageOncePerOpposingTest,
	"Wandbound.AfterDamage.Usage.OncePerTurnPerOpposingUnitUsesBattleOpponent")
bool FWBAfterDamageOncePerOpposingTest::RunTest(const FString&)
{
	FWBAfterDamageTriggerDefinition Definition = MakeTrigger(TEXT("per_opponent"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker);
	Definition.bOncePerTurnPerOpposingUnit = true;
	FScenarioOptions Options;
	Options.bBodyDouble = true;
	Options.HitTriggers.Add(Definition);
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	const FString Key = WBAfterDamageTrigger::BuildUsageKey(
		Result.Collection.Triggers[0], Result.Collection.Context);
	TestTrue(TEXT("Key uses attacker as opposing battle unit"),
		Key.EndsWith(TEXT(":opposing:u1")));
	TestFalse(TEXT("Key does not use Body Double recipient"),
		Key.EndsWith(TEXT(":opposing:u3")));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageGenericEffectRequestTest,
	"Wandbound.AfterDamage.Resolution.MandatoryUsesGenericEffectRequest")
bool FWBAfterDamageGenericEffectRequestTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("generic"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::DamageResolved,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestNotNull(TEXT("Generic request trace emitted"), FindTrace(
		Result.Resolution.TraceEvents, FName(TEXT("effect_request_resolved"))));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageOptionalFailsClosedTest,
	"Wandbound.AfterDamage.Resolution.OptionalFailsClosedUnsupported")
bool FWBAfterDamageOptionalFailsClosedTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("optional"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::DamageResolved,
		EWBAfterDamageTargetRole::Attacker));
	FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Mandatory control succeeds first"), Result.bOk);
	Result.Collection.Triggers[0].Definition.bMandatory = false;
	Result.State.PendingAttack.Stage = EWBAttackContinuationStage::AfterDamage;
	const FWBAfterDamageTriggerResolutionResult Unsupported =
		WBAfterDamageTrigger::Resolve(Result.State, Result.Collection);
	TestFalse(TEXT("Optional trigger denied"), Unsupported.bOk);
	TestEqual(TEXT("Precise optional diagnostic"), Unsupported.Reason,
		FString(TEXT("optional_after_damage_trigger_unsupported")));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageUnitAndWandCollectionTest,
	"Wandbound.AfterDamage.Collection.UnitAndEquippedWandSources")
bool FWBAfterDamageUnitAndWandCollectionTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bEquipHitWand = true;
	Options.HitTriggers.Add(MakeTrigger(TEXT("unit_source"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	Options.WandTriggers.Add(MakeTrigger(TEXT("wand_source"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Both source categories collect"), Result.Collection.Triggers.Num(), 2);
	TestEqual(TEXT("Unit orders before Wand"),
		static_cast<int32>(Result.Collection.Triggers[0].SourceKind),
		static_cast<int32>(EWBAfterDamageTriggerSourceKind::Unit));
	TestEqual(TEXT("Wand source is typed"),
		static_cast<int32>(Result.Collection.Triggers[1].SourceKind),
		static_cast<int32>(EWBAfterDamageTriggerSourceKind::EquippedWand));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageSuppressionTest,
	"Wandbound.AfterDamage.Collection.UnitSuppressionDoesNotSuppressEquippedWand")
bool FWBAfterDamageSuppressionTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bEquipHitWand = true;
	Options.bFrozenHit = true;
	Options.HitTriggers.Add(MakeTrigger(TEXT("unit_source"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::DamageResolved,
		EWBAfterDamageTargetRole::Attacker));
	Options.WandTriggers.Add(MakeTrigger(TEXT("wand_source"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::DamageResolved,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestFalse(TEXT("Frozen suppresses unit-origin passive"),
		HasTriggerFrom(Result.Collection, HitUnitId, TEXT("unit_source")));
	TestTrue(TEXT("Frozen does not suppress equipped Wand-origin passive"),
		HasTriggerFrom(Result.Collection, HitUnitId, TEXT("wand_source")));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageLethalWandSnapshotTest,
	"Wandbound.AfterDamage.Snapshot.LethalEquippedWandTriggerPersists")
bool FWBAfterDamageLethalWandSnapshotTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bEquipHitWand = true;
	Options.HitHP = 3;
	Options.WandTriggers.Add(MakeTrigger(TEXT("wand_last_word"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestTrue(TEXT("Wand trigger survives equipment cleanup snapshot"),
		Result.State.GetUnitById(AttackerId)->HasStatus(FName(TEXT("Rooted"))));
	TestTrue(TEXT("Equipment moved away during death cleanup"),
		Result.State.GetCardZoneState().EquippedCards.IsEmpty());
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageCounterContextTest,
	"Wandbound.AfterDamage.Integration.CounterAttackUsesSameCollector")
bool FWBAfterDamageCounterContextTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bCounter = true;
	Options.AttackerTriggers.Add(MakeTrigger(TEXT("counter_source"),
		EWBAfterDamageParticipantRole::Attacker,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::HitUnit));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestTrue(TEXT("Counter flag captured"), Result.Collection.Context.bCounterAttack);
	const FWBTraceEvent* Trace = FindTrace(Result.Resolution.TraceEvents,
		FName(TEXT("after_damage_trigger_resolved")));
	TestTrue(TEXT("Trigger trace is counter-aware"), Trace != nullptr && Trace->bCounterAttack);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageNPCContextTest,
	"Wandbound.AfterDamage.Integration.NPCAttackUsesSameCollector")
bool FWBAfterDamageNPCContextTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.bNPC = true;
	Options.AttackerTriggers.Add(MakeTrigger(TEXT("npc_source"),
		EWBAfterDamageParticipantRole::Attacker,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::HitUnit));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("Neutral controller retained"),
		Result.Collection.Context.AttackerControllerId, -1);
	TestEqual(TEXT("NPC source trigger resolves"),
		Result.Resolution.ResolvedTriggerCount, 1);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageTerminalTest,
	"Wandbound.AfterDamage.Terminal.StopsPostHitAndCounter")
bool FWBAfterDamageTerminalTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("terminal"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker,
		MakeDamagePayload(99)));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Trigger resolution succeeds"), Result.bOk);
	TestTrue(TEXT("Trigger can create terminal state"), Result.State.bGameOver);
	TestFalse(TEXT("Terminal death clears pending continuation"),
		Result.State.HasPendingAttack());
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageRemovalSuppressesContinuationTest,
	"Wandbound.AfterDamage.Terminal.RemovalSuppressesStaleCounter")
bool FWBAfterDamageRemovalSuppressesContinuationTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitHP = 4;
	Options.HitTriggers.Add(MakeTrigger(TEXT("self_remove"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Self,
		MakeDamagePayload(99)));
	const FScenarioResult Result = RunScenario(Options);
	TestTrue(TEXT("Trigger resolution succeeds"), Result.bOk);
	TestFalse(TEXT("AfterDamage removal clears attack"), Result.State.HasPendingAttack());
	TestFalse(TEXT("Removed defender cannot counter"),
		Result.State.GetUnitById(HitUnitId)->IsUnitOnBoard());
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageReplayDeterminismTest,
	"Wandbound.AfterDamage.Replay.DeterministicStateAndTrace")
bool FWBAfterDamageReplayDeterminismTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("deterministic"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult A = RunScenario(Options);
	const FScenarioResult B = RunScenario(Options);
	TestTrue(TEXT("First succeeds"), A.bOk);
	TestTrue(TEXT("Second succeeds"), B.bOk);
	TestEqual(TEXT("State digest matches"),
		WBProductionMatchReplay::BuildGameStateDigest(A.State),
		WBProductionMatchReplay::BuildGameStateDigest(B.State));
	TestEqual(TEXT("Trace digest matches"),
		WBProductionMatchReplay::BuildTraceDigest(A.Resolution.TraceEvents),
		WBProductionMatchReplay::BuildTraceDigest(B.Resolution.TraceEvents));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageNoTriggerChurnTest,
	"Wandbound.AfterDamage.Replay.NoTriggerAddsNoTraceChurn")
bool FWBAfterDamageNoTriggerChurnTest::RunTest(const FString&)
{
	const FScenarioResult Result = RunScenario(FScenarioOptions());
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestEqual(TEXT("No trigger traces"), Result.Resolution.TraceEvents.Num(), 0);
	TestEqual(TEXT("No trigger count"), Result.Resolution.ResolvedTriggerCount, 0);
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamagePrivacyTest,
	"Wandbound.AfterDamage.Privacy.TraceContainsNoHiddenZoneIdentity")
bool FWBAfterDamagePrivacyTest::RunTest(const FString&)
{
	FScenarioOptions Options;
	Options.HitTriggers.Add(MakeTrigger(TEXT("public_source"),
		EWBAfterDamageParticipantRole::HitUnit,
		EWBAfterDamageRequirement::PositiveHPDamage,
		EWBAfterDamageTargetRole::Attacker));
	const FScenarioResult Result = RunScenario(Options);
	const FString Serialized = WBReplayTrace::SerializeEvents(
		Result.Resolution.TraceEvents);
	TestTrue(TEXT("Scenario succeeds"), Result.bOk);
	TestFalse(TEXT("No hidden Hand token"), Serialized.Contains(TEXT("hidden_hand")));
	TestFalse(TEXT("No filesystem path"), Serialized.Contains(FPaths::ProjectDir()));
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageCardIdAuthorityGuardTest,
	"Wandbound.AfterDamage.Authority.NoAfterDamageCardIdAuthorityBranch")
bool FWBAfterDamageCardIdAuthorityGuardTest::RunTest(const FString&)
{
	const TArray<FString> RelativePaths = {
		TEXT("Source/WandboundCore/Private/WBAfterDamageTrigger.cpp"),
		TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp"),
		TEXT("Source/WandboundCore/Private/WBRules.cpp"),
		TEXT("Source/WandboundCore/Private/WBEffectRunner.cpp"),
		TEXT("Source/WandboundCore/Private/WBCardDefinitionRepository.cpp"),
		TEXT("Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp")
	};
	const TArray<FString> ForbiddenCardTokens = {
		TEXT("sumpgrip"),
		TEXT("bracing_band"),
		TEXT("saeryn"),
		TEXT("afterglow"),
		TEXT("poison_on_hit"),
		TEXT("lava_burn")
	};
	for (const FString& RelativePath : RelativePaths)
	{
		FString Source;
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), RelativePath);
		TestTrue(*FString::Printf(TEXT("Loads %s"), *RelativePath),
			FFileHelper::LoadFileToString(Source, *Path));
		Source.ToLowerInline();
		for (const FString& Token : ForbiddenCardTokens)
		{
			TestFalse(
				*FString::Printf(
					TEXT("%s contains no authority token %s"),
					*RelativePath,
					*Token),
				Source.Contains(Token));
		}
	}
	return true;
}

WB_AFTER_DAMAGE_TEST(FWBAfterDamageProductionSchemaTest,
	"Wandbound.AfterDamage.CardDB.TypedProductionFixtureLoads")
bool FWBAfterDamageProductionSchemaTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/AfterDamageTriggerFixture/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	if (!Loaded.bOk)
	{
		for (const FWBProductionCardDBDiagnostic& Diagnostic : Loaded.Diagnostics)
		{
			AddError(FString::Printf(TEXT("%s | %s | %s"),
				*Diagnostic.Code, *Diagnostic.FieldPath, *Diagnostic.Message));
		}
	}
	TestTrue(TEXT("Production fixture loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid())
	{
		return false;
	}
	const FWBProductionCardRecord* Hero = Loaded.Snapshot->FindHero(
		TEXT("after_damage_fixture_hero_beta"));
	TestNotNull(TEXT("Triggered Hero exists"), Hero);
	if (Hero != nullptr)
	{
		TestEqual(TEXT("One typed trigger parsed"),
			Hero->CoreDefinition.AfterDamageTriggers.Num(), 1);
		TestEqual(TEXT("Typed target parsed"),
			static_cast<int32>(Hero->CoreDefinition.AfterDamageTriggers[0].TargetRole),
			static_cast<int32>(EWBAfterDamageTargetRole::OpposingBattleUnit));
	}
	AddInfo(FString::Printf(TEXT("AFTER_DAMAGE_FIXTURE_DIGEST=%s"),
		*Loaded.Snapshot->ContentDigest));
	return true;
}

#undef WB_AFTER_DAMAGE_TEST

#endif
