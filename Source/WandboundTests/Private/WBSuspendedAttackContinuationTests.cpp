#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionSuspendedAttackSmoke.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardInstanceRef MakeAttackCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerPlayerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerPlayerId;
	return Card;
}

FWBCardEffectDefinition MakeAttackResponseEffect(
	const FString& EffectId,
	const EWBCardActivationSourceZone SourceZone,
	const EWBGenericEffectOp Operation,
	const FString& UsageKey)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = TEXT("Respond");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::None;
	Effect.SourceGate.RequiredZone = SourceZone;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = SourceZone == EWBCardActivationSourceZone::Board;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.OncePerTurnKey = UsageKey;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	FWBGenericEffectPayload Payload;
	Payload.Operation = Operation;
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardEffectDefinition MakePostHitHealEffect()
{
	FWBCardEffectDefinition Effect = MakeAttackResponseEffect(
		TEXT("attack_effect_post_hit_heal"),
		EWBCardActivationSourceZone::Board,
		EWBGenericEffectOp::HealEffect,
		TEXT("attack_effect_post_hit_heal_once"));
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.Payloads[0].HealEffect.Amount = 1;
	Effect.Payloads[0].HealEffect.SourceReason = FName(TEXT("attack_post_hit_fixture"));
	return Effect;
}

FWBCardDefinition MakeAttackHero(
	const FString& CardId,
	const int32 ATK,
	const int32 AR,
	const TArray<FWBCardEffectDefinition>& Effects = {})
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 8;
	Definition.CharacterStats.ATK = ATK;
	Definition.CharacterStats.AR = AR;
	Definition.CharacterStats.RL = 3;
	Definition.ActivatedEffects = Effects;
	return Definition;
}

FWBCardDefinition MakeAttackActionDefinition(
	const FString& CardId,
	const FWBCardEffectDefinition& Effect)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Action;
	Definition.ActivatedEffects.Add(Effect);
	return Definition;
}

FWBCardDefinition MakeAttackFiller()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("attack_continuation_filler");
	Definition.PublicName = TEXT("Attack Continuation Filler");
	Definition.Kind = EWBCardDefinitionKind::Action;
	return Definition;
}

FWBCardDefinition MakeAttackTrap()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("attack_continuation_trap");
	Definition.PublicName = TEXT("Attack Continuation Trap");
	Definition.Kind = EWBCardDefinitionKind::Trap;
	Definition.TrapDamage = 1;
	return Definition;
}

FWBCardDefinition MakeAttackNPC()
{
	FWBCardDefinition Definition = MakeAttackHero(
		TEXT("attack_continuation_npc"), 1, 1);
	Definition.Kind = EWBCardDefinitionKind::NPC;
	return Definition;
}

FWBSetupMarkerPlacement MakeAttackMarker(
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
		? TEXT("attack_continuation_trap")
		: TEXT("attack_continuation_npc");
	Marker.PlacementOrder = Order;
	return Marker;
}

