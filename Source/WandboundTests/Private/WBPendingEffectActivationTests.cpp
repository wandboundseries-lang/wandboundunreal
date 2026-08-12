#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardZoneObservation.h"
#include "WBMatchCoordinator.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionPendingEffectSmoke.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardInstanceRef MakeCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 PlayerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = PlayerId;
	return Card;
}

FWBGenericEffectPayload MakeHealPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("pending_fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeNegatePayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::NegatePendingEffect;
	return Payload;
}

FWBCardEffectDefinition MakeEffect(
	const FString& EffectId,
	const EWBCardActivationSourceZone Zone,
	const EWBCardActivationTimingRequirement Timing,
	const bool bNegate,
	const FString& UsageKey)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = bNegate ? TEXT("Negate") : TEXT("Activate");
	Effect.TargetRequirement = bNegate
		? EWBCardEffectTargetRequirement::None
		: EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate.RequiredZone = Zone;
	Effect.SourceGate.Timing = Timing;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit =
		Zone == EWBCardActivationSourceZone::Board
		|| Zone == EWBCardActivationSourceZone::Equipped;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.OncePerTurnKey = UsageKey;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	Effect.Payloads.Add(bNegate ? MakeNegatePayload() : MakeHealPayload());
	return Effect;
}

FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const TArray<FWBCardEffectDefinition>& Effects = {})
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 8;
	Definition.CharacterStats.ATK = 1;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	Definition.ActivatedEffects = Effects;
	return Definition;
}

FWBCardDefinition MakeAction(
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

FWBCardDefinition MakeWand(
	const FString& CardId,
	const FWBCardEffectDefinition& Effect)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = 0;
	Definition.ActivatedEffects.Add(Effect);
	return Definition;
}

FWBCardDefinition MakeFiller()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("pending_filler");
	Definition.PublicName = TEXT("Pending Filler");
	Definition.Kind = EWBCardDefinitionKind::Action;
	return Definition;
}

FWBCardDefinition MakeTrap()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("pending_trap");
	Definition.PublicName = TEXT("Pending Trap");
	Definition.Kind = EWBCardDefinitionKind::Trap;
	Definition.TrapDamage = 1;
	return Definition;
}

FWBSetupMarkerPlacement MakeMarker(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile& Tile,
	const int32 PlacementOrder)
{
	FWBSetupMarkerPlacement Marker;
	Marker.PlayerId = PlayerId;
	Marker.Type = Type;
	Marker.Tile = Tile;
	Marker.DefinitionId = Type == EWBMarkerType::Trap
		? TEXT("pending_trap")
		: TEXT("pending_npc");
	Marker.PlacementOrder = PlacementOrder;
	return Marker;
}

FWBMatchPlayerSetup MakePlayer(const int32 PlayerId)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(TEXT("pending_p%d_hero"), PlayerId);
	Setup.HeroCardId = PlayerId == 0
		? TEXT("pending_hero_a")
		: TEXT("pending_hero_b");
	Setup.OrderedDeck.Add(MakeCard(
		Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
	if (PlayerId == 0)
	{
		Setup.OrderedDeck.Add(MakeCard(
			TEXT("pending_negate_instance"),
			TEXT("pending_negate"),
			PlayerId));
		Setup.OrderedDeck.Add(MakeCard(
			TEXT("pending_wand_instance"),
			TEXT("pending_wand"),
			PlayerId));
	}
	for (int32 Index = 0; Index < 9; ++Index)
	{
		Setup.OrderedDeck.Add(MakeCard(
			FString::Printf(TEXT("pending_p%d_filler_%d"), PlayerId, Index),
			TEXT("pending_filler"),
			PlayerId));
	}
	return Setup;
}

FWBMatchInitializationRequest MakeRequest()
{
	const FWBCardEffectDefinition EffectA = MakeEffect(
		TEXT("effect_a"),
		EWBCardActivationSourceZone::Board,
		EWBCardActivationTimingRequirement::NormalTurnPriority,
		false,
		TEXT("effect_a_once"));
	const FWBCardEffectDefinition EffectB = MakeEffect(
		TEXT("effect_b"),
		EWBCardActivationSourceZone::Board,
		EWBCardActivationTimingRequirement::ResponseWindow,
		false,
		TEXT("effect_b_once"));
	FWBCardEffectDefinition CostedEffectB = EffectB;
	CostedEffectB.SourceGate.CostGate.RequiredRR = 1;
	CostedEffectB.SourceGate.CostGate.CostKind = FName(TEXT("RR"));
	CostedEffectB.SourceGate.CostGate.bRequiresExternalAffordability = true;
	const FWBCardEffectDefinition EffectC = MakeEffect(
		TEXT("effect_c"),
		EWBCardActivationSourceZone::Hand,
		EWBCardActivationTimingRequirement::ResponseWindow,
		true,
		TEXT("effect_c_once"));
	const FWBCardEffectDefinition WandEffect = MakeEffect(
		TEXT("effect_wand"),
		EWBCardActivationSourceZone::Equipped,
		EWBCardActivationTimingRequirement::NormalTurnPriority,
		false,
		TEXT("effect_wand_once"));

	FWBMatchInitializationRequest Request;
	Request.Seed = 87321;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("pending_effect_tests");
	Request.Repository.SourceVersion = TEXT("pending_effect_v1");
	Request.Repository.Definitions = {
		MakeCharacter(TEXT("pending_hero_a"), { EffectA }),
		MakeCharacter(TEXT("pending_hero_b"), { CostedEffectB }),
		MakeAction(TEXT("pending_negate"), EffectC),
		MakeWand(TEXT("pending_wand"), WandEffect),
		MakeFiller(),
		MakeTrap(),
		[]()
		{
			FWBCardDefinition NPC = MakeCharacter(TEXT("pending_npc"));
			NPC.Kind = EWBCardDefinitionKind::NPC;
			return NPC;
		}()
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

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId)
{
	return Actions.FindByPredicate([&EffectId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == EffectId;
	});
}

const FWBMatchLegalAction* FindResponsePass(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::PassResponse;
	});
}

