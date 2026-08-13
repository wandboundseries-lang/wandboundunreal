#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBEffectRunner.h"
#include "WBCardDefinitionFixtureLoader.h"
#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionPendingAttackRedirectSmoke.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 RedirectTargetUnitId = 9001;

FWBUnitState MakeRedirectUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile Tile,
	const int32 AR = 8)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("redirect_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 8;
	Unit.MaxHP = 8;
	Unit.ATK = 3;
	Unit.AR = AR;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeRedirectState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.TurnNumber = 2;
	State.Phase = EWBGamePhase::Response;
	State.AddUnitForTest(MakeRedirectUnit(10, 0, FWBTile(4, 4)));
	State.AddUnitForTest(MakeRedirectUnit(20, 1, FWBTile(4, 1)));
	State.AddUnitForTest(MakeRedirectUnit(30, 1, FWBTile(4, 2)));

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
	Pending.ContinuationId = TEXT("attack_continuation_1");
	State.SetPendingAttackForTest(Pending);

	State.ReactionWindow.Kind = EWBReactionWindowKind::PreHit;
	State.ReactionWindow.OriginatingPlayerId = 0;
	State.ReactionWindow.ConsecutivePassCount = 1;
	State.ReactionWindow.SourceActionId = Pending.DeclarationActionId;
	State.ReactionWindow.SourceUnitId = Pending.AttackerUnitId;
	State.ReactionWindow.TargetUnitId = Pending.DefenderUnitId;
	return State;
}

FWBCardEffectDefinition MakeResponseEffect(
	const FString& EffectId,
	const EWBGenericEffectOp Operation,
	const EWBCardEffectTargetRequirement TargetRequirement)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = TEXT("Respond");
	Effect.TargetRequirement = TargetRequirement;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	FWBGenericEffectPayload Payload;
	Payload.Operation = Operation;
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardDefinition MakeHero(
	const FString& CardId,
	const int32 ATK,
	const TArray<FWBCardEffectDefinition>& Effects)
{
	FWBCardDefinition Hero;
	Hero.CardId = CardId;
	Hero.PublicName = CardId;
	Hero.Kind = EWBCardDefinitionKind::Character;
	Hero.CharacterStats.HP = 8;
	Hero.CharacterStats.ATK = ATK;
	Hero.CharacterStats.AR = 8;
	Hero.CharacterStats.RL = 3;
	Hero.ActivatedEffects = Effects;
	return Hero;
}

FWBCardDefinition MakeFiller()
{
	FWBCardDefinition Filler;
	Filler.CardId = TEXT("redirect_filler");
	Filler.PublicName = TEXT("Redirect Filler");
	Filler.Kind = EWBCardDefinitionKind::Action;
	return Filler;
}

FWBCardDefinition MakeTrap()
{
	FWBCardDefinition Trap;
	Trap.CardId = TEXT("redirect_trap");
	Trap.PublicName = TEXT("Redirect Trap");
	Trap.Kind = EWBCardDefinitionKind::Trap;
	Trap.TrapDamage = 1;
	return Trap;
}

FWBCardDefinition MakeNPC()
{
	FWBCardDefinition NPC = MakeHero(TEXT("redirect_npc"), 1, {});
	NPC.Kind = EWBCardDefinitionKind::NPC;
	NPC.CharacterStats.AR = 1;
	return NPC;
}

FWBSetupMarkerPlacement MakeMarker(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile Tile,
	const int32 Order)
{
	FWBSetupMarkerPlacement Marker;
	Marker.PlayerId = PlayerId;
	Marker.Type = Type;
	Marker.Tile = Tile;
	Marker.DefinitionId = Type == EWBMarkerType::Trap
		? TEXT("redirect_trap")
		: TEXT("redirect_npc");
	Marker.PlacementOrder = Order;
	return Marker;
}

FWBCardInstanceRef MakeCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerId;
	return Card;
}