FWBMatchPlayerSetup MakeAttackPlayer(const int32 PlayerId, const bool bIncludeC)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(TEXT("attack_continuation_p%d_hero"), PlayerId);
	Setup.HeroCardId = PlayerId == 0
		? TEXT("attack_continuation_hero_a")
		: TEXT("attack_continuation_hero_b");
	Setup.OrderedDeck.Add(MakeAttackCard(
		Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
	if (PlayerId == 1 && bIncludeC)
	{
		Setup.OrderedDeck.Add(MakeAttackCard(
			TEXT("attack_continuation_c_instance"),
			TEXT("attack_continuation_c"),
			PlayerId));
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Setup.OrderedDeck.Add(MakeAttackCard(
			FString::Printf(TEXT("attack_continuation_p%d_filler_%d"), PlayerId, Index),
			TEXT("attack_continuation_filler"),
			PlayerId));
	}
	return Setup;
}

FWBMatchInitializationRequest MakeAttackRequest(
	const bool bIncludeA,
	const bool bIncludeB,
	const bool bIncludeC,
	const int32 DefenderAR = 0)
{
	const FWBCardEffectDefinition A = MakeAttackResponseEffect(
		TEXT("attack_effect_a_prevent"),
		EWBCardActivationSourceZone::Board,
		EWBGenericEffectOp::PreventPendingAttack,
		TEXT("attack_effect_a_once"));
	const FWBCardEffectDefinition B = MakeAttackResponseEffect(
		TEXT("attack_effect_b_negate"),
		EWBCardActivationSourceZone::Board,
		EWBGenericEffectOp::NegatePendingEffect,
		TEXT("attack_effect_b_once"));
	const FWBCardEffectDefinition C = MakeAttackResponseEffect(
		TEXT("attack_effect_c_negate"),
		EWBCardActivationSourceZone::Hand,
		EWBGenericEffectOp::NegatePendingEffect,
		TEXT("attack_effect_c_once"));

	FWBMatchInitializationRequest Request;
	Request.Seed = 99173;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("suspended_attack_tests");
	Request.Repository.SourceVersion = TEXT("suspended_attack_v1");
	Request.Repository.Definitions = {
		MakeAttackHero(
			TEXT("attack_continuation_hero_a"),
			3,
			8,
			bIncludeB ? TArray<FWBCardEffectDefinition>{ B } : TArray<FWBCardEffectDefinition>{}),
		MakeAttackHero(
			TEXT("attack_continuation_hero_b"),
			1,
			DefenderAR,
			bIncludeA ? TArray<FWBCardEffectDefinition>{ A } : TArray<FWBCardEffectDefinition>{}),
		MakeAttackActionDefinition(TEXT("attack_continuation_c"), C),
		MakeAttackFiller(),
		MakeAttackTrap(),
		MakeAttackNPC()
	};
	Request.Players = {
		MakeAttackPlayer(0, false),
		MakeAttackPlayer(1, bIncludeC)
	};
	Request.MarkerPlacements = {
		MakeAttackMarker(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeAttackMarker(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeAttackMarker(0, EWBMarkerType::NPC, FWBTile(0, 7), 2),
		MakeAttackMarker(0, EWBMarkerType::NPC, FWBTile(1, 7), 3),
		MakeAttackMarker(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeAttackMarker(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeAttackMarker(1, EWBMarkerType::NPC, FWBTile(0, 1), 6),
		MakeAttackMarker(1, EWBMarkerType::NPC, FWBTile(1, 1), 7)
	};
	return Request;
}

const FWBMatchLegalAction* FindAttackAction(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Attack;
	});
}

const FWBMatchLegalAction* FindAttackActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId)
{
	return Actions.FindByPredicate([&EffectId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == EffectId;
	});
}

const FWBMatchLegalAction* FindAttackPass(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::PassResponse;
	});
}

