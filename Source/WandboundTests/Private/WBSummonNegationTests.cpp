#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBMatchCoordinator.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionSummonNegationSmoke.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardInstanceRef MakeCard(
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

FWBCardEffectDefinition MakeResponseEffect(
	const FString& EffectId,
	const EWBGenericEffectOp Operation,
	const FString& UsageKey)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = EffectId;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::None;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.OncePerTurnKey = UsageKey;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	FWBGenericEffectPayload Payload;
	Payload.Operation = Operation;
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardEffectDefinition MakeTerminalResponseEffect()
{
	FWBCardEffectDefinition Effect = MakeResponseEffect(
		TEXT("terminal_response"),
		EWBGenericEffectOp::DamageEffect,
		TEXT("terminal_response_once"));
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.Payloads[0].DamageEffect.Amount = 999;
	Effect.Payloads[0].DamageEffect.bBypassArmor = true;
	Effect.Payloads[0].DamageEffect.DamageCause = FName(TEXT("fixture_react"));
	Effect.Payloads[0].DamageEffect.SourceReason = FName(TEXT("terminal_response"));
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
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	Definition.ActivatedEffects = Effects;
	return Definition;
}

FWBCardDefinition MakeFiller()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("summon_negation_filler");
	Definition.PublicName = TEXT("Summon Negation Filler");
	Definition.Kind = EWBCardDefinitionKind::Action;
	return Definition;
}

FWBCardDefinition MakeTrap()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("summon_negation_trap");
	Definition.PublicName = TEXT("Summon Negation Trap");
	Definition.Kind = EWBCardDefinitionKind::Trap;
	Definition.TrapDamage = 1;
	return Definition;
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
		? TEXT("summon_negation_trap")
		: TEXT("summon_negation_npc");
	Marker.PlacementOrder = Order;
	return Marker;
}

FWBMatchPlayerSetup MakePlayer(const int32 PlayerId)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(
		TEXT("summon_negation_p%d_hero"), PlayerId);
	Setup.HeroCardId = PlayerId == 0
		? TEXT("summon_negation_hero_a")
		: TEXT("summon_negation_hero_b");
	Setup.OrderedDeck.Add(MakeCard(
		Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
	if (PlayerId == 0)
	{
		Setup.OrderedDeck.Add(MakeCard(
			TEXT("summon_copy_a2"), TEXT("summon_student"), PlayerId));
		Setup.OrderedDeck.Add(MakeCard(
			TEXT("summon_copy_a1"), TEXT("summon_student"), PlayerId));
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Setup.OrderedDeck.Add(MakeCard(
			FString::Printf(TEXT("summon_negation_p%d_filler_%d"), PlayerId, Index),
			TEXT("summon_negation_filler"),
			PlayerId));
	}
	return Setup;
}

FWBMatchInitializationRequest MakeRequest(
	const bool bSummonNegator,
	const bool bEffectNegator = false,
	const bool bTerminalResponse = false)
{
	TArray<FWBCardEffectDefinition> Player0Effects;
	if (bEffectNegator)
	{
		Player0Effects.Add(MakeResponseEffect(
			TEXT("negate_pending_effect"),
			EWBGenericEffectOp::NegatePendingEffect,
			TEXT("negate_pending_effect_once")));
	}
	TArray<FWBCardEffectDefinition> Player1Effects;
	if (bSummonNegator)
	{
		Player1Effects.Add(MakeResponseEffect(
			TEXT("negate_pending_summon"),
			EWBGenericEffectOp::NegatePendingSummon,
			TEXT("negate_pending_summon_once")));
	}
	if (bTerminalResponse)
	{
		Player1Effects.Add(MakeTerminalResponseEffect());
	}

	FWBCardDefinition NPC = MakeCharacter(TEXT("summon_negation_npc"));
	NPC.Kind = EWBCardDefinitionKind::NPC;
	FWBMatchInitializationRequest Request;
	Request.Seed = 24681357;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("summon_negation_tests");
	Request.Repository.SourceVersion = TEXT("summon_negation_v1");
	Request.Repository.Definitions = {
		MakeCharacter(TEXT("summon_negation_hero_a"), Player0Effects),
		MakeCharacter(TEXT("summon_negation_hero_b"), Player1Effects),
		MakeCharacter(TEXT("summon_student")),
		MakeFiller(),
		MakeTrap(),
		NPC
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

const FWBMatchLegalAction* FindSummon(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& InstanceId = TEXT("summon_copy_a2"))
{
	return Actions.FindByPredicate([&InstanceId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& !Action.bHybridSummon
			&& Action.SummonRequest.SourceInstanceId == InstanceId;
	});
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

const FWBMatchLegalAction* FindPass(const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::PassResponse;
	});
}

const FWBPlayerCardZoneState* GetZones(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	return WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), PlayerId);
}

bool ZoneHasInstance(
	const TArray<FWBZoneCardEntry>& Entries,
	const FString& InstanceId)
{
	return Entries.ContainsByPredicate([&InstanceId](const FWBZoneCardEntry& Entry)
	{
		return Entry.Card.InstanceId == InstanceId;
	});
}