bool HasTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

int32 CountTraceForFrame(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind,
	const FString& FrameId)
{
	int32 Count = 0;
	for (const FWBTraceEvent& Event : Events)
	{
		if (Event.Kind == Kind && Event.PendingEffectFrameId == FrameId)
		{
			++Count;
		}
	}
	return Count;
}

int32 MaxPendingDepth(const TArray<FWBTraceEvent>& Events)
{
	int32 MaxDepth = 0;
	for (const FWBTraceEvent& Event : Events)
	{
		MaxDepth = FMath::Max(MaxDepth, Event.PendingEffectStackDepth);
	}
	return MaxDepth;
}

struct FNestedScenario
{
	bool bOk = false;
	FString Reason;
	FWBMatchInitializationRequest Request;
	WBMatchCoordinator Coordinator;
	FWBMatchOperationResult A;
	FWBMatchOperationResult B;
	FWBMatchOperationResult C;
	FString AFrameId;
	FString BFrameId;
	FString CTargetFrameId;
};

FNestedScenario RunNestedScenario()
{
	FNestedScenario Result;
	Result.Request = MakeRequest();
	const FWBMatchOperationResult Started =
		Result.Coordinator.InitializeMatch(Result.Request);
	const FWBMatchLegalAction* A = Started.bOk
		? FindActivation(Started.NextLegalActions, TEXT("effect_a"))
		: nullptr;
	if (A == nullptr)
	{
		Result.Reason = Started.bOk ? TEXT("effect_a_missing") : Started.Reason;
		return Result;
	}
	Result.A = Result.Coordinator.SubmitActionId(A->PlayerId, A->ActionId);
	if (!Result.A.bOk || Result.Coordinator.GetPendingEffectActivationStack().Num() != 1)
	{
		Result.Reason = Result.A.bOk ? TEXT("effect_a_not_pending") : Result.A.Reason;
		return Result;
	}
	Result.AFrameId = Result.Coordinator.GetPendingEffectActivationStack()[0].FrameId;

	const FWBMatchLegalActionGenerationResult LegalB =
		Result.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* B = LegalB.bOk
		? FindActivation(LegalB.Actions, TEXT("effect_b"))
		: nullptr;
	if (B == nullptr)
	{
		Result.Reason = LegalB.bOk ? TEXT("effect_b_missing") : LegalB.Reason;
		return Result;
	}
	Result.B = Result.Coordinator.SubmitActionId(B->PlayerId, B->ActionId);
	if (!Result.B.bOk || Result.Coordinator.GetPendingEffectActivationStack().Num() != 2)
	{
		Result.Reason = Result.B.bOk ? TEXT("effect_b_not_pending") : Result.B.Reason;
		return Result;
	}
	Result.BFrameId = Result.Coordinator.GetPendingEffectActivationStack()[1].FrameId;

	const FWBMatchLegalActionGenerationResult LegalC =
		Result.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* C = LegalC.bOk
		? FindActivation(LegalC.Actions, TEXT("effect_c"))
		: nullptr;
	if (C == nullptr)
	{
		Result.Reason = LegalC.bOk ? TEXT("effect_c_missing") : LegalC.Reason;
		return Result;
	}
	if (!C->ActivationCommand.EffectRequest.Payloads.IsEmpty())
	{
		Result.CTargetFrameId =
			C->ActivationCommand.EffectRequest.Payloads[0].PendingEffectFrameId;
	}
	Result.C = Result.Coordinator.SubmitActionId(C->PlayerId, C->ActionId);
	Result.bOk = Result.C.bOk;
	Result.Reason = Result.C.Reason;
	return Result;
}