FWBMatchPlayerSetup MakePlayer(const int32 PlayerId)
{
	FWBMatchPlayerSetup Player;
	Player.PlayerId = PlayerId;
	Player.HeroInstanceId = FString::Printf(TEXT("redirect_p%d_hero_instance"), PlayerId);
	Player.HeroCardId = FString::Printf(TEXT("redirect_hero_%d"), PlayerId);
	Player.OrderedDeck.Add(MakeCard(
		Player.HeroInstanceId, Player.HeroCardId, PlayerId));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Player.OrderedDeck.Add(MakeCard(
			FString::Printf(TEXT("redirect_p%d_filler_%d"), PlayerId, Index),
			TEXT("redirect_filler"),
			PlayerId));
	}
	return Player;
}

FWBMatchInitializationRequest MakeRedirectRequest()
{
	const FWBCardEffectDefinition NegateB = MakeResponseEffect(
		TEXT("redirect_negate_b"),
		EWBGenericEffectOp::NegatePendingEffect,
		EWBCardEffectTargetRequirement::None);
	const FWBCardEffectDefinition PreventP0 = MakeResponseEffect(
		TEXT("redirect_prevent_p0"),
		EWBGenericEffectOp::PreventPendingAttack,
		EWBCardEffectTargetRequirement::None);
	const FWBCardEffectDefinition RedirectA = MakeResponseEffect(
		TEXT("redirect_a"),
		EWBGenericEffectOp::RedirectPendingAttack,
		EWBCardEffectTargetRequirement::Unit);
	const FWBCardEffectDefinition NegateC = MakeResponseEffect(
		TEXT("redirect_negate_c"),
		EWBGenericEffectOp::NegatePendingEffect,
		EWBCardEffectTargetRequirement::None);
	const FWBCardEffectDefinition PreventA = MakeResponseEffect(
		TEXT("redirect_prevent_a"),
		EWBGenericEffectOp::PreventPendingAttack,
		EWBCardEffectTargetRequirement::None);

	FWBMatchInitializationRequest Request;
	Request.Seed = 41027;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("pending_attack_redirect_tests");
	Request.Repository.SourceVersion = TEXT("pending_attack_redirect_v1");
	Request.Repository.Definitions = {
		MakeHero(TEXT("redirect_hero_0"), 3, { NegateB, PreventP0 }),
		MakeHero(TEXT("redirect_hero_1"), 1, { RedirectA, NegateC, PreventA }),
		MakeFiller(),
		MakeTrap(),
		MakeNPC()
	};
	Request.Players = { MakePlayer(0), MakePlayer(1) };
	Request.MarkerPlacements = {
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(0, 7), 2),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(1, 7), 3),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(0, 1), 6),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(1, 1), 7)
	};
	return Request;
}

const FWBMatchLegalAction* FindCoreAction(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId,
	const int32 TargetUnitId = INDEX_NONE)
{
	return Actions.FindByPredicate(
		[&EffectId, TargetUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceEffectId == EffectId
				&& (TargetUnitId == INDEX_NONE
					|| Action.ActivationCommand.EffectRequest.Target.TargetUnitId
						== TargetUnitId);
		});
}

bool SubmitActivation(
	WBMatchCoordinator& Coordinator,
	const FString& EffectId,
	FString& OutReason,
	const int32 TargetUnitId = INDEX_NONE)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Action = Legal.bOk
		? FindActivation(Legal.Actions, EffectId, TargetUnitId)
		: nullptr;
	if (Action == nullptr)
	{
		OutReason = Legal.bOk ? EffectId + TEXT("_missing") : Legal.Reason;
		return false;
	}
	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(
		Action->PlayerId, Action->ActionId);
	OutReason = Result.Reason;
	return Result.bOk;
}

bool SubmitPass(WBMatchCoordinator& Coordinator, FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Pass = Legal.bOk
		? FindCoreAction(Legal.Actions, EWBActionType::PassResponse)
		: nullptr;
	if (Pass == nullptr)
	{
		OutReason = Legal.bOk ? TEXT("pass_response_missing") : Legal.Reason;
		return false;
	}
	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(
		Pass->PlayerId, Pass->ActionId);
	OutReason = Result.Reason;
	return Result.bOk;
}