bool HasAttackTrace(const TArray<FWBTraceEvent>& Events, const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

int32 CountAttackTrace(const TArray<FWBTraceEvent>& Events, const TCHAR* Kind)
{
	int32 Count = 0;
	for (const FWBTraceEvent& Event : Events)
	{
		if (Event.Kind == FName(Kind))
		{
			++Count;
		}
	}
	return Count;
}

bool SubmitAttackActivation(
	WBMatchCoordinator& Coordinator,
	const FString& EffectId,
	FString& OutReason)
{
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Action = Legal.bOk
		? FindAttackActivation(Legal.Actions, EffectId)
		: nullptr;
	if (Action == nullptr)
	{
		OutReason = Legal.bOk ? EffectId + TEXT("_missing") : Legal.Reason;
		return false;
	}
	const FWBMatchOperationResult Result =
		Coordinator.SubmitActionId(Action->PlayerId, Action->ActionId);
	OutReason = Result.Reason;
	return Result.bOk;
}

bool PassAttackResponsesToAction(WBMatchCoordinator& Coordinator, FString& OutReason)
{
	for (int32 Guard = 0; Guard < 12
		&& Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response; ++Guard)
	{
		const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindAttackPass(Legal.Actions)
			: nullptr;
		if (Pass == nullptr)
		{
			OutReason = Legal.bOk ? TEXT("response_pass_missing") : Legal.Reason;
			return false;
		}
		const FWBMatchOperationResult Result =
			Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId);
		if (!Result.bOk)
		{
			OutReason = Result.Reason;
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

struct FAttackScenario
{
	bool bOk = false;
	FString Reason;
	WBMatchCoordinator Coordinator;
	int32 AttackerUnitId = -1;
	int32 DefenderUnitId = -1;
	int32 DefenderHPBefore = -1;
	FWBMatchOperationResult AttackResult;
};

FAttackScenario OpenAttackScenario(
	const bool bIncludeA,
	const bool bIncludeB,
	const bool bIncludeC,
	const int32 DefenderAR = 0)
{
	FAttackScenario Scenario;
	const FWBMatchOperationResult Started = Scenario.Coordinator.InitializeMatch(
		MakeAttackRequest(bIncludeA, bIncludeB, bIncludeC, DefenderAR));
	if (!Started.bOk)
	{
		Scenario.Reason = Started.Reason;
		return Scenario;
	}
	// Production turn-one protection forbids the first player from attacking the
	// opponent. This fixture starts at the next legal combat turn boundary.
	Scenario.Coordinator.GetMutableStateForTest().TurnNumber = 2;
	const FWBPlayerStateData* AttackerPlayer = Scenario.Coordinator.GetState().GetPlayerById(0);
	const FWBPlayerStateData* DefenderPlayer = Scenario.Coordinator.GetState().GetPlayerById(1);
	if (AttackerPlayer == nullptr || DefenderPlayer == nullptr)
	{
		Scenario.Reason = TEXT("player_missing");
		return Scenario;
	}
	Scenario.AttackerUnitId = AttackerPlayer->HeroUnitId;
	Scenario.DefenderUnitId = DefenderPlayer->HeroUnitId;
	const FWBUnitState* Defender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.DefenderUnitId);
	Scenario.DefenderHPBefore = Defender != nullptr ? Defender->HP : -1;
	const FWBMatchLegalActionGenerationResult Legal =
		Scenario.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = Legal.bOk
		? FindAttackAction(Legal.Actions)
		: nullptr;
	if (Attack == nullptr)
	{
		Scenario.Reason = Legal.bOk ? TEXT("attack_missing") : Legal.Reason;
		return Scenario;
	}
	Scenario.AttackResult = Scenario.Coordinator.SubmitActionId(
		Attack->PlayerId, Attack->ActionId);
	Scenario.bOk = Scenario.AttackResult.bOk;
	Scenario.Reason = Scenario.AttackResult.Reason;
	return Scenario;
}

FAttackScenario RunABCPreventionScenario()
{
	FAttackScenario Scenario = OpenAttackScenario(true, true, true);
	if (!Scenario.bOk)
	{
		return Scenario;
	}
	if (!SubmitAttackActivation(
		Scenario.Coordinator, TEXT("attack_effect_a_prevent"), Scenario.Reason)
		|| !SubmitAttackActivation(
			Scenario.Coordinator, TEXT("attack_effect_b_negate"), Scenario.Reason)
		|| !SubmitAttackActivation(
			Scenario.Coordinator, TEXT("attack_effect_c_negate"), Scenario.Reason))
	{
		Scenario.bOk = false;
		return Scenario;
	}
	Scenario.bOk = PassAttackResponsesToAction(Scenario.Coordinator, Scenario.Reason);
	return Scenario;
}

FAttackScenario RunABDamageScenario()
{
	FAttackScenario Scenario = OpenAttackScenario(true, true, false);
	if (!Scenario.bOk)
	{
		return Scenario;
	}
	if (!SubmitAttackActivation(
		Scenario.Coordinator, TEXT("attack_effect_a_prevent"), Scenario.Reason)
		|| !SubmitAttackActivation(
			Scenario.Coordinator, TEXT("attack_effect_b_negate"), Scenario.Reason))
	{
		Scenario.bOk = false;
		return Scenario;
	}
	Scenario.bOk = PassAttackResponsesToAction(Scenario.Coordinator, Scenario.Reason);
	return Scenario;
}
}

#define WB_SUSPENDED_ATTACK_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackDefaultClosed,
	"Wandbound.AttackContinuation.State.DefaultClosed")
bool FWBSuspendedAttackDefaultClosed::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	TestFalse(TEXT("No pending attack by default"), State.HasPendingAttack());
	TestEqual(TEXT("Default stage is None"),
		static_cast<int32>(State.PendingAttack.Stage),
		static_cast<int32>(EWBAttackContinuationStage::None));
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackPreHitBoundary,
	"Wandbound.AttackContinuation.Declaration.PreHitOpensBeforeDamage")
