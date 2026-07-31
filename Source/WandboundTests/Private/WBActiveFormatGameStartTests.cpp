#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActiveFormat.h"
#include "WBBoardRegion.h"
#include "WBCardLifecycle.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBGameStartAddendum.h"
#include "WBInitialHeroSetup.h"
#include "WBMarkerResolution.h"
#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionRuntimeBootstrap.h"
#include "WBProductionStartupResult.h"
#include "WBRules.h"
#include "WBTurnOneRestrictions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FString ProductionPath(const FString& Relative)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/InitialCanonical"),
		Relative);
}

FWBProductionCardDatabaseLoadResult LoadProduction()
{
	return WBProductionCardDatabase::LoadManifestSuite(
		ProductionPath(TEXT("root_manifest.json")));
}

FWBActiveFormatLoadResult LoadFormat()
{
	return WBActiveFormat::Load(
		ProductionPath(TEXT("active_format_v1.json")));
}

FWBGameStartAddendumLoadResult LoadAddendum()
{
	return WBGameStartAddendum::Load(
		ProductionPath(TEXT("game_start_addendum_v1.json")));
}

FWBProductionRuntimeBootstrapResult BootstrapProduction()
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath =
		ProductionPath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath =
		ProductionPath(TEXT("match_spec.json"));
	return WBProductionRuntimeBootstrap::Build(Request);
}

TArray<FString> PreferredDeck(const FString& Hero)
{
	return {
		Hero,
		TEXT("char_new_1_6_18_3"),
		TEXT("char_new_2_4_20_2"),
		TEXT("char_new_3_4_16_2"),
		TEXT("char_new_rl_duelist"),
		TEXT("char_new_rl_monolith"),
		TEXT("char_test_02")
	};
}

FWBActiveFormatPlayerInput ProductionPlayer(
	const int32 PlayerId,
	const FString& Hero)
{
	FWBActiveFormatPlayerInput Player;
	Player.PlayerId = PlayerId;
	Player.HeroDefinitionId = Hero;
	Player.MainDeckDefinitionIds = PreferredDeck(Hero);
	Player.SetupTrapDefinitionIds = {
		TEXT("trap_generic_01"),
		TEXT("trap_generic_01")
	};
	Player.SetupNPCDefinitionIds = {
		TEXT("npc_generic_01"),
		TEXT("npc_human_marksman")
	};
	return Player;
}

FWBCardDefinition SimpleDefinition(
	const FString& Id,
	const EWBCardDefinitionKind Kind)
{
	FWBCardDefinition Definition;
	Definition.CardId = Id;
	Definition.PublicName = Id;
	Definition.PublicCategory =
		Kind == EWBCardDefinitionKind::Character
			? TEXT("Character")
			: TEXT("Fixture");
	Definition.Kind = Kind;
	Definition.CharacterStats.HP = 10;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 8;
	Definition.CharacterStats.RL = 3;
	Definition.TrapDamage =
		Kind == EWBCardDefinitionKind::Trap ? 2 : 0;
	return Definition;
}

FWBCardDefinitionRepository SetupRepository(
	const TArray<FWBSetupSummonTriggerDefinition>& Triggers = {})
{
	FWBCardDefinition HeroA =
		SimpleDefinition(TEXT("fixture_hero_alpha"),
			EWBCardDefinitionKind::Character);
	HeroA.SetupSummonTriggers = Triggers;
	FWBCardDefinition HeroB =
		SimpleDefinition(TEXT("fixture_hero_beta"),
			EWBCardDefinitionKind::Character);
	HeroB.PublicFactions.Add(TEXT("black_meridian"));
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("active_format_game_start_tests");
	Repository.SourceVersion = TEXT("1");
	Repository.Definitions = {
		HeroA,
		HeroB,
		SimpleDefinition(
			TEXT("fixture_filler"),
			EWBCardDefinitionKind::Action),
		SimpleDefinition(
			TEXT("fixture_trap"),
			EWBCardDefinitionKind::Trap),
		SimpleDefinition(
			TEXT("fixture_npc"),
			EWBCardDefinitionKind::NPC)
	};
	return Repository;
}

void AddSetupPlayerAndDeck(
	FWBGameStateData& State,
	const int32 PlayerId,
	const int32 DeckCount = 8)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.HeroUnitId = -1;
	State.Players.Add(Player);
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	for (int32 Index = 0; Index < DeckCount; ++Index)
	{
		FWBZoneCardEntry Entry;
		Entry.Card.InstanceId = FString::Printf(
			TEXT("p%d_filler_%d"),
			PlayerId,
			Index);
		Entry.Card.CardId = TEXT("fixture_filler");
		Entry.Card.OwnerPlayerId = PlayerId;
		Entry.Zone = EWBCardZone::Deck;
		Entry.ZoneIndex = Index;
		Zones.Deck.Add(Entry);
	}
	State.CardZoneState.PlayerZones.Add(Zones);
}

FWBInitialHeroSetupResult ApplyHeroFixture(
	const TArray<FWBSetupSummonTriggerDefinition>& Triggers,
	const TMap<int32, TArray<FString>>& Choices = {})
{
	FWBGameStateData State;
	State.FirstPlayerId = 1;
	State.CurrentPlayer = 1;
	State.PriorityPlayer = 1;
	State.bInitialSetupInProgress = true;
	State.bSuppressManualReactsDuringInitialHeroSetup = true;
	AddSetupPlayerAndDeck(State, 0);
	AddSetupPlayerAndDeck(State, 1);
	FWBInitialHeroSetupRequest Request;
	Request.FirstPlayerId = 1;
	Request.Placements = {
		{ 0, TEXT("alpha_instance"), TEXT("fixture_hero_alpha"), FWBTile(4, 8) },
		{ 1, TEXT("beta_instance"), TEXT("fixture_hero_beta"), FWBTile(4, 0) }
	};
	Request.TriggerOrderChoices = Choices;
	return WBInitialHeroSetup::Apply(
		State,
		SetupRepository(Triggers),
		Request);
}

FWBSetupSummonTriggerDefinition Trigger(
	const FString& Id,
	const EWBSetupSummonTriggerScope Scope,
	const int32 DrawCount = 0,
	const FString& Faction = FString())
{
	FWBSetupSummonTriggerDefinition Result;
	Result.TriggerId = Id;
	Result.Scope = Scope;
	Result.DrawCount = DrawCount;
	Result.FactionId = Faction;
	return Result;
}

bool HasTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.ContainsByPredicate(
		[Kind](const FWBTraceEvent& Event)
		{
			return Event.Kind == Kind;
		});
}

int32 TraceIndex(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind,
	const int32 PlayerId = INDEX_NONE)
{
	return Events.IndexOfByPredicate(
		[Kind, PlayerId](const FWBTraceEvent& Event)
		{
			return Event.Kind == Kind
				&& (PlayerId == INDEX_NONE
					|| Event.PlayerId == PlayerId);
		});
}