bool PassUntilPendingDepth(
	WBMatchCoordinator& Coordinator,
	const int32 TargetDepth,
	FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 12
			&& Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response
			&& Coordinator.GetPendingEffectActivationStack().Num() > TargetDepth;
		++Guard)
	{
		if (!SubmitPass(Coordinator, OutReason))
		{
			return false;
		}
		if (Coordinator.GetState().HasPendingAttack()
			&& Coordinator.GetState().PendingAttack.DefenderUnitId
				== RedirectTargetUnitId)
		{
			continue;
		}
	}
	if (Coordinator.GetPendingEffectActivationStack().Num() > TargetDepth)
	{
		OutReason = TEXT("pending_effect_depth_guard_exceeded");
		return false;
	}
	OutReason.Reset();
	return true;
}

bool PassToAction(WBMatchCoordinator& Coordinator, FString& OutReason)
{
	for (int32 Guard = 0;
		Guard < 24 && Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response;
		++Guard)
	{
		if (!SubmitPass(Coordinator, OutReason))
		{
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response)
	{
		OutReason = TEXT("response_guard_exceeded");
		return false;
	}
	OutReason.Reset();
	return true;
}

struct FRedirectScenario
{
	bool bOk = false;
	FString Reason;
	WBMatchCoordinator Coordinator;
	int32 AttackerUnitId = -1;
	int32 OriginalDefenderUnitId = -1;
	int32 OriginalDefenderHPBefore = -1;
	int32 RedirectTargetHPBefore = -1;
};

FRedirectScenario OpenRedirectScenario()
{
	FRedirectScenario Scenario;
	const FWBMatchOperationResult Started = Scenario.Coordinator.InitializeMatch(
		MakeRedirectRequest());
	if (!Started.bOk)
	{
		Scenario.Reason = Started.Reason;
		return Scenario;
	}
	Scenario.Coordinator.GetMutableStateForTest().TurnNumber = 2;
	const FWBPlayerStateData* AttackerPlayer =
		Scenario.Coordinator.GetState().GetPlayerById(0);
	const FWBPlayerStateData* DefenderPlayer =
		Scenario.Coordinator.GetState().GetPlayerById(1);
	if (AttackerPlayer == nullptr || DefenderPlayer == nullptr)
	{
		Scenario.Reason = TEXT("player_missing");
		return Scenario;
	}
	Scenario.AttackerUnitId = AttackerPlayer->HeroUnitId;
	Scenario.OriginalDefenderUnitId = DefenderPlayer->HeroUnitId;
	const FWBUnitState* OriginalDefender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.OriginalDefenderUnitId);
	Scenario.OriginalDefenderHPBefore = OriginalDefender != nullptr
		? OriginalDefender->HP
		: -1;

	const FWBMatchLegalActionGenerationResult Legal =
		Scenario.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = Legal.bOk
		? FindCoreAction(Legal.Actions, EWBActionType::Attack)
		: nullptr;
	if (Attack == nullptr)
	{
		Scenario.Reason = Legal.bOk ? TEXT("attack_missing") : Legal.Reason;
		return Scenario;
	}
	const FWBMatchOperationResult Attacked = Scenario.Coordinator.SubmitActionId(
		Attack->PlayerId, Attack->ActionId);
	if (!Attacked.bOk)
	{
		Scenario.Reason = Attacked.Reason;
		return Scenario;
	}

	const FWBUnitState* Attacker = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.AttackerUnitId);
	const FWBUnitState* Defender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.OriginalDefenderUnitId);
	if (Attacker == nullptr || Defender == nullptr)
	{
		Scenario.Reason = TEXT("attack_participant_missing");
		return Scenario;
	}
	const int32 StepY = Attacker->Y > Defender->Y ? 1 : -1;
	FWBUnitState RedirectTarget = MakeRedirectUnit(
		RedirectTargetUnitId,
		Defender->OwnerId,
		FWBTile(Defender->X, Defender->Y + StepY));
	if (!Scenario.Coordinator.GetMutableStateForTest().AddUnitForTest(RedirectTarget))
	{
		Scenario.Reason = TEXT("redirect_target_add_failed");
		return Scenario;
	}
	Scenario.RedirectTargetHPBefore = RedirectTarget.HP;
	Scenario.bOk = true;
	return Scenario;
}