TArray<FString> HandInstances(const FWBGameStateData& State, const int32 PlayerId)
{
	TArray<FString> Result;
	const FWBPlayerCardZoneState* Zones = GetZones(State, PlayerId);
	if (Zones != nullptr)
	{
		for (const FWBZoneCardEntry& Entry : Zones->Hand)
		{
			Result.Add(Entry.Card.InstanceId);
		}
	}
	return Result;
}

bool HasUnitCard(const FWBGameStateData& State, const FString& CardId)
{
	return State.Units.ContainsByPredicate([&CardId](const FWBUnitState& Unit)
	{
		return Unit.IsUnitOnBoard() && Unit.CardId == CardId;
	});
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

int32 CountTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	int32 Count = 0;
	for (const FWBTraceEvent& Event : Events)
	{
		Count += Event.Kind == Kind ? 1 : 0;
	}
	return Count;
}

int32 TraceIndex(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.IndexOfByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

struct FPendingScenario
{
	FWBMatchInitializationRequest Request;
	WBMatchCoordinator Coordinator;
	FWBMatchOperationResult Declaration;
	FString SummonActionId;
	bool bOk = false;
	FString Reason;
};

FPendingScenario OpenPending(
	const bool bEffectNegator = false,
	const bool bTerminalResponse = false)
{
	FPendingScenario Result;
	Result.Request = MakeRequest(true, bEffectNegator, bTerminalResponse);
	const FWBMatchOperationResult Started =
		Result.Coordinator.InitializeMatch(Result.Request);
	const FWBMatchLegalAction* Summon = Started.bOk
		? FindSummon(Started.NextLegalActions)
		: nullptr;
	if (Summon == nullptr)
	{
		Result.Reason = Started.bOk ? TEXT("summon_missing") : Started.Reason;
		return Result;
	}
	Result.SummonActionId = Summon->ActionId;
	Result.Declaration = Result.Coordinator.SubmitActionId(
		Summon->PlayerId, Summon->ActionId);
	Result.bOk = Result.Declaration.bOk
		&& Result.Coordinator.GetState().HasPendingSummon();
	Result.Reason = Result.Declaration.bOk
		? FString(TEXT("pending_summon_missing"))
		: Result.Declaration.Reason;
	return Result;
}

FWBMatchOperationResult SubmitCurrentPass(WBMatchCoordinator& Coordinator)
{
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Pass = Legal.bOk ? FindPass(Legal.Actions) : nullptr;
	return Pass != nullptr
		? Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId)
		: FWBMatchOperationResult();
}

struct FNegateTheNegationResult
{
	bool bOk = false;
	WBMatchCoordinator Coordinator;
	TArray<FWBTraceEvent> Trace;
};