TArray<FWBSetupMarkerPlacement> LegalMarkers()
{
	return {
		{ 0, EWBMarkerType::Trap, FWBTile(0, 8), TEXT("fixture_trap"), 0 },
		{ 0, EWBMarkerType::Trap, FWBTile(1, 8), TEXT("fixture_trap"), 1 },
		{ 0, EWBMarkerType::NPC, FWBTile(2, 8), TEXT("fixture_npc"), 2 },
		{ 0, EWBMarkerType::NPC, FWBTile(3, 8), TEXT("fixture_npc"), 3 },
		{ 1, EWBMarkerType::Trap, FWBTile(0, 0), TEXT("fixture_trap"), 4 },
		{ 1, EWBMarkerType::Trap, FWBTile(1, 0), TEXT("fixture_trap"), 5 },
		{ 1, EWBMarkerType::NPC, FWBTile(2, 0), TEXT("fixture_npc"), 6 },
		{ 1, EWBMarkerType::NPC, FWBTile(3, 0), TEXT("fixture_npc"), 7 }
	};
}

FWBGameStateData TurnOneState(const int32 FirstPlayer = 0)
{
	FWBGameStateData State;
	State.FirstPlayerId = FirstPlayer;
	State.CurrentPlayer = FirstPlayer;
	State.PriorityPlayer = FirstPlayer;
	State.TurnNumber = 1;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData P0;
	P0.PlayerId = 0;
	P0.RemainingMP = 6;
	FWBPlayerStateData P1;
	P1.PlayerId = 1;
	P1.RemainingMP = 6;
	State.Players = { P0, P1 };
	return State;
}

FWBAction AttackAction(
	const FWBUnitState& Attacker,
	const FWBUnitState& Defender)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = Attacker.OwnerId;
	Action.SourceUnitId = Attacker.UnitId;
	Action.TargetUnitId = Defender.UnitId;
	Action.FromTile = FWBTile(Attacker.X, Attacker.Y);
	Action.ToTile = FWBTile(Defender.X, Defender.Y);
	return Action;
}

FWBUnitState Unit(
	const int32 Id,
	const int32 Owner,
	const FWBTile Tile,
	const int32 Range = 8)
{
	FWBUnitState Result;
	Result.UnitId = Id;
	Result.OwnerId = Owner;
	Result.CardId = FString::Printf(TEXT("unit_%d"), Id);
	Result.X = Tile.X;
	Result.Y = Tile.Y;
	Result.HP = 10;
	Result.MaxHP = 10;
	Result.ATK = 2;
	Result.AR = Range;
	Result.AttacksLeft = 1;
	Result.MaxAttacksPerTurn = 1;
	return Result;
}

bool RunActiveFormatCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const FWBProductionCardDatabaseLoadResult Database =
		LoadProduction();
	const FWBActiveFormatLoadResult Format = LoadFormat();
	if (!Test.TestTrue(
		TEXT("Production format prerequisites load"),
		Database.bOk && Database.Snapshot.IsValid()
			&& Format.bOk))
	{
		return false;
	}

	if (Name.EndsWith(TEXT("StoredDeckMinimumAccepted")))
	{
		const FWBActiveFormatValidationResult Result =
			WBActiveFormat::ValidateStoredMainDeck(
				Format.Format,
				*Database.Snapshot,
				{ TEXT("char_test_01") });
		Test.TestTrue(TEXT("One stored card accepted"), Result.bOk);
	}
	else if (Name.EndsWith(TEXT("StoredDeckMaximumAccepted")))
	{
		FWBProductionCardDatabase Synthetic;
		TArray<FString> Ids;
		TArray<FWBCardDefinition> Definitions;
		for (int32 Index = 0; Index < 30; ++Index)
		{
			FWBProductionCardRecord Record;
			Record.Type = EWBProductionCardType::Character;
			Record.CoreDefinition = SimpleDefinition(
				FString::Printf(TEXT("character_%02d"), Index),
				EWBCardDefinitionKind::Character);
			Ids.Add(Record.CoreDefinition.CardId);
			Definitions.Add(Record.CoreDefinition);
			Synthetic.Records.Add(Record);
		}
		WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
			TEXT("synthetic"),
			TEXT("1"),
			Definitions,
			Synthetic.CoreRepository);
		const FWBActiveFormatValidationResult Result =
			WBActiveFormat::ValidateStoredMainDeck(
				Format.Format,
				Synthetic,
				Ids);
		Test.TestTrue(TEXT("Thirty stored cards accepted"), Result.bOk);
	}
	else if (Name.EndsWith(TEXT("MainDeckDuplicateRejected")))
	{
		const FWBActiveFormatValidationResult Result =
			WBActiveFormat::ValidateStoredMainDeck(
				Format.Format,
				*Database.Snapshot,
				{ TEXT("char_test_01"), TEXT("char_test_01") });
		Test.TestEqual(
			TEXT("Duplicate diagnostic"),
			Result.Reason,
			FString(TEXT("active_format_duplicate_main_deck_definition")));
	}
	else if (Name.EndsWith(TEXT("TrapInMainDeckRejected")))
	{
		const FWBActiveFormatValidationResult Result =
			WBActiveFormat::ValidateStoredMainDeck(
				Format.Format,
				*Database.Snapshot,
				{ TEXT("char_test_01"), TEXT("trap_generic_01") });
		Test.TestEqual(TEXT("Trap diagnostic"), Result.Reason,
			FString(TEXT("active_format_trap_in_main_deck")));
	}
	else if (Name.EndsWith(TEXT("NPCInMainDeckRejected")))
	{
		const FWBActiveFormatValidationResult Result =
			WBActiveFormat::ValidateStoredMainDeck(
				Format.Format,
				*Database.Snapshot,
				{ TEXT("char_test_01"), TEXT("npc_generic_01") });
		Test.TestEqual(TEXT("NPC diagnostic"), Result.Reason,
			FString(TEXT("active_format_npc_in_main_deck")));
	}
	else if (Name.EndsWith(TEXT("NonHybridCharacterRequired")))
	{
		const FWBActiveFormatValidationResult Result =
			WBActiveFormat::ValidateStoredMainDeck(
				Format.Format,
				*Database.Snapshot,
				{ TEXT("trap_generic_01") });
		Test.TestFalse(TEXT("Deck without Character rejected"), Result.bOk);
	}
	else
	{
		FWBActiveFormatPlayerInput Player =
			ProductionPlayer(0, TEXT("char_test_01"));
		if (Name.EndsWith(TEXT("InsufficientOpeningHandCapacityRejected")))
		{
			Player.MainDeckDefinitionIds.SetNum(6);
			const FWBActiveFormatValidationResult Result =
				WBActiveFormat::ValidatePlayerForLaunch(
					Format.Format,
					*Database.Snapshot,
					Player);
			Test.TestEqual(
				TEXT("Launch capacity diagnostic"),
				Result.Reason,
				FString(TEXT("active_format_insufficient_opening_hand_capacity")));
		}
		else if (Name.EndsWith(TEXT("MirroredDecksAccepted")))
		{
			FWBActiveFormatPlayerInput Other = Player;
			Other.PlayerId = 1;
			const FWBActiveFormatValidationResult Result =
				WBActiveFormat::ValidateMatchForLaunch(
					Format.Format,
					*Database.Snapshot,
					{ Player, Other });
			Test.TestTrue(TEXT("Mirrored decks accepted"), Result.bOk);
		}
		else
		{
			const FWBActiveFormatValidationResult Result =
				WBActiveFormat::ValidatePlayerForLaunch(
					Format.Format,
					*Database.Snapshot,
					Player);
			Test.TestTrue(TEXT("Launch player accepted"), Result.bOk);
			if (Name.EndsWith(TEXT("RepeatedSetupTrapAccepted")))
			{
				Test.TestEqual(TEXT("Repeated Trap retained"),
					Player.SetupTrapDefinitionIds[0],
					Player.SetupTrapDefinitionIds[1]);
			}
			if (Name.EndsWith(TEXT("RepeatedSetupNPCAccepted")))
			{
				Player.SetupNPCDefinitionIds = {
					TEXT("npc_generic_01"),
					TEXT("npc_generic_01")
				};
				Test.TestTrue(
					TEXT("Repeated NPC accepted"),
					WBActiveFormat::ValidatePlayerForLaunch(
						Format.Format,
						*Database.Snapshot,
						Player).bOk);
			}
		}
	}
	return true;
}