int32 CountTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	int32 Count = 0;
	for (const FWBTraceEvent& Event : Events)
	{
		if (Event.Kind == Kind)
		{
			++Count;
		}
	}
	return Count;
}

int32 CountAcceptedAttacks(const WBMatchCoordinator& Coordinator)
{
	int32 Count = 0;
	for (const FWBMatchCommittedActionRecord& Record :
		Coordinator.GetCommittedActionRecords())
	{
		if (Record.ChosenActionId.StartsWith(TEXT("attack:")))
		{
			++Count;
		}
	}
	return Count;
}
}

#define WB_REDIRECT_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_REDIRECT_TEST(FWBPendingAttackRedirectLegality,
	"Wandbound.AttackRedirect.Rules.RequiresExactLiveLegalTarget")
bool FWBPendingAttackRedirectLegality::RunTest(const FString&)
{
	FWBGameStateData State = MakeRedirectState();
	TestTrue(TEXT("Legal redirect accepted"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 30).bOk);
	TestEqual(TEXT("Exact continuation required"), WBRules::CanRedirectPendingAttack(
		State, TEXT("stale_continuation"), 30).Reason,
		FString(TEXT("pending_attack_target_mismatch")));
	TestEqual(TEXT("Current target cannot be reused"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 20).Reason,
		FString(TEXT("redirect_target_unchanged")));
	TestEqual(TEXT("Attacker cannot be target"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 10).Reason,
		FString(TEXT("same_unit")));
	State.GetMutableUnitById(30)->RemoveUnitFromBoard();
	TestEqual(TEXT("Removed target rejected"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 30).Reason,
		FString(TEXT("redirect_target_removed")));
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectGeometry,
	"Wandbound.AttackRedirect.Rules.RangeAlignmentWallsAndUnits")
bool FWBPendingAttackRedirectGeometry::RunTest(const FString&)
{
	FWBGameStateData State = MakeRedirectState();
	State.GetMutableUnitById(30)->X = 3;
	TestEqual(TEXT("Diagonal rejected"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 30).Reason,
		FString(TEXT("not_in_line")));

	State = MakeRedirectState();
	State.GetMutableUnitById(10)->AR = 1;
	TestEqual(TEXT("Out of range rejected"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 30).Reason,
		FString(TEXT("out_of_range")));

	State = MakeRedirectState();
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 3), FWBTile(4, 2)));
	TestEqual(TEXT("Wall blocks redirect"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 30).Reason,
		FString(TEXT("blocked_by_wall")));

	State = MakeRedirectState();
	State.AddUnitForTest(MakeRedirectUnit(40, 0, FWBTile(4, 3)));
	TestEqual(TEXT("Intervening unit blocks redirect"), WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 30).Reason,
		FString(TEXT("blocked_by_unit")));
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectNotDeclaration,
	"Wandbound.AttackRedirect.Rules.DoesNotRecheckDeclarationResourcesOrStatuses")
bool FWBPendingAttackRedirectNotDeclaration::RunTest(const FString&)
{
	FWBGameStateData State = MakeRedirectState();
	FWBUnitState* Attacker = State.GetMutableUnitById(10);
	Attacker->AttacksLeft = 0;
	Attacker->AddStatus(FName(TEXT("Stunned")), 1);
	State.CurrentPlayer = 1;
	State.PriorityPlayer = 1;
	TestTrue(TEXT("Redirect remains continuation legality"),
		WBRules::CanRedirectPendingAttack(
			State, TEXT("attack_continuation_1"), 30).bOk);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectSharedAuthority,
	"Wandbound.AttackRedirect.Rules.PlayerNPCCounterUseSharedContinuation")