FNegateTheNegationResult RunNegateTheNegation()
{
	FNegateTheNegationResult Result;
	FPendingScenario Pending = OpenPending(true);
	if (!Pending.bOk)
	{
		return Result;
	}
	const FWBMatchLegalActionGenerationResult NegateLegal =
		Pending.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* SummonNegate = FindActivation(
		NegateLegal.Actions, TEXT("negate_pending_summon"));
	if (SummonNegate == nullptr)
	{
		return Result;
	}
	const FWBMatchOperationResult DeclaredNegation =
		Pending.Coordinator.SubmitActionId(
			SummonNegate->PlayerId, SummonNegate->ActionId);
	if (!DeclaredNegation.bOk)
	{
		return Result;
	}
	const FWBMatchLegalActionGenerationResult CounterLegal =
		Pending.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EffectNegate = FindActivation(
		CounterLegal.Actions, TEXT("negate_pending_effect"));
	if (EffectNegate == nullptr)
	{
		return Result;
	}
	const FWBMatchOperationResult Countered =
		Pending.Coordinator.SubmitActionId(
			EffectNegate->PlayerId, EffectNegate->ActionId);
	if (!Countered.bOk)
	{
		return Result;
	}
	Result.Trace = Pending.Declaration.TraceEvents;
	Result.Trace.Append(DeclaredNegation.TraceEvents);
	Result.Trace.Append(Countered.TraceEvents);
	Result.Coordinator = MoveTemp(Pending.Coordinator);
	Result.bOk = true;
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationProductionSmokeTest,
	"Wandbound.SummonNegation.ProductionSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationProductionSmokeTest::RunTest(const FString& Parameters)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/SummonNegationFixture/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/SummonNegationFixture/match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionSummonNegationSmokeResult Result =
		WBProductionSummonNegationSmoke::Run(Request);
	TestTrue(*FString::Printf(TEXT("Production smoke succeeds: %s"), *Result.Reason), Result.bOk);
	TestEqual(TEXT("All three production paths verified"), Result.ScenariosVerified, 3);
	TestEqual(TEXT("Eight accepted records replayed"), Result.RecordsVerified, 8);
	TestFalse(TEXT("Final state digest exists"), Result.FinalStateDigest.IsEmpty());
	TestFalse(TEXT("Final trace digest exists"), Result.FinalTraceDigest.IsEmpty());
	TestTrue(TEXT("Packaged flag recognized"),
		WBProductionSummonNegationSmoke::IsRequested(
			TEXT("-WandboundProductionSummonNegationSmoke")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationFixtureTest,
	"Wandbound.SummonNegation.Fixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationFixtureTest::RunTest(const FString& Parameters)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Data/Replay/SummonNegationFixture/root_manifest.json")));
	FString LoadDetail = Loaded.Reason;
	for (const FWBProductionCardDBDiagnostic& Diagnostic : Loaded.Diagnostics)
	{
		LoadDetail += FString::Printf(
			TEXT(" | %s:%s:%s"),
			*Diagnostic.Code,
			*Diagnostic.DefinitionId,
			*Diagnostic.FieldPath);
	}
	TestTrue(*FString::Printf(TEXT("Fixture CardDB loads: %s"), *LoadDetail), Loaded.bOk);
	TestTrue(TEXT("Fixture snapshot exists"), Loaded.Snapshot.IsValid());
	if (Loaded.Snapshot.IsValid())
	{
		TestEqual(
			TEXT("Fixture digest is locked"),
			Loaded.Snapshot->ContentDigest,
			FString(TEXT("1749cdaa6ffc916c32ae48c97592e88249567e8a058876f70aa73b14855c4ab2")));
		const FWBProductionCardRecord* Hero =
			Loaded.Snapshot->FindHero(TEXT("sn_fixture_hero_b"));
		TestNotNull(TEXT("Summon-negation Hero loaded"), Hero);
		if (Hero != nullptr)
		{
			TestTrue(TEXT("Typed summon-negation payload imported"),
				Hero->CoreDefinition.ActivatedEffects.ContainsByPredicate(
					[](const FWBCardEffectDefinition& Effect)
					{
						return Effect.Payloads.ContainsByPredicate(
							[](const FWBGenericEffectPayload& Payload)
							{
								return Payload.Operation
									== EWBGenericEffectOp::NegatePendingSummon;
							});
					}));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationPendingStateTest,
	"Wandbound.SummonNegation.PendingState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationPendingStateTest::RunTest(const FString& Parameters)
{
	const FPendingScenario First = OpenPending();
	TestTrue(TEXT("Declaration succeeds and remains pending"), First.bOk);
	if (!First.bOk) return false;
	const FWBGameStateData& State = First.Coordinator.GetState();
	const FWBPendingSummonState& Pending = State.PendingSummon;
	TestTrue(TEXT("Pending active"), Pending.bActive);
	TestTrue(TEXT("Pending accepts negation"), Pending.bAcceptingNegation);
	TestFalse(TEXT("Pending initially unnegated"), Pending.bNegated);
	TestFalse(TEXT("Pending ID exists"), Pending.PendingSummonId.IsEmpty());
	TestEqual(TEXT("Summoning player"), Pending.SummoningPlayerId, 0);
	TestEqual(TEXT("Owner player"), Pending.OwnerPlayerId, 0);
	TestEqual(TEXT("Controller player"), Pending.ControllerPlayerId, 0);
	TestEqual(TEXT("Exact selected copy"), Pending.CardInstanceId, FString(TEXT("summon_copy_a2")));
	TestEqual(TEXT("Card definition snapshot"), Pending.CardId, FString(TEXT("summon_student")));
	TestEqual(TEXT("Source remains Hand"), static_cast<int32>(Pending.SourceZone), static_cast<int32>(EWBCardZone::Hand));
	TestTrue(TEXT("Destination valid"), Pending.DestinationTile.X >= 0 && Pending.DestinationTile.Y >= 0);
	TestEqual(TEXT("Declared normal origin"), static_cast<int32>(Pending.Origin), static_cast<int32>(EWBSummonOrigin::DeclaredNormalCharacter));
	TestEqual(TEXT("Declared provenance"), static_cast<int32>(Pending.DeclarationProvenance), static_cast<int32>(EWBDeclarationProvenance::PlayerDeclared));
	TestEqual(TEXT("Normal condition policy"), static_cast<int32>(Pending.ConditionPolicy), static_cast<int32>(EWBCharacterSummonConditionPolicy::Normal));
	TestEqual(TEXT("Source action retained"), Pending.SourceActionId, First.SummonActionId);
	TestEqual(TEXT("Resume priority retained"), Pending.ResumePriorityPlayerId, 0);
	TestEqual(TEXT("Resume phase retained"), static_cast<int32>(Pending.ResumeGamePhase), static_cast<int32>(EWBGamePhase::NormalTurn));
	TestTrue(TEXT("Event identity valid"), Pending.EventIdentity.IsValid());
	TestEqual(TEXT("Event kind Summon"), static_cast<int32>(Pending.EventIdentity.Kind), static_cast<int32>(EWBEventKind::Summon));
	TestEqual(TEXT("Event action declared"), static_cast<int32>(Pending.EventIdentity.ActionDeclaration), static_cast<int32>(EWBDeclarationProvenance::PlayerDeclared));
	TestTrue(TEXT("PreSummon window open"), State.HasOpenReactionWindow());
	TestEqual(TEXT("PreSummon kind"), static_cast<int32>(State.ReactionWindow.Kind), static_cast<int32>(EWBReactionWindowKind::PreSummon));
	TestEqual(TEXT("Opponent has first priority"), State.PriorityPlayer, 1);
	TestEqual(TEXT("Coordinator response phase"), static_cast<int32>(First.Coordinator.GetMatchPhase()), static_cast<int32>(EWBMatchLoopPhase::Response));
	const FWBPlayerCardZoneState* Zones = GetZones(State, 0);
	TestNotNull(TEXT("Player zones exist"), Zones);
	if (Zones != nullptr)
	{
		TestTrue(TEXT("Selected copy remains in Hand"), ZoneHasInstance(Zones->Hand, TEXT("summon_copy_a2")));
		TestTrue(TEXT("Duplicate copy remains in Hand"), ZoneHasInstance(Zones->Hand, TEXT("summon_copy_a1")));
	}
	TestFalse(TEXT("Unit not yet created"), HasUnitCard(State, TEXT("summon_student")));
	TestTrue(TEXT("Declaration traced"), HasTrace(First.Declaration.TraceEvents, FName(TEXT("summon_declared"))));
	TestTrue(TEXT("Pending traced"), HasTrace(First.Declaration.TraceEvents, FName(TEXT("summon_pending"))));
	TestTrue(TEXT("PreSummon open traced"), HasTrace(First.Declaration.TraceEvents, FName(TEXT("reaction_window_opened"))));
	TestFalse(TEXT("Successful summon not traced"), HasTrace(First.Declaration.TraceEvents, FName(TEXT("summon_unit"))));

	const FPendingScenario Second = OpenPending();
	TestTrue(TEXT("Repeated declaration succeeds"), Second.bOk);
	TestEqual(TEXT("Pending ID deterministic"), Second.Coordinator.GetState().PendingSummon.PendingSummonId, Pending.PendingSummonId);
	TestEqual(TEXT("Pending state digest deterministic"), Second.Coordinator.GetCurrentStateDigest(), First.Coordinator.GetCurrentStateDigest());
	TestEqual(TEXT("Pending trace digest deterministic"), Second.Coordinator.GetCurrentTraceDigest(), First.Coordinator.GetCurrentTraceDigest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationSuccessfulPathTest,
	"Wandbound.SummonNegation.SuccessfulPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationSuccessfulPathTest::RunTest(const FString& Parameters)
{
	const FWBMatchInitializationRequest Request = MakeRequest(false);
	WBMatchCoordinator First;
	const FWBMatchOperationResult Started = First.InitializeMatch(Request);
	const FWBMatchLegalAction* Summon = Started.bOk
		? FindSummon(Started.NextLegalActions)
		: nullptr;
	TestNotNull(TEXT("Exact summon available"), Summon);
	if (Summon == nullptr) return false;
	const FWBTile Target = Summon->SummonRequest.TargetTile;
	const FWBMatchOperationResult Result = First.SubmitActionId(
		Summon->PlayerId, Summon->ActionId);
	TestTrue(TEXT("Summon submission succeeds"), Result.bOk);
	TestFalse(TEXT("Pending summon cleared"), First.GetState().HasPendingSummon());
	TestFalse(TEXT("Response window closed"), First.GetState().HasOpenReactionWindow());
	TestEqual(TEXT("Normal phase restored"), static_cast<int32>(First.GetMatchPhase()), static_cast<int32>(EWBMatchLoopPhase::Action));
	TestEqual(TEXT("Priority restored"), First.GetState().PriorityPlayer, 0);
	const FWBPlayerCardZoneState* Zones = GetZones(First.GetState(), 0);
	TestNotNull(TEXT("Player zones remain valid"), Zones);
	if (Zones != nullptr)
	{
		TestFalse(TEXT("Selected exact copy consumed"), ZoneHasInstance(Zones->Hand, TEXT("summon_copy_a2")));
		TestTrue(TEXT("Duplicate exact copy retained"), ZoneHasInstance(Zones->Hand, TEXT("summon_copy_a1")));
	}
	const FWBUnitState* Unit = First.GetState().Units.FindByPredicate(
		[](const FWBUnitState& Candidate)
		{
			return Candidate.IsUnitOnBoard()
				&& Candidate.CardId == TEXT("summon_student");
		});
	TestNotNull(TEXT("Character created"), Unit);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Destination X preserved"), Unit->X, Target.X);
		TestEqual(TEXT("Destination Y preserved"), Unit->Y, Target.Y);
	}
	TestTrue(TEXT("Declaration trace exists"), HasTrace(Result.TraceEvents, FName(TEXT("summon_declared"))));
	TestTrue(TEXT("PreSummon opens even with no React"), HasTrace(Result.TraceEvents, FName(TEXT("reaction_window_opened"))));
	TestEqual(TEXT("Two automatic passes close empty PreSummon"), CountTrace(Result.TraceEvents, FName(TEXT("reaction_auto_passed"))), 2);
	TestTrue(TEXT("PreSummon close traced"), HasTrace(Result.TraceEvents, FName(TEXT("reaction_window_closed"))));
	TestTrue(TEXT("Successful summon traced once"), HasTrace(Result.TraceEvents, FName(TEXT("summon_unit"))));
	TestFalse(TEXT("No summon cancellation"), HasTrace(Result.TraceEvents, FName(TEXT("pending_summon_cancelled"))));
	TestTrue(TEXT("Summon occurs after PreSummon close"), TraceIndex(Result.TraceEvents, FName(TEXT("summon_unit"))) > TraceIndex(Result.TraceEvents, FName(TEXT("reaction_window_closed"))));

	WBMatchCoordinator Second;
	const FWBMatchOperationResult StartedAgain = Second.InitializeMatch(Request);
	const FWBMatchLegalAction* SummonAgain = StartedAgain.bOk
		? FindSummon(StartedAgain.NextLegalActions)
		: nullptr;
	TestNotNull(TEXT("Repeated exact summon available"), SummonAgain);
	if (SummonAgain != nullptr)
	{
		TestTrue(TEXT("Repeated submit succeeds"), Second.SubmitActionId(SummonAgain->PlayerId, SummonAgain->ActionId).bOk);
		TestEqual(TEXT("Successful state digest deterministic"), Second.GetCurrentStateDigest(), First.GetCurrentStateDigest());
		TestEqual(TEXT("Successful trace digest deterministic"), Second.GetCurrentTraceDigest(), First.GetCurrentTraceDigest());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationResolvedTest,
	"Wandbound.SummonNegation.ResolvedNegation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationResolvedTest::RunTest(const FString& Parameters)
{
	FPendingScenario Scenario = OpenPending(true);
	TestTrue(TEXT("Pending summon opened"), Scenario.bOk);
	if (!Scenario.bOk) return false;
	const TArray<FString> HandBefore = HandInstances(Scenario.Coordinator.GetState(), 0);
	const FWBMatchLegalActionGenerationResult Legal = Scenario.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Negate = FindActivation(
		Legal.Actions, TEXT("negate_pending_summon"));
	TestNotNull(TEXT("Summon-negation React legal"), Negate);
	TestNotNull(TEXT("PassResponse also legal"), FindPass(Legal.Actions));
	if (Negate == nullptr) return false;
	TestEqual(TEXT("React owned by opponent"), Negate->PlayerId, 1);
	TestEqual(TEXT("Typed summon target injected"), Negate->ActivationCommand.EffectRequest.Payloads[0].PendingSummonId, Scenario.Coordinator.GetState().PendingSummon.PendingSummonId);
	const FWBMatchOperationResult DeclaredNegation =
		Scenario.Coordinator.SubmitActionId(Negate->PlayerId, Negate->ActionId);
	TestTrue(TEXT("Negation activation accepted"), DeclaredNegation.bOk);
	TestTrue(TEXT("Summon still pending before React resolves"), Scenario.Coordinator.GetState().HasPendingSummon());
	TestFalse(TEXT("Activation does not immediately negate"), Scenario.Coordinator.GetState().PendingSummon.bNegated);
	TestEqual(TEXT("Nested PostEffect is active"), static_cast<int32>(Scenario.Coordinator.GetState().ReactionWindow.Kind), static_cast<int32>(EWBReactionWindowKind::PostEffect));
	TestEqual(TEXT("One pending effect frame"), Scenario.Coordinator.GetPendingEffectActivationStack().Num(), 1);
	const FWBMatchOperationResult Resolution = SubmitCurrentPass(Scenario.Coordinator);
	TestTrue(TEXT("Pass allows React resolution"), Resolution.bOk);
	TestFalse(TEXT("Pending summon clears after resolved negation"), Scenario.Coordinator.GetState().HasPendingSummon());
	TestFalse(TEXT("No response remains"), Scenario.Coordinator.GetState().HasOpenReactionWindow());
	TestFalse(TEXT("No unit created"), HasUnitCard(Scenario.Coordinator.GetState(), TEXT("summon_student")));
	TestTrue(TEXT("Hand order unchanged"), HandInstances(Scenario.Coordinator.GetState(), 0) == HandBefore);
	TestEqual(TEXT("Action phase restored"), static_cast<int32>(Scenario.Coordinator.GetMatchPhase()), static_cast<int32>(EWBMatchLoopPhase::Action));
	TestEqual(TEXT("Summoner priority restored"), Scenario.Coordinator.GetState().PriorityPlayer, 0);
	TestTrue(TEXT("Summon negation traced"), HasTrace(Resolution.TraceEvents, FName(TEXT("summon_negated"))));
	TestTrue(TEXT("Cancellation traced"), HasTrace(Resolution.TraceEvents, FName(TEXT("pending_summon_cancelled"))));
	TestFalse(TEXT("No successful summon trace"), HasTrace(Resolution.TraceEvents, FName(TEXT("summon_unit"))));
	TestFalse(TEXT("No destruction trace"), HasTrace(Resolution.TraceEvents, FName(TEXT("unit_destroyed"))));
	TestFalse(TEXT("No sacrifice trace"), HasTrace(Resolution.TraceEvents, FName(TEXT("unit_sacrificed"))));
	TestFalse(TEXT("No damage trace"), HasTrace(Resolution.TraceEvents, FName(TEXT("damage_effect"))));
	TestFalse(TEXT("No PostSummon window"), Resolution.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(TEXT("reaction_window_opened"))
			&& Event.ReactionWindowKind == FName(TEXT("post_summon"));
	}));
	TestEqual(TEXT("Two player decisions recorded"), Scenario.Coordinator.GetCommittedActionRecords().Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationCounteredTest,
	"Wandbound.SummonNegation.NegatingTheNegation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationCounteredTest::RunTest(const FString& Parameters)
{
	const FNegateTheNegationResult First = RunNegateTheNegation();
	TestTrue(TEXT("Nested counter scenario succeeds"), First.bOk);
	if (!First.bOk) return false;
	TestTrue(TEXT("Character ultimately summons"), HasUnitCard(First.Coordinator.GetState(), TEXT("summon_student")));
	TestFalse(TEXT("Pending summon clears"), First.Coordinator.GetState().HasPendingSummon());
	TestFalse(TEXT("No response remains"), First.Coordinator.GetState().HasOpenReactionWindow());
	TestTrue(TEXT("Effect frame was negated"), HasTrace(First.Trace, FName(TEXT("pending_effect_activation_negated"))));
	TestTrue(TEXT("Negated frame skipped"), HasTrace(First.Trace, FName(TEXT("pending_effect_activation_skipped"))));
	TestFalse(TEXT("Summon was never marked negated"), HasTrace(First.Trace, FName(TEXT("summon_negated"))));
	TestFalse(TEXT("Summon was not canceled"), HasTrace(First.Trace, FName(TEXT("pending_summon_cancelled"))));
	TestTrue(TEXT("Successful summon emitted"), HasTrace(First.Trace, FName(TEXT("summon_unit"))));
	TestEqual(TEXT("Three declared decisions recorded"), First.Coordinator.GetCommittedActionRecords().Num(), 3);
	TestEqual(TEXT("Replay schema remains one"), WBProductionMatchReplay::SchemaVersion, 1);

	const FNegateTheNegationResult Second = RunNegateTheNegation();
	TestTrue(TEXT("Repeated nested scenario succeeds"), Second.bOk);
	if (Second.bOk)
	{
		TestEqual(TEXT("Nested state digest deterministic"), Second.Coordinator.GetCurrentStateDigest(), First.Coordinator.GetCurrentStateDigest());
		TestEqual(TEXT("Nested trace digest deterministic"), Second.Coordinator.GetCurrentTraceDigest(), First.Coordinator.GetCurrentTraceDigest());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationRevalidationTest,
	"Wandbound.SummonNegation.Revalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationRevalidationTest::RunTest(const FString& Parameters)
{
	FPendingScenario Moved = OpenPending();
	TestTrue(TEXT("Moved-card scenario opens"), Moved.bOk);
	if (!Moved.bOk) return false;
	const FWBCardLifecycleResult MoveResult = WBCardLifecycle::MoveHandCardToDiscard(
		Moved.Coordinator.GetMutableStateForTest(), 0, TEXT("summon_copy_a2"));
	TestTrue(TEXT("Exact pending copy moved by test response mutation"), MoveResult.bOk);
	const FWBMatchOperationResult MovedClose = SubmitCurrentPass(Moved.Coordinator);
	TestTrue(TEXT("Moved-card window closes"), MovedClose.bOk);
	TestFalse(TEXT("Moved-card summon creates no unit"), HasUnitCard(Moved.Coordinator.GetState(), TEXT("summon_student")));
	const FWBPlayerCardZoneState* MovedZones = GetZones(Moved.Coordinator.GetState(), 0);
	TestNotNull(TEXT("Moved-card zones exist"), MovedZones);
	if (MovedZones != nullptr)
	{
		TestTrue(TEXT("Exact copy remains in Discard"), ZoneHasInstance(MovedZones->Discard, TEXT("summon_copy_a2")));
		TestTrue(TEXT("Duplicate cannot substitute"), ZoneHasInstance(MovedZones->Hand, TEXT("summon_copy_a1")));
	}
	TestTrue(TEXT("Moved-card revalidation failure traced"), HasTrace(MovedClose.TraceEvents, FName(TEXT("pending_summon_revalidation_failed"))));
	TestFalse(TEXT("Moved-card pending clears"), Moved.Coordinator.GetState().HasPendingSummon());

	FPendingScenario Occupied = OpenPending();
	TestTrue(TEXT("Occupied-tile scenario opens"), Occupied.bOk);
	if (Occupied.bOk)
	{
		FWBUnitState Blocker;
		Blocker.UnitId = 900;
		Blocker.OwnerId = 1;
		Blocker.ControllerPlayerId = 1;
		Blocker.CardId = TEXT("summon_negation_hero_b");
		Blocker.X = Occupied.Coordinator.GetState().PendingSummon.DestinationTile.X;
		Blocker.Y = Occupied.Coordinator.GetState().PendingSummon.DestinationTile.Y;
		Blocker.HP = 1;
		Blocker.MaxHP = 1;
		Occupied.Coordinator.GetMutableStateForTest().Units.Add(Blocker);
		const FWBMatchOperationResult OccupiedClose = SubmitCurrentPass(Occupied.Coordinator);
		TestTrue(TEXT("Occupied-tile window closes"), OccupiedClose.bOk);
		TestFalse(TEXT("Occupied tile prevents summon"), Occupied.Coordinator.GetState().Units.ContainsByPredicate([](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("summon_student");
		}));
		TestTrue(TEXT("Occupied failure traced"), HasTrace(OccupiedClose.TraceEvents, FName(TEXT("pending_summon_revalidation_failed"))));
		TestTrue(TEXT("Exact card retained after occupied failure"), ZoneHasInstance(GetZones(Occupied.Coordinator.GetState(), 0)->Hand, TEXT("summon_copy_a2")));
	}

	FPendingScenario UnitCap = OpenPending();
	TestTrue(TEXT("Unit-cap scenario opens"), UnitCap.bOk);
	if (UnitCap.bOk)
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FWBUnitState Extra;
			Extra.UnitId = 910 + Index;
			Extra.OwnerId = 0;
			Extra.ControllerPlayerId = 0;
			Extra.CardId = TEXT("summon_negation_hero_a");
			Extra.X = Index;
			Extra.Y = 6;
			Extra.HP = 1;
			Extra.MaxHP = 1;
			UnitCap.Coordinator.GetMutableStateForTest().Units.Add(Extra);
		}
		const FWBMatchOperationResult CapClose = SubmitCurrentPass(UnitCap.Coordinator);
		TestTrue(TEXT("Unit-cap window closes"), CapClose.bOk);
		TestFalse(TEXT("Unit cap prevents summon"), HasUnitCard(UnitCap.Coordinator.GetState(), TEXT("summon_student")));
		TestTrue(TEXT("Unit-cap failure traced"), HasTrace(CapClose.TraceEvents, FName(TEXT("pending_summon_revalidation_failed"))));
		TestTrue(TEXT("Exact card retained after cap failure"), ZoneHasInstance(GetZones(UnitCap.Coordinator.GetState(), 0)->Hand, TEXT("summon_copy_a2")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationTimingAndPrivacyTest,
	"Wandbound.SummonNegation.TimingAndPrivacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationTimingAndPrivacyTest::RunTest(const FString& Parameters)
{
	FPendingScenario Scenario = OpenPending();
	TestTrue(TEXT("Timing scenario opens"), Scenario.bOk);
	if (!Scenario.bOk) return false;
	const FWBMatchLegalActionGenerationResult PreSummonLegal =
		Scenario.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Negate = FindActivation(
		PreSummonLegal.Actions, TEXT("negate_pending_summon"));
	TestNotNull(TEXT("Negation legal in PreSummon"), Negate);
	TestNotNull(TEXT("Pass legal in PreSummon"), FindPass(PreSummonLegal.Actions));
	TestFalse(TEXT("No Move in PreSummon"), PreSummonLegal.Actions.ContainsByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Move;
	}));
	TestFalse(TEXT("No Attack in PreSummon"), PreSummonLegal.Actions.ContainsByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Attack;
	}));
	TestFalse(TEXT("No Summon in PreSummon"), PreSummonLegal.Actions.ContainsByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon;
	}));
	TestFalse(TEXT("No Equip in PreSummon"), PreSummonLegal.Actions.ContainsByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Equip;
	}));
	TestFalse(TEXT("No EndTurn in PreSummon"), PreSummonLegal.Actions.ContainsByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::EndTurn;
	}));
	if (Negate != nullptr)
	{
		FWBCardActivationCommand Stale = Negate->ActivationCommand;
		Stale.EffectRequest.Payloads[0].PendingSummonId = TEXT("stale_pending_summon");
		TestFalse(TEXT("Stale PendingSummonId rejected"), WBRules::CanApplyCardActivationCommand(
			Scenario.Coordinator.GetState(), Scenario.Coordinator.GetRepository(), Stale).bOk);
	}

	const EWBReactionWindowKind OtherKinds[] = {
		EWBReactionWindowKind::PreHit,
		EWBReactionWindowKind::PostHit,
		EWBReactionWindowKind::PostMove,
		EWBReactionWindowKind::PostSummon,
		EWBReactionWindowKind::PostEffect
	};
	for (const EWBReactionWindowKind Kind : OtherKinds)
	{
		Scenario.Coordinator.GetMutableStateForTest().ReactionWindow.Kind = Kind;
		const FWBMatchLegalActionGenerationResult Legal =
			Scenario.Coordinator.EnumerateLegalActions();
		TestNull(*FString::Printf(TEXT("Summon negation unavailable in kind %d"), static_cast<int32>(Kind)),
			FindActivation(Legal.Actions, TEXT("negate_pending_summon")));
	}
	Scenario.Coordinator.GetMutableStateForTest().ReactionWindow.Kind =
		EWBReactionWindowKind::PreSummon;

	const FWBMatchObservation OpponentView = Scenario.Coordinator.BuildObservation(1);
	TestEqual(TEXT("Opponent sees Response phase"), static_cast<int32>(OpponentView.MatchPhase), static_cast<int32>(EWBMatchLoopPhase::Response));
	TestEqual(TEXT("Public turn identifies PreSummon"), OpponentView.PublicTurn.ReactionWindowKind, FName(TEXT("pre_summon")));
	TestTrue(TEXT("Opponent sees its authorized React"), FindActivation(OpponentView.LegalActions, TEXT("negate_pending_summon")) != nullptr);
	TestEqual(TEXT("Opponent own-hand owner remains opponent"), OpponentView.CardZones.OwnHand.OwnerPlayerId, 1);
	TestFalse(TEXT("Opponent own-hand excludes selected summon instance"), OpponentView.CardZones.OwnHand.Cards.ContainsByPredicate([](const FWBObservedCardRef& Card)
	{
		return Card.InstanceId == TEXT("summon_copy_a2");
	}));
	const FWBObservedZoneSummary* PublicSummonerHand = OpponentView.CardZones.PublicSummary.PlayerHands.FindByPredicate([](const FWBObservedZoneSummary& Hand)
	{
		return Hand.OwnerPlayerId == 0;
	});
	TestNotNull(TEXT("Public summoner Hand summary exists"), PublicSummonerHand);
	if (PublicSummonerHand != nullptr)
	{
		TestEqual(TEXT("Public Hand is count-only"), static_cast<int32>(PublicSummonerHand->Visibility), static_cast<int32>(EWBZoneObservationVisibility::CountOnly));
		TestTrue(TEXT("Public Hand exposes no instances"), PublicSummonerHand->Cards.IsEmpty());
	}
	const FWBMatchObservation SummonerView = Scenario.Coordinator.BuildObservation(0);
	TestTrue(TEXT("Non-priority summoner gets no legal actions"), SummonerView.LegalActions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBSummonNegationTerminalCleanupTest,
	"Wandbound.SummonNegation.TerminalCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSummonNegationTerminalCleanupTest::RunTest(const FString& Parameters)
{
	FPendingScenario Scenario = OpenPending(false, true);
	TestTrue(TEXT("Terminal scenario opens"), Scenario.bOk);
	if (!Scenario.bOk) return false;
	const FWBMatchLegalActionGenerationResult Legal = Scenario.Coordinator.EnumerateLegalActions();
	const int32 HeroId = Scenario.Coordinator.GetState().GetPlayerById(0)->HeroUnitId;
	const FWBMatchLegalAction* TerminalReact = Legal.Actions.FindByPredicate(
		[HeroId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceEffectId == TEXT("terminal_response")
				&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId == HeroId;
		});
	TestNotNull(TEXT("Terminal response is legal"), TerminalReact);
	if (TerminalReact == nullptr) return false;
	const FWBMatchOperationResult Result = Scenario.Coordinator.SubmitActionId(
		TerminalReact->PlayerId, TerminalReact->ActionId);
	TestTrue(TEXT("Terminal response accepted"), Result.bOk);
	TestTrue(TEXT("Match becomes terminal"), Scenario.Coordinator.GetState().bGameOver);
	TestFalse(TEXT("Pending summon cleared on terminal commit"), Scenario.Coordinator.GetState().HasPendingSummon());
	TestFalse(TEXT("Reaction state cleared on terminal commit"), Scenario.Coordinator.GetState().HasOpenReactionWindow());
	TestFalse(TEXT("Pending unit never created"), HasUnitCard(Scenario.Coordinator.GetState(), TEXT("summon_student")));
	TestTrue(TEXT("Terminal pending cleanup traced"), HasTrace(Result.TraceEvents, FName(TEXT("pending_summon_cleared_terminal"))));
	TestFalse(TEXT("Terminal path has no successful summon"), HasTrace(Result.TraceEvents, FName(TEXT("summon_unit"))));
	TestTrue(TEXT("Terminal legal actions empty"), Scenario.Coordinator.EnumerateLegalActions().Actions.IsEmpty());
	return true;
}

#endif