bool FWBSuspendedAttackPreHitBoundary::RunTest(const FString& Parameters)
{
	const FAttackScenario Scenario = OpenAttackScenario(true, true, true);
	TestTrue(TEXT("Attack accepted"), Scenario.bOk);
	TestTrue(TEXT("Continuation remains active"), Scenario.Coordinator.GetState().HasPendingAttack());
	TestEqual(TEXT("Stage is PreHit"),
		static_cast<int32>(Scenario.Coordinator.GetState().PendingAttack.Stage),
		static_cast<int32>(EWBAttackContinuationStage::PreHit));
	TestEqual(TEXT("PreHit is open"),
		static_cast<int32>(Scenario.Coordinator.GetState().ReactionWindow.Kind),
		static_cast<int32>(EWBReactionWindowKind::PreHit));
	const FWBUnitState* Defender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.DefenderUnitId);
	TestEqual(TEXT("No damage before PreHit closes"),
		Defender != nullptr ? Defender->HP : -1,
		Scenario.DefenderHPBefore);
	TestTrue(TEXT("Declaration traced"), HasAttackTrace(
		Scenario.AttackResult.TraceEvents, TEXT("attack_declared")));
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackResponseLegality,
	"Wandbound.AttackContinuation.Actions.ResponseOnly")
bool FWBSuspendedAttackResponseLegality::RunTest(const FString& Parameters)
{
	const FAttackScenario Scenario = OpenAttackScenario(true, true, true);
	const FWBMatchLegalActionGenerationResult Legal =
		Scenario.Coordinator.EnumerateLegalActions();
	TestTrue(TEXT("Response legal actions enumerate"), Legal.bOk);
	TestNotNull(TEXT("PassResponse present"), FindAttackPass(Legal.Actions));
	for (const FWBMatchLegalAction& Action : Legal.Actions)
	{
		if (Action.Family == EWBMatchActionFamily::CoreAction)
		{
			TestTrue(TEXT("Only PassResponse core action is exposed"),
				Action.CoreAction.Type == EWBActionType::PassResponse);
		}
		TestTrue(TEXT("Only activation or PassResponse is exposed"),
			Action.Family == EWBMatchActionFamily::Activation
			|| (Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == EWBActionType::PassResponse));
	}
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackABCPrevents,
	"Wandbound.AttackContinuation.Regression.ABCPreventsAttack")
bool FWBSuspendedAttackABCPrevents::RunTest(const FString& Parameters)
{
	const FAttackScenario Scenario = RunABCPreventionScenario();
	if (!Scenario.bOk)
	{
		AddError(FString::Printf(TEXT("Scenario failed: %s"), *Scenario.Reason));
	}
	TestTrue(TEXT("A/B/C scenario completes"), Scenario.bOk);
	const FWBUnitState* Defender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.DefenderUnitId);
	TestEqual(TEXT("Prevented attack deals no damage"),
		Defender != nullptr ? Defender->HP : -1,
		Scenario.DefenderHPBefore);
	TestTrue(TEXT("Attack prevention traced"), HasAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("attack_prevented")));
	TestEqual(TEXT("Damage never starts"), CountAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("attack_damage_started")), 0);
	TestFalse(TEXT("Continuation is cleared"), Scenario.Coordinator.GetState().HasPendingAttack());
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackABAllowsDamage,
	"Wandbound.AttackContinuation.Regression.ABNegatesPreventAndDamagesOnce")
bool FWBSuspendedAttackABAllowsDamage::RunTest(const FString& Parameters)
{
	const FAttackScenario Scenario = RunABDamageScenario();
	if (!Scenario.bOk)
	{
		AddError(FString::Printf(TEXT("Scenario failed: %s"), *Scenario.Reason));
	}
	TestTrue(TEXT("A/B scenario completes"), Scenario.bOk);
	const FWBUnitState* Defender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.DefenderUnitId);
	TestEqual(TEXT("Original attack deals current ATK once"),
		Defender != nullptr ? Defender->HP : -1,
		Scenario.DefenderHPBefore - 3);
	TestEqual(TEXT("Damage stage starts once"), CountAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("attack_damage_started")), 1);
	TestFalse(TEXT("Attack was not prevented"), HasAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("attack_prevented")));
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackCounterContinuation,
	"Wandbound.AttackContinuation.Counter.AutomaticSameContinuation")
