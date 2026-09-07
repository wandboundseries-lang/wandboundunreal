#include "Misc/AutomationTest.h"

#include "Misc/Paths.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationExpansion.h"
#include "WBCardActivationSourceGate.h"
#include "WBCardZoneMutation.h"
#include "WBCardZoneObservation.h"
#include "WBEffectRunner.h"
#include "WBMatchCoordinator.h"
#include "WBProductionActivationDataProvider.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionDiscardActivationSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionRuntimeBootstrap.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 PlayerA = 0;
constexpr int32 PlayerB = 1;
constexpr int32 HeroA = 10;
constexpr int32 HeroB = 20;

FString FixturePath(const FString& FileName)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/DiscardActivationFixture"),
		FileName);
}

FWBProductionCardDatabaseLoadResult LoadFixture()
{
	return WBProductionCardDatabase::LoadManifestSuite(
		FixturePath(TEXT("root_manifest.json")));
}

FWBPlayerStateData MakePlayer(const int32 PlayerId, const int32 HeroUnitId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = HeroUnitId;
	Player.RemainingMP = 3;
	Player.LastMPRoll = 3;
	return Player;
}

FWBUnitState MakeHero(
	const int32 UnitId,
	const int32 OwnerPlayerId,
	const FString& CardId,
	const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.SetOwnerAndControllerForRules(OwnerPlayerId, OwnerPlayerId);
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 10;
	Unit.MaxHP = 12;
	Unit.ATK = 2;
	Unit.AR = 2;
	Unit.RLTotal = 3;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBZoneCardEntry MakeZoneCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerPlayerId,
	const EWBCardZone Zone,
	const int32 ZoneIndex)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OwnerPlayerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	return Entry;
}

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = PlayerA;
	State.PriorityPlayer = PlayerA;
	State.TurnNumber = 4;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players = {
		MakePlayer(PlayerA, HeroA),
		MakePlayer(PlayerB, HeroB)
	};
	State.AddUnitForTest(MakeHero(
		HeroA, PlayerA, TEXT("discard_fixture_hero_a"), FWBTile(4, 7)));
	State.AddUnitForTest(MakeHero(
		HeroB, PlayerB, TEXT("discard_fixture_hero_b"), FWBTile(4, 1)));

	FWBPlayerCardZoneState ZonesA;
	ZonesA.PlayerId = PlayerA;
	ZonesA.Discard = {
		MakeZoneCard(
			TEXT("discard_echo_a"), TEXT("discard_fixture_echo"),
			PlayerA, EWBCardZone::Discard, 0),
		MakeZoneCard(
			TEXT("discard_echo_b"), TEXT("discard_fixture_echo"),
			PlayerA, EWBCardZone::Discard, 1),
		MakeZoneCard(
			TEXT("discard_plain"), TEXT("discard_fixture_filler"),
			PlayerA, EWBCardZone::Discard, 2)
	};
	FWBPlayerCardZoneState ZonesB;
	ZonesB.PlayerId = PlayerB;
	ZonesB.Discard = {
		MakeZoneCard(
			TEXT("opponent_private_discard"), TEXT("discard_fixture_echo"),
			PlayerB, EWBCardZone::Discard, 0)
	};
	State.GetMutableCardZoneStateForTest().PlayerZones = { ZonesA, ZonesB };
	return State;
}

const FWBCardDefinition* FindDefinition(
	const FWBCardDefinitionRepository& Repository,
	const FString& CardId)
{
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, CardId);
	return Lookup.bFound ? Repository.Definitions.FindByPredicate(
		[&CardId](const FWBCardDefinition& Definition)
		{
			return Definition.CardId == CardId;
		}) : nullptr;
}

const FWBCardEffectDefinition* FindEffect(
	const FWBCardDefinition& Definition,
	const FString& EffectId)
{
	return Definition.ActivatedEffects.FindByPredicate(
		[&EffectId](const FWBCardEffectDefinition& Effect)
		{
			return Effect.EffectId == EffectId;
		});
}

FWBEffectTargetRef MakeHeroTarget()
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = HeroB;
	Target.TargetDeclaration = EWBDeclarationProvenance::PlayerDeclared;
	return Target;
}

FWBCardActivationSourceGateContext MakeDiscardContext(
	const FString& InstanceId,
	const FString& CardId = TEXT("discard_fixture_echo"))
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = PlayerA;
	Context.SourceUnitId = INDEX_NONE;
	Context.SourceCardId = CardId;
	Context.SourceCardInstanceId = InstanceId;
	Context.SourceZone = EWBCardActivationSourceZone::Discard;
	Context.ActivationUsageKey =
		WBCardActivationSourceGate::BuildDefaultUsageKeyForSource(
			PlayerA,
			INDEX_NONE,
			CardId,
			TEXT("discard_fixture_damage"),
			EWBCardActivationSourceZone::Discard,
			InstanceId);
	Context.bCostsSatisfiedExternally = true;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationExpansionResult ExpandDiscard(
	const FWBCardDefinition& Definition,
	const FString& InstanceId,
	const FString& EffectId = TEXT("discard_fixture_damage"))
{
	FWBCardActivationExpansionRequest Request;
	Request.PlayerId = PlayerA;
	Request.SourceUnitId = INDEX_NONE;
	Request.CardDefinition = Definition;
	Request.EffectId = EffectId;
	Request.Target = MakeHeroTarget();
	Request.SourceGateContext = MakeDiscardContext(InstanceId);
	return WBCardActivationExpansion::BuildActivationCommand(Request);
}

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

