#include "Misc/AutomationTest.h"

#include "WBAfterDamageTrigger.h"
#include "WBCSNInheritanceTrigger.h"
#include "WBDeathResolution.h"
#include "WBEffectRunner.h"
#include "WBEventSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBUnitState MakeSnapshotUnit(
	const int32 UnitId,
	const int32 OwnerPlayerId,
	const int32 ControllerPlayerId,
	const FString& CardId,
	const FWBTile Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.SetOwnerAndControllerForRules(OwnerPlayerId, ControllerPlayerId);
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 8;
	Unit.MaxHP = 8;
	Unit.ATK = 3;
	Unit.AR = 2;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetCanonicalRL(2, 2, 0);
	return Unit;
}

FWBGameStateData MakeSnapshotState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 7;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = 10;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 20;
	State.Players = { Player0, Player1 };
	State.AddUnitForTest(MakeSnapshotUnit(
		10, 0, 1, TEXT("snapshot_source"), FWBTile(3, 4)));
	State.AddUnitForTest(MakeSnapshotUnit(
		20, 1, 1, TEXT("snapshot_target"), FWBTile(3, 5)));
	return State;
}

FWBCardDefinition MakeAfterDamageDefinition(
	const FString& CardId,
	const FString& TriggerId)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Character;
	if (!TriggerId.IsEmpty())
	{
		FWBAfterDamageTriggerDefinition Trigger;
		Trigger.TriggerId = TriggerId;
		Trigger.SourceRole = EWBAfterDamageParticipantRole::Attacker;
		Trigger.DamageRequirement = EWBAfterDamageRequirement::DamageResolved;
		Trigger.TargetRole = EWBAfterDamageTargetRole::Attacker;
		FWBGenericEffectPayload Payload;
		Payload.Operation = EWBGenericEffectOp::HealEffect;
		Payload.HealEffect.Amount = 1;
		Trigger.Payloads.Add(Payload);
		Definition.AfterDamageTriggers.Add(Trigger);
	}
	return Definition;
}

FWBAfterDamageTriggerCollection CaptureSubstitutedDamage(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	State.AddUnitForTest(MakeSnapshotUnit(
		30, 1, 1, TEXT("snapshot_substitute"), FWBTile(4, 5)));
	FWBPendingAttackState Attack;
	Attack.bActive = true;
	Attack.Stage = EWBAttackContinuationStage::ApplyDamage;
	Attack.AttackerUnitId = 10;
	Attack.DefenderUnitId = 20;
	Attack.OriginalAttackerUnitId = 10;
	Attack.OriginalDefenderUnitId = 20;
	Attack.FinalDamageRecipientUnitId = 30;
	Attack.AttackingPlayerId = 1;
	Attack.DeclarationActionId = TEXT("attack:p1:u10:t20");
	Attack.ContinuationId = TEXT("attack_continuation:attack:p1:u10:t20");
	Attack.AttackDeclaration = EWBDeclarationProvenance::PlayerDeclared;
	Attack.TargetDeclaration = EWBDeclarationProvenance::PlayerDeclared;
	Attack.DamageCalculation.bValid = true;
	Attack.DamageCalculation.HitUnitId = 20;
	Attack.DamageCalculation.RawAttackDamage = 3;
	Attack.DamageCalculation.PreviousHP = 8;
	Attack.DamageCalculation.CalculatedHPDamage = 3;
	State.PendingAttack = Attack;
	return WBAfterDamageTrigger::CaptureBeforeDamage(State, Repository);
}
}

#define WB_TRIGGER_SNAPSHOT_TEST(ClassName, Path) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, Path, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_TRIGGER_SNAPSHOT_TEST(FWBEventIdentitySnapshotTest,
	"Wandbound.TriggerSnapshotFoundation.EventIdentity.DeterministicTypedIdentity")