bool OpponentHandIsHidden(
	const FWBMatchObservation& Observation,
	const int32 OpponentPlayerId)
{
	const FWBObservedZoneSummary* Hand =
		Observation.CardZones.PublicSummary.PlayerHands.FindByPredicate(
			[OpponentPlayerId](const FWBObservedZoneSummary& Zone)
			{
				return Zone.OwnerPlayerId == OpponentPlayerId;
			});
	return Hand != nullptr && Hand->Cards.IsEmpty();
}
}

#define WB_PENDING_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_PENDING_TEST(FWBPendingStateDefaultEmpty,
	"Wandbound.PendingEffect.State.DefaultEmpty")
bool FWBPendingStateDefaultEmpty::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Default stack empty"),
		Coordinator.GetPendingEffectActivationStack().IsEmpty());
	return true;
}

WB_PENDING_TEST(FWBPendingAcceptedCreatesFrame,
	"Wandbound.PendingEffect.State.AcceptedActivationCreatesFrame")
bool FWBPendingAcceptedCreatesFrame::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	if (!Scenario.bOk)
	{
		AddError(FString::Printf(TEXT("Scenario failed: %s"), *Scenario.Reason));
	}
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestFalse(TEXT("A frame was assigned"), Scenario.AFrameId.IsEmpty());
	return true;
}

WB_PENDING_TEST(FWBPendingImmutableIdentity,
	"Wandbound.PendingEffect.State.ImmutableIdentity")
bool FWBPendingImmutableIdentity::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestNotEqual(TEXT("Frames have distinct identity"),
		Scenario.AFrameId, Scenario.BFrameId);
	return true;
}

WB_PENDING_TEST(FWBPendingNestedStackDepth,
	"Wandbound.PendingEffect.Nested.AThenBThenC")
bool FWBPendingNestedStackDepth::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestEqual(TEXT("Trace reaches depth three"), MaxPendingDepth(Scenario.C.TraceEvents), 3);
	return true;
}

WB_PENDING_TEST(FWBPendingNegationTargetsB,
	"Wandbound.PendingEffect.Negation.CTargetsExactBFrame")
bool FWBPendingNegationTargetsB::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestEqual(TEXT("C targets B"), Scenario.CTargetFrameId, Scenario.BFrameId);
	TestNotEqual(TEXT("C does not target A"), Scenario.CTargetFrameId, Scenario.AFrameId);
	return true;
}

WB_PENDING_TEST(FWBPendingNegatedSkipped,
	"Wandbound.PendingEffect.Negation.BSkippedExactlyOnce")
bool FWBPendingNegatedSkipped::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestTrue(TEXT("Negation traced"), HasTrace(
		Scenario.C.TraceEvents, FName(TEXT("pending_effect_activation_negated"))));
	TestTrue(TEXT("Skip traced"), HasTrace(
		Scenario.C.TraceEvents, FName(TEXT("pending_effect_activation_skipped"))));
	return true;
}

WB_PENDING_TEST(FWBPendingCNegatesBSoAResolves,
	"Wandbound.PendingEffect.Negation.CNegatesBSoAStillResolves")
bool FWBPendingCNegatesBSoAResolves::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	const TArray<FWBTraceEvent>& Trace = Scenario.Coordinator.GetTraceLog();
	TestEqual(TEXT("B skipped exactly once"), CountTraceForFrame(
		Trace, FName(TEXT("pending_effect_activation_skipped")),
		Scenario.BFrameId), 1);
	TestEqual(TEXT("A resolves exactly once"), CountTraceForFrame(
		Trace, FName(TEXT("pending_effect_activation_resolved")),
		Scenario.AFrameId), 1);
	TestEqual(TEXT("A is not skipped"), CountTraceForFrame(
		Trace, FName(TEXT("pending_effect_activation_skipped")),
		Scenario.AFrameId), 0);
	return true;
}

WB_PENDING_TEST(FWBPendingBResolvesAndNegatesA,
	"Wandbound.PendingEffect.Negation.BResolvesAndNegatesA")