bool RunMarkerCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FWBGameStateData State;
	AddSetupPlayerAndDeck(State, 0, 0);
	AddSetupPlayerAndDeck(State, 1, 0);
	TArray<FWBSetupMarkerPlacement> Markers = LegalMarkers();
	if (Name.EndsWith(TEXT("OtherLegalOwnHalfTileAccepted")))
	{
		Test.TestTrue(
			TEXT("Legal setup accepted"),
			WBMarkerResolution::ApplyCanonicalSetup(
				State,
				SetupRepository(),
				Markers).bOk);
	}
	else
	{
		Markers[0].Tile = FWBTile(4, 8);
		const FWBMarkerResolutionResult First =
			WBMarkerResolution::ApplyCanonicalSetup(
				State,
				SetupRepository(),
				Markers);
		Test.TestEqual(
			TEXT("Reserved Hero spawn diagnostic"),
			First.Reason,
			FString(TEXT("setup_marker_on_reserved_hero_spawn_tile")));
		if (Name.EndsWith(TEXT("BothHeroSpawnTilesReserved")))
		{
			State.CardZoneState.MarkerPlaceholders.Reset();
			Markers = LegalMarkers();
			Markers[4].Tile = FWBTile(4, 0);
			Test.TestEqual(
				TEXT("Other Hero spawn is also reserved"),
				WBMarkerResolution::ApplyCanonicalSetup(
					State,
					SetupRepository(),
					Markers).Reason,
				FString(TEXT("setup_marker_on_reserved_hero_spawn_tile")));
		}
	}
	return true;
}

bool RunHeroCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	EWBSetupSummonTriggerScope Scope =
		EWBSetupSummonTriggerScope::OwnWhenSummoned;
	if (Name.Contains(TEXT("CharacterSummoned")))
	{
		Scope = EWBSetupSummonTriggerScope::CharacterSummoned;
	}
	else if (Name.Contains(TEXT("UnitSummoned")))
	{
		Scope = EWBSetupSummonTriggerScope::UnitSummoned;
	}
	else if (Name.Contains(TEXT("OpponentSummoned")))
	{
		Scope = EWBSetupSummonTriggerScope::OpponentSummonsUnit;
	}
	else if (Name.Contains(TEXT("FactionObserver")))
	{
		Scope = EWBSetupSummonTriggerScope::FactionSummoned;
	}
	const FString Faction = Scope
			== EWBSetupSummonTriggerScope::FactionSummoned
		? FString(TEXT("black_meridian"))
		: FString();
	const FWBSetupSummonTriggerDefinition Definition =
		Trigger(TEXT("fixture_trigger"), Scope, 0, Faction);
	TMap<int32, TArray<FString>> Choices;
	if (Scope == EWBSetupSummonTriggerScope::CharacterSummoned
		|| Scope == EWBSetupSummonTriggerScope::UnitSummoned)
	{
		Choices.Add(0, {
			TEXT("setup_trigger:p0:l0:s0:fixture_trigger"),
			TEXT("setup_trigger:p0:l0:s1:fixture_trigger")
		});
	}
	const FWBInitialHeroSetupResult Result =
		ApplyHeroFixture({ Definition }, Choices);
	Test.TestTrue(TEXT("Hero setup succeeds"), Result.bOk);
	Test.TestTrue(TEXT("Atomic batch committed"),
		Result.bSpawnBatchCommitted);
	Test.TestTrue(TEXT("Triggers collected after commit"),
		TraceIndex(Result.TraceEvents,
			FName(TEXT("hero_spawn_batch_committed")))
		< TraceIndex(Result.TraceEvents,
			FName(TEXT("hero_summon_triggers_collected"))));
	if (Name.Contains(TEXT("ReplayStable")))
	{
		const FWBInitialHeroSetupResult Again =
			ApplyHeroFixture({ Definition }, Choices);
		Test.TestEqual(
			TEXT("Trace replay is stable"),
			WBReplayTrace::SerializeEvents(Result.TraceEvents),
			WBReplayTrace::SerializeEvents(Again.TraceEvents));
	}
	if (Name.Contains(TEXT("NoCardIdHardCoding")))
	{
		Test.TestTrue(
			TEXT("Fixture IDs trigger through definitions"),
			Result.CollectedTriggerIds.ContainsByPredicate(
				[](const FString& Id)
				{
					return Id.Contains(TEXT("fixture_trigger"));
				}));
	}
	return true;
}