bool FWBEventIdentitySnapshotTest::RunTest(const FString&)
{
	const FWBEventIdentitySnapshot A = WBEventSnapshot::MakeIdentity(
		EWBEventKind::Attack, TEXT("event:7:1"), 7,
		TEXT("attack:p0:u1:t2"), TEXT("attack_continuation:1"),
		EWBDeclarationProvenance::Automatic,
		EWBDeclarationProvenance::PlayerDeclared);
	const FWBEventIdentitySnapshot B = WBEventSnapshot::MakeIdentity(
		EWBEventKind::Attack, TEXT("event:7:1"), 7,
		TEXT("attack:p0:u1:t2"), TEXT("attack_continuation:1"),
		EWBDeclarationProvenance::Automatic,
		EWBDeclarationProvenance::PlayerDeclared);
	TestTrue(TEXT("identity valid"), A.IsValid());
	TestEqual(TEXT("deterministic id"), A.EventId, B.EventId);
	TestEqual(TEXT("typed kind"), A.Kind, EWBEventKind::Attack);
	TestEqual(TEXT("counter-like attack is automatic"),
		A.ActionDeclaration, EWBDeclarationProvenance::Automatic);
	TestEqual(TEXT("selected target remains declared"),
		A.TargetDeclaration, EWBDeclarationProvenance::PlayerDeclared);
	return true;
}

WB_TRIGGER_SNAPSHOT_TEST(FWBParticipantSnapshotStabilityTest,
	"Wandbound.TriggerSnapshotFoundation.Participant.OwnerControllerHeroAndTileStable")
bool FWBParticipantSnapshotStabilityTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeSnapshotState();
	FWBUnitState* Unit = State.GetMutableUnitById(10);
	const FWBUnitParticipantSnapshot Snapshot =
		WBEventSnapshot::CaptureUnitParticipant(State, *Unit);
	Unit->SetOwnerAndControllerForRules(0, 0);
	Unit->CardId = TEXT("replacement_identity");
	Unit->X = 8;
	Unit->Y = 8;
	State.GetMutablePlayerById(0)->HeroUnitId = INDEX_NONE;
	TestEqual(TEXT("owner is historical"), Snapshot.OwnerPlayerId, 0);
	TestEqual(TEXT("controller is historical"), Snapshot.ControllerPlayerId, 1);
	TestEqual(TEXT("card is historical"), Snapshot.CardId,
		FString(TEXT("snapshot_source")));
	TestTrue(TEXT("tile is historical"), Snapshot.Tile == FWBTile(3, 4));
	TestTrue(TEXT("hero identity is historical"), Snapshot.bWasHero);
	return true;
}

WB_TRIGGER_SNAPSHOT_TEST(FWBSourceCasterProvenanceTest,
	"Wandbound.TriggerSnapshotFoundation.Source.CasterRequiresActivation")
bool FWBSourceCasterProvenanceTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeSnapshotState();
	const FWBUnitState* Unit = State.GetUnitById(10);
	const FWBEventSourceSnapshot Declared = WBEventSnapshot::CaptureUnitSource(
		State, *Unit, EWBActivationProvenance::PlayerDeclared);
	const FWBEventSourceSnapshot Automatic = WBEventSnapshot::CaptureUnitSource(
		State, *Unit, EWBActivationProvenance::AutomaticActivation);
	const FWBEventSourceSnapshot ResolutionOnly =
		WBEventSnapshot::CaptureUnitSource(State, *Unit);
	TestEqual(TEXT("declared activation has caster"),
		Declared.GetCasterUnitId(), 10);
	TestEqual(TEXT("automatic activation has caster"),
		Automatic.GetCasterUnitId(), 10);
	TestEqual(TEXT("resolution-only source has no caster"),
		ResolutionOnly.GetCasterUnitId(), INDEX_NONE);
	return true;
}

WB_TRIGGER_SNAPSHOT_TEST(FWBStatusSourceSnapshotAdapterTest,
	"Wandbound.TriggerSnapshotFoundation.Status.PreservesAuthorityWithoutFakeCaster")
bool FWBStatusSourceSnapshotAdapterTest::RunTest(const FString&)
{
	FWBStatusSourceProvenance Status;
	Status.SourcePlayerId = 1;
	Status.SourceOwnerPlayerId = 0;
	Status.SourceUnitId = 10;
	Status.SourceCardId = TEXT("snapshot_source");
	Status.SourceCardInstanceId = TEXT("wand-instance-4");
	Status.Origin = EWBStatusApplicationOrigin::Activation;
	const FWBEventSourceSnapshot Snapshot =
		WBEventSnapshot::FromStatusSource(Status);
	TestEqual(TEXT("owner preserved"), Snapshot.OwnerPlayerId, 0);
	TestEqual(TEXT("controller preserved"), Snapshot.ControllerPlayerId, 1);
	TestEqual(TEXT("card instance preserved"), Snapshot.SourceCardInstanceId,
		FString(TEXT("wand-instance-4")));
	TestEqual(TEXT("status adapter does not invent caster"),
		Snapshot.GetCasterUnitId(), INDEX_NONE);
	return true;
}