bool FWBPendingAttackRedirectSharedAuthority::RunTest(const FString&)
{
	FWBGameStateData State = MakeRedirectState();
	State.PendingAttack.AuthorityKind = EWBAttackAuthorityKind::NeutralNPC;
	State.PendingAttack.AttackingPlayerId = -1;
	TestTrue(TEXT("NPC attack may redirect through shared PreHit"),
		WBRules::CanRedirectPendingAttack(
			State, TEXT("attack_continuation_1"), 30).bOk);

	State.PendingAttack.AuthorityKind = EWBAttackAuthorityKind::Player;
	State.PendingAttack.AttackingPlayerId = 0;
	State.PendingAttack.bCounter = true;
	TestTrue(TEXT("Counter may redirect through shared PreHit"),
		WBRules::CanRedirectPendingAttack(
			State, TEXT("attack_continuation_1"), 30).bOk);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectFailClosedAfterPrevent,
	"Wandbound.AttackRedirect.Rules.PreventedAttackFailsClosed")
bool FWBPendingAttackRedirectFailClosedAfterPrevent::RunTest(const FString&)
{
	FWBGameStateData State = MakeRedirectState();
	State.PendingAttack.bPrevented = true;
	const FWBActionQueryResult Query = WBRules::CanRedirectPendingAttack(
		State, TEXT("attack_continuation_1"), 30);
	TestFalse(TEXT("Prevented attack cannot redirect"), Query.bOk);
	TestEqual(TEXT("Fail-closed reason"), Query.Reason,
		FString(TEXT("pending_attack_already_prevented")));
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectFixturePayload,
	"Wandbound.AttackRedirect.Fixture.TypedSelectedTargetPayloadLoads")
bool FWBPendingAttackRedirectFixturePayload::RunTest(const FString&)
{
	const FString Json = TEXT(
		"{\"repository_id\":\"redirect_fixture\","
		"\"source_version\":\"redirect_v1\","
		"\"cards\":[{\"card_id\":\"redirect_fixture_card\","
		"\"public_name\":\"Redirect Fixture\",\"kind\":\"fixture\","
		"\"activated_effects\":[{\"effect_id\":\"redirect_fixture_effect\","
		"\"public_label\":\"Redirect\",\"target_requirement\":\"unit\","
		"\"payloads\":[{\"type\":\"redirect_pending_attack\","
		"\"target\":\"selected\"}]}]}]}");
	const FWBCardDefinitionFixtureLoadResult Loaded =
		WBCardDefinitionFixtureLoader::LoadRepositoryFromJsonString(
			Json, TEXT("inline_redirect_fixture.json"));
	TestTrue(TEXT("Typed redirect fixture loads"), Loaded.bOk);
	if (!Loaded.bOk || Loaded.Repository.Definitions.IsEmpty()
		|| Loaded.Repository.Definitions[0].ActivatedEffects.IsEmpty()
		|| Loaded.Repository.Definitions[0].ActivatedEffects[0].Payloads.IsEmpty())
	{
		return false;
	}
	TestEqual(TEXT("Redirect payload operation"),
		Loaded.Repository.Definitions[0].ActivatedEffects[0].Payloads[0].Operation,
		EWBGenericEffectOp::RedirectPendingAttack);
	TestEqual(TEXT("Selected unit target requirement"),
		Loaded.Repository.Definitions[0].ActivatedEffects[0].TargetRequirement,
		EWBCardEffectTargetRequirement::Unit);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectMutation,
	"Wandbound.AttackRedirect.Effect.PreservesDeclarationIdentityAndPassState")