bool FWBSuspendedAttackCounterContinuation::RunTest(const FString& Parameters)
{
	const FAttackScenario Scenario = OpenAttackScenario(false, false, false, 8);
	TestTrue(TEXT("Attack and automatic counter complete"), Scenario.bOk);
	TestTrue(TEXT("Counter stage traced"), HasAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("counter_started")));
	TestEqual(TEXT("Original and counter damage each start once"), CountAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("attack_damage_started")), 2);
	TestEqual(TEXT("Counter does not fabricate accepted player action"),
		Scenario.Coordinator.GetCommittedActionRecords().Num(), 1);
	const FWBUnitState* Attacker = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.AttackerUnitId);
	TestEqual(TEXT("Counter uses defender ATK"), Attacker != nullptr ? Attacker->HP : -1, 7);
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackFrozen,
	"Wandbound.AttackContinuation.Damage.FrozenBehaviorUnchanged")
bool FWBSuspendedAttackFrozen::RunTest(const FString& Parameters)
{
	FAttackScenario Scenario;
	const FWBMatchOperationResult Started = Scenario.Coordinator.InitializeMatch(
		MakeAttackRequest(false, false, false));
	TestTrue(TEXT("Match initializes"), Started.bOk);
	Scenario.Coordinator.GetMutableStateForTest().TurnNumber = 2;
	const int32 DefenderId = Scenario.Coordinator.GetState().GetPlayerById(1)->HeroUnitId;
	FWBUnitState* Defender = Scenario.Coordinator.GetMutableStateForTest().GetMutableUnitById(DefenderId);
	Defender->AddStatus(FName(TEXT("Frozen")), 1);
	const int32 HPBefore = Defender->HP;
	const FWBMatchLegalActionGenerationResult Legal =
		Scenario.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = Legal.bOk
		? FindAttackAction(Legal.Actions)
		: nullptr;
	TestNotNull(TEXT("Attack exists"), Attack);
	if (Attack == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult Applied = Scenario.Coordinator.SubmitActionId(
		Attack->PlayerId, Attack->ActionId);
	TestTrue(TEXT("Attack completes"), Applied.bOk);
	Defender = Scenario.Coordinator.GetMutableStateForTest().GetMutableUnitById(DefenderId);
	TestEqual(TEXT("Frozen absorbs ordinary damage"), Defender != nullptr ? Defender->HP : -1, HPBefore);
	TestFalse(TEXT("Frozen is removed"), Defender != nullptr && Defender->HasStatus(FName(TEXT("Frozen"))));
	TestFalse(TEXT("Frozen break does not counter"), HasAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("counter_started")));
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackTerminal,
	"Wandbound.AttackContinuation.Death.HeroTerminalStopsContinuation")
bool FWBSuspendedAttackTerminal::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(
		MakeAttackRequest(false, false, false));
	TestTrue(TEXT("Match initializes"), Started.bOk);
	Coordinator.GetMutableStateForTest().TurnNumber = 2;
	const int32 DefenderId = Coordinator.GetState().GetPlayerById(1)->HeroUnitId;
	Coordinator.GetMutableStateForTest().GetMutableUnitById(DefenderId)->HP = 2;
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = Legal.bOk
		? FindAttackAction(Legal.Actions)
		: nullptr;
	TestNotNull(TEXT("Attack exists"), Attack);
	if (Attack == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult Applied = Coordinator.SubmitActionId(
		Attack->PlayerId, Attack->ActionId);
	if (!Applied.bOk)
	{
		AddError(FString::Printf(TEXT("Terminal attack failed: %s"), *Applied.Reason));
	}
	TestTrue(TEXT("Terminal attack accepted"), Applied.bOk);
	TestTrue(TEXT("Match is terminal"), Coordinator.GetState().bGameOver);
	TestEqual(TEXT("Coordinator reaches GameOver"),
		static_cast<int32>(Coordinator.GetMatchPhase()),
		static_cast<int32>(EWBMatchLoopPhase::GameOver));
	TestFalse(TEXT("No pending attack survives terminal"), Coordinator.GetState().HasPendingAttack());
	TestFalse(TEXT("No response survives terminal"), Coordinator.GetState().HasOpenReactionWindow());
	TestFalse(TEXT("Terminal damage skips PostHit close"), HasAttackTrace(
		Coordinator.GetTraceLog(), TEXT("attack_post_hit_closed")));
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackAttackerRemoved,
	"Wandbound.AttackContinuation.Participants.AttackerRemovedDuringPreHit")