FWBSetupMarkerPlacement MakeMarker(
	const int32 OwnerPlayerId,
	const EWBMarkerType Type,
	const FWBTile& Tile,
	const int32 PlacementOrder)
{
	FWBSetupMarkerPlacement Marker;
	Marker.PlayerId = OwnerPlayerId;
	Marker.Type = Type;
	Marker.Tile = Tile;
	Marker.DefinitionId = Type == EWBMarkerType::Trap
		? TEXT("discard_fixture_trap")
		: TEXT("discard_fixture_npc");
	Marker.PlacementOrder = PlacementOrder;
	return Marker;
}

FWBMatchPlayerSetup MakeMatchPlayer(const int32 PlayerId)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(TEXT("discard_p%d_hero"), PlayerId);
	Setup.HeroCardId = PlayerId == PlayerA
		? TEXT("discard_fixture_hero_a")
		: TEXT("discard_fixture_hero_b");
	Setup.OrderedDeck.Add(MakeCard(
		Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
	if (PlayerId == PlayerA)
	{
		Setup.OrderedDeck.Add(MakeCard(
			TEXT("discard_match_echo_a"), TEXT("discard_fixture_echo"), PlayerId));
		Setup.OrderedDeck.Add(MakeCard(
			TEXT("discard_match_echo_b"), TEXT("discard_fixture_echo"), PlayerId));
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Setup.OrderedDeck.Add(MakeCard(
			FString::Printf(TEXT("discard_p%d_filler_%d"), PlayerId, Index),
			TEXT("discard_fixture_filler"),
			PlayerId));
	}
	return Setup;
}

FWBMatchInitializationRequest MakeMatchRequest(
	const FWBCardDefinitionRepository& Repository,
	const bool bIncludeOpponentNegate)
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 90210;
	Request.FirstPlayerId = PlayerA;
	Request.Repository = Repository;
	if (!bIncludeOpponentNegate)
	{
		FWBCardDefinition* HeroDefinition = Request.Repository.Definitions.FindByPredicate(
			[](const FWBCardDefinition& Definition)
			{
				return Definition.CardId == TEXT("discard_fixture_hero_b");
			});
		if (HeroDefinition != nullptr)
		{
			HeroDefinition->ActivatedEffects.Reset();
		}
	}
	Request.Players = { MakeMatchPlayer(PlayerA), MakeMatchPlayer(PlayerB) };
	Request.MarkerPlacements = {
		MakeMarker(PlayerA, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeMarker(PlayerA, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeMarker(PlayerA, EWBMarkerType::NPC, FWBTile(0, 7), 2),
		MakeMarker(PlayerA, EWBMarkerType::NPC, FWBTile(1, 7), 3),
		MakeMarker(PlayerB, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeMarker(PlayerB, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeMarker(PlayerB, EWBMarkerType::NPC, FWBTile(0, 1), 6),
		MakeMarker(PlayerB, EWBMarkerType::NPC, FWBTile(1, 1), 7)
	};
	return Request;
}

const FWBMatchLegalAction* FindDiscard(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& InstanceId)
{
	return Actions.FindByPredicate(
		[&InstanceId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Discard
				&& Action.DiscardCardInstanceId == InstanceId;
		});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId,
	const FString& InstanceId = FString(),
	const int32 TargetUnitId = INDEX_NONE)
{
	return Actions.FindByPredicate(
		[&EffectId, &InstanceId, TargetUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceEffectId == EffectId
				&& (InstanceId.IsEmpty()
					|| Action.ActivationCommand.Source.SourceCardInstanceId == InstanceId)
				&& (TargetUnitId == INDEX_NONE
					|| Action.ActivationCommand.EffectRequest.Target.TargetUnitId
						== TargetUnitId);
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

bool SubmitDiscard(WBMatchCoordinator& Coordinator, const FString& InstanceId)
{
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Action = Legal.bOk
		? FindDiscard(Legal.Actions, InstanceId)
		: nullptr;
	return Action != nullptr
		&& Coordinator.SubmitActionId(Action->PlayerId, Action->ActionId).bOk;
}

bool DrainResponseByPassing(WBMatchCoordinator& Coordinator)
{
	for (int32 Step = 0; Step < 12; ++Step)
	{
		if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Response)
		{
			return true;
		}
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindPass(Legal.Actions)
			: nullptr;
		if (Pass == nullptr
			|| !Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId).bOk)
		{
			return false;
		}
	}
	return false;
}

bool ZoneContains(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const EWBCardZone Zone,
	const FString& InstanceId,
	int32* OutIndex = nullptr)
{
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), PlayerId);
	if (Zones == nullptr)
	{
		return false;
	}
	const TArray<FWBZoneCardEntry>* Entries = Zone == EWBCardZone::Discard
		? &Zones->Discard
		: (Zone == EWBCardZone::Hand ? &Zones->Hand : &Zones->Deck);
	for (const FWBZoneCardEntry& Entry : *Entries)
	{
		if (Entry.Card.InstanceId == InstanceId)
		{
			if (OutIndex != nullptr)
			{
				*OutIndex = Entry.ZoneIndex;
			}
			return true;
		}
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDiscardActivationParserAndGateTest,
	"Wandbound.DiscardActivationSource.ParserAndSourceGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDiscardActivationParserAndGateTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded = LoadFixture();
	TestTrue(TEXT("Synthetic CardDB fixture loads"), Loaded.bOk);
	TestTrue(TEXT("Synthetic CardDB snapshot exists"), Loaded.Snapshot.IsValid());
	if (!Loaded.bOk || !Loaded.Snapshot.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("Fixture definition count"), Loaded.Snapshot->Records.Num(), 6);
	TestEqual(TEXT("Fixture digest length"), Loaded.Snapshot->ContentDigest.Len(), 64);
	AddInfo(FString::Printf(
		TEXT("DISCARD_ACTIVATION_FIXTURE_DIGEST=%s"),
		*Loaded.Snapshot->ContentDigest));
	const FWBCardDefinition* Definition = FindDefinition(
		Loaded.Snapshot->CoreRepository, TEXT("discard_fixture_echo"));
	TestNotNull(TEXT("Discard definition parsed"), Definition);
	if (Definition == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Two explicit Discard effects parsed"),
		Definition->ActivatedEffects.Num(), 2);
	const FWBCardEffectDefinition* Heal = FindEffect(
		*Definition, TEXT("discard_fixture_damage"));
	const FWBCardEffectDefinition* React = FindEffect(
		*Definition, TEXT("discard_fixture_react"));
	TestNotNull(TEXT("Normal Discard effect parsed"), Heal);
	TestNotNull(TEXT("Response Discard effect parsed"), React);
	if (Heal == nullptr || React == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Normal source zone is Discard"),
		static_cast<int32>(Heal->SourceGate.RequiredZone),
		static_cast<int32>(EWBCardActivationSourceZone::Discard));
	TestEqual(TEXT("React source zone is Discard"),
		static_cast<int32>(React->SourceGate.RequiredZone),
		static_cast<int32>(EWBCardActivationSourceZone::Discard));
	TestEqual(TEXT("Normal timing preserved"),
		static_cast<int32>(Heal->SourceGate.Timing),
		static_cast<int32>(EWBCardActivationTimingRequirement::NormalTurnPriority));
	TestEqual(TEXT("Response timing preserved"),
		static_cast<int32>(React->SourceGate.Timing),
		static_cast<int32>(EWBCardActivationTimingRequirement::ResponseWindow));
	TestFalse(TEXT("Discard source requires no unit"),
		Heal->SourceGate.bRequiresSourceUnit);
	TestTrue(TEXT("Discard source still requires ownership"),
		Heal->SourceGate.bRequiresSourceUnitOwnership);
	TestTrue(TEXT("Default once-per-turn enabled"), Heal->SourceGate.bOncePerTurn);

	const FWBGameStateData State = MakeState();
	const FWBCardActivationSourceGateContext Valid =
		MakeDiscardContext(TEXT("discard_echo_a"));
	FWBCardActivationSourceGateResult GateResult =
		WBCardActivationSourceGate::Evaluate(State, Heal->SourceGate, Valid);
	TestTrue(TEXT("Exact owned Discard source passes"), GateResult.bOk);
	TestTrue(TEXT("Successful gate has empty reason"), GateResult.Reason.IsEmpty());

	FWBCardActivationSourceGateContext Missing = Valid;
	Missing.SourceCardInstanceId.Reset();
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		State, Heal->SourceGate, Missing);
	TestFalse(TEXT("Missing exact instance fails"), GateResult.bOk);
	TestEqual(TEXT("Missing exact instance reason"), GateResult.Reason,
		FString(TEXT("source_card_instance_id_missing")));

	FWBCardActivationSourceGateContext Unknown = Valid;
	Unknown.SourceCardInstanceId = TEXT("not_present");
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		State, Heal->SourceGate, Unknown);
	TestFalse(TEXT("Unknown exact instance fails"), GateResult.bOk);
	TestEqual(TEXT("Unknown exact instance reason"), GateResult.Reason,
		FString(TEXT("source_zone_card_not_found")));

	FWBCardActivationSourceGateContext WrongOwner = Valid;
	WrongOwner.PlayerId = PlayerB;
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		State, Heal->SourceGate, WrongOwner);
	TestFalse(TEXT("Wrong owner fails"), GateResult.bOk);
	TestEqual(TEXT("Wrong owner reason"), GateResult.Reason,
		FString(TEXT("source_zone_owner_mismatch")));

	FWBCardActivationSourceGateContext WrongCard = Valid;
	WrongCard.SourceCardId = TEXT("discard_fixture_filler");
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		State, Heal->SourceGate, WrongCard);
	TestFalse(TEXT("Wrong CardId fails"), GateResult.bOk);
	TestEqual(TEXT("Wrong CardId reason"), GateResult.Reason,
		FString(TEXT("source_zone_card_id_mismatch")));

	FWBCardActivationSourceGateContext WrongUnit = Valid;
	WrongUnit.SourceUnitId = HeroA;
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		State, Heal->SourceGate, WrongUnit);
	TestFalse(TEXT("Fabricated source unit fails"), GateResult.bOk);
	TestEqual(TEXT("Fabricated source unit reason"), GateResult.Reason,
		FString(TEXT("discard_source_unit_not_allowed")));

	FWBGameStateData HandState = State;
	FWBPlayerCardZoneState* HandZones = WBCardZoneState::FindMutablePlayerZones(
		HandState.GetMutableCardZoneStateForTest(), PlayerA);
	TestNotNull(TEXT("Mutable owner zones exist"), HandZones);
	if (HandZones != nullptr)
	{
		FWBZoneCardEntry Moved = HandZones->Discard[0];
		Moved.Zone = EWBCardZone::Hand;
		Moved.ZoneIndex = HandZones->Hand.Num();
		HandZones->Discard.RemoveAt(0);
		HandZones->Hand.Add(Moved);
	}
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		HandState, Heal->SourceGate, Valid);
	TestFalse(TEXT("Instance moved to Hand fails"), GateResult.bOk);
	TestEqual(TEXT("Moved to Hand reason"), GateResult.Reason,
		FString(TEXT("source_zone_card_not_in_discard")));

	FWBGameStateData DeckState = State;
	FWBPlayerCardZoneState* DeckZones = WBCardZoneState::FindMutablePlayerZones(
		DeckState.GetMutableCardZoneStateForTest(), PlayerA);
	if (DeckZones != nullptr)
	{
		FWBZoneCardEntry Moved = DeckZones->Discard[0];
		Moved.Zone = EWBCardZone::Deck;
		Moved.ZoneIndex = DeckZones->Deck.Num();
		DeckZones->Discard.RemoveAt(0);
		DeckZones->Deck.Add(Moved);
	}
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		DeckState, Heal->SourceGate, Valid);
	TestFalse(TEXT("Instance moved to Deck fails"), GateResult.bOk);
	TestEqual(TEXT("Moved to Deck reason"), GateResult.Reason,
		FString(TEXT("source_zone_card_not_in_discard")));

	FWBGameStateData DuplicateState = State;
	FWBPlayerCardZoneState* DuplicateZones = WBCardZoneState::FindMutablePlayerZones(
		DuplicateState.GetMutableCardZoneStateForTest(), PlayerB);
	if (DuplicateZones != nullptr)
	{
		DuplicateZones->Discard.Add(MakeZoneCard(
			TEXT("discard_echo_a"), TEXT("discard_fixture_echo"),
			PlayerB, EWBCardZone::Discard, 1));
	}
	GateResult = WBCardActivationSourceGate::EvaluateDiscardSourceParity(
		DuplicateState, Heal->SourceGate, Valid);
	TestFalse(TEXT("Duplicate exact identity fails closed"), GateResult.bOk);
	TestEqual(TEXT("Duplicate exact identity reason"), GateResult.Reason,
		FString(TEXT("source_zone_card_ambiguous")));

	FWBGameStateData TerminalState = State;
	TerminalState.bGameOver = true;
	GateResult = WBCardActivationSourceGate::Evaluate(
		TerminalState, Heal->SourceGate, Valid);
	TestFalse(TEXT("Terminal match blocks source"), GateResult.bOk);
	TestEqual(TEXT("Terminal source reason"), GateResult.Reason,
		FString(TEXT("game_over")));

	FWBCardActivationSourceGateDefinition DeckGate = Heal->SourceGate;
	DeckGate.RequiredZone = EWBCardActivationSourceZone::Deck;
	DeckGate.bRequiresFixtureZoneOwnership = false;
	FWBCardActivationSourceGateContext DeckContext = Valid;
	DeckContext.SourceZone = EWBCardActivationSourceZone::Deck;
	GateResult = WBCardActivationSourceGate::Evaluate(State, DeckGate, DeckContext);
	TestFalse(TEXT("Deck activation remains unsupported"), GateResult.bOk);
	TestEqual(TEXT("Deck activation fails closed"), GateResult.Reason,
		FString(TEXT("source_zone_mismatch")));

	const FString UsageA = WBCardActivationSourceGate::BuildDefaultUsageKeyForSource(
		PlayerA, INDEX_NONE, Definition->CardId, Heal->EffectId,
		EWBCardActivationSourceZone::Discard, TEXT("discard_echo_a"));
	const FString UsageB = WBCardActivationSourceGate::BuildDefaultUsageKeyForSource(
		PlayerA, INDEX_NONE, Definition->CardId, Heal->EffectId,
		EWBCardActivationSourceZone::Discard, TEXT("discard_echo_b"));
	TestNotEqual(TEXT("Duplicate copies have independent default usage keys"),
		UsageA, UsageB);
	TestTrue(TEXT("Usage A contains exact source"), UsageA.Contains(TEXT("discard_echo_a")));
	TestTrue(TEXT("Usage B contains exact source"), UsageB.Contains(TEXT("discard_echo_b")));
	TestEqual(TEXT("Mature Board key remains byte-compatible"),
		WBCardActivationSourceGate::BuildDefaultUsageKeyForSource(
			PlayerA, HeroA, TEXT("board_card"), TEXT("effect"),
			EWBCardActivationSourceZone::Board, FString()),
		WBCardActivationSourceGate::BuildDefaultUsageKey(
			PlayerA, HeroA, TEXT("board_card"), TEXT("effect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDiscardActivationExpansionAndResolutionTest,
	"Wandbound.DiscardActivationSource.ExpansionResolutionAndRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDiscardActivationExpansionAndResolutionTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded = LoadFixture();
	TestTrue(TEXT("Fixture loads for expansion"), Loaded.bOk);
	if (!Loaded.bOk || !Loaded.Snapshot.IsValid())
	{
		return false;
	}
	const FWBCardDefinition* Definition = FindDefinition(
		Loaded.Snapshot->CoreRepository, TEXT("discard_fixture_echo"));
	TestNotNull(TEXT("Expansion definition exists"), Definition);
	if (Definition == nullptr)
	{
		return false;
	}

	const FWBCardActivationExpansionResult Expanded =
		ExpandDiscard(*Definition, TEXT("discard_echo_a"));
	TestTrue(TEXT("Discard activation expands"), Expanded.bOk);
	TestEqual(TEXT("Command source zone is Discard"),
		static_cast<int32>(Expanded.Command.Source.SourceZone),
		static_cast<int32>(EWBCardZone::Discard));
	TestEqual(TEXT("Command preserves exact instance"),
		Expanded.Command.Source.SourceCardInstanceId,
		FString(TEXT("discard_echo_a")));
	TestEqual(TEXT("Command preserves source CardId"),
		Expanded.Command.Source.SourceCardId, Definition->CardId);
	TestEqual(TEXT("Command preserves effect id"),
		Expanded.Command.Source.SourceEffectId,
		FString(TEXT("discard_fixture_damage")));
	TestEqual(TEXT("No source unit is invented"),
		Expanded.Command.Source.SourceUnitId, INDEX_NONE);
	TestEqual(TEXT("No caster is invented"),
		Expanded.Command.Source.GetCasterUnitId(), INDEX_NONE);
	TestEqual(TEXT("Activation remains player declared"),
		static_cast<int32>(Expanded.Command.Source.ActivationProvenance),
		static_cast<int32>(EWBActivationProvenance::PlayerDeclared));
	TestEqual(TEXT("Effect source preserves exact instance"),
		Expanded.Command.EffectRequest.Source.SourceCardInstanceId,
		FString(TEXT("discard_echo_a")));
	TestEqual(TEXT("Effect source has no caster"),
		Expanded.Command.EffectRequest.Source.GetCasterUnitId(), INDEX_NONE);
	TestTrue(TEXT("Default usage is committed on success"),
		Expanded.Command.UsageCommit.bMarkUsageOnSuccess);
	TestTrue(TEXT("Default usage key is exact-instance aware"),
		Expanded.Command.UsageCommit.UsageKey.Contains(TEXT("discard_echo_a")));
	TestFalse(TEXT("Costless command has no payment commit"),
		Expanded.Command.CostPaymentCommit.bPayCostOnSuccess);

	FWBGameStateData State = MakeState();
	const int32 BeforeHP = State.GetUnitById(HeroB)->HP;
	const int32 BeforeDiscardCount = WBCardZoneState::CountCardsInZoneForPlayer(
		State.GetCardZoneState(), PlayerA, EWBCardZone::Discard);
	int32 BeforeIndex = INDEX_NONE;
	TestTrue(TEXT("Source begins in Discard"), ZoneContains(
		State, PlayerA, EWBCardZone::Discard, TEXT("discard_echo_a"), &BeforeIndex));
	const FWBActionQueryResult Query = WBRules::CanApplyCardActivationCommand(
		State, Loaded.Snapshot->CoreRepository, Expanded.Command);
	TestTrue(TEXT("Rules accept live exact Discard source"), Query.bOk);
	const FWBCardActivationCommandResult Applied =
		WBEffectRunner::ApplyCardActivationCommand(
			State, Expanded.Command, Loaded.Snapshot->CoreRepository);
	TestTrue(TEXT("Existing effect runner resolves Discard activation"), Applied.bOk);
	TestEqual(TEXT("Existing damage payload resolves"),
		State.GetUnitById(HeroB)->HP, BeforeHP - 1);
	TestEqual(TEXT("Discard count unchanged by activation"),
		WBCardZoneState::CountCardsInZoneForPlayer(
			State.GetCardZoneState(), PlayerA, EWBCardZone::Discard),
		BeforeDiscardCount);
	int32 AfterIndex = INDEX_NONE;
	TestTrue(TEXT("Exact source remains in Discard after resolution"), ZoneContains(
		State, PlayerA, EWBCardZone::Discard, TEXT("discard_echo_a"), &AfterIndex));
	TestEqual(TEXT("Source order remains unchanged"), AfterIndex, BeforeIndex);
	TestTrue(TEXT("Usage key is marked"), State.HasActivationUsageKeyThisTurn(
		PlayerA, Expanded.Command.UsageCommit.UsageKey));
	TestFalse(TEXT("Second use of same exact instance fails"),
		WBRules::CanApplyCardActivationCommand(
			State, Loaded.Snapshot->CoreRepository, Expanded.Command).bOk);

	const FWBCardActivationExpansionResult ExpandedB =
		ExpandDiscard(*Definition, TEXT("discard_echo_b"));
	TestTrue(TEXT("Duplicate copy expands independently"), ExpandedB.bOk);
	TestNotEqual(TEXT("Duplicate copy usage key differs"),
		Expanded.Command.UsageCommit.UsageKey,
		ExpandedB.Command.UsageCommit.UsageKey);
	TestTrue(TEXT("Duplicate copy remains legal"),
		WBRules::CanApplyCardActivationCommand(
			State, Loaded.Snapshot->CoreRepository, ExpandedB.Command).bOk);

	FWBGameStateData MovedState = MakeState();
	FWBCardZoneTransferRequest Move;
	Move.PlayerId = PlayerA;
	Move.SourceZone = EWBCardZone::Discard;
	Move.DestinationZone = EWBCardZone::Hand;
	Move.CardInstanceId = TEXT("discard_echo_a");
	Move.ExpectedCardId = TEXT("discard_fixture_echo");
	Move.DestinationPlacement = EWBOrderedZonePlacement::Append;
	const FWBCardZoneMutationResult MoveResult =
		WBCardZoneMutation::TransferExact(MovedState, Move);
	TestTrue(TEXT("Test moves exact source through zone authority"), MoveResult.bOk);
	const int32 MovedHP = MovedState.GetUnitById(HeroB)->HP;
	const FWBCardActivationCommandResult StaleResult =
		WBEffectRunner::ApplyCardActivationCommand(
			MovedState, Expanded.Command, Loaded.Snapshot->CoreRepository);
	TestFalse(TEXT("Resolution fails when exact source left Discard"), StaleResult.bOk);
	TestEqual(TEXT("Stale source liveness reason"), StaleResult.Reason,
		FString(TEXT("source_zone_card_not_in_discard")));
	TestEqual(TEXT("Stale resolution does not damage"),
		MovedState.GetUnitById(HeroB)->HP, MovedHP);
	TestTrue(TEXT("Stale source remains where explicit move placed it"), ZoneContains(
		MovedState, PlayerA, EWBCardZone::Hand, TEXT("discard_echo_a")));
	TestTrue(TEXT("Duplicate cannot substitute for stale source"), ZoneContains(
		MovedState, PlayerA, EWBCardZone::Discard, TEXT("discard_echo_b")));

	FWBCardEffectDefinition Costed = *FindEffect(
		*Definition, TEXT("discard_fixture_damage"));
	Costed.EffectId = TEXT("discard_fixture_costed");
	Costed.SourceGate.bOncePerTurn = false;
	Costed.SourceGate.CostGate.bRequiresExternalAffordability = true;
	Costed.SourceGate.CostGate.RequiredRR = 1;
	Costed.SourceGate.CostGate.CostKind = FName(TEXT("RR"));
	FWBCardActivationSourceGateContext CostContext =
		MakeDiscardContext(TEXT("discard_echo_a"));
	const FWBCardActivationSourceGateResult CostResult =
		WBCardActivationSourceGate::Evaluate(
			MakeState(), Costed.SourceGate, CostContext);
	TestFalse(TEXT("RR Discard effect without payer fails closed"), CostResult.bOk);
	TestEqual(TEXT("RR Discard effect reports missing affordability"),
		CostResult.Reason, FString(TEXT("cost_affordability_missing")));
	TestEqual(TEXT("No Hero payer invented"), CostContext.CostPayerUnitId, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDiscardActivationProviderPrivacyTest,
	"Wandbound.DiscardActivationSource.ProductionProviderAndPrivacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDiscardActivationProviderPrivacyTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded = LoadFixture();
	TestTrue(TEXT("Fixture loads for provider"), Loaded.bOk);
	if (!Loaded.bOk || !Loaded.Snapshot.IsValid())
	{
		return false;
	}
	FWBGameStateData State = MakeState();
	FWBProductionActivationDataProvider Provider;
	FWBProductionActivationDataProviderInput Input;
	Input.GameState = &State;
	Input.Repository = &Loaded.Snapshot->CoreRepository;
	Input.ViewerPlayerId = PlayerA;
	FWBProductionActivationDataProviderConfig Config;
	Provider.Configure(Input, Config);
	TestTrue(TEXT("Provider config enables Discard by default"),
		Config.bIncludeDiscardSources);
	TestTrue(TEXT("Provider configured"), Provider.IsConfigured());
	FWBRuntimeActivationDataProviderRequest Request;
	Request.PlayerId = PlayerA;
	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(Request);
	TestTrue(TEXT("Owner provider succeeds"), Result.bOk);
	TestEqual(TEXT("Two exact normal Discard candidates"),
		Result.RefreshInput.ActivationActionSet.Actions.Num(), 2);
	if (Result.RefreshInput.ActivationActionSet.Actions.Num() == 2)
	{
		const FWBCardActivationLegalAction& A =
			Result.RefreshInput.ActivationActionSet.Actions[0];
		const FWBCardActivationLegalAction& B =
			Result.RefreshInput.ActivationActionSet.Actions[1];
		TestEqual(TEXT("First candidate exact instance"),
			A.Candidate.SourceCardInstanceId, FString(TEXT("discard_echo_a")));
		TestEqual(TEXT("Second candidate exact instance"),
			B.Candidate.SourceCardInstanceId, FString(TEXT("discard_echo_b")));
		TestEqual(TEXT("First candidate source zone"),
			static_cast<int32>(A.Candidate.SourceZone),
			static_cast<int32>(EWBCardZone::Discard));
		TestEqual(TEXT("Second candidate source zone"),
			static_cast<int32>(B.Candidate.SourceZone),
			static_cast<int32>(EWBCardZone::Discard));
		TestEqual(TEXT("First command source zone"),
			static_cast<int32>(A.Command.Source.SourceZone),
			static_cast<int32>(EWBCardZone::Discard));
		TestEqual(TEXT("Second command source zone"),
			static_cast<int32>(B.Command.Source.SourceZone),
			static_cast<int32>(EWBCardZone::Discard));
		TestEqual(TEXT("No source unit on first candidate"), A.SourceUnitId, INDEX_NONE);
		TestEqual(TEXT("No source unit on second candidate"), B.SourceUnitId, INDEX_NONE);
		TestTrue(TEXT("First owner ID has zdiscard"),
			A.ActivationActionId.Contains(TEXT(":zdiscard:")));
		TestTrue(TEXT("Second owner ID has zdiscard"),
			B.ActivationActionId.Contains(TEXT(":zdiscard:")));
		TestTrue(TEXT("First owner ID identifies exact A"),
			A.ActivationActionId.Contains(TEXT(":idiscard_echo_a:")));
		TestTrue(TEXT("Second owner ID identifies exact B"),
			B.ActivationActionId.Contains(TEXT(":idiscard_echo_b:")));
		TestFalse(TEXT("Owner ID contains no zone index"),
			A.ActivationActionId.Contains(TEXT("zone_index")));
		TestEqual(TEXT("Deterministic zone order A then B"),
			A.Candidate.SourceCardInstanceId,
			FString(TEXT("discard_echo_a")));
	}

	FWBGameStateData OpponentState = State;
	OpponentState.CurrentPlayer = PlayerB;
	OpponentState.PriorityPlayer = PlayerB;
	FWBProductionActivationDataProvider OpponentProvider;
	Input.GameState = &OpponentState;
	Input.ViewerPlayerId = PlayerB;
	OpponentProvider.Configure(Input, Config);
	Request.PlayerId = PlayerB;
	const FWBRuntimeActivationDataProviderResult OpponentResult =
		OpponentProvider.GetActivationDecisionData(Request);
	TestTrue(TEXT("Opponent provider succeeds"), OpponentResult.bOk);
	TestEqual(TEXT("Opponent sees only own Discard source"),
		OpponentResult.RefreshInput.ActivationActionSet.Actions.Num(), 1);
	FString OpponentIds;
	for (const FWBCardActivationLegalAction& Action :
		OpponentResult.RefreshInput.ActivationActionSet.Actions)
	{
		OpponentIds += Action.ActivationActionId;
	}
	TestFalse(TEXT("Opponent cannot enumerate exact A"),
		OpponentIds.Contains(TEXT("discard_echo_a")));
	TestFalse(TEXT("Opponent cannot enumerate exact B"),
		OpponentIds.Contains(TEXT("discard_echo_b")));
	TestFalse(TEXT("Opponent receives no owner Discard order"),
		OpponentIds.Contains(TEXT("zone_index")));
	TestTrue(TEXT("Opponent exact source is its own"),
		OpponentIds.Contains(TEXT("opponent_private_discard")));

	const FWBCardZonePlayerObservation OpponentObservation =
		WBCardZoneObservation::BuildObservationForPlayer(State, PlayerB);
	TestEqual(TEXT("Opponent own Discard count"),
		OpponentObservation.OwnDiscard.Count, 1);
	const FWBObservedZoneSummary* HiddenDiscard =
		OpponentObservation.PublicSummary.PlayerDiscards.FindByPredicate(
			[](const FWBObservedZoneSummary& Summary)
			{
				return Summary.OwnerPlayerId == PlayerA;
			});
	TestNotNull(TEXT("Owner Discard has public count summary"), HiddenDiscard);
	if (HiddenDiscard != nullptr)
	{
		TestEqual(TEXT("Owner Discard count is visible only as count"),
			HiddenDiscard->Count, 3);
		TestTrue(TEXT("Owner Discard cards remain hidden"),
			HiddenDiscard->Cards.IsEmpty());
	}

	State.Phase = EWBGamePhase::Response;
	State.ReactionWindow.Reset();
	State.ReactionWindow.Kind = EWBReactionWindowKind::PostEffect;
	State.ReactionWindow.OriginatingPlayerId = PlayerB;
	State.PriorityPlayer = PlayerA;
	Input.GameState = &State;
	Input.ViewerPlayerId = PlayerA;
	Provider.Configure(Input, Config);
	Request.PlayerId = PlayerA;
	const FWBRuntimeActivationDataProviderResult ResponseResult =
		Provider.GetActivationDecisionData(Request);
	TestTrue(TEXT("Response provider succeeds"), ResponseResult.bOk);
	TestEqual(TEXT("Two exact Discard React candidates"),
		ResponseResult.RefreshInput.ActivationActionSet.Actions.Num(), 2);
	for (const FWBCardActivationLegalAction& Action :
		ResponseResult.RefreshInput.ActivationActionSet.Actions)
	{
		TestEqual(TEXT("Only response-timed Discard effect is exposed"),
			Action.Candidate.SourceEffectId,
			FString(TEXT("discard_fixture_react")));
	}

	State.PriorityPlayer = PlayerB;
	Provider.Configure(Input, Config);
	const FWBRuntimeActivationDataProviderResult WrongPriority =
		Provider.GetActivationDecisionData(Request);
	TestTrue(TEXT("Wrong-priority provider remains valid"), WrongPriority.bOk);
	TestTrue(TEXT("Wrong-priority owner gets no Discard React"),
		WrongPriority.RefreshInput.ActivationActionSet.Actions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDiscardActivationCoordinatorLifecycleTest,
	"Wandbound.DiscardActivationSource.CoordinatorLifecycleNegationAndReplayInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDiscardActivationCoordinatorLifecycleTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded = LoadFixture();
	TestTrue(TEXT("Fixture loads for coordinator"), Loaded.bOk);
	if (!Loaded.bOk || !Loaded.Snapshot.IsValid())
	{
		return false;
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(
		MakeMatchRequest(Loaded.Snapshot->CoreRepository, true));
	TestTrue(TEXT("Coordinator initializes"), Started.bOk);
	TestEqual(TEXT("Replay generation begins at one"),
		Coordinator.GetCoordinatorGeneration(), 1);
	TestTrue(TEXT("First exact source is in opening Hand"), ZoneContains(
		Coordinator.GetState(), PlayerA, EWBCardZone::Hand,
		TEXT("discard_match_echo_a")));
	TestTrue(TEXT("Second exact source is in opening Hand"), ZoneContains(
		Coordinator.GetState(), PlayerA, EWBCardZone::Hand,
		TEXT("discard_match_echo_b")));
	TestTrue(TEXT("First exact source discards through coordinator"),
		SubmitDiscard(Coordinator, TEXT("discard_match_echo_a")));
	TestTrue(TEXT("Second exact source discards through coordinator"),
		SubmitDiscard(Coordinator, TEXT("discard_match_echo_b")));
	TestTrue(TEXT("First exact source now in Discard"), ZoneContains(
		Coordinator.GetState(), PlayerA, EWBCardZone::Discard,
		TEXT("discard_match_echo_a")));
	TestTrue(TEXT("Second exact source now in Discard"), ZoneContains(
		Coordinator.GetState(), PlayerA, EWBCardZone::Discard,
		TEXT("discard_match_echo_b")));

	const FWBPlayerStateData* TargetPlayer =
		Coordinator.GetState().GetPlayerById(PlayerB);
	const FWBUnitState* TargetHero = TargetPlayer != nullptr
		? Coordinator.GetState().GetUnitById(TargetPlayer->HeroUnitId)
		: nullptr;
	TestNotNull(TEXT("Target Hero exists"), TargetHero);
	if (TargetHero == nullptr)
	{
		return false;
	}
	const int32 HPBefore = TargetHero->HP;
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	TestTrue(TEXT("Legal actions enumerate after Discard"), Legal.bOk);
	const int32 HeroUnitId = TargetHero->UnitId;
	const FWBMatchLegalAction* Activate = FindActivation(
		Legal.Actions,
		TEXT("discard_fixture_damage"),
		TEXT("discard_match_echo_a"),
		HeroUnitId);
	TestNotNull(TEXT("Exact Discard activation is production legal"), Activate);
	if (Activate == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Legal action source zone Discard"),
		static_cast<int32>(Activate->ActivationCommand.Source.SourceZone),
		static_cast<int32>(EWBCardZone::Discard));
	TestEqual(TEXT("Legal action source exact identity"),
		Activate->ActivationCommand.Source.SourceCardInstanceId,
		FString(TEXT("discard_match_echo_a")));
	TestEqual(TEXT("Legal action has no source unit"),
		Activate->ActivationCommand.Source.SourceUnitId, INDEX_NONE);
	TestTrue(TEXT("Legal action ID binds exact source"),
		Activate->ActionId.Contains(TEXT("discard_match_echo_a")));
	const FString ActivationId = Activate->ActionId;
	const FWBMatchOperationResult Declared =
		Coordinator.SubmitActionId(PlayerA, ActivationId);
	TestTrue(TEXT("Discard activation declaration accepted"), Declared.bOk);
	TestEqual(TEXT("One pending effect frame"),
		Coordinator.GetPendingEffectActivationStack().Num(), 1);
	TestEqual(TEXT("Existing response phase opens"),
		static_cast<int32>(Coordinator.GetMatchPhase()),
		static_cast<int32>(EWBMatchLoopPhase::Response));
	TestTrue(TEXT("Source remains in Discard while pending"), ZoneContains(
		Coordinator.GetState(), PlayerA, EWBCardZone::Discard,
		TEXT("discard_match_echo_a")));
	TestEqual(TEXT("Payload has not resolved while pending"),
		Coordinator.GetState().GetUnitById(HeroUnitId)->HP, HPBefore);
	if (Coordinator.GetPendingEffectActivationStack().Num() == 1)
	{
		const FWBPendingEffectActivationFrame& Frame =
			Coordinator.GetPendingEffectActivationStack()[0];
		TestEqual(TEXT("Pending frame preserves Discard zone"),
			static_cast<int32>(Frame.Command.Source.SourceZone),
			static_cast<int32>(EWBCardZone::Discard));
		TestEqual(TEXT("Pending frame preserves exact source"),
			Frame.Command.Source.SourceCardInstanceId,
			FString(TEXT("discard_match_echo_a")));
		TestEqual(TEXT("Pending frame preserves CardId"),
			Frame.Command.Source.SourceCardId,
			FString(TEXT("discard_fixture_echo")));
	}

	const FWBMatchObservation OpponentObservation =
		Coordinator.BuildObservation(PlayerB);
	FString OpponentLegalIds;
	for (const FWBMatchLegalAction& Action : OpponentObservation.LegalActions)
	{
		OpponentLegalIds += Action.ActionId;
	}
	TestFalse(TEXT("Opponent response observation hides exact Discard source"),
		OpponentLegalIds.Contains(TEXT("discard_match_echo_a")));
	TestFalse(TEXT("Opponent response observation hides duplicate source"),
		OpponentLegalIds.Contains(TEXT("discard_match_echo_b")));
	const FWBMatchLegalActionGenerationResult ResponseLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Negate = FindActivation(
		ResponseLegal.Actions, TEXT("discard_fixture_negate"));
	TestNotNull(TEXT("Existing NegatePendingEffect is legal"), Negate);
	if (Negate == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Negate targets existing pending frame"),
		Negate->ActivationCommand.EffectRequest.Payloads[0].PendingEffectFrameId,
		Coordinator.GetPendingEffectActivationStack()[0].FrameId);
	const FWBMatchOperationResult NegateDeclared =
		Coordinator.SubmitActionId(PlayerB, Negate->ActionId);
	TestTrue(TEXT("Existing negate activation accepted"), NegateDeclared.bOk);
	TestTrue(TEXT("Nested pending stack formed"),
		Coordinator.GetPendingEffectActivationStack().Num() >= 1);
	TestTrue(TEXT("Passes drain nested response deterministically"),
		DrainResponseByPassing(Coordinator));
	TestEqual(TEXT("Negated payload does not damage"),
		Coordinator.GetState().GetUnitById(HeroUnitId)->HP, HPBefore);
	TestTrue(TEXT("Negated source remains in Discard"), ZoneContains(
		Coordinator.GetState(), PlayerA, EWBCardZone::Discard,
		TEXT("discard_match_echo_a")));
	TestTrue(TEXT("Duplicate source remains in Discard"), ZoneContains(
		Coordinator.GetState(), PlayerA, EWBCardZone::Discard,
		TEXT("discard_match_echo_b")));
	TestTrue(TEXT("Pending stack closes"),
		Coordinator.GetPendingEffectActivationStack().IsEmpty());
	TestFalse(TEXT("Reaction window closes"),
		Coordinator.GetState().HasOpenReactionWindow());
	TestEqual(TEXT("Coordinator returns to Action"),
		static_cast<int32>(Coordinator.GetMatchPhase()),
		static_cast<int32>(EWBMatchLoopPhase::Action));
	TestTrue(TEXT("Accepted action records include activation decisions"),
		Coordinator.GetCommittedActionRecords().Num() >= 4);
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	TestEqual(TEXT("State digest remains SHA-256 sized"),
		Coordinator.GetCurrentStateDigest().Len(), 64);
	TestEqual(TEXT("Trace digest remains SHA-256 sized"),
		Coordinator.GetCurrentTraceDigest().Len(), 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDiscardActivationProductionSmokeTest,
	"Wandbound.DiscardActivationSource.ProductionSmokeAndFreshReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDiscardActivationProductionSmokeTest::RunTest(const FString&)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Data/Replay/DiscardActivationFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionDiscardActivationSmokeResult Result =
		WBProductionDiscardActivationSmoke::Run(Request);
	if (!Result.bOk)
	{
		AddError(FString::Printf(
			TEXT("Production Discard activation smoke failed: %s"),
			*Result.Reason));
	}
	TestTrue(TEXT("Production Discard activation smoke succeeds"), Result.bOk);
	TestEqual(TEXT("Pass-through and negated scenarios verified"),
		Result.ScenariosVerified, 2);
	TestTrue(TEXT("Replay records verified"), Result.RecordsVerified >= 8);
	TestEqual(TEXT("Final state digest is SHA-256 sized"),
		Result.FinalStateDigest.Len(), 64);
	TestEqual(TEXT("Final trace digest is SHA-256 sized"),
		Result.FinalTraceDigest.Len(), 64);
	return true;
}

#endif
