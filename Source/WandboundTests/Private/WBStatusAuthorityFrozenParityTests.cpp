#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBAction.h"
#include "WBCardActivationSourceGate.h"
#include "WBEffectRequest.h"
#include "WBEffectRunner.h"
#include "WBProductionMatchReplay.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayTrace.h"
#include "WBRules.h"
#include "WBStatusEffect.h"
#include "WBStatusSemantics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 ControllerId,
	const FWBTile Tile,
	const int32 ATK = 3)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.SetOwnerAndControllerForRules(ControllerId, ControllerId);
	Unit.CardId = FString::Printf(TEXT("status_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 8;
	Unit.MaxHP = 8;
	Unit.ATK = ATK;
	Unit.AR = 2;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 4;
	FWBPlayerStateData P0;
	P0.PlayerId = 0;
	P0.RemainingMP = 3;
	FWBPlayerStateData P1;
	P1.PlayerId = 1;
	P1.RemainingMP = 3;
	State.Players = { P0, P1 };
	State.AddUnitForTest(MakeUnit(1, 0, FWBTile(4, 4)));
	State.AddUnitForTest(MakeUnit(2, 1, FWBTile(4, 5)));
	return State;
}

FWBStatusEffectRequest MakeStatusRequest(
	const EWBStatusEffectOp Operation,
	const FName StatusId,
	const int32 Duration,
	const int32 TargetUnitId = 2)
{
	FWBStatusEffectRequest Request;
	Request.Operation = Operation;
	Request.TargetUnitId = TargetUnitId;
	Request.StatusId = StatusId;
	Request.Duration = Duration;
	return Request;
}

void PrimePendingAttack(
	FWBGameStateData& State,
	const int32 AttackerUnitId = 1,
	const int32 DefenderUnitId = 2,
	const bool bCounter = false)
{
	const FWBUnitState* Attacker = State.GetUnitById(AttackerUnitId);
	const FWBUnitState* Defender = State.GetUnitById(DefenderUnitId);
	FWBPendingAttackState Attack;
	Attack.bActive = true;
	Attack.Stage = EWBAttackContinuationStage::CalculateDamage;
	Attack.AttackerUnitId = AttackerUnitId;
	Attack.DefenderUnitId = DefenderUnitId;
	Attack.OriginalAttackerUnitId = AttackerUnitId;
	Attack.OriginalDefenderUnitId = DefenderUnitId;
	Attack.AttackingPlayerId = Attacker != nullptr
		? Attacker->GetControllerPlayerIdForRules() : INDEX_NONE;
	Attack.AttackerTile = Attacker != nullptr
		? FWBTile(Attacker->X, Attacker->Y) : FWBTile();
	Attack.DefenderTile = Defender != nullptr
		? FWBTile(Defender->X, Defender->Y) : FWBTile();
	Attack.DeclarationActionId = TEXT("status_foundation_attack");
	Attack.ContinuationId = TEXT("status_foundation_continuation");
	Attack.bCounter = bCounter;
	State.PendingAttack = Attack;
}

bool ResolveAttackThroughApply(FWBGameStateData& State)
{
	return WBEffectRunner::CalculatePendingAttackDamage(State).bOk
		&& WBEffectRunner::ResolvePendingAttackDamageSubstitution(State).bOk
		&& WBEffectRunner::ApplyCalculatedPendingAttackDamage(State, true).bOk;
}

TSharedPtr<FJsonObject> ParseObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}
}

#define WB_STATUS_FOUNDATION_TEST(ClassName, Path) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, Path, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_STATUS_FOUNDATION_TEST(FWBStatusCanonicalRegistryTest,
	"Wandbound.StatusAuthority.Canonicalization.RegistryAndAliases")