bool RunTriggerOrderingCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const TArray<FWBSetupSummonTriggerDefinition> Triggers = {
		Trigger(TEXT("trigger_a"),
			EWBSetupSummonTriggerScope::OwnWhenSummoned),
		Trigger(TEXT("trigger_b"),
			EWBSetupSummonTriggerScope::OwnWhenSummoned)
	};
	const TArray<FString> OrderedIds = {
		TEXT("setup_trigger:p0:l0:s0:trigger_b"),
		TEXT("setup_trigger:p0:l0:s0:trigger_a")
	};
	TMap<int32, TArray<FString>> Choices;
	Choices.Add(0, OrderedIds);
	const FWBInitialHeroSetupResult Result =
		ApplyHeroFixture(Triggers, Choices);
	Test.TestTrue(TEXT("Explicit order resolves"), Result.bOk);
	const int32 P1Resolved = TraceIndex(
		Result.TraceEvents,
		FName(TEXT("setup_trigger_resolved")),
		1);
	const int32 P0Resolved = TraceIndex(
		Result.TraceEvents,
		FName(TEXT("setup_trigger_resolved")),
		0);
	if (Name.Contains(TEXT("FirstPlayerBatch")))
	{
		Test.TestTrue(TEXT("First player batch precedes second"),
			P1Resolved == INDEX_NONE || P1Resolved < P0Resolved);
	}
	else if (Name.Contains(TEXT("SecondPlayerBatch")))
	{
		Test.TestTrue(TEXT("Second player batch follows first"),
			P1Resolved == INDEX_NONE || P1Resolved < P0Resolved);
	}
	else if (Name.Contains(TEXT("StableActionId")))
	{
		Test.TestTrue(
			TEXT("Stable order action ID emitted"),
			Result.TraceEvents.ContainsByPredicate(
				[](const FWBTraceEvent& Event)
				{
					return Event.Kind
							== FName(TEXT("setup_trigger_order_chosen"))
						&& Event.ActionId.StartsWith(
							TEXT("setup_trigger_order:p0:"));
				}));
	}
	else if (Name.Contains(TEXT("ReplayStable")))
	{
		const FWBInitialHeroSetupResult Again =
			ApplyHeroFixture(Triggers, Choices);
		Test.TestEqual(TEXT("Order choice replay stable"),
			WBReplayTrace::SerializeEvents(Result.TraceEvents),
			WBReplayTrace::SerializeEvents(Again.TraceEvents));
	}
	else if (Name.Contains(TEXT("SingleTriggerAutoResolves")))
	{
		Test.TestTrue(
			TEXT("Single trigger needs no order choice"),
			ApplyHeroFixture(
				{ Triggers[0] }).bOk);
	}
	else
	{
		const FWBInitialHeroSetupResult MissingChoice =
			ApplyHeroFixture(Triggers);
		Test.TestEqual(TEXT("Choice required"),
			MissingChoice.Reason,
			FString(TEXT("setup_trigger_order_choice_required")));
	}
	return true;
}

bool RunReactCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FWBGameStateData State;
	State.bInitialSetupInProgress = true;
	State.bSuppressManualReactsDuringInitialHeroSetup = true;
	FString Reason;
	if (Name.Contains(TEXT("NormalReactAvailabilityRestored")))
	{
		State.bInitialSetupInProgress = false;
		State.bSuppressManualReactsDuringInitialHeroSetup = false;
		Test.TestTrue(TEXT("Manual React gate restored"),
			WBInitialHeroSetup::CanSubmitManualReact(State, Reason));
	}
	else if (Name.Contains(TEXT("RequiredTriggerChoice")))
	{
		const FWBInitialHeroSetupResult Result =
			ApplyHeroFixture({
				Trigger(TEXT("a"),
					EWBSetupSummonTriggerScope::OwnWhenSummoned),
				Trigger(TEXT("b"),
					EWBSetupSummonTriggerScope::OwnWhenSummoned)
			});
		Test.TestEqual(TEXT("Required choice remains authoritative"),
			Result.Reason,
			FString(TEXT("setup_trigger_order_choice_required")));
	}
	else if (Name.Contains(TEXT("MandatoryNestedTrigger")))
	{
		Test.TestTrue(TEXT("Mandatory setup trigger resolves"),
			ApplyHeroFixture({
				Trigger(TEXT("mandatory"),
					EWBSetupSummonTriggerScope::OwnWhenSummoned)
			}).bOk);
	}
	else
	{
		Test.TestFalse(TEXT("Manual React rejected"),
			WBInitialHeroSetup::CanSubmitManualReact(State, Reason));
		Test.TestEqual(TEXT("Manual React diagnostic"), Reason,
			FString(TEXT("manual_react_not_allowed_during_initial_hero_setup")));
	}
	return true;
}

bool RunOpeningCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const FWBInitialHeroSetupResult Result =
		ApplyHeroFixture({
			Trigger(TEXT("draw_one"),
				EWBSetupSummonTriggerScope::OwnWhenSummoned,
				1)
		});
	Test.TestTrue(TEXT("Setup draw fixture resolves"), Result.bOk);
	Test.TestTrue(TEXT("Setup draw trace emitted"),
		HasTrace(Result.TraceEvents,
			FName(TEXT("setup_trigger_draw"))));
	Test.TestFalse(TEXT("Setup draw trace hides card identity"),
		Result.TraceEvents.ContainsByPredicate(
			[](const FWBTraceEvent& Event)
			{
				return Event.Kind == FName(TEXT("setup_trigger_draw"))
					&& (!Event.CardId.IsEmpty()
						|| !Event.CardInstanceId.IsEmpty());
			}));
	if (Name.Contains(TEXT("HeroDrawOneProducesSeven")))
	{
		FWBGameStateData State;
		AddSetupPlayerAndDeck(State, 0, 8);
		AddSetupPlayerAndDeck(State, 1, 8);
		State.FirstPlayerId = 0;
		State.CurrentPlayer = 0;
		State.bInitialSetupInProgress = true;
		FWBInitialHeroSetupRequest Request;
		Request.FirstPlayerId = 0;
		Request.Placements = {
			{ 0, TEXT("a"), TEXT("fixture_hero_alpha"), FWBTile(4, 8) },
			{ 1, TEXT("b"), TEXT("fixture_hero_beta"), FWBTile(4, 0) }
		};
		const FWBInitialHeroSetupResult Applied =
			WBInitialHeroSetup::Apply(
				State,
				SetupRepository({
					Trigger(TEXT("draw_one"),
						EWBSetupSummonTriggerScope::OwnWhenSummoned,
						1)
				}),
				Request);
		WBCardLifecycle::ApplySetupDraw(State, 0, 6);
		const FWBPlayerCardZoneState* Zones =
			WBCardZoneState::FindPlayerZones(
				State.GetCardZoneState(),
				0);
		Test.TestTrue(TEXT("Fixture setup completed"), Applied.bOk);
		Test.TestEqual(TEXT("Hero starts with seven cards"),
			Zones != nullptr ? Zones->Hand.Num() : -1, 7);
	}
	return true;
}