bool FWBPendingAttackRedirectMutation::RunTest(const FString&)
{
	FWBGameStateData State = MakeRedirectState();
	const FWBPendingAttackState Before = State.PendingAttack;
	const int32 PassesBefore = State.ReactionWindow.ConsecutivePassCount;
	const int32 PriorityBefore = State.PriorityPlayer;
	const FWBApplyActionResult Result = WBEffectRunner::ApplyPendingAttackRedirect(
		State, Before.ContinuationId, 30);
	TestTrue(TEXT("Redirect applies"), Result.bOk);
	TestEqual(TEXT("Current defender changes"), State.PendingAttack.DefenderUnitId, 30);
	TestEqual(TEXT("Original defender remains"),
		State.PendingAttack.OriginalDefenderUnitId, Before.OriginalDefenderUnitId);
	TestEqual(TEXT("Continuation remains"),
		State.PendingAttack.ContinuationId, Before.ContinuationId);
	TestEqual(TEXT("Declaration action remains"),
		State.PendingAttack.DeclarationActionId, Before.DeclarationActionId);
	TestEqual(TEXT("Attacker remains"),
		State.PendingAttack.AttackerUnitId, Before.AttackerUnitId);
	TestEqual(TEXT("Original attacker remains"),
		State.PendingAttack.OriginalAttackerUnitId, Before.OriginalAttackerUnitId);
	TestEqual(TEXT("Pass count not reset"),
		State.ReactionWindow.ConsecutivePassCount, PassesBefore);
	TestEqual(TEXT("Priority not changed"), State.PriorityPlayer, PriorityBefore);
	TestEqual(TEXT("Reaction target synchronized"),
		State.ReactionWindow.TargetUnitId, 30);
	TestEqual(TEXT("One redirect trace"), Result.TraceEvents.Num(), 1);
	if (!Result.TraceEvents.IsEmpty())
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind,
			FName(TEXT("pending_attack_redirected")));
		TestEqual(TEXT("Trace previous target"),
			Result.TraceEvents[0].PreviousTargetUnitId, 20);
		TestEqual(TEXT("Trace new target"),
			Result.TraceEvents[0].TargetUnitId, 30);
		const FString Json = WBReplayTrace::SerializeEvent(Result.TraceEvents[0]);
		TestTrue(TEXT("Serialized trace records previous target"),
			Json.Contains(TEXT("\"previous_target_unit_id\""))
			&& Json.Contains(TEXT("20")));
	}
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectDeepNested,
	"Wandbound.AttackRedirect.Reaction.DeepNestedRestorationKeepsCurrentTarget")
bool FWBPendingAttackRedirectDeepNested::RunTest(const FString&)
{
	FRedirectScenario Scenario = OpenRedirectScenario();
	if (!Scenario.bOk)
	{
		AddError(Scenario.Reason);
		return false;
	}
	TestTrue(TEXT("A prevent accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_prevent_a"), Scenario.Reason));
	TestTrue(TEXT("B negate accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_negate_b"), Scenario.Reason));
	TestTrue(TEXT("C redirect accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_a"), Scenario.Reason,
		RedirectTargetUnitId));
	TestTrue(TEXT("C resolves"), PassUntilPendingDepth(
		Scenario.Coordinator, 2, Scenario.Reason));
	TestEqual(TEXT("Redirect survives first restore"),
		Scenario.Coordinator.GetState().PendingAttack.DefenderUnitId,
		RedirectTargetUnitId);
	TestTrue(TEXT("B resolves"), PassUntilPendingDepth(
		Scenario.Coordinator, 1, Scenario.Reason));
	TestEqual(TEXT("Redirect survives second restore"),
		Scenario.Coordinator.GetState().PendingAttack.DefenderUnitId,
		RedirectTargetUnitId);
	TestTrue(TEXT("A unwinds and attack completes"), PassToAction(
		Scenario.Coordinator, Scenario.Reason));
	const FWBUnitState* Original = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.OriginalDefenderUnitId);
	const FWBUnitState* Redirected = Scenario.Coordinator.GetState().GetUnitById(
		RedirectTargetUnitId);
	TestEqual(TEXT("Original target unharmed"),
		Original != nullptr ? Original->HP : -1,
		Scenario.OriginalDefenderHPBefore);
	TestEqual(TEXT("Redirect target damaged once"),
		Redirected != nullptr ? Redirected->HP : -1,
		Scenario.RedirectTargetHPBefore - 3);
	TestEqual(TEXT("One redirect trace"), CountTrace(
		Scenario.Coordinator.GetTraceLog(), FName(TEXT("pending_attack_redirected"))), 1);
	TestEqual(TEXT("Only original attack accepted"),
		CountAcceptedAttacks(Scenario.Coordinator), 1);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectNegated,
	"Wandbound.AttackRedirect.Reaction.NegatedRedirectKeepsOriginalTarget")