bool FWBPendingBResolvesAndNegatesA::RunTest(const FString& Parameters)
{
	FWBMatchInitializationRequest Request = MakeRequest();
	for (FWBCardDefinition& Definition : Request.Repository.Definitions)
	{
		if (Definition.CardId == TEXT("pending_hero_b")
			&& !Definition.ActivatedEffects.IsEmpty())
		{
			Definition.ActivatedEffects[0].Payloads = { MakeNegatePayload() };
			Definition.ActivatedEffects[0].TargetRequirement =
				EWBCardEffectTargetRequirement::None;
		}
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Request);
	const FWBMatchLegalAction* A = FindActivation(
		Started.NextLegalActions, TEXT("effect_a"));
	TestNotNull(TEXT("A exists"), A);
	if (A == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("A accepted"),
		Coordinator.SubmitActionId(A->PlayerId, A->ActionId).bOk);
	const FString AFrameId =
		Coordinator.GetPendingEffectActivationStack().Last().FrameId;

	const FWBMatchLegalActionGenerationResult LegalB =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* B = LegalB.bOk
		? FindActivation(LegalB.Actions, TEXT("effect_b"))
		: nullptr;
	TestNotNull(TEXT("B exists"), B);
	if (B == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("B targets A exactly"),
		B->ActivationCommand.EffectRequest.Payloads[0].PendingEffectFrameId,
		AFrameId);
	TestTrue(TEXT("B accepted"),
		Coordinator.SubmitActionId(B->PlayerId, B->ActionId).bOk);

	for (int32 PassIndex = 0; PassIndex < 4
		&& !Coordinator.GetPendingEffectActivationStack().IsEmpty(); ++PassIndex)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindResponsePass(Legal.Actions)
			: nullptr;
		TestNotNull(TEXT("Pass exists while pending"), Pass);
		if (Pass == nullptr)
		{
			return false;
		}
		TestTrue(TEXT("Pass accepted"),
			Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId).bOk);
	}

	const TArray<FWBTraceEvent>& Trace = Coordinator.GetTraceLog();
	TestTrue(TEXT("Stack drains"),
		Coordinator.GetPendingEffectActivationStack().IsEmpty());
	TestEqual(TEXT("A skipped exactly once"), CountTraceForFrame(
		Trace, FName(TEXT("pending_effect_activation_skipped")),
		AFrameId), 1);
	TestEqual(TEXT("A payload never resolves"), CountTraceForFrame(
		Trace, FName(TEXT("pending_effect_activation_resolved")),
		AFrameId), 0);
	return true;
}

WB_PENDING_TEST(FWBPendingParentRestored,
	"Wandbound.PendingEffect.Nested.ParentContextRestored")
bool FWBPendingParentRestored::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestTrue(TEXT("Parent restoration traced"), HasTrace(
		Scenario.C.TraceEvents, FName(TEXT("pending_effect_parent_context_restored"))));
	TestTrue(TEXT("Stack drains"),
		Scenario.Coordinator.GetPendingEffectActivationStack().IsEmpty());
	TestEqual(TEXT("Returns to action"),
		static_cast<int32>(Scenario.Coordinator.GetMatchPhase()),
		static_cast<int32>(EWBMatchLoopPhase::Action));
	return true;
}

WB_PENDING_TEST(FWBPendingNoStuckResponse,
	"Wandbound.PendingEffect.Nested.NoStuckResponse")
bool FWBPendingNoStuckResponse::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestFalse(TEXT("No open response"),
		Scenario.Coordinator.GetState().HasOpenReactionWindow());
	return true;
}

WB_PENDING_TEST(FWBPendingTerminalClearsStack,
	"Wandbound.PendingEffect.State.TerminalClearsStack")
bool FWBPendingTerminalClearsStack::RunTest(const FString& Parameters)
{
	FWBMatchInitializationRequest Request = MakeRequest();
	for (FWBCardDefinition& Definition : Request.Repository.Definitions)
	{
		if (Definition.CardId != TEXT("pending_hero_a")
			|| Definition.ActivatedEffects.IsEmpty())
		{
			continue;
		}
		FWBGenericEffectPayload Damage;
		Damage.Operation = EWBGenericEffectOp::DamageEffect;
		Damage.DamageEffect.Amount = 99;
		Damage.DamageEffect.bBypassArmor = true;
		Damage.DamageEffect.DamageCause = FName(TEXT("pending_terminal"));
		Definition.ActivatedEffects[0].Payloads = { Damage };
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Request);
	const FWBPlayerStateData* Player1 = Coordinator.GetState().GetPlayerById(1);
	const int32 TargetHeroId = Player1 != nullptr ? Player1->HeroUnitId : -1;
	const FWBMatchLegalAction* A = Started.NextLegalActions.FindByPredicate(
		[TargetHeroId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceEffectId == TEXT("effect_a")
				&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId
					== TargetHeroId;
		});
	TestNotNull(TEXT("Terminal A action exists"), A);
	if (A == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("A accepted"),
		Coordinator.SubmitActionId(A->PlayerId, A->ActionId).bOk);
	for (int32 PassIndex = 0; PassIndex < 2; ++PassIndex)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindResponsePass(Legal.Actions)
			: nullptr;
		TestNotNull(TEXT("Response pass exists"), Pass);
		if (Pass == nullptr)
		{
			return false;
		}
		TestTrue(TEXT("Response pass accepted"),
			Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId).bOk);
	}
	TestTrue(TEXT("Match terminal"), Coordinator.GetState().bGameOver);
	TestTrue(TEXT("Terminal clears pending stack"),
		Coordinator.GetPendingEffectActivationStack().IsEmpty());
	TestFalse(TEXT("Terminal clears reaction"),
		Coordinator.GetState().HasOpenReactionWindow());
	return true;
}

WB_PENDING_TEST(FWBPendingUsageReserved,
	"Wandbound.PendingEffect.Costs.AcceptedUsageReserved")