bool RunBoardOrTurnCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	if (Name.Contains(TEXT("BoardRegion")))
	{
		Test.TestEqual(TEXT("P0 own"),
			WBBoardRegion::GetBoardRegionForPlayer(0, FWBTile(2, 8)),
			EWBPlayerRelativeBoardRegion::OwnHalf);
		Test.TestEqual(TEXT("P1 own"),
			WBBoardRegion::GetBoardRegionForPlayer(1, FWBTile(2, 0)),
			EWBPlayerRelativeBoardRegion::OwnHalf);
		Test.TestEqual(TEXT("Neutral for P0"),
			WBBoardRegion::GetBoardRegionForPlayer(0, FWBTile(2, 4)),
			EWBPlayerRelativeBoardRegion::NeutralRow);
		Test.TestEqual(TEXT("Neutral for P1"),
			WBBoardRegion::GetBoardRegionForPlayer(1, FWBTile(2, 4)),
			EWBPlayerRelativeBoardRegion::NeutralRow);
		Test.TestEqual(TEXT("P0/P1 reverse"),
			WBBoardRegion::GetBoardRegionForPlayer(0, FWBTile(2, 8)),
			WBBoardRegion::GetBoardRegionForPlayer(1, FWBTile(2, 0)));
		return true;
	}

	FWBGameStateData State = TurnOneState();
	if (Name.Contains(TEXT("RestrictionEnds")))
	{
		State.TurnNumber = 2;
	}
	else if (Name.Contains(TEXT("SecondPlayers")))
	{
		State.CurrentPlayer = 1;
		State.PriorityPlayer = 1;
	}
	else if (Name.Contains(TEXT("SetupNotRestricted")))
	{
		State.bInitialSetupInProgress = true;
	}
	const bool bExpected = Name.Contains(
		TEXT("RestrictionAppliesOnly"));
	Test.TestEqual(TEXT("Restriction scope"),
		WBTurnOneRestrictions::IsFirstPlayerTurnOneRestrictionActive(State),
		bExpected);
	return true;
}

bool RunSummonCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const FWBGameStateData State = TurnOneState();
	FWBTile Destination(4, 6);
	if (Name.Contains(TEXT("NeutralRow")))
	{
		Destination = FWBTile(4, 4);
	}
	else if (Name.Contains(TEXT("OpponentHalf"))
		|| Name.Contains(TEXT("IllegalSummon")))
	{
		Destination = FWBTile(4, 3);
	}
	const FWBTurnOneRestrictionQuery Query =
		WBTurnOneRestrictions::QuerySummonPlacement(
			State,
			0,
			Destination);
	if (Name.Contains(TEXT("OpponentHalf"))
		|| Name.Contains(TEXT("IllegalSummon")))
	{
		Test.TestEqual(TEXT("Opponent-half summon diagnostic"),
			Query.Reason,
			FString(TEXT("first_player_turn_one_summon_into_opponent_half")));
	}
	else
	{
		Test.TestTrue(TEXT("Summon destination accepted"), Query.bOk);
	}
	if (Name.Contains(TEXT("SummonIsNotMovement")))
	{
		FWBRelocationStep Step;
		Step.FromTile = FWBTile(4, 5);
		Step.ToTile = FWBTile(4, 4);
		Test.TestFalse(TEXT("Equivalent movement remains blocked"),
			WBTurnOneRestrictions::QueryRelocation(
				State,
				{ Step }).bOk);
	}
	return true;
}

bool RunRelocationCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const FWBGameStateData State = TurnOneState();
	TArray<FWBRelocationStep> Steps;
	FWBRelocationStep First;
	First.UnitId = 10;
	First.FromTile = FWBTile(4, 6);
	First.ToTile = FWBTile(5, 6);
	if (!Name.Contains(TEXT("WithinOwnHalfAccepted"))
		&& !Name.Contains(TEXT("EnemyUnitMovementWithinOpponentHalf")))
	{
		First.ToTile = FWBTile(4, 4);
	}
	if (Name.Contains(TEXT("EnemyUnitMovementWithinOpponentHalf")))
	{
		First.FromTile = FWBTile(4, 2);
		First.ToTile = FWBTile(5, 2);
	}
	Steps.Add(First);
	if (Name.Contains(TEXT("Swap")))
	{
		FWBRelocationStep Second;
		Second.UnitId = 11;
		Second.FromTile = First.ToTile;
		Second.ToTile = First.FromTile;
		Steps.Add(Second);
	}
	if (Name.Contains(TEXT("MultiTile")))
	{
		FWBRelocationStep Second = First;
		Second.FromTile = FWBTile(4, 4);
		Second.ToTile = FWBTile(4, 3);
		Steps.Add(Second);
	}
	const FWBTurnOneRestrictionQuery Query =
		WBTurnOneRestrictions::QueryRelocation(State, Steps);
	const bool bAllowed =
		Name.Contains(TEXT("WithinOwnHalfAccepted"))
		|| Name.Contains(TEXT("EnemyUnitMovementWithinOpponentHalf"));
	Test.TestEqual(TEXT("Shared relocation guard result"),
		Query.bOk, bAllowed);
	if (!bAllowed)
	{
		Test.TestEqual(TEXT("Boundary diagnostic"), Query.Reason,
			FString(TEXT("first_player_turn_one_protected_boundary_crossing")));
	}
	return true;
}

bool RunAttackCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FWBGameStateData State = TurnOneState();
	FWBUnitState Attacker = Unit(0, 0, FWBTile(0, 5), 8);
	FWBTile TargetTile(0, 4);
	if (Name.Contains(TEXT("OwnHalf")))
	{
		TargetTile = FWBTile(0, 6);
	}
	else if (Name.Contains(TEXT("OpponentHalf")))
	{
		TargetTile = FWBTile(0, 3);
	}
	const bool bOpponent = Name.Contains(TEXT("OpponentHero"))
		|| Name.Contains(TEXT("OpponentCharacter"))
		|| Name.Contains(TEXT("OpponentAttackNotGenerated"));
	FWBUnitState Defender = Unit(1, bOpponent ? 1 : -1, TargetTile);
	if (Name.Contains(TEXT("RequiresRange")))
	{
		Attacker.AR = 1;
		Defender.Y = 2;
	}
	State.Units = { Attacker, Defender };
	if (Name.Contains(TEXT("RequiresLineOfSight")))
	{
		State.AddWallForTest(
			FWBWallEdge(FWBTile(0, 5), FWBTile(0, 4)));
	}
	const FWBActionQueryResult Query =
		WBRules::CanDeclareAttack(
			State,
			AttackAction(Attacker, Defender));
	if (bOpponent)
	{
		Test.TestEqual(TEXT("Opponent target diagnostic"),
			Query.Reason,
			FString(TEXT("first_player_turn_one_opponent_controlled_attack_forbidden")));
	}
	else if (Name.Contains(TEXT("RequiresRange"))
		|| Name.Contains(TEXT("RequiresLineOfSight")))
	{
		Test.TestFalse(TEXT("Ordinary attack rule still applies"), Query.bOk);
	}
	else
	{
		Test.TestTrue(TEXT("Neutral NPC attack accepted"), Query.bOk);
	}
	if (Name.Contains(TEXT("Generated")))
	{
		const TArray<FWBAction> Actions =
			WBRules::GenerateLegalActionsForPlayer(State, 0);
		const bool bGenerated =
			Actions.ContainsByPredicate(
				[](const FWBAction& Action)
				{
					return Action.Type == EWBActionType::Attack
						&& Action.TargetUnitId == 1;
				});
		Test.TestEqual(TEXT("Attack generation"), bGenerated, !bOpponent);
	}
	return true;
}