bool FWBStatusCanonicalRegistryTest::RunTest(const FString&)
{
	TestEqual(TEXT("Burn"), WBStatusSemantics::CanonicalizeStatusId(TEXT("burn")), FName(TEXT("Burn")));
	TestEqual(TEXT("Poison"), WBStatusSemantics::CanonicalizeStatusId(TEXT("POISON")), FName(TEXT("Poison")));
	TestEqual(TEXT("Root"), WBStatusSemantics::CanonicalizeStatusId(TEXT("root")), FName(TEXT("Rooted")));
	TestEqual(TEXT("Rooted"), WBStatusSemantics::CanonicalizeStatusId(TEXT("rooted")), FName(TEXT("Rooted")));
	TestEqual(TEXT("Stun"), WBStatusSemantics::CanonicalizeStatusId(TEXT("stun")), FName(TEXT("Stunned")));
	TestEqual(TEXT("Stunned"), WBStatusSemantics::CanonicalizeStatusId(TEXT("stunned")), FName(TEXT("Stunned")));
	TestEqual(TEXT("Frozen"), WBStatusSemantics::CanonicalizeStatusId(TEXT("frozen")), FName(TEXT("Frozen")));
	for (const FName Alias : { FName(TEXT("CannotAttack")), FName(TEXT("cannot_attack")), FName(TEXT("cannot attack")), FName(TEXT("no_attack")) })
	{
		TestEqual(TEXT("Cannot Attack alias"), WBStatusSemantics::CanonicalizeStatusId(Alias), FName(TEXT("Cannot Attack")));
	}
	TestEqual(TEXT("Negated remains distinct"), WBStatusSemantics::CanonicalizeStatusId(TEXT("Negated")), FName(TEXT("Negated")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusLegacyMirrorNormalizationTest,
	"Wandbound.StatusAuthority.Canonicalization.LegacyMirrorsConverge")
bool FWBStatusLegacyMirrorNormalizationTest::RunTest(const FString&)
{
	FWBUnitState Unit = MakeUnit(7, 0, FWBTile(3, 3));
	Unit.Statuses.Add(TEXT("root"));
	Unit.Statuses.Add(TEXT("Rooted"));
	Unit.StatusTurnsRemaining.Add(TEXT("root"), 2);
	Unit.StatusTurnsRemaining.Add(TEXT("Rooted"), 3);
	Unit.NormalizeStatusStateForRules();
	TestEqual(TEXT("One typed state"), Unit.StatusStates.Num(), 1);
	TestEqual(TEXT("One mirror"), Unit.Statuses.Num(), 1);
	TestTrue(TEXT("Canonical mirror"), Unit.Statuses.Contains(TEXT("Rooted")));
	TestEqual(TEXT("Maximum duration retained"), Unit.GetStatusTurnsRemaining(TEXT("Rooted")), 3);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusProvenanceSnapshotTest,
	"Wandbound.StatusAuthority.Provenance.EventTimeSnapshot")
bool FWBStatusProvenanceSnapshotTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBStatusEffectRequest Request = MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Burn"), 2);
	Request.Source.SourcePlayerId = 0;
	Request.Source.SourceOwnerPlayerId = 0;
	Request.Source.SourceUnitId = 1;
	Request.Source.SourceCardId = TEXT("source_card");
	Request.Source.SourceCardInstanceId = TEXT("source_instance_4");
	Request.Source.SourceEffectId = TEXT("burn_effect");
	Request.Source.Origin = EWBStatusApplicationOrigin::Activation;
	TestTrue(TEXT("Apply succeeds"), WBStatusEffect::ApplyStatusEffect(State, Request).bOk);
	const FWBStatusInstanceState* Status = State.GetUnitById(2)->GetStatusState(TEXT("Burn"));
	TestNotNull(TEXT("Typed status exists"), Status);
	if (Status != nullptr)
	{
		TestEqual(TEXT("Target"), Status->TargetUnitId, 2);
		TestEqual(TEXT("Source player"), Status->Source.SourcePlayerId, 0);
		TestEqual(TEXT("Source owner"), Status->Source.SourceOwnerPlayerId, 0);
		TestEqual(TEXT("Source unit"), Status->Source.SourceUnitId, 1);
		TestEqual(TEXT("Source card"), Status->Source.SourceCardId, FString(TEXT("source_card")));
		TestEqual(TEXT("Source instance"), Status->Source.SourceCardInstanceId, FString(TEXT("source_instance_4")));
		TestEqual(TEXT("Source effect"), Status->Source.SourceEffectId, FString(TEXT("burn_effect")));
	}
	State.GetMutableUnitById(1)->SetControllerPlayerIdForRules(1);
	State.GetMutableUnitById(1)->MarkUnitDefeated();
	State.GetMutableUnitById(1)->RemoveUnitFromBoard();
	State.GetMutableUnitById(2)->SetControllerPlayerIdForRules(0);
	Status = State.GetUnitById(2)->GetStatusState(TEXT("Burn"));
	TestEqual(TEXT("Historical controller remains"), Status->Source.SourcePlayerId, 0);
	TestEqual(TEXT("Historical owner remains"), Status->Source.SourceOwnerPlayerId, 0);
	TestEqual(TEXT("Historical unit remains"), Status->Source.SourceUnitId, 1);
	const FWBApplyActionResult TickResult = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);
	TestTrue(TEXT("Timed status continues after source removal and control changes"), TickResult.bOk);
	Status = State.GetUnitById(2)->GetStatusState(TEXT("Burn"));
	TestNotNull(TEXT("Status remains after first tick"), Status);
	if (Status != nullptr)
	{
		TestEqual(TEXT("Duration advances independently of source lifetime"), Status->Duration, 1);
		TestEqual(TEXT("Tick preserves historical source unit"), Status->Source.SourceUnitId, 1);
	}
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusEffectRequestProvenanceTest,
	"Wandbound.StatusAuthority.Provenance.GenericEffectSnapshotsSource")
bool FWBStatusEffectRequestProvenanceTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBEffectRequest Request;
	Request.Source.PlayerId = 0;
	Request.Source.SourceUnitId = 1;
	Request.Source.SourceCardId = TEXT("activation_card");
	Request.Source.SourceCardInstanceId = TEXT("activation_instance");
	Request.Source.SourceEffectId = TEXT("apply_poison");
	Request.Source.ActivationProvenance = EWBActivationProvenance::PlayerDeclared;
	Request.Target.TargetUnitId = 2;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect = MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Poison"), 2);
	Request.Payloads.Add(Payload);
	TestTrue(TEXT("Effect succeeds"), WBEffectRunner::ApplyEffectRequest(State, Request).bOk);
	const FWBStatusInstanceState* Status = State.GetUnitById(2)->GetStatusState(TEXT("Poison"));
	TestNotNull(TEXT("Status exists"), Status);
	if (Status != nullptr)
	{
		TestEqual(TEXT("Owner snapshot"), Status->Source.SourceOwnerPlayerId, 0);
		TestEqual(TEXT("Instance snapshot"), Status->Source.SourceCardInstanceId, FString(TEXT("activation_instance")));
		TestEqual(TEXT("Activation origin"), static_cast<int32>(Status->Source.Origin), static_cast<int32>(EWBStatusApplicationOrigin::Activation));
	}
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusNoFakeSourceTest,
	"Wandbound.StatusAuthority.Provenance.GameRuleDoesNotFabricateSource")
bool FWBStatusNoFakeSourceTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBStatusEffectRequest Request = MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Rooted"), 0);
	Request.Source.Origin = EWBStatusApplicationOrigin::GameRule;
	TestTrue(TEXT("Apply succeeds"), WBStatusEffect::ApplyStatusEffect(State, Request).bOk);
	const FWBStatusInstanceState* Status = State.GetUnitById(2)->GetStatusState(TEXT("Rooted"));
	TestEqual(TEXT("No fake source player"), Status->Source.SourcePlayerId, INDEX_NONE);
	TestEqual(TEXT("No fake source unit"), Status->Source.SourceUnitId, INDEX_NONE);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusPermanentDurationTest,
	"Wandbound.StatusAuthority.Duration.PermanentAddReduceRemainPermanent")
bool FWBStatusPermanentDurationTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Permanent apply"), WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Rooted"), 0)).bOk);
	TestTrue(TEXT("Add duration"), WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::AddStatusDuration, TEXT("Rooted"), 3)).bOk);
	TestEqual(TEXT("Still permanent after add"), State.GetUnitById(2)->GetStatusTurnsRemaining(TEXT("Rooted")), 0);
	TestTrue(TEXT("Reduce duration"), WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ReduceStatusDuration, TEXT("Rooted"), 3)).bOk);
	TestTrue(TEXT("Still active after reduce"), State.GetUnitById(2)->HasStatus(TEXT("Rooted")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusReapplicationRefreshTest,
	"Wandbound.StatusAuthority.Duration.ReapplicationUsesMaximum")
bool FWBStatusReapplicationRefreshTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Burn"), 3));
	WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("burn"), 1));
	TestEqual(TEXT("Shorter application does not shorten"), State.GetUnitById(2)->GetStatusTurnsRemaining(TEXT("Burn")), 3);
	WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Burn"), 5));
	TestEqual(TEXT("Longer application refreshes"), State.GetUnitById(2)->GetStatusTurnsRemaining(TEXT("Burn")), 5);
	TestEqual(TEXT("Single instance"), State.GetUnitById(2)->StatusStates.Num(), 1);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBPoisonReapplicationTickTest,
	"Wandbound.StatusAuthority.Poison.ReapplicationTicksThenRefreshes")
bool FWBPoisonReapplicationTickTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	WBEffectRunner::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Poison"), 3));
	const FWBApplyActionResult Result = WBEffectRunner::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("poison"), 2));
	TestTrue(TEXT("Reapply succeeds"), Result.bOk);
	TestEqual(TEXT("Max HP reduced once"), State.GetUnitById(2)->MaxHP, 7);
	TestEqual(TEXT("Duration keeps maximum"), State.GetUnitById(2)->GetStatusTurnsRemaining(TEXT("Poison")), 3);
	TestTrue(TEXT("Immediate tick traced"), Result.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& Event) { return Event.Kind == FName(TEXT("status_tick")); }));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusCleanseProtectionTest,
	"Wandbound.StatusAuthority.Cleanse.AllPreservesInternalState")
bool FWBStatusCleanseProtectionTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBUnitState* Unit = State.GetMutableUnitById(2);
	Unit->AddStatus(TEXT("Burn"), 2);
	Unit->AddStatus(TEXT("Poison"), 2);
	Unit->AddStatus(TEXT("Negated"));
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::CleanseAllStatuses, NAME_None, 0));
	TestTrue(TEXT("Cleanse succeeds"), Result.bOk);
	TestFalse(TEXT("Burn removed"), Unit->HasStatus(TEXT("Burn")));
	TestFalse(TEXT("Poison removed"), Unit->HasStatus(TEXT("Poison")));
	TestTrue(TEXT("Negated preserved"), Unit->HasStatus(TEXT("Negated")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusUnknownAndSealedFailClosedTest,
	"Wandbound.StatusAuthority.Canonicalization.UnknownAndSealedFailClosed")
bool FWBStatusUnknownAndSealedFailClosedTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	const FWBStatusEffectResult Unknown = WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Mystery"), 1));
	TestFalse(TEXT("Unknown fails"), Unknown.bOk);
	TestEqual(TEXT("Unknown reason"), Unknown.Reason, FString(TEXT("unknown_canonical_status_id")));
	const FWBStatusEffectResult Sealed = WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Sealed"), 1));
	TestFalse(TEXT("Sealed deferred"), Sealed.bOk);
	TestEqual(TEXT("Sealed reason"), Sealed.Reason, FString(TEXT("status_not_implemented")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBFrozenApplyExtinguishesBurnTest,
	"Wandbound.StatusAuthority.Frozen.ApplicationExtinguishesBurn")
bool FWBFrozenApplyExtinguishesBurnTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(2)->AddStatus(TEXT("Burn"), 2);
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Frozen"), 2));
	TestTrue(TEXT("Frozen apply succeeds"), Result.bOk);
	TestFalse(TEXT("Burn removed"), State.GetUnitById(2)->HasStatus(TEXT("Burn")));
	TestTrue(TEXT("Frozen active"), State.GetUnitById(2)->HasStatus(TEXT("Frozen")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBFrozenConsumesIncomingStatusTest,
	"Wandbound.StatusAuthority.Frozen.IncomingStatusBreaksIceOnly")
bool FWBFrozenConsumesIncomingStatusTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(2)->AddStatus(TEXT("Frozen"), 2);
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Rooted"), 2));
	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestTrue(TEXT("Incoming consumed"), Result.bIncomingStatusConsumedByFrozen);
	TestFalse(TEXT("Frozen removed"), State.GetUnitById(2)->HasStatus(TEXT("Frozen")));
	TestFalse(TEXT("Rooted not applied"), State.GetUnitById(2)->HasStatus(TEXT("Rooted")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBFrozenPoisonReapplyPauseTest,
	"Wandbound.StatusAuthority.Frozen.PoisonReapplyBreaksWithoutTickOrRefresh")
bool FWBFrozenPoisonReapplyPauseTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(2)->AddStatus(TEXT("Poison"), 2);
	State.GetMutableUnitById(2)->AddStatus(TEXT("Frozen"), 2);
	const FWBStatusEffectResult Result = WBStatusEffect::ApplyStatusEffect(State, MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Poison"), 4));
	TestTrue(TEXT("Resolution succeeds"), Result.bOk);
	TestFalse(TEXT("No immediate tick"), Result.bAppliedImmediatePoisonTick);
	TestEqual(TEXT("Max HP unchanged"), State.GetUnitById(2)->MaxHP, 8);
	TestEqual(TEXT("Poison duration unchanged"), State.GetUnitById(2)->GetStatusTurnsRemaining(TEXT("Poison")), 2);
	TestFalse(TEXT("Frozen removed"), State.GetUnitById(2)->HasStatus(TEXT("Frozen")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusMoveSemanticsTest,
	"Wandbound.StatusAuthority.Actions.RootedAndFrozenBlockDeclaredMoves")
bool FWBStatusMoveSemanticsTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBAction Move;
	Move.Type = EWBActionType::Move;
	Move.PlayerId = 0;
	Move.SourceUnitId = 1;
	Move.FromTile = FWBTile(4, 4);
	Move.ToTile = FWBTile(5, 5);
	State.GetMutableUnitById(1)->AddStatus(TEXT("Rooted"), 1);
	TestEqual(TEXT("Rooted diagonal reason"), WBRules::QueryMove(State, Move).Reason, FString(TEXT("cannot_move")));
	State.GetMutableUnitById(1)->RemoveStatus(TEXT("Rooted"));
	State.GetMutableUnitById(1)->AddStatus(TEXT("Frozen"), 1);
	TestEqual(TEXT("Frozen diagonal reason"), WBRules::QueryMove(State, Move).Reason, FString(TEXT("cannot_move")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusNPCMoveSemanticsTest,
	"Wandbound.StatusAuthority.Actions.NPCMoveUsesSameStatusAuthority")
bool FWBStatusNPCMoveSemanticsTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBUnitState NPC = MakeUnit(9, INDEX_NONE, FWBTile(2, 2));
	State.AddUnitForTest(NPC);
	State.GetMutableUnitById(9)->AddStatus(TEXT("Frozen"), 1);
	FWBAction Move;
	Move.Type = EWBActionType::Move;
	Move.PlayerId = INDEX_NONE;
	Move.SourceUnitId = 9;
	Move.FromTile = FWBTile(2, 2);
	Move.ToTile = FWBTile(2, 3);
	TestEqual(TEXT("NPC Frozen reason"), WBRules::QueryNPCMove(State, Move, 1).Reason, FString(TEXT("cannot_move")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusActivationSemanticsTest,
	"Wandbound.StatusAuthority.Actions.ActivationDefaultsBlockFrozen")
bool FWBStatusActivationSemanticsTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(1)->AddStatus(TEXT("Frozen"), 1);
	FWBCardActivationSourceGateDefinition Gate;
	Gate.bRequiresSourceUnit = true;
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = 1;
	const FWBCardActivationSourceGateResult Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Frozen source denied"), Result.bOk);
	TestEqual(TEXT("Frozen reason"), Result.Reason, FString(TEXT("source_unit_frozen")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBCannotAttackSemanticSplitTest,
	"Wandbound.StatusAuthority.CannotAttack.DeclarationBlockedCounterAllowed")
bool FWBCannotAttackSemanticSplitTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(1)->AddStatus(TEXT("no_attack"), 2);
	FWBAction Attack;
	Attack.Type = EWBActionType::Attack;
	Attack.PlayerId = 0;
	Attack.SourceUnitId = 1;
	Attack.TargetUnitId = 2;
	TestFalse(TEXT("Declared attack denied"), WBRules::CanDeclareAttack(State, Attack).bOk);
	FWBAction Move;
	Move.Type = EWBActionType::Move;
	Move.PlayerId = 0;
	Move.SourceUnitId = 1;
	Move.FromTile = FWBTile(4, 4);
	Move.ToTile = FWBTile(3, 4);
	TestTrue(TEXT("Move remains legal"), WBRules::QueryMove(State, Move).bOk);
	State.GetMutableUnitById(1)->RemoveStatus(TEXT("Cannot Attack"));
	State.GetMutableUnitById(2)->AddStatus(TEXT("CannotAttack"), 2);
	PrimePendingAttack(State);
	TestTrue(TEXT("Counter remains legal"), WBRules::CanResolveCounterattack(State).bOk);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBCounterIndependentSuppressionTest,
	"Wandbound.StatusAuthority.Counter.StunnedAndFrozenStillSuppress")
bool FWBCounterIndependentSuppressionTest::RunTest(const FString&)
{
	for (const FName StatusId : { FName(TEXT("Stunned")), FName(TEXT("Frozen")) })
	{
		FWBGameStateData State = MakeState();
		State.GetMutableUnitById(2)->AddStatus(StatusId, 2);
		PrimePendingAttack(State);
		TestFalse(TEXT("Counter suppressed"), WBRules::CanResolveCounterattack(State).bOk);
	}
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBFrozenArmorAndZeroAttackTest,
	"Wandbound.StatusAuthority.Frozen.ZeroAttackBreaksBeforeArmor")
bool FWBFrozenArmorAndZeroAttackTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(1)->ATK = 0;
	State.GetMutableUnitById(2)->SetArmorForTest(3, 3);
	State.GetMutableUnitById(2)->AddStatus(TEXT("Frozen"), 2);
	PrimePendingAttack(State);
	TestTrue(TEXT("Attack resolves"), ResolveAttackThroughApply(State));
	TestFalse(TEXT("Frozen removed"), State.GetUnitById(2)->HasStatus(TEXT("Frozen")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, 8);
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 3);
	TestTrue(TEXT("Frozen break recorded"), State.PendingAttack.bFrozenBroken);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBFrozenFinalSubstituteTest,
	"Wandbound.StatusAuthority.Frozen.SubstitutionUsesFinalRecipient")
bool FWBFrozenFinalSubstituteTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(3, 1, FWBTile(3, 5)));
	State.GetMutableUnitById(3)->AddStatus(TEXT("Frozen"), 2);
	PrimePendingAttack(State);
	State.PendingAttack.DamageSubstitution.bActive = true;
	State.PendingAttack.DamageSubstitution.ProtectedUnitId = 2;
	State.PendingAttack.DamageSubstitution.SubstituteUnitId = 3;
	TestTrue(TEXT("Attack resolves"), ResolveAttackThroughApply(State));
	TestEqual(TEXT("Original HP unchanged"), State.GetUnitById(2)->HP, 8);
	TestEqual(TEXT("Substitute HP unchanged"), State.GetUnitById(3)->HP, 8);
	TestFalse(TEXT("Substitute Frozen removed"), State.GetUnitById(3)->HasStatus(TEXT("Frozen")));
	TestTrue(TEXT("Frozen break recorded"), State.PendingAttack.bFrozenBroken);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBFrozenOriginalDefenderSubstitutionTest,
	"Wandbound.StatusAuthority.Frozen.SubstitutionDoesNotConsumeReplacedDefenderIce")
bool FWBFrozenOriginalDefenderSubstitutionTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(3, 1, FWBTile(3, 5)));
	State.GetMutableUnitById(2)->AddStatus(TEXT("Frozen"), 2);
	PrimePendingAttack(State);
	State.PendingAttack.DamageSubstitution.bActive = true;
	State.PendingAttack.DamageSubstitution.ProtectedUnitId = 2;
	State.PendingAttack.DamageSubstitution.SubstituteUnitId = 3;
	TestTrue(TEXT("Attack resolves"), ResolveAttackThroughApply(State));
	TestTrue(TEXT("Original Frozen remains"), State.GetUnitById(2)->HasStatus(TEXT("Frozen")));
	TestEqual(TEXT("Substitute takes transferred HP damage"), State.GetUnitById(3)->HP, 5);
	TestFalse(TEXT("No Frozen break"), State.PendingAttack.bFrozenBroken);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBFrozenCounterAttackPipelineTest,
	"Wandbound.StatusAuthority.Frozen.CounterattackUsesSameBreakAuthority")
bool FWBFrozenCounterAttackPipelineTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(2)->AddStatus(TEXT("Frozen"), 2);
	PrimePendingAttack(State, 1, 2, true);
	TestTrue(TEXT("Counter hit resolves"), ResolveAttackThroughApply(State));
	TestFalse(TEXT("Frozen removed"), State.GetUnitById(2)->HasStatus(TEXT("Frozen")));
	TestEqual(TEXT("No HP damage"), State.GetUnitById(2)->HP, 8);
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBCannotAttackEndTurnExpiryTest,
	"Wandbound.StatusAuthority.CannotAttack.EndTurnDurationExpires")
bool FWBCannotAttackEndTurnExpiryTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(2)->AddStatus(TEXT("cannot_attack"), 1);
	TestTrue(TEXT("Tick succeeds"), WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0).bOk);
	TestFalse(TEXT("Cannot Attack expires"), State.GetUnitById(2)->HasStatus(TEXT("Cannot Attack")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBBurnSourceSurvivesRemovalTest,
	"Wandbound.StatusAuthority.Burn.SourceRemovalDoesNotExpireStatus")
bool FWBBurnSourceSurvivesRemovalTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBStatusEffectRequest Request = MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Burn"), 2);
	Request.Source.SourcePlayerId = 0;
	Request.Source.SourceOwnerPlayerId = 0;
	Request.Source.SourceUnitId = 1;
	Request.Source.Origin = EWBStatusApplicationOrigin::TriggeredResolution;
	WBStatusEffect::ApplyStatusEffect(State, Request);
	State.GetMutableUnitById(1)->MarkUnitDefeated();
	State.GetMutableUnitById(1)->RemoveUnitFromBoard();
	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, 0);
	TestTrue(TEXT("Tick succeeds"), Result.bOk);
	TestEqual(TEXT("Burn deals one"), State.GetUnitById(2)->HP, 7);
	TestTrue(TEXT("Burn remains"), State.GetUnitById(2)->HasStatus(TEXT("Burn")));
	TestTrue(TEXT("Historical source traced"), Result.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& Event) { return Event.Kind == FName(TEXT("status_tick")) && Event.SourceUnitId == 1; }));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusPublicPrivacyTest,
	"Wandbound.StatusAuthority.Privacy.PublicSummaryExcludesProvenance")
bool FWBStatusPublicPrivacyTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBStatusEffectRequest Request = MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("no_attack"), 2);
	Request.Source.SourceCardId = TEXT("private_source_card");
	Request.Source.SourceCardInstanceId = TEXT("private_instance");
	Request.Source.SourceEffectId = TEXT("private_effect");
	const FWBApplyActionResult ApplyResult = WBEffectRunner::ApplyStatusEffect(State, Request);
	TestTrue(TEXT("Status application succeeds"), ApplyResult.bOk);
	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	const FWBPublicUnitBoardSummary* Unit = Summary.Units.FindByPredicate([](const FWBPublicUnitBoardSummary& Candidate) { return Candidate.UnitId == 2; });
	TestNotNull(TEXT("Public unit exists"), Unit);
	TestTrue(TEXT("Canonical status is public"), Unit != nullptr && Unit->Statuses.ContainsByPredicate([](const FWBPublicUnitStatusSummary& Status) { return Status.StatusId == FName(TEXT("Cannot Attack")); }));
	const FString SerializedTrace = WBReplayTrace::SerializeEvents(ApplyResult.TraceEvents);
	TestFalse(TEXT("Trace excludes private source card"), SerializedTrace.Contains(TEXT("private_source_card")));
	TestFalse(TEXT("Trace excludes private source instance"), SerializedTrace.Contains(TEXT("private_instance")));
	TestFalse(TEXT("Trace excludes private source effect"), SerializedTrace.Contains(TEXT("private_effect")));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusDigestProvenanceTest,
	"Wandbound.StatusAuthority.Replay.ProvenanceParticipatesDeterministically")