bool FWBPendingUsageReserved::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	TestTrue(TEXT("A usage reserved"),
		Scenario.Coordinator.GetState().HasActivationUsageKeyThisTurn(0, TEXT("effect_a_once")));
	TestTrue(TEXT("B usage remains consumed when negated"),
		Scenario.Coordinator.GetState().HasActivationUsageKeyThisTurn(1, TEXT("effect_b_once")));
	TestTrue(TEXT("C usage reserved"),
		Scenario.Coordinator.GetState().HasActivationUsageKeyThisTurn(0, TEXT("effect_c_once")));
	return true;
}

WB_PENDING_TEST(FWBPendingNegatedCostNotPaid,
	"Wandbound.PendingEffect.Costs.NegatedRRNotPaid")
bool FWBPendingNegatedCostNotPaid::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	const FWBPlayerStateData* Player = Scenario.Coordinator.GetState().GetPlayerById(1);
	const FWBUnitState* Hero = Player != nullptr
		? Scenario.Coordinator.GetState().GetUnitById(Player->HeroUnitId)
		: nullptr;
	TestTrue(TEXT("Scenario completes"), Scenario.bOk);
	TestNotNull(TEXT("Player B hero exists"), Hero);
	if (Hero != nullptr)
	{
		TestEqual(TEXT("Negated B pays no RR"), Hero->RLUsed, 0);
	}
	TestFalse(TEXT("No cost-paid trace for skipped B"), HasTrace(
		Scenario.C.TraceEvents, FName(TEXT("card_activation_cost_paid"))));
	return true;
}

WB_PENDING_TEST(FWBPendingHandConsumed,
	"Wandbound.PendingEffect.Sources.HandConsumedOnAcceptance")
bool FWBPendingHandConsumed::RunTest(const FString& Parameters)
{
	const FNestedScenario Scenario = RunNestedScenario();
	FWBZoneCardEntry Entry;
	TestTrue(TEXT("Negate instance still exists"),
		WBCardZoneState::FindCardByInstanceId(
			Scenario.Coordinator.GetState().GetCardZoneState(),
			TEXT("pending_negate_instance"), Entry));
	TestEqual(TEXT("Negate card is discarded"),
		static_cast<int32>(Entry.Zone), static_cast<int32>(EWBCardZone::Discard));
	return true;
}

WB_PENDING_TEST(FWBPendingBoardSourceParity,
	"Wandbound.PendingEffect.Sources.BoardUsesSameLifecycle")
bool FWBPendingBoardSourceParity::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(MakeRequest());
	const FWBMatchLegalAction* A = FindActivation(
		Started.NextLegalActions, TEXT("effect_a"));
	TestNotNull(TEXT("Board activation exists"), A);
	if (A == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Board activation accepted"),
		Coordinator.SubmitActionId(A->PlayerId, A->ActionId).bOk);
	TestEqual(TEXT("One pending frame"),
		Coordinator.GetPendingEffectActivationStack().Num(), 1);
	if (Coordinator.GetPendingEffectActivationStack().Num() == 1)
	{
		const FWBPendingEffectActivationFrame& Frame =
			Coordinator.GetPendingEffectActivationStack()[0];
		TestEqual(TEXT("Frame preserves Board source"),
			static_cast<int32>(Frame.Command.Source.SourceZone),
			static_cast<int32>(EWBCardZone::Board));
		TestTrue(TEXT("Frame preserves source unit"),
			Frame.Command.Source.SourceUnitId >= 0);
	}
	return true;
}

WB_PENDING_TEST(FWBPendingEquippedSourceParity,
	"Wandbound.PendingEffect.Sources.EquippedUsesSameLifecycle")
bool FWBPendingEquippedSourceParity::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(MakeRequest());
	const FWBPlayerStateData* Player = Coordinator.GetState().GetPlayerById(0);
	const int32 HeroUnitId = Player != nullptr ? Player->HeroUnitId : -1;
	const FWBMatchLegalAction* Equip = Started.NextLegalActions.FindByPredicate(
		[HeroUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Equip
				&& Action.EquipRequest.SourceInstanceId == TEXT("pending_wand_instance")
				&& Action.EquipRequest.TargetUnitId == HeroUnitId;
		});
	TestNotNull(TEXT("Wand equip exists"), Equip);
	if (Equip == nullptr)
	{
		return false;
	}
	const FString EquipActionId = Equip->ActionId;
	TestTrue(TEXT("Wand equip accepted"),
		Coordinator.SubmitActionId(0, EquipActionId).bOk);

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* ActivateWand = Legal.bOk
		? FindActivation(Legal.Actions, TEXT("effect_wand"))
		: nullptr;
	TestNotNull(TEXT("Equipped activation exists"), ActivateWand);
	if (ActivateWand == nullptr)
	{
		return false;
	}
	const FString ActivationId = ActivateWand->ActionId;
	TestTrue(TEXT("Equipped activation accepted"),
		Coordinator.SubmitActionId(0, ActivationId).bOk);
	TestEqual(TEXT("One pending frame"),
		Coordinator.GetPendingEffectActivationStack().Num(), 1);
	if (Coordinator.GetPendingEffectActivationStack().Num() == 1)
	{
		const FWBPendingEffectActivationFrame& Frame =
			Coordinator.GetPendingEffectActivationStack()[0];
		TestEqual(TEXT("Frame preserves Equipped source"),
			static_cast<int32>(Frame.Command.Source.SourceZone),
			static_cast<int32>(EWBCardZone::Equipped));
		TestEqual(TEXT("Frame preserves exact Wand instance"),
			Frame.Command.Source.SourceCardInstanceId,
			FString(TEXT("pending_wand_instance")));
		TestEqual(TEXT("Frame preserves equipped unit"),
			Frame.Command.Source.SourceUnitId, HeroUnitId);
	}
	return true;
}