bool FWBPendingAttackRedirectNegated::RunTest(const FString&)
{
	FRedirectScenario Scenario = OpenRedirectScenario();
	TestTrue(TEXT("Scenario opens"), Scenario.bOk);
	TestTrue(TEXT("Redirect accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_a"), Scenario.Reason,
		RedirectTargetUnitId));
	TestTrue(TEXT("Negate accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_negate_b"), Scenario.Reason));
	TestTrue(TEXT("Responses complete"), PassToAction(
		Scenario.Coordinator, Scenario.Reason));
	const FWBUnitState* Original = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.OriginalDefenderUnitId);
	const FWBUnitState* Redirected = Scenario.Coordinator.GetState().GetUnitById(
		RedirectTargetUnitId);
	TestEqual(TEXT("Original defender takes damage"),
		Original != nullptr ? Original->HP : -1,
		Scenario.OriginalDefenderHPBefore - 3);
	TestEqual(TEXT("Replacement remains unharmed"),
		Redirected != nullptr ? Redirected->HP : -1,
		Scenario.RedirectTargetHPBefore);
	TestEqual(TEXT("Redirect never mutates"), CountTrace(
		Scenario.Coordinator.GetTraceLog(), FName(TEXT("pending_attack_redirected"))), 0);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectNegateNegate,
	"Wandbound.AttackRedirect.Reaction.NegatedNegateAllowsRedirect")
bool FWBPendingAttackRedirectNegateNegate::RunTest(const FString&)
{
	FRedirectScenario Scenario = OpenRedirectScenario();
	TestTrue(TEXT("Scenario opens"), Scenario.bOk);
	TestTrue(TEXT("Redirect accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_a"), Scenario.Reason,
		RedirectTargetUnitId));
	TestTrue(TEXT("First negate accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_negate_b"), Scenario.Reason));
	TestTrue(TEXT("Second negate accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_negate_c"), Scenario.Reason));
	TestTrue(TEXT("Responses complete"), PassToAction(
		Scenario.Coordinator, Scenario.Reason));
	const FWBUnitState* Redirected = Scenario.Coordinator.GetState().GetUnitById(
		RedirectTargetUnitId);
	TestEqual(TEXT("Redirect target takes damage"),
		Redirected != nullptr ? Redirected->HP : -1,
		Scenario.RedirectTargetHPBefore - 3);
	TestEqual(TEXT("Redirect resolves once"), CountTrace(
		Scenario.Coordinator.GetTraceLog(), FName(TEXT("pending_attack_redirected"))), 1);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectThenPrevent,
	"Wandbound.AttackRedirect.Reaction.RedirectThenPreventSameContinuation")
bool FWBPendingAttackRedirectThenPrevent::RunTest(const FString&)
{
	FRedirectScenario Scenario = OpenRedirectScenario();
	TestTrue(TEXT("Scenario opens"), Scenario.bOk);
	TestTrue(TEXT("Redirect accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_a"), Scenario.Reason,
		RedirectTargetUnitId));
	TestTrue(TEXT("Redirect resolves to outer PreHit"), PassUntilPendingDepth(
		Scenario.Coordinator, 0, Scenario.Reason));
	TestEqual(TEXT("Outer PreHit has redirected target"),
		Scenario.Coordinator.GetState().ReactionWindow.TargetUnitId,
		RedirectTargetUnitId);
	TestTrue(TEXT("Prevent accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_prevent_p0"), Scenario.Reason));
	TestTrue(TEXT("Responses complete"), PassToAction(
		Scenario.Coordinator, Scenario.Reason));
	const FWBUnitState* Redirected = Scenario.Coordinator.GetState().GetUnitById(
		RedirectTargetUnitId);
	TestEqual(TEXT("Prevented redirected target unharmed"),
		Redirected != nullptr ? Redirected->HP : -1,
		Scenario.RedirectTargetHPBefore);
	TestEqual(TEXT("One attack prevented"), CountTrace(
		Scenario.Coordinator.GetTraceLog(), FName(TEXT("attack_prevented"))), 1);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackPreventNegatedAfterRedirect,
	"Wandbound.AttackRedirect.Reaction.NegatedPreventPreservesRedirect")