bool RunProductionCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	if (Name.Contains(TEXT("ActiveFormatV1")))
	{
		const FWBActiveFormatLoadResult Result = LoadFormat();
		Test.TestTrue(TEXT("Active Format loads"), Result.bOk);
		if (Name.EndsWith(TEXT("DigestPinned")))
		{
			Test.TestEqual(TEXT("Active Format digest"),
				Result.Format.Digest,
				FString(TEXT("258c5925ae1af7a20663c96d225f21de97b339a523c4b968cc8c6d3a024529af")));
		}
		return true;
	}
	if (Name.Contains(TEXT("GameStartAddendum")))
	{
		const FWBGameStartAddendumLoadResult Result =
			LoadAddendum();
		Test.TestTrue(TEXT("Game-start addendum loads"), Result.bOk);
		if (Name.EndsWith(TEXT("DigestPinned")))
		{
			Test.TestEqual(TEXT("Addendum digest"),
				Result.Addendum.Digest,
				FString(TEXT("bdcd0d2fc19e853f874fc80eb58cb7967062faa3b8fe91f6c85a3289972d1e67")));
		}
		return true;
	}

	const FWBProductionRuntimeBootstrapResult Bootstrap =
		BootstrapProduction();
	Test.TestTrue(TEXT("Production bootstrap validates"), Bootstrap.bOk);
	if (!Bootstrap.bOk)
	{
		Test.AddError(Bootstrap.Reason);
		return false;
	}
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(Bootstrap.InitializationRequest);
	Test.TestTrue(TEXT("Production match initializes"), Started.bOk);
	if (!Started.bOk)
	{
		Test.AddError(Started.Reason);
		return false;
	}
	if (Name.Contains(TEXT("DecksValid")))
	{
		Test.TestEqual(TEXT("Two launch decks"),
			Bootstrap.InitializationRequest.Players.Num(), 2);
		Test.TestEqual(TEXT("P0 seven cards"),
			Bootstrap.InitializationRequest.Players[0].OrderedDeck.Num(), 7);
	}
	else if (Name.Contains(TEXT("SetupKitsValid")))
	{
		Test.TestEqual(TEXT("Eight markers"),
			Bootstrap.InitializationRequest.MarkerPlacements.Num(), 8);
	}
	else if (Name.Contains(TEXT("HeroesSpawnAtomically")))
	{
		Test.TestTrue(TEXT("Atomic Hero commit"),
			Coordinator.WasHeroSpawnBatchCommitted());
		Test.TestEqual(TEXT("Two Heroes on board"),
			Coordinator.GetState().Units.Num(), 2);
	}
	else if (Name.Contains(TEXT("OpeningHandsDrawn")))
	{
		Test.TestTrue(TEXT("Opening hands complete"),
			Coordinator.WereOpeningHandsDrawn());
		for (const FWBPlayerCardZoneState& Zones :
			Coordinator.GetState().GetCardZoneState().PlayerZones)
		{
			Test.TestEqual(TEXT("Opening hand has six"),
				Zones.Hand.Num(), 6);
		}
	}
	else if (Name.Contains(TEXT("FirstDecisionReached")))
	{
		Test.TestTrue(TEXT("Playable decision exists"),
			!Started.NextLegalActions.IsEmpty());
	}
	else if (Name.Contains(TEXT("StartupResultProductionStarted")))
	{
		FWBProductionRuntimeBootstrapRequest Request;
		Request.MatchSpecificationPath =
			ProductionPath(TEXT("match_spec.json"));
		const FWBProductionStartupResult Baseline =
			WBProductionStartupResult::FromBootstrap(
				Request,
				Bootstrap);
		const FWBProductionStartupResult Result =
			WBProductionStartupResult::StartedFromBootstrap(
				Baseline,
				Coordinator,
				1,
				0,
				!Started.NextLegalActions.IsEmpty());
		Test.TestEqual(TEXT("Startup result"),
			Result.ResultCode,
			FString(TEXT("production_started")));
	}
	return true;
}