WB_TRIGGER_SNAPSHOT_TEST(FWBDestructionSnapshotCompositionTest,
	"Wandbound.TriggerSnapshotFoundation.Destruction.HistoricalSourceAndHybridObserver")
bool FWBDestructionSnapshotCompositionTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeSnapshotState();
	FWBUnitDestructionSnapshot Snapshot;
	FString Reason;
	TestTrue(TEXT("destruction snapshot captured"),
		WBDeathResolution::BuildSuccessfulDestructionSnapshot(
			State, 10, EWBUnitDestructionCause::BattleDamage, 2,
			Snapshot, Reason));
	State.GetMutableUnitById(10)->RemoveUnitFromBoard();
	State.GetMutableUnitById(20)->SetOwnerAndControllerForRules(1, 0);
	TestEqual(TEXT("event kind"), Snapshot.EventIdentity.Kind,
		EWBEventKind::Destruction);
	TestEqual(TEXT("destroyed owner snapshot"),
		Snapshot.DestroyedUnitSnapshot.OwnerPlayerId, 0);
	TestEqual(TEXT("destroyed controller snapshot"),
		Snapshot.DestroyedUnitSnapshot.ControllerPlayerId, 1);
	TestTrue(TEXT("former tile snapshot"),
		Snapshot.DestroyedUnitSnapshot.Tile == FWBTile(3, 4));
	TestEqual(TEXT("observer captured"), Snapshot.ObserverSources.Num(), 1);
	if (Snapshot.ObserverSources.Num() == 1)
	{
		TestEqual(TEXT("observer controller remains historical"),
			Snapshot.ObserverSources[0].SourceSnapshot.ControllerPlayerId, 1);
		TestEqual(TEXT("observer policy is hybrid"),
			Snapshot.ObserverSources[0].EligibilityPolicy,
			EWBTriggerEligibilityPolicy::Hybrid);
	}
	return true;
}

WB_TRIGGER_SNAPSHOT_TEST(FWBAfterDamageParticipantSnapshotTest,
	"Wandbound.TriggerSnapshotFoundation.AfterDamage.SubstitutionRolesRemainDistinct")
bool FWBAfterDamageParticipantSnapshotTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeSnapshotState();
	FWBCardDefinitionRepository Repository;
	Repository.Definitions =
	{
		MakeAfterDamageDefinition(TEXT("snapshot_source"), TEXT("heal_attacker")),
		MakeAfterDamageDefinition(TEXT("snapshot_target"), FString()),
		MakeAfterDamageDefinition(TEXT("snapshot_substitute"), FString())
	};
	const FWBAfterDamageTriggerCollection Collection =
		CaptureSubstitutedDamage(State, Repository);
	TestTrue(TEXT("capture succeeds"), Collection.bOk);
	TestEqual(TEXT("typed event kind"), Collection.Context.EventIdentity.Kind,
		EWBEventKind::AfterDamage);
	TestEqual(TEXT("hit role preserved"),
		Collection.Context.HitUnitSnapshot.UnitId, 20);
	TestEqual(TEXT("final recipient role preserved"),
		Collection.Context.FinalDamageRecipientSnapshot.UnitId, 30);
	TestTrue(TEXT("substitution recorded"),
		Collection.Context.bDamageSubstituted);
	TestEqual(TEXT("trigger captured"), Collection.Triggers.Num(), 1);
	if (Collection.Triggers.Num() == 1)
	{
		TestEqual(TEXT("snapshot eligibility"),
			Collection.Triggers[0].EligibilityPolicy,
			EWBTriggerEligibilityPolicy::SnapshotAtCollection);
		TestEqual(TEXT("source controller historical"),
			Collection.Triggers[0].SourceSnapshot.ControllerPlayerId, 1);
	}
	return true;
}