bool FWBPendingAttackPreventNegatedAfterRedirect::RunTest(const FString&)
{
	FRedirectScenario Scenario = OpenRedirectScenario();
	TestTrue(TEXT("Scenario opens"), Scenario.bOk);
	TestTrue(TEXT("Redirect accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_a"), Scenario.Reason,
		RedirectTargetUnitId));
	TestTrue(TEXT("Redirect resolves"), PassUntilPendingDepth(
		Scenario.Coordinator, 0, Scenario.Reason));
	TestTrue(TEXT("Prevent accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_prevent_p0"), Scenario.Reason));
	TestTrue(TEXT("Prevent negate accepted"), SubmitActivation(
		Scenario.Coordinator, TEXT("redirect_negate_c"), Scenario.Reason));
	TestTrue(TEXT("Responses complete"), PassToAction(
		Scenario.Coordinator, Scenario.Reason));
	const FWBUnitState* Redirected = Scenario.Coordinator.GetState().GetUnitById(
		RedirectTargetUnitId);
	TestEqual(TEXT("Redirected target takes damage after prevent is negated"),
		Redirected != nullptr ? Redirected->HP : -1,
		Scenario.RedirectTargetHPBefore - 3);
	TestEqual(TEXT("Prevent never resolves"), CountTrace(
		Scenario.Coordinator.GetTraceLog(), FName(TEXT("attack_prevented"))), 0);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectDeterminism,
	"Wandbound.AttackRedirect.Replay.FreshEquivalentRunMatches")
bool FWBPendingAttackRedirectDeterminism::RunTest(const FString&)
{
	auto Run = []()
	{
		FRedirectScenario Scenario = OpenRedirectScenario();
		if (!Scenario.bOk
			|| !SubmitActivation(
				Scenario.Coordinator,
				TEXT("redirect_a"),
				Scenario.Reason,
				RedirectTargetUnitId)
			|| !PassToAction(Scenario.Coordinator, Scenario.Reason))
		{
			Scenario.bOk = false;
			return Scenario;
		}
		Scenario.bOk = true;
		return Scenario;
	};
	const FRedirectScenario First = Run();
	const FRedirectScenario Second = Run();
	TestTrue(TEXT("First run succeeds"), First.bOk);
	TestTrue(TEXT("Second run succeeds"), Second.bOk);
	TestEqual(TEXT("State digest matches"),
		First.Coordinator.GetCurrentStateDigest(),
		Second.Coordinator.GetCurrentStateDigest());
	TestEqual(TEXT("Trace digest matches"),
		First.Coordinator.GetCurrentTraceDigest(),
		Second.Coordinator.GetCurrentTraceDigest());
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectProductionFixtureLoads,
	"Wandbound.AttackRedirect.Fixture.ProductionBundleLoads")
bool FWBPendingAttackRedirectProductionFixtureLoads::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/PendingAttackRedirectFixture/root_manifest.json"));
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
	TestTrue(TEXT("Production redirect fixture loads"), Loaded.bOk);
	if (Loaded.Snapshot.IsValid())
	{
		AddInfo(FString::Printf(
			TEXT("PENDING_ATTACK_REDIRECT_BUNDLE_DIGEST=%s"),
			*Loaded.Snapshot->ContentDigest));
	}
	return true;
}

WB_REDIRECT_TEST(FWBPendingAttackRedirectProductionSmoke,
	"Wandbound.AttackRedirect.Fixture.ProductionSmoke")
bool FWBPendingAttackRedirectProductionSmoke::RunTest(const FString&)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/PendingAttackRedirectFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionPendingAttackRedirectSmokeResult Result =
		WBProductionPendingAttackRedirectSmoke::Run(Request);
	if (!Result.bOk)
	{
		AddError(FString::Printf(
			TEXT("Production redirect smoke failed: %s"), *Result.Reason));
	}
	TestTrue(TEXT("Production redirect smoke succeeds"), Result.bOk);
	return true;
}

#undef WB_REDIRECT_TEST

#endif