bool FWBSuspendedAttackAttackerRemoved::RunTest(const FString& Parameters)
{
	FAttackScenario Scenario = OpenAttackScenario(true, false, false);
	TestTrue(TEXT("PreHit opens"), Scenario.bOk);
	Scenario.Coordinator.GetMutableStateForTest().GetMutableUnitById(
		Scenario.AttackerUnitId)->RemoveUnitFromBoard();
	TestTrue(TEXT("Passes close safely"), PassAttackResponsesToAction(
		Scenario.Coordinator, Scenario.Reason));
	const FWBUnitState* Defender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.DefenderUnitId);
	TestEqual(TEXT("Removed attacker causes no damage"),
		Defender != nullptr ? Defender->HP : -1,
		Scenario.DefenderHPBefore);
	TestTrue(TEXT("Continuation cancellation traced"), HasAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("attack_continuation_cancelled")));
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackDefenderRemoved,
	"Wandbound.AttackContinuation.Participants.DefenderRemovedDuringPreHit")
bool FWBSuspendedAttackDefenderRemoved::RunTest(const FString& Parameters)
{
	FAttackScenario Scenario = OpenAttackScenario(true, false, false);
	TestTrue(TEXT("PreHit opens"), Scenario.bOk);
	Scenario.Coordinator.GetMutableStateForTest().GetMutableUnitById(
		Scenario.DefenderUnitId)->RemoveUnitFromBoard();
	TestTrue(TEXT("Passes close safely"), PassAttackResponsesToAction(
		Scenario.Coordinator, Scenario.Reason));
	TestFalse(TEXT("Continuation clears"), Scenario.Coordinator.GetState().HasPendingAttack());
	TestTrue(TEXT("Continuation cancellation traced"), HasAttackTrace(
		Scenario.Coordinator.GetTraceLog(), TEXT("attack_continuation_cancelled")));
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackDeterminism,
	"Wandbound.AttackContinuation.Replay.FreshRunDeterministic")
bool FWBSuspendedAttackDeterminism::RunTest(const FString& Parameters)
{
	const FAttackScenario First = RunABCPreventionScenario();
	const FAttackScenario Second = RunABCPreventionScenario();
	TestTrue(TEXT("First scenario completes"), First.bOk);
	TestTrue(TEXT("Second scenario completes"), Second.bOk);
	TestEqual(TEXT("State digests match"),
		First.Coordinator.GetCurrentStateDigest(),
		Second.Coordinator.GetCurrentStateDigest());
	TestEqual(TEXT("Trace digests match"),
		First.Coordinator.GetCurrentTraceDigest(),
		Second.Coordinator.GetCurrentTraceDigest());
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackRejectedResponse,
	"Wandbound.AttackContinuation.Replay.RejectedResponseNotRecorded")
bool FWBSuspendedAttackRejectedResponse::RunTest(const FString& Parameters)
{
	FAttackScenario Scenario = OpenAttackScenario(true, true, true);
	const int32 Before = Scenario.Coordinator.GetCommittedActionRecords().Num();
	const FWBMatchOperationResult Rejected = Scenario.Coordinator.SubmitActionId(
		0, TEXT("activate:stale:attack_response"));
	TestFalse(TEXT("Stale response rejected"), Rejected.bOk);
	TestEqual(TEXT("Rejected response is not recorded"),
		Scenario.Coordinator.GetCommittedActionRecords().Num(), Before);
	TestTrue(TEXT("Attack continuation remains active"),
		Scenario.Coordinator.GetState().HasPendingAttack());
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackStableSchema,
	"Wandbound.Authority.AttackContinuation.NoReplaySchemaChange")
bool FWBSuspendedAttackStableSchema::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackCurrentATK,
	"Wandbound.AttackContinuation.Participants.CurrentATKUsedAfterPreHit")
bool FWBSuspendedAttackCurrentATK::RunTest(const FString& Parameters)
{
	FAttackScenario Scenario = OpenAttackScenario(true, false, false);
	TestTrue(TEXT("PreHit opens"), Scenario.bOk);
	FWBUnitState* Attacker = Scenario.Coordinator.GetMutableStateForTest().GetMutableUnitById(
		Scenario.AttackerUnitId);
	Attacker->ATK = 1;
	TestTrue(TEXT("PreHit closes"), PassAttackResponsesToAction(
		Scenario.Coordinator, Scenario.Reason));
	const FWBUnitState* Defender = Scenario.Coordinator.GetState().GetUnitById(
		Scenario.DefenderUnitId);
	TestEqual(TEXT("Damage reads current ATK"),
		Defender != nullptr ? Defender->HP : -1,
		Scenario.DefenderHPBefore - 1);
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackPostHitNested,
	"Wandbound.AttackContinuation.PostHit.NestedActivationRestoresAndCompletes")