WB_PENDING_TEST(FWBPendingMultipleLegalResponses,
	"Wandbound.PendingEffect.Actions.MultipleLegalResponsesAndPass")
bool FWBPendingMultipleLegalResponses::RunTest(const FString& Parameters)
{
	FWBMatchInitializationRequest Request = MakeRequest();
	const FWBCardEffectDefinition EquivalentA = MakeEffect(
		TEXT("equivalent_response_a"),
		EWBCardActivationSourceZone::Hand,
		EWBCardActivationTimingRequirement::ResponseWindow,
		false,
		TEXT("equivalent_response_a_once"));
	const FWBCardEffectDefinition EquivalentB = MakeEffect(
		TEXT("equivalent_response_b"),
		EWBCardActivationSourceZone::Hand,
		EWBCardActivationTimingRequirement::ResponseWindow,
		false,
		TEXT("equivalent_response_b_once"));
	Request.Repository.Definitions.Add(MakeAction(
		TEXT("equivalent_definition_a"), EquivalentA));
	Request.Repository.Definitions.Add(MakeAction(
		TEXT("equivalent_definition_b"), EquivalentB));
	FWBMatchPlayerSetup* Player1 = Request.Players.FindByPredicate(
		[](const FWBMatchPlayerSetup& Player)
		{
			return Player.PlayerId == 1;
		});
	if (Player1 == nullptr)
	{
		AddError(TEXT("Player 1 setup missing"));
		return false;
	}
	Player1->OrderedDeck.Insert(MakeCard(
		TEXT("equivalent_instance_a"), TEXT("equivalent_definition_a"), 1), 1);
	Player1->OrderedDeck.Insert(MakeCard(
		TEXT("equivalent_instance_b"), TEXT("equivalent_definition_b"), 1), 2);

	auto EnumerateResponses = [&Request](
		TArray<FString>& OutIds,
		FWBMatchLegalAction& OutA,
		FWBMatchLegalAction& OutB) -> bool
	{
		WBMatchCoordinator Coordinator;
		const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Request);
		const FWBMatchLegalAction* Outer = FindActivation(
			Started.NextLegalActions, TEXT("effect_a"));
		if (Outer == nullptr
			|| !Coordinator.SubmitActionId(Outer->PlayerId, Outer->ActionId).bOk)
		{
			return false;
		}
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		if (!Legal.bOk)
		{
			return false;
		}
		for (const FWBMatchLegalAction& Action : Legal.Actions)
		{
			OutIds.Add(Action.ActionId);
			if (Action.ActivationCommand.Source.SourceEffectId
				== TEXT("equivalent_response_a"))
			{
				OutA = Action;
			}
			else if (Action.ActivationCommand.Source.SourceEffectId
				== TEXT("equivalent_response_b"))
			{
				OutB = Action;
			}
		}
		return true;
	};

	TArray<FString> FirstIds;
	TArray<FString> SecondIds;
	FWBMatchLegalAction FirstA;
	FWBMatchLegalAction FirstB;
	FWBMatchLegalAction SecondA;
	FWBMatchLegalAction SecondB;
	TestTrue(TEXT("First response enumeration succeeds"),
		EnumerateResponses(FirstIds, FirstA, FirstB));
	TestTrue(TEXT("Second response enumeration succeeds"),
		EnumerateResponses(SecondIds, SecondA, SecondB));
	TestFalse(TEXT("Equivalent A is enumerated"), FirstA.ActionId.IsEmpty());
	TestFalse(TEXT("Equivalent B is enumerated"), FirstB.ActionId.IsEmpty());
	TestTrue(TEXT("Pass is also enumerated"),
		FirstIds.ContainsByPredicate([](const FString& Id)
		{
			return Id.StartsWith(TEXT("pass_response:"));
		}));
	TestEqual(TEXT("Deterministic ordering count"),
		FirstIds.Num(), SecondIds.Num());
	for (int32 Index = 0; Index < FMath::Min(FirstIds.Num(), SecondIds.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Deterministic action %d"), Index),
			FirstIds[Index], SecondIds[Index]);
	}
	TestNotEqual(TEXT("Different definitions retain identity"),
		FirstA.ActionId, FirstB.ActionId);
	TestEqual(TEXT("Equivalent definitions share source lifecycle"),
		static_cast<int32>(FirstA.ActivationCommand.Source.SourceZone),
		static_cast<int32>(FirstB.ActivationCommand.Source.SourceZone));
	TestEqual(TEXT("Equivalent definitions share operation"),
		static_cast<int32>(FirstA.ActivationCommand.EffectRequest.Payloads[0].Operation),
		static_cast<int32>(FirstB.ActivationCommand.EffectRequest.Payloads[0].Operation));
	return true;
}