WB_TRIGGER_SNAPSHOT_TEST(FWBEquippedWandSourceSnapshotTest,
	"Wandbound.TriggerSnapshotFoundation.AfterDamage.EquippedWandExactInstanceNoCaster")
bool FWBEquippedWandSourceSnapshotTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeSnapshotState();
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = 0;
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(Zones);
	FWBEquippedCardEntry Wand;
	Wand.Card.CardId = TEXT("snapshot_wand");
	Wand.Card.InstanceId = TEXT("snapshot_wand_instance_3");
	Wand.Card.OwnerPlayerId = 0;
	Wand.EquippedToUnitId = 10;
	Wand.EquipOrder = 3;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Wand);
	FWBCardDefinition WandDefinition;
	WandDefinition.CardId = TEXT("snapshot_wand");
	WandDefinition.PublicName = TEXT("Snapshot Wand");
	WandDefinition.Kind = EWBCardDefinitionKind::Wand;
	FWBAfterDamageTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("wand_after_damage");
	Trigger.SourceRole = EWBAfterDamageParticipantRole::Attacker;
	Trigger.DamageRequirement = EWBAfterDamageRequirement::DamageResolved;
	Trigger.TargetRole = EWBAfterDamageTargetRole::Attacker;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Trigger.Payloads.Add(Payload);
	WandDefinition.AfterDamageTriggers.Add(Trigger);
	FWBCardDefinitionRepository Repository;
	Repository.Definitions =
	{
		MakeAfterDamageDefinition(TEXT("snapshot_source"), FString()),
		MakeAfterDamageDefinition(TEXT("snapshot_target"), FString()),
		MakeAfterDamageDefinition(TEXT("snapshot_substitute"), FString()),
		WandDefinition
	};
	const FWBAfterDamageTriggerCollection Collection =
		CaptureSubstitutedDamage(State, Repository);
	TestTrue(TEXT("capture succeeds"), Collection.bOk);
	TestEqual(TEXT("wand trigger captured"), Collection.Triggers.Num(), 1);
	if (Collection.Triggers.Num() == 1)
	{
		const FWBEventSourceSnapshot& Source =
			Collection.Triggers[0].SourceSnapshot;
		TestEqual(TEXT("bearer source unit"), Source.SourceUnitId, 10);
		TestEqual(TEXT("exact wand instance"), Source.SourceCardInstanceId,
			FString(TEXT("snapshot_wand_instance_3")));
		TestEqual(TEXT("automatic passive has no caster"),
			Source.GetCasterUnitId(), INDEX_NONE);
	}
	return true;
}

WB_TRIGGER_SNAPSHOT_TEST(FWBPersistentContextTypesTest,
	"Wandbound.TriggerSnapshotFoundation.PendingContexts.TypedAndBounded")
bool FWBPersistentContextTypesTest::RunTest(const FString&)
{
	FWBPendingNPCSpawnState NPC;
	NPC.EventIdentity = WBEventSnapshot::MakeIdentity(
		EWBEventKind::NPCSpawn, TEXT("npc_spawn:t7:s0:m2"), 7);
	NPC.TriggeringUnitSnapshot.UnitId = 10;
	FWBCSNInheritanceEventContext Inheritance;
	Inheritance.EventIdentity = WBEventSnapshot::MakeIdentity(
		EWBEventKind::Inheritance, TEXT("inheritance:7:1"), 7,
		FString(), TEXT("inheritance:7:1"));
	Inheritance.EligibilityPolicy = EWBTriggerEligibilityPolicy::Hybrid;
	TestTrue(TEXT("NPC identity valid"), NPC.EventIdentity.IsValid());
	TestEqual(TEXT("NPC event kind"), NPC.EventIdentity.Kind,
		EWBEventKind::NPCSpawn);
	TestTrue(TEXT("inheritance identity valid"),
		Inheritance.EventIdentity.IsValid());
	TestEqual(TEXT("inheritance policy hybrid"),
		Inheritance.EligibilityPolicy, EWBTriggerEligibilityPolicy::Hybrid);
	return true;
}

#undef WB_TRIGGER_SNAPSHOT_TEST

#endif