bool FWBSuspendedAttackPostHitNested::RunTest(const FString& Parameters)
{
	FWBMatchInitializationRequest Request = MakeAttackRequest(true, false, false);
	for (FWBCardDefinition& Definition : Request.Repository.Definitions)
	{
		if (Definition.CardId == TEXT("attack_continuation_hero_b"))
		{
			Definition.ActivatedEffects.Add(MakePostHitHealEffect());
		}
	}
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Request);
	TestTrue(TEXT("Match initializes"), Started.bOk);
	Coordinator.GetMutableStateForTest().TurnNumber = 2;
	const int32 DefenderId = Coordinator.GetState().GetPlayerById(1)->HeroUnitId;
	const FWBMatchLegalActionGenerationResult AttackLegal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = FindAttackAction(AttackLegal.Actions);
	TestNotNull(TEXT("Attack exists"), Attack);
	if (Attack == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Attack accepted"), Coordinator.SubmitActionId(
		Attack->PlayerId, Attack->ActionId).bOk);
	for (int32 Guard = 0; Guard < 2
		&& Coordinator.GetState().PendingAttack.Stage
			== EWBAttackContinuationStage::PreHit; ++Guard)
	{
		const FWBMatchLegalActionGenerationResult PreHitLegal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = FindAttackPass(PreHitLegal.Actions);
		TestNotNull(TEXT("PreHit pass exists"), Pass);
		if (Pass == nullptr
			|| !Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId).bOk)
		{
			return false;
		}
	}
	const FWBMatchLegalActionGenerationResult PostHitLegal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* React = PostHitLegal.Actions.FindByPredicate(
		[DefenderId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceEffectId
					== TEXT("attack_effect_post_hit_heal")
				&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId
					== DefenderId;
		});
	TestEqual(TEXT("Continuation reached PostHit"),
		static_cast<int32>(Coordinator.GetState().PendingAttack.Stage),
		static_cast<int32>(EWBAttackContinuationStage::PostHit));
	TestNull(TEXT("PreHit prevention is not exposed in PostHit"), FindAttackActivation(
		PostHitLegal.Actions, TEXT("attack_effect_a_prevent")));
	TestNotNull(TEXT("PostHit React exists"), React);
	if (React == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("PostHit React accepted"), Coordinator.SubmitActionId(
		React->PlayerId, React->ActionId).bOk);
	FString Reason;
	TestTrue(TEXT("Nested PostHit response completes"),
		PassAttackResponsesToAction(Coordinator, Reason));
	TestFalse(TEXT("Continuation clears"), Coordinator.GetState().HasPendingAttack());
	TestTrue(TEXT("Parent PostHit context restored"), HasAttackTrace(
		Coordinator.GetTraceLog(), TEXT("pending_effect_parent_context_restored")));
	const FWBUnitState* Defender = Coordinator.GetState().GetUnitById(DefenderId);
	TestEqual(TEXT("PostHit heal resolves after damage"), Defender != nullptr ? Defender->HP : -1, 6);
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackFixtureLoads,
	"Wandbound.AttackContinuation.Fixture.BundleLoads")
bool FWBSuspendedAttackFixtureLoads::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/SuspendedAttackFixture/root_manifest.json"));
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
	TestTrue(TEXT("Fixture bundle loads"), Loaded.bOk);
	if (Loaded.Snapshot.IsValid())
	{
		AddInfo(FString::Printf(
			TEXT("SUSPENDED_ATTACK_BUNDLE_DIGEST=%s"),
			*Loaded.Snapshot->ContentDigest));
	}
	return true;
}

WB_SUSPENDED_ATTACK_TEST(FWBSuspendedAttackProductionSmoke,
	"Wandbound.AttackContinuation.Fixture.ProductionSmoke")
bool FWBSuspendedAttackProductionSmoke::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Data/Replay/SuspendedAttackFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionSuspendedAttackSmokeResult Result =
		WBProductionSuspendedAttackSmoke::Run(Request);
	if (!Result.bOk)
	{
		AddError(FString::Printf(TEXT("Production smoke failed: %s"), *Result.Reason));
	}
	TestTrue(TEXT("Production smoke succeeds"), Result.bOk);
	return true;
}

#undef WB_SUSPENDED_ATTACK_TEST

#endif