WB_PENDING_TEST(FWBPendingResponseActionsOnly,
	"Wandbound.PendingEffect.Actions.ResponseFamiliesOnly")
bool FWBPendingResponseActionsOnly::RunTest(const FString& Parameters)
{
	FWBMatchInitializationRequest Request = MakeRequest();
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Request);
	const FWBMatchLegalAction* A = FindActivation(Started.NextLegalActions, TEXT("effect_a"));
	const FWBMatchOperationResult Submitted = A != nullptr
		? Coordinator.SubmitActionId(A->PlayerId, A->ActionId)
		: FWBMatchOperationResult();
	TestTrue(TEXT("A accepted"), Submitted.bOk);
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	TestTrue(TEXT("Legal response set"), Legal.bOk);
	for (const FWBMatchLegalAction& Action : Legal.Actions)
	{
		TestTrue(TEXT("Only activation or response pass"),
			Action.Family == EWBMatchActionFamily::Activation
			|| (Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == EWBActionType::PassResponse));
	}
	return true;
}

WB_PENDING_TEST(FWBPendingPrivacy,
	"Wandbound.PendingEffect.Privacy.OpponentHandAndFramesHidden")
bool FWBPendingPrivacy::RunTest(const FString& Parameters)
{
	FWBMatchInitializationRequest Request = MakeRequest();
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Request);
	const FWBMatchLegalAction* A = FindActivation(Started.NextLegalActions, TEXT("effect_a"));
	if (A != nullptr) Coordinator.SubmitActionId(A->PlayerId, A->ActionId);
	const FWBMatchObservation Observation = Coordinator.BuildObservation(1);
	TestTrue(TEXT("Opponent hand hidden"), OpponentHandIsHidden(Observation, 0));
	for (const FWBMatchLegalAction& Action : Observation.LegalActions)
	{
		TestFalse(TEXT("Other hand instance not exposed"),
			Action.ActionId.Contains(TEXT("pending_p0_filler")));
	}
	return true;
}

WB_PENDING_TEST(FWBPendingStateDigest,
	"Wandbound.PendingEffect.Replay.StateDigestIncludesPendingStack")
bool FWBPendingStateDigest::RunTest(const FString& Parameters)
{
	FWBMatchInitializationRequest Request = MakeRequest();
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Request);
	const FString Before = Coordinator.GetCurrentStateDigest();
	const FWBMatchLegalAction* A = FindActivation(Started.NextLegalActions, TEXT("effect_a"));
	if (A != nullptr) Coordinator.SubmitActionId(A->PlayerId, A->ActionId);
	TestNotEqual(TEXT("Pending frame changes digest"),
		Coordinator.GetCurrentStateDigest(), Before);
	return true;
}

WB_PENDING_TEST(FWBPendingDeterminism,
	"Wandbound.PendingEffect.Replay.DeterministicFreshRun")
bool FWBPendingDeterminism::RunTest(const FString& Parameters)
{
	const FNestedScenario First = RunNestedScenario();
	const FNestedScenario Second = RunNestedScenario();
	TestTrue(TEXT("First completes"), First.bOk);
	TestTrue(TEXT("Second completes"), Second.bOk);
	TestEqual(TEXT("State digests match"),
		First.Coordinator.GetCurrentStateDigest(),
		Second.Coordinator.GetCurrentStateDigest());
	TestEqual(TEXT("Trace digests match"),
		First.Coordinator.GetCurrentTraceDigest(),
		Second.Coordinator.GetCurrentTraceDigest());
	return true;
}

WB_PENDING_TEST(FWBPendingFixtureBundleLoads,
	"Wandbound.PendingEffect.Fixture.BundleLoads")
bool FWBPendingFixtureBundleLoads::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/PendingEffectActivationFixture/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	if (!Loaded.bOk)
	{
		AddError(FString::Printf(TEXT("Fixture load reason: %s"), *Loaded.Reason));
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
	TestTrue(TEXT("Fixture snapshot exists"), Loaded.Snapshot.IsValid());
	if (Loaded.Snapshot.IsValid())
	{
		AddInfo(FString::Printf(
			TEXT("PENDING_EFFECT_BUNDLE_DIGEST=%s"),
			*Loaded.Snapshot->ContentDigest));
		TestEqual(TEXT("Digest is SHA-256 length"),
			Loaded.Snapshot->ContentDigest.Len(), 64);
	}
	return true;
}