bool FWBStatusDigestProvenanceTest::RunTest(const FString&)
{
	FWBGameStateData A = MakeState();
	FWBGameStateData B = MakeState();
	FWBStatusEffectRequest Request = MakeStatusRequest(EWBStatusEffectOp::ApplyStatus, TEXT("Burn"), 2);
	Request.Source.SourcePlayerId = 0;
	Request.Source.SourceUnitId = 1;
	Request.Source.SourceCardInstanceId = TEXT("instance_a");
	Request.Source.Origin = EWBStatusApplicationOrigin::Activation;
	WBStatusEffect::ApplyStatusEffect(A, Request);
	WBStatusEffect::ApplyStatusEffect(B, Request);
	TestEqual(TEXT("Same event same digest"), WBProductionMatchReplay::BuildGameStateDigest(A), WBProductionMatchReplay::BuildGameStateDigest(B));
	Request.Source.SourceCardInstanceId = TEXT("instance_b");
	WBStatusEffect::ApplyStatusEffect(B, Request);
	TestNotEqual(TEXT("Different provenance changes digest"), WBProductionMatchReplay::BuildGameStateDigest(A), WBProductionMatchReplay::BuildGameStateDigest(B));
	return true;
}

WB_STATUS_FOUNDATION_TEST(FWBStatusReceiptSchemaTest,
	"Wandbound.StatusAuthority.Privacy.ReceiptRemainsEightFields")
bool FWBStatusReceiptSchemaTest::RunTest(const FString&)
{
	FWBProductionMatchReplayArchive Archive;
	Archive.Header.OpaqueMatchId = TEXT("status_receipt");
	WBProductionMatchReplay::RebuildIntegrity(Archive);
	const TSharedPtr<FJsonObject> Object = ParseObject(
		WBProductionMatchReplay::SerializeReceipt(
			WBProductionMatchReplay::BuildReceipt(Archive, true)));
	TestTrue(TEXT("Receipt parses"), Object.IsValid());
	TestEqual(TEXT("Exactly eight fields"), Object.IsValid() ? Object->Values.Num() : 0, 8);
	return true;
}

#undef WB_STATUS_FOUNDATION_TEST

#endif