bool RunAuthorityCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FString Core;
	FFileHelper::LoadFileToString(
		Core,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp")));
	if (Name.Contains(TEXT("CoreOwnsSetupRules")))
	{
		Test.TestTrue(TEXT("Core invokes atomic setup"),
			Core.Contains(TEXT("WBInitialHeroSetup::Apply")));
	}
	else if (Name.Contains(TEXT("SharedRelocationGuardUsed")))
	{
		FString Rules;
		FFileHelper::LoadFileToString(
			Rules,
			*FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Source/WandboundCore/Private/WBRules.cpp")));
		Test.TestTrue(TEXT("Rules use shared relocation guard"),
			Rules.Contains(TEXT("WBTurnOneRestrictions::QueryRelocation")));
	}
	else if (Name.Contains(TEXT("NoMeshyDependencyAdded")))
	{
		Test.TestFalse(TEXT("Core has no Meshy dependency"),
			Core.Contains(TEXT("Meshy")));
	}
	else if (Name.Contains(TEXT("NoCharacterModelsImported")))
	{
		Test.TestFalse(TEXT("Core has no model paths"),
			Core.Contains(TEXT(".fbx"))
				|| Core.Contains(TEXT(".uasset")));
	}
	else if (Name.Contains(TEXT("NoGodotFilesModified")))
	{
		Test.TestFalse(TEXT("Core has no Godot dependency"),
			Core.Contains(TEXT("Reference/GodotProject"))
				|| Core.Contains(TEXT(".gd")));
	}
	else
	{
		FString Runtime;
		FFileHelper::LoadFileToString(
			Runtime,
			*FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp")));
		Test.TestFalse(TEXT("Runtime does not mutate setup legality"),
			Runtime.Contains(TEXT("State.Units.Add"))
				|| Runtime.Contains(TEXT("hero_spawned")));
	}
	return true;
}

bool RunNamedCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	if (Name.Contains(TEXT(".ActiveFormat.V1.")))
	{
		return RunActiveFormatCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Setup.Markers.")))
	{
		return RunMarkerCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Setup.HeroSpawn."))
		|| Name.Contains(TEXT(".Setup.HeroSummon.")))
	{
		return RunHeroCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Setup.HeroTriggers.")))
	{
		return RunTriggerOrderingCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Setup.Reacts.")))
	{
		return RunReactCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Setup.OpeningHand.")))
	{
		return RunOpeningCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Rules.BoardRegion."))
		|| Name.Contains(TEXT(".Rules.TurnOne.Restriction"))
		|| Name.Contains(TEXT(".Rules.TurnOne.SecondPlayers"))
		|| Name.Contains(TEXT(".Rules.TurnOne.SetupNot")))
	{
		return RunBoardOrTurnCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Rules.TurnOne.Summon"))
		|| Name.Contains(TEXT(".Rules.TurnOne.IllegalSummon")))
	{
		return RunSummonCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Rules.TurnOne.Move"))
		|| Name.Contains(TEXT(".Rules.TurnOne.MultiTile"))
		|| Name.Contains(TEXT(".Rules.TurnOne.Teleport"))
		|| Name.Contains(TEXT(".Rules.TurnOne.Push"))
		|| Name.Contains(TEXT(".Rules.TurnOne.Pull"))
		|| Name.Contains(TEXT(".Rules.TurnOne.Swap"))
		|| Name.Contains(TEXT(".Rules.TurnOne.Relocation"))
		|| Name.Contains(TEXT(".Rules.TurnOne.EnemyUnit"))
		|| Name.Contains(TEXT(".Rules.TurnOne.IllegalRelocation")))
	{
		return RunRelocationCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Rules.TurnOne.NeutralNPC"))
		|| Name.Contains(TEXT(".Rules.TurnOne.Opponent"))
		|| Name.Contains(TEXT(".Rules.TurnOne.NeutralAttack")))
	{
		return RunAttackCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Production.")))
	{
		return RunProductionCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Authority.")))
	{
		return RunAuthorityCase(Test, Name);
	}
	Test.AddError(TEXT("Unmapped named canon test"));
	return false;
}

#define WB_CANON_TEST(ClassName, Path) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST( \
		ClassName, Path, \
		EAutomationTestFlags::EditorContext \
			| EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString&) \
	{ \
		return RunNamedCase(*this, TEXT(Path)); \
	}

WB_CANON_TEST(FWBFormatMin, "Wandbound.ActiveFormat.V1.StoredDeckMinimumAccepted")
WB_CANON_TEST(FWBFormatMax, "Wandbound.ActiveFormat.V1.StoredDeckMaximumAccepted")
WB_CANON_TEST(FWBFormatDuplicate, "Wandbound.ActiveFormat.V1.MainDeckDuplicateRejected")
WB_CANON_TEST(FWBFormatTrap, "Wandbound.ActiveFormat.V1.TrapInMainDeckRejected")
WB_CANON_TEST(FWBFormatNPC, "Wandbound.ActiveFormat.V1.NPCInMainDeckRejected")
WB_CANON_TEST(FWBFormatCharacter, "Wandbound.ActiveFormat.V1.NonHybridCharacterRequired")
WB_CANON_TEST(FWBFormatTrapRepeat, "Wandbound.ActiveFormat.V1.RepeatedSetupTrapAccepted")
WB_CANON_TEST(FWBFormatNPCRepeat, "Wandbound.ActiveFormat.V1.RepeatedSetupNPCAccepted")
WB_CANON_TEST(FWBFormatSeven, "Wandbound.ActiveFormat.V1.SevenCardLaunchDeckAccepted")
WB_CANON_TEST(FWBFormatCapacity, "Wandbound.ActiveFormat.V1.InsufficientOpeningHandCapacityRejected")
WB_CANON_TEST(FWBFormatMirrored, "Wandbound.ActiveFormat.V1.MirroredDecksAccepted")

WB_CANON_TEST(FWBMarkerHeroTile, "Wandbound.Setup.Markers.HeroSpawnTileRejected")
WB_CANON_TEST(FWBMarkerLegal, "Wandbound.Setup.Markers.OtherLegalOwnHalfTileAccepted")
WB_CANON_TEST(FWBMarkerBothHeroTiles, "Wandbound.Setup.Markers.BothHeroSpawnTilesReserved")

WB_CANON_TEST(FWBHeroAtomic, "Wandbound.Setup.HeroSpawn.BothHeroesCommittedAtomically")
WB_CANON_TEST(FWBHeroNoPartial, "Wandbound.Setup.HeroSpawn.NeitherHeroObservesPartialSpawnState")
WB_CANON_TEST(FWBHeroSharedState, "Wandbound.Setup.HeroSpawn.SharedPostSpawnTriggerCollection")
WB_CANON_TEST(FWBHeroReplay, "Wandbound.Setup.HeroSpawn.ReplayStable")

WB_CANON_TEST(FWBHeroOwnTrigger, "Wandbound.Setup.HeroSummon.OwnWhenSummonedTriggers")
WB_CANON_TEST(FWBHeroCharacterTrigger, "Wandbound.Setup.HeroSummon.CharacterSummonedObserverTriggers")
WB_CANON_TEST(FWBHeroUnitTrigger, "Wandbound.Setup.HeroSummon.UnitSummonedObserverTriggers")
WB_CANON_TEST(FWBHeroOpponentTrigger, "Wandbound.Setup.HeroSummon.OpponentSummonedObserverTriggers")
WB_CANON_TEST(FWBHeroFactionTrigger, "Wandbound.Setup.HeroSummon.FactionObserverTriggers")
WB_CANON_TEST(FWBHeroNoHardcode, "Wandbound.Setup.HeroSummon.NoCardIdHardCoding")

WB_CANON_TEST(FWBTriggerFirstBatch, "Wandbound.Setup.HeroTriggers.FirstPlayerBatchResolvesFirst")
WB_CANON_TEST(FWBTriggerSecondBatch, "Wandbound.Setup.HeroTriggers.SecondPlayerBatchResolvesSecond")
WB_CANON_TEST(FWBTriggerChoice, "Wandbound.Setup.HeroTriggers.ControllerChoosesMultipleTriggerOrder")
WB_CANON_TEST(FWBTriggerStableId, "Wandbound.Setup.HeroTriggers.OrderChoiceHasStableActionId")
WB_CANON_TEST(FWBTriggerReplay, "Wandbound.Setup.HeroTriggers.OrderChoiceReplayStable")
WB_CANON_TEST(FWBTriggerAuto, "Wandbound.Setup.HeroTriggers.SingleTriggerAutoResolves")

WB_CANON_TEST(FWBReactNotGenerated, "Wandbound.Setup.Reacts.ManualReactNotGenerated")
WB_CANON_TEST(FWBReactRejected, "Wandbound.Setup.Reacts.ManualReactSubmissionRejected")
WB_CANON_TEST(FWBReactChoice, "Wandbound.Setup.Reacts.RequiredTriggerChoiceStillAllowed")
WB_CANON_TEST(FWBReactNested, "Wandbound.Setup.Reacts.MandatoryNestedTriggerContinues")
WB_CANON_TEST(FWBReactRestored, "Wandbound.Setup.Reacts.NormalReactAvailabilityRestoredAfterSetup")

WB_CANON_TEST(FWBOpeningBeforeSix, "Wandbound.Setup.OpeningHand.HeroDrawOccursBeforeOpeningSix")
WB_CANON_TEST(FWBOpeningSeven, "Wandbound.Setup.OpeningHand.HeroDrawOneProducesSevenCardStartingHand")
WB_CANON_TEST(FWBOpeningPreserved, "Wandbound.Setup.OpeningHand.PreOpeningCardsNotReplaced")
WB_CANON_TEST(FWBOpeningPrivate, "Wandbound.Setup.OpeningHand.PrivateCardIdentitiesNotPublic")
WB_CANON_TEST(FWBOpeningReasons, "Wandbound.Setup.OpeningHand.TraceReasonsDistinct")

WB_CANON_TEST(FWBRegionP0, "Wandbound.Rules.BoardRegion.Player0Perspective")
WB_CANON_TEST(FWBRegionP1, "Wandbound.Rules.BoardRegion.Player1Perspective")
WB_CANON_TEST(FWBRegionNeutral, "Wandbound.Rules.BoardRegion.MiddleRowNeutralForBoth")
WB_CANON_TEST(FWBRegionReverse, "Wandbound.Rules.BoardRegion.HalvesReverseByPlayer")

WB_CANON_TEST(FWBTurnScopeFirst, "Wandbound.Rules.TurnOne.RestrictionAppliesOnlyToFirstPlayer")
WB_CANON_TEST(FWBTurnScopeEnds, "Wandbound.Rules.TurnOne.RestrictionEndsAfterFirstPlayersFirstTurn")
WB_CANON_TEST(FWBTurnScopeSecond, "Wandbound.Rules.TurnOne.SecondPlayersFirstTurnNotRestricted")
WB_CANON_TEST(FWBTurnScopeSetup, "Wandbound.Rules.TurnOne.SetupNotRestricted")

WB_CANON_TEST(FWBSummonOwn, "Wandbound.Rules.TurnOne.SummonOwnHalfAccepted")
WB_CANON_TEST(FWBSummonNeutral, "Wandbound.Rules.TurnOne.SummonNeutralRowAccepted")
WB_CANON_TEST(FWBSummonOpponent, "Wandbound.Rules.TurnOne.SummonOpponentHalfRejected")
WB_CANON_TEST(FWBSummonNotMove, "Wandbound.Rules.TurnOne.SummonIsNotMovement")
WB_CANON_TEST(FWBSummonNotGenerated, "Wandbound.Rules.TurnOne.IllegalSummonNotGenerated")

WB_CANON_TEST(FWBMoveOwn, "Wandbound.Rules.TurnOne.MoveWithinOwnHalfAccepted")
WB_CANON_TEST(FWBMoveNeutral, "Wandbound.Rules.TurnOne.MoveOwnHalfToNeutralRejected")
WB_CANON_TEST(FWBMoveOpponent, "Wandbound.Rules.TurnOne.MoveOwnHalfToOpponentHalfRejected")
WB_CANON_TEST(FWBMovePath, "Wandbound.Rules.TurnOne.MultiTilePathCrossingNeutralRejected")
WB_CANON_TEST(FWBTeleport, "Wandbound.Rules.TurnOne.TeleportCrossingBoundaryRejected")
WB_CANON_TEST(FWBPushFriendly, "Wandbound.Rules.TurnOne.PushFriendlyAcrossBoundaryRejected")
WB_CANON_TEST(FWBPushEnemy, "Wandbound.Rules.TurnOne.PushEnemyAcrossBoundaryRejected")
WB_CANON_TEST(FWBPullNeutral, "Wandbound.Rules.TurnOne.PullNeutralAcrossBoundaryRejected")
WB_CANON_TEST(FWBSwap, "Wandbound.Rules.TurnOne.SwapCrossingEitherBoundaryRejected")
WB_CANON_TEST(FWBRelocationBypass, "Wandbound.Rules.TurnOne.RelocationEffectCannotBypassRestriction")
WB_CANON_TEST(FWBEnemyWithinRegion, "Wandbound.Rules.TurnOne.EnemyUnitMovementWithinOpponentHalfNotRejectedByBoundaryRule")
WB_CANON_TEST(FWBRelocationNotGenerated, "Wandbound.Rules.TurnOne.IllegalRelocationNotGenerated")

WB_CANON_TEST(FWBAttackNeutralOwn, "Wandbound.Rules.TurnOne.NeutralNPCAttackOwnHalfAccepted")
WB_CANON_TEST(FWBAttackNeutralRow, "Wandbound.Rules.TurnOne.NeutralNPCAttackNeutralRowAccepted")
WB_CANON_TEST(FWBAttackNeutralOpponent, "Wandbound.Rules.TurnOne.NeutralNPCAttackOpponentHalfAccepted")
WB_CANON_TEST(FWBAttackHero, "Wandbound.Rules.TurnOne.OpponentHeroAttackRejected")
WB_CANON_TEST(FWBAttackCharacter, "Wandbound.Rules.TurnOne.OpponentCharacterAttackRejected")
WB_CANON_TEST(FWBAttackRange, "Wandbound.Rules.TurnOne.NeutralAttackStillRequiresRange")
WB_CANON_TEST(FWBAttackLOS, "Wandbound.Rules.TurnOne.NeutralAttackStillRequiresLineOfSight")
WB_CANON_TEST(FWBAttackGenerated, "Wandbound.Rules.TurnOne.NeutralAttackGenerated")
WB_CANON_TEST(FWBAttackNotGenerated, "Wandbound.Rules.TurnOne.OpponentAttackNotGenerated")

WB_CANON_TEST(FWBProdFormatLoads, "Wandbound.Production.ActiveFormatV1.Loads")
WB_CANON_TEST(FWBProdFormatDigest, "Wandbound.Production.ActiveFormatV1.DigestPinned")
WB_CANON_TEST(FWBProdAddendumLoads, "Wandbound.Production.GameStartAddendum.Loads")
WB_CANON_TEST(FWBProdAddendumDigest, "Wandbound.Production.GameStartAddendum.DigestPinned")
WB_CANON_TEST(FWBProdDecks, "Wandbound.Production.InitialMatch.DecksValid")
WB_CANON_TEST(FWBProdKits, "Wandbound.Production.InitialMatch.SetupKitsValid")
WB_CANON_TEST(FWBProdHeroes, "Wandbound.Production.InitialMatch.HeroesSpawnAtomically")
WB_CANON_TEST(FWBProdHands, "Wandbound.Production.InitialMatch.OpeningHandsDrawn")
WB_CANON_TEST(FWBProdDecision, "Wandbound.Production.InitialMatch.FirstDecisionReached")
WB_CANON_TEST(FWBProdStarted, "Wandbound.Production.InitialMatch.StartupResultProductionStarted")

WB_CANON_TEST(FWBAuthorityCore, "Wandbound.Authority.GameStart.CoreOwnsSetupRules")
WB_CANON_TEST(FWBAuthorityRuntime, "Wandbound.Authority.GameStart.RuntimeCannotMutateLegality")
WB_CANON_TEST(FWBAuthorityPresentation, "Wandbound.Authority.GameStart.PresentationCannotSequenceCoreSpawn")
WB_CANON_TEST(FWBAuthorityRelocation, "Wandbound.Authority.TurnOne.SharedRelocationGuardUsed")
WB_CANON_TEST(FWBAuthorityModels, "Wandbound.Authority.NoCharacterModelsImported")
WB_CANON_TEST(FWBAuthorityMeshy, "Wandbound.Authority.NoMeshyDependencyAdded")
WB_CANON_TEST(FWBAuthorityGodot, "Wandbound.Authority.NoGodotFilesModified")

#undef WB_CANON_TEST
}

#endif