WB_PENDING_TEST(FWBPendingProductionSmoke,
	"Wandbound.PendingEffect.Fixture.ProductionSmoke")
bool FWBPendingProductionSmoke::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Data/Replay/PendingEffectActivationFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionPendingEffectSmokeResult Result =
		WBProductionPendingEffectSmoke::Run(Request);
	if (!Result.bOk)
	{
		AddError(FString::Printf(TEXT("Production smoke failed: %s"), *Result.Reason));
	}
	TestTrue(TEXT("Production smoke succeeds"), Result.bOk);
	TestEqual(TEXT("Three accepted actions replay"), Result.RecordsVerified, 3);
	return true;
}

WB_PENDING_TEST(FWBPendingRejectedNotCommitted,
	"Wandbound.PendingEffect.Replay.RejectedActionNotCommitted")
bool FWBPendingRejectedNotCommitted::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(MakeRequest());
	const int32 BeforeRecords = Coordinator.GetCommittedActionRecords().Num();
	const FWBMatchOperationResult Rejected = Coordinator.SubmitActionId(
		0, TEXT("activate:stale:pending"));
	TestFalse(TEXT("Action rejected"), Rejected.bOk);
	TestEqual(TEXT("No replay record"),
		Coordinator.GetCommittedActionRecords().Num(), BeforeRecords);
	TestTrue(TEXT("No pending frame"),
		Coordinator.GetPendingEffectActivationStack().IsEmpty());
	return true;
}

WB_PENDING_TEST(FWBPendingCodecUnchanged,
	"Wandbound.Authority.PendingEffect.NoActionCodecChange")
bool FWBPendingCodecUnchanged::RunTest(const FString& Parameters)
{
	FString Source;
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Source/WandboundCore/Private/WBActionCodec.cpp"));
	TestTrue(TEXT("Codec source readable"), FFileHelper::LoadFileToString(Source, *Path));
	TestFalse(TEXT("Codec has no pending effect branch"),
		Source.Contains(TEXT("PendingEffect")));
	return true;
}

WB_PENDING_TEST(FWBPendingSchemaUnchanged,
	"Wandbound.Authority.PendingEffect.NoReplaySchemaChange")
bool FWBPendingSchemaUnchanged::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_PENDING_TEST(FWBPendingSingleAuthority,
	"Wandbound.Authority.PendingEffect.CoordinatorOwnsStack")
bool FWBPendingSingleAuthority::RunTest(const FString& Parameters)
{
	FString RuntimeSource;
	const FString RuntimeDir = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Source/WandboundRuntime"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *RuntimeDir, TEXT("*.cpp"), true, false);
	for (const FString& File : Files)
	{
		FString Contents;
		FFileHelper::LoadFileToString(Contents, *File);
		RuntimeSource += Contents;
	}
	TestFalse(TEXT("Runtime does not mutate pending stack"),
		RuntimeSource.Contains(TEXT("PendingEffectActivations.Add")));
	TestFalse(TEXT("Runtime does not pop pending stack"),
		RuntimeSource.Contains(TEXT("PendingEffectActivations.Pop")));
	TestFalse(TEXT("Runtime does not remove pending frames"),
		RuntimeSource.Contains(TEXT("PendingEffectActivations.Remove")));
	return true;
}

WB_PENDING_TEST(FWBPendingNoCardIdAuthority,
	"Wandbound.Authority.PendingEffect.NoCardIdBranchInReactionAuthority")
bool FWBPendingNoCardIdAuthority::RunTest(const FString& Parameters)
{
	const TArray<FString> Paths = {
		TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp"),
		TEXT("Source/WandboundCore/Private/WBCardActivationCandidateGenerator.cpp"),
		TEXT("Source/WandboundCore/Private/WBCardActivationLegalActionGenerator.cpp"),
		TEXT("Source/WandboundCore/Private/WBEffectRunner.cpp")
	};
	const TArray<FString> ForbiddenIds = {
		TEXT("marrow_oddsman"),
		TEXT("oddsman"),
		TEXT("sealplate"),
		TEXT("null_sigil"),
		TEXT("claimshifter"),
		TEXT("crash-in"),
		TEXT("crash_in"),
		TEXT("sever_thread"),
		TEXT("shatter_parry")
	};
	for (const FString& RelativePath : Paths)
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("%s loads"), *RelativePath),
			FFileHelper::LoadFileToString(
				Source,
				*FPaths::Combine(FPaths::ProjectDir(), RelativePath)));
		Source.ToLowerInline();
		for (const FString& ForbiddenId : ForbiddenIds)
		{
			TestFalse(*FString::Printf(
				TEXT("%s does not branch on %s"),
				*RelativePath,
				*ForbiddenId),
				Source.Contains(ForbiddenId));
		}
	}
	return true;
}

#undef WB_PENDING_TEST

#endif
